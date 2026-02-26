//
// kiwakiwaaにより 2026/02/25 に作成されました。
//

#pragma once

#include <cstddef>
#include <functional>

namespace MKD
{
    /**
     * executes body(i) in parallel
     * @param count execute for each i in [0, count]
     * @param body body function to execute
     * @note body must be thread safe
     */
    void parallelFor(size_t count, const std::function<void(size_t)>& body);
}