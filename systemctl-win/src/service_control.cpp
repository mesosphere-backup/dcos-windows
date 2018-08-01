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
            // wcerr << L"Read username = " << username << " password= " << user_password << std::endl;
            ::CredFree (pcred);
            return;
        }
        else {
            wcerr << L"CredRead() failed - errno -  fallback to env " << GetLastError() << std::endl;
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
        wstring msg = wstring(cmsg.begin(), cmsg.end());
        wcerr << msg << std::endl;
        
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
        wstring msg = wstring(cmsg.begin(), cmsg.end());
        wcerr << msg << std::endl;
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
 
    // wcerr << L"username = " << username << " password = " << password << std::endl;

    // Get the sid
    SID *psid;
    SID_NAME_USE nameuse;
    DWORD sid_size = 0;
    DWORD domain_size = 0;
    // Get sizes
    wcerr << L"ADD the SERVICELOGON PRIVILEGE" << std::endl;
    (void)LookupAccountNameW(  NULL, // local machine
                           username.c_str(),  // Account name
                           NULL,   // sid ptr
                           &sid_size, // sizeof sid
                           NULL,   // referenced domain
                           &domain_size,
                           &nameuse );
    // ignore the return
    wcerr << L"p2 sid_size = " << sid_size << " domainlen = " << domain_size << std::endl;
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
        wcerr << L"LookupAccountName() failed in AddUserServiceLogonPrivilege - errno " << GetLastError() << L" sid size " << sid_size << " domain_size << " << domain_size << std::endl;
        return;
    }

    // Get the LSA_HANDLE
    LSA_OBJECT_ATTRIBUTES attrs = {0};
    LSA_HANDLE policy_h;
    DWORD status = LsaOpenPolicy(NULL, &attrs, POLICY_ALL_ACCESS, &policy_h);
    if (status) {
        wcerr << L"LsaOpenPolicy() failed in AddUserServiceLogonPrivilege - errno " << status << std::endl;
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
                wcerr << L"Service Logon right already present" << std::endl;
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
        wcerr << L"LsaAddAccountRights() failed in AddUserServiceLogonPrivilege - errno " << status << std::endl;
    LsaClose(policy_h);
        return;
    }

    LsaClose(policy_h);
    wcerr << L"Service Logon right added" << std::endl;
}



boolean SystemDUnit::StartService(boolean blocking)

{
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        wcerr << L"failed to open service manager, err = " << last_error << std::endl;
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_ALL_ACCESS);
    if (!hsvc) {
        wcerr << L"In StartService(" << this->name << "): OpenService failed " << GetLastError() << std::endl;
        CloseServiceHandle(hsc);
        return false;
    }

    if (!StartServiceW(hsvc, 0, NULL)) {
        DWORD errcode = GetLastError();

        switch(errcode) {
        case ERROR_SERVICE_EXISTS:
            // The service already running is not an error
            wcerr << L"In StartService(" << this->name  << "): StartService failed " << GetLastError() << std::endl;
            CloseServiceHandle(hsvc);
            return false;

        case ERROR_ACCESS_DENIED:
        case ERROR_SERVICE_LOGON_FAILED:

            // The user lacks the necessary privelege. Add it and retry once

            wcerr << L"In StartService(" << this->name  << "): StartService failed to logon erno = " << GetLastError() << std::endl;
            CloseServiceHandle(hsvc); 
            AddUserServiceLogonPrivilege();  // We do this unconditionally 
            if (!this->m_retry++ ) {
               return  StartService(blocking);
            }
            return false;

        default:
            wcerr << L"In StartService(" << this->name  << "): StartService error =  " << errcode << std::endl;
            return false;
            break;
        }
    }
    
    wcerr << L"In StartService(" << this->name  << "): StartService running " << std::endl;
    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);
    
    return true;
}

boolean SystemDUnit::StopService(boolean blocking)
{
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        wcerr << "failed to open service manager, err = " << last_error << std::endl;
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_ALL_ACCESS);
    if (!hsvc) {
        wcerr << L"In Stop service(" << this->name << "): OpenService failed " << GetLastError() << std::endl;
        CloseServiceHandle(hsc);
        return false;
    }

    SERVICE_STATUS status = { 0 };
    if (!ControlService(hsvc, SERVICE_CONTROL_STOP, &status)) {
        wcerr << L"StopService(" << this->name << ") failed " << GetLastError() << std::endl;
        CloseServiceHandle(hsvc);
        return false;
    }
    
    wcerr << L"StopService(" << this->name << ") in progress" << std::endl;
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
        wcerr << "failed to open service manager, err = " << last_error << std::endl;
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_QUERY_STATUS);
    if (!hsvc)
    {   DWORD last_err = GetLastError();

        if (last_err == ERROR_SERVICE_DOES_NOT_EXIST ||
            last_err == ERROR_SERVICE_DISABLED ) {
            wcerr << L"service " << this->name << " is not enabled " << std::endl;
        }
        else {
            wcerr << L" In IsEnabled error from OpenService " << last_err << std::endl;
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
        wcerr << "failed to open service manager, err = " << last_error << std::endl;
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_QUERY_STATUS);
    if (!hsvc)
    {   DWORD last_err = GetLastError();

        if (last_err == ERROR_SERVICE_DOES_NOT_EXIST ||
            last_err == ERROR_SERVICE_DISABLED ) {
            wcerr << L"service " << this->name << " is not enabled " << std::endl;
        }
        else {
            wcerr << L" In IsEnabled error from OpenService " << last_err << std::endl;
        }
        CloseServiceHandle(hsc);
        return false;
    }
    
    SERVICE_STATUS svc_stat = {0};

    for (int retries = 0; retries < 5; retries++ ) {
        if (QueryServiceStatus(hsvc, &svc_stat)) {
            wcerr << L"IsActive::QueryServiceStatus succeed" << std::endl; 
            break;
        }
        wcerr << L"QueryServiceStatus failed " << GetLastError() << std::endl; 
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
        wcerr << "failed to open service manager, err = " << last_error << std::endl;
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), SERVICE_QUERY_STATUS);
    if (!hsvc)
    {   DWORD last_err = GetLastError();

        if (last_err == ERROR_SERVICE_DOES_NOT_EXIST ||
            last_err == ERROR_SERVICE_DISABLED ) {
            wcerr << L"service " << this->name << " is not enabled " << std::endl;
        }
        else {
            wcerr << L" In IsEnabled error from OpenService " << last_err << std::endl;
        }
        CloseServiceHandle(hsc);
        return false;
    }
    
    SERVICE_STATUS svc_stat = {0};

    for (int retries = 0; retries < 5; retries++ ) {
        if (QueryServiceStatus(hsvc, &svc_stat)) {
            wcerr << L"IsActive::QueryServiceStatus succeed" << std::endl; 
            break;
        }
        wcerr << L"QueryServiceStatus failed " << GetLastError() << std::endl; 
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



boolean 
SystemDUnit::RegisterService()

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

wcerr << "RegisterService: " << this->name << std::endl;

    std::wstringstream wcmdline ;

    AddUserServiceLogonPrivilege(); // Make sure the user can actually use the thing.

   // wcmdline << wspath;
    wcmdline << SystemDUnitPool::SERVICE_WRAPPER_PATH.c_str();
    wcmdline << SERVICE_WRAPPER.c_str();
    wcmdline << L" ";
    wcmdline << L" --service-name ";
    wcmdline << wservice_name.c_str();
    wcmdline << L" ";
    wcmdline << L" --service-unit ";
    wcmdline << SystemDUnitPool::ACTIVE_UNIT_DIRECTORY_PATH;
    wcmdline << "\\";
    wcmdline << wservice_name.c_str();

    int wchar_needed = 0;
    for (auto dependent : this->start_dependencies) {
        wchar_needed += dependent->name.size()+1;
    }
    wchar_needed++; // For the trailing null

wcerr << "start_dependencies.size(): " << this->start_dependencies.size() << std::endl;
wcerr << "dep buffer chars required: " << wchar_needed << std::endl;

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
wcerr << "dependent: " << pelem << std::endl;
pelem += wcslen(pelem);    
pelem++;
if (!*pelem) {
wcerr << "end of dep list" << std::endl;
    break;
}
}

    wstring username;
    wstring user_password;

    GetUserCreds(username, user_password);

    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        wcerr << L"Could not open service manager win err = " << last_error << std::endl;
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
            wcerr << L"CreateService failed " << GetLastError() << std::endl; 
            CloseServiceHandle(hsc);
            return false;
        }
    
        // We query the status to ensure that the service has actually been created.

        SERVICE_STATUS svc_stat = {0};

        for (int retries = 0; retries < 5; retries++ ) {
            if (QueryServiceStatus(hsvc, &svc_stat)) {
                wcerr << L"QueryServiceStatus succeed" << std::endl; 
            break;
        }
        wcerr << L"QueryServiceStatus failed " << GetLastError() << std::endl; 
    }

    CloseServiceHandle(hsvc); 
    CloseServiceHandle(hsc);


    return true;
}


boolean SystemDUnit::UnregisterService()
{
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        wcerr << "failed to open service manager, err = " << last_error << std::endl;
        return false;
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, this->name.c_str(), DELETE);
    if (!hsvc) {
        wcerr << L"In unregister: OpenService " << this->name << " failed " << GetLastError() << std::endl;
        CloseServiceHandle(hsvc);
        return false;
    }

    if (!DeleteService(hsvc)) {
        wcerr << L"In unregister: DeleteService " << this->name << " failed " << GetLastError() << std::endl;
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

