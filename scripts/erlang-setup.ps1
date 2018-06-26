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
    [string]$AgentBlobDirectory
)

$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables


$TEMPLATES_DIR = Join-Path $PSScriptRoot "Templates"


function Install-Erlang {
    New-Directory -RemoveExisting $ERLANG_DIR
    $erlangZip = Join-Path $AgentBlobDirectory "erlang.zip"
    Write-Log "Extracting the Windows Erlang zip @ $erlangZip to $ERLANG_DIR"
    Expand-7ZIPFile -File $erlangZip -DestinationPath $ERLANG_DIR
    Remove-File -Path $erlangZip -Fatal $false
    $binDir = "${ERTS_DIR}\bin" -replace '\\', '\\'
    $rootDir = $ERLANG_DIR -replace '\\', '\\'
    $context = @{
        'bin_dir' = $binDir
        'root_dir' = $rootDir
    }
    Start-RenderTemplate -TemplateFile "$TEMPLATES_DIR\erl.ini" -Context $context -OutFile "$ERLANG_DIR\bin\erl.ini"
    Start-RenderTemplate -TemplateFile "$TEMPLATES_DIR\erl.ini" -Context $context -OutFile "$ERTS_DIR\bin\erl.ini"
}

try {
    Install-Erlang
} catch {
    Write-Log $_.ToString()
    exit 1
}
Write-Log "Successfully installed the Erlang runtime"
exit 0
