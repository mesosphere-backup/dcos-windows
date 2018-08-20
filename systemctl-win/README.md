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
   msbuild ./Systemctl-win.sln -p:Configuration=release
```
Once you are nuget'ed, you can also use visual studio if you have a gui.  There are both release and debug targets.

This will produce two executables in the x64/release directory, systemctl.exe and systemd-exec.exe.  

### Running

Systemctl-win expects a directory at $env:SYSTEMDRIVE/etc/systemd/system to be precreated with any system units to be enabled. This differs somewhat 
from the linux practice of looking at a series of directories specified in a unit path.  Doing that remains a [to-do].  In order to start a process, executing
```
systemctl enable myservice.service
```
will register the service with the service control manager and copy the service unit to the $env:SYSTEMDRIVE/etc/systemd/active directory. Active is the only directory from wehich service units are executed.  This makes the service available for starting.

Executing 
```
systemctl start myservice.service
```
Will start the service. The service is expected to be a windows executable, *but not a windows service.*  The systemd-exec.exe program will 
handle communication with the windows service control manager (start service, stop service, pause etc).  This allows a transplanted linux daemon or 
other bare executable an environment that looks almost like a linux host.  

Among the exceptions, a large one is signals. Windows does not support signals. The closest it comes is console control events where a console app
(which would include any transplanted linux daemon) is sent a control-c or control break event. Linux daemons use signals (SIGKILL, SIGTERM, SIGUSR1, SIGHUP) 
to control execution of daemons without having to restart. In particular, signals are used to instruct a daemon to perform a graceful shutdown, saving
its state and performing cleanup (SIGTERM), an immediate exit (SIGKILL), or to reload configuration information (SIGHUP or SIGUSR1).   The daemon catches
these signals by way of an auxilliary thread which waits for signal events and executes a handler when a signal is received.  The signal handler may be
the default which just exits the process when SIGKILL or SIGTERM are encountered, or the application may specify a signal handler which executes application
specific code for each signal. The latter is usual. 

Windows does not provide a signal mechanism.  The closest it comes is events.  Events is a similar mechanism, but does not have support in the usual C++ runtime. 
In the general case, events may live in a global system namespace, so may readily be accessed by various processes.  This facility could be used to provide 
signal support, but it is a very different mechanism, and would require significant code changes in the application, essentially a complete rewrite of the 
signal handler.  

Rather than that, we have chosen to use the much more restricted mechanism, console control events.  This is an event in the same sense as more general events,  but
support is present in the C runtime.  The Win32 API GenerateConsoleiCtrlEvent is similar to raise() in its effect. It is paired with the win32 api 
SetConsoleCtrlHandler wihich is again quite similar to signal() in that it sets a signal handler.  There is, however a significant limitation.  Due to an 
implementation decision currently windows will only pass one of two values for signal.  Control-C-Event and Control-Break-Event.  This is fine for allowing
orderly shutdown of the service process, but does nothing for other traditional signal usage, like SIGHUP for reload.  If the application requires these,
it will have to set up its own event, thread and handler, as well as a program for managing it. This is a pretty permanent limitaiton unless Windows removes the 
restrictions in the console control event pipeline. Even then, while the situation would be greatly better, there would be still be some semantic differences.
In Linux, SIGKILL is preemptive, but the SIGKILL signal handler is executed even in the case of a hung process. In windows a SIGKILL signal handler attached to
a hung process would probably not be executed, but rather the service directly terminated. We beleive that would not be a serious design issue, but would 
require giving some thought to its mitigation.

When it is time to stop the service, executing
```
systemctl stop myservice.service
```
will send a stop service reqest to the service control manager, which will send an event to systemd-exec. That in turn will terminate the systemd-exec
execution thread and execute the execute-stop and execute-stop-post operations, if any.

The service will remain stopped until reboot or until a `systemctl start` command. 

The `systemctl restart` command has the same effect as a `systemctl stop` followed by a `systemctl start`



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
* kill



# Features

# Compiling and building
This project can be compiled either from Visual Studio 2015 or 2017.

You will have to download the appropriate platform toolset for each version.

You can compile either from the GUI or the command line.

An example script can be found [here](scripts/Build.ps1) for your convenience.

