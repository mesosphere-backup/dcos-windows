# escape=`

FROM microsoft/windowsservercore:1709

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

COPY ./setup-git.ps1 /tmp/
RUN  /tmp/setup-git.ps1
ENV GIT_PATH 'C:\Program Files\git\cmd'

COPY ./setup-cmake.ps1 /tmp/
RUN  /tmp/setup-cmake.ps1
ENV CMAKE_PATH 'C:\Program Files\CMake\bin'

COPY ./setup-patch.ps1 /tmp/
RUN  /tmp/setup-patch.ps1;
ENV PATCH_PATH 'C:\Program Files (x86)\GnuWin32\bin'

COPY ./setup-make.ps1 /tmp/
RUN  /tmp/setup-make.ps1;
ENV MAKE_PATH 'C:\Program Files (x86)\GnuWin32\bin'

COPY ./setup-python27.ps1 /tmp/
RUN  /tmp/setup-python27.ps1
ENV PYTHON_PATH 'C:\Python27;C:\Python27\scripts'

COPY ./setup-vs2017.ps1 /tmp/
RUN  /tmp/setup-vs2017.ps1

COPY ./setup-erlang.ps1 /tmp/
RUN  /tmp/setup-erlang.ps1
ENV ERLANG_PATH 'C:\Program Files\erlang\bin'

COPY ./setup-golang.ps1 /tmp/
RUN  /tmp/setup-golang.ps1
ENV GOPATH 'c:\gopath'
ENV GOLANG_PATH 'C:\gopath\bin;c:\go\bin'

RUN $env:Path = "$env:GIT_PATH" + ';' + `
                "$env:CMAKE_PATH" + ';' + `
                "$env:PATCH_PATH" + ';' + `
                "$env:PATCH_MAKE" + ';' + `
                "$env:PYTHON_PATH" + ';' + `
                "$env:ZIP_PATH" + ';' + `
                "$env:ERLANG_PATH" + ';' + `
                "$env:GOLANG_PATH" + ';' + `
                "$env:Path"; `
    [Environment]::SetEnvironmentVariable('PATH', $env:PATH, [EnvironmentVariableTarget]::Machine)

