#pragma once

#include <Metal/Metal.hpp>

#define GET_HELPER_TEXTURE_BINDING(binding) (30 - binding)
#define GET_HELPER_SAMPLER_STATE_BINDING(binding) (15 - binding)
