// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <string>
#include <vector>

#include "catch.hpp"
#include "text.hpp"

TEST_CASE("Text with color codes, no split")
{
        std::string str = "aaa {COLOR_LIGHT_MAGENTA}bbbbb{color_reset} cc";

        Text text(str);

        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Text with color codes, split")
{
        std::string str =
                "aaa {COLOR_LIGHT_MAGENTA}b {COLOR_LIGHT_CYAN}cc "
                "ddd eee f";

        Text text(str);

        text.set_w(9);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "b");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_cyan());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "ddd");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "eee");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "f");
}

TEST_CASE("Split before space")
{
        std::string str = "aaa {COLOR_LIGHT_MAGENTA}bbbbb{color_reset} cc";

        Text text(str);

        text.set_w(9);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Split after space")
{
        std::string str = "aaa {COLOR_LIGHT_MAGENTA}bbbbb{color_reset} cc";

        Text text(str);

        text.set_w(10);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Split before non-breaking space")
{
        std::string str = "aaa {COLOR_LIGHT_MAGENTA}bbbbb{color_reset}{_}cc";

        Text text(str);

        text.set_w(9);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Split after non-breaking space")
{
        std::string str = "aaa {COLOR_LIGHT_MAGENTA}bbbbb{color_reset}{_}cc";

        Text text(str);

        text.set_w(10);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Text with color codes, split before dot")
{
        std::string str = "aaa {COLOR_LIGHT_MAGENTA}bbbbb{color_reset}. cc";

        Text text(str);

        text.set_w(9);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == ".");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Text with newlines")
{
        std::string str = "aaa bbbbb cc\n\nddddd eeee";

        Text text(str);

        text.set_w(14);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "ddddd");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "eeee");
}

#include "utf8.hpp"
#include "text_format.hpp"

TEST_CASE("Chinese display columns and complete-character deletion")
{
        REQUIRE(utf8::width("玩家A") == 5);
        REQUIRE(utf8::visible_width("{COLOR_RED}玩家{reset_color}") == 4);
        std::string name = "玩家";
        utf8::pop_back(name);
        REQUIRE(name == "玩");
        utf8::pop_back(name);
        REQUIRE(name.empty());
        REQUIRE(utf8::next(std::string("\xE4\xB8", 2), 0).cp == 0xfffd);
        REQUIRE(utf8::width("A\xE2\x80\x8B" "B") == 2);
}

TEST_CASE("Chinese wrapping preserves UTF-8 across narrow columns")
{
        const auto rows = text_format::split("玩家持有A物品", 5);
        std::string restored;
        for (const auto& row : rows) {
                REQUIRE(utf8::width(row) <= 5);
                for (size_t p = 0; p < row.size();) {
                        auto g = utf8::next(row, p);
                        REQUIRE(g.cp != 0xfffd);
                        p = g.end;
                }
                restored += row;
        }
        REQUIRE(restored == "玩家持有A物品");
}
