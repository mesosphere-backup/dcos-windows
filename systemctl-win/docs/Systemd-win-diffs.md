# Differences between Systemctl-win and Linux Systemd.

In a Linux environment, systemd consists of :
* The systemd daeomn, which manages the whole thing and has support via dbus from the kernel.
* systemctl which interactively controls systemd units (enable, start, stop, disable, mask, etc) and shows execution state and status
* journalctl which controls and shows journal output from the systemd managed processes 
* systemd-analyze which allows the editing of the execution environment in real time


Systemctl-win consists of two components:
* Systemctl for windows implements a command line interface for linux-system services compatible with linux systemctl.
For more information, see [freedesktop.org systemd documentation](https://www.freedesktop.org/software/systemd/man/systemd.html)

* systemd-exec provides a compatible service wrapper for executing services according to linux service unit files. This allows daemon
binaries, scripts and executable command lines to be retained with minimum modifications.

* The service control manager, which is part of the windows system.

Both systemctl-win and systemd operate using systemd-style unit files.  The freedesktop.org systemd documentation documents the
contents of those files. Generally, the largest difference is that environment files contain either bash or powershell syntax, and executable 
lines (ExecStart, etc) must be in a windows-natural syntax (more or less compatible with cmd.exe).  Systemctl registers services with the 
windows service manager, so the services are managable via sc.exe, windows service manager, powershell cmdlets as well as systemctl. 
However, only systemctl-win services are visible to systemctl. 

Overall, there are some permanent limitations dictated by the system:
* Dbus is not supported by systemctl-win, so dbus type services and dbus based attributes cannot be supported. 
* Socket units are not currently supporterd in systemctl-win. 

## Differences in systemctl vss systemctl-win by Command

### Unit Commands:
#### list-units [PATTERN...]  (List units currently in memory)
This works as expected. There are format differences

#### list-sockets [PATTERN...]  (List socket units currently in memory, ordered by address)
This is not feasible in windows. Socket units are not supported in systemctl-win, and because of 
structural differences between windows are not expected to be supported in the future.

#### list-timers [PATTERN...]  (List timer units currently in memory, ordered by next elapse)
This is has not been implementented.

#### start .NAME...  (Start (activate) one or more units)
This works as expected.

#### stop NAME...   (Stop (deactivate) one or more units)
This works as expected.

#### reload NAME... (Reload one or more units)
This works as expected.

#### restart NAME...(Start or restart one or more units)
This works as expected.

#### try-restart NAME...(Restart one or more units if active)
This has not been implemented, but there are no known blocking issues

#### reload-or-restart NAME... (Reload one or more units if possible, otherwise start or restart)
This has not been implemented, but there are no known blocking issues

#### try-reload-or-restart NAME...  ( If active, reload one or more units, if supported, otherwise restart)
This has not been implemented, but there are no known blocking issues

#### isolate NAME (Start one unit and stop all others)
This has not been implemented, but there are no known blocking issues if the scope was limited to services managed by systemctl-win.

#### kill NAME... (Send signal to processes of a unit)
This works as expected, except the signals are limited to sigkill and sigterm due to known limitations in windows.

#### is-active PATTERN... (Check whether units are active)
Works as expected but does not supported regular expressions

#### is-failed PATTERN... (Check whether units are failed)
Works as expected but does not supported regular expressions

#### status [PATTERN...|PID...] (Show runtime status of one or more units)
Works as expected but does not supported regular expressions or PIDs

#### show [PATTERN...|JOB...]        Show properties of one or more units/jobs or the manager
Works as expected but does not supported regular expressions or jobs. 

#### cat PATTERN...                  Show files and drop-ins of one or more units
This has not been implemented, but there are no known blocking issues

#### set-property NAME ASSIGNMENT... Sets one or more properties of a unit
This has not been implemented, but there are no known blocking issues

#### help PATTERN...|PID...          Show manual for one or more units
This has not been implemented, but there are no known blocking issues

#### reset-failed [PATTERN...]       Reset failed state for all, one, or more units
This has not been implemented, but there are no known blocking issues

#### list-dependencies [NAME]        Recursively show units which are required or wanted by this unit or by which this unit is required or wanted
This has not been implemented, but there are no known blocking issues

### Unit File Commands:
#### list-unit-files [PATTERN...]    List installed unit files
Works as expected, but does not support regular expressions and has format differences

#### enable [NAME...|PATH...]        Enable one or more unit files
Works as expected but does not supported regular expressions

#### disable NAME...                 Disable one or more unit files
Works as expected but does not supported regular expressions

#### reenable NAME...                Reenable one or more unit files
This has not been implemented, but there are no known blocking issues

#### preset NAME...                  Enable/disable one or more unit files based on preset configuration
This has not been implemented, but there are no known blocking issues. 

This *really* has not been implemented (as in there is no concept in the code of presets).

#### preset-all                      Enable/disable all unit files based on preset configuration
This has not been implemented, but there are no known blocking issues

This *really* has not been implemented (as in there is no concept in the code of presets).

#### is-enabled NAME...              Check whether unit files are enabled
Works as expected. 

#### mask NAME...                    Mask one or more units
Works as expected. Does not manage status

#### unmask NAME...                  Unmask one or more units
Works as expected.  Does not manage status

#### link PATH...                    Link one or more units files into the search path
This has not been implemented, but there are no known blocking issues, except that the search path is a single fixed element.

#### revert NAME...                  Revert one or more unit files to vendor version
This has not been implemented, but there are no known blocking issues, except that there is no concept of a "vendor" version of service units.

#### add-wants TARGET NAME...        Add 'Wants' dependency for the target on specified one or more units
This has not been implemented, but there are no known blocking issues.

#### add-requires TARGET NAME...     Add 'Requires' dependency for the target on specified one or more units
This has not been implemented, but there are no known blocking issues.

#### edit NAME...                    Edit one or more unit files
This has not been implemented, but there are no known blocking issues, except that windows has no default editor other than notepad, 
which cannot handle \n line endings.

#### get-default                     Get the name of the default target
This has not been implemented, but there are no known blocking issues.

#### set-default NAME                Set the default target
This has not been implemented, but there are no known blocking issues.

### Machine Commands:
#### list-machines [PATTERN...]      List local containers and host
This has not been implemented, but there are no known blocking issues except windows has no concept of containers outside of docker.

### Job Commands:
This has not been implemented. There is no concept of jobs in windows.
#### list-jobs [PATTERN...]          List jobs
This has not been implemented. There is no concept of jobs in windows.
#### cancel [JOB...]                 Cancel all, one, or more jobs
This has not been implemented. There is no concept of jobs in windows.

### Environment Commands:
Environment commands have not been implemented but there are no known blocking issues

#### show-environment                Dump environment
Environment commands have not been implemented but there are no known blocking issues

#### set-environment NAME=VALUE...   Set one or more environment variables
Environment commands have not been implemented but there are no known blocking issues

#### unset-environment NAME...       Unset one or more environment variables
Environment commands have not been implemented but there are no known blocking issues

#### import-environment [NAME...]    Import all or some environment variables
Environment commands have not been implemented but there are no known blocking issues


### Manager Lifecycle Commands:
#### daemon-reload                   Reload systemd manager configuration
Works as expected. 

#### daemon-reexec                   Reexecute systemd manager
Not implemented. The service manager is the service control manager which is controlled by the windows system.

### System Commands:
System commands have not been implemented

#### is-system-running               Check whether system is fully running
System commands have not been implemented

#### default                         Enter system default mode
System commands have not been implemented

#### rescue                          Enter system rescue mode
System commands have not been implemented

#### emergency                       Enter system emergency mode
System commands have not been implemented

#### halt                            Shut down and halt the system
System commands have not been implemented

#### poweroff                        Shut down and power-off the system
System commands have not been implemented

## Unit Files

Differences fall into two areas: 
* The resident shell in windows is powershell or cmd.exe. Bash is only supported as an add on, and each of the possible add ons behave differently.
* Many of the limits are either unavailable or simply do not exist in windows.

### Unit support by attribute.
### Unit Service Section Attributes
#### Type=
Types simple, forking and oneshot implemented
Types notify and idle not implemented but no known blocking issues
Type dbus not feasible.

#### RemainAfterExit=
Works as expected

#### GuessMainPID=
Not implemented but no known blocking issues

#### PIDFile=
Not implemented but no known blocking issues

#### BusName=
Not feasible

#### ExecStart=
Works as expected

#### ExecStartPre=, ExecStartPost=
Works as expected

#### ExecReload=
Not implemented but no known blocking issues

#### ExecStop=
Works as expected

#### ExecStopPost=
Works as expected

The $SERVICE_RESULT, $EXIT_CODE and $EXIT_STATUS environment variables are not specificly set.

#### RestartSec=
Works as expected

#### TimeoutStartSec=
Works as expected

#### TimeoutStopSec=
Not implemented but no known blocking issues

#### TimeoutSec=
A shorthand for configuring both TimeoutStartSec= and TimeoutStopSec= to the specified value.

#### RuntimeMaxSec=
Not implemented but no known blocking issues

#### WatchdogSec=
Not implemented but no known blocking issues. Watchdogs are not implemented.

#### Restart=
Works as expected

#### SuccessExitStatus=
Not implemented but no known blocking issues.

#### RestartPreventExitStatus=
Not implemented but no known blocking issues.

#### RestartForceExitStatus=
Not implemented but no known blocking issues.

#### PermissionsStartOnly=
Not implemented but no known blocking issues.

#### RootDirectoryStartOnly=
Not implemented but no known blocking issues.

#### NonBlocking=
Not implemented but no known blocking issues except we don't support socket services.

#### NotifyAccess=
Not implemented but no known blocking issues except there is not notification mechanism in windows.

#### Sockets=
Not supported

#### FileDescriptorStoreMax=
Not implemented.

#### USBFunctionDescriptors=
Not Implemented

#### USBFunctionStrings=
Not Implemented

### Unit Section Options
#### Description=
Works as expected.

#### Documentation=
Works as expected.

#### Requires=
Works as expected.

#### Requisite=
Works as expected.

#### Wants=
Works as expected.

#### BindsTo=
Not implemented but no known blocking issues.

#### PartOf=
Not implemented but no known blocking issues.

#### Conflicts=
Works as expected.

#### Before=, After=
Works as expected.

#### OnFailure=
Not implemented but no known blocking issues.

#### PropagatesReloadTo=, ReloadPropagatedFrom=
Not Implemented

#### JoinsNamespaceOf=
Not implemented but no known blocking issues.

#### RequiresMountsFor=
Not Implemented. Windows does not have mounts in the linux sense.

#### OnFailureJobMode=
Not Implemented. No concept of jobs

#### IgnoreOnIsolate=
Not implemented but no known blocking issues.

#### StopWhenUnneeded=
Not implemented but no known blocking issues.

#### RefuseManualStart=, RefuseManualStop=
Not implemented but no known blocking issues.

#### AllowIsolate=
Not implemented but no known blocking issues.

#### DefaultDependencies=
Not implemented but no known blocking issues.

#### CollectMode=
Not implemented

#### JobTimeoutSec=, JobRunningTimeoutSec=, JobTimeoutAction=, JobTimeoutRebootArgument=
Not Implemented. No concept of jobs

#### StartLimitIntervalSec=interval, StartLimitBurst=burst
Accepted but not respected.

#### StartLimitAction=
Not implemented but no known blocking issues.

#### FailureAction=, SuccessAction=
Not implemented but no known blocking issues.

#### RebootArgument=
Not implemented

#### ConditionArchitecture=, ConditionVirtualization=, ConditionHost=, ConditionKernelCommandLine=, ConditionKernelVersion=, ConditionSecurity=, ConditionCapability=, ConditionACPower=, ConditionNeedsUpdate=, ConditionFirstBoot=, ConditionPathExists=, ConditionPathExistsGlob=, ConditionPathIsDirectory=, ConditionPathIsSymbolicLink=, ConditionPathIsMountPoint=, ConditionPathIsReadWrite=, ConditionDirectoryNotEmpty=, ConditionFileNotEmpty=, ConditionFileIsExecutable=, ConditionUser=, ConditionGroup=, ConditionControlGroupController=
All conditions are accepted, but currenlty only ConditionPathExits, ConditionPathIsDirectory, ConditionDirectoryNotEmpty are implemented.
ConditionACPower is not feasible. 
ConditionKernelVersion would produce variant output
ConditionNeedsUpdate is not feasible
ConditionUser, ConditionGroup, ConditionPathIsSymbolicLink and ConditionFileIsExecutable would have variant semantics.

#### AssertArchitecture=, AssertVirtualization=, AssertHost=, AssertKernelCommandLine=, AssertKernelVersion=, AssertSecurity=, AssertCapability=, AssertACPower=, AssertNeedsUpdate=, AssertFirstBoot=, AssertPathExists=, AssertPathExistsGlob=, AssertPathIsDirectory=, AssertPathIsSymbolicLink=, AssertPathIsMountPoint=, AssertPathIsReadWrite=, AssertDirectoryNotEmpty=, AssertFileNotEmpty=, AssertFileIsExecutable=, AssertUser=, AssertGroup=, AssertControlGroupController=
Similar to the ConditionArchitecture=, ConditionVirtualization=
Not Implemented

#### SourcePath=
Accepted but not used.

### Install Section Options
#### Alias=
Not Implemented, no know blocking issues

#### WantedBy=, RequiredBy=
Works as expected

#### Also=
Not Implemented, no know blocking issues

#### DefaultInstance=
Not Implemented

### Systemd Exec Settings

### Paths
#### WorkingDirectory=
Not implemented but no known blocking issues.

#### RootDirectory=
Not implemented but no known blocking issues.

#### RootImage=
Not Implemented

#### MountAPIVFS=
Not Implemented

#### BindPaths=, BindReadOnlyPaths=
Not Implemented

### Credentials
#### User=, Group=
Not implemented but no known blocking issues. Somewhat variant semantics due to system differences

#### DynamicUser=
Not implemented but no known blocking issues. Somewhat variant semantics due to system differences

#### SupplementaryGroups=
Not implemented but no known blocking issues. Somewhat variant semantics due to system differences

#### PAMName=
Not Implemented

### Capabilities
#### CapabilityBoundingSet=
Not Implemented

#### AmbientCapabilities=
Not Implemented

#### NoNewPrivileges=
Not Implemented

#### SecureBits=
Not Implemented

### Mandatory Access Control
#### SELinuxContext=
Not Implemented

#### AppArmorProfile=
Not Implemented

### Process Properties
#### LimitCPU=, LimitFSIZE=, LimitDATA=, LimitSTACK=, LimitCORE=, LimitRSS=, LimitNOFILE=, LimitAS=, LimitNPROC=, LimitMEMLOCK=, LimitLOCKS=, LimitSIGPENDING=, LimitMSGQUEUE=, LimitNICE=, LimitRTPRIO=, LimitRTTIME=
Not Implemented

#### UMask=
Not Implemented

#### KeyringMode=
Not Implemented

#### OOMScoreAdjust=
Not Implemented

#### TimerSlackNSec=
Not Implemented

#### Personality=
Not Implemented

#### IgnoreSIGPIPE=
Not Implemented

### Scheduling
Not Implemented

#### Nice=
Not Implemented

#### CPUSchedulingPolicy=
Not Implemented

#### CPUSchedulingPriority=
Not Implemented

#### CPUSchedulingResetOnFork=
Not Implemented

#### CPUAffinity=
Not Implemented

#### IOSchedulingClass=
Not Implemented

#### IOSchedulingPriority=
Not Implemented

### Sandboxing
#### ProtectSystem=
Not Implemented

#### ProtectHome=
Not Implemented

### RuntimeDirectory=, StateDirectory=, CacheDirectory=, LogsDirectory=, ConfigurationDirectory=
Not implemented but no known blocking issues. 

### RuntimeDirectoryMode=, StateDirectoryMode=, CacheDirectoryMode=, LogsDirectoryMode=, ConfigurationDirectoryMode=
Not implemented but no known blocking issues. 

#### RuntimeDirectoryPreserve=
Not implemented but no known blocking issues. 

### ReadWritePaths=, ReadOnlyPaths=, InaccessiblePaths=
Not implemented but no known blocking issues. 

#### TemporaryFileSystem=
Not implemented but no known blocking issues. 

#### PrivateTmp=
Not Implemented

#### PrivateDevices=
Not Implemented

#### PrivateNetwork=
Not Implemented

#### PrivateUsers=
Not Implemented

#### ProtectKernelTunables=
Not Implemented

#### ProtectKernelModules=
Not Implemented

#### ProtectControlGroups=
Not Implemented

#### RestrictAddressFamilies=
Not Implemented

#### RestrictNamespaces=
Not Implemented

#### LockPersonality=
Not Implemented

#### MemoryDenyWriteExecute=
Not Implemented

#### RestrictRealtime=
Not Implemented

#### RemoveIPC=
Not Implemented

#### PrivateMounts=
Not Implemented

#### MountFlags=
Not Implemented

### System Call Filtering
#### SystemCallFilter=
Not Implemented

#### SystemCallErrorNumber=
Not Implemented

#### SystemCallArchitectures=
Not Implmenented

### Environment
#### Environment=
Works as expected. Extended to allow powershell syntax as well as bash

#### EnvironmentFile=
Works as expected. Extended to allow powershell syntax as well as bash

#### PassEnvironment=
#### UnsetEnvironment=

### Logging and Standard Input/Output
#### StandardInput=
Works as expected

#### StandardOutput=
Works as expected

#### StandardError=
Works as expected

####StandardInputText=, StandardInputData=
Works as expected

#### LogLevelMax=
Works as expected

#### LogExtraFields=
Works as expected

#### SyslogIdentifier=
Works as expected

#### SyslogFacility=
Works as expected within the limits of windows. The functional options are syslog(which goes to the log file in /var/log), console
(which goes to the process console), file.

#### SyslogLevel=
Works as expected

#### SyslogLevelPrefix=
Works as expected

#### TTYPath= TTYReset= TTYVHangup= TTYVTDisallocate=
Not Implemented

Systemd Exec settings are not implemented except EnvironmentFile= which works as expected, with the extension that it allows powershell syntax environment files.
