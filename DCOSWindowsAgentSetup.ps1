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
    [string]$MasterIP,
    [ValidateNotNullOrEmpty()]
    [string]$AgentPrivateIP,
    [ValidateNotNullOrEmpty()]
    [string]$BootstrapUrl,
    [AllowNull()]
    [switch]$isPublic = $false,
    [AllowNull()]
    [string]$MesosDownloadDir,
    [AllowNull()]
    [string]$MesosInstallDir,
    [AllowNull()]
    [string]$MesosLaunchDir,
    [AllowNull()]
    [string]$MesosWorkDir,
    [AllowNull()]
    [string]$customAttrs,
    [AllowNull()]
    [string]$DcosVersion = ""
)

$ErrorActionPreference = "Stop"

$AGENT_BLOB_ROOT_DIR = Join-Path $env:TEMP "blob"
$AGENT_BLOB_DIR = Join-Path $AGENT_BLOB_ROOT_DIR "agentblob"


filter Timestamp {
    "[$(Get-Date -Format o)] $_"
}

function Write-Log {
    Param(
        [string]$Message
    )
    $msg = $Message | Timestamp
    Write-Output $msg
}

function Start-ExecuteWithRetry {
    Param(
        [Parameter(Mandatory=$true)]
        [ScriptBlock]$ScriptBlock,
        [int]$MaxRetryCount=10,
        [int]$RetryInterval=3,
        [string]$RetryMessage,
        [array]$ArgumentList=@()
    )
    $currentErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $retryCount = 0
    while ($true) {
        Write-Log "Start-ExecuteWithRetry attempt $retryCount"
        try {
            $res = Invoke-Command -ScriptBlock $ScriptBlock `
                                  -ArgumentList $ArgumentList
            $ErrorActionPreference = $currentErrorActionPreference
            Write-Log "Start-ExecuteWithRetry terminated"
            return $res
        } catch [System.Exception] {
            $retryCount++
            if ($retryCount -gt $MaxRetryCount) {
                $ErrorActionPreference = $currentErrorActionPreference
                Write-Log "Start-ExecuteWithRetry exception thrown"
                throw
            } else {
                if($RetryMessage) {
                    Write-Log "Start-ExecuteWithRetry RetryMessage: $RetryMessage"
                } elseif($_) {
                    Write-Log "Start-ExecuteWithRetry Retry: $_.ToString()"
                }
                Start-Sleep $RetryInterval
            }
        }
    }
}

function Invoke-CurlRequest {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$URL,
        [Parameter(Mandatory=$true)]
        [string]$Destination,
        [Parameter(Mandatory=$false)]
        [int]$RetryCount=10
    )
    Start-ExecuteWithRetry -ScriptBlock {
        curl.exe -s -o `"$Destination`" `"$URL`"
        if($LASTEXITCODE) {
            Throw "Fail to download $URL to destination $Destination"
        }
    } -MaxRetryCount $RetryCount
}

function Install-7Zip {
    Write-Log "Download 7-Zip"
    $7ZipFileName = "7z1801-x64.msi"
    $7ZipMsiUrl = "$BootstrapUrl/$7ZipFileName"
    $7ZipMsiFile = Join-Path $env:TEMP $7ZipFileName
    if(Test-Path $7ZipMsiFile) {
        Remove-Item -Recurse -Force $7ZipMsiFile
    }
    Write-Log "Downloading $7ZipMsiUrl to $7ZipMsiFile"
    Measure-Command { Invoke-CurlRequest -URL $7ZipMsiUrl -Destination $7ZipMsiFile }
    Write-Log "7-Zip finished downloading"
    Write-Log "Installing 7-Zip"
    $parameters = @{
        'FilePath' = 'msiexec.exe'
        'ArgumentList' = @("/i", $7ZipMsiFile, "/qn")
        'Wait' = $true
        'PassThru' = $true
    }
    $p = Start-Process @parameters
    if($p.ExitCode -ne 0) {
        Throw "Failed to install $7ZipMsiFile"
    }
    $7ZipDir = Join-Path $env:ProgramFiles "7-Zip"
    Add-ToSystemPath $7ZipDir
    Remove-Item $7ZipMsiFile -ErrorAction SilentlyContinue
    Write-Log "7-Zip installed"
}

function Expand-7ZIPFile {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$File,
        [Parameter(Mandatory=$true)]
        [string]$DestinationPath
    )
    7z.exe x $File -mmt8 $("-o" + $DestinationPath)
    if($LASTEXITCODE) {
        Throw "Failed to expand $File"
    }
}

function Add-ToSystemPath {
    Param(
        [Parameter(Mandatory=$true)]
        [string[]]$Path
    )
    $systemPath = [System.Environment]::GetEnvironmentVariable('Path', 'Machine').Split(';')
    $currentPath = $env:PATH.Split(';')
    foreach($p in $Path) {
        if($p -notin $systemPath) {
            $systemPath += $p
        }
        if($p -notin $currentPath) {
            $currentPath += $p
        }
    }
    $env:PATH = $currentPath -join ';'
    setx.exe /M PATH ($systemPath -join ';')
    if($LASTEXITCODE) {
        Throw "Failed to set the new system path"
    }
}

function Get-MasterIPs {
    [string[]]$ips = ConvertFrom-Json $MasterIP
    # NOTE(ibalutoiu): ACS-Engine adds the Zookeper port to every master IP and we need only the address
    [string[]]$masterIPs = $ips | ForEach-Object { $_.Split(':')[0] }
    return ,$masterIPs
}

function Get-AgentBlobFiles {
    Write-Log "Enter Get-AgentBlobFiles"
    if(Test-Path $AGENT_BLOB_DIR) {
        Remove-Item -Recurse -Force $AGENT_BLOB_DIR
    }
    New-Item -ItemType Directory $AGENT_BLOB_DIR
    Write-Log "Download AgentBlob"
    $AgentBlobUrl = "$BootstrapUrl/windowsAgentBlob.zip"
    $blobPath = Join-Path $env:TEMP "windowsAgentBlob.zip"
    if(Test-Path $blobPath) {
        Remove-Item -Recurse -Force $blobPath
    }
    Write-Log "Downloading $AgentBlobUrl to $blobPath"
    Measure-Command { Invoke-CurlRequest -URL $AgentBlobUrl -Destination $blobPath }
    Write-Log "Extracting the agent blob from $blobPath to $AGENT_BLOB_ROOT_DIR"
    Measure-Command { Expand-7ZIPFile -File $blobPath -DestinationPath $AGENT_BLOB_ROOT_DIR }
    Remove-Item $blobPath -ErrorAction SilentlyContinue
    # Add extracted root directory to the current PATH. This is useful for calling utility binaries (i.e. logging)
    $env:PATH += ";${AGENT_BLOB_DIR}"
    Write-Log "Exit Get-AgentBlobFiles"
}

function Start-DCOSAgentSetup {
    Write-Log "Enter Start-DCOSAgentSetup"
    $masterIPs = Get-MasterIPs
    & "${AGENT_BLOB_DIR}\scripts\agent-setup.ps1" -AgentBlobDirectory $AGENT_BLOB_DIR `
                                                  -MasterIPs $masterIps `
                                                  -AgentPrivateIP $AgentPrivateIP `
                                                  -BootstrapUrl $BootstrapUrl `
                                                  -isPublic:$isPublic `
                                                  -customAttrs $customAttrs `
                                                  -DcosVersion $DcosVersion
    if($LASTEXITCODE) {
        Throw "DC/OS agent setup failed"
    }
    Write-Log "Exit Start-DCOSAgentSetup"
}


try {
    Install-7Zip
    Get-AgentBlobFiles
    Start-DCOSAgentSetup
} catch {
    Write-Log "ERROR: $($_.ToString())"
    Write-Output $_.ScriptStackTrace
    exit 1
}
exit 0
