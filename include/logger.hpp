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
    static Logger<false> kernelLogger;
    static Logger<false> debugStringLogger; // Enables output for the outputDebugString SVC
    static Logger<false> errorLogger;
    static Logger<false> fileIOLogger;
    static Logger<false> svcLogger;
    static Logger<false> threadLogger;
    static Logger<false> gpuLogger;
    static Logger<false> rendererLogger;

    // Service loggers
    static Logger<false> acLogger;
    static Logger<false> amLogger;
    static Logger<false> aptLogger;
    static Logger<false> bossLogger;
    static Logger<false> cecdLogger;
    static Logger<false> cfgLogger;
    static Logger<false> dspServiceLogger;
    static Logger<false> frdLogger;
    static Logger<false> fsLogger;
    static Logger<false> hidLogger;
    static Logger<false> gspGPULogger;
    static Logger<false> gspLCDLogger;
    static Logger<false> micLogger;
    static Logger<false> nimLogger;
    static Logger<false> ndmLogger;
    static Logger<false> ptmLogger;
    static Logger<false> srvLogger;

    #define MAKE_LOG_FUNCTION(functionName, logger)      \
    template <typename... Args>                          \
    void functionName(const char* fmt, Args... args) {   \
        Log::logger.log(fmt, args...);                   \
    }
}