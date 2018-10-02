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
#include "service_exec.h"
#include <windows.h>
#include <winreg.h>
#include <strsafe.h>
#include <direct.h>
#include <string.h>
#include <locale>
#include <codecvt>
#include <regex>
#include <chrono>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <boost/stacktrace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/join.hpp>
#include <TlHelp32.h>
#pragma endregion

#define MAX_WAIT_CHILD_PROC (5 * 1000)

using namespace std;

void CWrapperService::RegisterMainPID()
{
    HKEY hRunKey = NULL;
    LSTATUS status = ERROR_SUCCESS;

    std::wstring subkey(L"SYSTEM\\CurrentControlSet\\Services\\");
    subkey.append(m_name);
    subkey.append(L"\\run");
    *logfile << Debug() << L"create registry key \\HKLM\\" << subkey << std::endl;
    status = RegCreateKeyW(HKEY_LOCAL_MACHINE, subkey.c_str(),  &hRunKey);
    if (status != ERROR_SUCCESS) {
        *logfile << Error() << L"could not create registry key \\HKLM\\" << subkey << "status = " << status << std::endl;
        return;
    }

    *logfile << Debug() << L"set registry value \\HKLM\\" << subkey << "\\ExecStartPID" << std::endl;

    status = RegSetValueExW( hRunKey, L"ExecStartPID", 0, REG_DWORD, (const BYTE*) &m_ExecStartProcInfo.dwProcessId, sizeof(DWORD));
    if (status != ERROR_SUCCESS) {
        *logfile << Error() << L"RegisterMainPID: could not create registry value \\HKLM\\" << subkey << "\\ExecStartPID status = " << status << std::endl;
        RegCloseKey(hRunKey);
        return;
    }

    status = RegCloseKey(hRunKey);
    if (status != ERROR_SUCCESS) {
        *logfile << Error() << L"RegisterMainPID: could not close registry key \\HKLM\\" << subkey << " status = " << status << std::endl;
        return;
    }
}

void CWrapperService::DeregisterMainPID()
{
    HKEY hRunKey = NULL;
    LSTATUS status = ERROR_SUCCESS;

    std::wstring subkey(L"SYSTEM\\CurrentControlSet\\Services\\");
    subkey.append(m_name);
    subkey.append(L"\\run");
    RegOpenKeyW(HKEY_CURRENT_CONFIG, subkey.c_str(),  &hRunKey);
    if (status != ERROR_SUCCESS) {
        *logfile << Error() << L"DeregisterMainPID could not open registry key \\HKLM\\" << subkey << " status = " << status << std::endl;
        return;
    }

    RegDeleteValueW( hRunKey, L"ExecStartPID");
    if (status != ERROR_SUCCESS) {
        *logfile << Error() << L"DeregisterMainPID could not delete registry value \\HKLM\\" << subkey << "\\ExecStartPID status = " << status << std::endl;
        RegCloseKey(hRunKey);
        return;
    }
    
    status = RegCloseKey(hRunKey);
    if (status != ERROR_SUCCESS) {
        *logfile << Error() << L"DeregisterMainPID could not close registry key \\HKLM\\" << subkey << " status = " << status << std::endl;
        return;
    }
}


// Generates flags mask and removes special executable prefix characters
unsigned 
CWrapperService::ProcessSpecialCharacters(std::wstring &ws)

{ unsigned mask = 0;

    wchar_t spec_char = ws[0]; 
    while (spec_char) {
        switch(spec_char) {
    case L'@':
        mask |= EXECFLAG_ARG0;
            ws.erase(0, 1);
        spec_char = ws[0];
        break;
    case L'-':
        mask |= EXECFLAG_IGNORE_FAIL;
            ws.erase(0, 1);
        spec_char = ws[0];
        break;

    case L'+':
        mask |= EXECFLAG_FULL_PRIVELEGE;
            ws.erase(0, 1);
        spec_char = ws[0];
        if (spec_char == '!') {
            *logfile << Warning() << L"Illegal combination of special execuatble chars +, ! and !! in commandline " << ws << std::endl;
        }
        break;

    case L'!':
        if (ws[1] == L'!') {
            mask |= EXECFLAG_AMBIENT_PRIVELEGE;
                ws.erase(0, 2);
            }
        else {
            mask |= EXECFLAG_ELEVATE_PRIVELEGE;
                ws.erase(0, 2);
            }
        spec_char = ws[0];
        break;

    default:
         return mask;
    }
    }

    return mask;
}


CWrapperService::CWrapperService(struct CWrapperService::ServiceParams &params)
                                 : CServiceBase(params.szServiceName, 
                                                params.stdOut,
                                                params.fCanStop, 
                                                params.fCanShutdown,
                                                params.fCanPauseContinue)
{  PROCESS_INFORMATION blank_proc_info = {0};

    if (!params.execStartPre.empty()) {
        for (auto ws: params.execStartPre) {
            m_ExecStartPreFlags.push_back(ProcessSpecialCharacters(ws));
            wstring cmdline = params.szShellCmdPre;
            cmdline.append(ws);
            cmdline.append(params.szShellCmdPost);
            m_ExecStartPreCmdLine.push_back(cmdline);
            m_ExecStartPreProcInfo.push_back(blank_proc_info);
        }
    }

    if (!params.execStart.empty()) {
        m_ExecStartFlags = ProcessSpecialCharacters(params.execStart);
        m_ExecStartCmdLine = params.szShellCmdPre;
        m_ExecStartCmdLine.append(params.execStart);
        m_ExecStartCmdLine.append(params.szShellCmdPost);
        m_ExecStartProcInfo = blank_proc_info;
    }

    if (!params.execStartPost.empty()) {
        for (auto ws: params.execStartPost) {
        m_ExecStartPostFlags.push_back(ProcessSpecialCharacters(ws));
            wstring cmdline = params.szShellCmdPre;
            cmdline.append(ws);
            cmdline.append(params.szShellCmdPost);
            m_ExecStartPostCmdLine.push_back(cmdline);
            m_ExecStartPostProcInfo.push_back(blank_proc_info);
        }
    }

    if (!params.execStop.empty()) {
        m_ExecStopFlags = ProcessSpecialCharacters(params.execStop);
        m_ExecStopCmdLine = params.szShellCmdPre;
        m_ExecStopCmdLine.append(params.execStop);
        m_ExecStopCmdLine.append(params.szShellCmdPost);
        m_ExecStopProcInfo = blank_proc_info;
    }

    if (!params.execStopPost.empty()) {
        for (auto ws: params.execStopPost) {
        m_ExecStopPostFlags.push_back(ProcessSpecialCharacters(ws));
            wstring cmdline = params.szShellCmdPre;
            cmdline.append(ws);
            cmdline.append(params.szShellCmdPost);
            m_ExecStopPostCmdLine.push_back(cmdline);
            m_ExecStopPostProcInfo.push_back(blank_proc_info);
        }
    }

    if (!params.files_before.empty()) {
        m_FilesBefore = params.files_before;
    }

    if (!params.services_before.empty()) {
        m_ServicesBefore = params.services_before;
    }

    if (!params.files_after.empty()) {
        m_FilesAfter = params.files_after;
    }

    if (!params.services_after.empty()) {
        m_ServicesAfter = params.services_after;
    }

    if (!params.files_requisite.empty()) {
        m_Requisite_Files = params.files_requisite;
    }

    if (!params.services_requisite.empty()) {
        m_Requisite_Services = params.services_requisite;
    }

    *logfile << Debug() << L"check for envfiles " << std::endl;
    if (!params.environmentFiles.empty()) {
        m_EnvironmentFiles = params.environmentFiles;
        for( auto envf : m_EnvironmentFiles ) {
            *logfile << Info() << L"envfile " << envf << std::endl;
        }
    }

    *logfile << Debug() << L"check for envfilesps " << std::endl;
    if (!params.environmentFilesPS.empty()) {
        m_EnvironmentFilesPS = params.environmentFilesPS;
        for( auto envf : m_EnvironmentFilesPS ) {
            *logfile << Info() << L"envfiile " << envf << std::endl;
        }
    }

    if (!params.environmentVars.empty()) {
        m_EnvironmentVars = params.environmentVars;
    }

    if (!params.unitPath.empty()) {
        m_unitPath = params.unitPath;
    }
    else {
        // If not defined, etc expect it is the systemd active service diirectory
        m_unitPath = L"c:\\etc\\SystemD\\active\\";
    }

    m_ConditionArchitecture  = params.conditionArchitecture;
    m_ConditionVirtualization = params.conditionVirtualization;
    m_ConditionHost           = params.conditionHost;
    m_ConditionKernelCommandLine = params.conditionKernelCommandLine;
    m_ConditionKernelVersion     = params.conditionKernelVersion;
    m_ConditionSecurity    = params.conditionSecurity;
    m_ConditionCapability  = params.conditionCapability;
    m_ConditionACPower     = params.conditionACPower;
    m_ConditionNeedsUpdate = params.conditionNeedsUpdate;
    m_ConditionFirstBoot   = params.conditionFirstBoot;
    m_ConditionPathExists        = params.conditionPathExists;
    m_ConditionPathExistsGlob    = params.conditionPathExistsGlob;
    m_ConditionPathIsDirectory   = params.conditionPathIsDirectory;
    m_ConditionPathIsSymbolicLink = params.conditionPathIsSymbolicLink;
    m_ConditionPathIsMountPoint  = params.conditionPathIsMountPoint;
    m_ConditionPathIsReadWrite   = params.conditionPathIsReadWrite;
    m_ConditionDirectoryNotEmpty = params.conditionDirectoryNotEmpty;
    m_ConditionFileNotEmpty      = params.conditionFileNotEmpty;
    m_ConditionFileIsExecutable  = params.conditionFileIsExecutable;
    m_ConditionUser  = params.conditionUser;
    m_ConditionGroup = params.conditionGroup;
    m_ConditionControlGroupController = params.conditionControlGroupController;

    m_ServiceName = params.szServiceName;
    m_ServiceType = params.serviceType;
    m_RestartAction = params.restartAction;
    m_RestartMillis = params.restartMillis;
    m_TimeoutStopMillis = params.timeoutStopMillis;
    m_WorkingDirectory = params.workingDirectory;

    m_StdErr = params.stdErr;
    m_StdOut = params.stdOut;

    m_WaitForProcessThread = NULL;
    m_hServiceThread = NULL;
    m_dwServiceThreadId = 0;
    m_IsStopping = FALSE;

    this->GetServiceDependencies(); // This will fill the m_Dependencies list
}

CWrapperService::~CWrapperService(void)

{
    *logfile << Debug() << L"~CWrapperService destructor " << std::endl;

    if (m_ExecStartProcInfo.hProcess)
    {
        ::CloseHandle(m_ExecStartProcInfo.hProcess);
        m_ExecStartProcInfo.hProcess = NULL;
    }

    if (m_WaitForProcessThread)
    {
        ::CloseHandle(m_WaitForProcessThread);
        m_WaitForProcessThread = NULL;
    }
}

void
CWrapperService::GetServiceDependencies()

{
    DWORD bytes_needed = 0;

    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        wostringstream os;
        int last_error = GetLastError();
        os << L"WaitForDependents could not open service manager win err = " << last_error << std::endl;
    *logfile << Error() << os.str();
        throw ServiceManagerException(last_error, os.str().c_str());
    }

    SC_HANDLE hsvc = OpenServiceW(hsc, m_ServiceName.c_str(), GENERIC_READ);
    if (!hsvc) {
        wostringstream os;
        int last_error = GetLastError();
        os << L"WaitForDependents OpeService failed " << GetLastError() << std::endl;
    *logfile << Error() << os.str();
        CloseServiceHandle(hsc);
        throw ServiceManagerException(last_error, os.str().c_str());
    }

    (void)::QueryServiceConfigW( hsvc, NULL, 0, &bytes_needed);

    *logfile << Debug() << "bytes needed = " << bytes_needed << std::endl;

    vector<char> config_buff(bytes_needed);
    QUERY_SERVICE_CONFIGW *service_config = (QUERY_SERVICE_CONFIGW*)config_buff.data();

    if (!::QueryServiceConfigW( hsvc, service_config, bytes_needed, &bytes_needed) ) {
        wostringstream os;
        int last_error = GetLastError();
        os << L"WaitForDependents could not get config err = " << last_error << std::endl;
    *logfile << Error() << os.str();
        CloseServiceHandle(hsc);
        throw ServiceManagerException(last_error, os.str().c_str());
    }

    *logfile << Debug() << L"GetServiceDependencies = " << service_config->lpDependencies << std::endl;
    
    // If this is a single string we need to parse it
    wchar_t *pdep = service_config->lpDependencies;
    if (pdep) {
        while (*pdep ) {
        this->m_Dependencies.push_back(pdep);
        pdep += wcslen(pdep);
        pdep++; // Skip the null
            *logfile << Info() << L"dep = " << this->m_Dependencies.back() << std::endl;
        }
    // Should leave a null at the end
    }
}


static wchar_t g_EnvVarChars[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890_";

std::wstring CWrapperService::ResolveEnvVars(std::wstring arg)

{

    // Parse the string and look up env variables
    // 2do:

    return L"";
}


enum OUTPUT_TYPE CWrapperService::StrToOutputType( std::wstring val, std::wstring *path )

{
    if (val.compare(L"inherit") == 0) {
        return OUTPUT_TYPE_INHERIT;
    }
    else if (val.compare(L"null") == 0) {
        return OUTPUT_TYPE_NULL;
    }
    else if (val.compare(L"tty") == 0) {
        return OUTPUT_TYPE_TTY;
    }
    else if (val.compare(L"journal") == 0) {
        return OUTPUT_TYPE_JOURNAL;
    }
    else if (val.compare(L"syslog") == 0) {
        return OUTPUT_TYPE_SYSLOG;
    }
    else if (val.compare(L"kmsg") == 0) {
        return OUTPUT_TYPE_KMSG;
    }
    else if (val.compare(L"journal+console") == 0) {
        return OUTPUT_TYPE_JOURNAL_PLUS_CONSOLE;
    }
    else if (val.compare(L"syslog+console") == 0) {
        return OUTPUT_TYPE_SYSLOG_PLUS_CONSOLE;
    }
    else if (val.compare(L"kmsg+console") == 0) {
        return OUTPUT_TYPE_KMSG_PLUS_CONSOLE;
    }
    else if (val.compare(0, 5, L"file:") == 0) {
        if (path != NULL ) {
            *path = val.substr(0, val.find_first_of(':')+1);
        }
        return OUTPUT_TYPE_FILE;
    }
    else if (val.compare(L"socket") == 0) {
        return OUTPUT_TYPE_SOCKET;
    }
    else if (val.compare(0, 3, L"fd:") == 0) {
        if (path != NULL ) {
            *path = val.substr(0, val.find_first_of(':')+1);
        }
        return OUTPUT_TYPE_FD;
    }
    else {
        return OUTPUT_TYPE_INVALID;
    }
}



void CWrapperService::GetCurrentEnv()
{

    wchar_t *tmpEnv = ::GetEnvironmentStringsW();
    LPCWSTR envPair = (LPCWSTR)tmpEnv;
    while (envPair[0])
    {
        wregex rgx(L"^([^=]*)=(.*)$");
        wsmatch matches;
        wstring envPairStr = envPair;
        if (regex_search(envPairStr, matches, rgx))
        {
            auto name = matches[1].str();
            auto value = matches[2].str();

            // We uppercase all env var names because they are case insensitive in powershell
            std::transform(name.begin(), name.end(), name.begin(), ::toupper);
            m_Env[name] = value;
        }

        envPair = envPair + envPairStr.length() + 1;
    }
    ::FreeEnvironmentStrings(tmpEnv);
}


/*
 * The rule is that if the env file passed in ends with .ps1, we will go ahead and read it as powershell
 * any other extension, including none, we treat as bash syntax
 */
boolean CWrapperService::LoadEnvVarsFromFile(const wstring& file_path)
{
    boolean failure_ok = false;
    wstring path;

    path = file_path;
    if (file_path[0] == '-') {
        failure_ok = true;
        path.erase(0,1);
    }

    size_t ext_idx = path.find_last_of(L'.');
    wstring file_ext = ext_idx != std::string::npos? path.substr(ext_idx) : L"";

    *logfile << Debug() << L"envfile path = " << path << std::endl;

    if (file_ext.compare(L".ps1") == 0) {
        if (!LoadPShellEnvVarsFromFile(path)) {
            return failure_ok;
        }
        else {
            return true;
        }
    }
    else {
        wifstream inputFile(path);
        wstring line;
    
        if (inputFile.fail()) {
            return failure_ok;
        }

        while (getline(inputFile, line))
        {
            wregex rgx(L"^([^#][^=]*)=(.*)$");
            wsmatch matches;
            if (regex_search(line, matches, rgx))
            {
                auto name = boost::algorithm::trim_copy(matches[1].str());
                auto value = boost::algorithm::trim_copy(matches[2].str());
                if (value[0] == L'\"' || value[0] == L'\'') {
                    value.erase(value.length()-1, 1);
                    value.erase(0, 1);
                }
                m_Env[name] = value;

                // Now we need to look for powershell syntax environment variables embedded in the string
                size_t pos = 0;
                while (pos != std::string::npos) {
                    size_t env_var_idx = value.find(L"$", pos);
                    if (env_var_idx != std::string::npos) {
                        wstring env_var;
                        size_t var_start = env_var_idx+1; // length of $env:
                        size_t var_end = std::string::npos;
                        var_end = value.find_first_not_of(g_EnvVarChars, var_start);
                        if (var_end != std::string::npos) {
                            
                           env_var = value.substr(var_start, var_end-var_start);
                        }
                        else {
                           env_var = value.substr(var_start);
                        }
                        // We uppercase all env var names because they are case insensitive in powershell
                        std::transform(env_var.begin(), env_var.end(), env_var.begin(), ::toupper);
    
                        *logfile << Debug() << L"expand m_env[" << env_var << L"] = "  << m_Env[env_var] << std::endl;
    
                        value.replace(env_var_idx, env_var.length()+1, m_Env[env_var]);
                        pos = env_var_idx; // We dont have an env there any more, but the env_var could 
                                           // contain more env_var so we rescan
    
                        *logfile << Debug() << L"env_var = " << env_var << L" expanded value = " << value << std::endl;
                    }
                    else {
                        break;
                    }     
                }
    
                m_Env[name] = value;
                *logfile << Debug() << L"bash environment file key : " << name << " val \'" << value << L"\'" << std::endl;
            }
        }
    }
    return true;
}

boolean CWrapperService::LoadPShellEnvVarsFromFile(const wstring& file_path)
{
    boolean failure_ok = false;
    wstring path;

    path = file_path;
    if (file_path[0] == '-') {
        failure_ok = true;
        path.erase(0,1);
    }

    wifstream inputFile(path);
    wstring line;

    if (inputFile.fail()) {
        return failure_ok;
    }
    while (getline(inputFile, line))
    {
        wregex rgx(L"^\\s*\\$env:([^#=]*)=['\"](.*)['\"]$");
        wsmatch matches;
        if (regex_search(line, matches, rgx))
        {
            auto name = boost::algorithm::trim_copy(matches[1].str());
            auto value = boost::algorithm::trim_copy(matches[2].str());

            // Now we need to look for powershell syntax environment variables embedded in the string
            size_t pos = 0;
            while (pos != std::string::npos) {
                size_t env_var_idx = value.find(L"$env:", pos);
                if (env_var_idx != std::string::npos) {
                    wstring env_var;
                    size_t var_start = env_var_idx+5; // length of $env:
                    size_t var_end = std::string::npos;
                    var_end = value.find_first_not_of(g_EnvVarChars, var_start);
                    if (var_end != std::string::npos) {
                        
                       env_var = value.substr(var_start, var_end-var_start);
                    }
                    else {
                       env_var = value.substr(var_start);
                    }
                    // We uppercase all env var names because they are case insensitive in powershell
                    std::transform(env_var.begin(), env_var.end(), env_var.begin(), ::toupper);

                    *logfile << Debug() << L"expand m_env[" << env_var << L"] = "  << m_Env[env_var] << std::endl;

                    value.replace(env_var_idx, env_var.length()+5, m_Env[env_var]);
                    pos = env_var_idx; // We dont have an env there any more, but the env_var could 
                                       // contain more env_var so we rescan

                    *logfile << Debug() << L"env_var = " << env_var << L" expanded value = " << value << std::endl;
                }
                else {
                    break;
                }     
            }

            m_Env[name] = value;
            *logfile << L"PS environment file key = " << name << " val " << value << std::endl;
        }
    }
    return true;
}


void CWrapperService::StartProcess(LPCWSTR cmdLine, DWORD processFlags, PROCESS_INFORMATION &procInfo, bool waitForProcess, bool failOnError)

{
    STARTUPINFO startupInfo;

    memset(&startupInfo, 0, sizeof(startupInfo));

    memset(&procInfo, 0, sizeof(PROCESS_INFORMATION));
    startupInfo.cb = sizeof(startupInfo);
    if (m_StdOut->GetHandle() != INVALID_HANDLE_VALUE) {
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdOutput = m_StdOut->GetHandle();
        *logfile << Debug() << L"has stdout " << std::endl;

    }

    if (m_StdErr->GetHandle() != INVALID_HANDLE_VALUE) {
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
        startupInfo.hStdError  = m_StdErr->GetHandle();
        *logfile << Debug() << L"has stderr " << std::endl;
    }

    if (startupInfo.dwFlags &= STARTF_USESTDHANDLES) {
        startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    }

    DWORD dwCreationFlags = processFlags | CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT;

    // Read the environment every time we start, but read it once per start

    LPVOID lpEnv = NULL;
    if (!m_envBuf.empty()) {
        for ( wchar_t *tmpenv = (wchar_t*)m_envBuf.c_str();
            *tmpenv && tmpenv < (wchar_t*)m_envBuf.c_str()+m_envBuf.size();
            ) {
            wstring this_env = tmpenv;
            *logfile << Debug() << "at exec, env : " << this_env << std::endl;
            tmpenv+= wcslen(tmpenv);
            tmpenv++;
        }
        lpEnv = (LPVOID)m_envBuf.c_str();
    }
    
    // Setup the working directory. Expand any environment variables referenced

    wstring cwd;  // This variable manages the storage for the wchar_t * and is easy to work with. It 
                  // free the storafge at the end of the function. 
    wchar_t *pWorkingDirectory = NULL;
    if (!m_WorkingDirectory.empty()) {
            // TODO: Look up if this string needs to be expanded in an environment variable via ResolveEnvVars
            // For now assume that the user gave us a valid location.
            pWorkingDirectory = (wchar_t*)m_WorkingDirectory.c_str();
    }

    DWORD tempCmdLineCount = lstrlen(cmdLine) + 1;
    LPWSTR tempCmdLine = new WCHAR[tempCmdLineCount];  //Needed since CreateProcessW may change the contents of CmdLine
    wcscpy_s(tempCmdLine, tempCmdLineCount, cmdLine);


    *logfile << Verbose() << "create process " << cmdLine << std::endl;

    BOOL result = ::CreateProcessW(NULL, tempCmdLine, NULL, NULL, TRUE, dwCreationFlags,
        lpEnv, pWorkingDirectory, &startupInfo, &procInfo);

    delete[] tempCmdLine;

    if (!result)
    {
        DWORD err = GetLastError();
        wostringstream os;
        os << L"Error " << err << L" while spawning the process: " << cmdLine << std::endl;
        *logfile << os.str();
        string str = wstring_convert<codecvt_utf8<WCHAR>>().to_bytes(os.str());
        throw RestartException(err, str.c_str());
    }

    if(waitForProcess)
    {
        *logfile << Verbose() << "waitfor process " << cmdLine << std::endl;
        ::WaitForSingleObject(procInfo.hProcess, INFINITE);

        DWORD exitCode = 0;
        BOOL result = ::GetExitCodeProcess(procInfo.hProcess, &exitCode);
        ::CloseHandle(procInfo.hProcess);

        if (!result || exitCode)
        {
            wostringstream os;
            if (!result) {
                *logfile << L"GetExitCodeProcess failed";
            }
            else {
                *logfile << L"Command \"" << cmdLine << L"\" failed with exit code: " << exitCode;
            }

            string str = wstring_convert<codecvt_utf8<WCHAR>>().to_bytes(os.str());
            throw RestartException(exitCode, str.c_str());
        }
        *logfile << Verbose() << "process success " << cmdLine << std::endl;
    }

}

void CWrapperService::OnStart(DWORD dwArgc, LPWSTR *lpszArgv)

{
    boolean waitforfinish = true;

    // Are we a timer? 
    HKEY hRunKey = NULL;
    LSTATUS status = ERROR_SUCCESS;

    std::wstring subkey(L"SYSTEM\\CurrentControlSet\\Services\\");
    subkey.append(this->m_name);
    subkey.append(L"\\timer");

    *logfile << Debug() << L"open registry key \\HKLM\\" << subkey << std::endl;

    status = RegOpenKeyW(HKEY_LOCAL_MACHINE, subkey.c_str(),  &hRunKey);
    if (status == ERROR_SUCCESS) {
        this->m_IsTimer = true;
        *logfile << Info() << L"Starting timer: " << m_ServiceName << std::endl;
    
        if (m_hServiceThread != NULL ) {
            *logfile << Warning() << L"timer thread for service " << m_name << " is already running" << std::endl;
             // 
        }
        else {
    
            // Spawn Service Thread
            m_hServiceThread = CreateThread( 
                    NULL,               // default security attributes
                    1024*1024*64,       // use 128M stack size  
                    TimerThread,        // thread function name
                    this,               // argument to thread function 
                    0,                  // use default creation flags 
                    &m_dwServiceThreadId);   // returns the thread identifier 
        
            if (m_hServiceThread == NULL ) {
                 throw ::GetLastError();
            }
        }
    }
    else {

        this->m_IsTimer = false;
        if (status != ERROR_FILE_NOT_FOUND && status != ERROR_PATH_NOT_FOUND) {
            *logfile << Error() << L"could not find registry key \\HKLM\\" << subkey << "status = " << status << std::endl;
            // Unexpected behaviour...
            return;
        }

        *logfile << Debug() << L"could not find registry key \\HKLM\\" << subkey << L": is not a timer" << std::endl;

        SetServiceStatus(SERVICE_RUNNING);
        m_IsStopping = FALSE;
    
        *logfile << Info() << L"Starting service: " << m_ServiceName << std::endl;
    
        if (m_hServiceThread != NULL ) {
            *logfile << Warning() << L"service thread for service " << m_name << " is already running" << std::endl;
             // 
        }
        else {
    
            // Spawn Service Thread
            m_hServiceThread = CreateThread( 
                    NULL,                   // default security attributes
                    1024*1024*128,         // use 128M stack size  
                    ServiceThread,          // thread function name
                    this,                   // argument to thread function 
                    0,                      // use default creation flags 
                    &m_dwServiceThreadId);   // returns the thread identifier 
        
            if (m_hServiceThread == NULL ) {
                 throw ::GetLastError();
            }
        }
        *logfile << Info() << L"exit service OnStart: " << std::endl;
    }

}



// OnStart will spawn a timer thread or a service thread depending on the presence of the key 
// \\HKLM\CurrentControlSet\Services\<service>\timer.  If that key is present we spawn a timer thread, 
// which discovers the timer attributes from the registry, and executes the service thread according to 
// their specification. If the attributes on_unit_active_millis or on_unit_inactive_millis are set, this is a 
// recurring timer. Otherwise we wait as required and spawn the service in the same manner otherwise as
// OnStart would have. 

DWORD WINAPI CWrapperService::TimerThread(LPVOID param)

{
    CWrapperService *self = (CWrapperService *)param;
    DWORD exitCode = 0;
    boolean done  = false;

    int64_t since_system_start_millis = GetTickCount64();
    int64_t since_timer_start_millis    = 0;
    int64_t since_service_active_millis = 0;
    int64_t since_service_inactive_millis = 0;

    DWORD on_active_millis        = 0;
    DWORD on_boot_millis          = 0;
    DWORD on_startup_millis       = 0;
    DWORD on_unit_active_millis   = 0;
    DWORD on_unit_inactive_millis = 0;
    DWORD accuracy_millis         = 60000; //Default accuracy for timer
    DWORD randomized_delay_millis = 0;

    wchar_t unit_name[256] = { 0 };
    DWORD unit_name_size = 256;

    HKEY hRunKey = NULL;
    LSTATUS status = ERROR_SUCCESS;

    std::wstring subkey(L"SYSTEM\\CurrentControlSet\\Services\\");
    subkey.append(self->m_name);
    subkey.append(L"\\timer");

    *logfile << Debug() << L"open registry key \\HKLM\\" << subkey << std::endl;

    status = RegOpenKeyW(HKEY_LOCAL_MACHINE, subkey.c_str(),  &hRunKey);
    if (status != ERROR_SUCCESS) {

       // This key is present in the registry or we would not have been called.

        *logfile << Error() << L"could not open registry key \\HKLM\\" << subkey << "status = " << status << std::endl;
        return status;
    }

    *logfile << Debug() << L"get registry value \\HKLM\\" << subkey << "\\unit" << std::endl;
    status = RegGetValueW( hRunKey, NULL, L"unit", RRF_RT_ANY, NULL, unit_name, &unit_name_size );
    if (status != ERROR_SUCCESS) {
        *logfile << Error() << L"\\HKLM\\" << subkey << "\\unit not read status = " << status << std::endl;
    }

    DWORD parm_size = sizeof(DWORD);
    DWORD parm_type = REG_DWORD;

    *logfile << Debug() << L"get registry value \\HKLM\\" << subkey << "\\on_active_millis" << std::endl;
    status = RegGetValueW( hRunKey, NULL, L"on_active_millis", RRF_RT_ANY, NULL, &on_active_millis, &parm_size );
    if (status != ERROR_SUCCESS) {
        *logfile << Debug() << L"\\HKLM\\" << subkey << L"\\on_active_millis not read status = " << status << std::endl;
    }

    *logfile << Debug() << L"get registry value \\HKLM\\" << subkey << "\\on_boot_millis" << std::endl;
    status = RegGetValueW( hRunKey, NULL, L"on_boot_millis", RRF_RT_ANY, NULL, &on_boot_millis, &parm_size );
    if (status != ERROR_SUCCESS) {
        *logfile << Debug() << L"\\HKLM\\" << subkey << "\\on_boot_millis not read status = " << status << std::endl;
    }

    *logfile << Debug() << L"get registry value \\HKLM\\" << subkey << "\\on_startup_millis" << std::endl;
    status = RegGetValueW( hRunKey, NULL, L"on_startup_millis", RRF_RT_ANY, NULL, &on_startup_millis, &parm_size );
    if (status != ERROR_SUCCESS) {
        *logfile << Debug() << L"\\HKLM\\" << subkey << "\\on_startup_millis not read status = " << status << std::endl;
    }

    *logfile << Debug() << L"get registry value \\HKLM\\" << subkey << "\\on_unit_active_millis" << std::endl;
    status = RegGetValueW( hRunKey, NULL, L"on_unit_active_millis", RRF_RT_ANY, NULL, &on_unit_active_millis, &parm_size );
    if (status != ERROR_SUCCESS) {
        *logfile << Debug() << L"\\HKLM\\" << subkey << "\\on_unit_active_millis not read status = " << status << std::endl;
    }

    *logfile << Debug() << L"get registry value \\HKLM\\" << subkey << "\\on_unit_inactive_millis" << std::endl;
    status = RegGetValueW( hRunKey, NULL, L"on_unit_inactive_millis", RRF_RT_ANY, NULL, &on_unit_inactive_millis, &parm_size );
    if (status != ERROR_SUCCESS) {
        *logfile << Debug() << L"\\HKLM\\" << subkey << "\\on_unit_inactive_millis not read status = " << status << std::endl;
    }

    *logfile << Debug() << L"get registry value \\HKLM\\" << subkey << "\\accuracy_millis" << std::endl;
    status = RegGetValueW( hRunKey, NULL, L"accuracy_millis", RRF_RT_ANY, NULL, &accuracy_millis, &parm_size );
    if (status != ERROR_SUCCESS) {
        *logfile << Debug() << L"\\HKLM\\" << subkey << "\\accuracy_millis not read status = " << status << std::endl;
    }

    *logfile << Debug() << L"get registry value \\HKLM\\" << subkey << "\\randomized_delay_millis" << std::endl;
    status = RegGetValueW( hRunKey, NULL, L"randomized_delay_millis", RRF_RT_ANY, NULL, &randomized_delay_millis, &parm_size );
    if (status != ERROR_SUCCESS) {
        *logfile << Debug() << L"\\HKLM\\" << subkey << "\\randomized_delay_millis not read status = " << status << std::endl;
    }

    status = RegCloseKey(hRunKey);
    if (status != ERROR_SUCCESS) {
        *logfile << Error() << L"StartTimer: could not close registry key \\HKLM\\" << subkey << " status = " << status << std::endl;
        return false;
    }

    // That out of the way, we execute the service unit.

    // On Active and On Boot are one time delays. 

    if (on_boot_millis > 0) {
        *logfile << Debug() << L"StartTimer start on boot sleep for " << on_boot_millis << L" millis" << std::endl;
        Sleep(on_boot_millis);
        *logfile << Debug() << L"StartTimer done with boot sleep" << std::endl;
    }

    if (on_active_millis > 0) {
        since_timer_start_millis = DWORD(GetTickCount64()-since_system_start_millis);
        
        DWORD wait_time = (DWORD)(on_active_millis - since_timer_start_millis);
        *logfile << Debug() << L"StartTimer start on active sleep for " << wait_time << L" millis" << std::endl;
        if (wait_time > 0) {
            Sleep(wait_time);
        }
    }

    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        *logfile << Error() << L"failed to open service manager, err = " << last_error << std::endl;
        return last_error;
    }


    SC_HANDLE hsvc = OpenServiceW(hsc, unit_name, SERVICE_ALL_ACCESS);
    if (!hsvc) {
        int last_error = GetLastError();
        *logfile << Error() << L"In StartService(" << unit_name << "): OpenService failed " << last_error << std::endl;
        CloseServiceHandle(hsc);
        return last_error;
    }

    self->SetServiceStatus(SERVICE_RUNNING);
    int64_t next_deadline = -1;
    do {
        done = true;   
        since_service_active_millis = GetTickCount64();
        if (on_unit_active_millis > 0) {
            next_deadline = GetTickCount64()+on_unit_active_millis;
            done = false;
        }

        time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        *logfile << Info() << L"Start service(" << unit_name  << ") at " << std::ctime(&now_time) << std::endl;
        if (!StartServiceW(hsvc, 0, NULL)) {
            DWORD errcode = GetLastError();
    
            switch(errcode) {
            case ERROR_SERVICE_EXISTS:
                // The service already running is not an error
                *logfile << Info() << L"In start timer service(" << unit_name  << "): StartService failed " << GetLastError() << std::endl;
                break;
    
            case ERROR_ACCESS_DENIED:
            case ERROR_SERVICE_LOGON_FAILED:
    
                // The user lacks the necessary privelege. Add it and retry once
    
                *logfile << Warning() << L"In StartService(" << unit_name  << "): StartService failed to logon erno = " 
                                      << GetLastError() << " account lacks privelege" << std::endl;

                break;
    
            default:
                *logfile << Warning() << "In StartService(" << unit_name  << "): StartService error =  " << errcode << std::endl;
                break;
            }
        }
        SERVICE_STATUS_PROCESS svc_status = {0};
        DWORD size_needed = sizeof(SERVICE_STATUS_PROCESS);
        while (!QueryServiceStatusEx(hsvc, SC_STATUS_PROCESS_INFO, (BYTE*)&svc_status, sizeof(SERVICE_STATUS_PROCESS), &size_needed)) {
           ::SleepEx(1000, true);
           *logfile << Debug() << L"service " << unit_name << L" not started. wait for 1000 millis" << std::endl;
        }
        HANDLE hSvcProc = OpenProcess( PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, svc_status.dwProcessId);
        if (hSvcProc == INVALID_HANDLE_VALUE) {
           *logfile << Error() << L"service " << unit_name << L" Could not get process handle" << std::endl;
        }

        *logfile << Debug() << L"service " << unit_name << L" started wait for " << (next_deadline-GetTickCount64()) << L" millis" << std::endl;

        // Wait for service inactive
        DWORD timeout = on_unit_active_millis? on_unit_active_millis : INFINITE;
        WaitForSingleObject(hSvcProc, timeout );

        *logfile << Debug() << L"service " << unit_name << L" started wait for " << (next_deadline-GetTickCount64()) << L" millis" << std::endl;

        if (svc_status.dwCurrentState == SERVICE_STOPPED) {
            *logfile << Debug() << L"service " << unit_name << L" status == " << svc_status.dwCurrentState << std::endl;
            since_service_inactive_millis = GetTickCount64();
            if (on_unit_inactive_millis > 0) {
                next_deadline = since_service_inactive_millis+on_unit_inactive_millis;
                *logfile << Debug() << L"service " << unit_name << L" on_unit_inactive_timeout sleep time " 
                                    << (next_deadline-since_service_inactive_millis) << std::endl;
                done = false;
            }
        }

        while (GetTickCount64() < next_deadline) {
            if (on_unit_inactive_millis > 0) {
                *logfile << Debug() << L"service " << unit_name << L" on_unit_inactive_timeout sleep" << std::endl;
            }
            ::SleepEx(accuracy_millis, true);
        }

        *logfile << Debug() << L"service " << unit_name << L" timeout complete" << std::endl;
      
    } while (!done);

    self->SetServiceStatus(SERVICE_STOP_PENDING);
    CloseServiceHandle(hsvc);
    CloseServiceHandle(hsc);
    self->SetServiceStatus(SERVICE_STOP);
    return S_OK;
}

DWORD WINAPI CWrapperService::ServiceThread(LPVOID param)

{ 
    CWrapperService *self = (CWrapperService *)param;
    DWORD exitCode = 0;
    boolean done  = false;

    boolean waitforfinish = true;
    do {
        try {

            *logfile << Info() << L"start " << self->m_ServiceName << "service thread" << std::endl;
            if (!self->EvaluateConditions()) {
                self->SetServiceStatus(SERVICE_STOPPED);
                throw RestartException(1, "condition failed");
            }

            // If files before exist, bail.
            for (auto before : self->m_FilesBefore) {
                *logfile << Debug() << L"before file " << before << std::endl;

                wstring path = self->m_unitPath;

                path.append(before);
                wifstream wifs(path);
                if (wifs.is_open()) {
                    *logfile << Debug() << L"before file " << before << " is present, so don't run" << std::endl;
                    wifs.close();
                    throw RestartException(1, "Before file is present, service not started");
                }
            }

            for (auto before : self->m_ServicesBefore) {
                 *logfile << Debug() << L"before service" << before << std::endl;
            }

            for (auto after : self->m_FilesAfter) {
                *logfile << Debug() << L"after file " << after << std::endl;

                // If files after do not exist, bail.
                wstring path = self->m_unitPath;
                path.append(after);
                wifstream wifs(path);
                if (!wifs.is_open()) {
                    *logfile << Info() << L"after file " << after << " is not present, so don't run" << std::endl;
                    throw RestartException(1, "After file is not present, service not started");
                }
                wifs.close();
            }

            for (auto after : self->m_ServicesAfter) {
                *logfile << Debug() << L"after service" << after << std::endl;
            }

            *logfile << Debug() << L"WaitForDependents = " << std::endl;

            self->SetServiceStatus(SERVICE_START_PENDING);
            if (!self->WaitForDependents(self->m_ServicesAfter)) {
                *logfile << Warning() << L"Failure in WaitForDepenents" << std::endl;
                throw RestartException(1068, "dependents failed");
            }

            exitCode = 0;

            // OK. We are going to launch. First resolve the environment

            self->GetCurrentEnv();
            for (auto envFile : self->m_EnvironmentFiles)
            {
                *logfile << Debug() << L"Set up environment file " << envFile << std::endl;
                if (!self->LoadEnvVarsFromFile(envFile)) {
                    *logfile << Error() << L"could not find required environment file " << envFile << std::endl;
                    throw RestartException(2, "environment file not found");
                }
            }

            for (auto envFile : self->m_EnvironmentFilesPS)
            {
                *logfile << Debug() << L"Set up pwsh environment file " << envFile << std::endl;
                if (!self->LoadPShellEnvVarsFromFile(envFile)) {
                    *logfile << Error() << L"could not find required environment file " << envFile << std::endl;
                    throw RestartException(2, "Powershell environment file not found");
                }
            }

            // Now we have the map, we can populate the buffer

            self->m_envBuf = L"";
            for(auto this_pair : self->m_Env) {
                self->m_envBuf.append(this_pair.first);
                self->m_envBuf.append(L"=");
                self->m_envBuf.append(this_pair.second);
                self->m_envBuf.push_back(L'\0');
                *logfile << Verbose() << L"env: " << this_pair.first << "=" << this_pair.second << std::endl;

                ::SetEnvironmentVariableW(this_pair.first.c_str(), this_pair.second.c_str());
            }
            self->m_envBuf.push_back(L'\0');

            self->SetServiceStatus(SERVICE_RUNNING);
            if (!self->m_ExecStartPreCmdLine.empty())
            {
                wostringstream os;
                for( int i = 0;  i < self->m_ExecStartPreCmdLine.size(); i++ ) {
                    auto ws = self->m_ExecStartPreCmdLine[i];
                    *logfile << Info() << L"Running ExecStartPre command: " << ws.c_str();
                      // to do, add special char processing
                    try {
                        self->StartProcess(ws.c_str(), 0, self->m_ExecStartPreProcInfo[i], true); 
                    }
                    catch(RestartException &ex) {
                         if (!(self->m_ExecStartPreFlags[i] & EXECFLAG_IGNORE_FAIL)) {
                            *logfile << Error() << L"Error in ExecStartPre command: " << ws.c_str() << "exiting" << std::endl;
                            throw ex;
                         }
                    }
                }
            }

            *logfile << Debug() << L"starting cmd " << self->m_ExecStartCmdLine.c_str() << std::endl;
            self->SetServiceStatus(SERVICE_RUNNING);
            self->m_IsStopping = FALSE;
        
            *logfile << Verbose() << L"Starting service: " << self->m_ServiceName << std::endl;
        
            if (!self->m_ExecStartCmdLine.empty()) {
                self->StartProcess(self->m_ExecStartCmdLine.c_str(), CREATE_NEW_PROCESS_GROUP, self->m_ExecStartProcInfo, false);
                self->RegisterMainPID();  // We register the pid in the registry so we can kill it later if we wish from systemctl
        
                *logfile << Verbose() << "waitfor main process " << std::endl;
                ::WaitForSingleObject(self->m_ExecStartProcInfo.hProcess, INFINITE);
        
                BOOL result = ::GetExitCodeProcess(self->m_ExecStartProcInfo.hProcess, &exitCode);
                ::CloseHandle(self->m_ExecStartProcInfo.hProcess);
                self->DeregisterMainPID();
        
                if (!result || exitCode) {
                    wostringstream os;
                    if (!result) {
                        *logfile << Error() << L"GetExitCodeProcess failed" << std::endl;
                    }
                    else {
                        *logfile << Error() << L"Command \"" << self->m_ExecStartCmdLine 
                     << L"\" failed with exit code: " << exitCode << std::endl;
                        throw RestartException(exitCode, "start command failed");
                    }
                }
            }
        
            if (!self->m_ExecStartPostCmdLine.empty()) {
                wostringstream os;

                for( int i = 0;  i < self->m_ExecStartPostCmdLine.size(); i++ ) {
                    auto ws = self->m_ExecStartPostCmdLine[i];
                    os << L"Running ExecStartPost command: " << ws.c_str();
                    *logfile << Verbose() << os.str() << std::endl;
                    try {
                        self->StartProcess(ws.c_str(), 0, self->m_ExecStartPostProcInfo[i], true);
                    }
                    catch(RestartException &ex) {
                        if (!(self->m_ExecStartPreFlags[i] & EXECFLAG_IGNORE_FAIL)) {
                            *logfile << Error() << L"Error in ExecStartPre command: " << ws.c_str() << "exiting" << std::endl;
                            throw ex;
                        }
                    }
                }
            }

            // We should stay active until all of the depends are finished
            self->WaitForDependents(self->m_Dependencies);
            *logfile << Verbose() << "process success " << self->m_ExecStartCmdLine << std::endl;
            throw RestartException(0, "success");
    }
    catch (RestartException &ex) {
    
            self->SetServiceStatus(SERVICE_PAUSED);
            switch ( self->m_RestartAction ) {
            default:
            case RESTART_ACTION_NO:
                done = true;
                break;
        
            case RESTART_ACTION_ALWAYS:
                done = false; 
                *logfile << Verbose() <<  L"Restart always in " << self->m_RestartMillis << L" milliseconds" << std::endl;
                ::SleepEx(self->m_RestartMillis, FALSE);
                *logfile << Verbose() << L"Restart always" << std::endl;
                break;
    
            case RESTART_ACTION_ON_SUCCESS:
                if (ex.exitCode != S_OK) {
                   done = true;
                }
                else {
                    *logfile << Verbose() << L"Restart on success in " << self->m_RestartMillis << L" milliseconds" << std::endl;
                    ::SleepEx(self->m_RestartMillis, TRUE); // But we respect restartSec.
                }
                break;
    
            case RESTART_ACTION_ON_FAILURE:
                if (ex.exitCode != S_OK) {
                   done = true;
                }
                else {
                    *logfile << Verbose() << L"Restart on success in " << self->m_RestartMillis << L" milliseconds" << std::endl;
                    ::SleepEx(self->m_RestartMillis, TRUE); // But we respect restartSec.
                }
                break;
    
            case RESTART_ACTION_ON_ABNORMAL:
            case RESTART_ACTION_ON_ABORT:
            case RESTART_ACTION_ON_WATCHDOG:
                // 2do: check the exit code
                *logfile << Verbose() << "Restart in " << self->m_RestartMillis << " milliseconds" << std::endl;
                ::SleepEx(self->m_RestartMillis, TRUE); // But we respect restartSec.
                break;
            }    
        }

        *logfile << Debug() << L"done = " << done << std::endl;
    } while (!done);

    *logfile << Debug() << L"exit service OnStart: " << std::endl;
    self->SetServiceStatus(SERVICE_STOPPED);
    return S_OK;

}


DWORD WINAPI CWrapperService::WaitForProcessThread(LPVOID lpParam)
{
    CWrapperService* self = (CWrapperService*)lpParam;

    ::WaitForSingleObject(self->m_ExecStartProcInfo.hProcess, INFINITE);
    ::CloseHandle(self->m_ExecStartProcInfo.hProcess);
    self->m_ExecStartProcInfo.hProcess = NULL;

    // TODO: think about respawning the child process
    if(!self->m_IsStopping)
    {
        self->WriteEventLogEntry(self->Name(), L"Child process ended", EVENTLOG_ERROR_TYPE);
        ::ExitProcess(-1);
    }

    return 0;
}

void WINAPI CWrapperService::KillProcessTree(DWORD dwProcId)
{
    PROCESSENTRY32 pe;
    memset(&pe, 0, sizeof(PROCESSENTRY32));
    pe.dwSize = sizeof(PROCESSENTRY32);

    *logfile << Verbose() << L"service kill process tree " << std::endl;
    HANDLE hSnap = :: CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

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
            SetConsoleCtrlHandler(NULL, true); // Disable Ctrl processing from here
            *logfile << Debug() << L"terminate subprocess " << dwProcId << "for service " << m_ServiceName  << std::endl;
            ::TerminateProcess(hProc, ERROR_PROCESS_ABORTED);
            ::CloseHandle(hProc);
        }
    }

    ::CloseHandle(hSnap);
}

void CWrapperService::OnStop()
{
    WriteEventLogEntry(m_name, L"Stopping service", EVENTLOG_INFORMATION_TYPE);

    m_IsStopping = TRUE;
    if (m_IsTimer) {
        ::TerminateThread(m_hServiceThread, ERROR_PROCESS_ABORTED);
        *logfile << Verbose() << L"service thread wait for terminate " << m_ServiceName.c_str() << std::endl;
        ::WaitForSingleObject(m_hServiceThread, INFINITE);
        *logfile << Verbose() << L"service thread terminated " << m_ServiceName.c_str() << std::endl;
        ::CloseHandle(m_hServiceThread);
        this->SetServiceStatus(SERVICE_STOPPED);
    }
    else {
        *logfile << Verbose() << L"stopping service " << m_ServiceName.c_str() << std::endl;
        if (!m_ExecStopCmdLine.empty())
        {
            wostringstream os;
            os << L"Running ExecStop command: " << m_ExecStopCmdLine.c_str();
            *logfile << Verbose() << os.str() << std::endl;
            WriteEventLogEntry(m_name, os.str().c_str(), EVENTLOG_INFORMATION_TYPE);
            StartProcess(m_ExecStopCmdLine.c_str(), 0, m_ExecStopProcInfo, true);
        }
    
        *logfile << Debug() << L"kill stopping service " << m_ServiceName.c_str() << std::endl;
        // KillProcessTree(m_dwProcessId);
    
        // Stop dependent services
        // this->StopServiceDependencies(); We don't do this .....
    
        // *logfile << Debug() << L"send  ctrl-break and wait for stop" << std::endl;
        *logfile << Error() << L"send  ctrl-break to process id " << m_ExecStartProcInfo.dwProcessId << " and wait for stop" << std::endl;
    
        // First, ask nicely. 
        // The CTRL_C_EVENT should go to all of the subprocesses since that all share a console.
        // We do this because some processes need warning before they terminate to perform cleanup. 
        DWORD wait_result = WAIT_FAILED;
        if (AttachConsole( m_ExecStartProcInfo.dwProcessId)) {
            SetConsoleCtrlHandler(NULL, true);
            if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_ExecStartProcInfo.dwProcessId)) {
                *logfile << Error() << L"send  ctrl-break to process id " << m_ExecStartProcInfo.dwProcessId << " failed error code " << ::GetLastError() << std::endl;
            }
            else {
    
                // Wait for them to stop (fixed 20 sec timeout)
                wait_result = ::WaitForSingleObject(m_ExecStartProcInfo.hProcess, this->m_TimeoutStopMillis);
            }
            FreeConsole();
            SetConsoleCtrlHandler(NULL, false);
        }
        else {
            *logfile << Error() << L"could not attach console " << m_ExecStartProcInfo.dwProcessId << " failed error code " << ::GetLastError() << std::endl;
        }
    
        if (wait_result == WAIT_TIMEOUT || wait_result == WAIT_FAILED) {
            *logfile << Info() << L"ctrl-c has no effect. forcibly terminate process " << m_ExecStartProcInfo.dwProcessId << "and wait for stop" << std::endl; 
        }    
    
        // Kill main process anyway. Worst case nothing happens because the process is gone.
        // ::TerminateProcess( m_ExecStartProcInfo.hProcess, ERROR_PROCESS_ABORTED);
        KillProcessTree( m_ExecStartProcInfo.dwProcessId);
    
        ::WaitForSingleObject(m_ExecStartProcInfo.hProcess, INFINITE);
        ::TerminateThread(m_hServiceThread, ERROR_PROCESS_ABORTED);
        *logfile << Verbose() << L"service thread wait for terminate " << m_ServiceName.c_str() << std::endl;
        ::WaitForSingleObject(m_hServiceThread, INFINITE);
        *logfile << Verbose() << L"service thread terminated " << m_ServiceName.c_str() << std::endl;
        ::CloseHandle(m_hServiceThread);
    
        m_hServiceThread = INVALID_HANDLE_VALUE;
    
        if (!m_ExecStopPostCmdLine.empty())
        {
            wostringstream os;
    
            int i = 0;
            for( auto ws: m_ExecStopPostCmdLine) {
                os << L"Running ExecStopPost command: " << ws.c_str();
                *logfile << Verbose() << os.str() << std::endl;
                StartProcess(ws.c_str(), 0, m_ExecStopPostProcInfo[i], true);
            i++;
            }
        }
    
        ::CloseHandle(m_WaitForProcessThread);
        m_WaitForProcessThread = NULL;
    }
}


boolean
CWrapperService::EvaluateConditions()

{
    if (!m_ConditionArchitecture.empty()) {
       for( auto ws: m_ConditionArchitecture) {
           if (!EvalConditionArchitecture(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionVirtualization.empty()) {
       for( auto ws: m_ConditionVirtualization) {
           if (!EvalConditionVirtualization(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionHost.empty()) {
       for( auto ws: m_ConditionHost) {
           if (!EvalConditionHost(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionKernelCommandLine.empty()) {
       for( auto ws: m_ConditionKernelCommandLine) {
           if (!EvalConditionKernelCommandLine(ws)) {
           return false;
       }
       }
    }
    if (!m_ConditionKernelVersion.empty()) {
       for( auto ws: m_ConditionKernelVersion) {
           if (!EvalConditionKernelVersion(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionSecurity.empty()) {
       for( auto ws: m_ConditionSecurity) {
           if (!EvalConditionSecurity(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionCapability.empty()) {
       for( auto ws: m_ConditionCapability) {
           if (!EvalConditionCapability(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionACPower.empty()) {
       for( auto ws: m_ConditionACPower) {
           if (!EvalConditionACPower(ws)) {
           return false;
       }
       }
    }
    if (!m_ConditionNeedsUpdate.empty()) {
       for( auto ws: m_ConditionNeedsUpdate) {
           if (!EvalConditionNeedsUpdate(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionFirstBoot.empty()) {
       for( auto ws: m_ConditionFirstBoot) {
           if (!EvalConditionFirstBoot(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionPathExists.empty()) {
       for( auto ws: m_ConditionPathExists) {
           if (!EvalConditionPathExists(ws)) {
               *logfile << Verbose() << L"Condition failed, Path " << ws << "exists " << std::endl;
               return false;
           }
       }
    }
    if (!m_ConditionPathExistsGlob.empty()) {
       for( auto ws: m_ConditionPathExistsGlob) {
           if (!EvalConditionPathExistsGlob(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionPathIsDirectory.empty()) {
       for( auto ws: m_ConditionPathIsDirectory) {
           if (!EvalConditionPathIsDirectory(ws)) {
           return false;
       }
       }
    }
    if (!m_ConditionPathIsSymbolicLink.empty()) {
       for( auto ws: m_ConditionPathIsSymbolicLink) {
           if (!EvalConditionPathIsSymbolicLink(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionPathIsMountPoint.empty()) {
       for( auto ws: m_ConditionPathIsMountPoint) {
           if (!EvalConditionPathIsMountPoint(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionPathIsReadWrite.empty()) {
       for( auto ws: m_ConditionPathIsReadWrite) {
           if (!EvalConditionPathIsReadWrite(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionDirectoryNotEmpty.empty()) {
       for( auto ws: m_ConditionDirectoryNotEmpty) {
           if (!EvalConditionDirectoryNotEmpty(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionFileNotEmpty.empty()) {
       for( auto ws: m_ConditionFileNotEmpty) {
           if (!EvalConditionFileNotEmpty(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionFileIsExecutable.empty()) {
       for( auto ws: m_ConditionFileIsExecutable) {
           if (!EvalConditionFileIsExecutable(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionUser.empty()) {
       for( auto ws: m_ConditionUser) {
           if (!EvalConditionUser(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionGroup.empty()) {
       for( auto ws: m_ConditionGroup) {
           if (!EvalConditionGroup(ws)) {
               return false;
           }
       }
    }
    if (!m_ConditionControlGroupController.empty()) {
       for( auto ws: m_ConditionControlGroupController) {
           if (!EvalConditionControlGroupController(ws)) {
               return false;
           }
       }
    }

    *logfile << Verbose() << L"Condition passed" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionArchitecture(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionArchitecture is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionVirtualization(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionVirtualization is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionHost(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionHost is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionKernelCommandLine(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionKernelCommandLine is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionKernelVersion(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionKernelVersion is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionSecurity(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionSecurity is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionCapability(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionCapability is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionACPower(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionACPower is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionNeedsUpdate(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionNeedsUpdate is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionFirstBoot(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionFirstBoot is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionPathExists(std::wstring arg)
{
    wchar_t *path = (wchar_t*)arg.c_str();
    int rslt = 0;

    *logfile << Verbose() << L"condition ConditionPathExists " << arg << std::endl;

    if (path[0] == L'!') {
        rslt =  ::GetFileAttributes(++path);
        return rslt == INVALID_FILE_ATTRIBUTES;
    }
    else {
        rslt =  ::GetFileAttributes(path);
        return rslt != INVALID_FILE_ATTRIBUTES;
    }
}


boolean 
CWrapperService::EvalConditionPathExistsGlob(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionPathExistsGlob is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionPathIsDirectory(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionPathIsDirectory is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionPathIsSymbolicLink(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionPathIsSymbolicLink is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionPathIsMountPoint(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionPathIsMountPoint is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionPathIsReadWrite(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionPathIsReadWrite is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionDirectoryNotEmpty(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionDirectoryNotEmpty is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionFileNotEmpty(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionFileNotEmpty is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionFileIsExecutable(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionFileIsExecutable is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionUser(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionUser is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionGroup(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionGroup is not implemented" << std::endl;
    return true;
}


boolean 
CWrapperService::EvalConditionControlGroupController(std::wstring arg)
{
    *logfile << Warning() << L"condition ConditionControlGroupController  is not implemented" << std::endl;
    return true;
}




boolean
CWrapperService::WaitForDependents(std::vector<std::wstring> &serviceList)

{
    int count = 0;
    SC_HANDLE hsc = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hsc) {
        int last_error = GetLastError();
        *logfile << Error() << L"WaitForDependents could not open service manager win err = " << last_error << std::endl;
        return false;
    }

    boolean done = false;
    do {
        done = true;
        for (auto service : serviceList) {
            SERVICE_STATUS service_status = {0};
            SC_HANDLE hsvc = OpenServiceW(hsc, service.c_str(), GENERIC_READ);
            if (!hsvc) {
                *logfile << Error() << L"WaitForDependents OpenService failed " << GetLastError() << std::endl;
                CloseServiceHandle(hsc);
                return true;  // If it doesn't exist we cant wait for it
            }

            *logfile << Debug() << L"Check status for service " << service << std::endl;

            if (!::QueryServiceStatus( hsvc, &service_status) ) {
                int last_error = GetLastError();
                 // 2do: handle MORE_DATA
                *logfile << Error() << L"WaitForDependents could not enum dependent services win err = " << last_error << std::endl;
                CloseServiceHandle(hsc);
                return false;
            }

            *logfile << Verbose() << L"status for service " << service << service_status.dwCurrentState << std::endl;
            if (service_status.dwCurrentState != SERVICE_STOP_PENDING &&
                service_status.dwCurrentState != SERVICE_STOPPED ) {
                *logfile << Debug() << L"done" << std::endl;
                done = false;
                break; // If someone is running we must wait. No need to keep looking
            }
        }

        if (!done) {
             Sleep(1000); // sleep for 1 sec as we check
        }
        count++;
    } while(!done && count < 500 );

    CloseServiceHandle(hsc);

   *logfile << Verbose() << L"Wait for dependents exits " << std::endl;
    return true;
}
