#include <iostream>
#include <fstream>
#include <sstream>
#include "service_unit.h"

namespace SystemCtlLog {

    enum LogLevel current_log_level = LogLevelInfo;
    std::wstringstream msg;
    
    void Error() 
    {
        wcerr << L"ERROR: " << msg.str() << std::endl;
        msg.flush();
        msg.str(L"");
    }
    
    void Warning() 
    {
        if (current_log_level <= LogLevelWarning) {
            wcerr << L"WARNING: " << msg.str() << std::endl;
        }    
        msg.flush();
        msg.str(L"");
    }
    
    void Info() 
    {
        if (current_log_level <= LogLevelInfo) {
            wcerr << L"INFO: " << msg.str() << std::endl;
        }    
        msg.flush();
        msg.str(L"");
    }
    
    void Verbose() 
    {
        if (current_log_level <= LogLevelVerbose) {
            wcerr << L"VERBOSE: " << msg.str() << std::endl;
        }    
        msg.flush();
        msg.str(L"");
    }
    
    void Debug() 
    {
        if (current_log_level <= LogLevelDebug) {
            wcerr << L"DEBUG: " << msg.str() << std::endl;
        }    
        msg.flush();
        msg.str(L"");
    }
}
