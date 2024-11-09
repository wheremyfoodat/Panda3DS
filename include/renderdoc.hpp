// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <string>

#include "helpers.hpp"

#ifdef PANDA3DS_ENABLE_RENDERDOC
namespace Renderdoc {
	// Loads renderdoc dynamic library module.
	void loadRenderdoc();

	// Begins a capture if a renderdoc instance is attached.
	void startCapture();

	// Ends current renderdoc capture.
	void endCapture();

	// Triggers capturing process.
	void triggerCapture();

	// Sets output directory for captures
	void setOutputDir(const std::string& path, const std::string& prefix);

	// Returns whether we've compiled with Renderdoc support
	static constexpr bool isSupported() { return true; }
}  // namespace Renderdoc
#else
namespace Renderdoc {
	static void loadRenderdoc() {}
	static void startCapture() { Helpers::panic("Tried to start a Renderdoc capture while support for renderdoc is disabled") }
	static void endCapture() { Helpers::panic("Tried to end a Renderdoc capture while support for renderdoc is disabled") }
	static void triggerCapture() { Helpers::panic("Tried to trigger a Renderdoc capture while support for renderdoc is disabled") }
	static void setOutputDir(const std::string& path, const std::string& prefix) {}
	static constexpr bool isSupported() { return false; }
}  // namespace Renderdoc
#endif

namespace Renderdoc {
	// RAII scope class that encloses a Renderdoc capture, as long as it's triggered by triggerCapture
	struct Scope {
		Scope() { Renderdoc::startCapture(); }
		~Scope() { Renderdoc::endCapture(); }

		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

		Scope(Scope&&) = delete;
		Scope& operator=(const Scope&&) = delete;
	};

	// RAII scope class that encloses a Renderdoc capture. Unlike regular Scope it doesn't wait for a trigger, it will always issue the capture
	// trigger on its own and take a capture
	struct InstantScope {
		InstantScope() {
			Renderdoc::triggerCapture();
			Renderdoc::startCapture();
		}

		~InstantScope() { Renderdoc::endCapture(); }
		
		InstantScope(const InstantScope&) = delete;
		InstantScope& operator=(const InstantScope&) = delete;

		InstantScope(InstantScope&&) = delete;
		InstantScope& operator=(const InstantScope&&) = delete;
	};
}  // namespace Renderdoc