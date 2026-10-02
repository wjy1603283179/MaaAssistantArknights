#pragma once

#include <array>
#include <string>

#include "Task/AbstractTaskPlugin.h"

namespace asst
{
class InfrastClueRecipientTaskPlugin final : public AbstractTaskPlugin
{
public:
    using AbstractTaskPlugin::AbstractTaskPlugin;

    void set_recipient(std::string recipient) { m_recipient = std::move(recipient); }

    virtual bool verify(AsstMsg msg, const json::value& details) const override;

private:
    virtual bool _run() override;

    std::array<std::string, 4> read_names(const cv::Mat& image) const;

    std::string m_recipient;
    std::array<std::string, 4> m_previous_names;
};
}
