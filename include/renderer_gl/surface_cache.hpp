#pragma once
#include <array>
#include <functional>
#include <map>
#include <optional>

#include "surfaces.hpp"
#include "textures.hpp"

// Surface cache class that can fit "capacity" instances of the "SurfaceType" class of surfaces
// SurfaceType *must* have all of the following.
// - An "allocate" function that allocates GL resources for the surfaces. On overflow it will panic
//   if evictOnOverflow is false, or kick out the oldest item if it is true (like a ring buffer)
// - A "free" function that frees up all resources the surface is taking up
// - A "matches" function that, when provided with a SurfaceType object reference
// Will tell us if the 2 surfaces match (Only as far as location in VRAM, format, dimensions, etc)
// Are concerned. We could overload the == operator, but that implies full equality
// Including equality of the allocated OpenGL resources, which we don't want
// - A "valid" member that tells us whether the function is still valid or not
// - A "location" member which tells us which location in 3DS memory this surface occupies
template <typename SurfaceType, size_t capacity, bool evictOnOverflow = false>
class SurfaceCache {
	// Vanilla std::optional can't hold actual references
	using OptionalRef = std::optional<std::reference_wrapper<SurfaceType>>;

	size_t size = 0;
	size_t evictionIndex = 0;
	std::array<SurfaceType, capacity> buffer;

	// Map from address to surface pointer
	std::multimap<u32, SurfaceType*> addressTree;

	void indexSurface(SurfaceType& surface) { addressTree.emplace(surface.location, &surface); }

	void unindexSurface(SurfaceType& surface) {
		auto range = addressTree.equal_range(surface.location);
		for (auto it = range.first; it != range.second; ++it) {
			if (it->second == &surface) {
				addressTree.erase(it);
				break;
			}
		}
	}

  public:
	void reset() {
		size = 0;
		evictionIndex = 0;
		addressTree.clear();
		for (auto& e : buffer) {
			e.free();
			e.valid = false;
		}
	}

	OptionalRef find(SurfaceType& other) {
		auto range = addressTree.equal_range(other.location);
		for (auto it = range.first; it != range.second; ++it) {
			SurfaceType* candidate = it->second;
			if (candidate->valid && candidate->matches(other)) {
				return *candidate;
			}
		}

		return std::nullopt;
	}

	OptionalRef findFromAddress(u32 address) {
		for (auto it = addressTree.begin(); it != addressTree.end(); ++it) {
			SurfaceType* surface = it->second;
			if (surface->valid && surface->location <= address && surface->location + surface->sizeInBytes() > address) {
				return *surface;
			}
		}

		return std::nullopt;
	}

	// Adds a surface object to the cache and returns it
	SurfaceType& add(const SurfaceType& surface) {
		if (size >= capacity) {
			if constexpr (evictOnOverflow) {  // Do a ring buffer if evictOnOverflow is true
				if constexpr (std::is_same<SurfaceType, ColourBuffer>() || std::is_same<SurfaceType, DepthBuffer>()) {
					Helpers::panicDev("Colour/Depth buffer cache overflowed, currently stubbed to do a ring-buffer. This might snap in half");
				}

				auto& e = buffer[evictionIndex];
				unindexSurface(e);
				evictionIndex = (evictionIndex + 1) % capacity;

				e.valid = false;
				e.free();
				e = surface;
				e.allocate();
				indexSurface(e);
				return e;
			} else {
				Helpers::panic("Surface cache full! Add emptying!");
			}
		}

		size++;

		// See if any existing surface fully overlaps
		for (auto& e : buffer) {
			if (e.valid && e.range.lower() >= surface.range.lower() && e.range.upper() <= surface.range.upper()) {
				unindexSurface(e);
				e.free();
				e = surface;
				e.allocate();
				indexSurface(e);
				return e;
			}
		}

		// Find an invalid entry in the cache and overwrite it with the new surface
		for (auto& e : buffer) {
			if (!e.valid) {
				e = surface;
				e.allocate();
				indexSurface(e);
				return e;
			}
		}

		// This should be unreachable but helps to panic anyways
		Helpers::panic("Couldn't add surface to cache\n");
	}

	SurfaceType& operator[](size_t i) { return buffer[i]; }
	const SurfaceType& operator[](size_t i) const { return buffer[i]; }
};
