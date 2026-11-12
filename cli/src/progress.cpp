//
// kiwakiwaaにより 2026/02/21 に作成されました。
//

#include "MKDCLI/progress.hpp"

#include <cstdio>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace MKDCLI
{
    void ExportProgress::onEvent(const MKD::ExportEvent& event)
    {
        std::visit([this]<typename ExportEvent>(const ExportEvent& e)
        {
            if constexpr (std::is_same_v<ExportEvent, MKD::ExportBeginEvent>)
                onExportBegin(e);
            else if constexpr (std::is_same_v<ExportEvent, MKD::ProgressEvent>)
                onProgress(e);
            else if constexpr (std::is_same_v<ExportEvent, MKD::PhaseEndEvent>)
                onPhaseEnd(e);
        }, event);
    }

    void ExportProgress::finish()
    {
        if (!bar_)
            return;
        if (!bar_->is_completed())
            bar_->mark_as_completed();
        bar_.reset();
    }


    void ExportProgress::onExportBegin(const MKD::ExportBeginEvent& e)
    {
#ifdef _WIN32
        const bool isTerminal = _isatty(_fileno(stderr));
#else
        const bool isTerminal = isatty(fileno(stderr));
#endif
        if (!isTerminal || e.totalItems == 0)
            return;

        grandTotal_ = e.totalItems;
        phaseOffset_ = 0;

        bar_ = std::make_unique<indicators::ProgressBar>(
            indicators::option::Stream{std::cerr},
            indicators::option::BarWidth{40},
            indicators::option::Start{"⟨"},
            indicators::option::Fill{"▓"},
            indicators::option::Lead{"▒"},
            indicators::option::Remainder{"─"},
            indicators::option::End{"⟩"},
            indicators::option::MaxProgress{grandTotal_},
            indicators::option::ShowPercentage{true},
            indicators::option::ShowElapsedTime{true},
            indicators::option::PrefixText{"EXPORTING  "}
        );
    }


    void ExportProgress::onProgress(const MKD::ProgressEvent& e) const
    {
        if (!bar_ || bar_->is_completed())
            return;

        const size_t globalCompleted = phaseOffset_ + e.completedItems;
        auto name = MKD::resourceTypeName(e.type);

        bar_->set_option(indicators::option::PostfixText{
            globalCompleted >= grandTotal_
                ? "│ Complete"
                : std::format("│ {} {}/{}", name, e.completedItems, e.totalItems)
        });
        bar_->set_progress(globalCompleted);
    }


    void ExportProgress::onPhaseEnd(const MKD::PhaseEndEvent& e)
    {
        phaseOffset_ += e.result.totalFiles;
    }
}
