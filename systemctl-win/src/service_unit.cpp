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
#include "windows.h"
#include <vector>
#include <ios>
#include <limits>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <assert.h>
#include <tlhelp32.h>
#include "service_unit.h"

using namespace std;

std::wstring ServiceTypeToWString[] = {
       L"undefined",
       L"simple",
       L"forking",
       L"oneshot",
       L"dbus",
       L"notify",
       L"idle"
    };   

std::wstring OutputTypeToWString[] = {
       L"invalid",
       L"inherit",
       L"null",
       L"tty",
       L"journal",
       L"syslog",
       L"kmsg",
       L"journal+console",
       L"syslog+console",
       L"kmsg+console",
       L"file",
       L"socket",
       L"fd"
    };   

std::wstring RestartActionToWString[] = {
         L"undefined",
         L"no",
         L"always",
         L"on_success",
         L"on_failure",
         L"on_abnormal",
         L"on_abort",
         L"on_watchdog"
    };

std::wstring NotifyActionToWString[] = {
          L"none",
          L"main",
          L"exec",
          L"all"
    };


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


// We need to be able to parse:
// 1min3sec
// 1min 3sec
// 1 min 3sec
// 1 min 3 sec
// 1 min 3.0 sec
// So, we have 2 types of token, one a string of digits, '-', or '.'
// the other a string of characters, which have to match a set of constants

boolean 
SystemDUnit::ParseDuration(std::wstring str, double &millis)

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

    } while(ptok < plimit);

    return true;
}

wchar_t BUFFER[MAX_BUFFER_SIZE] = { '\0' };

static class SystemDUnitPool the_pool;
class SystemDUnitPool *g_pool = &the_pool;

std::map<std::wstring, class SystemDUnit *> SystemDUnitPool::pool;

class SystemDUnit *
SystemDUnitPool::FindUnit(std::wstring name)

{   class SystemDUnit *&punit = pool[name];
    return punit;
}

boolean 
SystemDUnitPool::LinkWantedUnit(wstring file_path, wstring servicename)

{ 
    wstring link_name      = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH + L"\\" + file_path + L"\\" + servicename;
    wstring orig_file_name = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH + L"\\" + servicename;

    if (!::CreateHardLinkW(link_name.c_str(), orig_file_name.c_str(), NULL)) {
        DWORD last_err = GetLastError();
        SystemCtlLog::msg << L"could not link file " << link_name << L" to " << last_err;
    SystemCtlLog::Error();
    }

    return true;
}

boolean 
SystemDUnitPool::CopyUnitFileToActive(wstring servicename)

{
    // Even if we aren't running now, we could have a unit in the active directory
    // If so, we just need to register ourseleves with the service manager
    wifstream checkfs(SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename);
    if (checkfs.is_open()) {
        checkfs.close();
    }
    else {
              
        // If there is no file in the active directory, we copy one over, then register.

        wstring service_unit_path = SystemDUnitPool::UNIT_DIRECTORY_PATH+L"\\"+servicename;
    
        // Find the unit in the unit library
        wifstream fs(service_unit_path, std::fstream::in | std::fstream::binary);
        if (!fs.is_open()) {
             SystemCtlLog::msg << L"No service unit " << servicename.c_str() << L"Found in unit library";
             SystemCtlLog::Error();
             return false;
        }
        fs.seekg (0, fs.end);
        int length = fs.tellg();
        SystemCtlLog::msg << L"length " << length;
        SystemCtlLog::Debug();

        fs.seekg (0, fs.beg);
        wchar_t *buffer = new wchar_t [length];
        fs.read (buffer, length);
        fs.close();

        wofstream ofs(SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename, std::fstream::out | std::fstream::binary);
        ofs.write (buffer,length);
        ofs.close();    
    }

    return true;
}



class SystemDUnit *
SystemDUnitPool::ReadServiceUnit(std::wstring name, std::wstring service_unit_path) {
     wstring servicename = name;
     class SystemDUnit *punit = NULL;

     // Timers are read elsewhere

     if (name.rfind(L".timer") != string::npos) {
         return ReadTimerUnit(name, service_unit_path);
     }

     // Find the unit in the unit directory
     wifstream fs;
     try {
         fs.open(service_unit_path, std::fstream::in);
     }
     catch (exception e) {
         SystemCtlLog::msg << e.what();
         SystemCtlLog::Error();
     }
     if (fs.is_open()) {
         // wstring justname = servicename.substr(0, servicename.find_last_of('.'));
         punit = SystemDUnit::ParseSystemDServiceUnit(servicename, service_unit_path, fs);
     }
     else {
         SystemCtlLog::msg << L"No service unit " << servicename.c_str() << L"Found in unit library";
         SystemCtlLog::Error();
     }
     fs.close();

     return punit;
}

class SystemDUnit *SystemDUnit::ParseSystemDServiceUnit(wstring servicename, wstring unit_path, wifstream &fs)
{ 
    SystemCtlLog::msg << L"ParseSystemDServiceUnit unit = " << servicename;
    SystemCtlLog::Debug();

    std::wstring line;
    class SystemDUnit *punit = new class SystemDUnit((wchar_t*)servicename.c_str(), unit_path.c_str());
 
    (void)fs.getline(BUFFER, MAX_BUFFER_SIZE);
    line = BUFFER;
    while (true) {
        
        if (fs.eof()) {
            break;
        }
     
        line.erase(
            std::remove_if(line.begin(), line.end(), 
                    [](wchar_t c) -> bool
                    { 
                        return std::isspace(c); 
                    }), 
                line.end());
        
        if (line[0] == ';' || line[0] == '#' ) {
             // Comment
            (void)fs.getline(BUFFER, MAX_BUFFER_SIZE);
            continue;
        }
        else if (line.compare(L"[Unit]") == 0) {
             // Then we need to parse the unit section
             SystemCtlLog::msg << L"parse unit section";
             SystemCtlLog::Debug();

             line = punit->ParseUnitSection(fs);
        }
        else if (line.compare(L"[Service]") == 0) {
             // Then we need to parse the service section
             SystemCtlLog::msg << L"parse service section";
             SystemCtlLog::Debug();

             line = punit->ParseServiceSection(fs);
        }
        else if (line.compare(L"[Install]") == 0) {
             // Then we need to parse the install section
             SystemCtlLog::msg << L"parse install section";
             SystemCtlLog::Debug();

             line = punit->ParseInstallSection(fs);
        }
        else if (line.compare(L"[Timer]") == 0) {
             // Then we need to parse the unit section
             SystemCtlLog::msg << L"parse timer section";
             SystemCtlLog::Debug();

             line = punit->ParseTimerSection(fs);
        }
        else {
            if (line.length() == 0) {
                break;
            }
            SystemCtlLog::msg << L"Invalid section heading " << line.c_str();
            SystemCtlLog::Warning();
        }
    }
   
    return punit;
}
 

// When it returns this will be located at EOF or the next section ([Service] for example).

static inline wstring split_elems(wifstream &fs, vector<wstring> &attrs, vector<wstring> &values)

{
    wstring attrname;
    wstring attrval;
    wstring line;

    while (true) {
        try { 
            if (fs.eof()) {
                break;
            }
            (void) fs.getline(BUFFER, MAX_BUFFER_SIZE);
        }
         catch (const std::exception &e) {
            SystemCtlLog::msg << e.what();
        SystemCtlLog::Debug();
            break;
        }

        line = BUFFER;
        int first_non_space = line.find_first_not_of(L" \t\r\n");
        if (first_non_space < 0) {
            // blank line
            continue;
        }
        if (line[first_non_space] == '#' || line[first_non_space] == ';') {
            // This is a comment line. Treat it as empty
            continue;
        }

        // 2do: continuations via '\' at the end of the line.

        int split_pt  = line.find_first_of(L"=");
        if (split_pt == std::string::npos) {
            if (first_non_space == std::string::npos) {
                continue; // a blank line or eof
             }
             // If this line is part of the next section, we put it back
             if (line[first_non_space] == '[') {
                // fs.seekg(prev_pos, std::ios_base::beg);
             }
             break;
        }

        attrname = line.substr(first_non_space, split_pt);

        int last_non_space = line.find_last_not_of(L" \t\r\n");
        if (last_non_space == std::string::npos) {
            last_non_space = line.length();
        }

        attrval  = line.substr(split_pt+1, last_non_space );
        attrs.push_back(wstring(attrname));
        values.push_back(wstring(attrval));
    }
    return line;
}


static inline enum SystemDUnit::OUTPUT_TYPE String_To_OutputType(const wchar_t *str)

{
    wstring val = str;

    if (val.compare(L"inherit") == 0) {
        return SystemDUnit::OUTPUT_TYPE_INHERIT;
    }
    else if (val.compare(L"null") == 0) {
        return SystemDUnit::OUTPUT_TYPE_NULL;
    }
    else if (val.compare(L"tty") == 0) {
        return SystemDUnit::OUTPUT_TYPE_TTY;
    }
    else if (val.compare(L"journal") == 0) {
        return SystemDUnit::OUTPUT_TYPE_JOURNAL;
    }
    else if (val.compare(L"syslog") == 0) {
        return SystemDUnit::OUTPUT_TYPE_SYSLOG;
    }
    else if (val.compare(L"kmsg") == 0) {
        return SystemDUnit::OUTPUT_TYPE_KMSG;
    }
    else if (val.compare(L"journal+console") == 0) {
        return SystemDUnit::OUTPUT_TYPE_JOURNAL_PLUS_CONSOLE;
    }
    else if (val.compare(L"syslog+console") == 0) {
        return SystemDUnit::OUTPUT_TYPE_SYSLOG_PLUS_CONSOLE;
    }
    else if (val.compare(L"kmsg+console") == 0) {
        return SystemDUnit::OUTPUT_TYPE_KMSG_PLUS_CONSOLE;
    }
    else if (val.compare(0, 5, L"file:") == 0) {
        return SystemDUnit::OUTPUT_TYPE_FILE;
    }
    else if (val.compare(L"socket") == 0) {
        return SystemDUnit::OUTPUT_TYPE_SOCKET;
    }
    else if (val.compare(0, 3, L"fd:") == 0) {
        return SystemDUnit::OUTPUT_TYPE_FD;
    }
    else {
        return SystemDUnit::OUTPUT_TYPE_INVALID;
    }
}

wstring SystemDUnit::ParseUnitSection( wifstream &fs)

{ 
    vector<wstring> attrs;
    vector<wstring> values;

    wstring retval = split_elems(fs, attrs, values);
   
    for (auto i = 0; i < attrs.size(); i++) {
        if (attrs[i].compare(L"Description") == 0) {
            SystemCtlLog::msg << L"Description = " << values[i].c_str();
        SystemCtlLog::Debug();

            this->description = values[i].c_str(); 
        }
        else if (attrs[i].compare(L"Documentation") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"Requires") == 0) {
            SystemCtlLog::msg << L"Requires " << values[i].c_str();
        SystemCtlLog::Debug();

            wstring value_list = values[i]; 
            int end = 0;
            for (auto start = 0; end != std::string::npos; start = end+1) {
                end = value_list.find_first_of(' ', start);
                this->requires.push_back(value_list.substr(start, end));
            }
        }
        else if (attrs[i].compare(L"Requisite") == 0) {
            SystemCtlLog::msg << L"Requisite " << values[i].c_str();
        SystemCtlLog::Debug();

            wstring value_list = values[i];
            int end = 0;
            for (auto start = 0; end != std::string::npos; start = end+1) {
                end = value_list.find_first_of(' ', start);
                this->requisite.push_back(value_list.substr(start, end));
            }
        }
        else if (attrs[i].compare(L"Wants") == 0) {
            SystemCtlLog::msg << L"Wants " << values[i].c_str();
        SystemCtlLog::Debug();

            wstring value_list = values[i];
            int end = 0;
            for (auto start = 0; end != std::string::npos; start = end+1) {
                end = value_list.find_first_of(' ', start);
                this->wants.push_back(value_list.substr(start, end));
            }
        }
        else if (attrs[i].compare(L"BindsTo") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"PartOf") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"Conflicts") == 0) {
            SystemCtlLog::msg << L"Conflicts " << values[i].c_str();
        SystemCtlLog::Debug();

            wstring value_list = values[i];
            int end = 0;
            for (auto start = 0; end != std::string::npos; start = end+1) {
                end = value_list.find_first_of(' ', start);
                this->conflicts.push_back(value_list.substr(start, end));
            }
        }
        else if (attrs[i].compare(L"Before") == 0) {
            SystemCtlLog::msg << L"Before " << values[i].c_str();
        SystemCtlLog::Debug();

            wstring value_list = values[i];
            int end = 0;
            for (auto start = 0; end != std::string::npos; start = end+1) {
                end = value_list.find_first_of(' ', start);
                this->before.push_back(value_list.substr(start, end));
            }
        }
        else if (attrs[i].compare(L"After") == 0) {
            SystemCtlLog::msg << L"After " << values[i].c_str();
        SystemCtlLog::Debug();

            wstring value_list = values[i];
            int end = 0;
            for (auto start = 0; end != std::string::npos; start = end+1) {
                end = value_list.find_first_of(' ', start);
                this->after.push_back(value_list.substr(start, end));
            }
        }
        else if (attrs[i].compare(L"OnFailure") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"PropagatesReloadTo") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ReloadPropagatedFrom") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"JoinsNamespaceOf") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"RequiresMountsFor") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"OnFailureJobMode") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"IgnoreOnIsolate") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"StopWhenUnneeded") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"RefuseManualStart") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"RefuseManualStop") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AllowIsolate") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"DefaultDependencies") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"CollectMode") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"JobTimeoutSec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"JobRunningTimeoutSec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"JobTimeoutAction") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"JobTimeoutRebootArgument") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"FailureAction") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"SuccessAction") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"RebootArgument") == 0) {
            SystemCtlLog::msg <<  L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionArchitecture") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionVirtualization") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionHost") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionKernelCommandLine") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionKernelVersion") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionSecurity") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionCapability") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionACPower") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionNeedsUpdate") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionFirstBoot") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionPathExists") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionPathExistsGlob") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionPathIsDirectory") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionPathIsSymbolicLink") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionPathIsMountPoint") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionPathIsReadWrite") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionDirectoryNotEmpty") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionFileNotEmpty") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionFileIsExecutable") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionUser") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionGroup") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"ConditionControlGroupController") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertArchitecture") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertVirtualization") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertHost") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertKernelCommandLine") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertKernelVersion") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertSecurity") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertCapability") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertACPower") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertNeedsUpdate") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertFirstBoot") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertPathExists") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertPathExistsGlob") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertPathIsDirectory") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertPathIsSymbolicLink") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertPathIsMountPoint") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertPathIsReadWrite") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertDirectoryNotEmpty") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertFileNotEmpty") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertFileIsExecutable") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertUser") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertGroup") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
        SystemCtlLog::Debug();
        }
        else if (attrs[i].compare(L"AssertControlGroupController") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
        SystemCtlLog::Debug();
        }
        else {
            SystemCtlLog::msg << L"attribute not recognised: " << attrs[i].c_str();
        SystemCtlLog::Debug();
        }
    }
    
    return retval;
}

wstring SystemDUnit::ParseServiceSection( wifstream &fs)

{   
    unsigned long attr_bitmask = 0;
    vector<wstring> attrs;
    vector<wstring> values;

    enum ServiceType attr_type = SERVICE_TYPE_UNDEFINED;
    wstring  attr_pidfile  = L"";
    wstring  attr_busname  = L"";
    vector<wstring> attr_execstart;

    wstring retval = split_elems(fs, attrs, values);
    systemd_service_attr_func attr_method;

    for (auto i = 0; i < attrs.size(); i++) {
        attr_method = SystemD_Service_Attribute_Map[attrs[i]];
        if (attr_method) {
            (this->*attr_method)(attrs[i], values[i], attr_bitmask);
        }
        else {
            SystemCtlLog::msg << L"attribute not recognised: " << attrs[i].c_str();
            SystemCtlLog::Warning();
        }
    }

    // if type is not set, then we have default value combinatiosn that
    // produce a type value. We behvae after as if they had been specified that way

    attr_type = this->service_type;
    if (! (attr_bitmask & ATTRIBUTE_BIT_TYPE)) {
        if ((attr_bitmask & ATTRIBUTE_BIT_EXEC_START) ) {
            if (! (attr_bitmask & ATTRIBUTE_BIT_BUS_NAME) ) {
                attr_type = SERVICE_TYPE_SIMPLE;
                attr_bitmask |= ATTRIBUTE_BIT_TYPE;
            }
            else {
                attr_type = SERVICE_TYPE_DBUS;
                attr_bitmask |= ATTRIBUTE_BIT_TYPE;
            }
        }
        else {
            attr_type = SERVICE_TYPE_ONESHOT;
            attr_bitmask |= ATTRIBUTE_BIT_TYPE;
        }
    }

    // Validate the combinations of definitions


    // Set the values if value
    this->service_type = attr_type;

    return retval;
}

wstring SystemDUnit::ParseTimerSection( wifstream &fs)

{ 
    vector<wstring> attrs;
    vector<wstring> values;

    wstring retval = split_elems(fs, attrs, values);
   
    for (auto i = 0; i < attrs.size(); i++) {

        if (attrs[i].compare( L"OnActiveSec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"OnBootSec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"OnStartupSec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"OnUnitActiveSec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"OnUnitInactiveSec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"AccuracySec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"RandomizedDelaySec") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"Unit") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"Persistent") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"WakeSystem") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else if (attrs[i].compare( L"RemainAfterElapse") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str() ;
            SystemCtlLog::Debug();
        }
        else {
            SystemCtlLog::msg << L"attribute not recognised: " << attrs[i].c_str();
            SystemCtlLog::Debug();
        }
    }
    
    return retval;
}

wstring SystemDUnit::ParseInstallSection( wifstream &fs)

{
    vector<wstring> attrs;
    vector<wstring> values;

    wstring retval = split_elems(fs, attrs, values);
    for (auto i = 0; i < attrs.size(); i++) {
        if (attrs[i].compare(L"Alias") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
            SystemCtlLog::Verbose();
        }
        else if (attrs[i].compare(L"WantedBy") == 0) {
            SystemCtlLog::msg << L"WantedBy " << values[i].c_str();
            SystemCtlLog::Verbose();

            wstring value_list = values[i];
            int end = 0;
            for (auto start = 0; start != std::string::npos; start = end) {
                end = value_list.find_first_of(' ', start);
                if (end != string::npos){
                    this->wanted_by.push_back(value_list.substr(start, end));
                }
                else {
                    this->wanted_by.push_back(value_list);
                }
            }
        }
        else if (attrs[i].compare(L"RequiredBy") == 0) {
            SystemCtlLog::msg << "RequiredBy " << values[i].c_str();
            SystemCtlLog::Verbose();

            wstring value_list = values[i];
            int end = 0;
            for (auto start = 0; start != std::string::npos; start = end) {
                end = value_list.find_first_of(' ', start);
                if (end != string::npos){
                    this->required_by.push_back(value_list.substr(start, end));
                }
                else {
                    this->required_by.push_back(value_list);
                }
            }
        }
        else if (attrs[i].compare(L"Also") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
            SystemCtlLog::Verbose();
        }
        else if (attrs[i].compare(L"DefaultInstance") == 0) {
            SystemCtlLog::msg << L"2do: attrs = " << attrs[i].c_str() << L" value = " << values[i].c_str();
            SystemCtlLog::Verbose();
        }
        else {
            SystemCtlLog::msg <<  L"attribute not recognised: " << attrs[i].c_str() ;
            SystemCtlLog::Verbose();
        }
    }
    return retval;
}


boolean 
SystemDUnitPool::DirExists(wstring dir_path)

{
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW ffd;
    LARGE_INTEGER filesize;
   
    wstring dos_path = dir_path + L"\\*";
    std::replace_if(dos_path.begin(), dos_path.end(),
            [](wchar_t c) -> bool
                {
                    return c == '/';
                }, '\\');
    
    hFind = FindFirstFileW(dos_path.c_str(), &ffd);
    if (INVALID_HANDLE_VALUE == hFind) 
    {
       return false;
    } 
    FindClose(hFind);
    return true;
}




wstring 
SystemDUnitPool::FindServiceFilePath(wstring dir_path, wstring service_name)

{
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW ffd;
    LARGE_INTEGER filesize;
   
    wstring dos_path = dir_path + L"\\*";
    std::replace_if(dos_path.begin(), dos_path.end(),
            [](wchar_t c) -> bool
                {
                    return c == '/';
                }, '\\');
    
    DWORD errval = 0;
    hFind = FindFirstFileW(dos_path.c_str(), &ffd);
 
    if (INVALID_HANDLE_VALUE == hFind) 
    {
        errval = GetLastError();
        SystemCtlLog::msg << L"Could not find directory path " << dir_path.c_str() << L" error = " << errval ;
    SystemCtlLog::Warning();
 
        return L"";
    } 
    
    // List all the files in the directory with some info about them.
 
    do
    {
       if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
       {
           wstring subpath = ffd.cFileName;

           if ( (subpath.compare(L".")) != 0 && ( subpath.compare(L"..") != 0)) {
               SystemCtlLog::msg << "subdir found " << subpath.c_str() ;
           SystemCtlLog::Debug();

               subpath = dir_path + L"\\" + subpath;
               wstring file_path = FindServiceFilePath(subpath, service_name);
               if (!file_path.empty()) {
                   return file_path;
               }
               SystemCtlLog::msg << "return from dir" << subpath.c_str() ;
           SystemCtlLog::Debug();
           }
       }
       else
       {
          filesize.LowPart = ffd.nFileSizeLow;
          filesize.HighPart = ffd.nFileSizeHigh;
          SystemCtlLog::msg << L"filename " << ffd.cFileName << L" file size " << filesize.QuadPart ; ;
          SystemCtlLog::Debug();
          wstring filename = ffd.cFileName;
          if (filename.compare(service_name) == 0) {
              dir_path += L"\\";
              dir_path += filename;
              return dir_path;
          }
       }
    }
    while (FindNextFileW(hFind, &ffd) != 0);
  
    FindClose(hFind);
    return L"";
}


boolean 
SystemDUnitPool::Apply(wstring dir_path, boolean (*action)(wstring dir_path, void *context ), void *context)

{
    boolean rslt = false;

#if VC_SUPPORTS_STD_FILESYSTEM
    for ( auto & thisfile : std::filesystem::directory_iterator(SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH)) {
         std::wcout << thisfile << endl;
    }
#else
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW ffd;
    LARGE_INTEGER filesize;
   
    wstring dos_path = dir_path + L"\\*";
    std::replace_if(dos_path.begin(), dos_path.end(),
            [](wchar_t c) -> bool
                {
                    return c == '/';
                }, '\\');
    
    DWORD errval = 0;
    hFind = FindFirstFileW(dos_path.c_str(), &ffd);
 
    if (INVALID_HANDLE_VALUE == hFind) 
    {
        errval = GetLastError();
        SystemCtlLog::msg << L"Could not find directory path " << dir_path.c_str() << L" error = " << errval;
        SystemCtlLog::Warning();
 
        return false;
    } 
    
    // List all the files in the directory with some info about them.
 
    do
    {
       if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
       {
           wstring subpath = ffd.cFileName;

           if ( (subpath.compare(L".")) != 0 && ( subpath.compare(L"..") != 0)) {
               SystemCtlLog::msg << "subdir found " << subpath.c_str();
               SystemCtlLog::Debug();
               subpath = dir_path + L"\\" + subpath;
               rslt = Apply(subpath, action, context);
               SystemCtlLog::msg << "return from dir" << subpath.c_str();
               SystemCtlLog::Debug();
           }
           
       }
       else
       {
          filesize.LowPart = ffd.nFileSizeLow;
          filesize.HighPart = ffd.nFileSizeHigh;
          SystemCtlLog::msg << L"filename " << ffd.cFileName << L" file size " << filesize.QuadPart ;
          SystemCtlLog::Debug();
          if (action) {
              rslt = (*action)(dir_path+L"\\"+ffd.cFileName, context);
          }
       }
    }
    while (FindNextFileW(hFind, &ffd) != 0);
  
    FindClose(hFind);
#endif
    return rslt;
}

static boolean add_wanted_unit(wstring file_path, void *context)

{   SystemDUnit *pparent = (SystemDUnit *)context;

    wstring servicename = file_path.substr(file_path.find_last_of('\\') + 1);
    wstring file_type = file_path.substr(file_path.find_last_of('.'));
    if ((file_type.compare(L".service") == 0) ||
        (file_type.compare(L".target") == 0) ||
        (file_type.compare(L".timer") == 0) ||
        (file_type.compare(L".socket") == 0)) {
        pparent->AddWanted(servicename);
    }
    return true;
}

static boolean add_requireed_unit(wstring file_path, void *context)

{   SystemDUnit *pparent = (SystemDUnit *)context;

    wstring servicename = file_path.substr(file_path.find_last_of('\\') + 1);
    wstring file_type = file_path.substr(file_path.find_last_of('.'));
    if ((file_type.compare(L".service") == 0) ||
        (file_type.compare(L".target") == 0) ||
        (file_type.compare(L".timer") == 0) ||
        (file_type.compare(L".socket") == 0)) {
        pparent->AddRequired(servicename);
    }
    return true;
}

// Returns true if the file is read in or ignored. false if read fails
static boolean read_unit(wstring file_path, void *context)

{
    wstring servicename = file_path.substr(file_path.find_last_of('\\') + 1);
    wstring file_type = file_path.substr(file_path.find_last_of('.'));

    if (file_type.compare(L".timer") == 0) {
        //  A timer refers to a service, target, wants or requires. So we need to look at the timer info
        SystemDTimer *punit = SystemDUnitPool::ReadTimerUnit(servicename, file_path);
        if (!punit) {
            // Complain and exit
            SystemCtlLog::msg << "Failed to load timer: Unit file " << file_path.c_str() << "is invalid";
            SystemCtlLog::Error();
            return false;
        }
    }
    else if ((file_type.compare(L".service") == 0) ||
        (file_type.compare(L".target") == 0) ||
        (file_type.compare(L".socket") == 0)) {
        SystemDUnit *punit = SystemDUnitPool::ReadServiceUnit(servicename, file_path);
        if (!punit) {
            // Complain and exit
            SystemCtlLog::msg << "Failed to load unit: Unit file " << file_path.c_str() << "is invalid";
            SystemCtlLog::Error();
            return false;
        }
        // Look for wanted directory
        wstring wants_dir_path = file_path+L".wants";
        if (SystemDUnitPool::DirExists(wants_dir_path)) {
        // Add to wants
            (void)SystemDUnitPool::Apply(wants_dir_path, add_wanted_unit, (void*)punit);
        }

        // Look for required directory
        wstring requires_dir_path = file_path+L".requires";
        if (SystemDUnitPool::DirExists(requires_dir_path)) {
        // Add to requires
            (void)SystemDUnitPool::Apply(requires_dir_path, add_wanted_unit, (void*)punit);
        }
    }
    return true;
}

static boolean delete_unit(wstring file_path, void *context)

{
    wstring servicename = file_path.substr(file_path.find_last_of('\\') + 1);
    wstring file_type = file_path.substr(file_path.find_last_of('.'));

    if ((file_type.compare(L".service") == 0) ||
        (file_type.compare(L".target") == 0) ||
        (file_type.compare(L".timer") == 0) ||
        (file_type.compare(L".socket") == 0)) {
    // Delete the service
        class SystemDUnit *punit = SystemDUnitPool::FindUnit(servicename);
    if (punit) {
        punit->Disable(true);
    }
    }
    return true;
}

static boolean
enable_required_unit(wstring file_path, void *context )

{
    SystemCtlLog::msg << L"enable required Unit " << file_path.c_str();
    SystemCtlLog::Verbose();
    return true;
}

static boolean
load_wanted_unit(wstring file_path, void *context )

{ 
    boolean unit_loaded = false;
    wstring servicename = file_path.substr(file_path.find_last_of('\\') + 1);

    SystemCtlLog::msg << L"enable wanted Unit " << servicename.c_str();
    SystemCtlLog::Verbose();

    // normalise the file path
    wstring src_path = SystemDUnitPool::UNIT_DIRECTORY_PATH + L"\\" + servicename; // We only look for the unit file on the top directory
    std::replace_if(src_path.begin(), src_path.end(),
            [](wchar_t c) -> bool
                {
                    return c == '/';
                }, '\\');
    
    unit_loaded = read_unit(src_path, context);
    class  SystemDUnit *punit = NULL;
    if (unit_loaded) {
        punit = SystemDUnitPool::FindUnit(servicename);
        if (!punit) {
            SystemCtlLog::msg << L"could not find unit " << servicename;
            SystemCtlLog::Warning();
            return false;
        }
    }
    else {
        SystemCtlLog::msg << L"could not find unit " << servicename;
        SystemCtlLog::Warning();
        return false;
    }

    assert(punit != NULL);
    class SystemDUnit *parent_unit = (class SystemDUnit *)context;
    assert(parent_unit != NULL);

    if (!SystemDUnitPool::CopyUnitFileToActive(servicename)) {
        SystemCtlLog::msg << L"Could not copy to active service unit";
        SystemCtlLog::Warning();
        return false;
    }

    // Create the link in the active version of the wanted dir to the unit file copy in the active dir
    // Note we had to ensure the unit file was enabled first
    SystemDUnitPool::LinkWantedUnit(parent_unit->Name() + L".wants", servicename);
    parent_unit->AddWanted(punit->Name());

    return true;
}

boolean 
SystemDUnit::Enable(boolean block)

{
    wchar_t * buffer;
    wstring servicename = this->name;

    // Is the active dir there? If not, create it.
    wstring active_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH;
    if (!SystemDUnitPool::DirExists(active_dir_path)) {
         if (!CreateDirectoryW(active_dir_path.c_str(), NULL)) {  // 2do: security attributes
             SystemCtlLog::msg << L"Could not create active directory";
             SystemCtlLog::Error();
         }
    }

    if (!SystemDUnitPool::CopyUnitFileToActive(servicename)) {
        SystemCtlLog::msg << L"Could not copy activated service unit";
        SystemCtlLog::Error();
        return false;
    }

    // Is there a requires directory?
    wstring requires_dir_path = SystemDUnitPool::UNIT_DIRECTORY_PATH+this->unit_file_path+L".requires";
    if (SystemDUnitPool::DirExists(requires_dir_path)) {
        wstring active_requires_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+this->unit_file_path+L".requires";

        (void)CreateDirectoryW(active_requires_dir_path.c_str(), NULL);
        // Enable all of the units and add to the requires list
        
        (void)SystemDUnitPool::Apply(requires_dir_path, enable_required_unit, (void*)this);
    }

    // Is there a wants directory?
    wstring wants_dir_path = SystemDUnitPool::UNIT_DIRECTORY_PATH+this->unit_file_path+L".wants";
    if (SystemDUnitPool::DirExists(wants_dir_path)) {
        wstring active_wants_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+this->unit_file_path+L".wants";
        (void)CreateDirectoryW(active_wants_dir_path.c_str(), NULL);

        // Enable all of the units and add to the wants list
        (void)SystemDUnitPool::Apply(wants_dir_path, load_wanted_unit, (void*)this);
    }

    // Enable all of the units and add to the requires list
    for(auto other_service : this->GetRequires()) {

        SystemCtlLog::msg << L"required service = " << other_service;
        SystemCtlLog::Debug();

        class SystemDUnit *pother_unit = g_pool->GetPool()[other_service];
        if (!pother_unit) {
            wstring file_path = SystemDUnitPool::UNIT_DIRECTORY_PATH+L"\\"+other_service;
            pother_unit = SystemDUnitPool::ReadServiceUnit(other_service, file_path);
            if (pother_unit) {
                if (!pother_unit->Enable(true)) {
                    SystemCtlLog::msg << L"cannot enable dependency " << other_service;
                    SystemCtlLog::Error();
                    return false;
                }
            }
            else {
                SystemCtlLog::msg << L"cannot enable dependency " << other_service;
                SystemCtlLog::Error();
                return false;
            }
        }
        if (pother_unit) {
            this->AddStartDependency(pother_unit);
        }
    }

    for(auto other_service : this->GetWants()) {

        SystemCtlLog::msg << L"wanted service = " << other_service;
        SystemCtlLog::Verbose();

        class SystemDUnit *pother_unit = g_pool->GetPool()[other_service];
        if (!pother_unit) {
            wstring file_path = SystemDUnitPool::UNIT_DIRECTORY_PATH+L"\\"+other_service;
            pother_unit = SystemDUnitPool::ReadServiceUnit(other_service, file_path);
            // This is wanted not needed. We don't fail
        }
        if (pother_unit) {
            (void)pother_unit->Enable(true);
            this->AddStartDependency(pother_unit);
        }
    }

    if (this->IsEnabled()) {
        // We don't error but we don't do anything
        SystemCtlLog::msg << L"Already enabled = ";
        SystemCtlLog::Debug();
        return true;
    }

    for( auto dependent : this->start_dependencies ) {
        SystemCtlLog::msg << L"w5 dep = " << dependent->name;
        SystemCtlLog::Debug();
    }

    this->RegisterService();
    this->is_enabled = true;
    return true;
}

static boolean
disable_required_unit(wstring file_path, void *context )

{
    SystemCtlLog::msg << L"disable required Unit is a stub " << file_path.c_str();
    SystemCtlLog::Debug();
    return true;
}

static boolean
disable_wanted_unit(wstring file_path, void *context )

{
    SystemCtlLog::msg << L"disable wanted Unit is a stub " << file_path.c_str();
    SystemCtlLog::Debug();
    return true;
}

boolean SystemDUnit::Disable(boolean block)

{
    wstring servicename = this->name;

    // Is there a requires directory?
    wstring requires_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename+L".requires";
    if (SystemDUnitPool::DirExists(requires_dir_path)) {
        // Enable all of the units and add to the requires list
        (void)SystemDUnitPool::Apply(requires_dir_path, disable_required_unit, (void*)this);
    }

    // Is there a wants directory?
    wstring wants_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename+L".wants";
    if (SystemDUnitPool::DirExists(wants_dir_path)) {
        // Enable all of the units and add to the wants list
        (void)SystemDUnitPool::Apply(wants_dir_path, disable_wanted_unit, (void*)this);
    }

    // Disable unregisters the service from the service manager, but leaves the service unit 
    // in place. The next daemon-reload will pick it up again

    this->UnregisterService();
    
    // and mark the object
    this->is_enabled = false;
    return false;
}

DWORD SystemDUnit::GetMainPID()

{
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << L"GetMainPID could not open handle to service manager";
        SystemCtlLog::Error();
        return 0;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), GENERIC_READ);
    if (!hsvc) {
        SystemCtlLog::msg << L"GetMainPID could not open handle to the service: " << this->name;
        SystemCtlLog::Error();
        CloseServiceHandle(hsc);
        return 0;
    }
    SERVICE_STATUS_PROCESS svc_status = { 0 };
    DWORD size_needed = sizeof(SERVICE_STATUS_PROCESS);
    if (!QueryServiceStatusEx(hsvc, SC_STATUS_PROCESS_INFO, (BYTE*)&svc_status, sizeof(SERVICE_STATUS_PROCESS), &size_needed)) {
        int last_error = GetLastError();
        // TODO: handle MORE_DATA
        SystemCtlLog::msg << L"GetMainPID could not open handle to the service: " << this->name;
        SystemCtlLog::Error();
        CloseServiceHandle(hsc);
        CloseServiceHandle(hsvc);
        return 0;
    }

    CloseServiceHandle(hsc);
    CloseServiceHandle(hsvc);

    SystemCtlLog::msg << L"GetMainPID service:" << this->name << " svc_status.dwProcessId: " << svc_status.dwProcessId << L" svc_status.dwCurrentState: " << svc_status.dwCurrentState;
    SystemCtlLog::Debug();
    return svc_status.dwProcessId;
}

void WINAPI SystemDUnit::KillProcessTree(DWORD dwProcId)
{
    PROCESSENTRY32 pe;
    memset(&pe, 0, sizeof(PROCESSENTRY32));
    pe.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (::Process32First(hSnap, &pe))
    {
        BOOL bContinue = TRUE;
        while (bContinue)
        {
            if (pe.th32ParentProcessID == dwProcId)
            {
                KillProcessTree(pe.th32ProcessID);
            }
            bContinue = ::Process32Next(hSnap, &pe);
        }

        HANDLE hProc = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcId);
        if (hProc)
        {
            ::TerminateProcess(hProc, ERROR_PROCESS_ABORTED);
            ::CloseHandle(hProc);
        }
    }

    ::CloseHandle(hSnap);
}

/*
 *   A complication of systemctl kill is that windows doesn't have signals.
 *   Because of that we must try to divine the purpose of the signal and expected result.
 *   We currently only recognise sigkill and sigterm. SigTerm we send a Console Control Event to 
 *   the process. SigKill we just kill the process
 */
boolean SystemDUnit::Kill(int action, int killtarget, boolean block)

{
    wstring servicename = this->name;

    // identify info to kill
    switch (action) {
    case 9: // SIGKILL - We kill the process

        // Now we need to figure out the process
        // main: we look for the PID key in the registry for the exec start
        // control we look for the PID keys of the execstartpre and or ExecStop or ExecReload
        // all we do both...
        switch(killtarget) {
        case SystemCtl::KILL_ACTION_CONTROL:
        case SystemCtl::KILL_ACTION_ALL:
        case SystemCtl::KILL_ACTION_MAIN: {
                DWORD pid = GetMainPID();
                HANDLE hProc = INVALID_HANDLE_VALUE;
            
                if (!pid) {
                    SystemCtlLog::msg << L"the process is not active, nothing to do. Operation skipped" ;
                    SystemCtlLog::Warning();
                    return true;
                }
            
                        // SIGKILL just kills the process unceremoniously. 
                hProc = ::OpenProcess(PROCESS_TERMINATE, false, pid);
                if (hProc == INVALID_HANDLE_VALUE) {
                    SystemCtlLog::msg << L"could not open process " << pid << " presumed no longer active. Operation skipped";
                    SystemCtlLog::Warning();
                    return true;
                }
                KillProcessTree(pid);
                ::TerminateProcess(hProc, ERROR_PROCESS_ABORTED);
                DWORD wait_rslt = 0;
                do {
                    wait_rslt = ::WaitForSingleObject(hProc, 120000);  
                    if  (wait_rslt == WAIT_TIMEOUT) {
                        SystemCtlLog::msg << L"terminate process " << pid << L": timeout. Retrying";
                        SystemCtlLog::Warning();
                    }
                } while (wait_rslt == WAIT_TIMEOUT);
            
                CloseHandle(hProc);
            }
            break;
        }
        break;

    case 15: // SIGTERM - We send a control-c-event
        switch(killtarget) {
        case SystemCtl::KILL_ACTION_MAIN: {
                DWORD pid = GetMainPID();
                HANDLE hProc = INVALID_HANDLE_VALUE;
            
                if (!pid) {
                    SystemCtlLog::msg << L"the process is not active, nothing to do. Operation skipped" ;
                    SystemCtlLog::Warning();
                }
            
                        // SIGKILL just kills the process unceremoniously. 
                hProc = ::OpenProcess(PROCESS_TERMINATE, false, pid);
                if (hProc == INVALID_HANDLE_VALUE) {
                    SystemCtlLog::msg << L"could not open process " << pid << " presumed no longer active. Operation skipped";
                    SystemCtlLog::Warning();
                }
            
                if (AttachConsole(pid)) {
                    SetConsoleCtrlHandler(NULL, true);
                    if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)) {
                        SystemCtlLog::msg << L"send ctrl-break to process id " << pid << " failed error code " << ::GetLastError();
                        SystemCtlLog::Warning();
                    }
                    FreeConsole();
                    SetConsoleCtrlHandler(NULL, false);
                }
                CloseHandle(hProc);
            }
            break;
    
        case SystemCtl::KILL_ACTION_CONTROL:
        case SystemCtl::KILL_ACTION_ALL:
        default:
            // 2Do.
            break;
        }
        break;
        break;
    }
    
    return true;
}


static boolean
mask_required_unit(wstring file_path, void *context )

{
    SystemCtlLog::msg << L"mask required Unit " << file_path.c_str();
    SystemCtlLog::Debug();
    std::string filepath_A = std::string(file_path.begin(), file_path.end());
    std::remove(filepath_A.c_str());
    return true;
}

static boolean
mask_wanted_unit(wstring file_path, void *context )

{
    SystemCtlLog::msg << L"mask wanted Unit " << file_path.c_str();
    SystemCtlLog::Debug();
    std::string filepath_A = std::string(file_path.begin(), file_path.end());
    std::remove(filepath_A.c_str());
    return true;
}

boolean SystemDUnit::Mask(boolean block)

{
    // Mask unregisters the service from the service manager, then deletes the service unit 
    // from the active directory.
    wstring servicename = this->name;

    // Is there a requires directory?
    wstring requires_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename+L".requires";
    if (SystemDUnitPool::DirExists(requires_dir_path)) {
        // Enable all of the units and add to the requires list
        (void)SystemDUnitPool::Apply(requires_dir_path, mask_required_unit, (void*)this);
        SystemCtlLog::msg << L"remove directory " << requires_dir_path;
        SystemCtlLog::Debug();
        if (!RemoveDirectoryW(requires_dir_path.c_str())) {
            SystemCtlLog::msg << L"remove directory " << requires_dir_path << " failed " << std::endl; ;
            SystemCtlLog::Debug();
        }
    }

    // Is there a wants directory?
    wstring wants_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename+L".wants";
    if (SystemDUnitPool::DirExists(wants_dir_path)) {
        // Enable all of the units and add to the wants list
        (void)SystemDUnitPool::Apply(wants_dir_path, mask_wanted_unit, (void*)this);

        SystemCtlLog::msg << L"remove directory " << wants_dir_path << std::endl; ;
        SystemCtlLog::Debug();
        if (!RemoveDirectoryW(wants_dir_path.c_str())) {
            SystemCtlLog::msg << L"remove directory " << wants_dir_path << " failed " << std::endl; ;
            SystemCtlLog::Debug();
        }
    }

    this->UnregisterService();

    std::wstring filepath_W = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename;
    std::string filepath_A = std::string(filepath_W.begin(), filepath_W.end());
    // Delete the file
    std::remove(filepath_A.c_str());
    
    // and mark the object
    this->is_enabled = false;
    return false;
}



static boolean
unmask_required_unit(wstring file_path, void *context )

{
    SystemCtlLog::msg << L"unmask required Unit is a stub " << file_path.c_str();
    SystemCtlLog::Debug();
    return true;
}

static boolean
unmask_wanted_unit(wstring file_path, void *context )

{
    SystemCtlLog::msg << L"unmask wanted Unit is a stub " << file_path.c_str();
    SystemCtlLog::Debug();
    return true;
}

boolean SystemDUnit::Unmask(boolean block)

{
    wchar_t * buffer;

    wstring servicename = this->name;
    // Is there a requires directory?
    wstring requires_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename+L".requires";
    if (SystemDUnitPool::DirExists(requires_dir_path)) {
        // Enable all of the units and add to the requires list
        (void)SystemDUnitPool::Apply(requires_dir_path, unmask_required_unit, (void*)this);
    }

    // Is there a wants directory?
    wstring wants_dir_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename+L".wants";
    if (SystemDUnitPool::DirExists(wants_dir_path)) {
        // Enable all of the units and add to the wants list
        (void)SystemDUnitPool::Apply(wants_dir_path, unmask_wanted_unit, (void*)this);
    }

    // Is there a wants directory?
    if (this->IsEnabled()) {
        // We don't error but we don't do anything
        return true;
    }


    // Even if we aren't running now, we could have a unit in the active directory
    // If so, we just need to register ourseleves with the service manager
    wifstream checkfs(SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename);
    if (checkfs.is_open()) {
        checkfs.close();
        return true;
    }
    else {
              
        // If there is no file in the active directory, we copy one over, then register.

        wstring service_unit_path = SystemDUnitPool::UNIT_DIRECTORY_PATH+L"\\"+servicename;
    
        // Find the unit in the unit library
        wifstream fs(service_unit_path, std::fstream::in);
        if (!fs.is_open()) {
             SystemCtlLog::msg << "No service unit " << servicename.c_str() << "Found in unit library" ;
             SystemCtlLog::Error();
             return false;
        }
        fs.seekg (0, fs.end);
        int length = fs.tellg();
        fs.seekg (0, fs.beg);
        buffer = new wchar_t [length];
        fs.read (buffer,length);
    
        fs.close();
    
        wofstream ofs(SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+servicename);
        ofs.write (buffer,length);
        ofs.close();
    
        return true;
    }
    return false;
}


void SystemDUnit::ShowService()

{
    wcout << L"Type=" << ServiceTypeToWString[this->service_type] << std::endl;
    wcout << L"Restart=" << RestartActionToWString[this->restart_action] << std::endl;
    wcout << L"NotifyAccess=" << NotifyActionToWString[this->notify_access] << std::endl;
    wcout << L"RestartUSec=" << (this->restart_sec*1000000.0 ) << std::endl;
    wcout << L"TimeoutStartUSec=" << std::endl;
    wcout << L"TimeoutStopUSec=" << std::endl;
    wcout << L"RuntimeMaxUSec=" << std::endl;
    wcout << L"WatchdogUSec=" << std::endl;
    wcout << L"WatchdogTimestampMonotonic=" << std::endl;
    wcout << L"PermissionsStartOnly=" << std::endl;
    wcout << L"RootDirectoryStartOnly=" << std::endl;
    wcout << L"RemainAfterExit=" << std::endl;
    wcout << L"GuessMainPID=" << std::endl;
    wcout << L"MainPID=" << std::endl;
    wcout << L"ControlPID=" << std::endl;
    wcout << L"FileDescriptorStoreMax=" << std::endl;
    wcout << L"NFileDescriptorStore=" << std::endl;
    wcout << L"StatusErrno=" << std::endl;
    wcout << L"Result=" << std::endl;
    wcout << L"UID=" << std::endl;
    wcout << L"GID=" << std::endl;
    wcout << L"NRestarts=" << std::endl;
    wcout << L"ExecMainStartTimestampMonotonic=" << std::endl;
    wcout << L"ExecMainExitTimestampMonotonic=" << std::endl;
    wcout << L"ExecMainPID=" << std::endl;
    wcout << L"ExecMainCode=" << std::endl;
    wcout << L"ExecMainStatus=" << std::endl;
    wcout << L"ExecStart={" ;
    for (auto exec_start:this->exec_start) {
       wcout << exec_start << L";" ;
    }
    wcout << L"}" << std::endl;
    wcout << L"Slice=" << std::endl;
    wcout << L"MemoryCurrent=" << std::endl;
    wcout << L"CPUUsageNSec=" << std::endl;
    wcout << L"TasksCurrent=" << std::endl;
    wcout << L"IPIngressBytes=" << std::endl;
    wcout << L"IPIngressPackets=" << std::endl;
    wcout << L"IPEgressBytes=" << std::endl;
    wcout << L"IPEgressPackets=" << std::endl;
    wcout << L"Delegate=" << std::endl;
    wcout << L"CPUAccounting=" << std::endl;
    wcout << L"CPUWeight=" << std::endl;
    wcout << L"StartupCPUWeight=" << std::endl;
    wcout << L"CPUShares=" << std::endl;
    wcout << L"StartupCPUShares=" << std::endl;
    wcout << L"CPUQuotaPerSecUSec=" << std::endl;
    wcout << L"IOAccounting=" << std::endl;
    wcout << L"IOWeight=" << std::endl;
    wcout << L"StartupIOWeight=" << std::endl;
    wcout << L"BlockIOAccounting=" << std::endl;
    wcout << L"BlockIOWeight=" << std::endl;
    wcout << L"StartupBlockIOWeight=" << std::endl;
    wcout << L"MemoryAccounting=" << std::endl;
    wcout << L"MemoryLow=" << std::endl;
    wcout << L"MemoryHigh=" << std::endl;
    wcout << L"MemoryMax=" << std::endl;
    wcout << L"MemorySwapMax=" << std::endl;
    wcout << L"MemoryLimit=" << std::endl;
    wcout << L"DevicePolicy=" << std::endl;
    wcout << L"TasksAccounting=" << std::endl;
    wcout << L"TasksMax=" << std::endl;
    wcout << L"IPAccounting=" << std::endl;
    wcout << L"UMask=" << std::endl;
    wcout << L"LimitCPU=" << this->limitCPU << std::endl;
    wcout << L"LimitCPUSoft=" << this->limitCPUSoft << std::endl;
    wcout << L"LimitFSIZE=" << this->limitFSIZE << std::endl;
    wcout << L"LimitFSIZESoft=" << this->limitFSIZESoft << std::endl;
    wcout << L"LimitDATA=" << this->limitDATA << std::endl;
    wcout << L"LimitDATASoft=" << this->limitDATASoft << std::endl;
    wcout << L"LimitSTACK=" << this->limitSTACK << std::endl;
    wcout << L"LimitSTACKSoft=" << this->limitSTACKSoft << std::endl;
    wcout << L"LimitCORE=" << this->limitCORE << std::endl;
    wcout << L"LimitCORESoft=" << this->limitCORESoft << std::endl;
    wcout << L"LimitRSS=" << this->limitRSS << std::endl;
    wcout << L"LimitRSSSoft=" << this->limitRSSSoft << std::endl;
    wcout << L"LimitNOFILE=" << this->limitNOFILE << std::endl;
    wcout << L"LimitNOFILESoft=" << this->limitNOFILESoft << std::endl;
    wcout << L"LimitAS=" << this->limitAS << std::endl;
    wcout << L"LimitASSoft=" << this->limitASSoft << std::endl;
    wcout << L"LimitNPROC=" << this->limitNPROC << std::endl;
    wcout << L"LimitNPROCSoft=" << this->limitNPROCSoft << std::endl;
    wcout << L"LimitMEMLOCK=" << this->limitMEMLOCK << std::endl;
    wcout << L"LimitMEMLOCKSoft=" << this->limitMEMLOCKSoft << std::endl;
    wcout << L"LimitLOCKS=" << this->limitLOCKS << std::endl;
    wcout << L"LimitLOCKSSoft=" << this->limitLOCKSSoft << std::endl;
    wcout << L"LimitSIGPENDING=" << this->limitSIGPENDING << std::endl;
    wcout << L"LimitSIGPENDINGSoft=" << this->limitSIGPENDINGSoft << std::endl;
    wcout << L"LimitMSGQUEUE=" << this->limitMSGQUEUE << std::endl;
    wcout << L"LimitMSGQUEUESoft=" << this->limitMSGQUEUESoft << std::endl;
    wcout << L"LimitNICE=" << this->limitNICE << std::endl;
    wcout << L"LimitNICESoft=" << this->limitNICESoft << std::endl;
    wcout << L"LimitRTPRIO=" << this->limitRTPRIO << std::endl;
    wcout << L"LimitRTPRIOSoft=" << this->limitRTPRIOSoft << std::endl;
    wcout << L"LimitRTTIME=" << this->limitRTTIME << std::endl;
    wcout << L"LimitRTTIMESoft=" << this->limitRTTIMESoft << std::endl;
    wcout << L"OOMScoreAdjust=" << std::endl;
    wcout << L"Nice=" << std::endl;
    wcout << L"IOSchedulingClass=" << std::endl;
    wcout << L"IOSchedulingPriority=" << std::endl;
    wcout << L"CPUSchedulingPolicy=" << std::endl;
    wcout << L"CPUSchedulingPriority=" << std::endl;
    wcout << L"TimerSlackNSec=" << std::endl;
    wcout << L"CPUSchedulingResetOnFork=" << std::endl;
    wcout << L"NonBlocking=" << std::endl;
    wcout << L"StandardInput=" << std::endl;
    wcout << L"StandardInputData=" << std::endl;
    wcout << L"StandardOutput=" << std::endl;
    wcout << L"StandardError=" << std::endl;
    wcout << L"TTYReset=" << std::endl;
    wcout << L"TTYVHangup=" << std::endl;
    wcout << L"TTYVTDisallocate=" << std::endl;
    wcout << L"SyslogPriority=" << std::endl;
    wcout << L"SyslogLevelPrefix=" << std::endl;
    wcout << L"SyslogLevel=" << std::endl;
    wcout << L"SyslogFacility=" << std::endl;
    wcout << L"LogLevelMax=" << std::endl;
    wcout << L"SecureBits=" << std::endl;
    wcout << L"CapabilityBoundingSet=" << std::endl;
    wcout << L"AmbientCapabilities=" << std::endl;
    wcout << L"DynamicUser=" << std::endl;
    wcout << L"RemoveIPC=" << std::endl;
    wcout << L"MountFlags=" << std::endl;
    wcout << L"PrivateTmp=" << std::endl;
    wcout << L"PrivateDevices=" << std::endl;
    wcout << L"ProtectKernelTunables=" << std::endl;
    wcout << L"ProtectKernelModules=" << std::endl;
    wcout << L"ProtectControlGroups=" << std::endl;
    wcout << L"PrivateNetwork=" << std::endl;
    wcout << L"PrivateUsers=" << std::endl;
    wcout << L"ProtectHome=" << std::endl;
    wcout << L"ProtectSystem=" << std::endl;
    wcout << L"SameProcessGroup=" << std::endl;
    wcout << L"UtmpMode=" << std::endl;
    wcout << L"IgnoreSIGPIPE=" << std::endl;
    wcout << L"NoNewPrivileges=" << std::endl;
    wcout << L"SystemCallErrorNumber=" << std::endl;
    wcout << L"LockPersonality=" << std::endl;
    wcout << L"RuntimeDirectoryPreserve=" << std::endl;
    wcout << L"RuntimeDirectoryMode=" << std::endl;
    wcout << L"StateDirectoryMode=" << std::endl;
    wcout << L"CacheDirectoryMode=" << std::endl;
    wcout << L"LogsDirectoryMode=" << std::endl;
    wcout << L"ConfigurationDirectoryMode=" << std::endl;
    wcout << L"MemoryDenyWriteExecute=" << std::endl;
    wcout << L"RestrictRealtime=" << std::endl;
    wcout << L"RestrictNamespaces=" << std::endl;
    wcout << L"MountAPIVFS=" << std::endl;
    wcout << L"KeyringMode=" << std::endl;
    wcout << L"KillMode=" << std::endl;
    wcout << L"KillSignal=" << std::endl;
    wcout << L"SendSIGKILL=" << std::endl;
    wcout << L"SendSIGHUP=" << std::endl;
    wcout << L"Id=" << std::endl;
    wcout << L"Names=" << std::endl;
    wcout << L"Requires=" << std::endl;
    wcout << L"After=" << std::endl;
    wcout << L"Documentation=" << std::endl;
    wcout << L"Description=" << std::endl;
    wcout << L"LoadState=" << std::endl;
    wcout << L"ActiveState=" << std::endl;
    wcout << L"SubState=" << std::endl;
    wcout << L"FragmentPath=" << std::endl;
    wcout << L"UnitFileState=" << std::endl;
    wcout << L"UnitFilePreset=" << std::endl;
    wcout << L"StateChangeTimestampMonotonic=" << std::endl;
    wcout << L"InactiveExitTimestampMonotonic=" << std::endl;
    wcout << L"ActiveEnterTimestampMonotonic=" << std::endl;
    wcout << L"ActiveExitTimestampMonotonic=" << std::endl;
    wcout << L"InactiveEnterTimestampMonotonic=" << std::endl;
    wcout << L"CanStart=" << std::endl;
    wcout << L"CanStop=" << std::endl;
    wcout << L"CanReload=" << std::endl;
    wcout << L"CanIsolate=" << std::endl;
    wcout << L"StopWhenUnneeded=" << std::endl;
    wcout << L"RefuseManualStart=" << std::endl;
    wcout << L"RefuseManualStop=" << std::endl;
    wcout << L"AllowIsolate=" << std::endl;
    wcout << L"DefaultDependencies=" << std::endl;
    wcout << L"OnFailureJobMode=" << std::endl;
    wcout << L"IgnoreOnIsolate=" << std::endl;
    wcout << L"NeedDaemonReload=" << std::endl;
    wcout << L"JobTimeoutUSec=" << std::endl;
    wcout << L"JobRunningTimeoutUSec=" << std::endl;
    wcout << L"JobTimeoutAction=" << std::endl;
    wcout << L"ConditionResult=" << std::endl;
    wcout << L"AssertResult=" << std::endl;
    wcout << L"ConditionTimestampMonotonic=" << std::endl;
    wcout << L"AssertTimestampMonotonic=" << std::endl;
    wcout << L"Transient=" << std::endl;
    wcout << L"Perpetual=" << std::endl;
    wcout << L"StartLimitIntervalUSec=" << std::endl;
    wcout << L"StartLimitBurst=" << std::endl;
    wcout << L"StartLimitAction=" << std::endl;
    wcout << L"FailureAction=" << std::endl;
    wcout << L"SuccessAction=" << std::endl;
    wcout << L"CollectMode=" << std::endl;
}


static void setup_before(SystemDUnit *punit, wstring const &before) 

{
    class SystemDUnit *pother_unit = SystemDUnitPool::FindUnit(before);

    // Add to the after list 
    


}


static void register_unit(std::pair<std::wstring, class SystemDUnit *> entry)

{
    class SystemDUnit *punit = entry.second;

    SystemCtlLog::msg <<L"register unit " << punit->Name();
    SystemCtlLog::Debug();
    punit->RegisterService();
}



static void query_register_unit(std::pair<std::wstring, class SystemDUnit *> entry)
 
{
    class SystemDUnit *punit = entry.second;

    // Is the service loaded? Load it if not
    if (punit) {
        if (!punit->IsEnabled()) {
            SystemCtlLog::msg <<L"query register unit " << punit->Name();
            SystemCtlLog::Debug();
            punit->RegisterService();
        }
    }
}

void setup_own_dependencies(std::pair<std::wstring, class SystemDUnit *> entry)
 
{
    class SystemDUnit *punit = entry.second;

    if (punit) {
        SystemCtlLog::msg << L"setup own dependencies for = " << punit->Name();
        SystemCtlLog::Debug();
        for(auto other_service: punit->GetAfter()) {
            class SystemDUnit *pother_unit = g_pool->GetPool()[other_service];
            if (pother_unit) {
                punit->AddWaitDependency(pother_unit);
            }
        }

        for(auto other_service : punit->GetRequires()) {
            SystemCtlLog::msg << L"required service = " << other_service;
            SystemCtlLog::Debug();

            class SystemDUnit *pother_unit = g_pool->GetPool()[other_service];
            if (pother_unit) {
                punit->AddStartDependency(pother_unit);
            }
        }

        for(auto other_service : punit->GetWants()) {

           SystemCtlLog::msg << L"wanted service = " << other_service;
           SystemCtlLog::Debug();

            class SystemDUnit *pother_unit = g_pool->GetPool()[other_service];
            if (pother_unit) {
                punit->AddStartDependency(pother_unit);
            }
        }
    }
}


void setup_other_dependencies(std::pair<std::wstring, class SystemDUnit *> entry)
 
{
    class SystemDUnit *punit = entry.second;
    // where afters are our concern, we must setup before dependencies in the subject services..
    // So we add our befores to those services afters.

    if (punit) {
        for(auto other_service: punit->GetBefore()) {
            class SystemDUnit *pother_unit = g_pool->GetPool()[other_service];
            if (pother_unit) {
                pother_unit->AddWaitDependency(punit);
            }
        }

        for(auto other_service : punit->GetRequiredBy()) {
            class SystemDUnit *pother_unit = g_pool->GetPool()[other_service];
            if (pother_unit) {
                pother_unit->AddStartDependency(punit);
            }
        }
    }
}

SystemDUnitPool::SystemDUnitPool() 

{
    wstring system_drive;
    DWORD rslt = GetEnvironmentVariableW( L"SystemDrive", BUFFER, MAX_BUFFER_SIZE);

    if (rslt == 0) {
        system_drive = L"C:";
    }
    else {
        system_drive = BUFFER;
    }

    UNIT_DIRECTORY_PATH = system_drive + L"\\etc\\SystemD\\system";
    ACTIVE_UNIT_DIRECTORY_PATH = system_drive + L"\\etc\\SystemD\\active";
    UNIT_WORKING_DIRECTORY_PATH = system_drive + L"\\etc\\SystemD\\run";

    wchar_t name_buffer[2048]; 
    ::GetModuleFileNameW(NULL, name_buffer, 2048);
    wstring mypath = name_buffer;
    size_t index = mypath.find_last_of(L"\\/")+1;
    mypath.erase(index);

    SERVICE_WRAPPER_PATH = mypath; // Single instance for everybody

    this->globals.Version = L"237";
    this->globals.Features = L""; // Really dont know what goes here.... linux features are not relevant
    this->globals.Virtualization=L"microsoft";
    this->globals.Architecture=L"x86-64";
    this->globals.FirmwareTimestampMonotonic=0;
    this->globals.LoaderTimestampMonotonic=0;
    this->globals.KernelTimestampMonotonic=0;
    this->globals.InitRDTimestampMonotonic=0;
    this->globals.UserspaceTimestampMonotonic=0; //35195865
    this->globals.FinishTimestampMonotonic=0; // 219203351
    this->globals.SecurityStartTimestampMonotonic=0; //35259798
    this->globals.SecurityFinishTimestampMonotonic=0; //35266602
    this->globals.GeneratorsStartTimestampMonotonic=0; //36250986
    this->globals.GeneratorsFinishTimestampMonotonic=0; // 37395270
    this->globals.UnitsLoadStartTimestampMonotonic=0; //37396534
    this->globals.UnitsLoadFinishTimestampMonotonic=0; //38863767
    this->globals.LogLevel=L"info";
    this->globals.LogTarget=L"journal-or-kmsg";
    this->globals.NNames=365;
    this->globals.NFailedUnits=3;
    this->globals.NJobs=0;
    this->globals.NInstalledJobs=29126798;
    this->globals.NFailedJobs=0;
    this->globals.Progress=1;
    this->globals.Environment = std::wstring(L"PATH=")+_wgetenv(L"PATH"); L"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    this->globals.ConfirmSpawn=false;
    this->globals.ShowStatus=true;
    this->globals.UnitPath=UNIT_DIRECTORY_PATH; // L"/etc/systemd/system.control /run/systemd/system.control /run/systemd/transient /etc/systemd/system /run/systemd/system /run/systemd/generator /lib/systemd/system /run/systemd/generator.late";
    this->globals.DefaultStandardOutput=L"journal";
    this->globals.DefaultStandardError=L"journal";
    this->globals.RuntimeWatchdogUSec=std::chrono::duration<__int64, std::micro>(0);
    this->globals.ShutdownWatchdogUSec=std::chrono::duration<__int64, std::micro>(10*60*1000*1000); //10min
    this->globals.ServiceWatchdogs=true;
    this->globals.SystemState=L"degraded";
    this->globals.DefaultTimerAccuracyUSec=std::chrono::duration<__int64, std::micro>(60*1000*1000); //1min
    this->globals.DefaultTimeoutStartUSec=std::chrono::duration<__int64, std::micro>(90*1000*1000); // 1min 30s
    this->globals.DefaultTimeoutStopUSec=std::chrono::duration<__int64, std::micro>(90*1000*1000); // 1min 30s
    this->globals.DefaultRestartUSec=std::chrono::duration<__int64, std::micro>(100*1000);
    this->globals.DefaultStartLimitIntervalUSec=std::chrono::duration<__int64, std::micro>(10*1000*1000); //10s
    this->globals.DefaultStartLimitBurst=5;
    this->globals.DefaultCPUAccounting=false;
    this->globals.DefaultBlockIOAccounting=false;
    this->globals.DefaultMemoryAccounting=false;
    this->globals.DefaultTasksAccounting=true;
    this->globals.DefaultLimitCPU=-1; //infinity;
    this->globals.DefaultLimitCPUSoft=-1; //infinity
    this->globals.DefaultLimitFSIZE=-1; //infinity
    this->globals.DefaultLimitFSIZESoft=-1; //infinity
    this->globals.DefaultLimitDATA=-1; // infinity
    this->globals.DefaultLimitDATASoft=-1; // infinity
    this->globals.DefaultLimitSTACK=-1; // infinity
    this->globals.DefaultLimitSTACKSoft=8388608;
    this->globals.DefaultLimitCORE=-1; // infinity
    this->globals.DefaultLimitCORESoft=0;
    this->globals.DefaultLimitRSS=-1; // infinity
    this->globals.DefaultLimitRSSSoft=-1; // infinity
    this->globals.DefaultLimitNOFILE=4096;
    this->globals.DefaultLimitNOFILESoft=1024;
    this->globals.DefaultLimitAS=-1; // infinity
    this->globals.DefaultLimitASSoft=-1; // infinity
    this->globals.DefaultLimitNPROC=108066;
    this->globals.DefaultLimitNPROCSoft=108066;
    this->globals.DefaultLimitMEMLOCK=16777216;
    this->globals.DefaultLimitMEMLOCKSoft=16777216;
    this->globals.DefaultLimitLOCKS=-1;     //infinity
    this->globals.DefaultLimitLOCKSSoft=-1; //infinity
    this->globals.DefaultLimitSIGPENDING=108066;
    this->globals.DefaultLimitSIGPENDINGSoft=108066;
    this->globals.DefaultLimitMSGQUEUE=819200;
    this->globals.DefaultLimitMSGQUEUESoft=819200;
    this->globals.DefaultLimitNICE=0;
    this->globals.DefaultLimitNICESoft=0;
    this->globals.DefaultLimitRTPRIO=0;
    this->globals.DefaultLimitRTPRIOSoft=0;
    this->globals.DefaultLimitRTTIME=-1; //infinity
    this->globals.DefaultLimitRTTIMESoft=-1; //infinity
    this->globals.DefaultTasksMax=32419;
    this->globals.TimerSlackNSec=50000;
}

void SystemDUnitPool::ReloadPool()

{
    SystemCtlLog::msg << "do daemon reload" ; 
    SystemCtlLog::Debug();

    // 2do First clear out the pool, services and active dir and deregister the services
    //
    (void)Apply(SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH.c_str(), delete_unit, (void*)this);

    (void)Apply(SystemDUnitPool::UNIT_DIRECTORY_PATH.c_str(), read_unit, (void*)this);

     // Now we have the graph, we need to resolve a dependencies graph.
     for_each(g_pool->pool.begin(), g_pool->pool.end(), setup_own_dependencies);
     for_each(g_pool->pool.begin(), g_pool->pool.end(), setup_other_dependencies);
     for_each(g_pool->pool.begin(), g_pool->pool.end(), register_unit);
}


void SystemDUnitPool::LoadPool()

{
     SystemCtlLog::msg << "do daemon load" ;
     SystemCtlLog::Debug();
     (void)Apply(SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH.c_str(), read_unit, (void*)this);

     for (auto member: g_pool->GetPool()) {
         SystemCtlLog::msg << L"key = " << member.first << "value = " << member.second->Name() << std::endl; ;
         SystemCtlLog::Debug();
     }

     // Now we have the graph, we need to resolve a dependencies graph.
     for_each(g_pool->pool.begin(), g_pool->pool.end(), setup_own_dependencies);
     for_each(g_pool->pool.begin(), g_pool->pool.end(), setup_other_dependencies);
     for_each(g_pool->pool.begin(), g_pool->pool.end(), query_register_unit);
}


void SystemDUnitPool::ShowGlobal()
{
    wcout << L"Version=" << g_pool->globals.Version << std::endl;
    wcout << L"Features=" << g_pool->globals.Features << std::endl;
    wcout << L"Virtualization=" << g_pool->globals.Virtualization << std::endl;
    wcout << L"Architecture=" << g_pool->globals.Architecture << std::endl;
    wcout << L"FirmwareTimestampMonotonic=" << g_pool->globals.FirmwareTimestampMonotonic << std::endl;
    wcout << L"LoaderTimestampMonotonic="  << g_pool->globals.LoaderTimestampMonotonic << std::endl;
    wcout << L"KernelTimestamp=" << std::endl;
    wcout << L"KernelTimestampMonotonic=" << g_pool->globals.KernelTimestampMonotonic << std::endl;
    wcout << L"InitRDTimestampMonotonic=" << g_pool->globals.InitRDTimestampMonotonic << std::endl;
    wcout << L"UserspaceTimestamp=" << std::endl;
    wcout << L"UserspaceTimestampMonotonic=" << g_pool->globals.UserspaceTimestampMonotonic << std::endl;
    wcout << L"FinishTimestamp=" << std::endl;
    wcout << L"FinishTimestampMonotonic=" << g_pool->globals.FinishTimestampMonotonic << std::endl;
    wcout << L"SecurityStartTimestamp=" << std::endl;
    wcout << L"SecurityStartTimestampMonotonic=" << g_pool->globals.SecurityStartTimestampMonotonic << std::endl;
    wcout << L"SecurityFinishTimestamp=" << std::endl;
    wcout << L"SecurityFinishTimestampMonotonic=" << g_pool->globals.SecurityFinishTimestampMonotonic << std::endl;
    wcout << L"GeneratorsStartTimestamp=" << std::endl;
    wcout << L"GeneratorsStartTimestampMonotonic=" << g_pool->globals.GeneratorsStartTimestampMonotonic << std::endl;
    wcout << L"GeneratorsFinishTimestamp=" << std::endl;
    wcout << L"GeneratorsFinishTimestampMonotonic=" << g_pool->globals.GeneratorsFinishTimestampMonotonic << std::endl;
    wcout << L"UnitsLoadStartTimestamp=" << std::endl;
    wcout << L"UnitsLoadStartTimestampMonotonic=" << g_pool->globals.UnitsLoadStartTimestampMonotonic << std::endl;
    wcout << L"UnitsLoadFinishTimestamp=" << std::endl;
    wcout << L"UnitsLoadFinishTimestampMonotonic=" << g_pool->globals.UnitsLoadFinishTimestampMonotonic << std::endl;
    wcout << L"LogLevel=" << g_pool->globals.LogLevel << std::endl;
    wcout << L"LogTarget=" << g_pool->globals.LogTarget << std::endl;
    wcout << L"NNames=" << g_pool->globals.NNames << std::endl;
    wcout << L"NFailedUnits=" << g_pool->globals.NFailedUnits << std::endl;
    wcout << L"NJobs=" << g_pool->globals.NJobs << std::endl;
    wcout << L"NInstalledJobs=" << g_pool->globals.NInstalledJobs << std::endl;
    wcout << L"NFailedJobs=" << g_pool->globals.NFailedJobs << std::endl;
    wcout << L"Progress=" << g_pool->globals.Progress << std::endl;
    wcout << L"Environment=" << g_pool->globals.Environment  << std::endl;
    wcout << L"ConfirmSpawn=" << g_pool->globals.ConfirmSpawn << std::endl;
    wcout << L"ShowStatus=" << g_pool->globals.ShowStatus << std::endl;
    wcout << L"UnitPath=" << g_pool->globals.UnitPath << std::endl;
    wcout << L"DefaultStandardOutput=" << g_pool->globals.DefaultStandardOutput << std::endl;
    wcout << L"DefaultStandardError=" << g_pool->globals.DefaultStandardError << std::endl;
    wcout << L"RuntimeWatchdogUSec=" << std::chrono::duration_cast<std::chrono::microseconds>(g_pool->globals.RuntimeWatchdogUSec).count() << std::endl;
    wcout << L"ShutdownWatchdogUSec=" << std::chrono::duration_cast<std::chrono::microseconds>(g_pool->globals.ShutdownWatchdogUSec).count() << std::endl;
    wcout << L"ServiceWatchdogs=" << g_pool->globals.ServiceWatchdogs << std::endl;
    wcout << L"SystemState=" << g_pool->globals.SystemState << std::endl;
    wcout << L"DefaultTimerAccuracyUSec=" << std::chrono::duration_cast<std::chrono::microseconds>(g_pool->globals.DefaultTimerAccuracyUSec).count() << std::endl;
    wcout << L"DefaultTimeoutStartUSec=" << std::chrono::duration_cast<std::chrono::microseconds>(g_pool->globals.DefaultTimeoutStartUSec).count() << std::endl;
    wcout << L"DefaultTimeoutStopUSec=" << std::chrono::duration_cast<std::chrono::microseconds>(g_pool->globals.DefaultTimeoutStopUSec).count() << std::endl;
    wcout << L"DefaultRestartUSec=" << std::chrono::duration_cast<std::chrono::microseconds>(g_pool->globals.DefaultRestartUSec).count() << std::endl;
    wcout << L"DefaultStartLimitIntervalUSec=" << std::chrono::duration_cast<std::chrono::microseconds>(g_pool->globals.DefaultStartLimitIntervalUSec).count() << std::endl;
    wcout << L"DefaultStartLimitBurst=" << g_pool->globals.DefaultStartLimitBurst << std::endl;
    wcout << L"DefaultCPUAccounting=" << g_pool->globals.DefaultCPUAccounting << std::endl;
    wcout << L"DefaultBlockIOAccounting=" << g_pool->globals.DefaultBlockIOAccounting << std::endl;
    wcout << L"DefaultMemoryAccounting=" << g_pool->globals.DefaultMemoryAccounting << std::endl;
    wcout << L"DefaultTasksAccounting=" << g_pool->globals.DefaultTasksAccounting << std::endl;
    wcout << L"DefaultLimitCPU=" << g_pool->globals.DefaultLimitCPU << std::endl;
    wcout << L"DefaultLimitCPUSoft=" << g_pool->globals.DefaultLimitCPUSoft << std::endl;
    wcout << L"DefaultLimitFSIZE=" << g_pool->globals.DefaultLimitFSIZE << std::endl;
    wcout << L"DefaultLimitFSIZESoft=" << g_pool->globals.DefaultLimitFSIZESoft << std::endl;
    wcout << L"DefaultLimitDATA=" << g_pool->globals.DefaultLimitDATA << std::endl;
    wcout << L"DefaultLimitDATASoft=" << g_pool->globals.DefaultLimitDATASoft << std::endl;
    wcout << L"DefaultLimitSTACK=" << g_pool->globals.DefaultLimitSTACK << std::endl;
    wcout << L"DefaultLimitSTACKSoft=" << g_pool->globals.DefaultLimitSTACKSoft << std::endl;
    wcout << L"DefaultLimitCORE=" << g_pool->globals.DefaultLimitCORE << std::endl;
    wcout << L"DefaultLimitCORESoft=" << g_pool->globals.DefaultLimitCORESoft << std::endl;
    wcout << L"DefaultLimitRSS=" << g_pool->globals.DefaultLimitRSS << std::endl;
    wcout << L"DefaultLimitRSSSoft=" << g_pool->globals.DefaultLimitRSSSoft << std::endl;
    wcout << L"DefaultLimitNOFILE=" << g_pool->globals.DefaultLimitNOFILE << std::endl;
    wcout << L"DefaultLimitNOFILESoft=" << g_pool->globals.DefaultLimitNOFILESoft << std::endl;
    wcout << L"DefaultLimitAS=" << g_pool->globals.DefaultLimitAS << std::endl;
    wcout << L"DefaultLimitASSoft=" << g_pool->globals.DefaultLimitASSoft << std::endl;
    wcout << L"DefaultLimitNPROC=" << g_pool->globals.DefaultLimitNPROC << std::endl;
    wcout << L"DefaultLimitNPROCSoft=" << g_pool->globals.DefaultLimitNPROCSoft << std::endl;
    wcout << L"DefaultLimitMEMLOCK=" << g_pool->globals.DefaultLimitMEMLOCK << std::endl;
    wcout << L"DefaultLimitMEMLOCKSoft=" << g_pool->globals.DefaultLimitMEMLOCKSoft << std::endl;
    wcout << L"DefaultLimitLOCKS=" << g_pool->globals.DefaultLimitLOCKS << std::endl;
    wcout << L"DefaultLimitLOCKSSoft=" << g_pool->globals.DefaultLimitLOCKSSoft << std::endl;
    wcout << L"DefaultLimitSIGPENDING=" << g_pool->globals.DefaultLimitSIGPENDING << std::endl;
    wcout << L"DefaultLimitSIGPENDINGSoft=" << g_pool->globals.DefaultLimitSIGPENDINGSoft << std::endl;
    wcout << L"DefaultLimitMSGQUEUE=" << g_pool->globals.DefaultLimitMSGQUEUE << std::endl;
    wcout << L"DefaultLimitMSGQUEUESoft=" << g_pool->globals.DefaultLimitMSGQUEUESoft << std::endl;
    wcout << L"DefaultLimitNICE=" << g_pool->globals.DefaultLimitNICE << std::endl;
    wcout << L"DefaultLimitNICESoft=" << g_pool->globals.DefaultLimitNICESoft << std::endl;
    wcout << L"DefaultLimitRTPRIO=" << g_pool->globals.DefaultLimitRTPRIO << std::endl;
    wcout << L"DefaultLimitRTPRIOSoft=" << g_pool->globals.DefaultLimitRTPRIOSoft << std::endl;
    wcout << L"DefaultLimitRTTIME=" << g_pool->globals.DefaultLimitRTTIME << std::endl;
    wcout << L"DefaultLimitRTTIMESoft=" << g_pool->globals.DefaultLimitRTTIMESoft << std::endl;
    wcout << L"DefaultTasksMax=" << g_pool->globals.DefaultTasksMax << std::endl;
    wcout << L"TimerSlackNSec=" << g_pool->globals.TimerSlackNSec << std::endl;
}


boolean
SystemDUnit::attr_service_type( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_TYPE;
            
    if (attr_value.compare(L"simple") == 0 ) {
        this->service_type = SERVICE_TYPE_SIMPLE;
    }
    else if (attr_value.compare(L"forking") == 0 ) {
        this->service_type = SERVICE_TYPE_FORKING;
    }
    else if (attr_value.compare(L"oneshot") == 0 ) {
        this->service_type = SERVICE_TYPE_ONESHOT;
    }
    else if (attr_value.compare(L"dbus") == 0 ) {
        this->service_type = SERVICE_TYPE_DBUS;
    }
    else if (attr_value.compare(L"notify") == 0 ) {
        this->service_type = SERVICE_TYPE_NOTIFY;
    }
    else if (attr_value.compare(L"idle") == 0 ) {
        this->service_type = SERVICE_TYPE_IDLE;
    }
    else {
        this->service_type = SERVICE_TYPE_UNDEFINED;
        SystemCtlLog::msg << "service type " << attr_value.c_str() << "is unknown" ; ;
        SystemCtlLog::Warning();
    }
    return true;
}

boolean
SystemDUnit::attr_remain_after_exit( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_REMAIN_AFTER_EXIT;
    if (attr_value.compare(L"yes") == 0 ) {
        this->remain_after_exit = true;
    }
    else if (attr_value.compare(L"no") == 0 ) {
        this->remain_after_exit = false;
    }
    else {
        this->remain_after_exit = false;
    }

    return true;
}

boolean
SystemDUnit::attr_guess_main_pid( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_GUESS_MAIN_PID;
    if (attr_value.compare(L"yes") == 0 ) {
        this->guess_main_pid = true;
    }
    else if (attr_value.compare(L"no") == 0 ) {
        this->guess_main_pid = false;
    }
    else {
        this->guess_main_pid = false;
    }

    return true;
}


boolean
SystemDUnit::attr_pid_file( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_PID_FILE;
    this->pid_file = attr_value;
    return true;
}


boolean
SystemDUnit::attr_bus_name( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_BUS_NAME;
    this->bus_name = attr_value;
    return true;
}

boolean
SystemDUnit::attr_exec_start_pre( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_EXEC_START_PRE;
    this->exec_start_pre.push_back(attr_value);
    return true;
}


boolean
SystemDUnit::attr_exec_start( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_EXEC_START;
    this->exec_start.push_back(attr_value);
    return true;
}


boolean
SystemDUnit::attr_exec_start_post( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_EXEC_START_POST;
    this->exec_start_post.push_back(attr_value);
    return true;
}


boolean
SystemDUnit::attr_exec_stop( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_EXEC_STOP;
    this->exec_stop.push_back(attr_value);
    return true;
}


boolean
SystemDUnit::attr_exec_stop_post( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_EXEC_STOP_POST;
    this->exec_stop_post.push_back(attr_value);
    return true;
}


boolean
SystemDUnit::attr_exec_reload( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_EXEC_RELOAD;
    this->exec_reload.push_back(attr_value);
    return true;
}


boolean
SystemDUnit::attr_restart_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_RESTART_SEC;
    if (attr_value.compare(L"infinity") == 0 ) {
        this->restart_sec = DBL_MAX;
    }
    else {
        try {
        size_t toklen = 0;
            this->restart_sec = std::stod(attr_value, &toklen);
            if (toklen < attr_value.length()) {
                double millis = NAN;
    
                if (!ParseDuration(attr_value, millis) ) {
                    SystemCtlLog::msg << "restart_sec invalid value : " << attr_value.c_str() ; ;
                    SystemCtlLog::Warning();
                }
                else {
                        this->restart_sec = millis*0.001;
                }
            }
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "restart_sec invalid value : " << attr_value.c_str() ; ;
                SystemCtlLog::Warning();
            }
            else {
                this->restart_sec = millis*0.001;
            }
        }
    }
    return true;
}


boolean
SystemDUnit::attr_timeout_start_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    attr_bitmask |= ATTRIBUTE_BIT_TIMEOUT_START_SEC;
    if (attr_value.compare(L"infinity") == 0 ) {
        this->timeout_start_sec = DBL_MAX;
    }
    else {
        try {
           this->timeout_start_sec = stod(attr_value);
        }
        catch (const std::exception &e) {
        double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "timeout_start_sec invalid value : " << attr_value.c_str() ; ;
                SystemCtlLog::Warning();
            }
        else {
                this->timeout_start_sec = millis*0.001;
        }
        }
    }
    return true;
}


boolean
SystemDUnit::attr_timeout_stop_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();

    attr_bitmask |= ATTRIBUTE_BIT_TIMEOUT_STOP_SEC;
    if (attr_value.compare(L"infinity") == 0 ) {
        this->timeout_stop_sec = DBL_MAX;
    }
    else {
        try {
            this->timeout_stop_sec = stod(attr_value);
        }
        catch (const std::exception &e) {
            // Try to convert from time span like "5min 20s"
            SystemCtlLog::msg << "timeout_stop_sec invalid value : " << attr_value.c_str();
            SystemCtlLog::Warning();
        }
    }
    return true;
}



boolean
SystemDUnit::attr_timeout_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();

    attr_bitmask |= (ATTRIBUTE_BIT_TIMEOUT_START_SEC | ATTRIBUTE_BIT_TIMEOUT_STOP_SEC);
    if (attr_value.compare(L"infinity") == 0 ) {
        this->timeout_start_sec = DBL_MAX;
        this->timeout_stop_sec = DBL_MAX;
    }
    else {
        try {
           auto val = stod(attr_value);
           this->timeout_stop_sec = val;
        }
        catch (const std::exception &e) {
        double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "timeout_sec invalid value : " << attr_value.c_str() ;
                SystemCtlLog::Warning();
            }
        else {
                this->timeout_stop_sec = millis*0.001;
        }
        }
    }
    return true;
}


boolean
SystemDUnit::attr_runtime_max_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();

    attr_bitmask |= ATTRIBUTE_BIT_RUNTIME_MAX_SEC;
    if (attr_value.compare(L"infinity") == 0) {
        this->max_runtime_sec = DBL_MAX;
    }
    else {
        try {
            this->max_runtime_sec = stod(attr_value);
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "RuntimeMaxSec invalid value : " << attr_value.c_str();
                SystemCtlLog::Warning();
            }
            else {
                this->max_runtime_sec = millis*0.001;
            }
        }
    }
    return true;
}


boolean
SystemDUnit::attr_watchdog_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();

    attr_bitmask |= ATTRIBUTE_BIT_WATCHDOG_SEC;
    if (attr_value.compare(L"infinity") == 0 ) {
        this->watchdog_sec = DBL_MAX;
    }
    else {
        try {
            this->watchdog_sec = stod(attr_value);
        }
        catch (const std::exception &e) {
            // Try to convert from time span like "5min 20s"
            SystemCtlLog::msg << L"watchdog_sec invalid value : " << attr_value.c_str() ;
            SystemCtlLog::Warning();
        }
    }
    return true;
}


boolean
SystemDUnit::attr_restart( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    static std::map <std::wstring, enum RestartAction> restart_action_translate = {
              { L"undefined",   RESTART_ACTION_UNDEFINED },
              { L"no",          RESTART_ACTION_NO },
              { L"always",      RESTART_ACTION_ALWAYS},
              { L"on_success",  RESTART_ACTION_ON_SUCCESS },
              { L"on_failure",  RESTART_ACTION_ON_FAILURE },
              { L"on_abnormal", RESTART_ACTION_ON_ABNORMAL },
              { L"on_abort",    RESTART_ACTION_ON_ABORT },
              { L"on_watchdog", RESTART_ACTION_ON_WATCHDOG }
         };

    SystemCtlLog::msg << L"Restart=" << L" value = " << attr_value.c_str() ;
    SystemCtlLog::Verbose();
    try {
        this->restart_action = restart_action_translate[attr_value];
        attr_bitmask |= ATTRIBUTE_BIT_RESTART;
    }
    catch (...) {
        SystemCtlLog::msg << L"Restart=" << L" value = " << attr_value.c_str() << " invalid" ;
        SystemCtlLog::Warning();
        return false;
    }
    return true;
}


boolean
SystemDUnit::attr_success_exit_status( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_SUCCESS_EXIT_STATUS;
    return true;
}


boolean
SystemDUnit::attr_restart_prevent_exit_status( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_RESTART_PREVENT_EXIT_STATUS;
    return true;
}


boolean
SystemDUnit::attr_restart_force_exit_status( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_RESTART_FORCE_EXIT_STATUS;
    return true;
}


boolean
SystemDUnit::attr_permissions_start_only( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_PERMISSIONS_START_ONLY;
    return true;
}


boolean
SystemDUnit::attr_root_directory_start_only( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_ROOT_DIRECTORY_START_ONLY;
    return true;
}


boolean
SystemDUnit::attr_non_blocking( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_NON_BLOCKING;
    return true;
}


boolean
SystemDUnit::attr_notify_access( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_NOTIFY_ACCESS;
    return true;
}


boolean
SystemDUnit::attr_sockets( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_SOCKETS;
    return true;
}


boolean
SystemDUnit::attr_file_descriptor_store_max( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_FILE_DESCRIPTOR_STORE_MAX;
    return true;
}


boolean
SystemDUnit::attr_usb_function_descriptors( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_USB_FUNCTION_DESCRIPTORS;
    return true;
}


boolean
SystemDUnit::attr_usb_function_strings( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    attr_bitmask |= ATTRIBUTE_BIT_USB_FUNCTION_STRINGS;
    return true;
}


boolean
SystemDUnit::attr_environment( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"Environment " << attr_value.c_str();
    SystemCtlLog::Verbose();

    wstring value_list = attr_value;
    int end = 0;
    for (auto start = 0; start != std::string::npos; start = end) {
        end = value_list.find_first_of(' ', start);
        if (end != string::npos){
            this->environment_vars.push_back(value_list.substr(start, end));
        }
        else {
            this->environment_vars.push_back(value_list);
        }
    }
    return true;
}


boolean
SystemDUnit::attr_environment_file( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"EnvironmentFile= " << attr_value.c_str();
    SystemCtlLog::Verbose();
    this->environment_file.push_back(attr_value.c_str());
    return true;
}


boolean
SystemDUnit::attr_standard_output( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"StandardOutput = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    enum OUTPUT_TYPE out_type = String_To_OutputType(attr_value.c_str());
    switch (out_type) {
    default:
    case OUTPUT_TYPE_INVALID:
        SystemCtlLog::msg << L"StandardOutput invalid value " << attr_value.c_str();
        SystemCtlLog::Warning();
        break;

    case OUTPUT_TYPE_INHERIT:
    case OUTPUT_TYPE_NULL:
    case OUTPUT_TYPE_TTY:
    case OUTPUT_TYPE_JOURNAL:
    case OUTPUT_TYPE_SYSLOG:
    case OUTPUT_TYPE_KMSG:
    case OUTPUT_TYPE_JOURNAL_PLUS_CONSOLE:
    case OUTPUT_TYPE_SYSLOG_PLUS_CONSOLE:
    case OUTPUT_TYPE_KMSG_PLUS_CONSOLE:
    case OUTPUT_TYPE_SOCKET:
        this->output_type = out_type;
        break;
    case OUTPUT_TYPE_FILE:   // requires a path
    case OUTPUT_TYPE_FD:      // requires a name
        int    delpos = attr_value.find_first_of(L":")+1;
        wstring    val = attr_value.substr(delpos, attr_value.length()-delpos);
        this->output_type = out_type;
        this->output_file_path = val;
        break;
    }
    return true;
}


boolean
SystemDUnit::attr_standard_error( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"StandardError = " << attr_value.c_str();
    SystemCtlLog::Verbose();
    enum OUTPUT_TYPE out_type = String_To_OutputType(attr_value.c_str());
    switch (out_type) {
    default:
    case OUTPUT_TYPE_INVALID:
        SystemCtlLog::msg << L"StandardError invalid value " << attr_value.c_str();
        SystemCtlLog::Warning();
        break;
    case OUTPUT_TYPE_INHERIT:
    case OUTPUT_TYPE_NULL:
    case OUTPUT_TYPE_TTY:
    case OUTPUT_TYPE_JOURNAL:
    case OUTPUT_TYPE_SYSLOG:
    case OUTPUT_TYPE_KMSG:
    case OUTPUT_TYPE_JOURNAL_PLUS_CONSOLE:
    case OUTPUT_TYPE_SYSLOG_PLUS_CONSOLE:
    case OUTPUT_TYPE_KMSG_PLUS_CONSOLE:
    case OUTPUT_TYPE_SOCKET:
        this->output_type = out_type;
        break;
    case OUTPUT_TYPE_FILE:   // requires a path
    case OUTPUT_TYPE_FD:      // requires a name
        int    delpos = attr_value.find_first_of(L":")+1;
        wstring    val = attr_value.substr(delpos, attr_value.length()-delpos);
        this->error_type = out_type;
        this->error_file_path = val;
        break;
    }
    return true;
}


boolean
SystemDUnit::attr_not_implemented( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    SystemCtlLog::msg << L"2do: attrs = " << attr_name.c_str() << L" value = " << attr_value.c_str();
    SystemCtlLog::Debug();
    return true;
}


std::map< std::wstring , SystemDUnit::systemd_service_attr_func> SystemDUnit::SystemD_Service_Attribute_Map =  {
        { L"Type",            &SystemDUnit::attr_service_type },
        { L"RemainAfterExit", &SystemDUnit::attr_remain_after_exit },
        { L"GuessMainPID",    &SystemDUnit::attr_guess_main_pid },
        { L"PIDFile",         &SystemDUnit::attr_pid_file },
        { L"BusName",         &SystemDUnit::attr_bus_name },
        { L"ExecStartPre",    &SystemDUnit::attr_exec_start_pre },
        { L"ExecStart",       &SystemDUnit::attr_exec_start },
        { L"ExecStartPost",   &SystemDUnit::attr_exec_start_post },
        { L"ExecReload",      &SystemDUnit::attr_exec_reload },
        { L"ExecStop",        &SystemDUnit::attr_exec_stop },
        { L"ExecStopPost",    &SystemDUnit::attr_exec_stop },
        { L"RestartSec",      &SystemDUnit::attr_restart_sec },
        { L"TimeoutStartSec", &SystemDUnit::attr_timeout_start_sec },
        { L"TimeoutStopSec",  &SystemDUnit::attr_timeout_stop_sec },
        { L"TimeoutSec",      &SystemDUnit::attr_timeout_sec },
        { L"RuntimeMaxSec",   &SystemDUnit::attr_runtime_max_sec },
        { L"WatchdogSec",     &SystemDUnit::attr_watchdog_sec },
        { L"Restart",         &SystemDUnit::attr_restart },
        { L"SuccessExitStatus",  &SystemDUnit::attr_success_exit_status },
        { L"RestartPreventExitStatus", &SystemDUnit::attr_restart_prevent_exit_status },
        { L"RestartForceExitStatus",   &SystemDUnit::attr_restart_force_exit_status },
        { L"PermissionsStartOnly",     &SystemDUnit::attr_permissions_start_only },
        { L"RootDirectoryStartOnly",     &SystemDUnit::attr_root_directory_start_only },
        { L"NonBlocking",     &SystemDUnit::attr_non_blocking },
        { L"NotifyAccess",     &SystemDUnit::attr_notify_access },
        { L"Sockets",     &SystemDUnit::attr_sockets },
        { L"FileDescriptorStoreMax",  &SystemDUnit::attr_file_descriptor_store_max },
        { L"USBFunctionDescriptors",  &SystemDUnit::attr_usb_function_descriptors },
        { L"USBFunctionStrings",  &SystemDUnit::attr_usb_function_strings },
        { L"WorkingDirectory", &SystemDUnit::attr_not_implemented },
        { L"RootDirectory", &SystemDUnit::attr_not_implemented },
        { L"RootImage", &SystemDUnit::attr_not_implemented },
        { L"MountAPIVFS", &SystemDUnit::attr_not_implemented },
        { L"BindPaths", &SystemDUnit::attr_not_implemented },
        { L"BindReadOnlyPaths", &SystemDUnit::attr_not_implemented },
        { L"User", &SystemDUnit::attr_not_implemented },
        { L"Group", &SystemDUnit::attr_not_implemented },
        { L"DynamicUser", &SystemDUnit::attr_not_implemented },
        { L"SupplementaryGroups", &SystemDUnit::attr_not_implemented },
        { L"PAMName", &SystemDUnit::attr_not_implemented },
        { L"CapabilityBoundingSet", &SystemDUnit::attr_not_implemented },
        { L"AmbientCapabilities", &SystemDUnit::attr_not_implemented },
        { L"NoNewPrivileges", &SystemDUnit::attr_not_implemented },
        { L"SecureBits", &SystemDUnit::attr_not_implemented },
        { L"SELinuxContext", &SystemDUnit::attr_not_implemented },
        { L"AppArmorProfile", &SystemDUnit::attr_not_implemented },
        { L"SmackProcessLabel", &SystemDUnit::attr_not_implemented },
        { L"UMask", &SystemDUnit::attr_not_implemented },
        { L"KeyringMode", &SystemDUnit::attr_not_implemented },
        { L"OOMScoreAdjust", &SystemDUnit::attr_not_implemented },
        { L"TimerSlackNSec", &SystemDUnit::attr_not_implemented },
        { L"Personality", &SystemDUnit::attr_not_implemented },
        { L"IgnoreSIGPIPE", &SystemDUnit::attr_not_implemented },
        { L"Nice", &SystemDUnit::attr_not_implemented },
        { L"CPUSchedulingPolicy", &SystemDUnit::attr_not_implemented },
        { L"CPUSchedulingPriority", &SystemDUnit::attr_not_implemented },
        { L"CPUSchedulingResetOnFork", &SystemDUnit::attr_not_implemented },
        { L"CPUAffinity", &SystemDUnit::attr_not_implemented },
        { L"IOSchedulingClass", &SystemDUnit::attr_not_implemented },
        { L"IOSchedulingPriority", &SystemDUnit::attr_not_implemented },
        { L"ProtectSystem", &SystemDUnit::attr_not_implemented },
        { L"ProtectHome", &SystemDUnit::attr_not_implemented },
        { L"RuntimeDirectory", &SystemDUnit::attr_not_implemented },
        { L"StateDirectory", &SystemDUnit::attr_not_implemented },
        { L"CacheDirectory", &SystemDUnit::attr_not_implemented },
        { L"LogsDirectory", &SystemDUnit::attr_not_implemented },
        { L"ConfigurationDirectory", &SystemDUnit::attr_not_implemented },
        { L"RuntimeDirectoryMode", &SystemDUnit::attr_not_implemented },
        { L"StateDirectoryMode", &SystemDUnit::attr_not_implemented },
        { L"CacheDirectoryMode", &SystemDUnit::attr_not_implemented },
        { L"LogsDirectoryMode", &SystemDUnit::attr_not_implemented },
        { L"ConfigurationDirectoryMode", &SystemDUnit::attr_not_implemented },
        { L"RuntimeDirectoryPreserve", &SystemDUnit::attr_not_implemented },
        { L"ReadWritePaths", &SystemDUnit::attr_not_implemented },
        { L"ReadOnlyPaths", &SystemDUnit::attr_not_implemented },
        { L"InaccessiblePaths", &SystemDUnit::attr_not_implemented },
        { L"TemporaryFileSystem", &SystemDUnit::attr_not_implemented },
        { L"PrivateTmp", &SystemDUnit::attr_not_implemented },
        { L"PrivateDevices", &SystemDUnit::attr_not_implemented },
        { L"PrivateNetwork", &SystemDUnit::attr_not_implemented },
        { L"PrivateUsers", &SystemDUnit::attr_not_implemented },
        { L"ProtectKernelTunables", &SystemDUnit::attr_not_implemented },
        { L"ProtectKernelModules", &SystemDUnit::attr_not_implemented },
        { L"ProtectControlGroups", &SystemDUnit::attr_not_implemented },
        { L"RestrictAddressFamilies", &SystemDUnit::attr_not_implemented },
        { L"RestrictNamespaces", &SystemDUnit::attr_not_implemented },
        { L"LockPersonality", &SystemDUnit::attr_not_implemented },
        { L"MemoryDenyWriteExecute", &SystemDUnit::attr_not_implemented },
        { L"RestrictRealtime", &SystemDUnit::attr_not_implemented },
        { L"RemoveIPC", &SystemDUnit::attr_not_implemented },
        { L"MountFlags", &SystemDUnit::attr_not_implemented },
        { L"SystemCallFilter", &SystemDUnit::attr_not_implemented },
        { L"SystemCallErrorNumber", &SystemDUnit::attr_not_implemented },
        { L"SystemCallArchitectures", &SystemDUnit::attr_not_implemented },
        { L"Environment", &SystemDUnit::attr_environment },
        { L"EnvironmentFile", &SystemDUnit::attr_environment_file },
        { L"PassEnvironment", &SystemDUnit::attr_not_implemented },
        { L"UnsetEnvironment", &SystemDUnit::attr_not_implemented },
        { L"StandardInput", &SystemDUnit::attr_not_implemented },
        { L"StandardOutput", &SystemDUnit::attr_standard_output },
        { L"StandardError", &SystemDUnit::attr_standard_error },
        { L"StandardInputText", &SystemDUnit::attr_not_implemented },
        { L"StandardInputData", &SystemDUnit::attr_not_implemented },
        { L"LogLevelMax", &SystemDUnit::attr_not_implemented },
        { L"LogExtraFields", &SystemDUnit::attr_not_implemented },
        { L"StartLimitIntervalSec", &SystemDUnit::attr_not_implemented },
        { L"StartLimitInterval", &SystemDUnit::attr_not_implemented },
        { L"StartLimitBurst", &SystemDUnit::attr_not_implemented },
        { L"StartLimitAction", &SystemDUnit::attr_not_implemented },
        { L"KillSignal", &SystemDUnit::attr_not_implemented },
        { L"SyslogIdentifier", &SystemDUnit::attr_not_implemented },
        { L"SyslogFacility", &SystemDUnit::attr_not_implemented },
        { L"SyslogLevel", &SystemDUnit::attr_not_implemented },
        { L"SyslogLevelPrefix", &SystemDUnit::attr_not_implemented },
        { L"TTYPath", &SystemDUnit::attr_not_implemented },
        { L"TTYReset", &SystemDUnit::attr_not_implemented },
        { L"TTYVHangup", &SystemDUnit::attr_not_implemented },
        { L"TTYVTDisallocate", &SystemDUnit::attr_not_implemented },
        { L"UtmpIdentifier", &SystemDUnit::attr_not_implemented },
        { L"UtmpMode", &SystemDUnit::attr_not_implemented },
        { L"LimitCPU", &SystemDUnit::attr_not_implemented },
        { L"LimitFSIZE", &SystemDUnit::attr_not_implemented },
        { L"LimitDATA", &SystemDUnit::attr_not_implemented },
        { L"LimitSTACK", &SystemDUnit::attr_not_implemented },
        { L"LimitCORE", &SystemDUnit::attr_not_implemented },
        { L"LimitRSS", &SystemDUnit::attr_not_implemented },
        { L"LimitNOFILE", &SystemDUnit::attr_not_implemented },
        { L"LimitAS", &SystemDUnit::attr_not_implemented },
        { L"LimitNPROC", &SystemDUnit::attr_not_implemented },
        { L"LimitMEMLOCK", &SystemDUnit::attr_not_implemented },
        { L"LimitLOCKS", &SystemDUnit::attr_not_implemented },
        { L"LimitSIGPENDING", &SystemDUnit::attr_not_implemented },
        { L"LimitMSGQUEUE", &SystemDUnit::attr_not_implemented },
        { L"LimitNICE", &SystemDUnit::attr_not_implemented },
        { L"LimitRTPRIO", &SystemDUnit::attr_not_implemented },
        { L"LimitRTTIME", &SystemDUnit::attr_not_implemented },
};

