// SPDX-FileCopyrightText: 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "host_memory/dynamic_library.h"

#include <fmt/format.h>

#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Common {

	DynamicLibrary::DynamicLibrary() = default;
	DynamicLibrary::DynamicLibrary(const char* filename) { void(Open(filename)); }
	DynamicLibrary::DynamicLibrary(void* handle_) : handle{handle_} {}
	DynamicLibrary::DynamicLibrary(DynamicLibrary&& rhs) noexcept : handle{std::exchange(rhs.handle, nullptr)} {}

	DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& rhs) noexcept {
		Close();
		handle = std::exchange(rhs.handle, nullptr);
		return *this;
	}

	DynamicLibrary::~DynamicLibrary() { Close(); }

	std::string DynamicLibrary::GetUnprefixedFilename(const char* filename) {
#if defined(_WIN32)
		return std::string(filename) + ".dll";
#elif defined(__APPLE__)
		return std::string(filename) + ".dylib";
#else
		return std::string(filename) + ".so";
#endif
	}

	std::string DynamicLibrary::GetVersionedFilename(const char* libname, int major, int minor) {
#if defined(_WIN32)
		if (major >= 0 && minor >= 0)
			return fmt::format("{}-{}-{}.dll", libname, major, minor);
		else if (major >= 0)
			return fmt::format("{}-{}.dll", libname, major);
		else
			return fmt::format("{}.dll", libname);
#elif defined(__APPLE__)
		const char* prefix = std::strncmp(libname, "lib", 3) ? "lib" : "";
		if (major >= 0 && minor >= 0)
			return fmt::format("{}{}.{}.{}.dylib", prefix, libname, major, minor);
		else if (major >= 0)
			return fmt::format("{}{}.{}.dylib", prefix, libname, major);
		else
			return fmt::format("{}{}.dylib", prefix, libname);
#else
		const char* prefix = std::strncmp(libname, "lib", 3) ? "lib" : "";
		if (major >= 0 && minor >= 0)
			return fmt::format("{}{}.so.{}.{}", prefix, libname, major, minor);
		else if (major >= 0)
			return fmt::format("{}{}.so.{}", prefix, libname, major);
		else
			return fmt::format("{}{}.so", prefix, libname);
#endif
	}

	bool DynamicLibrary::Open(const char* filename) {
#ifdef _WIN32
		handle = reinterpret_cast<void*>(LoadLibraryA(filename));
#else
		handle = dlopen(filename, RTLD_NOW);
#endif
		return handle != nullptr;
	}

	void DynamicLibrary::Close() {
		if (!IsOpen()) return;

#ifdef _WIN32
		FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
		dlclose(handle);
#endif
		handle = nullptr;
	}

	void* DynamicLibrary::GetSymbolAddress(const char* name) const {
#ifdef _WIN32
		return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
#else
		return reinterpret_cast<void*>(dlsym(handle, name));
#endif
	}

}  // namespace Common