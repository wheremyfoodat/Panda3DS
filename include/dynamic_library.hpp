// SPDX-FileCopyrightText: 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace Common {
	/**
	 * Provides a platform-independent interface for loading a dynamic library and retrieving symbols.
	 * The interface maintains an internal reference count to allow one handle to be shared between
	 * multiple users.
	 */
	class DynamicLibrary final {
	  public:
		/// Default constructor, does not load a library.
		explicit DynamicLibrary();

		/// Automatically loads the specified library. Call IsOpen() to check validity before use.
		explicit DynamicLibrary(const char* filename);

		/// Initializes the dynamic library with an already opened handle.
		explicit DynamicLibrary(void* handle_);

		/// Moves the library.
		DynamicLibrary(DynamicLibrary&&) noexcept;
		DynamicLibrary& operator=(DynamicLibrary&&) noexcept;

		/// Delete copies, we can't copy a dynamic library.
		DynamicLibrary(const DynamicLibrary&) = delete;
		DynamicLibrary& operator=(const DynamicLibrary&) = delete;

		/// Closes the library.
		~DynamicLibrary();

		/// Returns the specified library name with the platform-specific suffix added.
		[[nodiscard]] static std::string getUnprefixedFilename(const char* filename);

		/// Returns the specified library name in platform-specific format.
		/// Major/minor versions will not be included if set to -1.
		/// If libname already contains the "lib" prefix, it will not be added again.
		/// Windows: LIBNAME-MAJOR-MINOR.dll
		/// Linux: libLIBNAME.so.MAJOR.MINOR
		/// Mac: libLIBNAME.MAJOR.MINOR.dylib
		[[nodiscard]] static std::string getVersionedFilename(const char* libname, int major = -1, int minor = -1);

		/// Returns true if a module is loaded, otherwise false.
		[[nodiscard]] bool isOpen() const { return handle != nullptr; }

		/// Loads (or replaces) the handle with the specified library file name.
		/// Returns true if the library was loaded and can be used.
		[[nodiscard]] bool open(const char* filename);

		/// Unloads the library, any function pointers from this library are no longer valid.
		void close();

		/// Returns the address of the specified symbol (function or variable) as an untyped pointer.
		/// If the specified symbol does not exist in this library, nullptr is returned.
		[[nodiscard]] void* getSymbolAddress(const char* name) const;

		/// Obtains the address of the specified symbol, automatically casting to the correct type.
		/// Returns true if the symbol was found and assigned, otherwise false.
		template <typename T>
		[[nodiscard]] bool getSymbol(const char* name, T* ptr) const {
			*ptr = reinterpret_cast<T>(getSymbolAddress(name));
			return *ptr != nullptr;
		}

	  private:
		/// Platform-dependent data type representing a dynamic library handle.
		void* handle = nullptr;
	};
}  // namespace Common