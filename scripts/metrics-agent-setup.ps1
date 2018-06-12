$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables

function New-MetricsEnvironment {
    $service = Get-Service $METRICS_SERVICE_NAME -ErrorAction SilentlyContinue
    if($service) {
        Stop-Service -Force -Name $METRICS_SERVICE_NAME
        & sc.exe delete $METRICS_SERVICE_NAME
        if($LASTEXITCODE) {
            Throw "Failed to delete exiting $METRICS_SERVICE_NAME service"
        }
        Write-Log "Deleted existing $METRICS_SERVICE_NAME service"
    }
    New-Directory -RemoveExisting $METRICS_DIR
    New-Directory $METRICS_BIN_DIR
    New-Directory $METRICS_LOG_DIR
    New-Directory $METRICS_SERVICE_DIR
    New-Directory $METRICS_CONFIG_DIR
}

function Install-MetricsFiles {
    $filesPath = Join-Path $AGENT_BLOB_DEST_DIR "metrics.zip"
    Write-Log "Extracting $filesPath to $METRICS_DIR"
    Expand-7ZIPFile -File $filesPath -DestinationPath $METRICS_DIR
    
    # This is an outstanding dcos-go PR that would change the default file
    # paths to new locations matching the Linux's setup.
    # Old default:
    #   C:\mesos\var\lib\dcos\cluster-id
    #   C:\mesos\bin\detect_ip.ps1
    # New default:
    #   C:\opt\mesosphere\bin\detect_ip.ps1
    #   C:\var\lib\dcos\cluster-id
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
    Copy-Item -Path $METRICS_DIR\detect_ip.ps1 -Destination $stdBin -Force

    $stdVarLib = Join-Path $env:SystemDrive "var\lib\dcos"
    If(!(test-path $stdVarLib)) {
        New-Item -ItemType Directory -Path $stdVarLib
    }    
    Copy-Item -Path $METRICS_DIR\cluster-id -Destination $stdVarLib -Force

    # Populate the required Metrics service files to the expected locations
    # This is to match what the current dcos-metrics code base expects
    # We might need to adjust the locations for those files once we determine 
    # better locations for them
    $mesosBin = Join-Path $env:SystemDrive "mesos\bin"
    If(!(test-path $mesosBin)) {
        New-Item -ItemType Directory -Path $mesosBin
    }
    Move-Item -Path $METRICS_DIR\detect_ip.ps1 $mesosBin -Force
    $mesosVarLib = Join-Path $env:SystemDrive "mesos\var\lib\dcos"
    If(!(test-path $mesosVarLib)) {
        New-Item -ItemType Directory -Path $mesosVarLib
    }
    Move-Item -Path $METRICS_DIR\cluster-id $mesosVarLib -Force

    Add-ToSystemPath $METRICS_DIR
    Remove-File -Path $filesPath -Fatal $false
}

function New-MetricsAgent {
    $metricsBinary = Join-Path $METRICS_DIR "dcos-metrics.exe"
    $logFile = Join-Path $METRICS_LOG_DIR "metrics-agent.log"
    New-Item -ItemType File -Path $logFile
    $configFile = Join-Path $METRICS_DIR "config\dcos-metrics-config.yaml"
    $metricsAgentArguments = (  "-loglevel debug " + `
                                "-role agent " + `
                                "-config $configFile " )
    $environmentFile = Join-Path $DCOS_DIR "environment"
    New-DCOSWindowsService -Name $METRICS_SERVICE_NAME -DisplayName $METRICS_SERVICE_DISPLAY_NAME -Description $METRICS_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $SERVICE_WRAPPER -BinaryPath "$metricsBinary $metricsAgentArguments" -EnvironmentFiles @($environmentFile)
    Start-Service $METRICS_SERVICE_NAME
}

try {
    New-MetricsEnvironment
    Install-MetricsFiles
    New-MetricsAgent
} catch {
    Write-Log $_.ToString()
    exit 1
}
Write-Log "Successfully finished setting up the Windows Metrics Service Agent"
exit 0
