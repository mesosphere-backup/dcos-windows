#!/usr/bin/pwsh
#
# Makes it possible to invoke a powershell script without specifying the ".ps1" extension
# This in turn makes it possible to interoperate with linux-centered python code that wants to invoke scripts with just the name.
# After applying this regkey execing a shell line of "xxx" will invoke the script "xxx" on linux, or on windows "xxx.ps1"

$pyhome = $env:PY_HOME 
if ( $pyhome -eq "" ) {
   $pyhome = "$env:SystemDrive/Program Files/Python36"
}

$pyexecutable = "$pyhome\python.exe"

if  (! (Test-Path $pyexecutable)) {
   exit -1
}

Set-ItemProperty  "Registry::HKEY_CLASSES_ROOT\.py" -Name "(default)" -value "Python.File"
Set-ItemProperty  "Registry::HKEY_CLASSES_ROOT\.pyc" -Name "(default)" -value "Python.File"
Set-ItemProperty  "Registry::HKEY_CLASSES_ROOT\Python.File" -Name "(default)" -value "$pyexecutable ""$1"" $*"
