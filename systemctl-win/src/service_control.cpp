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

static void
GetUserCreds(wstring &username, wstring &user_password)

{
    PCREDENTIALW pcred = NULL;

    // First, we try the cred mgr
    { //--- RETRIEVE user credentials. We need to have credentials specified for the service user otherwise we are
      //    LocalSystem which is a bit too restrictive to be able to set stuff up.

        BOOL ok = ::CredReadW (L"dcos/app", CRED_TYPE_GENERIC, 0, &pcred);
        if (ok) {
            user_password = wstring((wchar_t*)pcred->CredentialBlob, pcred->CredentialBlobSize / sizeof(wchar_t));
            username = wstring(pcred->UserName); // L"wp128869010\\azureuser"; // 
            // SystemCtlLog::msg << L"Read username = " << username << " password= " << user_password;
            // SystemCtlLog::Debug();
            ::CredFree (pcred);
            return;
        }
        else {
            SystemCtlLog::msg << L"CredRead() failed - errno -  fallback to env " << GetLastError();
            SystemCtlLog::Warning();
        }
    }

    // Not in the cred manager, we try environment variables

    DWORD buff_size = 0;
    vector<wchar_t> buf;
    DWORD status = 0;
    try {
        buff_size = GetEnvironmentVariableW(L"SYSTEMD_SERVICE_USERNAME", NULL, 0);
        if (!buff_size) {
            // Throw something
            throw std::exception("env var SYSTEMD_SERVICE_USERNAME not present in AddUserServiceLogonPrivilege");
        }
        buf = vector<wchar_t>(buff_size);
        status = GetEnvironmentVariableW(L"SYSTEMD_SERVICE_USERNAME", buf.data(), buff_size);
        if (!status) {
            // Throw something
            throw std::exception("env var SYSTEMD_SERVICE_USERNAME not present in AddUserServiceLogonPrivilege*2");
        }
        username = buf.data();
    }
    catch( std::exception &e ) {
        string cmsg = e.what();
        SystemCtlLog::msg << wstring(cmsg.begin(), cmsg.end());
        SystemCtlLog::Error();
        
        // No env var there. We fall back and try to use what we have
        buff_size = GetEnvironmentVariableW(L"USERDOMAIN", buf.data(), buff_size);
        if (!buff_size) {
             // Throw something
            throw std::exception("env var USERDOMAIN not present in AddUserServiceLogonPrivilege");
        }
        buf = vector<wchar_t>(buff_size);
        status = GetEnvironmentVariableW(L"USERDOMAIN", buf.data(), buff_size);
        username = buf.data();
        buff_size = GetEnvironmentVariableW(L"USERNAME", buf.data(), buff_size);
        if (!buff_size) {
             // Throw something
            throw std::exception("env var USERNAME not present in AddUserServiceLogonPrivilege*2");
        }
        buf = vector<wchar_t>(buff_size);
        status = GetEnvironmentVariableW(L"USERNAME", buf.data(), buff_size);
        username.append(L"\\");
        username.append(buf.data());
    }

    try {
        buff_size = GetEnvironmentVariableW(L"SYSTEMD_SERVICE_PASSWORD", NULL, 0);
        if (!buff_size) {
            // Throw something
            throw std::exception("env var SYSTEMD_SERVICE_PASSWORD not present in AddUserServiceLogonPrivilege");
        }
        buf = vector<wchar_t>(buff_size);
        status = GetEnvironmentVariableW(L"SYSTEMD_SERVICE_PASSWORD", buf.data(), buff_size);
        if (!status) {
            // Throw something
            throw std::exception("env var SYSTEMD_SERVICE_PASSWORD not present in AddUserServiceLogonPrivilege*2");
        }
        user_password = buf.data();
    }
    catch( std::exception &e ) {
        string cmsg = e.what();
        SystemCtlLog::msg << wstring(cmsg.begin(), cmsg.end());
        SystemCtlLog::Error();
        user_password = L""; // It is possible we actually don't have a password for this service account if it is 
                             // a managed service account
    }
}


void
SystemDUnit::AddUserServiceLogonPrivilege()

{
    wstring username;
    wstring password; // ignored 

    GetUserCreds(username, password);
 
    // SystemCtlLog::msg << L"username = " << username << " password = " << password << std::endl; 
    // SystemCtlLog::Debug();

    // Get the sid
    SID *psid;
    SID_NAME_USE nameuse;
    DWORD sid_size = 0;
    DWORD domain_size = 0;
    // Get sizes
    SystemCtlLog::msg << L"ADD the SERVICELOGON PRIVILEGE";
    SystemCtlLog::Info();
    (void)LookupAccountNameW(  NULL, // local machine
                           username.c_str(),  // Account name
                           NULL,   // sid ptr
                           &sid_size, // sizeof sid
                           NULL,   // referenced domain
                           &domain_size,
                           &nameuse );
    // ignore the return
    SystemCtlLog::msg << L"p2 sid_size = " << sid_size << " domainlen = " << domain_size;
    SystemCtlLog::Debug();
    std::vector<char>sid_buff(sid_size);
    psid = (SID*)sid_buff.data();
    std::vector<wchar_t>refdomain(domain_size+1);
    if (!LookupAccountNameW(  NULL, // local machine
                           username.c_str(),  // Account name
                           psid,   // sid ptr
                           &sid_size, // sizeof sid
                           refdomain.data(),   // referenced domain
                           &domain_size,
                           &nameuse )) {
        DWORD err = GetLastError();
        SystemCtlLog::msg << L"LookupAccountName() failed in AddUserServiceLogonPrivilege - errno " << GetLastError() << L" sid size " << sid_size << " domain_size << " << domain_size;
        SystemCtlLog::Warning();
        return;
    }

    // Get the LSA_HANDLE
    LSA_OBJECT_ATTRIBUTES attrs = {0};
    LSA_HANDLE policy_h;
    DWORD status = LsaOpenPolicy(NULL, &attrs, POLICY_ALL_ACCESS, &policy_h);
    if (status) {
        SystemCtlLog::msg << L"LsaOpenPolicy() failed in AddUserServiceLogonPrivilege - errno " << status;
        SystemCtlLog::Warning();
        return;
    }

    // Check to see if the right is already there. We coiuld just set it but that assumes the 
    // underlying code will definitely tolerate that.  Rather not take that bet.

    static const std::wstring se_service_logon = L"SeServiceLogonRight";
    LSA_UNICODE_STRING *pprivs = NULL;
    unsigned long priv_count = 0;

    status = LsaEnumerateAccountRights( policy_h,
                                  psid,
                                  &pprivs,
                                  &priv_count);

    // status isn't so great for enum, because it can return several non zero values in normal operation.
    // so we just check for the result if it fails or succeeds. priv_count will be 0 if it fails. That is normal
    // if no privs are configured.
    for (unsigned long i = 0; i < priv_count; i++ ) {
        if (pprivs && pprivs[i].Buffer) {
            if (se_service_logon.compare(0, pprivs[i].Length, pprivs[i].Buffer) == 0) {
                SystemCtlLog::msg << L"Service Logon right already present";
                SystemCtlLog::Debug();
                LsaFreeMemory(pprivs);
                LsaClose(policy_h);
                return;
            }
        }
    }

    LsaFreeMemory(pprivs);

    LSA_UNICODE_STRING privs[1];
    privs[0].Length = se_service_logon.length()*sizeof(wchar_t);
    privs[0].MaximumLength = se_service_logon.max_size()*sizeof(wchar_t);
    privs[0].Buffer = (wchar_t *)(se_service_logon.c_str());

    status = LsaAddAccountRights( policy_h,
                                  psid,
                                  privs,
                                  1);
    if (status) {
        SystemCtlLog::msg << L"LsaAddAccountRights() failed in AddUserServiceLogonPrivilege - errno " << status;
        SystemCtlLog::Warning();
        LsaClose(policy_h);
        return;
    }

    LsaClose(policy_h);
    SystemCtlLog::msg << L"Service Logon right added";
    SystemCtlLog::Info();
}



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
            return false;

        case ERROR_ACCESS_DENIED:
        case ERROR_SERVICE_LOGON_FAILED:

            // The user lacks the necessary privelege. Add it and retry once

            SystemCtlLog::msg << L"In StartService(" << this->name  << "): StartService failed to logon erno = " << GetLastError();
            SystemCtlLog::Info();
            CloseServiceHandle(hsvc); 
            AddUserServiceLogonPrivilege();  // We do this unconditionally 
            if (!this->m_retry++ ) {
               return  StartService(blocking);
            }
            return false;

        default:
            SystemCtlLog::msg << L"In StartService(" << this->name  << "): StartService error =  " << errcode;
            SystemCtlLog::Warning();
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
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << "failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_ALL_ACCESS);
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
    StartService(blocking);
    return true;
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

    if (svc_stat.dwCurrentState == SERVICE_STOPPED) {
        if (svc_stat.dwWin32ExitCode != 0) {
            return true;
	    }
    }

    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);
    return false;
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

    AddUserServiceLogonPrivilege(); // Make sure the user can actually use the thing.

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

    wstring username;
    wstring user_password;

    GetUserCreds(username, user_password);

    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << L"Could not open service manager win err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

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
            username.c_str(), //pcred? username.c_str(): NULL,  // LocalSystem account 
            user_password.c_str()); // pcred ? user_password.c_str() : NULL);   // no password 
 
    if (hsvc == NULL) 
    {
        SystemCtlLog::msg << L"CreateService failed " << GetLastError();
        SystemCtlLog::Error();
        CloseServiceHandle(hsc);
        return false;
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
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        SystemCtlLog::msg << L"failed to open service manager, err = " << last_error;
        SystemCtlLog::Error();
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), DELETE);
    if (!hsvc) {
        SystemCtlLog::msg << L"In unregister: OpenService " << this->name << L" failed " << GetLastError();
        SystemCtlLog::Error();
        CloseServiceHandle(hsvc);
        return false;
    }

    if (!DeleteService(hsvc)) {
        SystemCtlLog::msg << L"In unregister: DeleteService " << this->name << " failed " << GetLastError();
        SystemCtlLog::Error();
        CloseServiceHandle(hsvc);
        return false;
    }
    
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

