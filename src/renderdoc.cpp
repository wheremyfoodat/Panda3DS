// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef PANDA3DS_ENABLE_RENDERDOC
#include "renderdoc.hpp"

#include <renderdoc_app.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <array>
#include <filesystem>

namespace Renderdoc {
	enum class CaptureState {
		Idle,
		Triggered,
		InProgress,
	};

	static CaptureState captureState{CaptureState::Idle};
	RENDERDOC_API_1_6_0* rdocAPI{};

	void loadRenderdoc() {
#ifdef WIN32
		// Check if we are running in Renderdoc GUI
		HMODULE mod = GetModuleHandleA("renderdoc.dll");
		if (!mod) {
			// If enabled in config, try to load RDoc runtime in offline mode
			HKEY h_reg_key;
			LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\RenderDoc.RDCCapture.1\\DefaultIcon\\", 0, KEY_READ, &h_reg_key);
			if (result != ERROR_SUCCESS) {
				return;
			}
			std::array<wchar_t, MAX_PATH> keyString{};
			DWORD stringSize{keyString.size()};
			result = RegQueryValueExW(h_reg_key, L"", 0, NULL, (LPBYTE)keyString.data(), &stringSize);
			if (result != ERROR_SUCCESS) {
				return;
			}

			std::filesystem::path path{keyString.cbegin(), keyString.cend()};
			path = path.parent_path().append("renderdoc.dll");
			const auto path_to_lib = path.generic_string();
			mod = LoadLibraryA(path_to_lib.c_str());
		}

		if (mod) {
			const auto RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
			const s32 ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdocAPI);

			if (ret != 1) {
				Helpers::panic("Invalid return value from RENDERDOC_GetAPI");
			}
		}
#else
#ifdef ANDROID
		static constexpr const char RENDERDOC_LIB[] = "libVkLayer_GLES_RenderDoc.so";
#else
		static constexpr const char RENDERDOC_LIB[] = "librenderdoc.so";
#endif
		if (void* mod = dlopen(RENDERDOC_LIB, RTLD_NOW | RTLD_NOLOAD)) {
			const auto RENDERDOC_GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
			const s32 ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdocAPI);

			if (ret != 1) {
				Helpers::panic("Invalid return value from RENDERDOC_GetAPI");
			}
		}
#endif
		if (rdocAPI) {
			// Disable default capture keys as they suppose to trigger present-to-present capturing
			// and it is not what we want
			rdocAPI->SetCaptureKeys(nullptr, 0);

			// Also remove rdoc crash handler
			rdocAPI->UnloadCrashHandler();
		}
	}

	void startCapture() {
		if (!rdocAPI) {
			return;
		}

		if (captureState == CaptureState::Triggered) {
			rdocAPI->StartFrameCapture(nullptr, nullptr);
			captureState = CaptureState::InProgress;
		}
	}

	void endCapture() {
		if (!rdocAPI) {
			return;
		}

		if (captureState == CaptureState::InProgress) {
			rdocAPI->EndFrameCapture(nullptr, nullptr);
			captureState = CaptureState::Idle;
		}
	}

	void triggerCapture() {
		if (captureState == CaptureState::Idle) {
			captureState = CaptureState::Triggered;
		}
	}

	void setOutputDir(const std::string& path, const std::string& prefix) {
		if (rdocAPI) {
			rdocAPI->SetCaptureFilePathTemplate((path + '\\' + prefix).c_str());
		}
	}
}  // namespace Renderdoc
#endif