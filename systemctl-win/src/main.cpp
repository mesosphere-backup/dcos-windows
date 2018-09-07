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
#if VC_SUPPORTS_STD_FILESYSTEM
#include <filesystem> Not yet supported in vc
#endif
#include "windows.h"
#include "service_unit.h"
#include <map>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/join.hpp>

char usage[] = "usage: "\
               "  systemctl start <servicename>\n" \
               "  systemctl start <servicename>\n";

using namespace std;

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
   wstring usage = L"usage: Systemctl start <unitname>[ unitname .. unitname]";

    vector<wstring> units = vm["system_units"].as<vector<wstring>>();
    if (units.empty()) {
        // Complain and exit

        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
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
            SystemCtlLog::msg << L"Failed to enable unit: Unit file " << unitname.c_str() << L"does not exist"; SystemCtlLog::Error();
            SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
            exit(1);
        }
        unit->StartService(false); // We will add non-blocking later
    }

    return 0;
}


int SystemCtrl_Cmd_Stop( boost::program_options::variables_map &vm )
{
   wstring usage = L"usage: Systemctl stop <unitname>\n";

    vector<wstring> units = vm["system_units"].as<vector<wstring>>();
    if (units.empty()) {
        // Complain and exit
        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
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
            SystemCtlLog::msg << "Failed to stop service: Unit file " << unitname.c_str() << "does not exist\n";
            SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
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
    wstring usage = L"usage: Systemctl kill unitname1 [unitname2 ...]\n";
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

    vector<wstring> units = vm["system_units"].as<vector<wstring>>();
    if (units.empty()) {
        // Complain and exit
        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
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
            SystemCtlLog::msg << L"Failed to find unit to kill: Unit file " << unitname.c_str() << L"does not exist" << std::endl;
            SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
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
   wstring usage = L"usage: Systemctl show [ <unitname> ]\n";

   if (vm["system_units"].empty()) {
        g_pool->ShowGlobal();
        return 0;
   }
   vector<wstring> units = vm["system_units"].as<vector<wstring>>();
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
            SystemCtlLog::msg << L"Failed to show service: Unit file " << unitname.c_str() << L"does not exist"; SystemCtlLog::Error();
            SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
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
    wstring usage = L"usage: Systemctl enable <unitname>\n";

    if (vm["system_units"].empty()) {
        // Complain and exit
        SystemCtlLog::msg << L"No unit specified"; SystemCtlLog::Error();
        SystemCtlLog::msg << usage.c_str(); SystemCtlLog::Error();
        exit(1);
    }

    vector<wstring> units = vm["system_units"].as<vector<wstring>>();
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
    wstring usage = L"usage: Systemctl disable <unitname>\n";

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
            SystemCtlLog::msg << L"Failed to enable unit: Unit file " << unitname.c_str() << L"does not exist";
            SystemCtlLog::Error();
            SystemCtlLog::msg << usage.c_str();
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
            SystemCtlLog::msg << L"Failed to enable unit: Unit file " << unitname.c_str() << L"does not exist\n" << usage.c_str();
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
        // show usage message
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

        throw std::exception(msgstr.c_str());
    }
    else {
        result = (*func)(vm);

        if (result) {
            std::wstringstream msg;
            msg << "the command " << cmd << " failed. code = " << result;
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


