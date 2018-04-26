#======================================================================================
# This script sets up the AdminRouter agent on a Windows node
# This AdminRouter was implemented on top of the Apache HTTP server + config with
# reverse proxy settings
#======================================================================================
Param(
    [Parameter(Mandatory=$true)]
    [string]$AgentPrivateIP
)

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables

$TEMPLATES_DIR = Join-Path $PSScriptRoot "templates"

function New-AdminRouterEnvironment {
    $service = Get-Service $ADMINROUTER_SERVICE_NAME -ErrorAction SilentlyContinue
    if($service) {
        Stop-Service -Force -Name $ADMINROUTER_SERVICE_NAME
        & sc.exe delete $ADMINROUTER_SERVICE_NAME
        if($LASTEXITCODE) {
            Throw "Failed to delete exiting $ADMINROUTER_SERVICE_NAME service"
        }
        Write-Output "Deleted existing $ADMINROUTER_SERVICE_NAME service"
    }
    New-Directory -RemoveExisting $ADMINROUTER_DIR
    New-Directory $ADMINROUTER_LOG_DIR
}

function Get-ApacheServerPackage {
    Write-Output "Downloading Apache HTTP Server zip file: $WINDOWS_APACHEL_HTTP_SERVER_URL"
    $filesPath = Join-Path $env:TEMP "httpd-2.4.33-win64-VC15.zip"
    Start-ExecuteWithRetry { Invoke-WebRequest -UseBasicParsing -Uri $WINDOWS_APACHEL_HTTP_SERVER_URL -OutFile $filesPath}

    # To be sure that a download is intact and has not been tampered with from the original download side
    # we need to perform biniary hash validation
    Write-Output "Validating Hash value for downloaded file: $filesPath"
    $hashFromDowloadedFile = Get-FileHash $filesPath -Algorithm SHA256

    # validate both hashes are the same
    if ($hashFromDowloadedFile.Hash -eq $WINDOWS_APACHEL_HTTP_SERVER_SHA256) {
        Write-Output 'Hash validation test passed'
    } else {
        Throw 'Hash validation test FAILED!!'
    }

    Write-Output "Extracting binaries archive in: $ADMINROUTER_DIR"
    Expand-Archive -LiteralPath $filesPath -DestinationPath $ADMINROUTER_DIR -Force
    Remove-item $filesPath
    Write-Output "Finshed downloading"  
}

function Generate-ApacheConfigFromTemplate {
    Param(
        [Parameter(Mandatory=$true)]
        [string]$TemplateFile,
        [Parameter(Mandatory=$true)]
        [string]$OutputFile
    )
     $context = @{
        "adminrouter_install_dir" = ("$ADMINROUTER_APACHE_DIR" -replace '\\', '/')
        "adminrouter_port" = ("$ADMINROUTER_AGENT_PORT")
        "local_metrics_port" = ("$METRICS_AGENT_PORT")
        "local_diagnostics_port" = ("$DIAGNOSTICS_AGENT_PORT")
        "local_pkgpanda_port" = ("$PKGPANDA_AGENT_PORT")
        "local_logging_port" = ("$LOGGING_AGENT_PORT")
    }
    Start-RenderTemplate -TemplateFile $TemplateFile `
                         -Context $context -OutFile $OutputFile
}

function Install-ApacheHttpServer {
    Write-Output "Installing Apache HTTP Server..."
    Get-ApacheServerPackage

    $template = "$TEMPLATES_DIR\adminrouter\apache_template.conf"
    $configfile = "$ADMINROUTER_APACHE_DIR\conf\adminrouter.conf"
    Write-Output "Generating Apache config file $configfile from $template ..."
    Generate-ApacheConfigFromTemplate -TemplateFile $template -OutputFile $configfile
    Write-Output "Apache HTTP Server installed"
}

function New-AdminRouterAgent {
    $logFile = Join-Path $ADMINROUTER_LOG_DIR "adminrouter-agent.log"
    New-Item -ItemType File -Path $logFile
    $adminrouterBinary = Join-Path $ADMINROUTER_APACHE_BIN_DIR "httpd.exe"
 
    # open up adminrouter port
    Open-WindowsFirewallRule -Name "Allow inbound TCP Port $ADMINROUTER_AGENT_PORT for AdminRouter" -Direction "Inbound" -LocalPort $ADMINROUTER_AGENT_PORT -Protocol "TCP"

    # start Apache server
    $configFile = Join-Path $ADMINROUTER_APACHE_CONF_DIR "adminrouter.conf"
    $adminrouterAgentArguments = (  "-f `"${configFile}`" " )

    New-DCOSWindowsService -Name $ADMINROUTER_SERVICE_NAME -DisplayName $ADMINROUTER_SERVICE_DISPLAY_NAME -Description $ADMINROUTER_SERVICE_DESCRIPTION `
                           -LogFile $logFile -WrapperPath $SERVICE_WRAPPER -BinaryPath "$adminrouterBinary $adminrouterAgentArguments"
    Start-Service $ADMINROUTER_SERVICE_NAME
}

try {
    New-AdminRouterEnvironment
    Install-ApacheHttpServer
    New-AdminRouterAgent
} catch {
    Write-Output $_.ToString()
    exit 1
}
Write-Output "Successfully finished setting up the Windows AdminRouter Service Agent"
exit 0