// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map_controller.hpp"

#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "populate_monsters.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"

// Counter to ckeck for mob spawning. It is reset at the start of each level.
static int s_spawn_counter = 0;

// -----------------------------------------------------------------------------
// Map controllers
// -----------------------------------------------------------------------------
void MapControllerStd::on_enter()
{
        s_spawn_counter = 0;

        if (!map::g_player->m_properties.has(prop::Id::deaf)) {
                audio::try_play_ambient(1);
        }

        if (map::g_dlvl == (g_dlvl_first_mid_game / 2)) {
                const std::string msg =
                        "我没想到这里竟然深到如此地步，这不可能！而且这里有着古代文明留下的确凿痕迹，完全违背逻辑与常理！\n\n不知为何，回去的路似乎已经消失了；即使我想回去，还能回得去吗？";

                popup::Popup(popup::AddToMsgHistory::yes)
                        .set_msg(msg)
                        .run();
        }
        else if (map::g_dlvl == g_dlvl_first_mid_game) {
                const std::string msg =
                        "它永无止境！这不可能是真的！我觉得自己像在梦游；这恐怖感压得我喘不过气。";

                popup::Popup(popup::AddToMsgHistory::yes)
                        .set_msg(msg)
                        .run();
        }
        else if (map::g_dlvl == g_dlvl_longer_snd_dist) {
                const std::string msg =
                        "我听见远处传来微弱的回声。这里的声音似乎都传得更远——我得小心行事。";

                popup::Popup(popup::AddToMsgHistory::yes)
                        .set_msg(msg)
                        .run();
        }
}

void MapControllerStd::on_std_turn()
{
        ++s_spawn_counter;

        const bool has_necronomicon =
                map::g_player->m_inv.has_item_in_backpack(
                        item::Id::necronomicon);

        const int spawn_n_turns =
                has_necronomicon
                ? 200
                : 300;

        if (s_spawn_counter > spawn_n_turns) {
                populate_mon::spawn_for_repopulate_over_time();
                s_spawn_counter = 0;
        }
}

void MapControllerBoss::on_enter()
{
        audio::play(audio::SfxId::boss_voice1);

        msg_log::more_prompt();

        msg_log::add("我觉得这里已经有人知道我的存在！", colors::msg_note());

        for (auto* const actor : game_time::g_actors) {
                if (actor::is_player(actor)) {
                        continue;
                }

                actor->become_aware_player(actor::AwareSource::other);
        }
}

void MapControllerBoss::on_std_turn()
{
        const P stair_pos(map::w() - 2, 11);

        const auto terrain_at_stair_pos =
                map::g_terrain.at(stair_pos)->id();

        if (terrain_at_stair_pos == terrain::Id::stairs) {
                // Stairs already created
                return;
        }

        for (const auto* const actor : game_time::g_actors) {
                if ((actor::id(*actor) == "MON_THE_HIGH_PRIEST") && actor::is_alive(*actor)) {
                        // The boss is still alive
                        return;
                }
        }

        // The boss is dead, and stairs have not yet been created

        msg_log::add("地面隆隆作响……");

        map::update_terrain(
                terrain::make(
                        terrain::Id::stairs,
                        stair_pos));

        map::update_terrain(
                terrain::make(
                        terrain::Id::rubble_low,
                        stair_pos - P(1, 0)));

        // TODO: This was in the 'on_death' hook for TheHighPriest - it should
        // be a property if this event should still exist
        // const size_t nr_snakes = rnd::range(4, 5);

        // actor_factory::spawn(
        //         pos,
        //         {nr_snakes, "MON_PIT_VIPER"});
}

void MapControllerEgypt::on_std_turn()
{
        const int aware_dist = 9;

        if (!m_has_triggered_awareness) {
                bool is_in_aware_dist = false;

                for (actor::Actor* const actor : game_time::g_actors) {
                        if ((actor::id(*actor) == "MON_KHEPHREN") &&
                            (king_dist(map::g_player->m_pos, actor->m_pos) <= aware_dist)) {
                                is_in_aware_dist = true;
                                break;
                        }
                }

                if (is_in_aware_dist) {
                        msg_log::more_prompt();

                        msg_log::add(
                                "我觉得这里已经有人知道我的存在！",
                                colors::msg_note());

                        for (auto* const actor : game_time::g_actors) {
                                if (actor::is_player(actor)) {
                                        continue;
                                }

                                actor->become_aware_player(actor::AwareSource::other);

                                actor->m_ai_state.is_roaming_allowed = MonRoamingAllowed::yes;
                        }

                        m_has_triggered_awareness = true;
                }
        }
}

void MapControllerEgypt::on_enter()
{
        const std::string msg =
                "我继续深入，却被周围环境的骤变惊住了。这间石室的古老建筑风格前所未见，石墙上刻着精美的象形文字。\n\n空气中弥漫着沙土和陌生熏香的气味，仿佛置身于另一个时空。沉闷的寂静令人窒息，只有我的脚步声在石地上回荡。\n\n我凑近观察那些刻纹，不禁好奇究竟是哪个文明创造了它们。他们是谁？又如何在如此深的地下建造出这种地方？";

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_msg(msg)
                .run();
}

void MapControllerDeepOneLair::on_enter()
{
        const std::string msg =
                "这里的墙壁湿滑，空气中弥漫着沉重的盐水与腐朽气息。我能感到皮肤上的湿气，呼吸也变得短促。\n\n有隧道通往更深处的地下水道，但我只能猜测它们通向何方。整个洞穴系统似乎彼此相连，通道也许会通往海洋深处，或地下湖泊与河流。";

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_msg(msg)
                .run();
}

// -----------------------------------------------------------------------------
// map_control
// -----------------------------------------------------------------------------
namespace map_control
{
std::unique_ptr<MapController> g_controller = nullptr;

}  // namespace map_control
