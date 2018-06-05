Param(
    [Parameter(Mandatory=$true)]
    [string[]]$MasterAddress,
    [string]$AgentPrivateIP,
    [switch]$Public=$false
)

$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables


$TEMPLATES_DIR = Join-Path $PSScriptRoot "templates"
$SPARTAN_LATEST_RELEASE_URL = "$LOG_SERVER_BASE_URL/spartan-build/master/latest/release.zip"


function New-Environment {
    $service = Get-Service $SPARTAN_SERVICE_NAME -ErrorAction SilentlyContinue
    if($service) {
        Stop-Service -Force -Name $SPARTAN_SERVICE_NAME
        Start-ExternalCommand { sc.exe delete $SPARTAN_SERVICE_NAME } -ErrorMessage "Failed to delete exiting DC/OS Spartan service"
    }
    New-Directory -RemoveExisting $SPARTAN_DIR
    New-Directory $SPARTAN_RELEASE_DIR
    New-Directory $SPARTAN_SERVICE_DIR
    New-Directory $SPARTAN_LOG_DIR
    $spartanReleaseZip = Join-Path $AGENT_BLOB_DEST_DIR "spartan-release.zip"
    Write-Log "Extracting $spartanReleaseZip to $SPARTAN_RELEASE_DIR"
    Expand-7ZIPFile -File $spartanReleaseZip -DestinationPath $SPARTAN_RELEASE_DIR
    Remove-File -Path $spartanReleaseZip -Fatal $false
}

function Install-SpartanDevice {
    $spartanDevice = Get-NetAdapter -Name $SPARTAN_DEVICE_NAME -ErrorAction SilentlyContinue
    if($spartanDevice) {
        return
    }
    $devCon = Join-Path $AGENT_BLOB_DEST_DIR "devcon.exe"
    Write-Log "Creating the Spartan network device"
    Start-ExternalCommand { & $devCon install "${env:windir}\Inf\Netloop.inf" "*MSLOOP" } -ErrorMessage "Failed to install the Spartan dummy interface"
    Remove-File -Path $devCon -Fatal $false
    Get-NetAdapter | Where-Object { $_.DriverDescription -eq "Microsoft KM-TEST Loopback Adapter" } | Rename-NetAdapter -NewName $SPARTAN_DEVICE_NAME
}

function Set-SpartanDevice {
    $spartanDevice = Get-NetAdapter -Name $SPARTAN_DEVICE_NAME -ErrorAction SilentlyContinue
    if(!$spartanDevice) {
        Throw "Spartan network device was not found"
    }
    foreach($ip in $SPARTAN_LOCAL_ADDRESSES) {
        $address = Get-NetIPAddress -InterfaceAlias $SPARTAN_DEVICE_NAME -AddressFamily "IPv4" -IPAddress $ip -ErrorAction SilentlyContinue
        if($address) {
            continue
        }
        New-NetIPAddress -InterfaceAlias $SPARTAN_DEVICE_NAME -AddressFamily "IPv4" -IPAddress $ip -PrefixLength 32 | Out-Null
    }
    Disable-NetAdapter $SPARTAN_DEVICE_NAME -Confirm:$false
    Enable-NetAdapter $SPARTAN_DEVICE_NAME -Confirm:$false
    foreach($ip in $SPARTAN_LOCAL_ADDRESSES) {
        $startTime = Get-Date
        while(!(Test-Connection $ip -Count 1 -Quiet)) {
            if(((Get-Date) - $startTime).Minutes -ge 5) {
                Throw "Spartan address $ip didn't become reachable within 5 minutes"
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
        Throw "Could not find any NetIPAddress configured with the IP: $AgentPrivateIP"
    }
    $mainInterfaceIndex = $mainAddress.InterfaceIndex
    return (Get-DnsClientServerAddress -InterfaceIndex $mainInterfaceIndex).ServerAddresses
}

function New-SpartanWindowsAgent {
    $erlBinary = Join-Path $ERTS_DIR "bin\erl.exe"
    if(!(Test-Path $erlBinary)) {
        Throw "The erl binary $erlBinary doesn't exist. Cannot configure the Spartan agent Windows service"
    }
    $upstreamDNSResolvers = Get-UpstreamDNSResolvers | ForEach-Object { "{{" + ($_.Split('.') -join ', ') + "}, 53}" }
    $dnsZonesFile = "${SPARTAN_RELEASE_DIR}\spartan\data\zones.json" -replace '\\', '\\'
    $exhibitorURL = "http://master.mesos:${EXHIBITOR_PORT}/exhibitor/v1/cluster/status"
    $context = @{
        "exhibitor_url" = $exhibitorURL
        "dns_zones_file" = $dnsZonesFile
        "upstream_resolvers" = "[$($upstreamDNSResolvers -join ', ')]"
    }
    $spartanConfigFile = Join-Path $SPARTAN_DIR "sys.spartan.config"
    Start-RenderTemplate -TemplateFile "$TEMPLATES_DIR\spartan\sys.spartan.config" -Context $context -OutFile "$SPARTAN_DIR\sys.spartan.config"
    $spartanVMArgsFile = Join-Path $SPARTAN_DIR "vm.spartan.args"
    $context = @{
        "agent_private_ip" = $AgentPrivateIP
        "epmd_port" = $EPMD_PORT
    }
    Start-RenderTemplate -TemplateFile "$TEMPLATES_DIR\spartan\vm.spartan.args" -Context $context -OutFile "$SPARTAN_DIR\vm.spartan.args"
    $spartanArguments = ("-noshell -noinput +Bd -mode embedded " + `
                         "-rootdir `"${SPARTAN_RELEASE_DIR}\spartan`" " + `
                         "-boot `"${SPARTAN_RELEASE_DIR}\spartan\releases\0.0.1\spartan`" " + `
                         "-boot_var ERTS_LIB_DIR `"${SPARTAN_RELEASE_DIR}\lib`" " + `
                         "-boot_var RELEASE_DIR `"${SPARTAN_RELEASE_DIR}\spartan`" " + `
                         "-config `"${spartanConfigFile}`" " + `
                         "-args_file `"${spartanVMArgsFile}`" -pa " + `
                         "-- foreground")
    $environmentFile = Join-Path $SPARTAN_SERVICE_DIR "environment-file"
    Set-Content -Path $environmentFile -Value @(
        "MASTER_SOURCE=master_list",
        "MASTER_LIST_FILE=${MASTERS_LIST_FILE}"
    )
    $logFile = Join-Path $SPARTAN_LOG_DIR "spartan.log"
    New-DCOSWindowsService -Name $SPARTAN_SERVICE_NAME -DisplayName $SPARTAN_SERVICE_DISPLAY_NAME -Description $SPARTAN_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $SERVICE_WRAPPER -EnvironmentFiles @($environmentFile) -BinaryPath "`"$erlBinary $spartanArguments`""
    Start-Service $SPARTAN_SERVICE_NAME
    Set-DnsClientServerAddress -InterfaceAlias * -ServerAddresses $SPARTAN_LOCAL_ADDRESSES
}


try {
    New-Environment
    Install-SpartanDevice
    Set-SpartanDevice
    New-SpartanWindowsAgent
    Open-WindowsFirewallRule -Name "Allow inbound TCP Port 53 for Spartan" -Direction "Inbound" -LocalPort 53 -Protocol "TCP"
    Open-WindowsFirewallRule -Name "Allow inbound UDP Port 53 for Spartan" -Direction "Inbound" -LocalPort 53 -Protocol "UDP"
} catch {
    Write-Log $_.ToString()
    exit 1
}
Write-Log "Successfully finished setting up the Windows Spartan Agent"
exit 0
