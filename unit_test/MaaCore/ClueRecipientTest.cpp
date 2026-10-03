#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>

#include "Task/Infrast/ClueRecipient.h"

TEST_CASE("Clue recipient searches stop when pagination wraps around", "[clue-recipient]")
{
    asst::infrast::ClueRecipientPageTracker pages;
    const std::array<std::string, 4> first { "Doctor#1234", "", "", "" };
    const std::array<std::string, 4> second { "Doctor#4321", "", "", "" };
    REQUIRE(pages.visit(first));
    REQUIRE(pages.visit(second));
    REQUIRE_FALSE(pages.visit(first));
    REQUIRE_FALSE(pages.visit(second));
    pages.reset();
    REQUIRE(pages.visit(first));
    REQUIRE_FALSE(pages.visit(first));
}

TEST_CASE("Only recipient send buttons trigger the pre-send guard", "[clue-recipient]")
{
    using asst::infrast::clue_recipient_send_row;
    for (size_t row = 0; row < 4; ++row) {
        const auto task = "InfrastClueSendToRecipient" + std::to_string(row + 1);
        REQUIRE(clue_recipient_send_row(task) == row);
        REQUIRE_FALSE(clue_recipient_send_row(task + "@LoadingText"));
    }
    REQUIRE_FALSE(clue_recipient_send_row("InfrastClueSendToNamedRecipient"));
    REQUIRE_FALSE(clue_recipient_send_row("InfrastClueSendToRecipient0"));
    REQUIRE_FALSE(clue_recipient_send_row("InfrastClueSendToRecipient5"));
    REQUIRE_FALSE(clue_recipient_send_row("InfrastClueSendToRecipient"));
}

TEST_CASE("Clue recipients require a complete player name", "[clue-recipient]")
{
    using asst::infrast::is_valid_clue_recipient;
    REQUIRE(is_valid_clue_recipient("博士#0123"));
    REQUIRE(is_valid_clue_recipient("Doctor#0000"));
    REQUIRE(is_valid_clue_recipient("Doctor#9999"));
    REQUIRE_FALSE(is_valid_clue_recipient(""));
    REQUIRE_FALSE(is_valid_clue_recipient("Doctor"));
    REQUIRE_FALSE(is_valid_clue_recipient("#1234"));
    REQUIRE_FALSE(is_valid_clue_recipient("Doctor#123"));
    REQUIRE_FALSE(is_valid_clue_recipient("Doctor#12345"));
    REQUIRE_FALSE(is_valid_clue_recipient("Doctor#１２３４"));
    REQUIRE_FALSE(is_valid_clue_recipient("Doctor#12O4"));
    REQUIRE_FALSE(is_valid_clue_recipient("Doctor\n#1234"));
}

TEST_CASE("Clue recipient matching distinguishes names and discriminators", "[clue-recipient]")
{
    const std::array<std::string, 4> names { "Doctor#1234", "Doctor#4321", "MyDoctor#1234", "博士#0123" };
    using asst::infrast::find_clue_recipient;
    REQUIRE(find_clue_recipient(names, "Doctor#1234") == 0);
    REQUIRE(find_clue_recipient(names, "Doctor#4321") == 1);
    REQUIRE(find_clue_recipient(names, "MyDoctor#1234") == 2);
    REQUIRE(find_clue_recipient(names, "博士#0123") == 3);
    REQUIRE_FALSE(find_clue_recipient(names, "Doctor"));
    REQUIRE_FALSE(find_clue_recipient(names, "doctor#1234"));
    REQUIRE_FALSE(find_clue_recipient(names, "Doctor#5678"));
    REQUIRE_FALSE(find_clue_recipient(names, ".*#1234"));
}

TEST_CASE("Ambiguous or unreadable clue recipients never select a row", "[clue-recipient]")
{
    using asst::infrast::find_clue_recipient;
    const std::array<std::string, 4> duplicate { "Doctor#1234", "", "Doctor#1234", "" };
    REQUIRE_FALSE(find_clue_recipient(duplicate, "Doctor#1234"));
    const std::array<std::string, 4> unreadable {};
    REQUIRE_FALSE(find_clue_recipient(unreadable, "Doctor#1234"));
    const std::array<std::string, 4> partial { "", "", "博士#0123", "" };
    REQUIRE(find_clue_recipient(partial, "博士#0123") == 2);
}
