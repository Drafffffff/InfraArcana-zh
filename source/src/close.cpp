// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "close.hpp"

#include <string>

#include "actor.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "direction.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "query.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void player_try_close_or_jam_terrain(terrain::Terrain* const terrain)
{
        const bool can_see_terrain = map::g_seen.at(terrain->pos());

        if (terrain->id() != terrain::Id::door) {
                if (can_see_terrain) {
                        msg_log::add(
                                "我看不到那里有什么可以关上或卡住的东西。");
                }
                else {
                        msg_log::add(
                                "那里没有东西可关，也没有东西可堵。");
                }

                return;
        }

        // This is a door

        auto* const door = static_cast<terrain::Door*>(terrain);

        if (door->is_open()) {
                door->actor_try_close(*map::g_player);

                return;
        }

        // Door is closed - try to jam it

        if ((door->type() == terrain::DoorType::metal) ||
            door->is_warded()) {
                if (can_see_terrain) {
                        msg_log::add("这扇门无法卡住。");
                }
                else {
                        msg_log::add("那里没有东西可关，也没有东西可堵。");
                }

                return;
        }

        const bool has_spike =
                map::g_player->m_inv.has_item_in_backpack(
                        item::Id::iron_spike);

        if (!has_spike) {
                msg_log::add("我没有东西可以卡住门。");

                return;
        }

        // Player has spikes

        const bool did_spike_door = door->actor_try_jam(*map::g_player);

        if (!did_spike_door) {
                return;
        }

        // Player did spike the door

        map::g_player->m_inv.decr_item_type_in_backpack(item::Id::iron_spike);

        const int nr_spikes_left =
                map::g_player->m_inv
                        .item_stack_size_in_backpack(
                                item::Id::iron_spike);

        if (nr_spikes_left == 0) {
                msg_log::add("我没有剩余的铁刺了。");
        }
        else {
                msg_log::add(
                        "我有 " +
                        std::to_string(nr_spikes_left) +
                        " 根铁刺剩余。");
        }
}  // player_try_close_or_jam_terrain

// -----------------------------------------------------------------------------
// close
// -----------------------------------------------------------------------------
namespace close
{
void player_try_close_or_jam()
{
        msg_log::clear();

        const std::string hint =
                common_text::g_direction_query +
                " " +
                common_text::g_cancel_hint;

        msg_log::add(
                hint,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const auto input_dir = query::dir(AllowCenter::no);

        msg_log::clear();

        if ((input_dir != Dir::END) && (input_dir != Dir::center)) {
                // Valid direction
                const auto p =
                        map::g_player->m_pos +
                        dir_utils::offset(input_dir);

                player_try_close_or_jam_terrain(map::g_terrain.at(p));
        }
}

}  // namespace close
