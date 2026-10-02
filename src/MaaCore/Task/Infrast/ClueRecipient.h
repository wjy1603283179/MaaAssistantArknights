#pragma once

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace asst::infrast
{
inline std::optional<size_t> clue_recipient_send_row(std::string_view task)
{
    constexpr std::string_view prefix = "InfrastClueSendToRecipient";
    if (task.size() != prefix.size() + 1 || !task.starts_with(prefix) || task.back() < '1' || task.back() > '4') {
        return std::nullopt;
    }
    return task.back() - '1';
}

inline bool is_valid_clue_recipient(std::string_view name)
{
    const auto separator = name.rfind('#');
    return separator != std::string_view::npos && separator > 0 && name.size() - separator == 5 &&
           std::ranges::none_of(name, [](unsigned char ch) { return ch < 32 || ch == 127; }) &&
           std::ranges::all_of(name.substr(separator + 1), [](char ch) { return ch >= '0' && ch <= '9'; });
}

inline std::optional<size_t> find_clue_recipient(std::span<const std::string> names, std::string_view recipient)
{
    if (!is_valid_clue_recipient(recipient)) {
        return std::nullopt;
    }

    std::optional<size_t> row;
    for (size_t index = 0; index < names.size(); ++index) {
        if (names[index] != recipient) {
            continue;
        }
        if (row) {
            return std::nullopt;
        }
        row = index;
    }
    return row;
}
}
