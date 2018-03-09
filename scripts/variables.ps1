# DCOS common configurations
$LOG_SERVER_BASE_URL = "http://dcos-win.westus.cloudapp.azure.com"
$ERLANG_URL = "$LOG_SERVER_BASE_URL/downloads/erl8.3.zip"
$ZOOKEEPER_PORT = 2181
$EXHIBITOR_PORT = 8181
$DCOS_DIR = Join-Path $env:SystemDrive "DCOS"
$ERLANG_DIR = Join-Path $DCOS_DIR "erl8.3"
$ERTS_DIR = Join-Path $ERLANG_DIR "erts-8.3"
$DOCKER_HOME = Join-Path $env:ProgramFiles "Docker"
$SERVICE_WRAPPER = Join-Path $DCOS_DIR "service-wrapper.exe"

# Mesos configurations
$MESOS_SERVICE_NAME = "dcos-mesos-slave"
$MESOS_SERVICE_DISPLAY_NAME = "DCOS Mesos Windows Slave"
$MESOS_SERVICE_DESCRIPTION = "Windows Service for the DCOS Mesos Slave"
$MESOS_AGENT_PORT = 5051
$MESOS_DIR = Join-Path $DCOS_DIR "mesos"
$MESOS_BIN_DIR = Join-Path $MESOS_DIR "bin"
$MESOS_WORK_DIR = Join-Path $MESOS_DIR "work"
$MESOS_LOG_DIR = Join-Path $MESOS_DIR "log"
$MESOS_SERVICE_DIR = Join-Path $MESOS_DIR "service"

# EPMD configurations
$EPMD_SERVICE_NAME = "dcos-epmd"
$EPMD_SERVICE_DISPLAY_NAME = "DCOS EPMD Windows Agent"
$EPMD_SERVICE_DESCRIPTION = "Windows Service for the DCOS EPMD Agent"
$EPMD_PORT = 61420
$EPMD_DIR = Join-Path $DCOS_DIR "epmd"
$EPMD_LOG_DIR = Join-Path $EPMD_DIR "log"
$EPMD_SERVICE_DIR = Join-Path $EPMD_DIR "service"

# Spartan configurations
$SPARTAN_LOCAL_ADDRESSES = @("192.51.100.1", "192.51.100.2", "192.51.100.3")
$SPARTAN_SERVICE_NAME = "dcos-spartan"
$SPARTAN_SERVICE_DISPLAY_NAME = "DCOS Spartan Windows Agent"
$SPARTAN_SERVICE_DESCRIPTION = "Windows Service for the DCOS Spartan Windows Agent"
$SPARTAN_DEVICE_NAME = "spartan"
$SPARTAN_DIR = Join-Path $DCOS_DIR "spartan"
$SPARTAN_LOG_DIR = Join-Path $SPARTAN_DIR "log"
$SPARTAN_RELEASE_DIR = Join-Path $SPARTAN_DIR "release"
$SPARTAN_SERVICE_DIR = Join-Path $SPARTAN_DIR "service"

# Diagnostics configurations
$DIAGNOSTICS_SERVICE_NAME = "dcos-diagnostics"
$DIAGNOSTICS_SERVICE_DISPLAY_NAME = "DCOS Diagnostics Windows agent"
$DIAGNOSTICS_SERVICE_DESCRIPTION = "Windows Service for the DCOS Diagnostics agent"
$DIAGNOSTICS_AGENT_PORT = 9003
$DIAGNOSTICS_DIR = Join-Path $DCOS_DIR "diagnostics"
$DIAGNOSTICS_CONFIG_DIR = Join-Path $DIAGNOSTICS_DIR "config"
$DIAGNOSTICS_BIN_DIR = Join-Path $DIAGNOSTICS_DIR "bin"
$DIAGNOSTICS_LOG_DIR = Join-Path $DIAGNOSTICS_DIR "log"
$DIAGNOSTICS_SERVICE_DIR = Join-Path $DIAGNOSTICS_DIR "service"

# Installers URLs
$SERVICE_WRAPPER_URL = "$LOG_SERVER_BASE_URL/downloads/service-wrapper.exe"
$VCREDIST_2013_URL = "https://download.microsoft.com/download/2/E/6/2E61CFA4-993B-4DD4-91DA-3737CD5CD6E3/vcredist_x64.exe"
$DEVCON_CAB_URL = "https://download.microsoft.com/download/7/D/D/7DD48DE6-8BDA-47C0-854A-539A800FAA90/wdk/Installers/787bee96dbd26371076b37b13c405890.cab"
$URL_REWRITE_MODULE_URL = 'http://download.microsoft.com/download/D/D/E/DDE57C26-C62C-4C59-A1BB-31D58B36ADA2/rewrite_amd64_en-US.msi'
$ARR_MODULE_URL = "https://download.microsoft.com/download/E/9/8/E9849D6A-020E-47E4-9FD0-A023E99B54EB/requestRouter_amd64.msi"
