#!/usr/bin/pwsh
#
# Makes it possible to invoke a powershell script without specifying the ".ps1" extension
# This in turn makes it possible to interoperate with linux-centered python code that wants to invoke scripts with just the name.
# After applying this regkey execing a shell line of "xxx" will invoke the script "xxx" on linux, or on windows "xxx.ps1"

Set-ItemProperty  "Registry::HKEY_CLASSES_ROOT\.ps1" -Name "(default)" -value "Microsoft.PowerShellScript.1"
