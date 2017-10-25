#
#
#
#[void]$global:script_path # A declaration

#$TEMPLATES_DIR = Join-Path $PSScriptRoot "templates"
#
# EPMD configurations
$EPMD_SERVICE_NAME = "dcos-epmd"
$global:EPMD_PORT = 61420
$EPMD_DIR = Join-Path $DCOS_DIR "epmd"
$EPMD_SERVICE_DIR = Join-Path $EPMD_DIR "service"
$EPMD_LOG_DIR = Join-Path $EPMD_DIR "log"


function New-Environment {
    $service = Get-Service $EPMD_SERVICE_NAME -ErrorAction SilentlyContinue
    if($service) {
        Stop-Service -Force -Name $EPMD_SERVICE_NAME
        Start-ExternalCommand { sc.exe delete $EPMD_SERVICE_NAME } -ErrorMessage "Failed to delete exiting EPMD service"
    }
    New-Directory -RemoveExisting $EPMD_DIR
    New-Directory $EPMD_SERVICE_DIR
    New-Directory $EPMD_LOG_DIR
}

function New-EPMDWindowsAgent {
    param (
             [string] $ScriptPath
          )
    $epmdBinary = Join-Path $ERTS_DIR "bin\epmd.exe"
    if(!(Test-Path $epmdBinary)) {
        Throw "The EPMD binary $epmdBinary doesn't exist. Cannot configure the EPMD agent Windows service"
    }
    $context = @{
        "service_name" = $EPMD_SERVICE_NAME
        "service_display_name" = "DCOS EPMD Windows Agent"
        "service_description" = "Windows Service for the DCOS EPMD Agent"
        "service_binary" = $epmdBinary
        "service_arguments" = "-port $EPMD_PORT"
        "log_dir" = $EPMD_LOG_DIR
    }
    import-module "$ScriptPath/../templating/extra/Templating.psm1"
    . "$script_path/../templating/extra/Load-Assemblies.ps1"

    $content = Invoke-RenderTemplateFromFile -Context $context -Template "$ScriptPath/../WinSW/extra/windows-service.xml"
    try {
        [System.IO.File]::WriteAllText("$EPMD_SERVICE_DIR\epmd-service.xml", $content)
    } 
    catch {
        Write-Output "could not write file"
        throw $_
    }

    $serviceWapper = Join-Path $EPMD_SERVICE_DIR "epmd-service.exe"
    Invoke-WebRequest -UseBasicParsing -Uri $SERVICE_WRAPPER_URL -OutFile $serviceWapper
    $p = Start-Process -FilePath $serviceWapper -ArgumentList @("install") -NoNewWindow -PassThru -Wait
    if($p.ExitCode -ne 0) {
        Throw "Failed to set up the EPMD Windows service. Exit code: $($p.ExitCode)"
    }
    Start-Service $EPMD_SERVICE_NAME

    # Check to verify the service is actually running before we return. If the service is not up in 20 seconds, throw an exception
    $timeout = 2
    $count = 0
    $maxCount = 10
    while ($count -lt $maxCount) {
        Start-Sleep -Seconds $timeout
        Write-Output "Checking $EPMD_SERVICE_NAME service status"
        $status = (Get-Service -Name $EPMD_SERVICE_NAME).Status
        if($status -ne [System.ServiceProcess.ServiceControllerStatus]::Running) {
            Throw "Service $EPMD_SERVICE_NAME is not running"
        }
        $count++
    }
}

class Epmd:Installable
{
    static [string] $ClassName = "Epmd"
    [string] Setup( [string] $script_path,
           [string[]]$MasterAddress,
           [string]$AgentPrivateIP,
           [switch]$Public=$false
         ) { 

        $UDP_RULE_NAME = "Allow inbound UDP Port $global:EPMD_PORT for EPMD"
        $TCP_RULE_NAME = "Allow inbound TCP Port $global:EPMD_PORT for EPMD"
        Write-Host "Setup Epmd : $script_path";

        try {
            New-Environment
            New-EPMDWindowsAgent -ScriptPath $script_path

            Write-Output "Open firewall rule: $TCP_RULE_NAME"
            $firewallRule = Get-NetFirewallRule -DisplayName $TCP_RULE_NAME -ErrorAction SilentlyContinue
            if($firewallRule) {
                Write-Output "Firewall rule $TCP_RULE_NAME already exists. Skipping"
            }
            else 
            {
                New-NetFirewallRule -DisplayName $TCP_RULE_NAME -Direction "Inbound" -LocalPort $global:EPMD_PORT -Protocol "TCP" -Action Allow | Out-Null
            }

            Write-Output "Open firewall rule: $UDP_RULE_NAME"
            $firewallRule = Get-NetFirewallRule -DisplayName $UDP_RULE_NAME -ErrorAction SilentlyContinue
            if($firewallRule) {
                Write-Output "Firewall rule $UDP_RULE_NAME already exists. Skipping"
            }
            else 
            {
                New-NetFirewallRule -DisplayName $UDP_RULE_NAME -Direction "Inbound" -LocalPort $global:EPMD_PORT -Protocol "UDP" -Action Allow | Out-Null
            }
        } catch {
            throw $_
        }
        Write-Output "Successfully finished setting up the Windows Epmd Agent"
        return $true
    }
}



