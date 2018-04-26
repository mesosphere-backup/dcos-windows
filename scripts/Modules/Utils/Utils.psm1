# Copyright 2017 Cloudbase Solutions Srl
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

$templating = (Resolve-Path "$PSScriptRoot\..\Templating").Path
Import-Module $templating


function Start-ExternalCommand {
    <#
    .SYNOPSIS
    Helper function to execute a script block and throw an exception in case of error.
    .PARAMETER ScriptBlock
    Script block to execute
    .PARAMETER ArgumentList
    A list of parameters to pass to Invoke-Command
    .PARAMETER ErrorMessage
    Optional error message. This will become part of the exception message we throw in case of an error.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [Alias("Command")]
        [ScriptBlock]$ScriptBlock,
        [array]$ArgumentList=@(),
        [string]$ErrorMessage
    )
    PROCESS {
        if($LASTEXITCODE){
            # Leftover exit code. Some other process failed, and this
            # function was called before it was resolved.
            # There is no way to determine if the ScriptBlock contains
            # a powershell commandlet or a native application. So we clear out
            # the LASTEXITCODE variable before we execute. By this time, the value of
            # the variable is not to be trusted for error detection anyway.
            $LASTEXITCODE = ""
        }
        $res = Invoke-Command -ScriptBlock $ScriptBlock -ArgumentList $ArgumentList
        if ($LASTEXITCODE) {
            if(!$ErrorMessage){
                Throw ("Command exited with status: {0}" -f $LASTEXITCODE)
            }
            Throw ("{0} (Exit code: $LASTEXITCODE)" -f $ErrorMessage)
        }
        return $res
    }
}

function New-Directory {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$Path,
        [Parameter(Mandatory=$false)]
        [switch]$RemoveExisting
    )
    if(Test-Path $Path) {
        if($RemoveExisting) {
            # Remove if it already exist
            Remove-Item -Recurse -Force $Path
        } else {
            return
        }
    }
    return (New-Item -ItemType Directory -Path $Path)
}

function Start-RenderTemplate {
    Param(
        [Parameter(Mandatory=$true)]
        [HashTable]$Context,
        [Parameter(Mandatory=$true)]
        [string]$TemplateFile,
        [Parameter(Mandatory=$false)]
        [string]$OutFile
    )
    $content = Invoke-RenderTemplateFromFile -Context $Context -Template $TemplateFile
    if($OutFile) {
        [System.IO.File]::WriteAllText($OutFile, $content)
    } else {
        return $content
    }
}

function Open-WindowsFirewallRule {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$Name,
        [ValidateSet("Inbound", "Outbound")]
        [string]$Direction,
        [ValidateSet("TCP", "UDP")]
        [string]$Protocol,
        [Parameter(Mandatory=$false)]
        [string]$LocalAddress="0.0.0.0",
        [Parameter(Mandatory=$true)]
        [int]$LocalPort
    )
    Write-Output "Open firewall rule: $Name"
    $firewallRule = Get-NetFirewallRule -DisplayName $Name -ErrorAction SilentlyContinue
    if($firewallRule) {
        Write-Output "Firewall rule already exist"
        return
    }
    New-NetFirewallRule -DisplayName $Name -Direction $Direction -LocalPort $LocalPort -Protocol $Protocol -Action Allow | Out-Null
}

function New-DCOSWindowsService {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$WrapperPath,
        [Parameter(Mandatory=$true)]
        [string]$Name,
        [Parameter(Mandatory=$false)]
        [string]$DisplayName,
        [Parameter(Mandatory=$false)]
        [string]$Description,
        [Parameter(Mandatory=$true)]
        [string]$BinaryPath,
        [Parameter(Mandatory=$false)]
        [string]$LogFile,
        [Parameter(Mandatory=$false)]
        [string[]]$EnvironmentFiles,
        [Parameter(Mandatory=$false)]
        [string]$PreStartCommand
    )
    $service = Get-Service -Name $Name -ErrorAction SilentlyContinue
    if($service) {
        Write-Output "The service $Name already exists"
        return
    }

    $params = @{
        'Name' = $Name
        'StartupType' = 'Automatic'
        'Confirm' = $false
    }

    if($DisplayName) {
        $params['DisplayName'] = $DisplayName
    }

    if($Description) {
        $params['Description'] = $Description
    }

    $binaryPathName = "`"$WrapperPath`" --service-name `"$Name`""

    if($PreStartCommand) {
        $binaryPathName += " --exec-start-pre `"$PreStartCommand`""
    }

    foreach($file in $EnvironmentFiles) {
        $binaryPathName += " --environment-file `"$file`""
    }

    $serviceActions = "actions=restart/0/restart/0/restart/30000"
    $failureCommand = ""

    if($LogFile) {
        $binaryPathName += " --log-file `"$LogFile`""
        # This is a multiline string needed to execute in a shell.
        # Please mind how you use and remove blank characters
        $failureCommand = `
@"
powershell.exe 
Move-Item -Path \"$LogFile\" -Destination \"$LogFile`_`$(get-date -f yyyy-MM-dd-hh-mm-ss)\";
Restart-Service $Name;
"@
        $serviceActions = "actions=restart/0/restart/0/run/30000"
    }

    $binaryPathName += " $BinaryPath"
    $params['BinaryPathName'] = $binaryPathName
    New-Service @params | Out-Null

    if($LogFile) {
        Start-ExternalCommand { sc.exe failure $Name reset=40 command="$failureCommand" $serviceActions }
    } else {
        Start-ExternalCommand { sc.exe failure $Name reset=40 $serviceActions }
    }
    Start-ExternalCommand { sc.exe failureflag $Name 1 }
}

function Start-ExecuteWithRetry {
    Param(
        [Parameter(Mandatory=$true)]
        [Alias("Command")]
        [ScriptBlock]$ScriptBlock,
        [int]$MaxRetryCount=10,
        [int]$RetryInterval=3,
        [array]$ArgumentList=@()
    )
    $currentErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $retryCount = 0
    while ($true) {
        try {
            $res = Invoke-Command -ScriptBlock $ScriptBlock `
                                  -ArgumentList $ArgumentList
            $ErrorActionPreference = $currentErrorActionPreference
            return $res
        } catch [System.Exception] {
            $retryCount++
            if ($retryCount -gt $MaxRetryCount) {
                $ErrorActionPreference = $currentErrorActionPreference
                Throw
            } else {
                Start-Sleep $RetryInterval
            }
        }
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
