#pragma once
#include <cstdarg>
#include <fstream>

namespace Log {
    // Our logger class
    template <bool enabled>
    class Logger {
    public:
        void log(const char* fmt, ...) {
            if constexpr (!enabled) return;
            
            std::va_list args;
            va_start(args, fmt);
            std::vprintf(fmt, args);
            va_end(args);
        }
    };

    // Our loggers here. Enable/disable by toggling the template param
    static Logger<true> kernelLogger;
    static Logger<true> debugStringLogger; // Enables output for the outputDebugString SVC
    static Logger<true> errorLogger;
    static Logger<true> fileIOLogger;
    static Logger<true> svcLogger;
    static Logger<true> gpuLogger;

    // Service loggers
    static Logger<true> aptLogger;
    static Logger<true> cfgLogger;
    static Logger<true> dspServiceLogger;
    static Logger<true> fsLogger;
    static Logger<true> hidLogger;
    static Logger<true> gspGPULogger;
    static Logger<true> gspLCDLogger;
    static Logger<true> ndmLogger;
    static Logger<true> ptmLogger;
    static Logger<true> srvLogger;

    #define MAKE_LOG_FUNCTION(functionName, logger)      \
    template <typename... Args>                          \
    void functionName(const char* fmt, Args... args) {   \
        Log::logger.log(fmt, args...);                   \
    }
}