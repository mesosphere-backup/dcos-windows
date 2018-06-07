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

#include <string>
#include <map>
#include "ServiceBase.h"

typedef std::map<std::wstring, std::wstring> EnvMap;

class CWrapperService : public CServiceBase
{
public:

    CWrapperService(LPCWSTR pszServiceName,
        LPCWSTR szCmdLine,
        LPCWSTR szExecStartPreCmdLine,
        const EnvMap& environment,
        BOOL fTree = TRUE,
        BOOL fConsole = FALSE,
        int fConsole_timeout = 15,
        BOOL fCanStop = TRUE,
        BOOL fCanShutdown = TRUE,
        BOOL fCanPauseContinue = FALSE,
        HANDLE fStdOutErrHandle = INVALID_HANDLE_VALUE);
    virtual ~CWrapperService(void);

protected:

    virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
    virtual void OnStop();

private:

    static DWORD WINAPI WaitForProcessThread(LPVOID lpParam);
    static void WINAPI KillProcessTree(DWORD dwProcId);
    PROCESS_INFORMATION StartProcess(LPCWSTR cmdLine, bool waitForProcess = false);

    std::wstring m_CmdLine;
    std::wstring m_ExecStartPreCmdLine;
    std::wstring m_envBuf;
    DWORD m_dwProcessId;
    HANDLE m_hProcess;
    HANDLE m_WaitForProcessThread;
    HANDLE m_StdOutErrHandle;
    BOOL m_StopTree;
    BOOL m_Console;
    int m_ConsoleTimeout;
    volatile BOOL m_IsStopping;
};
