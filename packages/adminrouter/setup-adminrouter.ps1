#
#
#
#[void]$global:script_path # A declaration


class AdminRouter:Installable
{
    static [string] $ClassName = "AdminRouter"
    Setup([string] $script_path) { Write-Host "Setup AdminRouter : $script_path"; }
}



