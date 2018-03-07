#======================================================================================
# This script sets up the AdminRouter agent on a Windows node
# This AdminRouter consists IIS, Application Request Routing and URL Rewrite with a 
# set of a url rewrite rules
#======================================================================================
Param(
    [Parameter(Mandatory=$true)]
    [string]$AgentPrivateIP
)

Function Install-AdminModule{
  Param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$MSISourceUri,
        [Parameter(Mandatory=$true)][string]$DownloadDestFile,
        [Parameter(Mandatory=$true)][string]$Logfile
        )

    Write-Output "Download and install $Name module"
    Invoke-WebRequest -Uri $MSISourceUri -OutFile $DownloadDestFile
    $arguments= ' /qn /l*v ' + $Logfile

    Start-Process `
         -file  $DownloadDestFile `
         -arg $arguments `
         -passthru | wait-process
    if ($? -ne $true)
    {
        Write-Host "Could not install $Name module"
    }

}

Function Install-URLRewriteAndARRModules{

    $downloadDir = Join-Path $env:SystemDrive "dcos-download/"

    # URL Rewrite
    $UrlRewrite = "IIS URL Rewrite Module 2"
    $UrlRewritelogfile = "rewrite.log"
    [Uri]    $URLRewrite_Plugin_Uri  = 'http://download.microsoft.com/download/D/D/E/DDE57C26-C62C-4C59-A1BB-31D58B36ADA2/rewrite_amd64_en-US.msi'
    [string] $URLRewrite_Plugin_File = $downloadDir + "rewrite_amd64_en-US.msi"

    # Application Request Routing: AAR
    $ARR = "Microsoft Application Request Routing 3.0"
    $ARRlogfile = "apprequestrouting.log"
    [Uri]    $ARR_Plugin_Uri = "https://download.microsoft.com/download/E/9/8/E9849D6A-020E-47E4-9FD0-A023E99B54EB/requestRouter_amd64.msi"
    [string] $ARR_Plugin_File = $downloadDir + "requestRouter_amd64.msi"

	New-Item -Path $downloadDir -ItemType directory -Force

    Install-AdminModule -Name $UrlRewrite -MSISourceUri $URLRewrite_Plugin_Uri -DownloadDestFile $URLRewrite_Plugin_File -Logfile $ARRlogfile 
    Install-AdminModule -Name $ARR -MSISourceUri $ARR_Plugin_Uri -DownloadDestFile $ARR_Plugin_File -Logfile $UrlRewritelogfile

}

function Install-IISAndTools{
	$RequiredComponents = @("Web-Server","Web-Mgmt-Tools","Web-Request-Monitor", "Web-Http-Tracing")
	foreach ($component in $RequiredComponents) {
		$feature = get-windowsfeature | where { $_.Name -like $element }
		if ($feature.InstallState -Eq "Installed"){
		        Write-Host " 	$component was already installed"
        } else {
	  	    Install-WindowsFeature $component
		}
	}
}

function Open-AdminRouterPort {
    $AdminRouterPort = 61001
    Open-WindowsFirewallRule -Name "Allow inbound TCP Port $AdminRouterPort for AdminRouter" -Direction "Inbound" -LocalPort $AdminRouterPort -Protocol "TCP"
}

function Setup-AdminRouter(
           [string[]]$MasterAddress,
           [string]$AgentPrivateIP) {

    #  Install IIS and tools
    Install-IISAndTools
    import-module WebAdministration

 	#  Install URL Rewrite and Application Request Routing modules
    Install-URLRewriteAndARRModules

    # Setup the adminrouter server      
	Write-Host "Setting up the AdminRouter server"
	$adminRouterPath="IIS:\AppPools\AdminRouter"
	$existed = Test-Path -Path $adminRouterPath
	if (! $existed -eq $True)
	{
		Write-Host "creating IIS:\AppPools\AdminRouter"
		$adminappgroup = new-item "IIS:\AppPools\AdminRouter" -Force
	}

    Write-Host "Setting up new website at $agentPrivateIP with $env:SystemDrive:\inetpub\wwwroot\DCOS as root directory"
    $adminrouter_dir = new-item -ItemType "Directory" "$env:SystemDrive:\inetpub\wwwroot\DCOS" -force
    New-Website -Name "adminrouter" -Port 61001 -IPAddress $agentPrivateIP -PhysicalPath $adminrouter_dir -ApplicationPool "AdminRouter" -Force
}

function Add-Routes{
    # enable proxy
    Write-Output "Enabling proxy on Application Request Routing"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/proxy" -name "enabled" -value "True"

    # setup url rewrite rule for DC/OS Diagnostics service
    Write-Output "setup url rewrite rule for DC/OS Diagnostics service"
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules" -name "." -value @{name='Reverse Proxy for the DC/OS Diagnostics agent service';stopProcessing='True'}
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules/rule[@name='Reverse Proxy for the DC/OS Diagnostics agent service']/match" -name "url" -value "^system/health/v1(.*)"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules/rule[@name='Reverse Proxy for the DC/OS Diagnostics agent service']/action" -name "type" -value "Rewrite"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules/rule[@name='Reverse Proxy for the DC/OS Diagnostics agent service']/action" -name "url" -value "http://localhost:9003/{R:0}"
}


try {
    Write-Output "Setting up AdminRouter"
    Open-AdminRouterPort
    Setup-AdminRouter -AgentPrivateIP $AgentPrivateIP
    Add-Routes
} catch {
    Write-Output $_.ToString()
    exit 1 
}
    
Write-Output "Successfully finished setting up the AdminRouter"
exit 0
