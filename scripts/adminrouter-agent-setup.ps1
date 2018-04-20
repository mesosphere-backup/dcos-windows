#======================================================================================
# This script sets up the AdminRouter agent on a Windows node
# This AdminRouter was implemented on top of the Apache HTTP server + config with 
# reverse proxy settings
#======================================================================================
Param(
    [Parameter(Mandatory=$true)]
    [string]$AgentPrivateIP,
    [string]$AdminRouterWindowsBinariesURL
)

$utils = (Resolve-Path "$PSScriptRoot\Modules\Utils").Path
Import-Module $utils

$variables = (Resolve-Path "$PSScriptRoot\variables.ps1").Path
. $variables

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

function Install-AdminRouterFiles {
    $filesPath = Join-Path $env:TEMP "adminrouter-files.zip"
    Write-Output "Downloading AdminRouter binaries"
    Start-ExecuteWithRetry { Invoke-WebRequest -Uri $AdminRouterWindowsBinariesURL -OutFile $filesPath }
    Write-Output "Extracting binaries archive in: $ADMINROUTER_DIR"
    Expand-Archive -LiteralPath $filesPath -DestinationPath $ADMINROUTER_DIR
    Add-ToSystemPath $ADMINROUTER_DIR
    Remove-item $filesPath
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
    Install-AdminRouterFiles
    New-AdminRouterAgent
} catch {
    Write-Output $_.ToString()
    exit 1
}
Write-Output "Successfully finished setting up the Windows AdminRouter Service Agent"
exit 0
