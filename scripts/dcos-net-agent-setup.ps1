Param(
    [Parameter(Mandatory=$true)]
    [string]$AgentPrivateIP,
    [Parameter(Mandatory=$true)]
    [string]$DCOSNetZipPackageUrl
)

$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables


$TEMPLATES_DIR = Join-Path $PSScriptRoot "templates"


function Install-VCredist {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$InstallerURL
    )
    Write-Output "Install VCredist: $InstallerURL"
    $installerPath = Join-Path $env:TEMP "vcredist_x64.exe"
    Start-ExecuteWithRetry { Invoke-WebRequest -UseBasicParsing -Uri $InstallerURL -OutFile $installerPath }
    $p = Start-Process -Wait -PassThru -FilePath $installerPath -ArgumentList @("/install", "/passive")
    if($p.ExitCode -ne 0) {
        Throw ("Failed install VCredist $InstallerURL. Exit code: {0}" -f $p.ExitCode)
    }
    Write-Output "Finished to install VCredist: $InstallerURL"
    Remove-File -Path $installerPath -Fatal $false
}

function New-Environment {
    New-Directory -RemoveExisting $DCOS_NET_DIR
    New-Directory $DCOS_NET_SERVICE_DIR
    $zipPkg = Join-Path $env:TEMP "dcos-net.zip"
    Write-Output "Downloading latest dcos-net build"
    Start-ExecuteWithRetry { Invoke-WebRequest -UseBasicParsing -Uri $DCOSNetZipPackageUrl -OutFile $zipPkg }
    Write-Output "Extracting dcos-net zip archive to $DCOS_NET_DIR"
    Expand-Archive -LiteralPath $zipPkg -DestinationPath $DCOS_NET_DIR
    Remove-File -Path $zipPkg -Fatal $false
    New-Directory -RemoveExisting "$DCOS_NET_DIR\lashup"
    New-Directory -RemoveExisting "$DCOS_NET_DIR\mnesia"
    New-Directory -RemoveExisting "$DCOS_NET_DIR\config.d"
    Install-VCredist -InstallerURL $VCREDIST_2013_URL
    Install-VCredist -InstallerURL $VCREDIST_2017_URL
    $binDir = "${DCOS_NET_DIR}\erts-9.2\bin" -replace '\\', '\\'
    $rootDir = "${DCOS_NET_DIR}" -replace '\\', '\\'
    $context = @{
        'bin_dir' = $binDir
        'root_dir' = $rootDir
    }
    Start-RenderTemplate -TemplateFile "$TEMPLATES_DIR\erl.ini" -Context $context -OutFile "${binDir}\erl.ini"
    Add-ToSystemPath $DCOS_NET_BIN_DIR
}

function New-DevConBinary {
    $devConDir = Join-Path $env:TEMP "devcon"
    if(Test-Path $devConDir) {
        Remove-Item -Recurse -Force $devConDir
    }
    New-Item -ItemType Directory -Path $devConDir | Out-Null
    $devConCab = Join-Path $devConDir "devcon.cab"
    Start-ExecuteWithRetry { Invoke-WebRequest -UseBasicParsing -Uri $DEVCON_CAB_URL -OutFile $devConCab | Out-Null }
    $devConFile = "filbad6e2cce5ebc45a401e19c613d0a28f"
    Start-ExternalCommand { expand.exe $devConCab -F:$devConFile $devConDir } -ErrorMessage "Failed to expand $devConCab" | Out-Null
    $devConBinary = Join-Path $env:TEMP "devcon.exe"
    Move-Item "$devConDir\$devConFile" $devConBinary
    Remove-File -Recurse -Force -Path $devConDir -Fatal $false
    return $devConBinary
}

function Install-DCOSNetDevice {
    $dcosNetDevice = Get-NetAdapter -Name $DCOS_NET_DEVICE_NAME -ErrorAction SilentlyContinue
    if($dcosNetDevice) {
        return
    }
    $devCon = New-DevConBinary
    Write-Output "Creating the dcos-net network device"
    Start-ExternalCommand { & $devCon install "${env:windir}\Inf\Netloop.inf" "*MSLOOP" } -ErrorMessage "Failed to install the dcos-net dummy interface"
    Remove-File -Path $devCon -Fatal $false
    Get-NetAdapter | Where-Object { $_.DriverDescription -eq "Microsoft KM-TEST Loopback Adapter" } | Rename-NetAdapter -NewName $DCOS_NET_DEVICE_NAME
}

function Set-DCOSNetDevice {
    $dcosNetDevice = Get-NetAdapter -Name $DCOS_NET_DEVICE_NAME -ErrorAction SilentlyContinue
    if(!$dcosNetDevice) {
        Throw "dcos-net network device was not found"
    }
    foreach($ip in $DCOS_NET_LOCAL_ADDRESSES) {
        $address = Get-NetIPAddress -InterfaceAlias $DCOS_NET_DEVICE_NAME -AddressFamily "IPv4" -IPAddress $ip -ErrorAction SilentlyContinue
        if($address) {
            continue
        }
        New-NetIPAddress -InterfaceAlias $DCOS_NET_DEVICE_NAME -AddressFamily "IPv4" -IPAddress $ip -PrefixLength 32 | Out-Null
    }
    Disable-NetAdapter $DCOS_NET_DEVICE_NAME -Confirm:$false
    Enable-NetAdapter $DCOS_NET_DEVICE_NAME -Confirm:$false
    foreach($ip in $DCOS_NET_LOCAL_ADDRESSES) {
        $startTime = Get-Date
        while(!(Test-Connection $ip -Count 1 -Quiet)) {
            if(((Get-Date) - $startTime).Minutes -ge 5) {
                Throw "dcos-net address $ip didn't become reachable within 5 minutes"
            }
        }
    }
}

function Get-UpstreamDNSResolvers {
    <#
    .SYNOPSIS
    Returns the DNS resolver(s) configured on the main interface
    #>
    $mainAddress = Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -eq $AgentPrivateIP }
    if(!$mainAddress) {
        Throw "Could not find any interface configured with the IP: $AgentPrivateIP"
    }
    $mainInterfaceIndex = $mainAddress.InterfaceIndex
    return (Get-DnsClientServerAddress -InterfaceIndex $mainInterfaceIndex).ServerAddresses
}

function New-DCOSNetWindowsAgent {
    $erlBinary = Join-Path $DCOS_NET_DIR "erts-9.2\bin\erl.exe"
    if(!(Test-Path $erlBinary)) {
        Throw "The erl binary $erlBinary doesn't exist. Cannot configure the dcos-net Windows agent"
    }
    $upstreamDNSResolvers = Get-UpstreamDNSResolvers | ForEach-Object { "{{" + ($_.Split('.') -join ', ') + "}, 53}" }
    $context = @{
        "exhibitor_url" = "http://master.mesos:${EXHIBITOR_PORT}/exhibitor/v1/cluster/status"
        "dns_zones_file" = ("${DCOS_NET_DIR}\data\zones.json" -replace '\\', '\\')
        "upstream_resolvers" = "[$($upstreamDNSResolvers -join ', ')]"
        "lashup_dir" = ("${DCOS_NET_DIR}\lashup" -replace '\\', '\\')
        "mnesia_dir" = ("${DCOS_NET_DIR}\mnesia" -replace '\\', '\\')
        "config_dir" = ("${DCOS_NET_DIR}\config.d" -replace '\\', '\\')
        "dcos_root_dir" = ("${DCOS_NET_DIR}" -replace '\\', '\\')
    }
    $configFile = Join-Path $DCOS_NET_DIR "sys.config"
    Start-RenderTemplate -TemplateFile "$TEMPLATES_DIR\dcos-net\sys.config" `
                         -Context $context -OutFile $configFile
    $context = @{
        "agent_private_ip" = $AgentPrivateIP
    }
    $vmArgsFile = Join-Path $DCOS_NET_DIR "vm.args"
    Start-RenderTemplate -TemplateFile "$TEMPLATES_DIR\dcos-net\vm.args" `
                         -Context $context -OutFile $vmArgsFile
    $context = @{
        "dist_port" = "62501"
        "epmd_port" = "61420"
        "dcos_rest_port" = "62080"
    }
    $commonConfigFile = Join-Path $DCOS_NET_DIR "config.d\common.config"
    Start-RenderTemplate -TemplateFile "$TEMPLATES_DIR\dcos-net\common.config" `
                         -Context $context -OutFile $commonConfigFile
    $dcosNetArguments = ("-boot $DCOS_NET_DIR\releases\0.0.1\dcos-net " + `
                         "-config $DCOS_NET_DIR\sys.config " + `
                         "-args_file $DCOS_NET_DIR\vm.args " + `
                         "-noshell -noinput +Bd -mode embedded -pa -- foreground")
    $environmentFile = Join-Path $DCOS_NET_SERVICE_DIR "environment-file"
    Set-Content -Path $environmentFile -Value @(
        "MASTER_SOURCE=master_list",
        "MASTER_LIST_FILE=${MASTERS_LIST_FILE}"
        "ERL_FLAGS=-epmd_module dcos_net_epmd -start_epmd false -no_epmd -proto_dist dcos_net"
    )
    $logFile = Join-Path $DCOS_NET_LOG_DIR "dcos-net.log"
    New-DCOSWindowsService -Name $DCOS_NET_SERVICE_NAME -DisplayName $DCOS_NET_SERVICE_DISPLAY_NAME -Description $DCOS_NET_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $SERVICE_WRAPPER -EnvironmentFiles @($environmentFile) -BinaryPath "`"$erlBinary $dcosNetArguments`""
    Start-Service $DCOS_NET_SERVICE_NAME
    Set-DnsClientServerAddress -InterfaceAlias * -ServerAddresses $DCOS_NET_LOCAL_ADDRESSES
}


try {
    New-Environment
    Install-DCOSNetDevice
    Set-DCOSNetDevice
    New-DCOSNetWindowsAgent
    Open-WindowsFirewallRule -Name "Allow inbound TCP Port 53 for dcos-net" -Direction "Inbound" -LocalPort 53 -Protocol "TCP"
    Open-WindowsFirewallRule -Name "Allow inbound UDP Port 53 for dcos-net" -Direction "Inbound" -LocalPort 53 -Protocol "UDP"
} catch {
    Write-Output $_.ToString()
    exit 1
}
Write-Output "Successfully finished setting up the Windows dcos-net agent"
exit 0
