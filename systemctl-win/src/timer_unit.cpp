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
#include "service_unit.h"

using namespace std;


extern wchar_t BUFFER[];

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

class SystemDTimer *
SystemDUnitPool::ReadTimerUnit(std::wstring name, std::wstring service_unit_path)
{
     wstring servicename = name;
     class SystemDTimer *punit = NULL;

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
         punit = SystemDTimer::ParseSystemDTimerUnit(servicename, service_unit_path, fs);
     }
     else {
         SystemCtlLog::msg << L"No service unit " << servicename.c_str() << L"Found in unit library";
         SystemCtlLog::Error();
     }
     fs.close();

     return punit;
}


class SystemDTimer *SystemDTimer::ParseSystemDTimerUnit(wstring servicename, wstring unit_path, wifstream &fs)
{ 
    SystemCtlLog::msg << L"ParseSystemDTimerUnit unit = " << servicename;
    SystemCtlLog::Debug();

    std::wstring line;
    class SystemDTimer *punit = new class SystemDTimer((wchar_t*)servicename.c_str(), unit_path.c_str());
 
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
        else if (line.compare(L"[Service]") == 0) {
            // Then we need to parse the service section
            SystemCtlLog::msg << L"parse service section";
            SystemCtlLog::Debug();

            line = punit->ParseServiceSection(fs);
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
 
wstring SystemDTimer::ParseTimerSection( wifstream &fs)

{   
    unsigned long attr_bitmask = 0;
    vector<wstring> attrs;
    vector<wstring> values;

    wstring retval = split_elems(fs, attrs, values);
    systemd_timer_attr_func attr_method;

    for (auto i = 0; i < attrs.size(); i++) {
        attr_method = SystemD_Timer_Attribute_Map[attrs[i]];
        if (attr_method) {
            (this->*attr_method)(attrs[i], values[i], attr_bitmask);
        }
        else {
            SystemCtlLog::msg << L"attribute not recognised: " << attrs[i].c_str();
            SystemCtlLog::Warning();
        }
    }

    return retval;
}


boolean 
SystemDTimer::Enable(boolean block)

{
    wstring unitname = this->unit;
    SystemCtlLog::msg << L"SystemDTimer::Enable" << this->name;
    SystemCtlLog::Debug();


    wstring service_unit_path = SystemDUnitPool::UNIT_DIRECTORY_PATH+L"\\"+unitname; // We only look for service
                                                                            //  unit files in the top level directory
    class SystemDUnit *unit = SystemDUnitPool::FindUnit(unitname);
    if (!unit) {
        unit = SystemDUnitPool::ReadServiceUnit(unitname, service_unit_path);
        if (!unit) {
            // Complain and exit
            SystemCtlLog::msg << L"Failed to load unit from timer: Unit file " << service_unit_path.c_str() << L"is invalid\n";
            SystemCtlLog::Error();
            return -1;       
        }
    }
    unit->Enable(block);

    SystemDUnit::Enable(block);

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

boolean SystemDTimer::Disable(boolean block)

{
    wstring servicename = this->name;
    SystemCtlLog::msg << L"SystemDTimer::Enable is a stub " << this->name;
    SystemCtlLog::Debug();

    return true;
}


/*
 *   A complication of systemctl kill is that windows doesn't have signals.
 *   Because of that we must try to divine the purpose of the signal and expected result.
 *   We currently only recognise sigkill and sigterm. SigTerm we send a Console Control Event to 
 *   the process. SigKill we just kill the process
 */
boolean SystemDTimer::Kill(int action, int killtarget, boolean block)

{
    wstring servicename = this->name;
    SystemCtlLog::msg << L"SystemDTimer::Kill is a stub " << this->name;
    SystemCtlLog::Debug();
    
    return true;
}


boolean SystemDTimer::Mask(boolean block)

{
    // Mask unregisters the service from the service manager, then deletes the service unit 
    // from the active directory.
    wstring servicename = this->name;
    SystemCtlLog::msg << L"SystemDTimer::Mask is a stub " << this->name ;
    SystemCtlLog::Debug();
    std::wstring filepath_W = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH + L"\\" + servicename;
    std::string filepath_A = std::string(filepath_W.begin(), filepath_W.end());
    // Delete the file
    this->UnregisterService();
    std::remove(filepath_A.c_str());


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

boolean SystemDTimer::Unmask(boolean block)

{
    wchar_t * buffer;

    wstring servicename = this->name;
    wifstream checkfs(SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH + L"\\" + servicename);
    if (checkfs.is_open()) {
        checkfs.close();
        return true;
    }
    else {

        // If there is no file in the active directory, we copy one over, then register.

        wstring service_unit_path = SystemDUnitPool::UNIT_DIRECTORY_PATH + L"\\" + servicename;
        wstring service_unit_path2 = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH + L"\\" + servicename;

        // Find the unit in the unit library
        wifstream fs(service_unit_path, std::fstream::in);
        if (!fs.is_open()) {
            SystemCtlLog::msg << "No service unit " << servicename.c_str() << "Found in unit library";
            SystemCtlLog::Error();
            return false;
        }
        fs.close();
        BOOL try_copy = CopyFileW(service_unit_path.c_str(), service_unit_path2.c_str(), false);
        if (try_copy != TRUE) {
            SystemCtlLog::msg << L"Copy from : " << service_unit_path.c_str() << std::endl << L" to : "
                << service_unit_path2.c_str() << std::endl << L"with result: " << try_copy << std::endl
                << L"And last error: " << GetLastError() << std::endl;
            SystemCtlLog::Error();
        }

        return true;
    }
    return false;
}


void SystemDTimer::ShowService()

{
    SystemCtlLog::msg << L"SystemDTimer::ShowService  is a stub " << this->name;
    SystemCtlLog::Debug();
}

boolean
SystemDTimer::attr_on_active_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    if (attr_value.compare(L"infinity") == 0 ) {
        this->on_active_sec = DBL_MAX;
    }
    else {
        try {
        size_t toklen = 0;
            this->on_active_sec = std::stod(attr_value, &toklen);
            if (toklen < attr_value.length()) {
                double millis = NAN;
    
                if (!ParseDuration(attr_value, millis) ) {
                    SystemCtlLog::msg << "on_active_sec invalid value : " << attr_value.c_str() ;
                    SystemCtlLog::Warning();
                }
                else {
                        this->on_active_sec = millis*0.001;
                }
            }
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "on_active_sec invalid value : " << attr_value.c_str() ;
                SystemCtlLog::Warning();
            }
            else {
                this->on_active_sec = millis*0.001;
            }
        }
    }
    return true;
}

boolean
SystemDTimer::attr_on_boot_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    if (attr_value.compare(L"infinity") == 0 ) {
        this->on_boot_sec = DBL_MAX;
    }
    else {
        try {
        size_t toklen = 0;
            this->on_boot_sec = std::stod(attr_value, &toklen);
            if (toklen < attr_value.length()) {
                double millis = NAN;
    
                if (!ParseDuration(attr_value, millis) ) {
                    SystemCtlLog::msg << "on_boot_sec invalid value : " << attr_value.c_str() ;
                    SystemCtlLog::Warning();
                }
                else {
                        this->on_boot_sec = millis*0.001;
                }
            }
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "on_boot_sec invalid value : " << attr_value.c_str() ;
                SystemCtlLog::Warning();
            }
            else {
                this->on_boot_sec = millis*0.001;
            }
        }
    }
    return true;
}

boolean
SystemDTimer::attr_on_startup_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    if (attr_value.compare(L"infinity") == 0 ) {
        this->on_startup_sec = DBL_MAX;
    }
    else {
        try {
        size_t toklen = 0;
            this->on_startup_sec = std::stod(attr_value, &toklen);
            if (toklen < attr_value.length()) {
                double millis = NAN;
    
                if (!ParseDuration(attr_value, millis) ) {
                    SystemCtlLog::msg << "on_startup_sec invalid value : " << attr_value.c_str() ;
                    SystemCtlLog::Warning();
                }
                else {
                        this->on_startup_sec = millis*0.001;
                }
            }
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "on_startup_sec invalid value : " << attr_value.c_str() ;
                SystemCtlLog::Warning();
            }
            else {
                this->on_startup_sec = millis*0.001;
            }
        }
    }
    return true;
}

boolean
SystemDTimer::attr_on_unit_active_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    if (attr_value.compare(L"infinity") == 0 ) {
        this->on_unit_active_sec = DBL_MAX;
    }
    else {
        try {
        size_t toklen = 0;
            this->on_unit_active_sec = std::stod(attr_value, &toklen);
            if (toklen < attr_value.length()) {
                double millis = NAN;
    
                if (!ParseDuration(attr_value, millis) ) {
                    SystemCtlLog::msg << "on_unit_active_sec invalid value : " << attr_value.c_str() ;
                    SystemCtlLog::Warning();
                }
                else {
                        this->on_unit_active_sec = millis*0.001;
                }
            }
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "on_unit_active_sec invalid value : " << attr_value.c_str() ;
                SystemCtlLog::Warning();
            }
            else {
                this->on_unit_active_sec = millis*0.001;
            }
        }
    }
    return true;
}

boolean
SystemDTimer::attr_on_unit_inactive_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    if (attr_value.compare(L"infinity") == 0 ) {
        this->on_unit_inactive_sec = DBL_MAX;
    }
    else {
        try {
        size_t toklen = 0;
            this->on_unit_inactive_sec = std::stod(attr_value, &toklen);
            if (toklen < attr_value.length()) {
                double millis = NAN;
    
                if (!ParseDuration(attr_value, millis) ) {
                    SystemCtlLog::msg << "on_unit_inactive invalid value : " << attr_value.c_str() ;
                    SystemCtlLog::Warning();
                }
                else {
                        this->on_unit_inactive_sec = millis*0.001;
                }
            }
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "on_unit_inactive invalid value : " << attr_value.c_str() ;
                SystemCtlLog::Warning();
            }
            else {
                this->on_unit_inactive_sec = millis*0.001;
            }
        }
    }
    return true;
}

boolean
SystemDTimer::attr_accuracy_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    if (attr_value.compare(L"infinity") == 0 ) {
        this->accuracy_sec = DBL_MAX;
    }
    else {
        try {
        size_t toklen = 0;
            this->accuracy_sec = std::stod(attr_value, &toklen);
            if (toklen < attr_value.length()) {
                double millis = NAN;
    
                if (!ParseDuration(attr_value, millis) ) {
                    SystemCtlLog::msg << "accuracy_sec invalid value : " << attr_value.c_str() ;
                    SystemCtlLog::Warning();
                }
                else {
                        this->accuracy_sec = millis*0.001;
                }
            }
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "accuracy_sec invalid value : " << attr_value.c_str() ;
                SystemCtlLog::Warning();
            }
            else {
                this->accuracy_sec = millis*0.001;
            }
        }
    }
    return true;
}

boolean
SystemDTimer::attr_randomize_delay_sec( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    if (attr_value.compare(L"infinity") == 0 ) {
        this->randomized_delay_sec = DBL_MAX;
    }
    else {
        try {
        size_t toklen = 0;
            this->randomized_delay_sec = std::stod(attr_value, &toklen);
            if (toklen < attr_value.length()) {
                double millis = NAN;
    
                if (!ParseDuration(attr_value, millis) ) {
                    SystemCtlLog::msg << "restart_sec invalid value : " << attr_value.c_str() ;
                    SystemCtlLog::Warning();
                }
                else {
                    this->randomized_delay_sec = millis*0.001;
                }
            }
        }
        catch (const std::exception &e) {
            double millis = NAN;

            if (!ParseDuration(attr_value, millis) ) {
                SystemCtlLog::msg << "restart_sec invalid value : " << attr_value.c_str() ;
                SystemCtlLog::Warning();
            }
            else {
                this->randomized_delay_sec = millis*0.001;
            }
        }
    }
    return true;
}

boolean
SystemDTimer::attr_unit( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    this->unit = attr_value;
    return true;
}

boolean
SystemDTimer::attr_persistent( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )
{
    if (attr_value.compare(L"yes") == 0 ||
        attr_value.compare(L"true") == 0 ) {
        this->persistent = true;
    }
    else if (attr_value.compare(L"no") == 0 ||
            attr_value.compare(L"false") == 0 ) {
        this->persistent = false;
    }
    else {
        this->persistent = false;
    }

    return true;
}


boolean
SystemDTimer::attr_wake_system( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )

{
    if (attr_value.compare(L"yes") == 0 ||
        attr_value.compare(L"true") == 0 ) {
        this->wake_system = true;
    }
    else if (attr_value.compare(L"no") == 0 ||
            attr_value.compare(L"false") == 0 ) {
        this->wake_system = false;
    }
    else {
        this->wake_system = false;
    }

    return true;
}

boolean
SystemDTimer::attr_remain_after_elapse( wstring attr_name, wstring attr_value, unsigned long &attr_bitmask )
{
    if (attr_value.compare(L"yes") == 0  ||
        attr_value.compare(L"true") == 0 ) {
        this->remain_after_elapse = true;
    }
    else if (attr_value.compare(L"no") == 0 ||
            attr_value.compare(L"false") == 0 ) {
        this->remain_after_elapse = false;
    }
    else {
        this->remain_after_elapse = false;
    }

    return true;
}



std::map< std::wstring, SystemDTimer::systemd_timer_attr_func> SystemDTimer::SystemD_Timer_Attribute_Map = {
        { L"OnActiveSec",       &SystemDTimer::attr_on_active_sec },
        { L"OnBootSec",         &SystemDTimer::attr_on_boot_sec },
        { L"OnStartupSec",      &SystemDTimer::attr_on_startup_sec },
        { L"OnUnitActiveSec",   &SystemDTimer::attr_on_unit_active_sec },
        { L"OnUnitInactiveSec", &SystemDTimer::attr_on_unit_inactive_sec },
        { L"AccuracySec",       &SystemDTimer::attr_accuracy_sec },
        { L"RandomizedDelaySec", &SystemDTimer::attr_randomize_delay_sec },
        { L"Unit",               &SystemDTimer::attr_unit },
        { L"Persistent",         &SystemDTimer::attr_persistent },
        { L"WakeSystem",         &SystemDTimer::attr_wake_system },
        { L"RemainAfterElapse",  &SystemDTimer::attr_remain_after_elapse }
};
