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
#include "windows.h"
#include "wincred.h"
#include "winsvc.h"
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <ios>
#include <LsaLookup.h>
#include <ntsecapi.h>
#include "service_unit.h"

// relevant status from ntstatus.h
#define STATUS_NO_SUCH_FILE 0xc00000f
#define STATUS_SUCCESS      0x0

using namespace std;

wstring SystemDUnit::SERVICE_WRAPPER = L"systemd-exec.exe";
wstring SystemDUnitPool::SERVICE_WRAPPER_PATH;

boolean SystemDUnit::StartService(boolean blocking)

{
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << L"failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_ALL_ACCESS);
    if (!hsvc) {
        SystemCtlLog::msg << L"In StartService(" << this->name << "): OpenService failed " << GetLastError();
        SystemCtlLog::Error();
        CloseServiceHandle(hsc);
        return false;
    }

    if (!StartServiceW(hsvc, 0, NULL)) {
        DWORD errcode = GetLastError();

        switch(errcode) {
        case ERROR_SERVICE_EXISTS:
            // The service already running is not an error
            SystemCtlLog::msg << L"In StartService(" << this->name  << "): StartService failed " << GetLastError();
            SystemCtlLog::Info();
            CloseServiceHandle(hsvc);
            CloseServiceHandle(hsc);
            return false;

        case ERROR_ACCESS_DENIED:
        case ERROR_SERVICE_LOGON_FAILED:

            // The user lacks the necessary privilege. Add it and retry once

            SystemCtlLog::msg << L"In StartService(" << this->name  << "): StartService failed to logon erno = " << GetLastError();
            SystemCtlLog::Info();
            CloseServiceHandle(hsvc); 
            CloseServiceHandle(hsc);
            if (!this->m_retry++ ) {
                CloseServiceHandle(hsvc);
                CloseServiceHandle(hsc);
                return  StartService(blocking);
            }
            CloseServiceHandle(hsvc);
            CloseServiceHandle(hsc);
            return false;

        default:
            SystemCtlLog::msg << L"In StartService(" << this->name  << "): StartService error =  " << errcode;
            SystemCtlLog::Warning();
            CloseServiceHandle(hsvc);
            CloseServiceHandle(hsc);
            return false;
            break;
        }
    }
    
    SystemCtlLog::msg << L"In StartService(" << this->name  << "): StartService running ";
    SystemCtlLog::Info();
    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);
    
    return true;
}

boolean SystemDUnit::StopService(boolean blocking)
{
    SC_HANDLE hsc = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << "failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_STOP);
    if (!hsvc) {
        SystemCtlLog::msg << L"In Stop service(" << this->name << "): OpenService failed " << GetLastError();
        SystemCtlLog::Error();
        CloseServiceHandle(hsc);
        return false;
    }

    SERVICE_STATUS status = { 0 };
    if (!ControlService(hsvc, SERVICE_CONTROL_STOP, &status)) {
        SystemCtlLog::msg << L"StopService(" << this->name << ") failed " << GetLastError();
        SystemCtlLog::Error();
        CloseServiceHandle(hsvc);
        CloseServiceHandle(hsc);
        return false;
    }
    
    SystemCtlLog::msg << L"StopService(" << this->name << ") in progress" ;
    SystemCtlLog::Info();
    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);
    
    return true;
}

boolean SystemDUnit::ReloadService(boolean blocking)
{
    return true;
}

boolean SystemDUnit::RestartService(boolean blocking)
{
    StopService(blocking);
    // WaitForStop
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << "failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_QUERY_STATUS);
    if (!hsvc)
    {   DWORD last_err = GetLastError();

        if (last_err == ERROR_SERVICE_DOES_NOT_EXIST ||
            last_err == ERROR_SERVICE_DISABLED ) {
            SystemCtlLog::msg << L"service " << this->name << " is not enabled ";
            SystemCtlLog::Error();
        }
        else {
            SystemCtlLog::msg << L" In IsEnabled error from OpenService " << last_err;
            SystemCtlLog::Error();
        }
        CloseServiceHandle(hsc);
        return false;
    }
    
    SERVICE_STATUS svc_stat = {0};
    do {
        for (int retries = 0; retries < 5; retries++ ) {
            if (QueryServiceStatus(hsvc, &svc_stat)) {
                SystemCtlLog::msg << L"Restart::QueryServiceStatus succeed status = " << svc_stat.dwCurrentState  ;
                SystemCtlLog::Debug();
                break;
            }
            SystemCtlLog::msg << L"QueryServiceStatus failed " << GetLastError();
            SystemCtlLog::Warning();
        }
        ::Sleep(500);
    } 
    while (svc_stat.dwCurrentState == SERVICE_RUNNING);

    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);

    return StartService(blocking);
}


boolean SystemDUnit::IsEnabled()
{
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << "failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_QUERY_STATUS);
    if (!hsvc)
    {   DWORD last_err = GetLastError();

        if (last_err == ERROR_SERVICE_DOES_NOT_EXIST ||
            last_err == ERROR_SERVICE_DISABLED ) {
            SystemCtlLog::msg << L"service " << this->name << " is not enabled ";
            SystemCtlLog::Debug();
        }
        else {
            SystemCtlLog::msg << L" In IsEnabled error from OpenService " << last_err;
            SystemCtlLog::Error();
        }
        CloseServiceHandle(hsc);
        return false;
    }
    
    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);
    return true;
}



boolean SystemDUnit::IsActive()
{
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << "failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_QUERY_STATUS);
    if (!hsvc)
    {   DWORD last_err = GetLastError();

        if (last_err == ERROR_SERVICE_DOES_NOT_EXIST ||
            last_err == ERROR_SERVICE_DISABLED ) {
            SystemCtlLog::msg << L"service " << this->name << " is not enabled ";
            SystemCtlLog::Info();
        }
        else {
            SystemCtlLog::msg << L" In IsEnabled error from OpenService " << last_err;
            SystemCtlLog::Error();
        }
        CloseServiceHandle(hsc);
        return false;
    }
    
    SERVICE_STATUS svc_stat = {0};

    for (int retries = 0; retries < 5; retries++ ) {
        if (QueryServiceStatus(hsvc, &svc_stat)) {
            SystemCtlLog::msg << L"IsActive::QueryServiceStatus succeed";
            SystemCtlLog::Debug();
            break;
        }
        SystemCtlLog::msg << L"QueryServiceStatus failed " << GetLastError();
        SystemCtlLog::Warning();
    }

    if (svc_stat.dwCurrentState == SERVICE_RUNNING) {
        CloseServiceHandle(hsvc);
        CloseServiceHandle(hsc);
        return true;
    }

    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);
    return false;
}



boolean SystemDUnit::IsFailed()
{
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << "failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_QUERY_STATUS);
    if (!hsvc)
    {   DWORD last_err = GetLastError();

        if (last_err == ERROR_SERVICE_DOES_NOT_EXIST ||
            last_err == ERROR_SERVICE_DISABLED ) {
            SystemCtlLog::msg << L"service " << this->name << " is not enabled ";
            SystemCtlLog::Debug();
        }
        else {
            SystemCtlLog::msg << L" In IsEnabled error from OpenService " << last_err;
            SystemCtlLog::Error();
        }
        CloseServiceHandle(hsc);
        return false;
    }
    
    SERVICE_STATUS svc_stat = {0};

    for (int retries = 0; retries < 5; retries++ ) {
        if (QueryServiceStatus(hsvc, &svc_stat)) {
            SystemCtlLog::msg << L"IsActive::QueryServiceStatus succeed";
            SystemCtlLog::Debug();
            break;
        }
        SystemCtlLog::msg << L"QueryServiceStatus failed " << GetLastError();
        SystemCtlLog::Warning();
    }

    if (svc_stat.dwCurrentState == SERVICE_STOPPED &&
        svc_stat.dwWin32ExitCode != 0) {
            CloseServiceHandle(hsvc);
            CloseServiceHandle(hsc);
            return true;
    }

    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);
    return false;
}

boolean SystemDUnit::CatUnit(wostream &wostr)

{
    wstring service_unit_path = SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH+L"\\"+this->name;
    
    // Find the unit in the unit library
    wifstream fs(service_unit_path, std::fstream::in | std::fstream::binary);
    if (!fs.is_open()) {
         SystemCtlLog::msg << L"No service unit " << this->name.c_str() << L"Found in unit library";
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

    wostr.write (buffer,length);

    return true;
}

void
SystemDUnit::RegisterServiceProperties()

{
    // Register properties for service 

}

boolean 
SystemDUnit::RegisterService(std::wstring unit )

{
    // We point at an absolute path based on the location of systemctl.exe.
    // This means that the systemd-exec.exe must be in the same binary
    // directory as the systemctl. 

    HMODULE hModule = GetModuleHandleW(NULL);
    wchar_t path[MAX_PATH];
    
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring wspath = path;
    int pathend = wspath.find_last_of(L'\\')+1;
    wspath = wspath.substr(0, pathend);

    std::wstring wservice_name         = this->name;
    std::wstring wservice_display_name = this->name;

    SystemCtlLog::msg << L"RegisterService: " << this->name;
    SystemCtlLog::Verbose();

    std::wstringstream wcmdline ;

    if (unit.empty()) {
        unit = wservice_name;
    }

    // wcmdline << wspath;
    // 
    wcmdline << SystemDUnitPool::SERVICE_WRAPPER_PATH.c_str();
    wcmdline << SERVICE_WRAPPER.c_str();
    wcmdline << L" ";
    wcmdline << L" --service-name ";
    wcmdline << unit.c_str();
    wcmdline << L" ";
    wcmdline << L" --service-unit ";
    wcmdline << SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH;
    wcmdline << "\\";
    wcmdline << unit.c_str();

    int wchar_needed = 0;
    for (auto dependent : this->start_dependencies) {
        wchar_needed += dependent->name.size()+1;
    }
    wchar_needed++; // For the trailing null

    SystemCtlLog::msg << L"start_dependencies.size(): " << this->start_dependencies.size();
    SystemCtlLog::Debug();
    SystemCtlLog::msg << L"dep buffer chars required: " << wchar_needed << std::endl; 
    SystemCtlLog::Debug();

    vector<wchar_t> dep_buffer(wchar_needed+10);
    wchar_t *bufp = dep_buffer.data();
    for (auto dependent : this->start_dependencies) {
        memcpy(bufp, dependent->name.c_str(), dependent->name.size()*sizeof(wchar_t));
        bufp += dependent->name.size();
        *bufp++ = L'\0';
    }
    *bufp++ = L'\0';

    wchar_t *pelem = dep_buffer.data();
    wchar_t *plimit = pelem+dep_buffer.max_size();
    while ( pelem < plimit ) {
        SystemCtlLog::msg << "dependent: " << pelem ;
        SystemCtlLog::Debug();
        pelem += wcslen(pelem);    
        pelem++;
        if (!*pelem) {
            SystemCtlLog::msg << L"end of dep list" ;
            SystemCtlLog::Debug();
            break;
        }
    }

    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << L"Could not open service manager win err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    wstring user_name_temp = this->user_name;
    SC_HANDLE hsvc = CreateServiceW( 
            hsc,                       // SCM database 
            wservice_name.c_str(),             // name of service 
            wservice_display_name.c_str(),     // service name to display 
            SERVICE_ALL_ACCESS,        // desired access 
            SERVICE_WIN32_OWN_PROCESS, // service type 
            SERVICE_AUTO_START,      // start type 
            SERVICE_ERROR_NORMAL,      // error control type 
            wcmdline.str().c_str(),    // path to service's binary 
            NULL,                      // no load ordering group 
            NULL,                      // no tag identifier 
            dep_buffer.data(),  // dependencies 
            user_name_temp.c_str(), //pcred? username.c_str(): NULL,  // LocalSystem account
            NULL); // pcred ? user_password.c_str() : NULL);   // no password

    if (hsvc == NULL) 
    {
        SystemCtlLog::msg << L"CreateService for: " << wservice_name.c_str() << L", failed with last error: " << GetLastError();
        SystemCtlLog::Error();
        SystemCtlLog::msg << L"With username:  " << user_name_temp.c_str();
        SystemCtlLog::Error();
        CloseServiceHandle(hsc);
        return false;
    }

    if (this->description.length() > 0) {
        SERVICE_DESCRIPTIONW sd = { 0 };
        sd.lpDescription = (wchar_t *)this->description.c_str();
        SystemCtlLog::msg << L"ChangeServiceConfig2W description= " << this->description.c_str();
        SystemCtlLog::Debug();
        if (!ChangeServiceConfig2W(hsvc, SERVICE_CONFIG_DESCRIPTION, &sd)) {
            SystemCtlLog::msg << L"ChangeServiceConfig2W failed " << GetLastError();
            SystemCtlLog::Debug();
        }
    }

    // We query the status to ensure that the service has actually been created.

    SERVICE_STATUS svc_stat = {0};

    for (int retries = 0; retries < 5; retries++ ) {
        if (QueryServiceStatus(hsvc, &svc_stat)) {
            SystemCtlLog::msg << L"QueryServiceStatus succeed";
            SystemCtlLog::Debug();
            break;
        }
        SystemCtlLog::msg << L"QueryServiceStatus failed " << GetLastError();
        SystemCtlLog::Debug();
    }

    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);

    return true;
}




//
boolean SystemDTimer::RegisterService( std::wstring unit )

{
    SystemDUnit::RegisterService();

    // Now reLgister the timer key and values

    HKEY hRunKey = NULL;
    LSTATUS status = ERROR_SUCCESS;

    std::wstring subkey(L"SYSTEM\\CurrentControlSet\\Services\\");
    subkey.append(this->name);
    subkey.append(L"\\timer");

    SystemCtlLog::msg << L"create registry key \\HKLM\\" << subkey;
    SystemCtlLog::Debug();

    status = RegCreateKeyW(HKEY_LOCAL_MACHINE, subkey.c_str(),  &hRunKey);
    if (status != ERROR_SUCCESS) {
        SystemCtlLog::msg << L"could not create registry key \\HKLM\\" << subkey << "status = " << status;
        SystemCtlLog::Error();
        return false;
    }

    SystemCtlLog::msg << L"set registry value \\HKLM\\" << subkey << "\\unit";
    SystemCtlLog::Debug();

    status = RegSetValueExW( hRunKey, L"unit", 0, REG_SZ, (const BYTE*) this->unit.c_str(), this->unit.length()*sizeof(wchar_t));
    if (status != ERROR_SUCCESS) {
        SystemCtlLog::msg << L"RegisterTimerService: could not create registry value \\HKLM\\" << subkey << "\\unit status = " << status;
        SystemCtlLog::Error();
        RegCloseKey(hRunKey);
        return false;
    }

    if ( !isnan(on_active_sec)) {
        DWORD millis = (DWORD)(on_active_sec*1000.0);

        SystemCtlLog::msg << L"set registry value \\HKLM\\" << subkey << "\\on_active_millis";
        SystemCtlLog::Debug();

        status = RegSetValueExW( hRunKey, L"on_active_millis", 0, REG_DWORD, (const BYTE*) &millis, sizeof(DWORD));
        if (status != ERROR_SUCCESS) {
            SystemCtlLog::msg << L"RegisterTimerService: could not create registry value \\HKLM\\" << subkey << "\\on_active_millis status = " << status;
            SystemCtlLog::Error();
            RegCloseKey(hRunKey);
            return false;
        }
    }

    if ( !isnan(on_boot_sec)) {
        DWORD millis = (DWORD)(on_boot_sec*1000.0);

        SystemCtlLog::msg << L"set registry value \\HKLM\\" << subkey << "\\on_boot_millis";
        SystemCtlLog::Debug();

        status = RegSetValueExW( hRunKey, L"on_boot_millis", 0, REG_DWORD, (const BYTE*) &millis, sizeof(DWORD));
        if (status != ERROR_SUCCESS) {
            SystemCtlLog::msg << L"RegisterMainPID: could not create registry value \\HKLM\\" << subkey << "\\on_boot_millis status = " << status;
            SystemCtlLog::Error();
            RegCloseKey(hRunKey);
            return false;
        }
    }

    if ( !isnan(on_startup_sec)) {
        DWORD millis = (DWORD)(on_startup_sec*1000.0);

        SystemCtlLog::msg << L"set registry value \\HKLM\\" << subkey << "\\on_startup_millis";
        SystemCtlLog::Debug();

        status = RegSetValueExW( hRunKey, L"on_startup_millis", 0, REG_DWORD, (const BYTE*) &millis, sizeof(DWORD));
        if (status != ERROR_SUCCESS) {
            SystemCtlLog::msg << L"RegisterMainPID: could not create registry value \\HKLM\\" << subkey << "\\on_startup_millis status = " << status;
            SystemCtlLog::Error();
            RegCloseKey(hRunKey);
            return false;
        }
    }

    if ( !isnan(on_unit_active_sec)) {
        DWORD millis = (DWORD)(on_unit_active_sec*1000.0);

        SystemCtlLog::msg << L"set registry value \\HKLM\\" << subkey << "\\on_unit_active_millis";
        SystemCtlLog::Debug();

        status = RegSetValueExW( hRunKey, L"on_unit_active_millis", 0, REG_DWORD, (const BYTE*) &millis, sizeof(DWORD));
        if (status != ERROR_SUCCESS) {
            SystemCtlLog::msg << L"RegisterMainPID: could not create registry value \\HKLM\\" << subkey << "\\on_unit_active_millis status = " << status;
            SystemCtlLog::Error();
            RegCloseKey(hRunKey);
            return false;
        }
    }

    if ( !isnan(on_unit_inactive_sec)) {
        DWORD millis = (DWORD)on_unit_inactive_sec;

        SystemCtlLog::msg << L"set registry value \\HKLM\\" << subkey << "\\on_unit_inactive_millis";
        SystemCtlLog::Debug();

        status = RegSetValueExW( hRunKey, L"on_unit_inactive_millis", 0, REG_DWORD, (const BYTE*) &millis, sizeof(DWORD));
        if (status != ERROR_SUCCESS) {
            SystemCtlLog::msg << L"RegisterMainPID: could not create registry value \\HKLM\\" << subkey << "\\on_unit_inactive_millis status = " << status;
            SystemCtlLog::Error();
            RegCloseKey(hRunKey);
            return false;
        }
    }

    if ( !isnan(accuracy_sec)) {
        DWORD millis = (DWORD)(accuracy_sec*1000.0);

        SystemCtlLog::msg << L"set registry value \\HKLM\\" << subkey << "\\accuracy_millis";
        SystemCtlLog::Debug();

        status = RegSetValueExW( hRunKey, L"accuracy_millis", 0, REG_DWORD, (const BYTE*) &millis, sizeof(DWORD));
        if (status != ERROR_SUCCESS) {
            SystemCtlLog::msg << L"RegisterMainPID: could not create registry value \\HKLM\\" << subkey << "\\accuracy_millis status = " << status;
            SystemCtlLog::Error();
            RegCloseKey(hRunKey);
            return false;
        }
    }

    if ( !isnan(randomized_delay_sec)) {
        DWORD millis = (DWORD)(randomized_delay_sec*1000.0);

        SystemCtlLog::msg << L"set registry value \\HKLM\\" << subkey << "\\randomized_delay_millis";
        SystemCtlLog::Debug();

        status = RegSetValueExW( hRunKey, L"randomized_delay_millis", 0, REG_DWORD, (const BYTE*) &millis, sizeof(DWORD));
        if (status != ERROR_SUCCESS) {
            SystemCtlLog::msg << L"RegisterMainPID: could not create registry value \\HKLM\\" << subkey << "\\randomized_delay_millis status = " << status;
            SystemCtlLog::Error();
            RegCloseKey(hRunKey);
            return false;
        }
    }

    status = RegCloseKey(hRunKey);
    if (status != ERROR_SUCCESS) {
        SystemCtlLog::msg << L"RegisterMainPID: could not close registry key \\HKLM\\" << subkey << " status = " << status << std::endl;
        SystemCtlLog::Error();
        return false;
    }

    return true;
}

boolean SystemDUnit::UnregisterService()
{
    SystemCtlLog::msg << L"Trying to delete: " << this->name.c_str();
    SystemCtlLog::Debug();
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << L"failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_ALL_ACCESS);
    if (!hsvc) {
        SystemCtlLog::msg << L"In unregister: OpenService " << this->name << L" failed " << GetLastError();
        SystemCtlLog::Error();
        CloseServiceHandle(hsc);
        return false;
    }

    if (!DeleteService(hsvc)) {
        SystemCtlLog::msg << L"In unregister: DeleteService " << this->name << " failed " << GetLastError();
        SystemCtlLog::Error();
        CloseServiceHandle(hsvc);
        CloseServiceHandle(hsc);
        return false;
    }
    SystemCtlLog::msg << L"DeleteService was ok for: " << this->name.c_str();
    SystemCtlLog::Debug();
    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);
    return true;
}

boolean SystemDUnit::CheckForRequisites()
{
    return true;
}

boolean SystemDUnit::WaitForAfters()
{
    return true;
}

