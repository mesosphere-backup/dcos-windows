#!/usr/bin/pwsh
#
# Simple helper script to do a full local  build

Set-PSDebug 

# Fail quickly if docker isn't working / up
docker ps

# Cleanup from previous build
rm -recurse /tmp/dcos_build_venv

# Force Python stdout/err to be unbuffered.
$env:PYTHONUNBUFFERED="notempty"

# Write a DC/OS Release tool configuration file which specifies where the build
# should be published to. This default config makes the release tool push the
# release to a folder in the current user's home directory.
if ( Test-Path "dcos-release.config.yaml" ) {
$config_yaml = 
            " storage: `
                local: `
                 kind: local_path `
                 path: $HOME/dcos-artifacts `
              options: `
            preferred: local `
 cloudformation_s3_url: https://s3-us-west-2.amazonaws.com/downloads.dcos.io/dcos "

   $config_yaml |`:Set-Content -Path "dcos-release.config.yaml" 
}

# Create a python virtual environment to install the DC/OS tools to
python3 -m venv /tmp/dcos_build_venv
python3 -m venv /tmp/dcos_build_venv/Scripts/Activate.ps1

# Install the DC/OS tools
./prep_local

# Build a release of DC/OS
release create `whoami` local_build

Set-PSDebug -Off
