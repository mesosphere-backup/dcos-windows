# Copyright 2018 Microsoft Corporation
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#

Param(
    [Parameter(Mandatory=$true)]
    [string]$AgentBlobDirectory,
    [bool]$IncludeMetricsToMonitoredSericeList,
    [switch]$Public=$false
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
    $filesPath = Join-Path $AgentBlobDirectory "diagnostics.zip"
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

function Get-MonitoredServices {
    $services = @($DIAGNOSTICS_SERVICE_NAME, $ADMINROUTER_SERVICE_NAME, $DCOS_NET_SERVICE_NAME)
    if($Public) {
        $services += @($MESOS_PUBLIC_SERVICE_NAME)
    } else {
        $services += @($MESOS_SERVICE_NAME)
    }
    if($IncludeMetricsToMonitoredSericeList) {
        $services += @($METRICS_SERVICE_NAME)
    }
    return $services
}

function New-DiagnosticsAgent {
    $diagnosticsBinary = Join-Path $DIAGNOSTICS_DIR "dcos-diagnostics.exe"
    $logFile = Join-Path $DIAGNOSTICS_LOG_DIR "diagnostics-agent.log"
    New-Item -ItemType File -Path $logFile
    $config = @{
        "debug" = $true
        "no-unix-socket" = $true
        "agent-port" = $ADMINROUTER_AGENT_PORT # This is the front-end port and should be the admin router port
        "port" = $DIAGNOSTICS_AGENT_PORT
        "endpoint-config" = Join-Path $DIAGNOSTICS_CONFIG_DIR "dcos-diagnostics-endpoint-config.json"
    }
    if($Public) {
        $config["role"] = "agent_public"
    } else {
        $config["role"] = "agent"
    }
    $json = ConvertTo-Json -InputObject $config
    $configFile = Join-Path $DIAGNOSTICS_CONFIG_DIR "dcos-diagnostics-config.json"
    $asciiEncoding = [System.Text.Encoding]::ASCII
    [System.IO.File]::WriteAllLines($configFile, $json, $asciiEncoding)
    Get-MonitoredServices | Out-File -Encoding ascii -FilePath "${DIAGNOSTICS_DIR}\servicelist.txt"
    $environmentFile = Join-Path $DCOS_DIR "environment"
    New-DCOSWindowsService -Name $DIAGNOSTICS_SERVICE_NAME -DisplayName $DIAGNOSTICS_SERVICE_DISPLAY_NAME -Description $DIAGNOSTICS_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $SERVICE_WRAPPER -BinaryPath "$diagnosticsBinary daemon --config ${configFile}" -EnvironmentFiles @($environmentFile)
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
