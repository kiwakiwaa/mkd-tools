//
// kiwakiwaaにより 2026/02/26 に作成されました。
//

#include "MKD/output/export_accumulator.hpp"


namespace MKD
{
    ExportAccumulator::ExportAccumulator(const size_t total, const ResourceType type, const ExportCallback& callback)
        : totalItems_(total),
          reportInterval_(std::max<size_t>(50, total / 400)),
          type_(type),
          callback_(callback)
    {
    }


    void ExportAccumulator::recordExport(const uint64_t bytes)
    {
        exported_.fetch_add(1, std::memory_order_relaxed);
        totalBytes_.fetch_add(bytes, std::memory_order_relaxed);
        maybeReport();
    }


    void ExportAccumulator::recordSkip()
    {
        skipped_.fetch_add(1, std::memory_order_relaxed);
        maybeReport();
    }


    void ExportAccumulator::recordFailure(std::string error)
    {
        failed_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(mutex_);
        errors_.push_back(std::move(error));
    }


    ExportResult ExportAccumulator::finalize()
    {
        // Only send the final report if the last item didn't already trigger one
        if (const size_t done = completed(); done % reportInterval_ != 0 || done == 0)
            report();

        ExportResult result;
        result.totalResources = totalItems_;
        result.exported = exported_.load();
        result.skipped = skipped_.load();
        result.failed = failed_.load();
        result.totalBytes = totalBytes_.load();
        result.errors = std::move(errors_);
        return result;
    }


    void ExportAccumulator::maybeReport()
    {
        if (const size_t done = completed(); done % reportInterval_ != 0)
            return;
        report();
    }


    void ExportAccumulator::report()
    {
        if (!callback_)
            return;

        std::lock_guard lock(mutex_);
        callback_(ProgressEvent{
            .type = type_,
            .completedItems = completed(),
            .totalItems = totalItems_,
            .bytesWritten = totalBytes_.load(std::memory_order_relaxed)
        });
    }


    size_t ExportAccumulator::completed() const
    {
        return exported_.load(std::memory_order_relaxed)
             + skipped_.load(std::memory_order_relaxed)
             + failed_.load(std::memory_order_relaxed);
    }
}
