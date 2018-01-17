# includes basic code from building

[CmdletBinding(DefaultParameterSetName="Standard")]
param(
    [string]
    [ValidateNotNullOrEmpty()]
    $pkgSrc,  # Location of the packages tree sources

    [string]
    [ValidateNotNullOrEmpty()]
    $pkgDest  # Location of the packages tree compiled binaries

)

$branch = "paul-pkgpanda"

# DCOS common configurations
$LOG_SERVER_BASE_URL = "http://dcos-win.westus.cloudapp.azure.com"

# Mesos configurationsG
$MESOS_SERVICE_NAME = "dcos-mesos-slave"
$MESOS_AGENT_PORT = 5051
$MESOS_DIR = "c:\mesos-tmp"
$MESOS_BIN_DIR = Join-Path $MESOS_DIR "bin"
$MESOS_WORK_DIR = Join-Path $MESOS_DIR "work"
$MESOS_LOG_DIR = Join-Path $MESOS_DIR "log"
$MESOS_SERVICE_DIR = Join-Path $MESOS_DIR "service"
$MESOS_BUILD_DIR = Join-Path $MESOS_DIR "build"
$MESOS_BINARIES_DIR = Join-Path $MESOS_DIR "binaries"
$MESOS_GIT_REPO_DIR = Join-Path $MESOS_DIR "mesos"
$MESOS_BUILD_OUT_DIR = Join-Path $MESOS_DIR "build-output"
$MESOS_BUILD_LOGS_DIR = Join-Path $MESOS_BUILD_OUT_DIR "logs"
$MESOS_BUILD_BINARIES_DIR = Join-Path $MESOS_BUILD_OUT_DIR "binaries"
$MESOS_BUILD_BASE_URL = "$LOG_SERVER_BASE_URL/mesos-build"

# Tools installation directories
$GIT_DIR = Join-Path $env:ProgramFiles "Git"
$CMAKE_DIR = Join-Path $env:ProgramFiles "CMake"
$VS2017_DIR = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2017\Professional"
$PATCH_DIR = Join-Path ${env:ProgramFiles(x86)} "GnuWin32"
$PYTHON_DIR = Join-Path $env:SystemDrive "Python27"
$PUTTY_DIR = Join-Path $env:ProgramFiles "PuTTY"
$7ZIP_DIR = Join-Path $env:ProgramFiles "7-Zip"

function Set-VCVariables {
    Param(
        [string]$Version="15.0",
        [string]$Platform="amd64"
    )
    if($Version -eq "15.0") {
        $vcPath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\"
    } else {
        $vcPath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio $Version\VC\"
    }
    Push-Location $vcPath
    try {
        $vcVars = Start-ExternalCommand { cmd.exe /c "vcvarsall.bat $Platform & set" } -ErrorMessage "Failed to get all VC variables"
        $vcVars | Foreach-Object {
            if ($_ -match "=") {
                $v = $_.split("=")
                Set-Item -Force -Path "ENV:\$($v[0])" -Value "$($v[1])"
            }
        }
    } catch {
        Pop-Location
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

function New-Environment {
    Write-Output "Creating new tests environment"
    New-Directory $MESOS_BUILD_DIR
    New-Directory $MESOS_BINARIES_DIR
    New-Directory $MESOS_BUILD_OUT_DIR -RemoveExisting
    New-Directory $MESOS_BUILD_LOGS_DIR
#    Add-Content -Path $ParametersFile -Value "BRANCH=$Branch"
    Set-LatestMesosCommit    
    # Set Visual Studio variables based on tested branch
    Set-VCVariables "15.0"
    Write-Output "New tests environment was successfully created"
}

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

function Set-LatestMesosCommit {
    Push-Location $MESOS_GIT_REPO_DIR
    try {
        Start-ExternalCommand { git.exe log -n 1 } -ErrorMessage "Failed to get the latest commit message for the Mesos git repo" | Out-File "$MESOS_BUILD_LOGS_DIR\latest-commit.log"
        $mesosCommitId = Start-ExternalCommand { git.exe log --format="%H" -n 1 } -ErrorMessage "Failed to get the latest commit id for the Mesos git repo"
        Set-Variable -Name "LATEST_COMMIT_ID" -Value $mesosCommitId -Scope Global -Option ReadOnly
    } finally {
        Pop-Location
    }
}

function Get-LatestCommitID {
    if(!$global:LATEST_COMMIT_ID) {
        Throw "Failed to get the latest Mesos commit ID. Perhaps it has not saved."
    }
    return $global:LATEST_COMMIT_ID
}

function Get-BuildOutputsUrl {
    $mesosCommitID = Get-LatestCommitID
    return "$MESOS_BUILD_BASE_URL/$Branch/$mesosCommitID"
}

function Get-BuildLogsUrl {
    $buildOutUrl = Get-BuildOutputsUrl
    return "$buildOutUrl/logs"
}

function Copy-CmakeBuildLogs {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$BuildName
    )
    Copy-Item "$MESOS_DIR\CMakeFiles\CMakeOutput.log" "$MESOS_BUILD_LOGS_DIR\$BuildName-CMakeOutput.log"
    Copy-Item "$MESOS_DIR\CMakeFiles\CMakeError.log" "$MESOS_BUILD_LOGS_DIR\$BuildName-CMakeError.log"
    if($global:BUILD_STATUS -eq 'FAIL') {
        $logsUrl = Get-BuildLogsUrl
        $global:LOGS_URLS += $("$logsUrl/$BuildName-CMakeOutput.log", "$logsUrl/$BuildName-CMakeError.log")
    }
}

function Wait-ProcessToFinish {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$ProcessPath,
        [Parameter(Mandatory=$false)]
        [String[]]$ArgumentList,
        [Parameter(Mandatory=$false)]
        [String]$StandardOutput,
        [Parameter(Mandatory=$false)]
        [String]$StandardError,
        [Parameter(Mandatory=$false)]
        [int]$Timeout=7200
    )
    $parameters = @{
        'FilePath' = $ProcessPath
        'NoNewWindow' = $true
        'PassThru' = $true
    }
    if ($ArgumentList.Count -gt 0) {
        $parameters['ArgumentList'] = $ArgumentList
    }
    if ($StandardOutput) {
        $parameters['RedirectStandardOutput'] = $StandardOutput
    }
    if ($StandardError) {
        $parameters['RedirectStandardError'] = $StandardError
    }
    $process = Start-Process @parameters
    $errorMessage = "The process $ProcessPath didn't finish successfully"
    try {
        Wait-Process -InputObject $process -Timeout $Timeout -ErrorAction Stop
        Write-Output "Process finished within the timeout of $Timeout seconds"
    } catch [System.TimeoutException] {
        Write-Output "The process $ProcessPath exceeded the timeout of $Timeout seconds"
        Stop-Process -InputObject $process -Force -ErrorAction SilentlyContinue
        Throw $_
    }
    if($process.ExitCode -ne 0) {
        Write-Output "$errorMessage. Exit code: $($process.ExitCode)"
        Throw $errorMessage
    }
}

function Start-MesosCIProcess {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$ProcessPath,
        [Parameter(Mandatory=$false)]
        [string[]]$ArgumentList,
#        [Parameter(Mandatory=$true)]
#        [string]$StdoutFileName,
#        [Parameter(Mandatory=$true)]
#        [string]$StderrFileName,
        [Parameter(Mandatory=$true)]
        [string]$BuildErrorMessage
    )
#    $stdoutFile = Join-Path $MESOS_BUILD_LOGS_DIR $StdoutFileName
##    $stderrFile = Join-Path $MESOS_BUILD_LOGS_DIR $StderrFileName
 #   New-Item -ItemType File -Path $stdoutFile
 #   New-Item -ItemType File -Path $stderrFile
 #   $logsUrl = Get-BuildLogsUrl
 #   $stdoutUrl = "${logsUrl}/${StdoutFileName}"
 #   $stderrUrl = "${logsUrl}/${StderrFileName}"
    $command = $ProcessPath -replace '\\', '\\'
    if($ArgumentList.Count) {
        $ArgumentList | Foreach-Object { $command += " $($_ -replace '\\', '\\')" }
    }
    try {
        Wait-ProcessToFinish -ProcessPath $ProcessPath -ArgumentList $ArgumentList 
#`
#                             -StandardOutput $stdoutFile -StandardError $stderrFile
        $msg = "Successfully executed: $command"
    } catch {
        $msg = "Failed command: $command"
        $global:BUILD_STATUS = 'FAIL'
        $global:LOGS_URLS += $($stdoutUrl, $stderrUrl)
        Write-Output "Exception: $($_.ToString())"
#        Add-Content -Path $ParametersFile -Value "FAILED_COMMAND=$command"
        Throw $BuildErrorMessage
    } finally {
        Write-Output $msg
  #      Write-Output "Stdout log available at: $stdoutUrl"
  #      Write-Output "Stderr log available at: $stderrUrl"
    }
}

function Start-MesosBuild {
    Write-Output "Creating mesos cmake makefiles"
    Push-Location $MESOS_BUILD_DIR
    $logsUrl = Get-BuildLogsUrl
    try {
        $generatorName = "Visual Studio 15 2017 Win64"
        $parameters = @("$MESOS_GIT_REPO_DIR", "-G", "`"$generatorName`"", "-T", "host=x64", "-DENABLE_LIBEVENT=1")

        Start-MesosCIProcess -ProcessPath "cmake.exe" -ArgumentList $parameters -BuildErrorMessage "Mesos cmake files failed to generate."
            #-StdoutFileName "mesos-build-cmake-stdout.log" -StderrFileName "mesos-build-cmake-stderr.log"
    } finally {
        Pop-Location
    }
    Write-Output "mesos cmake makefiles were generated successfully"

    Write-Output "Started building Mesos binaries"
    Push-Location $MESOS_BUILD_DIR

    try {

        Start-MesosCIProcess -ProcessPath "cmake.exe" -ArgumentList @("--build", ".", "--", "/m") -BuildErrorMessage "Mesos binaries failed to build."
        #-StdoutFileName "mesos-binaries-build-cmake-stdout.log" -StderrFileName "mesos-binaries-build-cmake-stderr.log" `
    } finally {
        Pop-Location
    }
    Write-Output "Mesos binaries were successfully built"


}


# Add all the tools to PATH
$toolsDirs = @("$CMAKE_DIR\bin", "$GIT_DIR\cmd", "$GIT_DIR\bin", "$PYTHON_DIR",
                "$PYTHON_DIR\Scripts", "$7ZIP_DIR", "$PATCH_DIR\bin")
$env:PATH += ';' + ($toolsDirs -join ';')

copy-item -Recurse "$pkgSrc" -destination "$MESOS_DIR"

New-Environment

Start-MesosBuild

#Copy build directory to destination directory. 
#For now we grab the whole lot
Copy-Item  -Path "$MESOS_BUILD_DIR\src\*" -Destination "$pkgDest" -Filter "*.exe"
#Copy-Item -path "$pkgSrc\extra\*" -Destination "$pkgDest"
