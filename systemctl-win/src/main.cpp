/*
 * **==============================================================================
 * **
 * ** Copyright (c) Microsoft Corporation. All rights reserved. See file LICENSE
 * ** for license information.
 * **
 * **==============================================================================
 * */
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#if VC_SUPPORTS_STD_FILESYSTEM
#include <filesystem> Not yet supported in vc
#endif
#include "windows.h"
#include "service_unit.h"
#include <map>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/join.hpp>


using namespace std;

namespace SystemCtlHelp {
    enum CMD_TYPE {
        UNIT_CMD = 1,
        UNIT_FILE_CMD,
        MACHINE_CMD,
        JOB_CMD,
        ENV_CMD,
        LIFECYCLE_CMD,
        SYSTEM_CMD,
    };

    std::map< std::wstring , std::tuple<std::wstring, int, std::wstring>> Help_Map =  {
        { L"list-units",     { L"list-units [PATTERN...] ",      UNIT_CMD, L"List units currently in memory" } },
        { L"list-sockets",   { L"list-sockets [PATTERN...] ",    UNIT_CMD, L"List socket units currently in memory, ordered by address" } },
        { L"list-timers",    { L"list-timers [PATTERN...] ",     UNIT_CMD, L"List timer units currently in memory, ordered by next elapse" } },
        { L"start",          { L"start NAME... ",                UNIT_CMD, L"Start (activate) one or more units" } },
        { L"stop",           { L"stop NAME... ",                 UNIT_CMD, L"Stop (deactivate) one or more units" } },
        { L"reload",         { L"reload NAME... ",               UNIT_CMD, L"Reload one or more units" } },
        { L"restart",        { L"restart NAME... ",              UNIT_CMD, L"Start or restart one or more units" } },
        { L"try-restart",    { L"try-restart NAME... ",          UNIT_CMD, L"Restart one or more units if active" } },
        { L"reload-or-restart", { L"reload-or-restart NAME... ", UNIT_CMD, L"Reload one or more units if possible, otherwise start or restart" } },
        { L"try-reload-or-restart",    { L"try-reload-or-restart NAME... ", UNIT_CMD, L"If active, reload one or more units, if supported, otherwise restart" } },
        { L"isolate",        { L"isolate NAME ",                 UNIT_CMD, L"Start one unit and stop all others" } },
        { L"kill",           { L"kill NAME... ",                 UNIT_CMD, L"Send signal to processes of a unit" } },
        { L"is-active",      { L"is-active PATTERN... ",         UNIT_CMD, L"Check whether units are active" } },
        { L"is-failed",      { L"is-failed PATTERN... ",         UNIT_CMD, L"Check whether units are failed" } },
        { L"status",         { L"status [PATTERN... UNIT|PID...] ", UNIT_CMD, L"Show runtime status of one or more units" } },
        { L"show",           { L"show [PATTERN... UNIT...] ",    UNIT_CMD, L"Show properties of one or more units/jobs or the manager" } },
        { L"cat",            { L"cat PATTERN... ",               UNIT_CMD, L"Show files and drop-ins of one or more units" } },
        { L"set-property",   { L"set-property NAME ASSIGNMENT... ", UNIT_CMD, L"Sets one or more properties of a unit" } },
        { L"help",           { L"help PATTERN... UNIT|PID... ",  UNIT_CMD, L"Show manual for one or more units" } },
        { L"reset-failed",   { L"reset-failed [PATTERN...] ", UNIT_CMD, L"Reset failed state for all, one, or more units" } },
        { L"list-dependencies",  { L"list-dependencies [NAME] ", UNIT_CMD, L"Recursively show units which are required or wanted by this unit or by which this unit is required or wanted" } },
        { L"list-unit-files", { L"list-unit-files [PATTERN...] ",UNIT_FILE_CMD, L"List installed unit files"} },
        { L"enable",         { L"enable [NAME...| PATH...] ",    UNIT_FILE_CMD, L"Enable one or more unit files"} },
        { L"disable",        { L"disable NAME... ",              UNIT_FILE_CMD, L"Disable one or more unit files"} },
        { L"reenable",       { L"reenable NAME... ",             UNIT_FILE_CMD, L"Reenable one or more unit files"} },
        { L"preset",         { L"preset NAME... ",               UNIT_FILE_CMD, L"Enable/disable one or more unit files based on preset configuration"} },
        { L"preset-all",     { L"preset-all ",                   UNIT_FILE_CMD, L"Enable/disable all unit files based on preset configuration"} },
        { L"is-enabled",     { L"is-enabled NAME... ",           UNIT_FILE_CMD, L"Check whether unit files are enabled"} },
        { L"mask",           { L"mask NAME... ",                 UNIT_FILE_CMD, L"Mask one or more units"} },
        { L"unmask",         { L"unmask NAME... ",               UNIT_FILE_CMD, L"Unmask one or more units"} },
        { L"link",           { L"link PATH... ",                 UNIT_FILE_CMD, L"Link one or more units files into the search path"} },
        { L"revert",         { L"revert NAME... ",               UNIT_FILE_CMD, L"Revert one or more unit files to vendor version" } },
        { L"add-wants",      { L"add-wants TARGET NAME... ",     UNIT_FILE_CMD, L"Add 'Wants' dependency for the target on specified one or more units" } },
        { L"add-requires",   { L"add-requires TARGET NAME... ",  UNIT_FILE_CMD, L"Add 'Requires' dependency for the target on specified one or more units" } },
        { L"edit",           { L"edit NAME... ",                 UNIT_FILE_CMD, L"Edit one or more unit files" } },
        { L"get-default",    { L"get-default ",                  UNIT_FILE_CMD, L"Get the name of the default target" } },
        { L"set-default",    { L"set-default NAME ",             UNIT_FILE_CMD, L"Set the default target" } },
        { L"list-machines",  { L"list-machines [PATTERN...] ",   MACHINE_CMD,   L"List local containers and host" } },
        { L"list-jobs",      { L"list-jobs [PATTERN...] ",       JOB_CMD,       L"List jobs" } },
        { L"cancel",         { L"cancel [JOB...] ",              JOB_CMD,       L"Cancel all, one, or more jobs" } },
        { L"show-environment",{ L"show-environment ",            ENV_CMD,       L"Dump environment" } },
        { L"set-environment", { L"set-environment NAME=VALUE... ",ENV_CMD,      L"Set one or more environment variables" } },
        { L"unset-environment",{ L"unset-environment NAME... ",  ENV_CMD,       L"Unset one or more environment variables" } },
        { L"import-environment",{ L"import-environment [NAME...] ", ENV_CMD,    L"Import all or some environment variables" } },
        { L"daemon-reload",  { L"daemon-reload ",                LIFECYCLE_CMD, L"Reload systemd manager configuration" } },
        { L"daemon-reexec",  { L"daemon-reexec ",                LIFECYCLE_CMD, L"Reexecute systemd manager" } },
        { L"is-system-running", { L"is-system-running ",         SYSTEM_CMD,    L"Check whether system is fully running" } },
        { L"default",        { L"default ",                      SYSTEM_CMD,    L"Enter system default mode" } },
        { L"rescue",         { L"rescue ",                       SYSTEM_CMD,    L"Enter system rescue mode" } },
        { L"emergency",      { L"emergency ",                    SYSTEM_CMD,    L"Enter system emergency mode" } },
        { L"halt",           { L"halt ",                         SYSTEM_CMD,    L"Shut down and halt the system" } },
        { L"poweroff",       { L"poweroff ",                     SYSTEM_CMD,    L"Shut down and power-off the system" } },
        { L"reboot",         { L"reboot [ARG] ",                 SYSTEM_CMD,    L"Shut down and reboot the system" } },
        { L"kexec",          { L"kexec ",                        SYSTEM_CMD,    L"Shut down and reboot the system with kexec" } },
        { L"exit",           { L"exit [EXIT_CODE] ",             SYSTEM_CMD,    L"Request user instance or container exit" } },
        { L"switch-root",    { L"switch-root ROOT [INIT] ",      SYSTEM_CMD,    L"Change to a different root file system" } },
        { L"suspend",        { L"suspend ",                      SYSTEM_CMD,    L" Suspend the system" } },
        { L"hibernate",      { L"hibernate ",                    SYSTEM_CMD,    L" Hibernate the system" } },
        { L"hybrid-sleep",   { L"hybrid-sleep ",                 SYSTEM_CMD,    L" Hibernate and suspend the system" } },
        { L"suspend-then-hibernate", { L"suspend-then-hibernate ", SYSTEM_CMD,  L" Suspend the system, wake after a period of time and put it into hibernate" } },
    };
};

void PrintCommandUsage(const wstring &cmd, const wstring &local_msg = L"")
{
    if (!local_msg.empty()) {
        wcerr << local_msg << std::endl;
    }
    auto help_entry = SystemCtlHelp::Help_Map[cmd];
    std::wstring  cmdname = std::get<0>(help_entry);
    std::wstring  cmddesc = std::get<2>(help_entry);
    wcerr << L"Usage:" << std::endl;
    wcerr << L"    systemctl " << cmdname << std::endl;
    wcerr << L"        " << cmddesc << std::endl;
}

void PrintMasterUsage(std::wstring binary_name, boost::program_options::options_description &desc )

{
    // show master usage message
    wcout << "Usage: " << binary_name << " command [options] " << std::endl;
    wcout << L"Unit commands:" << std::endl;
    for (auto const &this_entry: SystemCtlHelp::Help_Map) {
        auto help_entry = this_entry.second;
        int cmdtype = std::get<1>(help_entry);
        if (cmdtype == SystemCtlHelp::UNIT_CMD) {
            std::wstring  cmdname = std::get<0>(help_entry);
            std::wstring  cmddesc = std::get<2>(help_entry);
            wcout << std::setw(30) << cmdname;
            wcout << L" ";
            wcout << std::setw(-100) << cmddesc;
            wcout << std::endl;
        }
    }
    wcout << std::endl;

    wcout << L"Unit File commands:" << std::endl;
    for (auto const &this_entry: SystemCtlHelp::Help_Map) {
        auto help_entry = this_entry.second;
        int cmdtype = std::get<1>(help_entry);
        if (cmdtype == SystemCtlHelp::UNIT_FILE_CMD) {
            std::wstring  cmdname = std::get<0>(help_entry);
            std::wstring  cmddesc = std::get<2>(help_entry);
            wcout << std::setw(30) << cmdname;
            wcout << L" ";
            wcout << std::setw(-100) << cmddesc;
            wcout << std::endl;
        }
    }
    wcout << std::endl;

    wcout << L"Environment commands:" << std::endl;
    for (auto const &this_entry: SystemCtlHelp::Help_Map) {
        auto help_entry = this_entry.second;
        int cmdtype = std::get<1>(help_entry);
        if (cmdtype == SystemCtlHelp::ENV_CMD) {
            std::wstring  cmdname = std::get<0>(help_entry);
            std::wstring  cmddesc = std::get<2>(help_entry);
            wcout << std::setw(30) << cmdname;
            wcout << L" ";
            wcout << std::setw(-100) << cmddesc;
            wcout << std::endl;
        }
    }
    wcout << std::endl;
    wcout << L"Manager Lifecycle commands:" << std::endl;
    for (auto const &this_entry: SystemCtlHelp::Help_Map) {
        auto help_entry = this_entry.second;
        int cmdtype = std::get<1>(help_entry);
        if (cmdtype == SystemCtlHelp::ENV_CMD) {
            std::wstring  cmdname = std::get<0>(help_entry);
            std::wstring  cmddesc = std::get<2>(help_entry);
            wcout << std::setw(30) << cmdname;
            wcout << L" ";
            wcout << std::setw(-100) << cmddesc;
            wcout << std::endl;
        }
    }
    wcout << std::endl;
    wcout << "Options:" << std::endl;

    cout << desc;
}


static const int NOT_IMPLEMENTED = -1;

wstring SystemDUnitPool::UNIT_DIRECTORY_PATH = L""; // Quasi constant
wstring SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH = L""; // Quasi constant
wstring SystemDUnitPool::UNIT_WORKING_DIRECTORY_PATH = L""; // Quasi constant


enum SystemCtl_Cmd {
    SYETEMCTL_CMD_DAEMON_RELOAD = 0,
    SYSTEMCTL_CMD_LIST_UNITS,
    SYSTEMCTL_CMD_LIST_SOCKETS,
    SYSTEMCTL_CMD_LIST_TIMERS,
    SYSTEMCTL_CMD_START,
    SYSTEMCTL_CMD_STOP,
    SYSTEMCTL_CMD_RELOAD,
    SYSTEMCTL_CMD_RESTART,
    SYSTEMCTL_CMD_TRY_RESTART,
    SYSTEMCTL_CMD_RELOAD_OR_RESTART,
    SYSTEMCTL_CMD_TRY_RELOAD_OR_RESTART,
    SYSTEMCTL_CMD_ISOLATE,
    SYSTEMCTL_CMD_KILL,
    SYSTEMCTL_CMD_IS_ACTIVE,
    SYSTEMCTL_CMD_IS_FAILED,
    SYSTEMCTL_CMD_STATUS,
    SYSTEMCTL_CMD_SHOW,
    SYSTEMCTL_CMD_CAT,
    SYSTEMCTL_CMD_SET_PROPERTY,
    SYSTEMCTL_CMD_HELP,
    SYSTEMCTL_CMD_RESET_FAILED,
    SYSTEMCTL_CMD_LIST_DEPENDENCIES,
    SYSTEMCTL_CMD_LIST_UNIT_FILES,
    SYSTEMCTL_CMD_ENABLE,
    SYSTEMCTL_CMD_DISABLE,
    SYSTEMCTL_CMD_REENABLE,
    SYSTEMCTL_CMD_PRESET,
    SYSTEMCTL_CMD_PRESET_ALL,
    SYSTEMCTL_CMD_IS_ENABLED,
    SYSTEMCTL_CMD_MASK,
    SYSTEMCTL_CMD_UNMASK,
    SYSTEMCTL_CMD_LINK,
    SYSTEMCTL_CMD_REVERT,
    SYSTEMCTL_CMD_ADD_WANTS,
    SYSTEMCTL_CMD_ADD_REQUIRES,
    SYSTEMCTL_CMD_EDIT,
    SYSTEMCTL_CMD_GET_DEFAULT,
    SYSTEMCTL_CMD_SET_DEFAULT,
    SYSTEMCTL_CMD_LIST_MACHINES,
    SYSTEMCTL_CMD_LIST_JOBS,
    SYSTEMCTL_CMD_CANCEL,
    SYSTEMCTL_CMD_SHOW_ENVIRONMENT,
    SYSTEMCTL_CMD_SET_ENVIRONMENT,
    SYSTEMCTL_CMD_UNSET_ENVIRONMENT,
    SYSTEMCTL_CMD_IMPORT_ENVIRONMENT,
    SYSTEMCTL_CMD_DAEMON_RELOAD,
    SYSTEMCTL_CMD_DAEMON_REEXEC,
    SYSTEMCTL_CMD_IS_SYSTEM_RUNNING,
    SYSTEMCTL_CMD_DEFAULT,
    SYSTEMCTL_CMD_RESCUE,
    SYSTEMCTL_CMD_EMERGENCY,
    SYSTEMCTL_CMD_HALT,
    SYSTEMCTL_CMD_POWEROFF,
    SYSTEMCTL_CMD_REBOOT,
    SYSTEMCTL_CMD_KEXEC,
    SYSTEMCTL_CMD_EXIT,
    SYSTEMCTL_CMD_SWITCH_ROOT,
    SYSTEMCTL_CMD_SUSPEND,
    SYSTEMCTL_CMD_HIBERNATE,
    SYSTEMCTL_CMD_HYBRID_SLEEP
};


int SystemCtrl_Cmd_List_Units( boost::program_options::variables_map &vm )
{
    std::map<std::wstring, class SystemDUnit*> pool = g_pool->GetPool();

    for(std::map<std::wstring, class SystemDUnit *>::iterator it = pool.begin(); it != pool.end(); ++it) {
        SystemDUnit *punit = it->second;
        wcout << punit->Name() << std::endl;
    }

    return 0;
}


int SystemCtrl_Cmd_List_Sockets( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_List_Timers( boost::program_options::variables_map &vm )
{
    return -1;
}

int SystemCtrl_Cmd_Start( boost::program_options::variables_map &vm )
{
    vector<wstring> units;
    if (vm.count("system_units")) {
        units = vm["system_units"].as<vector<wstring>>();
    } 
    else {
        // Complain and exit

        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        PrintCommandUsage(vm["command"].as<wstring>(), L"no unit specified");
        exit(1);
    }

    for (wstring unitname: units) {
    
        if (unitname.rfind(L".service") == string::npos &&
            unitname.rfind(L".target")  == string::npos &&
            unitname.rfind(L".timer")   == string::npos &&
            unitname.rfind(L".socket")  == string::npos ) {
              unitname.append(L".service");
        }

        class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << L"Failed to enable unit: Unit file " << unitname.c_str() << L"does not exist";
            PrintCommandUsage(vm["command"].as<wstring>(), SystemCtlLog::msg.str());
            SystemCtlLog::Error();
            exit(1);
        }
        unit->StartService(false); // We will add non-blocking later
    }

    return 0;
}


int SystemCtrl_Cmd_Stop( boost::program_options::variables_map &vm )
{

    vector<wstring> units;
    if (vm.count("system_units")) {
        units = vm["system_units"].as<vector<wstring>>();
    } 
    else {
        // Complain and exit

        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        PrintCommandUsage(vm["command"].as<wstring>(), L"no unit specified");
        exit(1);
    }

    for (wstring unitname: units) {
        if (unitname.rfind(L".service") == string::npos &&
            unitname.rfind(L".target")  == string::npos &&
            unitname.rfind(L".timer")   == string::npos &&
            unitname.rfind(L".socket")  == string::npos ) {
              unitname.append(L".service");
        }
    
        wcerr << L"Stop service " << unitname << std::endl;
        class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << "Failed to stop service: Unit file " << unitname.c_str() << " does not exist\n";
            PrintCommandUsage(vm["command"].as<wstring>(), SystemCtlLog::msg.str());
            SystemCtlLog::Error();
            exit(1);
        }
        unit->StopService(false); // We will add non-blocking later
    }
    return 0;
}


int SystemCtrl_Cmd_Reload( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Restart( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Try_Restart( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Reload_Or_Restart( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Try_Reload_Or_Restart( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Isolate( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Kill( boost::program_options::variables_map &vm )

{
    static std::map<std::wstring, int>kill_who_dict = {
              { L"main",    SystemCtl::KILL_ACTION_MAIN},
              { L"control", SystemCtl::KILL_ACTION_CONTROL},
              { L"all",     SystemCtl::KILL_ACTION_ALL }
         };

    static std::map<std::wstring, int>signal_dict = {
            { L"SIGHUP",   1},
            { L"SIGINT",   2},
            { L"SIGQUIT",  3},
            { L"SIGILL",   4},
            { L"SIGTRAP",  5},
            { L"SIGABRT",  6},
            { L"SIGIOT",   6},
            { L"SIGBUS",   7},
            { L"SIGFPE",   8},
            { L"SIGKILL",  9},
            { L"SIGUSR1", 10},
            { L"SIGSEGV", 11},
            { L"SIGUSR2", 12},
            { L"SIGPIPE", 13},
            { L"SIGALRM", 14},
            { L"SIGTERM", 15},
            { L"SIGSTKFLT", 16},
            { L"SIGCHLD", 17},
            { L"SIGCONT", 18},
            { L"SIGSTOP", 19},
            { L"SIGTSTP", 20},
            { L"SIGTTIN", 21},
            { L"SIGTTOU", 22},
            { L"SIGURG", 23},
            { L"SIGXCPU", 24},
            { L"SIGXFSZ", 25},
            { L"SIGVTALRM",26},
            { L"SIGPROF", 27},
            { L"SIGWINCH",28},
            { L"SIGIO", 29},
            { L"SIGPOLL", 29},
            { L"SIGLOST", 29},
            { L"SIGPWR", 30},
            { L"SIGSYS", 31},
            { L"1",   1},    // We do this so we don't have to parse a command with just the number .In linux land just the number is equivalent.
            { L"2",   2},
            { L"3",  3},
            { L"4",   4},
            { L"5",  5},
            { L"6",  6},
            { L"7",   7},
            { L"8",   8},
            { L"9L",  9},
            { L"10", 10},
            { L"11", 11},
            { L"12", 12},
            { L"13", 13},
            { L"14", 14},
            { L"15", 15},
            { L"16", 16},
            { L"17", 17},
            { L"18", 18},
            { L"19", 19},
            { L"20", 20},
            { L"21", 21},
            { L"22", 22},
            { L"23", 23},
            { L"24", 24},
            { L"25", 25},
            { L"26",26},
            { L"27", 27},
            { L"28",28},
            { L"29", 29},
            { L"30", 30},
            { L"31", 31}
         };

    vector<wstring> units;
    if (vm.count("system_units")) {
        units = vm["system_units"].as<vector<wstring>>();
    } 
    else {
        // Complain and exit

        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        PrintCommandUsage(vm["command"].as<wstring>(), L"no unit specified");
        exit(1);
    }

    for (wstring unitname: units) {
        if (unitname.rfind(L".service") == string::npos) {
              unitname.append(L".service");
        }

        int kill_action = SystemCtl::KILL_ACTION_ALL;
        if (vm.count("kill-who")) {
            std::wstring who = vm["kill-who"].as<wstring>();
            kill_action = kill_who_dict[who];
            if (!kill_action) {
                // Illegal Value
                 SystemCtlLog::msg << L"Kill action " << vm["kill-who"].as<wstring>() << L"is unknown";
                 SystemCtlLog::Error();
                 return 1;
            }
        }

        int kill_signal;
        if (vm.count("signal")) {
            std::wstring signame = vm["signal"].as<wstring>();
            kill_signal = signal_dict[signame];
                if (!kill_signal) {
                SystemCtlLog::msg << L"Signal " << vm["signal"].as<wstring>() << L"is unknown";
                SystemCtlLog::Error();
                return 1;
            }
        }

        class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << L"Failed to find unit to kill: Unit file " << unitname.c_str() << L" does not exist" << std::endl;
            SystemCtlLog::Error();
            return 1;
        }
        unit->Kill(kill_signal, kill_action, false); // We will add non-blocking later
    }
    return 0;
}


int SystemCtrl_Cmd_Is_Active( boost::program_options::variables_map &vm )
{

    if (vm["system_units"].empty()) {
        // Complain and exit
        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        exit(1);
    }

    vector<wstring> units = vm["system_units"].as<vector<wstring>>();
    for (wstring unitname: units) {
        SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
        // Not an error.
            wcout << L"inactive";
            exit(1);
        }
        if (unit->IsActive() ) {
            wcout << L"active";
            exit(0);
        }
        else {
            wcout << L"inactive";
            exit(1);
        }
    }
    return 0;
}


int SystemCtrl_Cmd_Is_Failed( boost::program_options::variables_map &vm )
{
    if (vm["system_units"].empty()) {
        // Complain and exit
        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        exit(1);
    }
    vector<wstring> units = vm["system_units"].as<vector<wstring>>();
    for (wstring unitname: units) {
        SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            wcout << L"inactive";
            exit(1);
        }
        if (unit->IsFailed() ) {
            wcout << L"failed";
            exit(0);
        }
        else {
            wcout << L"inactive";
            exit(1);
        }
    }
    return 0;
}


int SystemCtrl_Cmd_Status( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Show( boost::program_options::variables_map &vm )
{

    vector<wstring> units;
    if (vm.count("system_units")) {
        units = vm["system_units"].as<vector<wstring>>();
    } 
    else {

        g_pool->ShowGlobal();
        return 0;
    }

    for (wstring unitname: units) {
        if (unitname.rfind(L".service") == string::npos &&
            unitname.rfind(L".target")  == string::npos &&
            unitname.rfind(L".timer")   == string::npos &&
            unitname.rfind(L".socket")  == string::npos ) {
            unitname.append(L".service");
        }
    
        SystemCtlLog::msg << L"Show service " << unitname; SystemCtlLog::Info();

        class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << L"Failed to show service: Unit file " << unitname.c_str() << L" does not exist";
            PrintCommandUsage(vm["command"].as<wstring>(), SystemCtlLog::msg.str());
            SystemCtlLog::Error();
            exit(1);
        }
        unit->ShowService(); // We will add non-blocking later
    }
    return 0;
}


int SystemCtrl_Cmd_Cat( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Set_Property( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Help( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Reset_Failed( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_List_Dependencies( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_List_Unit_Files( boost::program_options::variables_map &vm )

{
    std::map<std::wstring, class SystemDUnit*> pool = g_pool->GetPool();

    for(std::map<std::wstring, class SystemDUnit *>::iterator it = pool.begin(); it != pool.end(); ++it) {
        SystemDUnit *punit = it->second;
        wcout << punit->FilePath() << std::endl;
    }

    return 0;
}


int SystemCtrl_Cmd_Enable( boost::program_options::variables_map &vm )
{

    vector<wstring> units;
    if (vm.count("system_units")) {
        units = vm["system_units"].as<vector<wstring>>();
    } 
    else {
        // Complain and exit

        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        PrintCommandUsage(vm["command"].as<wstring>(), L"no unit specified");
        exit(1);
    }

    for (wstring unitname: units) {
        // We allow a shorthand reference via just the service name, but 
        // we recognise the valid file extensions if given.
        if (unitname.rfind(L".service") == string::npos &&
            unitname.rfind(L".target")  == string::npos &&
            unitname.rfind(L".timer")   == string::npos &&
            unitname.rfind(L".socket")  == string::npos ) {
            unitname.append(L".service");
        }
    
        wstring service_unit_path = SystemDUnitPool::UNIT_DIRECTORY_PATH+L"\\"+unitname; // We only look for service unit files in the top level directory
        class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            unit = SystemDUnitPool::ReadServiceUnit(unitname, service_unit_path);
            if (!unit) {
                // Complain and exit
                SystemCtlLog::msg << L"Failed to load unit: Unit file " << service_unit_path.c_str() << L"is invalid\n";
                PrintCommandUsage(vm["command"].as<wstring>(), SystemCtlLog::msg.str());
                SystemCtlLog::Error();
                return -1;       
           }
        }
        unit->Enable(false); // We will add non-blocking later
    }

    ::Sleep(1000);

    return 0;
}


int SystemCtrl_Cmd_Disable( boost::program_options::variables_map &vm )
{
    vector<wstring> units;
    if (vm.count("system_units")) {
        units = vm["system_units"].as<vector<wstring>>();
    } 
    else {
        // Complain and exit

        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        PrintCommandUsage(vm["command"].as<wstring>(), L"no unit specified");
        exit(1);
    }

    for (wstring unitname: units) {
        if (unitname.rfind(L".service") == string::npos) {
              unitname.append(L".service");
        }
    
        class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << L"Failed to enable unit: Unit file " << unitname.c_str() << L" does not exist";
            PrintCommandUsage(vm["command"].as<wstring>(), SystemCtlLog::msg.str());
            SystemCtlLog::Error();
            exit(1);
        }
        unit->Disable(false); // We will add non-blocking later
    }
    return 0;
}


int SystemCtrl_Cmd_Reenable( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Preset( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Preset_All( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Is_Enabled( boost::program_options::variables_map &vm )
{
    if (vm["system_units"].empty()) {
        // Complain and exit
        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        exit(1);
    }
    vector<wstring> units = vm["system_units"].as<vector<wstring>>();

    for (wstring unitname: units) {
        SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << L"Failed to get unit file state for " << unitname.c_str() << L": No such file or directory";
            SystemCtlLog::Error();
            exit(1);
        }
        if (unit->IsEnabled() ) {
            wcout << L"enabled" << std::endl;
            exit(0);
        }
        else {
            wcout << L"false" << std::endl;
            exit(1);
        }
    }
    return 0;
}


int SystemCtrl_Cmd_Mask( boost::program_options::variables_map &vm )
{
    wstring usage = L"usage: Systemctl mask <unitname>\n";

    if (vm["system_units"].empty()) {
        // Complain and exit
        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
        exit(1);
    }
    vector<wstring> units = vm["system_units"].as<vector<wstring>>();
    for (wstring unitname: units) {
        if (unitname.rfind(L".service") == string::npos &&
            unitname.rfind(L".target")  == string::npos &&
            unitname.rfind(L".timer")   == string::npos &&
            unitname.rfind(L".socket")  == string::npos ) {
            unitname.append(L".service");
        }

        SystemCtlLog::msg << L"Mask unit " << unitname << std::endl;
        SystemCtlLog::Info();

        class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << L"Failed to enable unit: Unit file " << unitname.c_str() << L" does not exist\n" << usage.c_str();
            SystemCtlLog::Error();
            exit(1);
        }
        unit->Mask(false); // We will add non-blocking later
    }
    return 0;
}


int SystemCtrl_Cmd_Unmask( boost::program_options::variables_map &vm )
{
    wstring usage = L"usage: Systemctl unmask <unitname>\n";

    if (vm["system_units"].empty()) {
        // Complain and exit
        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
        exit(1);
    }

    vector<wstring> units = vm["system_units"].as<vector<wstring>>();
    for (wstring unitname: units) {
        if (unitname.rfind(L".service") == string::npos) {
            unitname.append(L".service");
        }
    
        class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << L"Failed to unmask unit: Unit file " << unitname.c_str() << L"does not exist\n" << usage.c_str();
            SystemCtlLog::Error();
            exit(1);
        }
        unit->Unmask(false); // We will add non-blocking later
    }
    return 0;
}


int SystemCtrl_Cmd_Link( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Revert( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Add_Wants( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Add_Requires( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Edit( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Get_Default( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Set_Default( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_List_Machines( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_List_Jobs( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Cancel( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Show_Environment( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Set_Environment( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Unset_Environment( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Import_Environment( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Daemon_Reload( boost::program_options::variables_map &vm )
{
    g_pool->ReloadPool( );
    return 0;
}


int SystemCtrl_Cmd_Daemon_Reexec( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Is_System_Running( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Default( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Rescue( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Emergency( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Halt( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Poweroff( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Reboot( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Kexec( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Exit( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Switch_Root( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Suspend( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Hibernate( boost::program_options::variables_map &vm )
{
    return -1;
}


int SystemCtrl_Cmd_Hybrid_Sleep( boost::program_options::variables_map &vm )
{
    return -1;
}



typedef int (*cmdfunc)( boost::program_options::variables_map &vm );

std::map< std::wstring , cmdfunc> Command_Map =  {
        { L"list-units", SystemCtrl_Cmd_List_Units},
        { L"list-sockets", SystemCtrl_Cmd_List_Sockets},
        { L"list-timers", SystemCtrl_Cmd_List_Timers},
        { L"start", SystemCtrl_Cmd_Start},
        { L"stop", SystemCtrl_Cmd_Stop},
        { L"reload", SystemCtrl_Cmd_Reload},
        { L"restart", SystemCtrl_Cmd_Restart},
        { L"try-restart", SystemCtrl_Cmd_Try_Restart},
        { L"reload-or-restart", SystemCtrl_Cmd_Reload_Or_Restart},
        { L"try-reload-or-restart", SystemCtrl_Cmd_Try_Reload_Or_Restart},
        { L"isolate", SystemCtrl_Cmd_Isolate},
        { L"kill", SystemCtrl_Cmd_Kill},
        { L"is-active", SystemCtrl_Cmd_Is_Active},
        { L"is-failed", SystemCtrl_Cmd_Is_Failed},
        { L"status", SystemCtrl_Cmd_Status},
        { L"show", SystemCtrl_Cmd_Show},
        { L"cat", SystemCtrl_Cmd_Cat},
        { L"set-property", SystemCtrl_Cmd_Set_Property},
        { L"help", SystemCtrl_Cmd_Help},
        { L"reset-failed", SystemCtrl_Cmd_Reset_Failed},
        { L"list-dependencies", SystemCtrl_Cmd_List_Dependencies},
        { L"list-unit-files", SystemCtrl_Cmd_List_Unit_Files},
        { L"enable", SystemCtrl_Cmd_Enable},
        { L"disable", SystemCtrl_Cmd_Disable},
        { L"reenable", SystemCtrl_Cmd_Reenable},
        { L"preset", SystemCtrl_Cmd_Preset},
        { L"preset-all", SystemCtrl_Cmd_Preset_All},
        { L"is-enabled", SystemCtrl_Cmd_Is_Enabled},
        { L"mask", SystemCtrl_Cmd_Mask},
        { L"unmask", SystemCtrl_Cmd_Unmask},
        { L"link", SystemCtrl_Cmd_Link},
        { L"revert", SystemCtrl_Cmd_Revert},
        { L"add-wants", SystemCtrl_Cmd_Add_Wants},
        { L"add-requires", SystemCtrl_Cmd_Add_Requires},
        { L"edit", SystemCtrl_Cmd_Edit},
        { L"get-default", SystemCtrl_Cmd_Get_Default},
        { L"set-default", SystemCtrl_Cmd_Set_Default},
        { L"list-machines", SystemCtrl_Cmd_List_Machines},
        { L"list-jobs", SystemCtrl_Cmd_List_Jobs},
        { L"cancel", SystemCtrl_Cmd_Cancel},
        { L"show-environment", SystemCtrl_Cmd_Show_Environment},
        { L"set-environment", SystemCtrl_Cmd_Set_Environment},
        { L"unset-environment", SystemCtrl_Cmd_Unset_Environment},
        { L"import-environment", SystemCtrl_Cmd_Import_Environment},
        { L"daemon-reload", SystemCtrl_Cmd_Daemon_Reload},
        { L"daemon-reexec", SystemCtrl_Cmd_Daemon_Reexec},
        { L"is-system-running", SystemCtrl_Cmd_Is_System_Running},
        { L"default", SystemCtrl_Cmd_Default},
        { L"rescue", SystemCtrl_Cmd_Rescue},
        { L"emergency", SystemCtrl_Cmd_Emergency},
        { L"halt", SystemCtrl_Cmd_Halt},
        { L"poweroff", SystemCtrl_Cmd_Poweroff},
        { L"reboot", SystemCtrl_Cmd_Reboot},
        { L"kexec", SystemCtrl_Cmd_Kexec},
        { L"exit", SystemCtrl_Cmd_Exit},
        { L"switch-root", SystemCtrl_Cmd_Switch_Root},
        { L"suspend", SystemCtrl_Cmd_Suspend},
        { L"hibernate", SystemCtrl_Cmd_Hibernate},
        { L"hybrid-sleep", SystemCtrl_Cmd_Hybrid_Sleep}
};



void ParseArgs(int argc, wchar_t *argv[])

{
    int result = 0;
    boost::program_options::positional_options_description pd;
    pd.add("command", 1);
    pd.add("system_units", -1);

    boost::program_options::options_description desc{ "Options" };
    desc.add_options()
        ("command", boost::program_options::wvalue<wstring>(), "command to execute")
        ("systemd-execpath", boost::program_options::wvalue<wstring>(), "location of service wrapper")
        ("system_units", boost::program_options::wvalue<vector<wstring>>(), "system_units")
        ("help,t", "Show this help")
        ("version", "Show package version")
        ("system", "Connect to system manager")
        ("user", "Connect to user service manager")
        ("host,H", boost::program_options::wvalue<wstring>(), "Operate on remote host")
        ("machine,M", boost::program_options::wvalue<wstring>(), "Operate on local container")
        ("type,t", boost::program_options::wvalue<wstring>(), "List units of a particular type")
        ("state", boost::program_options::wvalue<wstring>(), "List units with particular LOAD or SUB or ACTIVE state")
        ("property,p", boost::program_options::wvalue<wstring>(), "Show only properties by this name")
        ("all,a", "Show all properties/all units currently in memory, including dead/empty ones. "\
            "To list all units installed on the system, use the 'list-unit-files' command instead.")
            ("failed", "Same as --state=failed")
        ("full,l", "Don't ellipsize unit names on output")
        ("recursive,r", "Show unit list of host and local containers")
        ("reverse", "Show reverse dependencies with 'list-dependencies'")
        ("job-mode", boost::program_options::wvalue<wstring>(), "Specify how to deal with already queued jobs, when queueing a new job")
        ("show-types", "When showing sockets, explicitly show their type")
        ("value", "When showing properties, only print the value")
        ("ignore-inhibitors,i", "When shutting down or sleeping, ignore inhibitors")
        ("kill-who", boost::program_options::wvalue<wstring>(), "Who to send signal to")
        ("signal,s", boost::program_options::wvalue<wstring>(), "Which signal to send")
        ("now", "Start or stop unit in addition to enabling or disabling it")
        ("quiet", "Suppress output")
        ("wait", "For (re)start, wait until service stopped again")
        ("no-block", "Do not wait until operation finished")
        ("no-wall", "Don't send wall message before halt/power-off/reboot")
        ("no-reload", "Don't reload daemon after en-/dis-abling unit files")
        ("no-legend", "Do not print a legend (column headers and hints)")
        ("no-pager", "Do not pipe output into a pager")
        ("no-ask-password", "Do not ask for system passwords")
        ("global", "Enable/disable unit files globally")
        ("runtime", "Enable unit files only temporarily until next reboot")
        ("force,f", "When enabling unit files, override existing symlinks When shutting down, execute action immediately")
        ("preset-mode", boost::program_options::wvalue<wstring>(), "Apply only enable, only disable, or all presets")
        ("root", "Enable unit files in the specified root directory")
        ("lines,n", boost::program_options::wvalue<int>(), "Number of journal entries to show")
        ("output", boost::program_options::wvalue<wstring>(), "Change journal output mode (short, short-precise, short-iso, short-iso-precise, "\
            "short-full, short-monotonic, short-unix, verbose, export, json, json-pretty, json-sse, cat)")
            ("firmware-setup", "Tell the firmware to show the setup menu on next boot")
        ("plain", "Print unit dependencies as a list instead of a tree");

    boost::program_options::variables_map vm;
    auto parsed = boost::program_options::wcommand_line_parser(argc, argv)
        .options(desc).positional(pd).allow_unregistered().run();
    boost::program_options::store(parsed, vm);
    auto additionalArgs = collect_unrecognized(parsed.options, boost::program_options::include_positional);
    boost::program_options::notify(vm);

    vector<wstring> system_units;
    if (vm.count("system_units")) {
        system_units = vm["system_units"].as<vector<wstring>>();
    }
    wstring cmd;

    if (vm.count("command")) {
        cmd = vm["command"].as<wstring>();
    }
    else {
        cmd = L"help";
    }

    if (cmd.compare(L"help") == 0) {

        PrintMasterUsage(argv[0], desc );
        exit(0);
    }

    if (vm.count("systemd-execpath")) {
         SystemDUnitPool::SERVICE_WRAPPER_PATH = vm["systemd-execpath"].as<wstring>();
    }
    else {
        // show usage message
    }

    cmdfunc func = Command_Map[cmd];
    if (!func) {
        std::wstringstream msg;
        msg << "Invalid command " << cmd;
        std::wstring ws = msg.str();
        std::string msgstr = std::string(ws.begin(), ws.end());
        PrintMasterUsage(argv[0], desc);

        throw std::exception(msgstr.c_str());
    }
    else {
        result = (*func)(vm);

        if (result) {
            std::wstringstream msg;
            if (result == NOT_IMPLEMENTED) {
                 msg << L"the command " << cmd << " has not been implemented" ;
            }
            else {
                 msg << "the command " << cmd << " failed. code = " << result;
            }
            std::wstring ws = msg.str();
            std::string msgstr = std::string(ws.begin(), ws.end());

            throw std::exception(msgstr.c_str());
        }
    }
}
    


int wmain(int argc, wchar_t *argv[])
{ 
    try {
        g_pool->LoadPool();
        ParseArgs(argc, argv);
    }
    catch(std::exception &e) {
        SystemCtlLog::msg << e.what();
            SystemCtlLog::Error();
        exit(1);
    }
    exit(0);
}

