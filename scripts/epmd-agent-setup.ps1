$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables


$TEMPLATES_DIR = Join-Path $PSScriptRoot "templates"


function New-Environment {
    $service = Get-Service $EPMD_SERVICE_NAME -ErrorAction SilentlyContinue
    if($service) {
        Stop-Service -Force -Name $EPMD_SERVICE_NAME
        Start-ExternalCommand { sc.exe delete $EPMD_SERVICE_NAME } -ErrorMessage "Failed to delete exiting EPMD service"
    }
    New-Directory -RemoveExisting $EPMD_DIR
    New-Directory $EPMD_SERVICE_DIR
    New-Directory $EPMD_LOG_DIR
}

function New-EPMDWindowsAgent {
    $epmdBinary = Join-Path $ERTS_DIR "bin\epmd.exe"
    if(!(Test-Path $epmdBinary)) {
        Throw "The EPMD binary $epmdBinary doesn't exist. Cannot configure the EPMD agent Windows service"
    }
    $logFile = Join-Path $EPMD_LOG_DIR "epmd.log"
    New-DCOSWindowsService -Name $EPMD_SERVICE_NAME -DisplayName $EPMD_SERVICE_DISPLAY_NAME -Description $EPMD_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $SERVICE_WRAPPER -BinaryPath "$epmdBinary -port $EPMD_PORT"
    Start-Service $EPMD_SERVICE_NAME
}

try {
    New-Environment
    New-EPMDWindowsAgent
    Open-WindowsFirewallRule -Name "Allow inbound TCP Port $EPMD_PORT for EPMD" -Direction "Inbound" -LocalPort $EPMD_PORT -Protocol "TCP"
    Open-WindowsFirewallRule -Name "Allow inbound UDP Port $EPMD_PORT for EPMD" -Direction "Inbound" -LocalPort $EPMD_PORT -Protocol "UDP"
} catch {
    Write-Log $_.ToString()
    exit 1
}
Write-Log "Successfully installed the EPMD Windows agent"
exit 0
