// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "common_text.hpp"

namespace common_text
{
const std::string g_next_page_up_hint =
        "[上页, <] 更多";

const std::string g_next_page_down_hint =
        "[下页, >] 更多";

const std::string g_screen_exit_hint =
        "[空格, Esc] 退出";

const std::string g_minimap_exit_hint =
        "[空格, Esc, m] 退出";

const std::string g_game_over_summary_exit_hint =
        "[空格, Esc] 显示高分";

const std::string g_set_option_hint =
        "[回车, 左, 右] 设置选项";

const std::string g_scroll_hint =
        "[2/8, 下/上, 上页/下页, 首页/末页] 滚动";

const std::string g_scrollable_info_screen_hint =
        g_scroll_hint +
        " " +
        g_screen_exit_hint;

const std::string g_cancel_hint =
        "[空格, Esc] 取消";

const std::string g_confirm_hint =
        "[空格, Esc, 回车] 继续";

const std::string g_confirm_drop_hint =
        "[回车] 确认";

const std::string g_any_key_hint =
        "[任意键] 继续";

const std::string g_yes_or_no_hint =
        "[y/n]";

const std::string g_direction_query =
        "哪个方向？";

const std::string g_disarm_no_trap =
        "我发现那里没有什么可以解除武装的。";

const std::string g_mon_prevent_cmd =
        "当敌人靠近时就不会。";

const std::string g_shock_prevent_cmd =
        "当疯狂临近时就不会了。";

const std::string g_fire_prevent_cmd =
        "火势正在蔓延！";

const std::string g_mon_disappear =
        "突然消失了！";

const std::string g_mon_disappear_reappear =
        "突然消失又出现！";

const std::string g_miscast_player =
        "我无法集中注意力！";

const std::string g_miscast_mon =
        "无法集中注意力。";

const std::vector<std::string> g_exorcist_purge_phrases = {
        "这个地方现在感觉更加宁静了。",
        "这个地方的神圣性已经有所恢复。",
        "一大恶业已被消灭。",
        "我感觉到整个区域弥漫着寂静。"};

}  // namespace common_text
