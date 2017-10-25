#
#
#
#[void]$global:script_path # A declaration


class WinSW:Installable
{
    static [string] $ClassName = "WinSW"
    [string] Setup(
           [string] $script_path,
           [parameter(Mandatory=$true)]
           [string[]]$MasterAddress,
           [string]$AgentPrivateIP,
           [switch]$Public=$false
         ) { 
        Write-Host "Setup WinSW : $script_path";

        $obj = ( Get-Content "$script_path/setupinfo.json" | ConvertFrom-Json)
        $package_uri = [string] $obj.package_uri;

        # We just copy down the .exe and leave it in the download directory. 
        # Other components will use it to wrap
        Invoke-WebRequest -Uri $package_uri -OutFile $global:dcos_download
        return ClassName
    }
}



