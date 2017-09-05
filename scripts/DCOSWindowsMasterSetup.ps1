#
#  DCOSWindowsMasterSetup.ps1  -MasterIp <string> -MasterPrivateIP <string> -BootstapUri <uri>
#
[CmdletBinding(DefaultParameterSetName="Standard")]
param(
    [string]
    [ValidateNotNullOrEmpty()]
    $MasterIP,  # with port.  Could be a list of ipaddr:port or hostname:port

    [string]
    [ValidateNotNullOrEmpty()]
    $MasterPrivateIP,  # ie 10.0.0.5

    [string]
    [ValidateNotNullOrEmpty()]
    $BootstrapUrl,

    [string]
    [AllowNull()]
    $MesosDownloadDir, # ie c:mesos-download

    [string]
    [AllowNull()]
    $MesosInstallDir,  # ie c:\mesos

    [string]
    [AllowNull()]
    $MesosLaunchDir,  # ie c:\mesos\src

    [string]
    [AllowNull()]
    $MesosWorkDir   # ie c:\mesos\work
)


# Should/Will be parameter

. .\packages.ps1

$global:TestDcosBinariesUri = "https://dcosdevstorage.blob.core.windows.net/dcos-windows"

$global:DCOSWindowsBinariesUri = ""

$global:DCOSDownloadDir = "c:\dcos-staging"
$global:DcosDir = "c:\dcos"

$global:NssmDir = "c:\nssm"

$global:MesosInstallDir = "c:\mesos"
$global:MesosWorkDir    = "c:\mesos\work"
$global:MesosLaunchDir  = "c:\mesos\bin"

$global:MesosMasterIp = ""


function
Expand-ZIPFile($file, $destination)
{
    $shell = new-object -com shell.application
    $zip = $shell.NameSpace($file)
    foreach($item in $zip.items())
    {
        $shell.Namespace($destination).copyhere($item, 0x14)
    }
}


function 
Remove-Directory($dirname)
{

    try {
        #Get-ChildItem $dirname -Recurse | Remove-Item  -force -confirm:$false
        # This doesn't work because of long file names
        # But this does:
        Invoke-Expression ("cmd /C rmdir /s /q "+$dirname)
    }
    catch {
        # If this fails we don't want it to stop

    }
}

function
Get-DCOSBinaries($download_uri, $download_dir)
{
    # Get Mesos Binaries
    $zipfile = ( $global:MesosVersion+$global:MesosBuildNumber+$global:MesosFileType )

    Remove-Directory($download_dir)
    $dir = New-Item -ItemType "directory" -Path $download_dir  -force
    Invoke-WebRequest -Uri ($download_uri+"/"+$zipfile) -OutFile ($download_dir+"\"+$zipfile)

    Remove-Directory($global:MesosWorkDir)
    Remove-Directory($global:MesosLaunchDir)
    Remove-Directory($global:MesosLogDir)
    Remove-Directory($global:MesosInstallDir)

    $dir = New-Item -ItemType "directory" -Path $global:MesosInstallDir -force
    $dir = New-Item -ItemType "directory" -Path $global:MesosLaunchDir -force
    $dir = New-Item -ItemType "directory" -Path $global:MesosWorkDir -force
    $dir = New-Item -ItemType "directory" -Path $global:MesosLogDir -force

    Expand-ZIPFile -File ($download_dir+"\"+$zipfile) -Destination $global:MesosLaunchDir

    # Get nssm runtime
    $zipfile = $global:NssmVersion+$global:NssmBuildNumber+".zip"

    Invoke-WebRequest -Uri ($download_uri+"/"+$zipfile) -OutFile ($download_dir+"\"+$zipfile)

    Remove-Directory($global:NssmDir)
    $dir = New-Item -ItemType "directory" -Path $global:NssmDir -force
    Expand-ZIPFile -File ($download_dir+"\"+$zipfile) -Destination ($download_dir)
    Copy-Item -Path ($download_dir+"\"+$global:NssmVersion+$global:NssmBuildNumber+"\win64\nssm.exe") -Destination $global:NssmDir

    # Get Erlang Runtime

    # Get Spartan

    # Get NavStar

    # Get Minuteman
}



function
ValidateMasterIP($input_ip_str)
{
   # with port.  Could be a list of ipaddr:port or hostname:port
   #
   # Validate in a bit. For now, just accept it.

   try {
       $hostlist = ConvertFrom-Json $input_ip_str

       $hoststr = "zk://"
       for ($i = 0; $i -lt $hostlist.count; $i++ )
       {
           if ($i -gt 0) 
           {
              $hoststr += ","
           }
           $hoststr += $hostlist[$i]    
       }
       $hoststr += "/mesos"
   }
   catch {
       $hoststr = "invalid address"
   }

   return $hoststr
}


function
ValidateBootstrapURI($input_bootstrap_uri)
{
   # Check to see if the binaries are where we expect them
   # if not, we faqil
   #
   if ($false)
   {
       throw $input_bootstrap_uri+" is not a valid DC/OS download source"
       return $null;
   }
   else
   {
       return $input_bootstrap_uri
   }

}


try {
    
    throw "Windows Mesos Master is not yet implemented"

    $global:MesosMasterIp = ValidateMasterIP $MasterIP
    $global:DCOSWindowsBinariesUri = ValidateBootstrapURI $BootstrapUrl

    if ($MesosDownloadDir)
    {
        # This should be the download destination of the script and Great Big Zip file
        $global:DCOSDownloadDir = $MesosDownloadDir
    }
    else
    {
        $global:DCOSDownloadDir = "c:\dcos-download"
    }

    if ($MesosInstallDir)
    {
        # This should be the download destination of the script and Great Big Zip file
        $global:MesosInstallDir = $MesosInstallDir
    }
    else
    {
        $global:MesosInstallDir = "c:\mesos"
    }

    if ($MesosLaunchDir)
    {
        $global:MesosLaunchDir = $MesosLaunchDir
    }
    else
    {
        $global:MesosLaunchDir = $global:MesosInstallDir+"\bin"
    }

    if ($MesosWorkDir)
    {
        $global:MesosWorkDir = $MesosWorkDir
    }
    else
    {
        $global:MesosWorkDir = $global:MesosInstallDir+"\work"
    }

    if ($MesosLogDir)
    {
        $global:MesosLogkDir = $MesosLogDir
    }
    else
    {
        $global:MesosLogDir = $global:MesosInstallDir+"\log"
    }

    try {

        # If we don't shut down nssm we can't rebuild the directories
        # if nssm is running
        Write-Host "Stop and Remove Mesos Service"
        net stop Mesos
        Invoke-Expression ($global:NssmDir+"\nssm remove Mesos confirm" )
        Stop-Process -name "nssm*" -force
    }
    catch {

    }

    Write-Host "Get DC/OS Binaries"
    Get-DCOSBinaries $global:DCOSWindowsBinariesUri $global:DCOSDownloadDir
    
    Write-Host "Open up the Marathon port (5051)"
 
    netsh advfirewall firewall add rule name="mesos" dir=in  protocol=tcp localport=5051 action=allow

    Write-Host "Register Mesos Service"
    
    $mesos_run = (" --master="+$global:MesosMasterIp `
                     +" --work_dir="+$global:MesosWorkDir `
                     +" --runtime_dir="+$global:MesosWorkDir `
                     +" --launcher_dir="+$global:MesosLaunchDir `
                     +" --ip="+$AgentPrivateIP`
                     +" --isolation=windows/cpu,filesystem/windows --containerizers=docker,mesos")

Write-Host "run = "+$mesos_run

    Invoke-Expression ($global:NssmDir+"\nssm install Mesos "+$global:MesosLaunchDir+"\mesos-agent.exe" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppDirectory "+$global:MesosLaunchDir ) 
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppParameters "+$mesos_run )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos DisplayName Mesos" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos Description Mesos" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos Start SERVICE_AUTO_START" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos ObjectName LocalSystem" )
    # Invoke-Expression ($global:NssmDir+"\nssm set Mesos Type SERVICE_WIN32_OWN_PROCESS" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos Type SERVICE_INTERACTIVE_PROCESS" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppThrottle 1500" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppStdout "+$global:MesosLogDir+"\MesosStdOut.log" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppStderr "+$global:MesosLogDir+"\MesosStdOut.log" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppStdoutCreationDisposition 4" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppStderrCreationDisposition 4" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppRotateFiles 1" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppRotateOnline 1" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppRotateSeconds 86400" )
    Invoke-Expression ($global:NssmDir+"\nssm set Mesos AppRotateBytes 1048576" )
    net start Mesos
}
catch
{
   Write-Host "error: " +$_.Excption.message
}
