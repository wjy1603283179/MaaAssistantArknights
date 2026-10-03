#pragma once

#include <array>
#include <string>

#include "ClueRecipient.h"
#include "Task/AbstractTaskPlugin.h"

namespace asst
{
class InfrastClueRecipientTaskPlugin final : public AbstractTaskPlugin
{
public:
    using AbstractTaskPlugin::AbstractTaskPlugin;

    void set_recipient(std::string recipient) { m_recipient = std::move(recipient); }

    bool is_recipient_unavailable() const noexcept { return m_recipient_unavailable; }

    virtual bool verify(AsstMsg msg, const json::value& details) const override;

private:
    virtual bool _run() override;

    std::array<std::string, 4> read_names(const cv::Mat& image) const;

    std::string m_recipient;
    infrast::ClueRecipientPageTracker m_pages;
    bool m_search_started = false;
    bool m_recipient_unavailable = false;
};
}
