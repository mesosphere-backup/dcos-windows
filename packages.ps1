#
#
#  packages.ps1 - Provides the filename and location of each package required for installing 
#  functioning DC/OS master and agent. It is used by the script DCOSWindowsAgentSetup.ps1 and DCOSWindowsMasterSetup.ps1.
#

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

$global:ErlangBinariesUri = ""
$global:ErlangSourceRepo  = ""
$global:ErlangBuildNumber = ""
$global:ErlangVersion     = "otp_win64_19.3"
$global:ErlangFileType    = ".zip"
$global:ErlangSha1        = ""

$global:SpartanBinariesUri = ""
$global:SpartanSourceRepo  = ""
$global:SpartanBuildNumber = ""
$global:SpartanVersion     = "dcos-spartan"
$global:SpartanFileType    = ".zip"
$global:SpartanSha1        = ""

$global:NavstarBinariesUri = ""
$global:NavstarSourceRepo  = ""
$global:NavstarBuildNumber = ""
$global:NavstarVersion     = "dcos-navstar"
$global:NavstarFileType    = ".zip"
$global:NavstarSha1        = ""

$global:MinuteManBinariesUri = ""
$global:MinuteManSourceRepo  = ""
$global:MinuteManBuildNumber = ""
$global:MinuteManVersion     = "dcos-minuteman"
$global:MinuteManFileType    = ".zip"
$global:MinuteManSha1        = ""

$global:AdminrouterBinariesUri = ""
$global:AdminrouterSourceRepo  = ""
$global:AdminrouterBuildNumber = ""
$global:AdminrouterVersion     = "dcos-adminrouter"
$global:AdminrouterFileType    = ".zip"
$global:AdminrouterSha1        = ""

$global:NginxBinariesUri = ""
$global:NginxSourceRepo  = ""
$global:NginxBuildNumber = ""
$global:NginxVersion     = "ngnix"
$global:NginxFileType    = ".zip"
$global:NginxSha1        = ""
