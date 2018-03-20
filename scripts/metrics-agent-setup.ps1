Param(
    [Parameter(Mandatory=$true)]
    [string]$MetricsWindowsBinariesURL
)

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
        Write-Output "Deleted existing $METRICS_SERVICE_NAME service"
    }
    New-Directory -RemoveExisting $METRICS_DIR
    New-Directory $METRICS_BIN_DIR
    New-Directory $METRICS_LOG_DIR
    New-Directory $METRICS_SERVICE_DIR
    New-Directory $METRICS_CONFIG_DIR
}

function Install-MetricsFiles {
    $filesPath = Join-Path $env:TEMP "metrics-files.zip"
    Write-Output "Downloading Metrics binaries"
    Start-ExecuteWithRetry { Invoke-WebRequest -Uri $MetricsWindowsBinariesURL -OutFile $filesPath }
    Write-Output "Extracting binaries archive in: $METRICS_DIR"
    Expand-Archive -LiteralPath $filesPath -DestinationPath $METRICS_DIR
 
    # Populate the required Metrics service file to the expected location
    # This is to match what the current dcos-metrics code base expects
    # We might need to adjust the locations for those files once we determine 
    # better locations for them
    $mesosBin = Join-Path $env:SystemDrive "mesos\bin"
    New-Item -ItemType Directory -Path $mesosBin
    Copy-Item -Path $METRICS_DIR\detect_ip.ps1 $mesosBin
    $mesosVarLib = Join-Path $env:SystemDrive "mesos\var\lib\dcos"
    New-Item -ItemType directory -Path $mesosVarLib
    Copy-Item -Path $METRICS_DIR\cluster-id $mesosVarLib

    Add-ToSystemPath $METRICS_DIR
    Remove-item $filesPath
}

function New-MetricsAgent {
    $metricsBinary = Join-Path $METRICS_DIR "dcos-metrics.exe"
    $logFile = Join-Path $METRICS_LOG_DIR "metrics-agent.log"
    New-Item -ItemType File -Path $logFile

    $metricsAgentArguments = (  "-loglevel debug " + `
                                "-role agent " )
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
    Write-Output $_.ToString()
    exit 1
}
Write-Output "Successfully finished setting up the Windows Metrics Service Agent"
exit 0
