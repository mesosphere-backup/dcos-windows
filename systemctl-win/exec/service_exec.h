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

#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#include <string>
#include <sstream>
#include <locale>
#include <codecvt>
#include <map>
#include <vector>
#include <exception>
#include "journalstream.h"
#include "ServiceBase.h"

using namespace journalstreams;

typedef std::map<std::wstring, std::wstring> EnvMap;

class CWrapperService : public CServiceBase
{
public:
    enum ServiceType {
       SERVICE_TYPE_UNDEFINED,
       SERVICE_TYPE_SIMPLE,
       SERVICE_TYPE_FORKING,
       SERVICE_TYPE_ONESHOT,
       SERVICE_TYPE_DBUS,
       SERVICE_TYPE_NOTIFY,
       SERVICE_TYPE_IDLE
    };

    enum RestartAction {
       RESTART_ACTION_UNDEFINED,
       RESTART_ACTION_NO,
       RESTART_ACTION_ALWAYS,
       RESTART_ACTION_ON_SUCCESS,
       RESTART_ACTION_ON_FAILURE,
       RESTART_ACTION_ON_ABNORMAL,
       RESTART_ACTION_ON_ABORT,
       RESTART_ACTION_ON_WATCHDOG
    };

    enum NotifyAction {
        NOTIFY_ACTION_NONE,
	NOTIFY_ACTION_MAIN,
	NOTIFY_ACTION_EXEC,
	NOTIFY_ACTION_ALL
    };

    // The parameter list has gotten very long. This way we have a packet of params
    // with defaults. Since C++ does not have named parameters this allows use to init some
    // and define others

    struct ServiceParams {
        LPCWSTR szServiceName;
        LPCWSTR szShellCmdPre;
        LPCWSTR szShellCmdPost;
        std::vector<std::wstring> execStartPre;
        std::wstring              execStart;
        std::vector<std::wstring> execStartPost;
        std::wstring              execStop;
        std::vector<std::wstring> execStopPost;
        enum ServiceType serviceType;
        enum RestartAction   restartAction;
        int  restartMillis;
        std::wstring workingDirectory;
        BOOL fCanStop;
        BOOL fCanShutdown;
        BOOL fCanPauseContinue;
        std::wstring unitPath;
        wojournalstream *stdErr;
        wojournalstream *stdOut;
        std::vector<std::wstring> environmentFilesPS;
        std::vector<std::wstring> environmentFiles;
        std::vector<std::wstring> environmentVars;
        std::vector<std::wstring> files_before;
        std::vector<std::wstring> services_before;
        std::vector<std::wstring> files_after;
        std::vector<std::wstring> services_after;
        std::vector<std::wstring> files_requisite;
        std::vector<std::wstring> services_requisite;
        std::vector<std::wstring> conditionArchitecture;
        std::vector<std::wstring> conditionVirtualization;
        std::vector<std::wstring> conditionHost;
        std::vector<std::wstring> conditionKernelCommandLine;
        std::vector<std::wstring> conditionKernelVersion;
        std::vector<std::wstring> conditionSecurity;
        std::vector<std::wstring> conditionCapability;
        std::vector<std::wstring> conditionACPower;
        std::vector<std::wstring> conditionNeedsUpdate;
        std::vector<std::wstring> conditionFirstBoot;
        std::vector<std::wstring> conditionPathExists;
        std::vector<std::wstring> conditionPathExistsGlob;
        std::vector<std::wstring> conditionPathIsDirectory;
        std::vector<std::wstring> conditionPathIsSymbolicLink;
        std::vector<std::wstring> conditionPathIsMountPoint;
        std::vector<std::wstring> conditionPathIsReadWrite;
        std::vector<std::wstring> conditionDirectoryNotEmpty;
        std::vector<std::wstring> conditionFileNotEmpty;
        std::vector<std::wstring> conditionFileIsExecutable;
        std::vector<std::wstring> conditionUser;
        std::vector<std::wstring> conditionGroup;
        std::vector<std::wstring> conditionControlGroupController;
        ServiceParams(): szServiceName(NULL), 
            szShellCmdPre(NULL),
            szShellCmdPost(NULL),
            serviceType(SERVICE_TYPE_SIMPLE),
            restartAction(RESTART_ACTION_NO),
            restartMillis(INFINITE),
            fCanStop(TRUE),
            fCanShutdown(TRUE),
            fCanPauseContinue(FALSE) {  };
    };

    CWrapperService( struct CWrapperService::ServiceParams &params );
    virtual ~CWrapperService(void);

protected:

    virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
    virtual void OnStop();
    virtual boolean WaitForDependents(std::vector<std::wstring> &serviceList);

private:

    struct RestartException : public std::exception
    {
        std::string msg;
        DWORD exitCode;

        RestartException(DWORD err, const char *str=NULL) {
            msg = str;
            exitCode = err;
        }

	const char * what () const throw ()
        {
            std::stringstream ss;
            ss << msg << "exit code: " << exitCode ;
       	    return ss.str().c_str();
        }
    };

    struct ServiceManagerException : public std::exception
    {
        std::string msg;
        DWORD exitCode;

        ServiceManagerException(DWORD err, const std::wstring wstr) {
            std::string msg = std::wstring_convert<std::codecvt_utf8<WCHAR>>().to_bytes(wstr);
            exitCode = err;
        }

	const char * what () const throw ()
        {
            std::stringstream ss;
            ss << msg << "service manager excpetion exit code: " << exitCode ;
       	    return ss.str().c_str();
        }
    };

    // Special executable prefixes. See systemd.service
    // we make a mask because some chars may be used together

    static const wchar_t  EXECCHAR_ARG0 = L'@';
    static const unsigned EXECFLAG_ARG0 = 0x000000001;

    static const wchar_t  EXECCHAR_IGNORE_FAIL = L'-';
    static const unsigned EXECFLAG_IGNORE_FAIL = 0x000000002;

    static const wchar_t  EXECCHAR_FULL_PRIVELEGE = L'-';
    static const unsigned EXECFLAG_FULL_PRIVELEGE = 0x000000004;

    static const wchar_t  EXECCHAR_ELEVATE_PRIVELEGE = L'!';
    static const unsigned EXECFLAG_ELEVATE_PRIVELEGE = 0x000000008;
    static const unsigned EXECFLAG_AMBIENT_PRIVELEGE = 0x000000008; // !!

    static DWORD WINAPI ServiceThread(LPVOID param); // Pass this pointer

    void GetCurrentEnv();
    void LoadEnvVarsFromFile(const std::wstring& path);
    void LoadPShellEnvVarsFromFile(const std::wstring& path);

    void GetServiceDependencies(); // I know that the dependent info is in the unit file (wants and requires). 
                                 // But if the policy changes for some reason, getting the dependents is 
                 		    // more robust than assuming that I know what is there
   // void StopServiceDependencies();

    static DWORD WINAPI WaitForProcessThread(LPVOID lpParam);
    void WINAPI KillProcessTree(DWORD dwProcId);
    static enum OUTPUT_TYPE StrToOutputType( std::wstring ws, std::wstring *path );
    unsigned ProcessSpecialCharacters( std::wstring &ws);

    void StartProcess(LPCWSTR cmdLine, DWORD processFlags, PROCESS_INFORMATION &procInfo, bool waitForProcess, bool failOnError=false);
    void RegisterMainPID();
    void DeregisterMainPID();

    boolean EvaluateConditions();
    std::wstring ResolveEnvVars(std::wstring str); // Expands any environment variables that are in the 
                                                   // string. We need to do this for things like directories
                                         // which are passed before the command processor has time to run.

    boolean EvalConditionArchitecture(std::wstring arg);
    boolean EvalConditionVirtualization(std::wstring arg);
    boolean EvalConditionHost(std::wstring arg);
    boolean EvalConditionKernelCommandLine(std::wstring arg);
    boolean EvalConditionKernelVersion(std::wstring arg);
    boolean EvalConditionSecurity(std::wstring arg);
    boolean EvalConditionCapability(std::wstring arg);
    boolean EvalConditionACPower(std::wstring arg);
    boolean EvalConditionNeedsUpdate(std::wstring arg);
    boolean EvalConditionFirstBoot(std::wstring arg);
    boolean EvalConditionPathExists(std::wstring arg);
    boolean EvalConditionPathExistsGlob(std::wstring arg);
    boolean EvalConditionPathIsDirectory(std::wstring arg);
    boolean EvalConditionPathIsSymbolicLink(std::wstring arg);
    boolean EvalConditionPathIsMountPoint(std::wstring arg);
    boolean EvalConditionPathIsReadWrite(std::wstring arg);
    boolean EvalConditionDirectoryNotEmpty(std::wstring arg);
    boolean EvalConditionFileNotEmpty(std::wstring arg);
    boolean EvalConditionFileIsExecutable(std::wstring arg);
    boolean EvalConditionUser(std::wstring arg);
    boolean EvalConditionGroup(std::wstring arg);
    boolean EvalConditionControlGroupController(std::wstring arg);

    std::wstring m_ServiceName;

    std::vector<std::wstring> m_ExecStartPreCmdLine;
    std::vector<unsigned>     m_ExecStartPreFlags;
    std::vector<PROCESS_INFORMATION> m_ExecStartPreProcInfo;

    std::wstring m_ExecStartCmdLine;
    unsigned m_ExecStartFlags;
    PROCESS_INFORMATION m_ExecStartProcInfo;

    std::vector<std::wstring> m_ExecStartPostCmdLine;
    std::vector<unsigned>     m_ExecStartPostFlags;
    std::vector<PROCESS_INFORMATION> m_ExecStartPostProcInfo;

    std::wstring m_ExecStopCmdLine;
    unsigned m_ExecStopFlags;
    PROCESS_INFORMATION m_ExecStopProcInfo;

    std::vector<std::wstring> m_ExecStopPostCmdLine;
    std::vector<unsigned>     m_ExecStopPostFlags;
    std::vector<PROCESS_INFORMATION> m_ExecStopPostProcInfo;

    std::vector<std::wstring> m_FilesBefore;     // Service won't execute if these exist
    std::vector<std::wstring> m_ServicesBefore;  // Service won't execute if these are running
    std::vector<std::wstring> m_FilesAfter;      // Service won't execute if these exist
    std::vector<std::wstring> m_ServicesAfter;   // Service won't execute if these are running
    std::vector<std::wstring> m_Requisite_Files; // Service won't execute if these don't exist
    std::vector<std::wstring> m_Requisite_Services; //  Service won't execute if these are running

    std::vector<std::wstring> m_EnvironmentFiles;    // Evaluated each time the service is started.
    std::vector<std::wstring> m_EnvironmentFilesPS;  // Evaluated each time the service is started.
    std::vector<std::wstring> m_EnvironmentVars;
    std::wstring m_unitPath;
    std::wstring m_envBuf;
    EnvMap m_Env;

    std::vector<std::wstring> m_ConditionArchitecture;
    std::vector<std::wstring> m_ConditionVirtualization;
    std::vector<std::wstring> m_ConditionHost;
    std::vector<std::wstring> m_ConditionKernelCommandLine;
    std::vector<std::wstring> m_ConditionKernelVersion;
    std::vector<std::wstring> m_ConditionSecurity;
    std::vector<std::wstring> m_ConditionCapability;
    std::vector<std::wstring> m_ConditionACPower;
    std::vector<std::wstring> m_ConditionNeedsUpdate;
    std::vector<std::wstring> m_ConditionFirstBoot;
    std::vector<std::wstring> m_ConditionPathExists;
    std::vector<std::wstring> m_ConditionPathExistsGlob;
    std::vector<std::wstring> m_ConditionPathIsDirectory;
    std::vector<std::wstring> m_ConditionPathIsSymbolicLink;
    std::vector<std::wstring> m_ConditionPathIsMountPoint;
    std::vector<std::wstring> m_ConditionPathIsReadWrite;
    std::vector<std::wstring> m_ConditionDirectoryNotEmpty;
    std::vector<std::wstring> m_ConditionFileNotEmpty;
    std::vector<std::wstring> m_ConditionFileIsExecutable;
    std::vector<std::wstring> m_ConditionUser;
    std::vector<std::wstring> m_ConditionGroup;
    std::vector<std::wstring> m_ConditionControlGroupController;

    std::vector<std::wstring> m_Dependencies;

    HANDLE m_WaitForProcessThread;

    HANDLE m_hServiceThread;
    DWORD  m_dwServiceThreadId;

    enum ServiceType m_ServiceType;
    enum RestartAction m_RestartAction;
    int  m_RestartMillis;
    int  m_StartLimitIntervalMillis;
    std::wstring m_WorkingDirectory;

    wojournalstream *m_StdErr;
    wojournalstream *m_StdOut;
    volatile BOOL m_IsStopping;
};
