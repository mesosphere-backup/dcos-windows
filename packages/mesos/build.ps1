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

# Mesos configurations
$MESOS_SERVICE_NAME = "dcos-mesos-slave"
$MESOS_AGENT_PORT = 5051
$MESOS_DIR = $pkgSrv
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

function New-Environment {
    Write-Output "Creating new tests environment"
    New-Directory $MESOS_BUILD_DIR
    New-Directory $MESOS_BINARIES_DIR
    New-Directory $MESOS_BUILD_OUT_DIR -RemoveExisting
    New-Directory $MESOS_BUILD_LOGS_DIR
    Add-Content -Path $ParametersFile -Value "BRANCH=$Branch"
    # Set Visual Studio variables based on tested branch
    Set-VCVariables "15.0"
    Write-Output "New tests environment was successfully created"
}

function Get-BuildLogsUrl {
    $buildOutUrl = Get-BuildOutputsUrl
    return "$buildOutUrl/logs"
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

function Start-MesosBuild {
    Write-Output "Building Mesos"
    Push-Location $MESOS_DIR
    $logsUrl = Get-BuildLogsUrl
    try {
        $generatorName = "Visual Studio 15 2017 Win64"
        $parameters = @("$MESOS_GIT_REPO_DIR", "-G", "`"$generatorName`"", "-T", "host=x64", "-DENABLE_LIBEVENT=ON", "-DHAS_AUTHENTICATION=ON", "-DENABLE_JAVA=ON")
        if($EnableSSL) {
            $parameters += "-DENABLE_SSL=ON"
        }
        Start-MesosCIProcess -ProcessPath "cmake.exe" -StdoutFileName "mesos-build-cmake-stdout.log" -StderrFileName "mesos-build-cmake-stderr.log" `
                             -ArgumentList $parameters -BuildErrorMessage "Mesos failed to build."
    } finally {
        Copy-CmakeBuildLogs -BuildName 'mesos-build'
        Pop-Location
    }
    Write-Output "Mesos was successfully built"
    Push-Location $MESOS_DIR
    try {
        Start-MesosCIProcess -ProcessPath "cmake.exe" -StdoutFileName "mesos-java-build-cmake-stdout.log" -StderrFileName "mesos-java-build-cmake-stderr.log" `
                             -ArgumentList @("--build", ".", "--target", "mesos-java") -BuildErrorMessage "mesos-java failed to build."
    } finally {
        Copy-CmakeBuildLogs -BuildName 'mesos-java-build'
        Pop-Location
    }
}


New-Environment
