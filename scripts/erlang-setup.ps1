$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables


$TEMPLATES_DIR = Join-Path $PSScriptRoot "templates"



function Install-VCredist {
    Write-Output "Install VCredist 2013"
    $installerPath = Join-Path $env:TEMP "vcredist_2013_x64.exe"
    Start-ExecuteWithRetry { Invoke-WebRequest -UseBasicParsing -Uri $VCREDIST_2013_URL -OutFile $installerPath }
    $p = Start-Process -Wait -PassThru -FilePath $installerPath -ArgumentList @("/install", "/passive")
    if ($p.ExitCode -ne 0) {
        Throw ("Failed install VCredist 2013. Exit code: {0}" -f $p.ExitCode)
    }
    Write-Output "Finished to install VCredist 2013 x64"
    Remove-File -Path $installerPath -Fatal $false
}

function Install-Erlang {
    New-Directory -RemoveExisting $ERLANG_DIR
    $erlangZip = Join-Path $env:TEMP "erlang.zip"
    Write-Output "Downloading the Windows Erlang runtime zip"
    Start-ExecuteWithRetry { Invoke-WebRequest -UseBasicParsing -Uri $ERLANG_URL -OutFile $erlangZip }
    Write-Output "Extracting the Windows Erlang zip to $ERLANG_DIR"
    Expand-Archive -LiteralPath $erlangZip -DestinationPath $ERLANG_DIR
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
    Install-VCredist
    Install-Erlang
} catch {
    Write-Output $_.ToString()
    exit 1
}
Write-Output "Successfully installed the Erlang runtime"
exit 0
