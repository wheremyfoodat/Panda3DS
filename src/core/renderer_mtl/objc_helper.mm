#include "renderer_mtl/objc_helper.hpp"

// TODO: change the include
#import <Metal/Metal.h>

namespace Metal {

dispatch_data_t createDispatchData(const void* data, size_t size) {
    return dispatch_data_create(data, size, dispatch_get_global_queue(0, 0), ^{});
}

} // namespace Metal
