//
// kiwakiwaaにより 2026/02/25 に作成されました。
//

#include "MKD/platform/parallel.hpp"

#include <algorithm>
#include <thread>
#include <vector>

namespace MKD
{
    void parallel_for(const size_t count, const std::function<void(size_t)>& body)
    {
        if (count == 0)
            return;

        const size_t nThreads = std::min(count, static_cast<size_t>(std::thread::hardware_concurrency()));
        if (nThreads <= 1)
        {
            for (size_t i = 0; i < count; ++i)
                body(i);
            return;
        }

        const size_t chunkSize = (count + nThreads - 1) / nThreads;
        std::vector<std::jthread> workers;
        workers.reserve(nThreads - 1);

        for (size_t t = 1; t < nThreads; ++t)
        {
            const size_t start = t * chunkSize;
            const size_t end = std::min(start + chunkSize, count);
            workers.emplace_back([&body, start, end] {
                for (size_t i = start; i < end; ++i)
                    body(i);
            });
        }

        // Use the calling thread for the first chunk
        for (size_t i = 0, end = std::min(chunkSize, count); i < end; ++i)
            body(i);

        // jthreads join automatically on destruction
    }
}
