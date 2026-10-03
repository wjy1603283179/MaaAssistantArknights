#include "InfrastClueRecipientTaskPlugin.h"

#include "ClueRecipient.h"
#include "Controller/Controller.h"
#include "Utils/Logger.hpp"
#include "Vision/OCRer.h"

bool asst::InfrastClueRecipientTaskPlugin::verify(AsstMsg msg, const json::value& details) const
{
    if (m_using_default_strategy || details.get("subtask", std::string()) != "ProcessTask") {
        return false;
    }

    const auto task = details.get("details", "task", std::string());
    return (msg == AsstMsg::SubTaskCompleted &&
            (task == "InfrastClueFindRecipient" || task == "InfrastClueSelectForRecipient" ||
             task == "InfrastClueSendToNamedRecipient" || task == "InfrastClueCloseRecipient" ||
             task == "InfrastClueRecipientNotFound" || task == "InfrastClueFallbackRecipient")) ||
           (msg == AsstMsg::SubTaskStart && infrast::clue_recipient_send_row(task).has_value());
}

std::array<std::string, 4> asst::InfrastClueRecipientTaskPlugin::read_names(const cv::Mat& image) const
{
    std::array<std::string, 4> names;
    const auto nickname = m_recipient.substr(0, m_recipient.rfind('#'));
    for (size_t row = 0; row < names.size(); ++row) {
        OCRer analyzer(image);
        analyzer.set_task_info("InfrastClueRecipientName" + std::to_string(row + 1));
        const auto results = analyzer.analyze();
        if (!results) {
            continue;
        }
        for (const auto& result : *results) {
            if (result.score < 0.9) {
                continue;
            }
            auto name = result.text;
            if (name == nickname) {
                // 昵称与编号可能被检测为分离文本；编号使用字符模型，避免名片背景干扰中文模型。
                OCRer discriminator(image);
                discriminator.set_task_info("InfrastClueRecipientDiscriminator");
                discriminator.set_roi(
                    { result.rect.x + result.rect.width + 4,
                      result.rect.y - 2,
                      result.rect.height * 4,
                      result.rect.height + 4 });
                if (auto tag = discriminator.analyze(); tag && tag->front().score >= 0.9) {
                    name += tag->front().text;
                }
            }
            if (!infrast::is_valid_clue_recipient(name)) {
                continue;
            }
            if (!names[row].empty() && names[row] != name) {
                names[row].clear();
                break;
            }
            names[row] = std::move(name);
        }
    }
    return names;
}

bool asst::InfrastClueRecipientTaskPlugin::on_recipient_not_found(ProcessTask& task)
{
    const auto action = m_search_attempts.record_not_found();
    LogWarn << __FUNCTION__ << "Clue recipient not found:" << m_recipient << "attempt"
            << m_search_attempts.not_found_count();
    return task.override_next(
        "InfrastClueRecipientNotFound",
        { action == infrast::ClueRecipientSearchAttempts::Action::Retry ? "InfrastClueRetryRecipient"
                                                                        : "InfrastClueFallbackRecipient" });
}

bool asst::InfrastClueRecipientTaskPlugin::_run()
{
    LogTraceFunction;

    auto* task = dynamic_cast<ProcessTask*>(m_task_ptr);
    if (!task) {
        return false;
    }

    const auto& task_name = task->get_last_task_name();
    if (task_name == "InfrastClueRecipientNotFound") {
        return on_recipient_not_found(*task);
    }
    if (task_name == "InfrastClueFallbackRecipient") {
        // 本次流程的满库存分支也须恢复原版逻辑，不修改保存的好友名称。
        if (!task->remove_override_next("CloseCluePageThenSendClue")) {
            return false;
        }
        m_using_default_strategy = true;
        LogWarn << __FUNCTION__ << "Clue recipient search failed twice; using the default gifting strategy";
        return true;
    }
    if (task_name == "InfrastClueSendToNamedRecipient") {
        m_search_started = false;
        return true;
    }
    if (task_name == "InfrastClueSelectForRecipient") {
        m_pages.reset();
        return true;
    }
    if (task_name == "InfrastClueCloseRecipient") {
        m_recipient_unavailable |= m_search_started;
        return true;
    }

    const auto names = read_names(ctrler()->get_image());
    const auto row = infrast::find_clue_recipient(names, m_recipient);
    if (const auto send_row = infrast::clue_recipient_send_row(task_name)) {
        // 发送按钮匹配后再次读取姓名，避免翻页、重排或弹窗导致送给其他好友。
        if (!row || row != send_row) {
            LogWarn << __FUNCTION__ << "Clue recipient changed before sending; skipping" << m_recipient;
            m_recipient_unavailable = true;
            task->set_enable(false);
        }
        return true;
    }

    // 默认流程只允许翻页或退出；仅精确、唯一命中时才加入对应行的发送按钮。
    m_search_started = true;
    std::vector<std::string> next { "InfrastClueRecipientNextPage", "InfrastClueRecipientNotFound" };
    if (row) {
        m_search_attempts.reset();
        next = { "InfrastClueSendToRecipient" + std::to_string(*row + 1), "InfrastClueCloseRecipient" };
        LogInfo << __FUNCTION__ << "Clue recipient found:" << m_recipient << "row" << *row + 1;
    }
    else if (std::ranges::all_of(names, &std::string::empty) || std::ranges::count(names, m_recipient) > 1) {
        next = { "InfrastClueCloseRecipient" };
        m_recipient_unavailable = true;
        LogWarn << __FUNCTION__ << "Clue recipient names are ambiguous or unreadable; skipping" << m_recipient;
    }
    else if (!m_pages.visit(names)) {
        next = { "InfrastClueRecipientNotFound" };
    }
    return task->override_next("InfrastClueFindRecipient", std::move(next));
}
