# Systemctl-win

Systemctl-win provides a services control and wrapper that allows complex applications to be ported to the windows environment
without requiriing extensive restructuring. It does not provide an identical environment, but is generally compatible. 

Systemctl-win consists of two components:
* Systemctl for windows implements a command line interface for linux-system services compatible with linux systemctl.
For more information, see [freedesktop.org systemd documentation](https://www.freedesktop.org/software/systemd/man/systemd.html)

* systemd-exec provides a compatible service wrapper for executing services according to linux service unit files. This allows daemon
binaries, scripts and executable command lines to be retained with minimum modifications.

Both systemctl and systemd-exec operate using systemd-style unit files.  The freedesktop.org systemd documentation documents the
contents of those files. Generally, the largest difference is that environment files contain either bash or powershell syntax, and executable 
lines (ExecStart, etc) must be in a windows-natural syntax (more or less compatible with cmd.exe).  Systemctl registers services with the 
windows service manager, so the services are managable via sc.exe, windows service manager, powershell cmdlets as well as systemctl. 
However, only systemctl-win services are visible to systemctl. 

## Building

Prerequisites:
* visual studio 2017 including command line build tools and environment 
* windows 10 SDK
* nuget.exe in your path.
* git for windows 2.17 or later.

To build systemctl-win, clone the project github.com/dcos/dcos-windows.

```
  git clone git@github.com:dcos/dcos-windows.git
```

Systemctl-win is set up to build through visual studio, so change directory to $PROJECTBASE/dcos-windows/systemctl-win. We will assume 
using powershell core (pwsh.exe) from here. The first order of buisness is to nuget all the dependent packages, basically various boost packages,
then build using msbuild.
```
   cd ~/Projects/dcos-windows/systemctl-win
   nuget restore ./Systemctl-win.sln
   pushd ./systemd-exec
   nuget restore ./systemd-exec.sln
   popd
   msbuild ./Systemctl-win.sln -p:Configuration=release
```
Once you are nuget'ed, you can also use visual studio if you have a gui.  There are both release and debug targets.

This will produce two executables in the x64/release directory, systemctl.exe and systemd-exec.exe.  

### Running

Systemctl-win expects a directory at $env:SYSTEMDRIVE/etc/systemd/system to be precreated with any system units to be enabled. This differs somewhat 
from the linux practice of looking at a series of directories specified in a unit path.  Doing that remains a [to-do].
   
 

# License
The project is created and maintained under [Apache License v2](LICENSE)

## systemctl for windows.

### Supported commands 

* enable
* start
* stop 
* disable
* mask
* is-enabled
* is-active
* is-failed
* show


# Features
Beside just wrapping binaries the service wrapper has some interesting features:
- `--log-file <file>` this will allow the service wrapper to output the STD_ERR and STD_OUT to a given file specified by <file>
- `environment-file` environment file to be added to the given wrapped binary
- `environment-file-pshell` environment file in powershell format to be added to the given wrapped binary
- `exec-start-pre` command to be executed before starting the service

Simple example to wrap a binary:

`TODO`

Simple example to wrap standard out and error:

`TODO`

Simple example to set the environment file:

`TODO`

Simple example to execute a command before starting the service:

`TODO`

An example that contains all of the above:

`TODO`

# Compiling and building
This project can be compiled either from Visual Studio 2015 or 2017.

You will have to download the appropriate platform toolset for each version.

You can compile either from the GUI or the command line.

An example script can be found [here](scripts/Build.ps1) for your convenience.

# To Do

* Implement UnitPath rather than looking for system unit files only in c:/etc/systemd/system. 
