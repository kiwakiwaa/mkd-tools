//
// kiwakiwaaにより 2026/02/25 に作成されました。
//

#include "MKD/platform/parallel.hpp"

#include <algorithm>
#include <thread>
#include <vector>

namespace MKD::detail
{
    void parallel_dispatch(const size_t count, void* ctx, void(*fn)(size_t, void*))
    {
        const auto nThreads = std::min(count, static_cast<size_t>(std::thread::hardware_concurrency()));

        if (nThreads <= 1)
        {
            for (size_t i = 0; i < count; ++i)
                fn(i, ctx);
            return;
        }

        const size_t chunk = (count + nThreads - 1) / nThreads;
        std::vector<std::jthread> workers;
        workers.reserve(nThreads - 1);

        for (size_t t = 1; t < nThreads; ++t)
        {
            const size_t start = t * chunk;
            const size_t end = std::min(start + chunk, count);
            workers.emplace_back([=, &ctx, &fn] {
                for (size_t i = start; i < end; ++i)
                    fn(i, ctx);
            });
        }

        for (size_t i = 0, end = std::min(chunk, count); i < end; ++i)
            fn(i, ctx);
    }
}
