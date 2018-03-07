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

function Add-ToSystemPath {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$Path
    )
    $systemPath = [System.Environment]::GetEnvironmentVariable("PATH", "Machine")
    if ($Path -in $systemPath.Split(';')) {
        return
    }
    $newPath = $systemPath + ";" + $Path
    Start-ExternalCommand -ScriptBlock { setx.exe /M PATH $newPath } -ErrorMessage "Failed to set the system path"
    $env:PATH += ";$Path"
}

function Install-DiagnosticsFiles {
    $filesPath = Join-Path $env:TEMP "diagnostics-files.zip"
    Write-Output "Downloading Diagnostics binaries"
    Start-ExecuteWithRetry { Invoke-WebRequest -Uri $DiagnosticsWindowsBinariesURL -OutFile $filesPath }
    Write-Output "Extracting binaries archive in: $DIAGNOSTICS_DIR"
    Expand-Archive -LiteralPath $binariesPath -DestinationPath $DIAGNOSTICS_DIR
    Add-ToSystemPath $DIAGNOSTICS_BIN_DIR
    Remove-item $filesPath
}

function New-DiagnosticsAgent {
    $diagnosticsBinary = Join-Path $DIAGNOSTICS_BIN_DIR "dcos-diagnostics.exe"
    $logFile = Join-Path $DIAGNOSTICS_LOG_DIR "diagnostics-agent.log"
    New-Item -ItemType File -Path $logFile

    $dcos_endpoint_config_dir = Join-Path $DIAGNOSTICS_DIR "dcos-diagnostics-endpoint-config.json"

    $diagnosticsAgentArguments = (  "--master=`"${masterZkAddress}`"" + `
                                    "daemon" + `
                                    "--role agent" + `
                                    "--debug" + `
                                    "--no-unix-socket" + `
                                    "--port `"${DIAGNOSTICS_AGENT_PORT}`"" + `
                                    "--endpoint-config `"${dcos_endpoint_config_dir}`"")

    $environmentFile = Join-Path $DCOS_DIR "environment"
    $wrapperPath = Join-Path $DIAGNOSTICS_SERVICE_DIR "service-wrapper.exe"
    Start-ExecuteWithRetry { Invoke-WebRequest -UseBasicParsing -Uri $SERVICE_WRAPPER_URL -OutFile $wrapperPath }
    New-DCOSWindowsService -Name $DIAGNOSTICS_SERVICE_NAME -DisplayName $DIAGNOSTICS_SERVICE_DISPLAY_NAME -Description $DIAGNOSTICS_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $wrapperPath -BinaryPath "$diagnosticsBinary $diagnosticsAgentArguments" -EnvironmentFiles @($environmentFile)
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
