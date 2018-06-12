Param(
    [Parameter(Mandatory=$true)]
    [string[]]$MasterAddress,
    [string]$AgentPrivateIP,
    [switch]$Public=$false,
    [string]$CustomAttributes
)

$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables


$TEMPLATES_DIR = Join-Path $PSScriptRoot "templates"


function New-MesosEnvironment {
    $service = Get-Service $MESOS_SERVICE_NAME -ErrorAction SilentlyContinue
    if($service) {
        Stop-Service -Force -Name $MESOS_SERVICE_NAME
        & sc.exe delete $MESOS_SERVICE_NAME
        if($LASTEXITCODE) {
            Throw "Failed to delete exiting $MESOS_SERVICE_NAME service"
        }
        Write-Log "Deleted existing $MESOS_SERVICE_NAME service"
    }
    New-Directory -RemoveExisting $MESOS_DIR
    New-Directory $MESOS_BIN_DIR
    New-Directory $MESOS_LOG_DIR
    New-Directory $MESOS_WORK_DIR
    New-Directory $MESOS_SERVICE_DIR
}

function Install-MesosBinaries {
    $binariesPath = Join-Path $AGENT_BLOB_DEST_DIR "mesos.zip"
    Write-Log "Extracting $binariesPath to $MESOS_BIN_DIR"
    Expand-7ZIPFile -File $binariesPath -DestinationPath $MESOS_BIN_DIR
    Add-ToSystemPath $MESOS_BIN_DIR
    Remove-File -Path $binariesPath -Fatal $false
}

function Get-MesosAgentAttributes {
    if($CustomAttributes) {
        return $CustomAttributes
    }
    $attributes = "os:windows"
    if($Public) {
        $attributes += ";public_ip:yes"
    }
    return $attributes
}

function Get-MesosAgentPrivateIP {
    if($AgentPrivateIP) {
        return $AgentPrivateIP
    }
    $primaryIfIndex = (Get-NetRoute -DestinationPrefix "0.0.0.0/0").ifIndex
    return (Get-NetIPAddress -AddressFamily IPv4 -ifIndex $primaryIfIndex).IPAddress
}

function New-MesosWindowsAgent {
    $mesosBinary = Join-Path $MESOS_BIN_DIR "mesos-agent.exe"
    $agentAddress = Get-MesosAgentPrivateIP
    $mesosAttributes = Get-MesosAgentAttributes
    $masterZkAddress = "zk://" + ($MasterAddress -join ":2181,") + ":2181/mesos"
    $mesosPath = ($DOCKER_HOME -replace '\\', '\\') + ';' + ($MESOS_BIN_DIR -replace '\\', '\\')
    $logFile = Join-Path $MESOS_LOG_DIR "mesos-agent.log"
    New-Item -ItemType File -Path $logFile
    $mesosAgentArguments = ("--master=`"${masterZkAddress}`"" + `
                           " --work_dir=`"${MESOS_WORK_DIR}`"" + `
                           " --runtime_dir=`"${MESOS_WORK_DIR}`"" + `
                           " --launcher_dir=`"${MESOS_BIN_DIR}`"" + `
                           " --external_log_file=`"${logFile}`"" + `
                           " --ip=`"${agentAddress}`"" + `
                           " --isolation=`"windows/cpu,filesystem/windows`"" + `
                           " --containerizers=`"docker,mesos`"" + `
                           " --attributes=`"${mesosAttributes}`"" + `
                           " --hostname=`"${AgentPrivateIP}`"" +
                           " --executor_environment_variables=`"{\\\`"PATH\\\`": \\\`"${mesosPath}\\\`"}`"")
    if($Public) {
        $mesosAgentArguments += " --default_role=`"slave_public`""
    }
    $environmentFile = Join-Path $MESOS_SERVICE_DIR "environment-file"
    if (!(Test-Path $environmentFile))
    {
      Set-Content -Path $environmentFile -Value @(
          "MESOS_AUTHENTICATE_HTTP_READONLY=false",
          "MESOS_AUTHENTICATE_HTTP_READWRITE=false"
      )
    }
    New-DCOSWindowsService -Name $MESOS_SERVICE_NAME -DisplayName $MESOS_SERVICE_DISPLAY_NAME -Description $MESOS_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $SERVICE_WRAPPER -BinaryPath "$mesosBinary $mesosAgentArguments" -EnvironmentFiles @($environmentFile)
    Start-Service $MESOS_SERVICE_NAME
}

try {
    New-MesosEnvironment
    Install-MesosBinaries
    New-MesosWindowsAgent
    Open-WindowsFirewallRule -Name "Allow inbound TCP Port $MESOS_AGENT_PORT for Mesos Slave" -Direction "Inbound" -LocalPort $MESOS_AGENT_PORT -Protocol "TCP"
    Open-WindowsFirewallRule -Name "Allow inbound TCP Port $ZOOKEEPER_PORT for Zookeeper" -Direction "Inbound" -LocalPort $ZOOKEEPER_PORT -Protocol "TCP" # It's needed on the private DCOS agents
} catch {
    Write-Log $_.ToString()
    exit 1
}
Write-Log "Successfully finished setting up the Windows Mesos Agent"
exit 0
