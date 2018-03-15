$LOG_SERVER_BASE_URL = "http://dcos-win.westus.cloudapp.azure.com"
# Installers URLs
$SERVICE_WRAPPER_URL = "$LOG_SERVER_BASE_URL/downloads/WinSW.NET4.exe"
$VS2017_URL = "https://download.visualstudio.microsoft.com/download/pr/10930949/045b56eb413191d03850ecc425172a7d/vs_Community.exe"
$CMAKE_URL = "https://cmake.org/files/v3.9/cmake-3.9.0-win64-x64.msi"
$GNU_WIN32_URL = "https://10gbps-io.dl.sourceforge.net/project/gnuwin32/patch/2.5.9-7/patch-2.5.9-7-setup.exe"
$GIT_URL = "$LOG_SERVER_BASE_URL/downloads/Git-2.14.1-64-bit.exe"
$PYTHON_URL = "https://www.python.org/ftp/python/2.7.13/python-2.7.13.msi"
$PUTTY_URL = "https://the.earth.li/~sgtatham/putty/0.70/w64/putty-64bit-0.70-installer.msi"
$7ZIP_URL = "http://d.7-zip.org/a/7z1700-x64.msi"
$VCREDIST_2013_URL = "https://download.microsoft.com/download/2/E/6/2E61CFA4-993B-4DD4-91DA-3737CD5CD6E3/vcredist_x64.exe"
$DEVCON_CAB_URL = "https://download.microsoft.com/download/7/D/D/7DD48DE6-8BDA-47C0-854A-539A800FAA90/wdk/Installers/787bee96dbd26371076b37b13c405890.cab"
# Tools installation directories
$GIT_DIR = Join-Path $env:ProgramFiles "Git"
$CMAKE_DIR = Join-Path $env:ProgramFiles "CMake"
$VS2017_DIR = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2017\Community"
$GNU_WIN32_DIR = Join-Path ${env:ProgramFiles(x86)} "GnuWin32"
$PYTHON_DIR = Join-Path $env:SystemDrive "Python27"
$PUTTY_DIR = Join-Path $env:ProgramFiles "PuTTY"
$7ZIP_DIR = Join-Path $env:ProgramFiles "7-Zip"

function Install-Prerequisites {
    $prerequisites = @{
        'git'= @{
            'url'= $GIT_URL
            'install_args' = @("/SILENT")
            'install_dir' = $GIT_DIR
        }
        'cmake'= @{
            'url'= $CMAKE_URL
            'install_args'= @("/quiet")
            'install_dir'= $CMAKE_DIR
        }
        'gnuwin32'= @{
            'url'= $GNU_WIN32_URL
            'install_args'= @("/VERYSILENT","/SUPPRESSMSGBOXES","/SP-")
            'install_dir'= $GNU_WIN32_DIR
        }
        'python27'= @{
            'url'= $PYTHON_URL
            'install_args'= @("/qn")
            'install_dir'= $PYTHON_DIR
        }
        'putty'= @{
            'url'= $PUTTY_URL
            'install_args'= @("/q")
            'install_dir'= $PUTTY_DIR
        }
        '7zip'= @{
            'url'= $7ZIP_URL
            'install_args'= @("/q")
            'install_dir'= $7ZIP_DIR
        }
        'vs2017'= @{
            'url'= $VS2017_URL
            'install_args'= @(
                "--quiet",
                "--add", "Microsoft.VisualStudio.Component.CoreEditor",
                "--add", "Microsoft.VisualStudio.Workload.NativeDesktop",
                "--add", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "--add", "Microsoft.VisualStudio.Component.VC.DiagnosticTools",
                "--add", "Microsoft.VisualStudio.Component.Windows10SDK.15063.Desktop",
                "--add", "Microsoft.VisualStudio.Component.VC.CMake.Project",
                "--add", "Microsoft.VisualStudio.Component.VC.ATL"
            )
            'install_dir'= $VS2017_DIR
        }
    }
    foreach($program in $prerequisites.Keys) {
        if(Test-Path $prerequisites[$program]['install_dir']) {
            Write-Output "$program is already installed"
            continue
        }
        Write-Output "Downloading $program from $($prerequisites[$program]['url'])"
        $fileName = $prerequisites[$program]['url'].Split('/')[-1]
        $programFile = Join-Path $env:TEMP $fileName
        Invoke-WebRequest -UseBasicParsing -Uri $prerequisites[$program]['url'] -OutFile $programFile
        $parameters = @{
            'FilePath' = $programFile
            'ArgumentList' = $prerequisites[$program]['install_args']
            'Wait' = $true
            'PassThru' = $true
        }
        if($programFile.EndsWith('.msi')) {
            $parameters['FilePath'] = 'msiexec.exe'
            $parameters['ArgumentList'] += @("/i", $programFile)
        }
        Write-Output "Installing $programFile"
        $p = Start-Process @parameters
        if($p.ExitCode -ne 0) {
            Throw "Failed to install prerequisite $programFile during the environment setup"
        }
    }
    # Add all the tools to PATH
    $toolsDirs = @("$CMAKE_DIR\bin", "$GIT_DIR\cmd", "$GIT_DIR\bin", "$PYTHON_DIR",
                   "$PYTHON_DIR\Scripts", "$7ZIP_DIR", "$GNU_WIN32_DIR\bin")
    $env:PATH += ';' + ($toolsDirs -join ';')
}

function Start-MesosCIProcess {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$ProcessPath,
        [Parameter(Mandatory=$false)]
        [string[]]$ArgumentList,
        [Parameter(Mandatory=$true)]
        [string]$StdoutFileName,
        [Parameter(Mandatory=$true)]
        [string]$StderrFileName,
        [Parameter(Mandatory=$true)]
        [string]$BuildErrorMessage
    )
    $stdoutFile = Join-Path $MESOS_BUILD_LOGS_DIR $StdoutFileName
    $stderrFile = Join-Path $MESOS_BUILD_LOGS_DIR $StderrFileName
    New-Item -ItemType File -Path $stdoutFile
    New-Item -ItemType File -Path $stderrFile
    $logsUrl = Get-BuildLogsUrl
    $stdoutUrl = "${logsUrl}/${StdoutFileName}"
    $stderrUrl = "${logsUrl}/${StderrFileName}"
    $command = $ProcessPath -replace '\\', '\\'
    if($ArgumentList.Count) {
        $ArgumentList | Foreach-Object { $command += " $($_ -replace '\\', '\\')" }
    }
    try {
        Wait-ProcessToFinish -ProcessPath $ProcessPath -ArgumentList $ArgumentList `
                             -StandardOutput $stdoutFile -StandardError $stderrFile
        $msg = "Successfully executed: $command"
    } catch {
        $msg = "Failed command: $command"
        $global:BUILD_STATUS = 'FAIL'
        $global:LOGS_URLS += $($stdoutUrl, $stderrUrl)
        Write-Output "Exception: $($_.ToString())"
        Add-Content -Path $ParametersFile -Value "FAILED_COMMAND=$command"
        Throw $BuildErrorMessage
    } finally {
        Write-Output $msg
        Write-Output "Stdout log available at: $stdoutUrl"
        Write-Output "Stderr log available at: $stderrUrl"
    }
}




try {
    Install-Prerequisites
} catch {
    $errMsg = $_.ToString()
    Write-Output $errMsg
    write-host("Failed to set up container")
    exit 1
} finally {
}
write-host("Container successfully set up")
exit 0
