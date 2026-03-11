//
// kiwakiwaaにより 2026/02/25 に作成されました。
//

#include "platform/parallel.hpp"

#import <dispatch/dispatch.h>

namespace MKD::detail
{
    void parallel_dispatch(size_t count, void* ctx, void(*fn)(size_t, void*))
    {
        dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);
        dispatch_apply(count, queue, ^(size_t i) { fn(i, ctx); });
    }
}