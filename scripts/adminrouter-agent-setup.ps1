#======================================================================================
# This script sets up the AdminRouter agent on a Windows node
# This AdminRouter consists IIS, Application Request Routing and URL Rewrite with a 
# set of a url rewrite rules
#======================================================================================
Param(
    [Parameter(Mandatory=$true)]
    [string]$AgentPrivateIP
)

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables

Function Install-AdminModule{
  Param(
        [Parameter(Mandatory=$true)][string]$Name,
        [Parameter(Mandatory=$true)][string]$MSISourceUri,
        [Parameter(Mandatory=$true)][string]$DownloadDestFile,
        [Parameter(Mandatory=$true)][string]$Logfile
        )

    Write-Output "Download and install $Name module"
    Start-ExecuteWithRetry { Invoke-WebRequest -Uri $MSISourceUri -OutFile $DownloadDestFile }

    $MSIArguments = @(
        "/i"
        ('"{0}"' -f $DownloadDestFile)
        "/qn"
        "/norestart"
        "/L*v"
        $Logfile
    )
    $p = Start-Process "msiexec.exe" -ArgumentList $MSIArguments -Wait -PassThru -NoNewWindow

    if($p.ExitCode -ne 0) {
        Throw "ERROR: Could not install $Name module"
    }
}

Function Install-URLRewriteAndARRModules{
    $downloadDir = Join-Path $env:TEMP "dcos-download/"
    # URL Rewrite
    $UrlRewrite = "IIS URL Rewrite Module 2"
    $UrlRewritelogfile = "rewrite.log"
    [string] $URLRewrite_Plugin_File = $downloadDir + "rewrite_amd64_en-US.msi"

    # Application Request Routing: AAR
    $ARR = "Microsoft Application Request Routing 3.0"
    $ARRlogfile = "apprequestrouting.log"
    [string] $ARR_Plugin_File = $downloadDir + "requestRouter_amd64.msi"
	New-Item -Path $downloadDir -ItemType directory -Force
    Install-AdminModule -Name $UrlRewrite -MSISourceUri $URL_REWRITE_MODULE_URL -DownloadDestFile $URLRewrite_Plugin_File -Logfile $ARRlogfile 
    Install-AdminModule -Name $ARR -MSISourceUri $ARR_MODULE_URL -DownloadDestFile $ARR_Plugin_File -Logfile $UrlRewritelogfile
}

function Install-IISAndTools{
    $RequiredComponents = @("Web-Server","Web-Mgmt-Tools","Web-Request-Monitor", "Web-Http-Tracing")
    foreach ($component in $RequiredComponents) {
        $feature = Get-WindowsFeature -Name $component
        if ($feature.InstallState -Eq "Installed"){
            Write-Output " 	$component was already installed"
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

    # Remove default IIS website listening on port 80
    Get-Website 'Default Web Site' | Remove-Website -Confirm:$false

    #  Install URL Rewrite and Application Request Routing modules
    Install-URLRewriteAndARRModules

    # Setup the adminrouter server      
    Write-Output "Setting up the AdminRouter server"
    $adminRouterPath="IIS:\AppPools\AdminRouter"
    $existed = Test-Path -Path $adminRouterPath
    if (! $existed -eq $True) {
        Write-Output "creating IIS:\AppPools\AdminRouter"
        $adminappgroup = new-item "IIS:\AppPools\AdminRouter" -Force
    }

    Write-Output "Setting up new website at $agentPrivateIP with $env:SystemDrive:\inetpub\wwwroot\DCOS as root directory"
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
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules/rule[@name='Reverse Proxy for the DC/OS Diagnostics agent service']/action" -name "url" -value "http://localhost:$DIAGNOSTICS_AGENT_PORT/{R:0}"

    Write-Output "setup url rewrite rule for DC/OS Metrics service"
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules" -name "." -value @{name='Reverse Proxy for the DC/OS Metrics agent service';stopProcessing='True'}
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules/rule[@name='Reverse Proxy for the DC/OS Metrics agent service']/match" -name "url" -value "^system/v1/metrics/(.*)"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules/rule[@name='Reverse Proxy for the DC/OS Metrics agent service']/action" -name "type" -value "Rewrite"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST'  -filter "system.webServer/rewrite/rules/rule[@name='Reverse Proxy for the DC/OS Metrics agent service']/action" -name "url" -value "http://localhost:$METRICS_AGENT_PORT/{R:1}"


    # setup up outbound rules
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/preConditions" -name "." -value @{name='IsHTML';logicalGrouping='MatchAll'}
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/preConditions/preCondition[@name='IsHTML']" -name "." -value @{input='{RESPONSE_CONTENT_TYPE}';pattern='^text/html'}
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/preConditions" -name "." -value @{name='IsRedirection'}
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/preConditions/preCondition[@name='IsRedirection']" -name "." -value @{input='{RESPONSE_STATUS}';pattern='3\d\d'}
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules" -name "." -value @{name='outbound rewrite';preCondition='IsHTML'}
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='outbound rewrite']/match" -name "filterByTags" -value "A"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='outbound rewrite']/match" -name "pattern" -value "^(.*)"
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='outbound rewrite']/conditions" -name "." -value @{input='{URL}';pattern='^/(system/health/v1|system/v1/metrics)/.\*'}
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='outbound rewrite']/action" -name "type" -value "Rewrite"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='outbound rewrite']/action" -name "value" -value "/{C:1}/{R:1}"
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules" -name "." -value @{name='Rewrite location header';preCondition='IsRedirection'}
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='Rewrite location header']/match" -name "serverVariable" -value "RESPONSE_Location"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='Rewrite location header']/match" -name "pattern" -value "http://[^/]+/(.*)"
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='Rewrite location header']/conditions" -name "." -value @{input='{ORIGINAL_HOST}';pattern='.+'}
    Add-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='Rewrite location header']/conditions" -name "." -value @{input='{URL}';pattern='^(system/health/v1|system/v1/metrics)/.\*'}
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='Rewrite location header']/action" -name "type" -value "Rewrite"
    Set-WebConfigurationProperty -pspath 'MACHINE/WEBROOT/APPHOST/adminrouter'  -filter "system.webServer/rewrite/outboundRules/rule[@name='Rewrite location header']/action" -name "value" -value "http://{ORIGINAL_URL}/{C:1}/{R:1}"
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
