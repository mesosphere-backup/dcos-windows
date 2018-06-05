Param(
    [bool]$IncludeMetricsToMonitoredSericeList 
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
        Write-Log "Deleted existing $DIAGNOSTICS_SERVICE_NAME service"
    }
    New-Directory -RemoveExisting $DIAGNOSTICS_DIR
    New-Directory $DIAGNOSTICS_LOG_DIR
    New-Directory $DIAGNOSTICS_CONFIG_DIR
}

function Install-DiagnosticsFiles {
    $filesPath = Join-Path $AGENT_BLOB_DEST_DIR "diagnostics.zip"
    Write-Log "Extracting $filesPath to $DIAGNOSTICS_DIR"
    Expand-7ZIPFile -File $filesPath -DestinationPath $DIAGNOSTICS_DIR

    # There is an outstanding dcos-go PRs that would change the default
    # detect_ip.ps1 file path to new location matching the Linux's setup
    # Old default:
    #   C:\DCOS\diagnostics\detect_ip.ps1
    # New default:
    #   C:\opt\mesosphere\bin\detect_ip.ps1
    #
    # This is to avoid functionality break that would be caused between
    # the time right after the PR is merged and before this DC-OS script
    # is updated for reflecting the path change. 
    # Once the PR is merged, we will come back to clean the
    # redudant copies of the files there
    $stdBin = Join-Path $env:SystemDrive "\opt\mesosphere\bin"
    If(!(test-path $stdBin)) {
        New-Item -ItemType Directory -Path $stdBin
    }
    Copy-Item -Path $DIAGNOSTICS_DIR\detect_ip.ps1 -Destination $stdBin -Force

    Add-ToSystemPath $DIAGNOSTICS_DIR
    Remove-File -Path $filesPath -Fatal $false
}

function Get-DCOSVersionFromFile {
    if(!(Test-Path $GLOBAL_ENV_FILE)) {
        Throw "ERORR: Global environment file $GLOBAL_ENV_FILE doesn't exist"
    }
    # Get the $dcosVersion by parsing the global DC/OS environment file
    $dcosVersion = Get-Content $GLOBAL_ENV_FILE | Where-Object { $_.StartsWith('DCOS_VERSION=') } | ForEach-Object { $_.Split('=')[1] }
    if(!$dcosVersion) {
        Throw "ERROR: Cannot get the DC/OS from $GLOBAL_ENV_FILE"
    }
    return $dcosVersion
}

function Get-MonitoredServices {
    $services = @($DIAGNOSTICS_SERVICE_NAME, $MESOS_SERVICE_NAME, $ADMINROUTER_SERVICE_NAME)
    if ($IncludeMetricsToMonitoredSericeList) {
        $services += @($METRICS_SERVICE_NAME)
    }
    $dcosVersion = Get-DCOSVersionFromFile
    if($dcosVersion.StartsWith("1.8") -or $dcosVersion.StartsWith("1.9") -or $dcosVersion.StartsWith("1.10")) {
        $services += @($EPMD_SERVICE_NAME, $SPARTAN_SERVICE_NAME)
    } else {
        # DC/OS release >= 1.11
        $services += @($DCOS_NET_SERVICE_NAME)
    }
    return $services
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
    Get-MonitoredServices | Out-File -Encoding ascii -FilePath "${DIAGNOSTICS_DIR}\servicelist.txt"
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
    Write-Log $_.ToString()
    exit 1
}
Write-Log "Successfully finished setting up the Windows Diagnostics Service Agent"
exit 0
