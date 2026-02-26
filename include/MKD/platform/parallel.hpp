//
// kiwakiwaaにより 2026/02/25 に作成されました。
//

#pragma once

#include <cstddef>
#include <functional>

namespace MKD
{
    /// Executes body(i) for i in [0, count), distributing work across available cores.
    /// The callable must be thread-safe. Blocks until all iterations complete.
    void parallel_for(size_t count, const std::function<void(size_t)>& body);
}
