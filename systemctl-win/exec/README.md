# OpenStackService
Windows service wrapper for generic executables. This will help users to create simple services for their binaries.

# License
The project is created and maintained under [Apache License v2](LICENSE)

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
