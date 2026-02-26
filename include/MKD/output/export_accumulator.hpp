//
// kiwakiwaaにより 2026/02/25 に作成されました。
//

#pragma once

#include "MKD/output/event.hpp"
#include "MKD/output/export_result.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace MKD
{
    class ExportAccumulator
    {
    public:
        ExportAccumulator(size_t total, ResourceType type, const ExportCallback& callback);

        void recordExport(uint64_t bytes);

        void recordSkip();

        void recordFailure(std::string error);

        ExportResult finalize();

    private:
        void maybeReport();

        void report();

        size_t completed() const;

        std::atomic<size_t> exported_{0};
        std::atomic<size_t> skipped_{0};
        std::atomic<size_t> failed_{0};
        std::atomic<uint64_t> totalBytes_{0};

        std::mutex mutex_;
        std::vector<std::string> errors_;

        const size_t totalItems_;
        const size_t reportInterval_;
        const ResourceType type_;
        const ExportCallback& callback_;
    };
}
