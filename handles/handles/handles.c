/*
 * Copyright (c) 2018 Microsoft Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <windows.h>
#include <RestartManager.h>
#include <stdio.h>

#ifndef WINDOWS_LEAN_AND_MEAN
#define WINDOWS_LEAN_AND_MEAN
#endif // !WINDOWS_LEAN_AND_MEAN

#define SESSION_KEY_LENGTH (CCH_RM_SESSION_KEY + 1)

// Force link with the following library
#pragma comment(lib, "Rstrtmgr.lib")

int wmain(int argc, WCHAR **argv)
{
	WCHAR session_key[SESSION_KEY_LENGTH];
	DWORD session, error, reason;
	PCWSTR file_path = NULL;
	UINT i, no_proc_needed, no_proc = 0;
	RM_PROCESS_INFO* rgpi = NULL;

	if (argc != 2) {
		wprintf(L"Please specify a file path.\n");
		goto return_error;
	}
	file_path = argv[1];

	memset(session_key, 0, sizeof(WCHAR) * SESSION_KEY_LENGTH);
	error = RmStartSession(&session, 0, session_key);
	if (error != 0) {
		wprintf(L"RmStartSession returned %d\n", error);
		goto return_error;
	}
	error = RmRegisterResources(session, 1, &file_path,
			                    0, NULL, 0, NULL);
	if (error != 0) {
		wprintf(L"RmRegisterResources(%ls) returned %d\n",
			    file_path, error);
		goto return_error;
	}

	// Fail the first try just to get the number of processes
	error = RmGetList(session, &no_proc_needed,
				      &no_proc, rgpi, &reason);
	if (error == ERROR_MORE_DATA) {
		no_proc = no_proc_needed;
		rgpi = malloc(sizeof(RM_PROCESS_INFO) * no_proc);
		if (!rgpi) {
			wprintf(L"Failed to allocate memory for number %ud of"
				    L"processes information", no_proc);
			goto return_error;
		}
		error = RmGetList(session, &no_proc_needed,
			              &no_proc, rgpi, &reason);
	}

	if (error != 0) {
		wprintf(L"RmGetList returned %d\n", error);
		goto return_error;
	}

	for (i = 0; i < no_proc; i++) {
		wprintf(L"Application Name = %ls\n",
			    rgpi[i].strAppName);
		wprintf(L"Application Process ID = %d\n",
			    rgpi[i].Process.dwProcessId);
	}
	RmEndSession(session);

	return 0;

return_error:
	if (rgpi) {
		free(rgpi);
	}

	return 1;
}
