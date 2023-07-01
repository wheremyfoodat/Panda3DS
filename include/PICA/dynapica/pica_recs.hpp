#pragma once
#include "helpers.hpp"
#include "vertex_loader_rec.hpp"

// Common file for our PICA JITs (From vertex config -> CPU assembly and from PICA shader -> CPU assembly)

namespace Dynapica {
#ifdef PANDA3DS_DYNAPICA_SUPPORTED
	static constexpr bool supported() { return true; }
#else
	static constexpr bool supported() { return false; }
#endif
}