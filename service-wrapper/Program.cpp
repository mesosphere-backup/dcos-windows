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
#include <stdio.h>
#include <windows.h>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/join.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <regex>
#include <vector>
#include "ServiceBase.h"
#include "service-wrapper.h"
#pragma endregion

using namespace std;
using namespace boost::program_options;

struct CLIArgs
{
    vector<wstring> environmentFiles;
    vector<wstring> environmentFilesPShell;
    wstring execStartPre;
    wstring serviceName;
    BOOL tree;
    BOOL console;
    int console_timeout;
    vector<wstring> additionalArgs;
    wstring logFile;
};

CLIArgs ParseArgs(int argc, wchar_t *argv[]);
EnvMap LoadEnvVarsFromFile(const wstring& path);
EnvMap GetCurrentEnv();


CLIArgs ParseArgs(int argc, wchar_t *argv[])
{
    CLIArgs args;
    options_description desc{ "Options" };
    desc.add_options()
        ("environment-file,e", wvalue<vector<wstring>>(), "Environment file")
        ("environment-file-pshell", wvalue<vector<wstring>>(), "Powershell environment files")
        ("exec-start-pre", wvalue<wstring>(), "Command to be executed before starting the service")
        ("service-name,n", wvalue<wstring>(), "Service name")
        ("log-file", wvalue<wstring>(), "Log file containing  the redirected STD OUT and ERR of the child process")
        ("stop-tree", wvalue<BOOL>(), "Stop the wrapped process and its tree")
        ("stop-console", wvalue<BOOL>(), "Send SIGTERM to the wrapped and wait for it to gracefully shutdown")
        ("stop-console-timeout", wvalue<int>(), "Timeout to wait for the process to be gracefully shutdown");

    variables_map vm;
    auto parsed = wcommand_line_parser(argc, argv).
        options(desc).allow_unregistered().run();
    store(parsed, vm);
    auto additionalArgs = collect_unrecognized(parsed.options, include_positional);
    notify(vm);

    if (vm.count("environment-file"))
        args.environmentFiles = vm["environment-file"].as<vector<wstring>>();

    if (vm.count("environment-file-pshell"))
        args.environmentFilesPShell = vm["environment-file-pshell"].as<vector<wstring>>();

    if (vm.count("exec-start-pre"))
        args.execStartPre = vm["exec-start-pre"].as<wstring>();

    if (vm.count("log-file"))
        args.logFile = vm["log-file"].as<wstring>();

    if (vm.count("service-name"))
        args.serviceName = vm["service-name"].as<wstring>();
    else if (!additionalArgs.empty())
    {
        args.serviceName = additionalArgs[0];
        additionalArgs = vector<wstring>(additionalArgs.begin() + 1, additionalArgs.end());
    }

    if (vm.count("stop-tree"))
        args.tree = vm["stop-tree"].as<BOOL>();
    else
        args.tree = TRUE;

    if (vm.count("stop-console"))
        args.console = vm["stop-console"].as<BOOL>();
    else
        args.console = FALSE;

    if (vm.count("stop-console-timeout"))
        args.console_timeout = vm["stop-console-timeout"].as<int>();
    else
        args.console_timeout = 15 * 1000;

    if(args.serviceName.empty())
        throw exception("Service name not provided");

    args.additionalArgs = additionalArgs;
    if (args.additionalArgs.empty())
        throw exception("Service executable not provided");

    return args;
}

EnvMap GetCurrentEnv()
{
    EnvMap currentEnv;

    LPTCH tmpEnv = ::GetEnvironmentStrings();
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
            currentEnv[name] = value;
        }

        envPair = envPair + envPairStr.length() + 1;
    }
    ::FreeEnvironmentStrings(tmpEnv);

    return currentEnv;
}

EnvMap LoadEnvVarsFromFile(const wstring& path)
{
    wifstream inputFile(path);
    wstring line;
    EnvMap env;

    while (getline(inputFile, line))
    {
        wregex rgx(L"^([^#][^=]*)=(.*)$");
        wsmatch matches;
        if (regex_search(line, matches, rgx))
        {
            auto name = boost::algorithm::trim_copy(matches[1].str());
            auto value = boost::algorithm::trim_copy(matches[2].str());
            env[name] = value;
        }
    }

    return env;
}

EnvMap LoadPShellEnvVarsFromFile(const wstring& path)
{
    wifstream inputFile(path);
    wstring line;
    EnvMap env;

    while (getline(inputFile, line))
    {
        wregex rgx(L"^\\s*\\$env:([^#=]*)=['\"](.*)['\"]$");
        wsmatch matches;
        if (regex_search(line, matches, rgx))
        {
            auto name = boost::algorithm::trim_copy(matches[1].str());
            auto value = boost::algorithm::trim_copy(matches[2].str());
            env[name] = value;
        }
    }

    return env;
}

int wmain(int argc, wchar_t *argv[])
{
    HANDLE hLogFile = INVALID_HANDLE_VALUE;
    try
    {
        EnvMap env;
        auto args = ParseArgs(argc, argv);
        if (!args.environmentFiles.empty() ||
            !args.environmentFilesPShell.empty())
        {
            auto currentEnv = GetCurrentEnv();
            for (auto envFile : args.environmentFiles)
            {
                env = LoadEnvVarsFromFile(envFile);
                env.insert(currentEnv.begin(), currentEnv.end());
                currentEnv = env;
            }
            for (auto envFile : args.environmentFilesPShell)
            {
                env = LoadPShellEnvVarsFromFile(envFile);
                env.insert(currentEnv.begin(), currentEnv.end());
                currentEnv = env;
            }
        }

        auto it = args.additionalArgs.begin();
        wstring cmdLine = *it++;
        for (; it != args.additionalArgs.end(); ++it)
            cmdLine += L" \"" + *it + L"\"";

        if (!args.logFile.empty())
        {
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = NULL;
            sa.bInheritHandle = TRUE;
            hLogFile = CreateFile(args.logFile.c_str(),
                                FILE_APPEND_DATA,
                                FILE_READ_DATA | FILE_WRITE_DATA,
                                &sa,
                                OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
            if (hLogFile == INVALID_HANDLE_VALUE)
            {
                char msg[100];
                sprintf_s(msg, "Failed to create log file w/err 0x%08lx", GetLastError());
                throw exception(msg);
            }
        }
        CWrapperService service(args.serviceName.c_str(), cmdLine.c_str(),
                                args.execStartPre.c_str(), env, args.tree, args.console,
                                args.console_timeout, TRUE, TRUE, FALSE, hLogFile);
        if (!CServiceBase::Run(service))
        {
            char msg[100];
            sprintf_s(msg, "Service failed to run w/err 0x%08lx", GetLastError());
            CloseHandle(hLogFile);
            throw exception(msg);
        }
        CloseHandle(hLogFile);
        return 0;
    }
    catch (exception &ex)
    {
        std::cerr << ex.what() << '\n';
        CloseHandle(hLogFile);
        return -1;
    }
}
