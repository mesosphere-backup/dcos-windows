# Copyright 2018 Microsoft Corporation
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#

# Sample script to build the solution with the target:`SDK10Release`
# You need to have VS2017 build tools, and as a prerequisite you need to run
# this script from a powershell which has VS2017 dev tools environment loaded
$ErrorActionPreference = "Stop"
$url = "https://dist.nuget.org/win-x86-commandline/v4.5.1/nuget.exe"
Invoke-WebRequest $url -TimeoutSec 30 -DisableKeepAlive -OutFile ..\nuget.exe
..\nuget.exe install .\packages.config -OutputDirectory packages
MSBuild.exe /nologo /maxcpucount ..\service-wrapper.sln /target:Build /property:Configuration="SDK10Release" /p:Platform=x64
