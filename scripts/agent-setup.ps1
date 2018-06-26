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

[CmdletBinding(DefaultParameterSetName="Standard")]
Param(
    [ValidateNotNullOrEmpty()]
    [string]$AgentBlobDirectory,
    [ValidateNotNullOrEmpty()]
    [string[]]$MasterIPs,
    [ValidateNotNullOrEmpty()]
    [string]$AgentPrivateIP,
    [ValidateNotNullOrEmpty()]
    [string]$BootstrapUrl,
    [AllowNull()]
    [switch]$isPublic = $false,
    [AllowNull()]
    [string]$customAttrs,
    [AllowNull()]
    [string]$DcosVersion = ""
)

$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables


function Install-VCredist {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$Installer
    )
    Write-Log "Enter Install-VCredist"
    $installerPath = Join-Path $AgentBlobDirectory $Installer
    Write-Log "Install VCredist from $installerPath"
    $p = Start-Process -Wait -PassThru -FilePath $installerPath -ArgumentList @("/install", "/passive")
    if($p.ExitCode -ne 0) {
        Throw ("Failed install VCredist $Installer. Exit code: {0}" -f $p.ExitCode)
    }
    Write-Log "Finished to install VCredist: $Installer"
    Remove-Item -Recurse -Force $installerPath -ErrorAction SilentlyContinue
    Write-Log "Exit Install-VCredist"
}

function Install-MesosAgent {
    Write-Log "Enter Install-MesosAgent"
    & "${AgentBlobDirectory}\scripts\mesos-agent-setup.ps1" -AgentBlobDirectory $AgentBlobDirectory `
                                                            -MasterAddress $MasterIPs -AgentPrivateIP $AgentPrivateIP `
                                                            -Public:$isPublic -CustomAttributes $customAttrs
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS Mesos Windows slave agent"
    }
    Write-Log "Exit Install-MesosAgent"
}

function Install-ErlangRuntime {
    & "${AgentBlobDirectory}\scripts\erlang-setup.ps1" -AgentBlobDirectory $AgentBlobDirectory
    if($LASTEXITCODE) {
        Throw "Failed to setup the Windows Erlang runtime"
    }
}

function Install-EPMDAgent {
    & "${AgentBlobDirectory}\scripts\epmd-agent-setup.ps1"
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS EPMD Windows agent"
    }
}

function Install-SpartanAgent {
    & "${AgentBlobDirectory}\scripts\spartan-agent-setup.ps1" -AgentBlobDirectory $AgentBlobDirectory -MasterAddress $MasterIPs -AgentPrivateIP $AgentPrivateIP -Public:$isPublic
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS Spartan Windows agent"
    }
}

function Install-AdminRouterAgent {
    Write-Log "Enter Install-AdminRouterAgent"
    & "${AgentBlobDirectory}\scripts\adminrouter-agent-setup.ps1" -AgentBlobDirectory $AgentBlobDirectory -AgentPrivateIP $AgentPrivateIP
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS AdminRouter Windows agent"
    }
    Write-Log "Exit Install-AdminRouterAgent"
}

function Install-DiagnosticsAgent {
    Param(
        [bool]$IncludeMatricsService
    )
    Write-Log "Enter Install-DiagnosticsAgent"
    & "${AgentBlobDirectory}\scripts\diagnostics-agent-setup.ps1" -AgentBlobDirectory $AgentBlobDirectory -IncludeMetricsToMonitoredSericeList $IncludeMatricsService
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS Diagnostics Windows agent"
    }
    Write-Log "Exit Install-DiagnosticsAgent"
}

function Install-DCOSNetAgent {
    Write-Log "Enter Install-DCOSNetAgent"
    & "${AgentBlobDirectory}\scripts\dcos-net-agent-setup.ps1" -AgentBlobDirectory $AgentBlobDirectory -AgentPrivateIP $AgentPrivateIP
    if($LASTEXITCODE) {
        Throw "Failed to setup the dcos-net Windows agent"
    }
    Write-Log "Exit Install-DCOSNetAgent"
}

function Install-MetricsAgent {
    Write-Log "Enter Install-MetricsAgent"
    & "${AgentBlobDirectory}\scripts\metrics-agent-setup.ps1" -AgentBlobDirectory $AgentBlobDirectory
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS Metrics Windows agent"
    }
    Write-Log "Exit Install-MetricsAgent"
}

function Start-DockerConfiguration {
    Write-Log "Enter Start-DockerConfiguration"

    # stop Docker Windows service
    $dockerServiceObj = Stop-Service $DOCKER_SERVICE_NAME -PassThru
    $dockerServiceObj.WaitForStatus('Stopped','00:03:00')
    if ($dockerServiceObj.Status -ne 'Stopped') { 
       Throw "Docker service failed to stop"
    } else {
        Write-Log "Docker service was stopped successfully"
    }

    # remove Docker default network: nat
    Get-HNSNetwork | Remove-HNSNetwork
    Set-Content -Path "${DOCKER_DATA}\config\daemon.json" -Value '{ "bridge" : "none" }' -Encoding Ascii

    # update Docker binaries
    Start-ExecuteWithRetry { Copy-item "${AgentBlobDirectory}\docker.exe" "${DOCKER_HOME}\docker.exe" -Force }
    Start-ExecuteWithRetry { Copy-item "${AgentBlobDirectory}\dockerd.exe" "${DOCKER_HOME}\dockerd.exe" -Force }

    # start Docker Windows Service
    $dockerServiceObj = Start-Service $DOCKER_SERVICE_NAME -PassThru
    $dockerServiceObj.WaitForStatus('Running','00:03:00')
    if ($dockerServiceObj.Status -ne 'Running') { 
        Throw "Docker service failed to start"
    } else {
        Write-Log "Docker service was started successfully"
    }

    Write-Log "Exit Start-DockerConfiguration"
}

function New-DockerNATNetwork {
    #
    # This needs to be used by all the containers since DCOS Spartan DNS server
    # is not bound to the gateway address.
    # The Docker gateway address is added to the DNS server list unless
    # disable_gatewaydns network option is enabled.
    #
    Write-Log "Enter New-DockerNATNetwork"
    Start-ExecuteWithRetry {
        docker.exe network create --driver="nat" --opt "com.docker.network.windowsshim.disable_gatewaydns=true" "${DCOS_NAT_NETWORK_NAME}"
        if($LASTEXITCODE -ne 0) {
            Throw "Failed to create the new Docker NAT network with disable_gatewaydns flag"
        }
    }
    Write-Log "Exit New-DockerNATNetwork: created ${DCOS_NAT_NETWORK_NAME} network with flag: com.docker.network.windowsshim.disable_gatewaydns=true"
}

function Get-DCOSVersion {
    param([ref]$Reference)

    Write-Log "Enter Get-DCOSVersion"
    $timeout = 7200.0
    $startTime = Get-Date
    while(((Get-Date) - $startTime).TotalSeconds -lt $timeout) {
        foreach($ip in $MasterIPs) {
            try {
                $response = Invoke-WebRequest -UseBasicParsing -Uri "http://$ip/dcos-metadata/dcos-version.json"
            } catch {
                Write-Log "Invoke-WebRequest http://$ip/dcos-metadata/dcos-version.json failed with $($_.ToString())"
                continue
            }
            $Reference.Value = (ConvertFrom-Json -InputObject $response.Content).version
            Write-Log "Exit Get-DCOSVersion"
            return
        }
        Write-Log "Wait 30 seconds before try again"
        Start-Sleep -Seconds 30
    }
    Throw "ERROR: Cannot find the DC/OS version from any of the masters $($MasterIPs -join ', ') within a timeout of $timeout seconds"
}

function Get-MesosFlags {
    param([ref]$Reference)

    Write-Log "Enter Get-MesosFlags"
    $timeout = 7200.0
    $startTime = Get-Date
    while(((Get-Date) - $startTime).TotalSeconds -lt $timeout) {
        foreach($ip in $MasterIPs) {
            try {
                $response = Invoke-WebRequest -UseBasicParsing -Uri "http://${ip}:5050/flags"
            } catch {
                continue
            }
            $Reference.Value = (ConvertFrom-Json -InputObject $response.Content).flags
            Write-Log "Exit Get-MesosFlags"
            return
        }
        Start-Sleep -Seconds 30
    }
    Throw "ERROR: Cannot find the Mesos flags from any of the masters $($MasterIPs -join ', ') within a timeout of $timeout seconds"
}

function New-DCOSEnvironmentFile {
    Write-Log "Enter New-DCOSEnvironmentFile"
    if(!(Test-Path -Path $DCOS_DIR)) {
        New-Item -ItemType "Directory" -Path $DCOS_DIR
    }
    if($DcosVersion -eq "") {
        Write-Log "Trying to find the DC/OS version by querying the API of the masters: $($MasterIPs -join ', ')"
        Get-DCOSVersion -Reference ([ref] $DcosVersion)
    }
    Set-Content -Path $GLOBAL_ENV_FILE -Value @(
        "PROVIDER=azure",
        "DCOS_VERSION=${DcosVersion}"
    )
    Write-Log "Exit New-DCOSEnvironmentFile"
}

function New-DCOSMastersListFile {
    Write-Log "Enter New-DCOSMastersListFile"
    if(!(Test-Path -Path $DCOS_DIR)) {
        New-Item -ItemType "Directory" -Path $DCOS_DIR
    }
    ConvertTo-Json -InputObject $MasterIPs -Compress | Out-File -Encoding ascii -PSPath $MASTERS_LIST_FILE
    Write-Log "Exit New-DCOSMastersListFile"
}

function New-DCOSServiceWrapper {
    Write-Log "Enter New-DCOSServiceWrapper"
    $parent = Split-Path -Parent -Path $SERVICE_WRAPPER
    if(!(Test-Path -Path $parent)) {
        New-Item -ItemType "Directory" -Path $parent
    }
    Write-Log "Copying $SERVICE_WRAPPER_FILE file"
    Copy-item "${AgentBlobDirectory}\$SERVICE_WRAPPER_FILE" $SERVICE_WRAPPER -Force
    Write-Log "Exit New-DCOSServiceWrapper"
}

function Install-CommonComponents {
    Write-Log "Enter Install-CommonComponents"
    Install-VCredist -Installer $VCREDIST_2013_INSTALLER
    Install-VCredist -Installer $VCREDIST_2017_INSTALLER
    Write-Log "Exit Install-CommonComponents"
}

try {
    Write-Log "Setting up DCOS Windows Agent DcosVersion=[$DcosVersion]"
    Start-DockerConfiguration
    New-DockerNATNetwork
    Install-CommonComponents
    New-DCOSEnvironmentFile
    New-DCOSMastersListFile
    New-DCOSServiceWrapper
    Install-MesosAgent
    if($DcosVersion.StartsWith("1.8") -or $DcosVersion.StartsWith("1.9") -or $DcosVersion.StartsWith("1.10")) {
        Install-ErlangRuntime
        Install-EPMDAgent
        Install-SpartanAgent
    } else {
        # DC/OS release >= 1.11
        Install-DCOSNetAgent
    }
    Install-AdminRouterAgent
    $mesosFlags = ""
    Get-MesosFlags -Reference ([ref] $mesosFlags)
    $IsMatricsServiceInstalled = $false
    if($mesosFlags.authenticate_agents -eq "false") {
        # Install Metrics only if mesos_authentication is disabled
        Install-MetricsAgent
        $IsMatricsServiceInstalled = $true
    }
    # To get collect a complete list of services for node health monitoring,
    # the Diagnostics needs always to be the last one to install
    Install-DiagnosticsAgent -IncludeMatricsService $IsMatricsServiceInstalled
} catch {
    Write-Log "Error in setting up DCOS Windows Agent: $($_.ToString())"
    exit 1
}
Write-Log "Successfully finished setting up DCOS Windows Agent"
exit 0
