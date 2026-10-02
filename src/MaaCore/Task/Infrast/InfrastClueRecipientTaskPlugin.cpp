#include "InfrastClueRecipientTaskPlugin.h"

#include "ClueRecipient.h"
#include "Controller/Controller.h"
#include "Utils/Logger.hpp"
#include "Vision/RegionOCRer.h"

bool asst::InfrastClueRecipientTaskPlugin::verify(AsstMsg msg, const json::value& details) const
{
    if (details.get("subtask", std::string()) != "ProcessTask") {
        return false;
    }

    const auto task = details.get("details", "task", std::string());
    return (msg == AsstMsg::SubTaskCompleted &&
            (task == "InfrastClueFindRecipient" || task == "InfrastClueSelectForRecipient")) ||
           (msg == AsstMsg::SubTaskStart && infrast::clue_recipient_send_row(task).has_value());
}

std::array<std::string, 4> asst::InfrastClueRecipientTaskPlugin::read_names(const cv::Mat& image) const
{
    std::array<std::string, 4> names;
    for (size_t row = 0; row < names.size(); ++row) {
        RegionOCRer analyzer(image);
        analyzer.set_task_info("InfrastClueRecipientName" + std::to_string(row + 1));
        if (auto result = analyzer.analyze(); result && result->score >= 0.9) {
            names[row] = std::move(result->text);
        }
    }
    return names;
}

bool asst::InfrastClueRecipientTaskPlugin::_run()
{
    LogTraceFunction;

    auto* task = dynamic_cast<ProcessTask*>(m_task_ptr);
    if (!task) {
        return false;
    }

    const auto& task_name = task->get_last_task_name();
    if (task_name == "InfrastClueSelectForRecipient") {
        m_previous_names = {};
        return true;
    }

    const auto names = read_names(ctrler()->get_image());
    const auto row = infrast::find_clue_recipient(names, m_recipient);
    if (const auto send_row = infrast::clue_recipient_send_row(task_name)) {
        // 发送按钮匹配后再次读取姓名，避免翻页、重排或弹窗导致送给其他好友。
        if (!row || row != send_row) {
            LogWarn << __FUNCTION__ << "Clue recipient changed before sending; skipping" << m_recipient;
            task->set_enable(false);
        }
        return true;
    }

    // 默认流程只允许翻页或退出；仅精确、唯一命中时才加入对应行的发送按钮。
    std::vector<std::string> next { "InfrastClueRecipientNextPage", "InfrastClueCloseRecipient" };
    if (row) {
        next = { "InfrastClueSendToRecipient" + std::to_string(*row + 1), "InfrastClueCloseRecipient" };
        LogInfo << __FUNCTION__ << "Clue recipient found:" << m_recipient << "row" << *row + 1;
    }
    else if (
        names == m_previous_names || std::ranges::all_of(names, &std::string::empty) ||
        std::ranges::count(names, m_recipient) > 1) {
        next = { "InfrastClueCloseRecipient" };
        LogWarn << __FUNCTION__ << "Clue recipient not found, ambiguous or unreadable; skipping" << m_recipient;
    }
    m_previous_names = names;
    return task->override_next("InfrastClueFindRecipient", std::move(next));
}
