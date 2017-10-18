#
#
#
#[void]$global:script_path # A declaration


class WinSW:Installable
{
    static [string] $ClassName = "WinSW"
    Setup([string] $script_path) { Write-Host "Setup WinSW : $script_path"; }
}



