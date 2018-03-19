Param(
    [Parameter(Mandatory=$true)]
    [string]$DiagnosticsWindowsBinariesURL
)

$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables

function New-DiagnosticsEnvironment {
    $service = Get-Service $DIAGNOSTICS_SERVICE_NAME -ErrorAction SilentlyContinue
    if($service) {
        Stop-Service -Force -Name $DIAGNOSTICS_SERVICE_NAME
        & sc.exe delete $DIAGNOSTICS_SERVICE_NAME
        if($LASTEXITCODE) {
            Throw "Failed to delete exiting $DIAGNOSTICS_SERVICE_NAME service"
        }
        Write-Output "Deleted existing $DIAGNOSTICS_SERVICE_NAME service"
    }
    New-Directory -RemoveExisting $DIAGNOSTICS_DIR
    New-Directory $DIAGNOSTICS_BIN_DIR
    New-Directory $DIAGNOSTICS_LOG_DIR
    New-Directory $DIAGNOSTICS_SERVICE_DIR
    New-Directory $DIAGNOSTICS_CONFIG_DIR
}

function Install-DiagnosticsFiles {
    $filesPath = Join-Path $env:TEMP "diagnostics-files.zip"
    Write-Output "Downloading Diagnostics binaries"
    Start-ExecuteWithRetry { Invoke-WebRequest -Uri $DiagnosticsWindowsBinariesURL -OutFile $filesPath }
    Write-Output "Extracting binaries archive in: $DIAGNOSTICS_DIR"
    Expand-Archive -LiteralPath $filesPath -DestinationPath $DIAGNOSTICS_DIR
    Add-ToSystemPath $DIAGNOSTICS_DIR
    Remove-item $filesPath
}

function New-DiagnosticsAgent {
    $diagnosticsBinary = Join-Path $DIAGNOSTICS_DIR "dcos-diagnostics.exe"
    $logFile = Join-Path $DIAGNOSTICS_LOG_DIR "diagnostics-agent.log"
    New-Item -ItemType File -Path $logFile
    $dcos_endpoint_config_dir = Join-Path $DIAGNOSTICS_DIR "dcos-diagnostics-endpoint-config.json"
    $diagnosticsAgentArguments = (  "daemon " + `
                                    "--role agent " + `
                                    "--debug " + `
                                    "--no-unix-socket " + `
                                    "--port `"${DIAGNOSTICS_AGENT_PORT}`" " + `
                                    "--endpoint-config `"${dcos_endpoint_config_dir}`"")
    (Get-Service -Name "dcos-*").Name | Out-File -Encoding ascii -FilePath "${DIAGNOSTICS_DIR}\servicelist.txt"
    $environmentFile = Join-Path $DCOS_DIR "environment"
    New-DCOSWindowsService -Name $DIAGNOSTICS_SERVICE_NAME -DisplayName $DIAGNOSTICS_SERVICE_DISPLAY_NAME -Description $DIAGNOSTICS_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $SERVICE_WRAPPER -BinaryPath "$diagnosticsBinary $diagnosticsAgentArguments" -EnvironmentFiles @($environmentFile)
    Start-Service $DIAGNOSTICS_SERVICE_NAME
}

try {
    New-DiagnosticsEnvironment
    Install-DiagnosticsFiles
    New-DiagnosticsAgent
} catch {
    Write-Output $_.ToString()
    exit 1
}
Write-Output "Successfully finished setting up the Windows Diagnostics Service Agent"
exit 0
