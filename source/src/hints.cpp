// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "hints.hpp"

#include <cstring>
#include <string>
#include <utility>

#include "actor.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "io.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "popup.hpp"
#include "saving.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool s_hints_displayed[(size_t)hints::Id::END];

static const std::string s_title_prefix = "提示： ";

static std::pair<std::string, std::string> id_to_text(const hints::Id id)
{
        switch (id) {
        case hints::Id::altars:
                return {
                        "祭坛",
                        "站在祭坛旁时，所有法术的施放等级都会提高，包括从手稿和记忆中施放的法术。"};

        case hints::Id::fountains:
                return {
                        "喷泉",
                        "饮用喷泉通常会恢复少量生命值、精神值和精神震慑（但有时也会产生其他效果，有好有坏！）。喷泉可以多次饮用，但每次饮用都有一定几率永久干涸。"};

        case hints::Id::destroying_corpses:
                return {
                        "摧毁尸体",
                        "按 [k] 或 [w] 可以摧毁尸体。这对某些怪物很有用。砍刀等武器能更轻易地摧毁尸体——查看物品说明，确认武器是否带有此加成。放置得当的炸药或燃烧瓶通常也能有效阻止难缠的怪物。"};

        case hints::Id::unload_weapons:
                return {
                        "卸载武器",
                        "可以按 [u] 或 [G] 卸下地面枪械中的弹药。"};

        case hints::Id::infected:
                return {
                        "已感染",
                        "感染应尽早治疗。常见的处理方法是使用医疗包。治疗只需少量回合和资源，但若过程中被打断，之前的努力就会白费（不过中断时不会消耗医疗资源）。\n\n感染若不治疗，最终会发展为疾病（生命值上限降低50%），只能通过特殊方式清除，例如饮用某些药水。"};

        case hints::Id::overburdened:
                return {
                        "负重过多",
                        "携带重量过大会使移动耗时翻倍。这种情况极其危险且有害。"};

        case hints::Id::high_shock:
                return {
                        "高震慑",
                        "处于极度精神震慑（压力、偏执）状态会损害理智。降低震慑、从而避免或推迟理智受损的一种方法是寻找光源，例如开启电灯笼或点燃照明棒。"};

        case hints::Id::status_effects:
                return {
                        "状态效果",
                        "已施加一种状态效果。状态效果是施加于生物身上的正面、负面或中性效果，例如困惑、燃烧、隐形或电击抗性。游戏主画面会简单列出当前生效的状态效果。\n\n在角色界面（按 [C] 或 [@] 打开）中，可以查看更详细的列表及每种效果的说明。\n\n以大写字母显示的状态效果属于“永久”效果，只有采取某些特殊行动才会移除，例如使用医疗包治疗创伤。"};

        case hints::Id::study_inscription:
                return {
                        "铭文",
                        "这里有一段铭文，研究它可以获得一些经验。\n\n它还可能让你回忆起遗忘的法术，或揭示随身携带的手稿、药水的信息。你携带的未知物品越多，揭示这类信息的几率就越高。"};

        case hints::Id::kick_brazier:
                return {
                        "踢火盆",
                        "踢翻火盆可在小范围内点燃生物。"};

        case hints::Id::kick_statue:
                return {
                        "踢雕像",
                        "踢倒雕像可以伤害并震晕另一侧的生物。"};

        case hints::Id::temporary_and_permanent_shock:
                return {
                        "暂时性与永久性精神震慑",

                        "某些情况会造成“暂时性”精神震慑，情况改变后便会消退。例如进入黑暗区域或站在血迹旁时，震慑值会飙升，直到你离开。\n\n身处明亮区域也会降低震慑值，直到你重新回到地下的昏暗环境。\n\n看到怪物、施放法术、消磨时间等都会造成“永久性”震慑；它不会自行消失，除非抵达下一层、疯狂度上升（震慑达到100%时）或通过某种方式治愈。"};

        default:
                ASSERT(false);
                return {"", ""};
        }
}

static bool should_display_hint(const hints::Id id)
{
        const auto hints_mode = config::hints_mode();

        switch (hints_mode) {
        case HintsMode::once_per_game:
                return !s_hints_displayed[(size_t)id];

        case HintsMode::once:
                return !config::has_seen_hint_global(id);

        case HintsMode::never:
                return false;

        case HintsMode::END:
                break;
        }

        ASSERT(false);

        return false;
}

// -----------------------------------------------------------------------------
// hints
// -----------------------------------------------------------------------------
namespace hints
{
void init()
{
        memset(s_hints_displayed, 0, sizeof(s_hints_displayed));
}

void save()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                saving::put_bool(s_hints_displayed[i]);
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                s_hints_displayed[i] = saving::get_bool();
        }
}

void display(const Id id)
{
        if (!should_display_hint(id)) {
                return;
        }

        if (!actor::is_alive(*map::g_player)) {
                return;
        }

        msg_log::more_prompt();

        states::draw();
        io::update_screen();

        io::sleep(100);

        const auto text = id_to_text(id);

        if (text.second.empty()) {
                ASSERT(false);

                return;
        }

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_title(s_title_prefix + text.first)
                .set_msg(text.second)
                .run();

        s_hints_displayed[(size_t)id] = true;

        config::set_hint_seen_global(id);
}

}  // namespace hints
