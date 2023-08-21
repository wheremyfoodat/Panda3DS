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
	// Enables output for the outputDebugString SVC
	static Logger<true> debugStringLogger;
	static Logger<false> errorLogger;
	static Logger<false> fileIOLogger;
	static Logger<false> svcLogger;
	static Logger<false> threadLogger;
	static Logger<false> gpuLogger;
	static Logger<false> rendererLogger;
	static Logger<false> shaderJITLogger;

	// Service loggers
	static Logger<false> acLogger;
	static Logger<false> actLogger;
	static Logger<false> amLogger;
	static Logger<false> aptLogger;
	static Logger<false> bossLogger;
	static Logger<false> camLogger;
	static Logger<false> cecdLogger;
	static Logger<false> cfgLogger;
	static Logger<false> dspServiceLogger;
	static Logger<false> dlpSrvrLogger;
	static Logger<false> frdLogger;
	static Logger<false> fsLogger;
	static Logger<false> hidLogger;
	static Logger<false> httpLogger;
	static Logger<false> irUserLogger;
	static Logger<false> gspGPULogger;
	static Logger<false> gspLCDLogger;
	static Logger<false> ldrLogger;
	static Logger<false> mcuLogger;
	static Logger<false> micLogger;
	static Logger<false> newsLogger;
	static Logger<false> nfcLogger;
	static Logger<false> nimLogger;
	static Logger<false> ndmLogger;
	static Logger<false> ptmLogger;
	static Logger<false> socLogger;
	static Logger<false> sslLogger;
	static Logger<false> y2rLogger;
	static Logger<false> srvLogger;

	// We have 2 ways to create a log function
	// MAKE_LOG_FUNCTION: Creates a log function which is toggleable but always killed for user-facing builds
	// MAKE_LOG_FUNCTION_USER: Creates a log function which is toggleable, may be on for user builds as well
	// We need this because sadly due to the loggers taking variadic arguments, compilers will not properly
	// Kill them fully even when they're disabled. The only way they will is if the function with varargs is totally empty

#define MAKE_LOG_FUNCTION_USER(functionName, logger)     \
	template <typename... Args>                          \
	void functionName(const char* fmt, Args&&... args) { \
		Log::logger.log(fmt, args...);                   \
	}

#ifdef PANDA3DS_USER_BUILD
#define MAKE_LOG_FUNCTION(functionName, logger) \
	template <typename... Args>                 \
	void functionName(const char* fmt, Args&&... args) {}
#else
#define MAKE_LOG_FUNCTION(functionName, logger) MAKE_LOG_FUNCTION_USER(functionName, logger)
#endif
}
