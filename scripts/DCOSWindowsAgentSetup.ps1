[CmdletBinding(DefaultParameterSetName="Standard")]
Param(
    [ValidateNotNullOrEmpty()]
    [string]$MasterIP,
    [ValidateNotNullOrEmpty()]
    [string]$AgentPrivateIP,
    [ValidateNotNullOrEmpty()]
    [string]$BootstrapUrl,
    [AllowNull()]
    [switch]$isPublic = $false,
    [AllowNull()]
    [string]$MesosDownloadDir,
    [AllowNull()]
    [string]$MesosInstallDir,
    [AllowNull()]
    [string]$MesosLaunchDir,
    [AllowNull()]
    [string]$MesosWorkDir,
    [AllowNull()]
    [string]$customAttrs
)

$ErrorActionPreference = "Stop"

$SCRIPTS_REPO_URL = "https://github.com/dcos/dcos-windows"
$SCRIPTS_DIR = Join-Path $env:TEMP "dcos-windows"
$MESOS_BINARIES_URL = "$BootstrapUrl/mesos.zip"

function Add-ToSystemPath {
    Param(
        [Parameter(Mandatory=$true)]
        [string[]]$Path
    )
    $systemPath = [System.Environment]::GetEnvironmentVariable('Path', 'Machine').Split(';')
    $currentPath = $env:PATH.Split(';')
    foreach($p in $Path) {
        if($p -notin $systemPath) {
            $systemPath += $p
        }
        if($p -notin $currentPath) {
            $currentPath += $p
        }
    }
    $env:PATH = $currentPath -join ';'
    setx.exe /M PATH ($systemPath -join ';')
    if($LASTEXITCODE) {
        Throw "Failed to set the new system path"
    }
}

function Install-Git {
    $gitInstallerURL = "http://dcos-win.westus.cloudapp.azure.com/downloads/Git-2.14.1-64-bit.exe"
    $gitInstallDir = Join-Path $env:ProgramFiles "Git"
    $gitPaths = @("$gitInstallDir\cmd", "$gitInstallDir\bin")
    if(Test-Path $gitInstallDir) {
        Write-Output "Git is already installed"
        Add-ToSystemPath $gitPaths
        return
    }
    Write-Output "Downloading Git from $gitInstallerURL"
    $programFile = Join-Path $env:TEMP "git.exe"
    Invoke-WebRequest -UseBasicParsing -Uri $gitInstallerURL -OutFile $programFile
    $parameters = @{
        'FilePath' = $programFile
        'ArgumentList' = @("/SILENT")
        'Wait' = $true
        'PassThru' = $true
    }
    Write-Output "Installing Git"
    $p = Start-Process @parameters
    if($p.ExitCode -ne 0) {
        Throw "Failed to install Git during the environment setup"
    }
    Add-ToSystemPath $gitPaths
}

function New-ScriptsDirectory {
    if(Test-Path $SCRIPTS_DIR) {
        Remove-Item -Recurse -Force -Path $SCRIPTS_DIR
    }
    Install-Git
    $p = Start-Process -FilePath 'git.exe' -Wait -PassThru -NoNewWindow -ArgumentList @('clone', $SCRIPTS_REPO_URL, $SCRIPTS_DIR)
    if($p.ExitCode -ne 0) {
        Throw "Failed to clone $SCRIPTS_REPO_URL repository"
    }
}

function Get-MasterIPs {
    [string[]]$ips = ConvertFrom-Json $MasterIP
    # NOTE(ibalutoiu): ACS-Engine adds the Zookeper port to every master IP and we need only the address
    [string[]]$masterIPs = $ips | ForEach-Object { $_.Split(':')[0] }
    return $masterIPs
}

function Install-MesosAgent {
    $masterIPs = Get-MasterIPs
    & "$SCRIPTS_DIR\scripts\mesos-agent-setup.ps1" -MasterAddress $masterIPs -MesosWindowsBinariesURL $MESOS_BINARIES_URL `
                                                -AgentPrivateIP $AgentPrivateIP -Public:$isPublic -CustomAttributes $customAttrs
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS Mesos Windows slave agent"
    }
}

function Install-ErlangRuntime {
    & "$SCRIPTS_DIR\scripts\erlang-setup.ps1"
    if($LASTEXITCODE) {
        Throw "Failed to setup the Windows Erlang runtime"
    }
}

function Install-EPMDAgent {
    & "$SCRIPTS_DIR\scripts\epmd-agent-setup.ps1"
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS EPMD Windows agent"
    }
}

function Install-SpartanAgent {
    $masterIPs = Get-MasterIPs
    & "$SCRIPTS_DIR\scripts\spartan-agent-setup.ps1" -MasterAddress $masterIPs -AgentPrivateIP $AgentPrivateIP -Public:$isPublic
    if($LASTEXITCODE) {
        Throw "Failed to setup the DCOS Spartan Windows agent"
    }
}

function Update-Docker {
    $dockerHome = Join-Path $env:ProgramFiles "Docker"
    $baseUrl = "http://dcos-win.westus.cloudapp.azure.com/downloads/docker"
    $version = "18.02.0-ce"
    Stop-Service "Docker"
    Invoke-WebRequest -UseBasicParsing -Uri "${baseUrl}/${version}/docker.exe" -OutFile "${dockerHome}\docker.exe"
    Invoke-WebRequest -UseBasicParsing -Uri "${baseUrl}/${version}/dockerd.exe" -OutFile "${dockerHome}\dockerd.exe"
    Start-Service "Docker"
}

function New-DockerNATNetwork {
    #
    # This needs to be used by all the containers since DCOS Spartan DNS server
    # is not bound to the gateway address.
    # The Docker gateway address is added to the DNS server list unless
    # disable_gatewaydns network option is enabled.
    #
    docker.exe network create --driver="nat" --opt "com.docker.network.windowsshim.disable_gatewaydns=true" "nat_network"
    if($LASTEXITCODE -ne 0) {
        Throw "Failed to create the new Docker NAT network with disable_gatewaydns flag"
    }
}

try {
    Update-Docker
    New-DockerNATNetwork
    New-ScriptsDirectory
    Install-MesosAgent
    Install-ErlangRuntime
    Install-EPMDAgent
    Install-SpartanAgent
} catch {
    Write-Output $_.ToString()
    exit 1
}
Write-Output "Successfully finished setting up the DCOS Windows Agent"
exit 0
