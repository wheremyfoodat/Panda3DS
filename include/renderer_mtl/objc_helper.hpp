#pragma once

#include <Metal/Metal.hpp>

namespace Metal {

dispatch_data_t createDispatchData(const void* data, size_t size);

} // namespace Metal

// Cast from std::string to NS::String*
inline NS::String* toNSString(const std::string& str) {
    return NS::String::string(str.c_str(), NS::ASCIIStringEncoding);
}
