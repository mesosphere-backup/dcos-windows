# DCOS common configurations
$LOG_SERVER_BASE_URL = "http://dcos-win.westus.cloudapp.azure.com"
$ZOOKEEPER_PORT = 2181
$EXHIBITOR_PORT = 8181
$DCOS_DIR = Join-Path $env:SystemDrive "DCOS"
$DCOS_ETC_DIR = Join-Path $env:SystemDrive "DCOS-etc"
$DOCKER_HOME = Join-Path $env:ProgramFiles "Docker"
$DOCKER_DATA = Join-Path $env:ProgramData "Docker"
$DOCKER_SERVICE_NAME = "Docker"
$SERVICE_WRAPPER_FILE = "service-wrapper.exe"
$SERVICE_WRAPPER = Join-Path $DCOS_DIR $SERVICE_WRAPPER_FILE
$GLOBAL_ENV_FILE = Join-Path $DCOS_DIR "environment"
$MASTERS_LIST_FILE = Join-Path $DCOS_DIR "master_list"
$DCOS_NAT_NETWORK_NAME = "dcosnat"

# Mesos configurations
$MESOS_SERVICE_NAME = "dcos-mesos-slave"
$MESOS_SERVICE_DISPLAY_NAME = "Mesos Agent"
$MESOS_SERVICE_DESCRIPTION = "${MESOS_SERVICE_DISPLAY_NAME}: distributed systems kernel agent"
$MESOS_PUBLIC_SERVICE_NAME = "dcos-mesos-slave-public"
$MESOS_PUBLIC_SERVICE_DISPLAY_NAME = "Mesos Agent Public"
$MESOS_PUBLIC_SERVICE_DESCRIPTION = "${MESOS_PUBLIC_SERVICE_DISPLAY_NAME}: distributed systems kernel public agent"
$MESOS_AGENT_PORT = 5051
$MESOS_DIR = Join-Path $DCOS_DIR "mesos"
$MESOS_BIN_DIR = Join-Path $MESOS_DIR "bin"
$MESOS_WORK_DIR = Join-Path $MESOS_DIR "work"
$MESOS_LOG_DIR = Join-Path $MESOS_DIR "log"
$MESOS_SERVICE_DIR = Join-Path $MESOS_DIR "service"
$MESOS_ETC_DIR = Join-Path $DCOS_ETC_DIR "mesos"
$MESOS_ETC_SERVICE_DIR = Join-Path $MESOS_ETC_DIR "service"
# From documentation:
# Amount of time to wait for an executor to register with the slave
# before considering it hung and shutting it down (e.g., 60secs, 3mins, etc)
$MESOS_REGISTER_TIMEOUT = "15mins"

# Diagnostics configurations
$DIAGNOSTICS_SERVICE_NAME = "dcos-diagnostics"
$DIAGNOSTICS_SERVICE_DISPLAY_NAME = "DC/OS Diagnostics Agent"
$DIAGNOSTICS_SERVICE_DESCRIPTION = "${DIAGNOSTICS_SERVICE_DISPLAY_NAME}: exposes component health"
$DIAGNOSTICS_AGENT_PORT = 9003
$DIAGNOSTICS_DIR = Join-Path $DCOS_DIR "diagnostics"
$DIAGNOSTICS_CONFIG_DIR = Join-Path $DIAGNOSTICS_DIR "config"
$DIAGNOSTICS_LOG_DIR = Join-Path $DIAGNOSTICS_DIR "log"

# dcos-net configurations
$DCOS_NET_LOCAL_ADDRESSES = @("198.51.100.1", "198.51.100.2", "198.51.100.3")
$DCOS_NET_SERVICE_NAME = "dcos-net"
$DCOS_NET_SERVICE_DISPLAY_NAME = "DC/OS Net"
$DCOS_NET_SERVICE_DESCRIPTION = "${DCOS_NET_SERVICE_DISPLAY_NAME}: A distributed systems & network overlay orchestration engine"
$DCOS_NET_DEVICE_NAME = "dcos-net"
$DCOS_NET_DIR = Join-Path $DCOS_DIR "dcos-net"
$DCOS_NET_BIN_DIR = Join-Path $DCOS_NET_DIR "bin"
$DCOS_NET_LOG_DIR = Join-Path $DCOS_NET_DIR "log"
$DCOS_NET_SERVICE_DIR = Join-Path $DCOS_NET_DIR "service"

# Metrics configurations
$METRICS_SERVICE_NAME = "dcos-metrics"
$METRICS_SERVICE_DISPLAY_NAME = "DC/OS Metrics Agent"
$METRICS_SERVICE_DESCRIPTION = "${METRICS_SERVICE_DISPLAY_NAME}: exposes node, container, and application metrics"
$METRICS_AGENT_PORT = 9000
$METRICS_DIR = Join-Path $DCOS_DIR "metrics"
$METRICS_CONFIG_DIR = Join-Path $METRICS_DIR "config"
$METRICS_BIN_DIR = Join-Path $METRICS_DIR "bin"
$METRICS_LOG_DIR = Join-Path $METRICS_DIR "log"
$METRICS_SERVICE_DIR = Join-Path $METRICS_DIR "service"

# AdminRouter configurations
$ADMINROUTER_SERVICE_NAME = "dcos-adminrouter"
$ADMINROUTER_SERVICE_DISPLAY_NAME = "Admin Router Agent"
$ADMINROUTER_SERVICE_DESCRIPTION = "${ADMINROUTER_SERVICE_DISPLAY_NAME}: exposes a unified control plane proxy for components and services using Apache2"
$ADMINROUTER_AGENT_PORT = 61001
$ADMINROUTER_DIR = Join-Path $DCOS_DIR "adminrouter"
$ADMINROUTER_LOG_DIR = Join-Path $ADMINROUTER_DIR "log"
$ADMINROUTER_APACHE_SUBDIR = "Apache24"
$ADMINROUTER_APACHE_DIR = Join-Path $ADMINROUTER_DIR $ADMINROUTER_APACHE_SUBDIR
$ADMINROUTER_APACHE_CONF_DIR = Join-Path $ADMINROUTER_APACHE_DIR "conf"
$ADMINROUTER_APACHE_BIN_DIR = Join-Path $ADMINROUTER_APACHE_DIR "bin"

# Other ports
$PKGPANDA_AGENT_PORT = 9001
$LOGGING_AGENT_PORT = 9002

# Installers
$VCREDIST_2013_INSTALLER = "VC_redist_2013_x64.exe"
$VCREDIST_2017_INSTALLER = "VC_redist_2017_x64.exe"

