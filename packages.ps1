#
#
#  packages.ps1 - Provides the filename and location of each package required for installing 
#  functioning DC/OS master and agent. It is used by the script DCOSWindowsAgentSetup.ps1 and DCOSWindowsMasterSetup.ps1.
#
class Installable
{
    static [string] $ClassName = "Installable"
    Setup([string] $script_path) { Write-Host "Setup Installable  : $script_path"; }
}


function Setup-Installable {
    param (
           [HashTable] $PackageStatus,
           [string] $ScriptPath,
           [parameter(Mandatory=$true)] [string[]]$MasterAddress,
           [string]$AgentPrivateIP,
           [switch]$Public=$false
    )
    $obj = ( Get-Content "$ScriptPath/setupinfo.json" | ConvertFrom-Json)

    if ($PackageStatus[$obj.package_name]) 
    {
         Write-Host "$obj.package_name is already installed. skipping"
    }
    else {
        if ($obj.requires)
        {
            foreach ($depend in $obj.requires) {
                if ($PackageStatus[$depend]) 
                {
                    Write-Host "dependent $depend is already installed. skipping"
                }
                else 
                {
                    $depend_path = Join-Path -path $global:download_dir -child "packages\$depend"
                    Setup-Installable -PackageStatus $PackageStatus -ScriptPath $depend_path -MasterAddress $MasterAddress -AgentPrivateIP $agentIP -Public:$Public
                    if ($PackageStatus["$depend"]  -ne $true ) 
                    {
                        Write-Host "pre-requisite $depend install failed. Failing deploy"
                        throw "Could not install. pre-requisite $depend setup failed."
                    }
                }
            }
        }

        # It is possible that there is a reference to this package in another packages depends.
        # We should tolerate that. I think this could only happen if the last depend depended on this package
        # so that all of the depends were already done.

        if ($PackageStatus[$obj.package_name]) 
        {
             Write-Host "$obj.package_name is already installed. skipping"
        }
        else {
            $setup = [String] $obj.agent_setup
            Invoke-Expression ". '$ScriptPath/$setup'"

            $installer = New-Object $obj.package_name;
            $rslt = $installer.Setup($ScriptPath, $MasterAddress, $agentPrivateIP, $Public)
            $PackageStatus.Add($obj.package_name, $rslt)
        }
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



# Remote log server
$REMOTE_LOG_SERVER = "10.3.1.6"
$REMOTE_USER = "logs"
$REMOTE_KEY = Join-Path $env:SystemDrive "jenkins\workspace\key\logs_id_rsa.ppk"
$REMOTE_MESOS_BUILD_DIR = "/data/mesos-build"
$REMOTE_SPARTAN_BUILD_DIR = "/data/spartan-build"

# DCOS common configurations
$LOG_SERVER_BASE_URL = "http://dcos-win.westus.cloudapp.azure.com"
$ZOOKEEPER_PORT      = 2181
$EXHIBITOR_PORT      = 8181
$DCOS_DIR            = Join-Path $env:SystemDrive "DCOS"

# Mesos configurations
$MESOS_SERVICE_NAME  = "dcos-mesos-slave"
$MESOS_AGENT_PORT    = 5051
$MESOS_DIR           = Join-Path $DCOS_DIR "mesos"
$MESOS_BIN_DIR       = Join-Path $MESOS_DIR "bin"
$MESOS_WORK_DIR      = Join-Path $MESOS_DIR "work"
$MESOS_LOG_DIR       = Join-Path $MESOS_DIR "log"
$MESOS_SERVICE_DIR   = Join-Path $MESOS_DIR "service"
$MESOS_BUILD_DIR     = Join-Path $MESOS_DIR "build"
$MESOS_BINARIES_DIR  = Join-Path $MESOS_DIR "binaries"
$MESOS_GIT_REPO_DIR  = Join-Path $MESOS_DIR "mesos"
$MESOS_BUILD_OUT_DIR = Join-Path $MESOS_DIR "build-output"
$MESOS_BUILD_LOGS_DIR = Join-Path $MESOS_BUILD_OUT_DIR "logs"
$MESOS_BUILD_BINARIES_DIR = Join-Path $MESOS_BUILD_OUT_DIR "binaries"
$MESOS_BUILD_BASE_URL = "$LOG_SERVER_BASE_URL/mesos-build"

# EPMD configurations
#$EPMD_SERVICE_NAME = "dcos-epmd"
#$EPMD_PORT = 61420
#$EPMD_DIR = Join-Path $DCOS_DIR "epmd"
#$EPMD_SERVICE_DIR = Join-Path $EPMD_DIR "service"
#$EPMD_LOG_DIR = Join-Path $EPMD_DIR "log"


# Installers URLs
#$SERVICE_WRAPPER_URL = "$LOG_SERVER_BASE_URL/downloads/WinSW.NET4.exe"
#$VS2017_URL = "https://download.visualstudio.microsoft.com/download/pr/10930949/045b56eb413191d03850ecc425172a7d/vs_Community.exe"
#$CMAKE_URL = "https://cmake.org/files/v3.9/cmake-3.9.0-win64-x64.msi"
#$GNU_WIN32_URL = "https://10gbps-io.dl.sourceforge.net/project/gnuwin32/patch/2.5.9-7/patch-2.5.9-7-setup.exe"
#$GIT_URL = "$LOG_SERVER_BASE_URL/downloads/Git-2.14.1-64-bit.exe"
#$PYTHON_URL = "https://www.python.org/ftp/python/2.7.13/python-2.7.13.msi"
#$PUTTY_URL = "https://the.earth.li/~sgtatham/putty/0.70/w64/putty-64bit-0.70-installer.msi"
#$7ZIP_URL = "http://d.7-zip.org/a/7z1700-x64.msi"
#$VCREDIST_2013_URL = "https://download.microsoft.com/download/2/E/6/2E61CFA4-993B-4DD4-91DA-3737CD5CD6E3/vcredist_x64.exe"
#$DEVCON_CAB_URL = "https://download.microsoft.com/download/7/D/D/7DD48DE6-8BDA-47C0-854A-539A800FAA90/wdk/Installers/787bee96dbd26371076b37b13c405890.cab"

# Tools installation directories
#$GIT_DIR = Join-Path $env:ProgramFiles "Git"
#$CMAKE_DIR = Join-Path $env:ProgramFiles "CMake"
#$VS2017_DIR = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2017\Community"
#$GNU_WIN32_DIR = Join-Path ${env:ProgramFiles(x86)} "GnuWin32"
#$PYTHON_DIR = Join-Path $env:SystemDrive "Python27"
#$PUTTY_DIR = Join-Path $env:ProgramFiles "PuTTY"
#$7ZIP_DIR = Join-Path $env:ProgramFiles "7-Zip"

# Git repositories URLs
#$MESOS_GIT_URL = "https://github.com/apache/mesos"
#$SPARTAN_GIT_URL = "https://github.com/dcos/spartan"


$global:dcos_download = "$env:SystemDrive/dcos-download"


$global:NssmDir = "c:\nssm"
$global:NssmBinariesUrl = "http://nssm.cc/ci/nssm-2.24-101-g897c7ad.zip"
$global:NssmSourceRepo  = ""
$global:NssmBuildNumber = "-101-g897c7ad"
$global:NssmVersion     = "nssm-2.24"
$global:NssmFileType    = ".zip"
$global:NssmSha1        = ""

$global:MesosBinariesUri = "http://104.210.40.105/binaries/master/latest/"
$global:MesosSourceRepo  = ""
$global:MesosBuildNumber = ""
$global:MesosVersion     = "mesos"
$global:MesosFileType    = ".zip"
$global:MesosSha1        = ""
