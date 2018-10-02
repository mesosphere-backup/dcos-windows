/*
Copyright 2012 Cloudbase Solutions Srl
All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License"); you may
not use this file except in compliance with the License. You may obtain
a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations
under the License.
*/

#pragma region Includes
#include <stdio.h>
#include <windows.h>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/join.hpp>
#include <ostream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <vector>
#include "ServiceBase.h"
#include "service_exec.h"
#pragma endregion

using namespace std;
using namespace boost::program_options;

struct CLIArgs
{
    int unitType;
    wstring unitPath;
    vector<wstring> environmentFiles;
    vector<wstring> environmentFilesPShell;
    vector<wstring> environmentVars;
    vector<wstring> execStartPre;
    wstring         execStart;
    vector<wstring> execStartPost;
    wstring execStop;
    vector<wstring> execStopPost;
    wstring serviceName;
    vector<wstring> conditionArchitecture;
    vector<wstring> conditionVirtualization;
    vector<wstring> conditionHost;
    vector<wstring> conditionKernelCommandLine;
    vector<wstring> conditionKernelVersion;
    vector<wstring> conditionSecurity;
    vector<wstring> conditionCapability;
    vector<wstring> conditionACPower;
    vector<wstring> conditionNeedsUpdate;
    vector<wstring> conditionFirstBoot;
    vector<wstring> conditionPathExists;
    vector<wstring> conditionPathExistsGlob;
    vector<wstring> conditionPathIsDirectory;
    vector<wstring> conditionPathIsSymbolicLink;
    vector<wstring> conditionPathIsMountPoint;
    vector<wstring> conditionPathIsReadWrite;
    vector<wstring> conditionDirectoryNotEmpty;
    vector<wstring> conditionFileNotEmpty;
    vector<wstring> conditionFileIsExecutable;
    vector<wstring> conditionUser;
    vector<wstring> conditionGroup;
    vector<wstring> conditionControlGroupController;
    wstring logFilePath;    
    wstring serviceUnit;
    wstring shellCmd_pre;
    wstring shellCmd_post;
    enum CWrapperService::ServiceType serviceType;
    vector<wstring> requisite_files;
    vector<wstring> requisite_services;
    vector<wstring> before_files;
    vector<wstring> before_services;
    vector<wstring> after_files;
    vector<wstring> after_services;
    wstring stderrOutputType;
    wstring stderrFilePath;
    wstring stdoutOutputType;
    wstring stdoutFilePath;
    enum CWrapperService::RestartAction restartAction;
    int  restartMillis;
    int  timeoutStartMillis;
    int  timeoutStopMillis;
    int  timeoutMillis;
    int  runtimeMaxMillis;
    int  watchdogMillis;
    int  startLimitIntervalMillis;
    wstring workingDirectory;
    int verbosity;  
};

CLIArgs ParseArgs(int argc, wchar_t *argv[]);
EnvMap LoadEnvVarsFromFile(const wstring& path);
EnvMap GetCurrentEnv();

std::wstring DEFAULT_LOG_DIRECTORY         = L"C:\\var\\log\\";
std::wstring DEFAULT_LOG_PATH      = L"c:\\var\\log\\system.log";

class wojournalstream unit_stderr;
class wojournalstream unit_stdout;
class wojournalstream unit_log;

class wojournalstream *logfile = &unit_log;


//wstring DEFAULT_SHELL_PRE  =  L"powershell -command \"& {";
wstring DEFAULT_SHELL_PRE  =  L"pwsh.exe -command \"& {";
wstring DEFAULT_SHELL_POST =  L" } \" ";
wstring DEFAULT_START_ACTION = L"Write-Host \"No Start Action\" ";

CWrapperService::ServiceParams params;


// Systemd recognised units 
// for converting time spans into milliseconds in double
std::map<std::wstring, double>TimeScales = {
     { L"usec", 0.001 },
     { L"us",   0.001 },
     { L"msec", 1.000 },
     { L"ms",   1.000 },
     { L"seconds", 1000.0},
     { L"second",  1000.0},
     { L"sec",     1000.0},
     { L"s",       1000.0},
     { L"minutes", 60000.0 },
     { L"minute",  60000.0 },
     { L"min",     60000.0 },
     { L"m",       60000.0 },
     { L"hours", 3600000.0 },
     { L"hour",  3600000.0 },
     { L"hr",    3600000.0 },
     { L"h",     3600000.0 },
     { L"days",  3600000.0*24.0 },
     { L"day",   3600000.0*24.0 },
     { L"d",     3600000.0*24.0 },
     { L"weeks", 3600000.0*24.0*7.0 },
     { L"week",  3600000.0*24.0*7.0 },
     { L"w",     3600000.0*24.0*7.0 },
     { L"months",3600000.0*24.0*30.44 },
     { L"month", 3600000.0*24.0*30.44 },
     { L"M",     3600000.0*24.0*30.44 }, // (defined as 30.44 days)
     { L"years", 3600000.0*24.0*365.25 }, 
     { L"year",3600000.0*24.0*365.25 }, 
     { L"y", 3600000.0*24.0*365.25 }    // (defined as 365.25 days)
};

static boolean string_duration_to_millis(std::wstring str, double &millis)

{
    boolean done = false;

    wchar_t *ptok = (wchar_t*)str.c_str();
    wchar_t *plimit = ptok + str.length();

    millis = 0.0;
    do {
        double numval = NAN;
        double scale  = 1000.0;
        size_t toklen = 0;

        while (isspace(*ptok) && ptok < plimit) ptok++; // Skip white space
        wchar_t *tokstart = ptok;
        if (isdigit(*ptok) || *ptok == '-' || *ptok == '.' && ptok < plimit) {
            try {
                numval = stod(tokstart, &toklen);
            }
            catch (...) {
                *logfile << Warning() << L"string_duration_to_millis: malformed string: " << str << std::endl;
                return false;  // Malformed string
            }
            ptok += toklen;
        }

        while (isspace(*ptok) && ptok < plimit) ptok++; // Just ignore white space

        tokstart = ptok;
        if (isalpha(*ptok) && ptok < plimit) {
            while (isalpha(*ptok) && ptok < plimit) ptok++; // Skip white space
            wstring token(tokstart, ptok-tokstart);
            scale = TimeScales[token];
        }

        if (!isnan(scale) && !isnan(numval)) {
            millis += numval*scale;
        }
        else {
            *logfile << Warning() << L"string_duration_to_millis: malformed string: " << str << std::endl;
            return false;
        }

    } while(ptok < plimit);

    return true;
}

static void EscapeForPowershell(std::wstring &cmdline)

{
    for ( size_t start = 0; start != std::string::npos; ) {
        size_t end = cmdline.find(L'\"', start);
        if (end != std::string::npos) {
             cmdline.insert(end,  L"\\`");
             end += 3;
        }
        start = end;
    }

    for ( size_t start = 0; start != std::string::npos; ) {
        size_t end = cmdline.find(L'&', start);
        if (end != std::string::npos) {
             cmdline.insert(end,  L"\\`");
             end += 3;
        }
        start = end;
    }

}


CLIArgs ParseArgs(int argc, wchar_t *argv[])
{
    CLIArgs args;
    
    variables_map service_unit_options;

    options_description config{ "service-unit-options" };
    config.add_options()
        ("Unit.Description", value<string>(), "Description") 
        ("Unit.Requisite", wvalue<vector<wstring>>(), "Prerequisites. If not present, stop service")
        ("Unit.Wants", wvalue<vector<wstring>>(), "Prerequisites. If not present, stop service")
        ("Unit.Requires", wvalue<vector<wstring>>(), "Prerequisites. If not present, stop service")
        ("Unit.Before",    wvalue<vector<wstring>>(), "Do not run service if these things exist") 
        ("Unit.After",     wvalue<vector<wstring>>(), "Do not run service until these things exist") 
        ("Unit.ConditionArchitecture", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionVirtualization", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionHost", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionKernelCommandLine", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionKernelVersion", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionSecurity", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionCapability", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionACPower", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionNeedsUpdate", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionFirstBoot", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionPathExists", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionPathExistsGlob", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionPathIsDirectory", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionPathIsSymbolicLink", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionPathIsMountPoint", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionPathIsReadWrite", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionDirectoryNotEmpty", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionFileNotEmpty", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionFileIsExecutable", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionUser", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionGroup", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Unit.ConditionControlGroupController", wvalue<vector<wstring>>(), "Evaluate the condition and run if true")
        ("Service.Type", wvalue<wstring>(), "SystemD service type")
        ("Service.Shell", wvalue<wstring>(), "Windows Extension. Shell to use for exec actions. Default is Powershell")
        ("Service.EnvironmentFile", wvalue<vector<wstring>>(), "Environment files")
        ("Service.EnvironmentFile-PS", wvalue<vector<wstring>>(), "Environment files in Powershell format")
        ("Service.Environment", wvalue<vector<wstring>>(), "Environment Variable settings" )
        ("Service.ExecStartPre", wvalue<vector<wstring>>(), "Execute before starting service")
        ("Service.ExecStart", wvalue<wstring>(), "Execute commands at when starting service")
        ("Service.ExecStartPost", wvalue<vector<wstring>>(), "Execute after starting service")
        ("Service.ExecStop", wvalue<wstring>(), "Execute command when stopping service")
        ("Service.ExecStopPost", wvalue<vector<wstring>>(), "Execute multiple commands  after stopping service")
        ("Service.StandardOutput", wvalue<wstring>(), "standard out")
        ("Service.StandardError", wvalue<wstring>(), "standard error")
        ("Service.BusName", wvalue<wstring>(), "SystemD dbus name. Used only for resolving service type")
        ("Service.Restart", wvalue<wstring>(), "restart policy for the service")
        ("Service.RestartSec", wvalue<wstring>(), "time between restarts in seconds")
        ("Service.TimeoutStartSec",  wvalue<wstring>(), "timeout for start")
        ("Service.TimeoutStopSec",  wvalue<wstring>(), "timeout for stop")
        ("Service.TimeoutSec",     wvalue<wstring>(), "timeout used when timeout is not defined")
        ("Service.RuntimeMaxSec", wvalue<wstring>(), "maximum run time for service")
        ("Service.WatchdogSec",  wvalue<wstring>(), "watchdog timer")
        ("StartLimitIntervalSec", wvalue<wstring>(), "timeout for start")
        ("Service.WorkingDirectory", wvalue<wstring>(), "working directory")
        ("Service.StartLimitInterval", wvalue<wstring>(), "minimum time between restarts")
        ("Service.KillSignal", wvalue<wstring>(), "signal to send process when stopping. Ignored")
        ("Service.PermissionStartOnly", wvalue<wstring>(), "not implemented")
        ("Service.SupplementaryGroups", wvalue<wstring>(), "not implemented")
        ("Service.User",       wvalue<wstring>(), "not implemented")
        ("Service.LogLevelMax",    wvalue<wstring>(), "filters log output from chatty services")
        ("Service.SysLogLevel",    wvalue<wstring>(), "default log message priority")
        ("Service.LimitNOFILE", wvalue<wstring>(), "maximum number of file handles allowed for this service is not implemented");

    options_description desc{ "Options" };
    desc.add_options()
        ("debug", "run from the command line")
        ("verbosity", wvalue<wstring>(), "print info and debug spew")
        ("service-unit", wvalue<wstring>(), "Service uses the service unit file in %SystemDrive%/etc/SystemD/active" )
        ("log-file,l", wvalue<wstring>(), "Log file containing  the redirected STD OUT and ERR of the child process")
        ("service-name", wvalue<wstring>(), "Service name");

    variables_map vm;
    auto parsed = wcommand_line_parser(argc, argv)
                .options(desc).allow_unregistered().run();
    store(parsed, vm);
    auto additionalArgs = collect_unrecognized(parsed.options, include_positional);
    notify(vm);
    
    if (vm.count("debug")) {
        CServiceBase::bDebug = true;
    }

    *logfile << Verbose() << L"check for service unit" << std::endl;
    if (vm.count("service-unit")) {
        args.unitPath = vm["service-unit"].as<wstring>();
        string unit_path( args.unitPath.begin(), args.unitPath.end());
        std::ifstream service_unit_file;
        std::string service_unit_contents;
        try {
            
            service_unit_file.open(unit_path.c_str(), ios::in);
            if (!service_unit_file.is_open()) {
                *logfile << Error() << L"couldn't open " << unit_path.c_str() << std::endl;
            }

            //service_unit_contents = std::string((std::istreambuf_iterator<char>()), std::istreambuf_iterator<char>());
            char buf[256];
            service_unit_file.getline(buf, 256);

            service_unit_file.clear();
            service_unit_file.seekg(0, std::ios::beg);
        }
        catch (...) {
            *logfile << Error() << L"Problems reading service unit " << unit_path.c_str() << std::endl;
        }

        *logfile << Info() << L"opened service unit " << service_unit_contents.c_str() << std::endl;
        auto config_parsed = parse_config_file<char>(service_unit_file, config, true);
        store(config_parsed, service_unit_options);
        notify(service_unit_options);

        for(unsigned i = 0; i < args.unitPath.length(); i++ ) {
            if (args.unitPath[i] == '\\') {
                args.unitPath[i] = '/';
            }
        }
        size_t start_index = args.unitPath.rfind('/');
        args.serviceUnit = args.unitPath.substr(start_index+1, args.unitPath.length() - start_index);
        *logfile << Info() << L"has service unit " << args.serviceUnit.c_str() << std::endl;
        *logfile << Info() << L"unit path " << args.unitPath << std::endl ;
    }
    else {
        // 2do: Else derive the service name from the execStart executable
        // else give up
    }

    if (service_unit_options.count("Service.StandardOutput")) {
        args.stdoutOutputType = service_unit_options["Service.StandardOutput"].as<wstring>();
 
        if (args.stdoutOutputType.compare(0, 5, L"file:") == 0) {
            args.stdoutFilePath = args.stdoutOutputType.substr(5, args.stdoutOutputType.length());
        }
    }
    else {
        args.stdoutOutputType = L"journal";
    }

    if (args.stdoutFilePath.empty()) {
        args.stdoutFilePath = DEFAULT_LOG_DIRECTORY;
        args.stdoutFilePath.append(args.serviceUnit);
        args.stdoutFilePath.append(L".stdout.log");
    }

    *logfile << Verbose() << L"open stdoutFile output type = " << args.stdoutOutputType << std::endl;
    *logfile << Verbose() << L"open stdoutFile Path = " << args.stdoutFilePath << std::endl;
    unit_stdout.open(args.stdoutOutputType, args.stdoutFilePath);
    unit_stdout.setlogginglevel(journalstreams::LOGGING_LEVEL_INFO);
    unit_stdout.set_default_msglevel(journalstreams::LOGGING_LEVEL_INFO);

    args.stderrOutputType = L"journal";
    if ( service_unit_options.count("Service.StandardError")) {
        args.stderrOutputType = service_unit_options["Service.StandardError"].as<wstring>();
 
        if (args.stderrOutputType.compare(0, 5, L"file:") == 0) {
            args.stderrFilePath = args.stderrOutputType.substr(5, args.stderrOutputType.length());
        }
    }
    else {
        args.stderrOutputType = L"journal";
    }

    if (args.stderrFilePath.empty()) {
        args.stderrFilePath = DEFAULT_LOG_DIRECTORY;
        args.stderrFilePath.append(args.serviceUnit);
        args.stderrFilePath.append(L".stderr.log");
    }

    unit_stderr.open(args.stderrOutputType, args.stderrFilePath);
    unit_stderr.setlogginglevel(journalstreams::LOGGING_LEVEL_INFO);
    unit_stderr.set_default_msglevel(journalstreams::LOGGING_LEVEL_INFO);

    if (vm.count("log-file")) {
        args.logFilePath = vm["log-file"].as<wstring>();
        args.logFilePath.erase(remove( args.logFilePath.begin(), args.logFilePath.end(), '\"' ), args.logFilePath.end());
        args.logFilePath.erase(remove( args.logFilePath.begin(), args.logFilePath.end(), '\'' ), args.logFilePath.end());
    }

    if (vm.count("service-name")) {
        args.serviceName = vm["service-name"].as<wstring>();
        args.serviceName.erase(remove( args.serviceName.begin(), args.serviceName.end(), '\"' ), args.serviceName.end());
        args.serviceName.erase(remove( args.serviceName.begin(), args.serviceName.end(), '\'' ), args.serviceName.end());
    }

    if (service_unit_options.count("Service.Type")) {

        // Convert string to enum
        if (service_unit_options["Service.Type"].as<std::wstring>().compare(L"simple") == 0) {
            args.serviceType = CWrapperService::SERVICE_TYPE_SIMPLE;
        }
        else if (service_unit_options["Service.Type"].as<std::wstring>().compare(L"forking") == 0) {
            args.serviceType = CWrapperService::SERVICE_TYPE_FORKING;
        }
        else if (service_unit_options["Service.Type"].as<std::wstring>().compare(L"oneshot") == 0) {
            args.serviceType = CWrapperService::SERVICE_TYPE_ONESHOT;
        }
        else if (service_unit_options["Service.Type"].as<std::wstring>().compare(L"dbus") == 0) {
            args.serviceType = CWrapperService::SERVICE_TYPE_DBUS;
        }
        else if (service_unit_options["Service.Type"].as<std::wstring>().compare(L"notify") == 0) {
            args.serviceType = CWrapperService::SERVICE_TYPE_NOTIFY;
        }
        else if (service_unit_options["Service.Type"].as<std::wstring>().compare(L"idle") == 0) {
            args.serviceType = CWrapperService::SERVICE_TYPE_IDLE;
        }
        *logfile << Info() << "Service Type" << args.serviceType << std::endl;
    }
    else {

        // Unit type not defined. Figure out a default
        if (service_unit_options.count("Service.BusName")) {
            args.serviceType =CWrapperService:: SERVICE_TYPE_DBUS;
        }
        else if (service_unit_options.count("Service.ExecStart")) {
            args.serviceType = CWrapperService::SERVICE_TYPE_SIMPLE;
        }
        else {
            args.serviceType = CWrapperService::SERVICE_TYPE_ONESHOT;
        }
    }
  
    if (service_unit_options.count("Service.Restart")) {
        if (service_unit_options["Service.Restart"].as<std::wstring>().compare(L"no") == 0) {
            args.restartAction = CWrapperService::RESTART_ACTION_NO;
        }
        else if (service_unit_options["Service.Restart"].as<std::wstring>().compare(L"always") == 0) {
            args.restartAction = CWrapperService::RESTART_ACTION_ALWAYS;
        }
        else if (service_unit_options["Service.Restart"].as<std::wstring>().compare(L"on-success") == 0) {
            args.restartAction = CWrapperService::RESTART_ACTION_ON_SUCCESS;
        }
        else if (service_unit_options["Service.Restart"].as<std::wstring>().compare(L"on-failure") == 0) {
            args.restartAction = CWrapperService::RESTART_ACTION_ON_FAILURE;
        }
        else if (service_unit_options["Service.Restart"].as<std::wstring>().compare(L"on-abnormal") == 0) {
            args.restartAction = CWrapperService::RESTART_ACTION_ON_ABNORMAL;
        }
        else if (service_unit_options["Service.Restart"].as<std::wstring>().compare(L"on-abort") == 0) {
            args.restartAction = CWrapperService::RESTART_ACTION_ON_ABORT;
        }
        else if (service_unit_options["Service.Restart"].as<std::wstring>().compare(L"on-watchdog") == 0) {
            args.restartAction = CWrapperService::RESTART_ACTION_ON_WATCHDOG;
        }
        else {
            args.restartAction = CWrapperService::RESTART_ACTION_UNDEFINED;
        }
    }
    else {
        args.restartAction = CWrapperService::RESTART_ACTION_NO;
    }

    if (service_unit_options.count("Service.WorkingDirectory")) {
        args.workingDirectory = service_unit_options["Service.WorkingDirectory"].as<std::wstring>();
    }

    args.restartMillis = 100; // From systemd.service default 100ms
    if (service_unit_options.count("Service.RestartSec")) {
        wstring str_sec = service_unit_options["Service.RestartSec"].as<std::wstring>();
        double millis = 0.0;
        if (string_duration_to_millis(str_sec, millis)) {
            args.restartMillis = millis;
        }
        else {
            *logfile << Warning() << "restart_sec " << str_sec << " is not a valid string" << std::endl;
        }
    }
    *logfile << Info() << "restart_sec " << args.restartMillis << " milliseconds" << std::endl;

    args.timeoutStartMillis = 90000;  // Default timeout start
    if (service_unit_options.count("Service.TimeoutStartSec")) {
        wstring str_sec = service_unit_options["Service.TimeoutStartSec"].as<std::wstring>();
        double millis = 0.0;
        if (string_duration_to_millis(str_sec, millis)) {
            args.timeoutStartMillis = millis;
        }
        else {
            *logfile << Warning() << "timeout_start_sec " << str_sec << " is not a valid string" << std::endl;
        }
    }
    *logfile << Info() << "timeout_start_sec " << args.timeoutStartMillis << " milliseconds" << std::endl;

    args.timeoutStopMillis = 90000;  // Default timeout stop
    if (service_unit_options.count("Service.TimeoutStopSec")) {
        wstring str_sec = service_unit_options["Service.TimeoutStopSec"].as<std::wstring>();
        double millis = 0.0;
        if (string_duration_to_millis(str_sec, millis)) {
            args.timeoutStopMillis = millis;
        }
        else {
            *logfile << Warning() << "timeout_stop_sec " << str_sec << " is not a valid string" << std::endl;
        }
    }
    *logfile << Info() << "timeout_stop_sec " << args.timeoutStopMillis << " milliseconds" << std::endl;

    args.timeoutMillis = 90000;  // Default timeout
    if (service_unit_options.count("Service.TimeoutSec")) {
        wstring str_sec = service_unit_options["Service.TimeoutSec"].as<std::wstring>();
        double millis = 0.0;
        if (string_duration_to_millis(str_sec, millis)) {
            args.timeoutMillis = millis;
        }
        else {
            *logfile << Warning() << "timeout_sec " << str_sec << " is not a valid string" << std::endl;
        }
    }
    *logfile << Info() << "timeout_stop_sec " << args.timeoutMillis << " milliseconds" << std::endl;

    args.runtimeMaxMillis = 0;  // Default runtimeMax, off
    if (service_unit_options.count("Service.RuntimeMaxSec")) {
        wstring str_sec = service_unit_options["Service.RuntimeMaxSec"].as<std::wstring>();
        double millis = 0.0;
        if (string_duration_to_millis(str_sec, millis)) {
            args.runtimeMaxMillis = millis;
        }
        else {
            *logfile << Warning() << "runtime_max_sec " << str_sec << " is not a valid string" << std::endl;
        }
    }
    *logfile << Info() << "runtime_max_sec " << args.runtimeMaxMillis << " milliseconds" << std::endl;

    args.watchdogMillis = 0;  // Default watchdog
    if (service_unit_options.count("Service.WatchdogSec")) {
        wstring str_sec = service_unit_options["Service.WatchdogSec"].as<std::wstring>();
        double millis = 0.0;
        if (string_duration_to_millis(str_sec, millis)) {
            args.watchdogMillis = millis;
        }
        else {
            *logfile << Warning() << "watchdog_sec " << str_sec << " is not a valid string" << std::endl;
        }
    }
    *logfile << Info() << "watchdog_sec " << args.watchdogMillis << " milliseconds" << std::endl;

    args.startLimitIntervalMillis = 0;  // Default startLimitInterval
    if (service_unit_options.count("Service.StartLimitIntervalSec")) {
        wstring str_sec = service_unit_options["Service.StartLimitIntervalSec"].as<std::wstring>();
        double millis = 0.0;
        if (string_duration_to_millis(str_sec, millis)) {
            args.startLimitIntervalMillis = millis;
        }
        else {
            *logfile << Warning() << "start_limit_interval_sec " << str_sec << " is not a valid string" << std::endl;
        }
    }
    *logfile << Info() << "start_limit_interval_sec " << args.startLimitIntervalMillis << " milliseconds" << std::endl;

    *logfile << Info() << "service type " << args.serviceType << std::endl;
    if (service_unit_options.count("Unit.Requisite")) {

        // Sort into service and non service members. They require different code to check
        std::vector<std::wstring> wsv = service_unit_options["Unit.Requisite"].as<std::vector<std::wstring>>();
        for (auto ws: wsv) {
            if (ws.rfind(L".service") != std::string::npos ) {
                args.requisite_services.push_back(ws);
            }
            else if (ws.rfind(L".target") != std::string::npos ) {
                args.requisite_files.push_back(ws);
            }
        }
    }

    if (service_unit_options.count("Unit.Before")) {
        
        // Sort into service and non service members. They require different code to check
        std::vector<std::wstring> wsv = service_unit_options["Unit.Before"].as<std::vector<std::wstring>>();
        for (auto ws: wsv) {
            if (ws.rfind(L".service") != std::string::npos ) {
                args.before_services.push_back(ws);
            }
            else if (ws.rfind(L".target") != std::string::npos ) {
                args.before_files.push_back(ws);
            }
        }
    }

    if (service_unit_options.count("Unit.After")) {
        
        // Sort into service and non service members. They require different code to check
        std::vector<std::wstring> wsv = service_unit_options["Unit.After"].as<std::vector<std::wstring>>();

        for (auto ws: wsv) {
            if (ws.rfind(L".service") != std::string::npos ) {
                args.after_services.push_back(ws);
            }
            else if (ws.rfind(L".target") != std::string::npos ) {
                args.after_files.push_back(ws);
            }
        }
    }

    if (service_unit_options.count( "Unit.ConditionArchitecture")) {
        args.conditionArchitecture = service_unit_options["Unit.ConditionArchitecture"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionVirtualization")) {
        args.conditionVirtualization = service_unit_options["Unit.ConditionVirtualization"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionHost")) {
        args.conditionHost = service_unit_options["Unit.ConditionHost"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionKernelCommandLine")) {
        args.conditionKernelCommandLine = service_unit_options["Unit.ConditionKernelCommandLine"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionKernelVersion")) {
        args.conditionKernelVersion = service_unit_options["Unit.ConditionKernelVersion"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionSecurity")) {
        args.conditionSecurity = service_unit_options["Unit.ConditionSecurity"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionCapability")) {
        args.conditionCapability = service_unit_options["Unit.ConditionCapability"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionACPower")) {
        args.conditionACPower = service_unit_options["Unit.ConditionACPower"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionNeedsUpdate")) {
        args.conditionNeedsUpdate = service_unit_options["Unit.ConditionNeedsUpdate"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionFirstBoot")) {
        args.conditionFirstBoot = service_unit_options["Unit.ConditionFirstBoot"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionPathExists")) {
        args.conditionPathExists = service_unit_options["Unit.ConditionPathExists"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionPathExistsGlob")) {
        args.conditionPathExistsGlob = service_unit_options["Unit.ConditionPathExistsGlob"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionPathIsDirectory")) {
        args.conditionPathIsDirectory = service_unit_options["Unit.ConditionPathIsDirectory"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionPathIsSymbolicLink")) {
        args.conditionPathIsSymbolicLink = service_unit_options["Unit.ConditionPathIsSymbolicLink"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionPathIsMountPoint")) {
        args.conditionPathIsMountPoint = service_unit_options["Unit.ConditionPathIsMountPoint"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count("Unit.ConditionPathIsReadWrite")) {
        args.conditionPathIsReadWrite = service_unit_options["Unit.ConditionPathIsReadWrite"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionDirectoryNotEmpty")) {
        args.conditionDirectoryNotEmpty = service_unit_options["Unit.ConditionDirectoryNotEmpty"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count("Unit.ConditionFileNotEmpty")) {
        args.conditionFileNotEmpty = service_unit_options["Unit.ConditionFileNotEmpty"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count( "Unit.ConditionFileIsExecutable")) {
        args.conditionFileIsExecutable = service_unit_options["Unit.ConditionFileIsExecutable"].as<std::vector<std::wstring>>();
    }
    if (service_unit_options.count("Unit.ConditionUser")) {
        args.conditionUser = service_unit_options["Unit.ConditionUser"].as<std::vector<std::wstring>>();
    }

    if (service_unit_options.count("Unit.ConditionGroup")) {
        args.conditionGroup = service_unit_options["Unit.ConditionGroup"].as<std::vector<std::wstring>>();
    }

    if (service_unit_options.count("Unit.ConditionControlGroupController")) {
        args.conditionControlGroupController = service_unit_options["Unit.ConditionControlGroupController"].as<std::vector<std::wstring>>();
    }

    if (service_unit_options.count("Service.Shell")) {
        wstring shellname = service_unit_options["Service.Shell"].as<wstring>();
        transform(shellname.begin(), shellname.end(), shellname.begin(), tolower);
        if (shellname.compare(L"powershell") == 0) {
            args.shellCmd_pre = L"powershell -command \"& {";
            args.shellCmd_post = L" } \" ";
        }
        else if (shellname.compare(L"cmd") == 0) {
            args.shellCmd_pre = L"cmd /c \'";  // Untested. Fix if needed;
            args.shellCmd_post = L"\' ";
        }
        else if (shellname.compare(L"bash") == 0) {
            args.shellCmd_pre = L"bash -c \'";  // Untested. Fix if needed;
            args.shellCmd_post = L"\' ";
        }
        else {
            wostringstream ws;
            ws << "Unsupported shell type " << shellname << " specified in service unit " 
                                            << args.serviceName << std::endl;
            throw exception(std::string(ws.str().begin(), ws.str().end()).c_str());
        }
    }
    else {
        args.shellCmd_pre = DEFAULT_SHELL_PRE;
        args.shellCmd_post = DEFAULT_SHELL_POST;
    }

    if (service_unit_options.count("Service.Environment")) {
        args.environmentVars = service_unit_options["Service.Environment"].as<vector<wstring>>();
    }

    *logfile << Debug() << L"service_unit_options.count(\"Service.EnvironmentFile\") " << service_unit_options.count("Service.EnvironmentFile") << std::endl;
    if (service_unit_options.count("Service.EnvironmentFile")) {
        args.environmentFiles = service_unit_options["Service.EnvironmentFile"].as<vector<wstring>>();
        *logfile << Debug() << L"Env file" <<std::endl;
    }

    if (service_unit_options.count("Service.EnvironmentFile-PS")) {
        args.environmentFilesPShell = service_unit_options["Service.EnvironmentFilesPS"].as<vector<wstring>>();
    }

    if (service_unit_options.count("Service.ExecStartPre")) {
        vector<wstring> ws_vector = service_unit_options["Service.ExecStartPre"].as<vector<wstring>>();

        for (auto ws : ws_vector) {
           EscapeForPowershell(ws);
           *logfile << Debug() << "p5.1 exec start pre cmdline = " << ws << std::endl;
        }
        args.execStartPre = ws_vector;
    }

    if (service_unit_options.count("Service.ExecStart")) {

        args.execStart = service_unit_options["Service.ExecStart"].as<wstring>();
        EscapeForPowershell(args.execStart);
        *logfile << Debug() << "p6.1 execstart = " << args.execStart << std::endl;
    }

    if (service_unit_options.count("Service.ExecStartPost")) {
        vector<wstring> ws_vector = service_unit_options["Service.ExecStartPost"].as<vector<wstring>>();

        for (auto ws : ws_vector) {
            EscapeForPowershell(ws);
            *logfile << Debug() << "p6.2 execstartpost = " << ws << std::endl;
        }
        args.execStartPost = ws_vector;
    }

    if (service_unit_options.count("Service.ExecStop")) {
        args.execStop = service_unit_options["Service.ExecStop"].as<wstring>();
        EscapeForPowershell(args.execStop);
        *logfile << Debug() << "p6.1 execstop = " << args.execStop << std::endl;
    }


    if (service_unit_options.count("Service.ExecStopPost")) {
        vector<wstring> ws_vector = service_unit_options["Service.ExecStopPost"].as<vector<wstring>>();
        for (auto ws : ws_vector) {
            EscapeForPowershell(ws);
            *logfile << Debug() << "p6.2 execstoppost = " << ws << std::endl;
        }
        args.execStopPost = ws_vector;
    }

    *logfile << Debug() << L"service name " << args.serviceName << std::endl;
    for (auto arg: additionalArgs ) {
        *logfile << Debug() << L"additionalArgs " << arg << std::endl;
    }

    if(args.serviceName.empty()) {
        throw exception("Service name not provided");
    }
    
    return args;
}



int wmain(int argc, wchar_t *argv[])
{
    try
    {
        EnvMap env;

        unit_log.open(L"file:", L"c:\\var\\log\\systemd.log");

        unit_log.setlogginglevel(journalstreams::LOGGING_LEVEL_WARNING);
        unit_log.set_default_msglevel(journalstreams::LOGGING_LEVEL_INFO);
        auto args = ParseArgs(argc, argv);

        *logfile << Error() << L"log file name " << args.logFilePath.c_str() << std::endl;

        params.szServiceName  = args.serviceName.c_str();
        params.szShellCmdPre  = L"";
        params.szShellCmdPost = L"";

        params.execStartPre = args.execStartPre;
        params.execStart      = args.execStart;
        params.execStartPost  = args.execStartPost;
        params.execStop       = args.execStop;
        params.execStopPost   = args.execStopPost;
        params.environmentFilesPS = args.environmentFilesPShell;
        params.environmentFiles   = args.environmentFiles;
        params.environmentVars    = args.environmentVars;
        params.files_before       = args.before_files;
        params.services_before    = args.before_services;
        params.files_after        = args.after_files;
        params.services_after     = args.after_services;
        params.files_requisite    = args.requisite_files;
        params.services_requisite = args.requisite_services;
        params.stdErr         = &unit_stderr;
        params.stdOut         = &unit_stdout;
        params.fCanStop       = TRUE;
        params.fCanShutdown   = TRUE;
        params.fCanPauseContinue = TRUE;
 
        params.conditionArchitecture = args.conditionArchitecture;
        params.conditionVirtualization = args.conditionVirtualization;
        params.conditionHost = args.conditionHost;
        params.conditionKernelCommandLine = args.conditionKernelCommandLine;
        params.conditionKernelVersion = args.conditionKernelVersion;
        params.conditionSecurity = args.conditionSecurity;
        params.conditionCapability = args.conditionCapability;
        params.conditionACPower = args.conditionACPower;
        params.conditionNeedsUpdate = args.conditionNeedsUpdate;
        params.conditionFirstBoot = args.conditionFirstBoot;
        params.conditionPathExists = args.conditionPathExists;
        params.conditionPathExistsGlob = args.conditionPathExistsGlob;
        params.conditionPathIsDirectory = args.conditionPathIsDirectory;
        params.conditionPathIsSymbolicLink = args.conditionPathIsSymbolicLink;
        params.conditionPathIsMountPoint = args.conditionPathIsMountPoint;
        params.conditionPathIsReadWrite = args.conditionPathIsReadWrite;
        params.conditionDirectoryNotEmpty = args.conditionDirectoryNotEmpty;
        params.conditionFileNotEmpty = args.conditionFileNotEmpty;
        params.conditionFileIsExecutable = args.conditionFileIsExecutable;
        params.conditionUser = args.conditionUser;
        params.conditionGroup = args.conditionGroup;
        params.conditionControlGroupController = args.conditionControlGroupController;
        params.restartAction = args.restartAction;
        params.restartMillis = args.restartMillis;
        params.timeoutStopMillis = args.timeoutStopMillis;
        params.workingDirectory = args.workingDirectory;

        CWrapperService service(params);
        if (!CServiceBase::Run(service))
        {
            char msg[100];
            sprintf_s(msg, "Service failed to run w/err GetLastError: 0x%08lx", GetLastError());
            throw exception(msg);
        }
        if (CWrapperService::bDebug) {
            // For off-line debug

            ::Sleep(10000); // Let the service start
            service.Stop();
        }

        return 0;
    }
    catch (exception &ex)
    {
        *logfile << Error() << ex.what() << '\n';
        return -1;
    }

}
