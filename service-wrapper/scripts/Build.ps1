#Sample script to build the solution with the target:`SDK10Release`
#You need to have VS2017 build tools, and as a prerequisite you need to run
#this script from a powershell which has VS2017 dev tools environment loaded
$ErrorActionPreference = "Stop"
$url = "https://dist.nuget.org/win-x86-commandline/v4.5.1/nuget.exe"
Invoke-WebRequest $url -TimeoutSec 30 -DisableKeepAlive -OutFile ..\nuget.exe
..\nuget.exe install .\packages.config -OutputDirectory packages
MSBuild.exe /nologo /maxcpucount ..\service-wrapper.sln /target:Build /property:Configuration="SDK10Release" /p:Platform=x64
