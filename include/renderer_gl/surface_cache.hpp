#pragma once
#include <functional>
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

    size_t size;
    size_t evictionIndex;
    std::array<SurfaceType, capacity> buffer;

public:
    void reset() {
        size = 0;
        evictionIndex = 0;
        for (auto& e : buffer) { // Free the VRAM of all surfaces
            e.free();
        }
    }

    OptionalRef find(SurfaceType& other) {
        for (auto& e : buffer) {
            if (e.matches(other) && e.valid)
                return e;
        }

        return std::nullopt;
    }

    OptionalRef findFromAddress(u32 address) {
        for (auto& e : buffer) {
            if (e.location <= address && e.location + e.sizeInBytes() > address && e.valid)
                return e;
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
				evictionIndex = (evictionIndex + 1) % capacity;

				e.valid = false;
				e.free();
				e = surface;
				e.allocate();
				return e;
			} else {
				Helpers::panic("Surface cache full! Add emptying!");
			}
		}

		size++;

		// Find an existing surface we completely invalidate and overwrite it with the new surface
		for (auto& e : buffer) {
			if (e.valid && e.range.lower() >= surface.range.lower() && e.range.upper() <= surface.range.upper()) {
				e.free();
				e = surface;
				e.allocate();
				return e;
			}
		}

		// Find an invalid entry in the cache and overwrite it with the new surface
		for (auto& e : buffer) {
			if (!e.valid) {
				e = surface;
				e.allocate();
				return e;
			}
		}

		// This should be unreachable but helps to panic anyways
		Helpers::panic("Couldn't add surface to cache\n");
	}

    SurfaceType& operator[](size_t i) {
        return buffer[i];
    }

    const SurfaceType& operator[](size_t i) const {
        return buffer[i];
    }

	void invalidateRegion(u32 start, u32 size) {
		if (size == 0) {
			return;
		}

		boost::icl::right_open_interval<u32> interval(start, start + size);

		for (auto& e : buffer) {
			if (e.valid && boost::icl::intersects(e.range, interval)) {
				e.valid = false;
				e.free();
			}
		}
	}
};
