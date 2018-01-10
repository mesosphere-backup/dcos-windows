# DCOS common configurations
$LOG_SERVER_BASE_URL = "http://dcos-win.westus.cloudapp.azure.com"
$ERLANG_URL = "$LOG_SERVER_BASE_URL/downloads/erl8.3.zip"
$ZOOKEEPER_PORT = 2181
$EXHIBITOR_PORT = 8181
$DCOS_DIR = Join-Path $env:SystemDrive "DCOS"
$ERLANG_DIR = Join-Path $DCOS_DIR "erl8.3"
$ERTS_DIR = Join-Path $ERLANG_DIR "erts-8.3"
$DOCKER_HOME = Join-Path $env:ProgramFiles "Docker"

# Mesos configurations
$MESOS_SERVICE_NAME = "dcos-mesos-slave"
$MESOS_AGENT_PORT = 5051
$MESOS_DIR = Join-Path $DCOS_DIR "mesos"
$MESOS_BIN_DIR = Join-Path $MESOS_DIR "bin"
$MESOS_WORK_DIR = Join-Path $MESOS_DIR "work"
$MESOS_LOG_DIR = Join-Path $MESOS_DIR "log"
$MESOS_SERVICE_DIR = Join-Path $MESOS_DIR "service"
$MESOS_BUILD_DIR = Join-Path $MESOS_DIR "build"
$MESOS_BINARIES_DIR = Join-Path $MESOS_DIR "binaries"
$MESOS_GIT_REPO_DIR = Join-Path $MESOS_DIR "mesos"
$MESOS_BUILD_OUT_DIR = Join-Path $MESOS_DIR "build-output"
$MESOS_BUILD_LOGS_DIR = Join-Path $MESOS_BUILD_OUT_DIR "logs"
$MESOS_BUILD_BINARIES_DIR = Join-Path $MESOS_BUILD_OUT_DIR "binaries"
$MESOS_BUILD_BASE_URL = "$LOG_SERVER_BASE_URL/mesos-build"

# EPMD configurations
$EPMD_SERVICE_NAME = "dcos-epmd"
$EPMD_PORT = 61420
$EPMD_DIR = Join-Path $DCOS_DIR "epmd"
$EPMD_SERVICE_DIR = Join-Path $EPMD_DIR "service"
$EPMD_LOG_DIR = Join-Path $EPMD_DIR "log"

# Spartan configurations
$SPARTAN_SERVICE_NAME = "dcos-spartan"
$SPARTAN_DEVICE_NAME = "spartan"
$SPARTAN_DIR = Join-Path $DCOS_DIR "spartan"
$SPARTAN_RELEASE_DIR = Join-Path $SPARTAN_DIR "release"
$SPARTAN_LOG_DIR = Join-Path $SPARTAN_DIR "log"
$SPARTAN_SERVICE_DIR = Join-Path $SPARTAN_DIR "service"
$SPARTAN_GIT_REPO_DIR = Join-Path $SPARTAN_DIR "spartan"
$SPARTAN_BUILD_OUT_DIR = Join-Path $SPARTAN_DIR "build-output"
$SPARTAN_BUILD_LOGS_DIR = Join-Path $SPARTAN_BUILD_OUT_DIR "logs"
$SPARTAN_BUILD_BASE_URL = "$LOG_SERVER_BASE_URL/spartan-build"

# Installers URLs
$SERVICE_WRAPPER_URL = "$LOG_SERVER_BASE_URL/downloads/WinSW.NET4.exe"
$VCREDIST_2013_URL = "https://download.microsoft.com/download/2/E/6/2E61CFA4-993B-4DD4-91DA-3737CD5CD6E3/vcredist_x64.exe"
$DEVCON_CAB_URL = "https://download.microsoft.com/download/7/D/D/7DD48DE6-8BDA-47C0-854A-539A800FAA90/wdk/Installers/787bee96dbd26371076b37b13c405890.cab"

# Git repositories URLs
$MESOS_GIT_URL = "https://github.com/apache/mesos"
$SPARTAN_GIT_URL = "https://github.com/dcos/spartan"
