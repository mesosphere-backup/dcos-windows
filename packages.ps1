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
        [string] $script_path
    )
    $obj = ( Get-Content "$script_path/setupinfo.json" | ConvertFrom-Json)
    $setup = [String] $obj.agent_setup
    Invoke-Expression ". '$script_path/$setup'"

    $installer = New-Object $obj.package_name;
    $installer.Setup($script_path)
}

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
