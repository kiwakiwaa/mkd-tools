//
// kiwakiwaaにより 2026/02/25 に作成されました。
//

#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>

namespace MKD
{
    namespace detail
    {
        // platform specific
        void parallel_dispatch(size_t count, void* ctx, void (*fn)(size_t index, void* ctx));
    }

    template<std::invocable<size_t> F>
    void parallelFor(size_t count, F&& body)
    {
        if (count == 0) return;

        detail::parallel_dispatch(count, &body,
                                  [](size_t i, void* ctx) {
                                      (*static_cast<std::decay_t<F>*>(ctx))(i);
                                  });
    }

    // Requires random access iteration
    template<std::ranges::random_access_range R, typename F>
        requires std::invocable<F, std::ranges::range_reference_t<R> >
    void parallelFor(R&& range, F&& body)
    {
        auto begin = std::ranges::begin(range);
        parallel_for(
            static_cast<size_t>(std::ranges::distance(range)),
            [&](const size_t i) { body(begin[static_cast<std::ptrdiff_t>(i)]); }
        );
    }
}
