/****************************** Module Header ******************************\
* Module Name:  ServiceBase.h
* Project:      CppWindowsService
* Copyright (c) Microsoft Corporation.
*
* Provides a class for performing logging according to the output types available in 
* systemd unit files.
*
* This source is subject to the Microsoft Public License.
* See http://www.microsoft.com/en-us/openness/resources/licenses.aspx#MPL.
* All other rights reserved.
*
* THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
* EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/
#include <ios>
#include <ostream>
#include <iostream>
#include <sstream>
#include "windows.h"
#include "journalstream.h"


static inline enum OUTPUT_TYPE String_To_OutputType(const std::wstring val)

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
    else if (val.compare(0, 5, L"file:", 5) == 0) {
        return OUTPUT_TYPE_FILE;
    }
    else if (val.compare(L"socket") == 0) {
        return OUTPUT_TYPE_SOCKET;
    }
    else if (val.compare(0, 3, L"fd:name. ") == 0) {
        return OUTPUT_TYPE_FD;
    }
    else {
        return OUTPUT_TYPE_INVALID;
    }
}


wojournalstream::wojournalstream(std::wstring output_type, std::wstring filepath ):
           std::basic_ostream<wchar_t>(&buf),
           buf()

{
    m_output_type = String_To_OutputType(output_type);
    m_filehandle  = INVALID_HANDLE_VALUE;
    boolean needs_handle = false;

    switch (m_output_type) {
    default:
    case OUTPUT_TYPE_INVALID:
        throw std::exception("invalid output type");
        break;

    case OUTPUT_TYPE_INHERIT:
        // Figure out the windows handle for fd 0,1,2
        break;

    case OUTPUT_TYPE_NULL:
        // Go through a different stream
        break;

    case OUTPUT_TYPE_SOCKET:
    case OUTPUT_TYPE_FD:
    case OUTPUT_TYPE_TTY:
    case OUTPUT_TYPE_KMSG:
    case OUTPUT_TYPE_KMSG_PLUS_CONSOLE:
        throw std::exception("unsupported output type");
        break;

    case OUTPUT_TYPE_JOURNAL:
        // For syslog and journal, the caller supplies the file name for output
        buf.set_output_logfile(true);
        buf.set_output_eventlog(false);
        needs_handle = true;
        break;

    case OUTPUT_TYPE_SYSLOG:
        // For syslog and journal, the caller supplies the file name for output
        buf.set_output_logfile(true);
        buf.set_output_eventlog(true);
        needs_handle = true;
        break;

    case OUTPUT_TYPE_JOURNAL_PLUS_CONSOLE:
        // For syslog and journal, the caller supplies the file name for output
        buf.set_output_console(true);
        buf.set_output_logfile(true);
        buf.set_output_eventlog(false);
        needs_handle = true;
        break;

    case OUTPUT_TYPE_SYSLOG_PLUS_CONSOLE:
        // For syslog and journal, the caller supplies the file name for output
        buf.set_output_console(true);
        buf.set_output_logfile(true);
        buf.set_output_eventlog(true);
        needs_handle = true;
        break;

    case OUTPUT_TYPE_FILE:
        buf.set_output_console(false);
        buf.set_output_logfile(true);
        buf.set_output_eventlog(false);
        needs_handle = true;
    }

    if (needs_handle) {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        m_filehandle = CreateFileW(filepath.c_str(),
                                FILE_APPEND_DATA | SYNCHRONIZE,
                                FILE_READ_DATA | FILE_WRITE_DATA,
                                &sa,
                                OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

        if (m_filehandle == INVALID_HANDLE_VALUE)
        {
            std::stringstream msg;
            msg << "Failed to create log file w/err " << GetLastError();
            throw std::exception(msg.str().c_str());
        }
        buf.set_handle(m_filehandle);
    }
}

void
wojournalstream::open(std::wstring output_type, std::wstring filepath )

{
    m_output_type = String_To_OutputType(output_type);

    if (m_filehandle != INVALID_HANDLE_VALUE) {
        (void)buf.sync();
        CloseHandle(m_filehandle);
        buf.set_handle(m_filehandle);
    }

    m_filehandle  = INVALID_HANDLE_VALUE;

    boolean needs_handle = false;

    switch (m_output_type) {
    default:
    case OUTPUT_TYPE_INVALID:
        throw std::exception("invalid output type");
        break;

    case OUTPUT_TYPE_INHERIT:
        // Figure out the windows handle for fd 0,1,2
        break;

    case OUTPUT_TYPE_NULL:
        // Go through a different stream
        break;

    case OUTPUT_TYPE_SOCKET:
    case OUTPUT_TYPE_FD:
    case OUTPUT_TYPE_TTY:
    case OUTPUT_TYPE_KMSG:
    case OUTPUT_TYPE_KMSG_PLUS_CONSOLE:
        throw std::exception("unsupported output type");
        break;

    case OUTPUT_TYPE_JOURNAL:
        // For syslog and journal, the caller supplies the file name for output
        buf.set_output_logfile(true);
        buf.set_output_eventlog(false);
        needs_handle = true;
        break;

    case OUTPUT_TYPE_SYSLOG:
        // For syslog and journal, the caller supplies the file name for output
        buf.set_output_logfile(true);
        buf.set_output_eventlog(true);
        needs_handle = true;
        break;

    case OUTPUT_TYPE_JOURNAL_PLUS_CONSOLE:
        // For syslog and journal, the caller supplies the file name for output
        buf.set_output_console(true);
        buf.set_output_logfile(true);
        buf.set_output_eventlog(false);
        needs_handle = true;
        break;

    case OUTPUT_TYPE_SYSLOG_PLUS_CONSOLE:
        // For syslog and journal, the caller supplies the file name for output
        buf.set_output_console(true);
        buf.set_output_logfile(true);
        buf.set_output_eventlog(true);
        needs_handle = true;
        break;

    case OUTPUT_TYPE_FILE:
        buf.set_output_console(false);
        buf.set_output_logfile(true);
        buf.set_output_eventlog(false);
        needs_handle = true;
    }

    if (needs_handle) {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        m_filehandle = CreateFileW(filepath.c_str(),
                                FILE_APPEND_DATA | SYNCHRONIZE,
                                FILE_READ_DATA | FILE_WRITE_DATA,
                                &sa,
                                OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

        if (m_filehandle == INVALID_HANDLE_VALUE)
        {
            std::stringstream msg;
            msg << "Failed to create log file w/err " << GetLastError();
            throw std::exception(msg.str().c_str());
        }
        buf.set_handle(m_filehandle);
    }
}

