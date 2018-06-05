$ErrorActionPreference = "Stop"

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables


$TEMPLATES_DIR = Join-Path $PSScriptRoot "templates"



function Install-VCredist {
    Write-Log "Install VCredist 2013"
    $installerPath = Join-Path $AGENT_BLOB_DEST_DIR $VCREDIST_2013_INSTALLER
    Write-Log "Install VCredist 2013 from $installerPath"
    $p = Start-Process -Wait -PassThru -FilePath $installerPath -ArgumentList @("/install", "/passive")
    if ($p.ExitCode -ne 0) {
        Throw ("Failed install VCredist 2013. Exit code: {0}" -f $p.ExitCode)
    }
    Write-Log "Finished to install VCredist 2013 x64"
    Remove-File -Path $installerPath -Fatal $false
}

function Install-Erlang {
    New-Directory -RemoveExisting $ERLANG_DIR
    $erlangZip = Join-Path $AGENT_BLOB_DEST_DIR "erlang.zip"
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
    Install-VCredist
    Install-Erlang
} catch {
    Write-Log $_.ToString()
    exit 1
}
Write-Log "Successfully installed the Erlang runtime"
exit 0
