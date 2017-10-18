#
#
#
#[void]$global:script_path # A declaration


class Spartan:Installable
{
    static [string] $ClassName = "Spartan"
    Setup([string] $script_path) { Write-Host "Setup Spartan : $script_path"; }
}



