// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "bash.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "drop.hpp"
#include "explosion.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "hints.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "map_travel.hpp"
#include "msg_log.hpp"
#include "pickup.hpp"
#include "player_bon.hpp"
#include "popup.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "study_inscription.hpp"
#include "terrain_dmg.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"
#include "terrain_mob.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
struct MaterialBurnData
{
        int finish_burning_one_in_n {1};
        int hit_adjacent_one_in_n {1};
};

static MaterialBurnData get_material_burn_data(const Material material)
{
        MaterialBurnData d;

        switch (material) {
        case Material::fluid:
        case Material::empty:
                d.finish_burning_one_in_n = 1;
                d.hit_adjacent_one_in_n = 1;
                break;

        case Material::stone:
                d.finish_burning_one_in_n = 14;
                d.hit_adjacent_one_in_n = 10;
                break;

        case Material::metal:
                d.finish_burning_one_in_n = 14;
                d.hit_adjacent_one_in_n = 10;
                break;

        case Material::plant:
                d.finish_burning_one_in_n = 25;
                d.hit_adjacent_one_in_n = 9;
                break;

        case Material::wood:
                d.finish_burning_one_in_n = 25;
                d.hit_adjacent_one_in_n = 2;
                break;

        case Material::cloth:
                d.finish_burning_one_in_n = 20;
                d.hit_adjacent_one_in_n = 8;
                break;
        }

        return d;
}

static void scorch_actor(actor::Actor& actor)
{
        if (actor::is_player(&actor)) {
                msg_log::add(
                        "我被火焰灼伤了。",
                        colors::msg_bad());
        }
        else if (actor::can_player_see_actor(actor)) {
                const std::string name_the =
                        text_format::first_to_upper(
                                actor::name_the(actor));

                msg_log::add(
                        name_the +
                                " 被火焰灼烧。",
                        colors::msg_good());
        }

        actor::hit(actor, 1, DmgType::fire, nullptr);
}

static void spread_burning(const terrain::Terrain& terrain)
{
        // Center position not allowed.
        const P p(dir_utils::rnd_adj_pos(terrain.pos(), false));

        if (!map::is_pos_inside_outer_walls(p)) {
                return;
        }

        auto* const other_terrain = map::g_terrain.at(p);

        other_terrain->hit(DmgType::fire, nullptr);

        if (other_terrain->is_burning()) {
                other_terrain->m_started_burning_this_turn = true;

                if (map::g_player->m_pos == p) {
                        msg_log::add(
                                "火势蔓延到了这里！",
                                colors::msg_note(),
                                MsgInterruptPlayer::yes,
                                MorePromptOnMsg::yes);
                }
        }
}

static WasDestroyed on_new_turn_terrain_burning(terrain::Terrain& terrain)
{
        terrain.clear_gore();

        // TODO: Hit dead actors

        // Hit actor standing on terrain
        auto* actor = map::living_actor_at(terrain.pos());

        if (actor) {
                // Occasionally try to set actor on fire, otherwise just do
                // small fire damage.
                if (rnd::one_in(4)) {
                        actor->m_properties.apply(prop::make(prop::Id::burning));
                }
                else {
                        scorch_actor(*actor);
                }
        }

        const auto material_burn_data = get_material_burn_data(terrain.material());

        if (rnd::one_in(material_burn_data.finish_burning_one_in_n)) {
                terrain.m_burn_state = terrain::BurnState::has_burned;

                const auto was_destroyed = terrain.on_finished_burning();

                if (was_destroyed == WasDestroyed::yes) {
                        return was_destroyed;
                }
        }

        if (rnd::one_in(material_burn_data.hit_adjacent_one_in_n)) {
                spread_burning(terrain);
        }

        // Create smoke?
        if (rnd::one_in(20)) {
                const auto p = dir_utils::rnd_adj_pos(terrain.pos(), true);

                if (map::is_pos_inside_outer_walls(p) &&
                    !map_parsers::BlocksProjectiles().run(p)) {
                        auto* const smoke =
                                static_cast<terrain::Smoke*>(
                                        terrain::make(terrain::Id::smoke, p));

                        smoke->set_nr_turns(10);

                        game_time::add_mob(smoke);
                }
        }

        return WasDestroyed::no;
}

// Used for toppling statues or urns.
//
// NOTE: The object will be deleted!
//
static void topple_object(
        const terrain::Terrain& terrain,
        const std::string& name,
        const Dir direction,
        actor::Actor* const actor_toppling)
{
        const auto alerts_mon =
                actor::is_player(actor_toppling)
                ? AlertsMon::yes
                : AlertsMon::no;

        std::string snd_msg = "我听见一声巨响。";

        if (map::g_seen.at(terrain.pos())) {
                msg_log::add("这 " + name + " 倒下了。");

                snd_msg = "";
        }

        Snd snd(
                snd_msg,
                audio::SfxId::statue_crash,
                IgnoreMsgIfOriginSeen::no,
                terrain.pos(),
                actor_toppling,
                SndVol::low,
                alerts_mon);

        snd_emit::run(snd);

        const P dst_pos = terrain.pos() + dir_utils::offset(direction);

        map::update_terrain(make(terrain::Id::rubble_low, terrain.pos()));

        // NOTE: This object is now deleted!

        actor::Actor* const actor_behind = map::living_actor_at(dst_pos);

        if (actor_behind &&
            actor::is_alive(*actor_behind) &&
            !actor_behind->m_properties.has(prop::Id::ethereal)) {
                if (actor::is_player(actor_behind)) {
                        msg_log::add("它落在我身上！");
                }
                else {
                        // Monster is hit
                        const bool is_player_seeing_actor =
                                actor::can_player_see_actor(
                                        *actor_behind);

                        if (is_player_seeing_actor) {
                                msg_log::add("它落在 " + actor::name_a(*actor_behind) + ".");
                        }
                }

                actor::hit(
                        *actor_behind,
                        rnd::range(3, 6),
                        DmgType::blunt,
                        actor_toppling);

                if (actor::is_alive(*actor_behind)) {
                        // TODO: There should probably be a check if the
                        // creature is immune to blunt/physical damage here.

                        prop::Prop* const paralyzed = prop::make(prop::Id::paralyzed);

                        paralyzed->set_duration(rnd::range(2, 3));

                        actor_behind->m_properties.apply(paralyzed);
                }
        }

        const terrain::Id terrain_id = map::g_terrain.at(dst_pos)->id();

        // NOTE: This is kinda hacky, but the rubble is mostly just for
        // decoration anyway, so it doesn't really matter.
        if (terrain_id == terrain::Id::floor ||
            terrain_id == terrain::Id::grass ||
            terrain_id == terrain::Id::carpet) {
                map::update_terrain(make(terrain::Id::rubble_low, dst_pos));
        }

        map::update_vision();
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
void make_blood(const P& origin)
{
        for (const auto& d : dir_utils::g_dir_list_w_center) {
                if (!rnd::one_in(3)) {
                        continue;
                }

                const auto p = origin + d;

                map::g_terrain.at(p)->try_make_bloody();
        }
}

void make_gore(const P& origin)
{
        for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                        const auto c = origin + P(dx, dy);

                        if (rnd::one_in(3)) {
                                map::g_terrain.at(c)->try_put_gore();
                        }
                }
        }
}

void Terrain::bump(actor::Actor& actor_bumping)
{
        if (!can_move(actor_bumping) &&
            actor::is_player(&actor_bumping)) {
                if (map::g_seen.at(m_pos)) {
                        msg_log::add(m_data->msg_on_player_blocked);
                }
                else {
                        msg_log::add(m_data->msg_on_player_blocked_blind);
                }
        }
}

AllowAction Terrain::pre_bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping) ||
            actor_bumping.m_properties.has(prop::Id::confused)) {
                return AllowAction::yes;
        }

        const prop::PropHandler& props = actor_bumping.m_properties;

        if ((m_burn_state == BurnState::burning) &&
            !props.has(prop::Id::r_fire) &&
            !props.has(prop::Id::flying) &&
            !props.has(prop::Id::tiny_flying) &&
            can_move(actor_bumping) &&
            map::g_seen.at(m_pos)) {
                const std::string msg = "要走进火焰吗？ " + common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const BinaryAnswer query_result = query::yes_or_no();

                msg_log::clear();

                const bool allowed = (query_result == BinaryAnswer::yes);

                return allowed ? AllowAction::yes : AllowAction::no;
        }

        return AllowAction::yes;
}

void Terrain::on_new_turn()
{
        if (m_nr_turns_color_corrupted > 0) {
                --m_nr_turns_color_corrupted;
        }

        if ((m_burn_state == BurnState::burning) &&
            !m_started_burning_this_turn) {
                const auto was_destroyed = on_new_turn_terrain_burning(*this);

                if (was_destroyed == WasDestroyed::yes) {
                        return;
                }
        }

        // Run specialized new turn actions
        on_new_turn_hook();
}

void Terrain::try_start_burning(const Verbose verbose)
{
        clear_gore();

        const bool is_not_burned = m_burn_state == BurnState::not_burned;
        const bool has_burnt = m_burn_state == BurnState::has_burned;

        if (is_not_burned || (has_burnt && rnd::one_in(3))) {
                if (map::g_seen.at(m_pos) &&
                    (verbose == Verbose::yes)) {
                        std::string str = name(Article::the) + " 着火了。";

                        str[0] = (char)std::toupper(str[0]);

                        msg_log::add(str);
                }

                m_burn_state = BurnState::burning;

                m_started_burning_this_turn = true;
        }
}  // namespace terrain

WasDestroyed Terrain::on_finished_burning()
{
        return WasDestroyed::no;
}

int Terrain::shock_when_adj() const
{
        int shock = base_shock_when_adj();

        if (is_corrupted_color()) {
                shock += 6;
        }

        if (has_gore()) {
                shock += player_bon::is_bg(Bg::ghoul) ? 2 : 3;
        }

        return shock;
}

int Terrain::base_shock_when_adj() const
{
        return m_data->shock_when_adjacent;
}

void Terrain::try_make_bloody()
{
        if (can_have_blood()) {
                m_is_bloody = true;
        }
}

bool Terrain::is_bloody() const
{
        return m_is_bloody;
}

void Terrain::try_put_gore()
{
        if (!can_have_gore()) {
                return;
        }

        const int roll_character = rnd::range(1, 2);

        switch (roll_character) {
        case 1:
                m_gore_character = '\'';
                break;

        case 2:
                m_gore_character = '`';
                break;

        default:
                ASSERT(false);
                break;
        }

        const int roll_tile = rnd::range(1, 8);

        switch (roll_tile) {
        case 1:
                m_gore_tile = gfx::TileId::gore1;
                break;

        case 2:
                m_gore_tile = gfx::TileId::gore2;
                break;

        case 3:
                m_gore_tile = gfx::TileId::gore3;
                break;

        case 4:
                m_gore_tile = gfx::TileId::gore4;
                break;

        case 5:
                m_gore_tile = gfx::TileId::gore5;
                break;

        case 6:
                m_gore_tile = gfx::TileId::gore6;
                break;

        case 7:
                m_gore_tile = gfx::TileId::gore7;
                break;

        case 8:
                m_gore_tile = gfx::TileId::gore8;
                break;

        default:
                ASSERT(false);
                break;
        }
}

bool Terrain::has_gore() const
{
        // TODO: This is hacky
        return (m_gore_tile != gfx::TileId::END);
}

void Terrain::try_corrupt_color()
{
        if (id() == terrain::Id::chasm) {
                return;
        }

        m_nr_turns_color_corrupted = rnd::range(200, 220);
}

bool Terrain::is_corrupted_color() const
{
        return (m_nr_turns_color_corrupted > 0);
}

void Terrain::cycle_graphics(const io::GraphicsCycle cycle)
{
        if (cycle == io::GraphicsCycle::fast) {
                m_burn_color_bg.set_rgb(
                        (uint8_t)rnd::range(80, 255),
                        0,
                        0);

                m_corrupt_color.set_rgb(
                        rnd::range(40, 255),
                        rnd::range(40, 255),
                        rnd::range(40, 255));
        }

        cycle_graphics_hook(cycle);
}

Color Terrain::color() const
{
        if (m_burn_state == BurnState::burning) {
                // Burning
                return colors::orange();
        }

        if (is_corrupted_color()) {
                // Corrupted color
                return m_corrupt_color;
        }

        if (m_is_bloody) {
                // Bloody
                return colors::light_red();
        }

        if (m_burn_state == BurnState::has_burned) {
                // Has burned
                return colors::dark_gray();
        }

        return color_default();
}

Color Terrain::color_bg() const
{
        switch (m_burn_state) {
        case BurnState::not_burned:
        case BurnState::has_burned:
                return colors::black();

        case BurnState::burning:
                return m_burn_color_bg;
        }

        ASSERT(false && "Failed to set color");

        return colors::yellow();
}

std::optional<map::MinimapAppearance> Terrain::minimap_appearance() const
{
        return {};
}

void Terrain::clear_gore()
{
        m_gore_tile = gfx::TileId::END;
        m_gore_character = ' ';
        m_is_bloody = false;
}

void Terrain::add_light(Array2<bool>& light) const
{
        if (m_burn_state == BurnState::burning) {
                for (const P& d : dir_utils::g_dir_list_w_center) {
                        const P p(m_pos + d);

                        if (map::is_pos_inside_map(p)) {
                                light.at(p) = true;
                        }
                }
        }

        add_light_hook(light);
}

// -----------------------------------------------------------------------------
// Floor
// -----------------------------------------------------------------------------
Floor::Floor(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(FloorType::common) {}

void Floor::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                if (rnd::one_in(3)) {
                        try_start_burning(Verbose::no);
                }
                break;

        default:
                break;
        }
}

gfx::TileId Floor::tile() const
{
        if (m_burn_state == BurnState::has_burned) {
                return gfx::TileId::scorched_ground;
        }
        else {
                return m_data->tile;
        }
}

std::string Floor::name(const Article article) const
{
        std::string str = (article == Article::a) ? "" : "这 ";

        if (m_burn_state == BurnState::burning) {
                str += "火焰";
        }
        else {
                if (m_burn_state == BurnState::has_burned) {
                        str += "焦灼的 ";
                }

                switch (m_type) {
                case FloorType::common:
                        str += "石地板";
                        break;

                case FloorType::cave:
                        str += "洞窟地面";
                        break;

                case FloorType::stone_path:
                        if (article == Article::a) {
                                str.insert(0, "一个 ");
                        }

                        str += "石径";
                        break;
                }
        }

        return str;
}

Color Floor::color_default() const
{
        return colors::white();
}

// -----------------------------------------------------------------------------
// Wall
// -----------------------------------------------------------------------------
Wall::Wall(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Wall::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion: {
                destr_all_adj_doors(m_pos);

                if ((dmg_type == DmgType::pure) || rnd::coin_toss()) {
                        destr_stone_wall(m_pos);
                }
                else {
                        map::update_terrain(make(Id::rubble_high, m_pos));
                }

                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

gfx::TileId Wall::tile() const
{
        const P p_below = m_pos.with_y_offset(1);

        Id id_below = Id::END;

        if (p_below.y < map::h()) {
                const Terrain* const terrain_below = map::g_terrain.at(p_below);

                if (map::g_seen.at(p_below)) {
                        id_below = terrain_below->id();
                }
                else {
                        const map::PlayerMemoryTerrain& memory_below =
                                map::g_terrain_memory.at(p_below);

                        id_below = memory_below.id;
                }
        }

        switch (id_below) {
        case Id::wall:
        case Id::door:
        case Id::rubble_high:
                return top_wall_tile();

        default:
                return front_wall_tile();
        }
}

std::string Wall::name(const Article article) const
{
        std::string article_str;
        std::string name_str;

        switch (m_type) {
        case WallType::common:
        case WallType::common_alt:
        case WallType::leng_monestary:
        case WallType::egypt:
                article_str = "一个";
                name_str = "石墙";
                break;

        case WallType::mi_go:
                article_str = "一个";
                name_str = "外星墙壁";
                break;

        case WallType::cave:
                article_str = "一个";
                name_str = "洞窟墙壁";
                break;

        case WallType::cliff:
                article_str = "一个";
                name_str = "悬崖";
                break;
        }

        if (m_is_mossy) {
                article_str = "一个";
                name_str = "苔藓覆盖的 " + name_str;
        }

        if (article == Article::the) {
                article_str = "这";
        }

        return article_str + " " + name_str;
}

Color Wall::color_default() const
{
        if (m_is_mossy) {
                return colors::dark_green();
        }

        switch (m_type) {
        case WallType::cliff:
                return colors::dark_gray();

        case WallType::egypt:
        case WallType::cave:
                return colors::gray_brown();

        case WallType::mi_go:
                return colors::orange();

        case WallType::common:
        case WallType::common_alt:
                // Return the wall color of the current map
                return map::g_wall_color;

        case WallType::leng_monestary:
                return colors::red();
        }

        ASSERT(false && "Failed to set color");
        return colors::yellow();
}

gfx::TileId Wall::front_wall_tile() const
{
        switch (m_type) {
        case WallType::common:
                return gfx::TileId::wall_front;

        case WallType::common_alt:
                return gfx::TileId::wall_front_alt1;

        case WallType::cliff:
        case WallType::cave:
                return gfx::TileId::wall_cave_front;

        case WallType::leng_monestary:
        case WallType::egypt:
                return gfx::TileId::wall_egypt_front;

        case WallType::mi_go:
                return gfx::TileId::wall_mi_go_front;
        }

        ASSERT(false && "Failed to set front wall tile");
        return gfx::TileId::END;
}

gfx::TileId Wall::top_wall_tile() const
{
        switch (m_type) {
        case WallType::common:
        case WallType::common_alt:
                return gfx::TileId::wall_top;

        case WallType::cliff:
        case WallType::cave:
                return gfx::TileId::wall_cave_top;

        case WallType::leng_monestary:
        case WallType::egypt:
                return gfx::TileId::wall_egypt_top;

        case WallType::mi_go:
                return gfx::TileId::wall_mi_go_top;
        }

        ASSERT(false && "Failed to set top wall tile");
        return gfx::TileId::END;
}

void Wall::set_rnd_common_wall()
{
        m_type = (rnd::one_in(6)) ? WallType::common_alt : WallType::common;
}

void Wall::set_moss_grown()
{
        m_is_mossy = true;
}

// -----------------------------------------------------------------------------
// Pillar
// -----------------------------------------------------------------------------
Pillar::Pillar(const P& p, const TerrainData* const data) :
        Terrain(p, data)
{
}

void Pillar::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        if (m_is_inscribed && !m_has_player_studied_inscription && map::g_seen.at(m_pos)) {
                study_inscription::run();

                m_has_player_studied_inscription = true;

                game_time::tick();

                return;
        }

        Terrain::bump(actor_bumping);
}

void Pillar::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion: {
                if ((dmg_type == DmgType::pure) || rnd::coin_toss()) {
                        destr_stone_wall(m_pos);
                }
                else {
                        map::update_terrain(make(Id::rubble_high, m_pos));
                }

                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

void Pillar::on_new_turn()
{
        if (can_be_studied() &&
            map::g_player->m_pos.is_adjacent(m_pos) &&
            map::g_seen.at(m_pos)) {
                hints::display(hints::Id::study_inscription);
        }
}

std::string Pillar::name(const Article article) const
{
        std::string article_str;
        std::string name_str;

        if (m_is_broken) {
                article_str = "一个";
                name_str = "断裂的石柱";
        }
        else if (m_is_inscribed) {
                article_str = "一个";
                name_str = "刻铭石柱";
        }
        else {
                article_str = "一个";
                name_str = "石柱";
        }

        if (article == Article::the) {
                article_str = "这";
        }

        return article_str + " " + name_str;
}

Color Pillar::color_default() const
{
        if (m_is_inscribed && !m_has_player_studied_inscription) {
                return colors::light_sepia();
        }
        else {
                return colors::gray();
        }
}

std::optional<map::MinimapAppearance> Pillar::minimap_appearance() const
{
        if (!can_be_studied()) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.color = color_default();
        appearance.legend_text = "铭文物体";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

gfx::TileId Pillar::tile() const
{
        if (m_is_inscribed) {
                return gfx::TileId::pillar_inscribed;
        }
        else if (m_is_broken) {
                return gfx::TileId::pillar_broken;
        }
        else {
                return gfx::TileId::pillar;
        }
}

// -----------------------------------------------------------------------------
// Petroglyph
// -----------------------------------------------------------------------------
Petroglyph::Petroglyph(const P& p, const TerrainData* const data) :
        Terrain(p, data)
{
}

void Petroglyph::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        if (!m_has_player_studied_inscription && map::g_seen.at(m_pos)) {
                study_inscription::run();

                m_has_player_studied_inscription = true;

                game_time::tick();

                return;
        }

        Terrain::bump(actor_bumping);
}

void Petroglyph::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion: {
                if ((dmg_type == DmgType::pure) || rnd::coin_toss()) {
                        destr_stone_wall(m_pos);
                }
                else {
                        map::update_terrain(make(Id::rubble_high, m_pos));
                }

                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

void Petroglyph::on_new_turn()
{
        if (can_be_studied() &&
            map::g_player->m_pos.is_adjacent(m_pos) &&
            map::g_seen.at(m_pos)) {
                hints::display(hints::Id::study_inscription);
        }
}

std::string Petroglyph::name(const Article article) const
{
        const std::string article_str = (article == Article::a) ? "一个" : "这";

        const std::string name_str = "岩刻";

        return article_str + " " + name_str;
}

Color Petroglyph::color_default() const
{
        if (m_has_player_studied_inscription) {
                return colors::gray();
        }
        else {
                return colors::light_sepia();
        }
}

std::optional<map::MinimapAppearance> Petroglyph::minimap_appearance() const
{
        if (!can_be_studied()) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.color = color_default();
        appearance.legend_text = "铭文物体";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

// -----------------------------------------------------------------------------
// High rubble
// -----------------------------------------------------------------------------
RubbleHigh::RubbleHigh(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void RubbleHigh::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion:
                destr_stone_wall(m_pos);
                map::update_vision();
                break;

        default:
                break;
        }
}

std::string RubbleHigh::name(const Article article) const
{
        std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "一大堆碎石";
}

Color RubbleHigh::color_default() const
{
        // Return the wall color of the current map
        return map::g_wall_color;
}

// -----------------------------------------------------------------------------
// Low rubble
// -----------------------------------------------------------------------------
RubbleLow::RubbleLow(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void RubbleLow::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

std::string RubbleLow::name(const Article article) const
{
        std::string str;

        if (article == Article::the) {
                str += "这 ";
        }

        if (m_burn_state == BurnState::burning) {
                str += "燃烧 ";
        }

        return str + "碎石";
}

Color RubbleLow::color_default() const
{
        // Return the wall color of the current map
        return map::g_wall_color;
}

// -----------------------------------------------------------------------------
// Bones
// -----------------------------------------------------------------------------
Bones::Bones(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Bones::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

std::string Bones::name(const Article article) const
{
        std::string str;

        if (article == Article::the) {
                str += "这 ";
        }

        return str + "骨头";
}

Color Bones::color_default() const
{
        return colors::dark_gray();
}

// -----------------------------------------------------------------------------
// Grave
// -----------------------------------------------------------------------------
GraveStone::GraveStone(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void GraveStone::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

void GraveStone::bump(actor::Actor& actor_bumping)
{
        if (actor::is_player(&actor_bumping)) {
                msg_log::add(m_inscr);
        }
}

std::string GraveStone::name(const Article article) const
{
        const std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "墓碑（“" + m_inscr + "\")";
}

Color GraveStone::color_default() const
{
        return colors::white();
}

// -----------------------------------------------------------------------------
// Church bench
// -----------------------------------------------------------------------------
ChurchBench::ChurchBench(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void ChurchBench::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("教堂长椅被摧毁了。");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

std::string ChurchBench::name(const Article article) const
{
        const std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "教堂长椅";
}

Color ChurchBench::color_default() const
{
        return colors::brown();
}

// -----------------------------------------------------------------------------
// Statue
// -----------------------------------------------------------------------------
Statue::Statue(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(rnd::one_in(8) ? StatueType::ghoul : StatueType::common),
        m_player_bg(Bg::END)
{
}

int Statue::base_shock_when_adj() const
{
        const bool is_ghoul_statue =
                (m_type == StatueType::ghoul) ||
                (m_player_bg == Bg::ghoul);

        if (is_ghoul_statue && !player_bon::is_bg(Bg::ghoul)) {
                return 10;
        }
        else {
                return 0;
        }
}

void Statue::topple(
        const Dir direction,
        actor::Actor* const actor_toppling)
{
        topple_object(*this, "雕像", direction, actor_toppling);
}

bool Statue::allow_player_melee_attack(
        const DmgType dmg_type,
        const item::Item& wpn) const
{
        (void)wpn;

        switch (dmg_type) {
        case DmgType::kicking:
                return true;
                break;

        default:
                break;
        }

        return false;
}

void Statue::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg;

        switch (dmg_type) {
        case DmgType::kicking:
        case DmgType::control_object_spell: {
                ASSERT(actor);

                if ((dmg_type == DmgType::kicking) &&
                    actor->m_properties.has(prop::Id::weakened)) {
                        msg_log::add("它扭动了一下。");

                        return;
                }

                const auto direction = dir_utils::dir(m_pos - from_pos);

                // NOTE: This call deletes the object!
                topple(direction, actor);
        } break;

        case DmgType::explosion:
        case DmgType::pure: {
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

void Statue::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        if (!map::g_seen.at(m_pos) || m_inscr.empty()) {
                Terrain::bump(actor_bumping);
        }
        else {
                msg_log::add(m_inscr);
        }
}

void Statue::on_new_turn()
{
        if (map::g_player->m_pos.is_adjacent(m_pos) && map::g_seen.at(m_pos)) {
                hints::display(hints::Id::kick_statue);
        }
}

std::string Statue::name(const Article article) const
{
        std::string str = (article == Article::a) ? "一个 " : "这 ";

        switch (m_type) {
        case StatueType::common:
                str += "雕像";
                break;

        case StatueType::ghoul:
                str += "食尸鬼生物雕像";
                break;
        }

        if (!m_inscr.empty()) {
                str += " (\"" + m_inscr + "\")";
        }

        return str;
}

gfx::TileId Statue::tile() const
{
        if (m_player_bg == Bg::ghoul) {
                return gfx::TileId::ghoul;
        }
        else if (m_player_bg != Bg::END) {
                return gfx::TileId::player_unarmed;
        }
        else if (m_type == StatueType::ghoul) {
                return gfx::TileId::ghoul;
        }
        else {
                return gfx::TileId::witch_or_warlock;
        }
}

Color Statue::color_default() const
{
        if (m_player_bg == Bg::END) {
                return colors::white();
        }
        else {
                // The statue will be drawn with a player tile - make it more
                // distinct from the player character
                return colors::gray();
        }
}

void Statue::set_player_bg(const Bg bg)
{
        m_player_bg = bg;
}

// -----------------------------------------------------------------------------
// Statue
// -----------------------------------------------------------------------------
Urn::Urn(const P& p, const TerrainData* const data) :
        Terrain(p, data)
{
}

void Urn::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        if (m_is_inscribed && !m_has_player_studied_inscription && map::g_seen.at(m_pos)) {
                study_inscription::run();

                m_has_player_studied_inscription = true;

                game_time::tick();

                return;
        }

        Terrain::bump(actor_bumping);
}

void Urn::topple(
        const Dir direction,
        actor::Actor* const actor_toppling)
{
        topple_object(*this, "骨灰瓮", direction, actor_toppling);
}

bool Urn::allow_player_melee_attack(
        const DmgType dmg_type,
        const item::Item& wpn) const
{
        (void)wpn;

        switch (dmg_type) {
        case DmgType::kicking:
                return true;
                break;

        default:
                break;
        }

        return false;
}

void Urn::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg;

        switch (dmg_type) {
        case DmgType::kicking:
        case DmgType::control_object_spell: {
                ASSERT(actor);

                if ((dmg_type == DmgType::kicking) &&
                    actor->m_properties.has(prop::Id::weakened)) {
                        msg_log::add("它扭动了一下。");

                        return;
                }

                const auto direction = dir_utils::dir(m_pos - from_pos);

                // NOTE: This call deletes the object!
                topple(direction, actor);
        } break;

        case DmgType::explosion:
        case DmgType::pure: {
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

void Urn::on_new_turn()
{
        if (can_be_studied() &&
            map::g_player->m_pos.is_adjacent(m_pos) &&
            map::g_seen.at(m_pos)) {
                hints::display(hints::Id::study_inscription);
        }
}

std::string Urn::name(const Article article) const
{
        std::string article_str;
        std::string name_str;

        if (m_is_inscribed) {
                article_str = "一个";
                name_str = "刻铭骨灰瓮";
        }
        else {
                article_str = "一个";
                name_str = "骨灰瓮";
        }

        if (article == Article::the) {
                article_str = "这";
        }

        return article_str + " " + name_str;
}

gfx::TileId Urn::tile() const
{
        if (m_is_inscribed) {
                return gfx::TileId::urn_inscribed;
        }
        else {
                return gfx::TileId::urn;
        }
}

Color Urn::color_default() const
{
        if (m_is_inscribed && !m_has_player_studied_inscription) {
                return colors::light_sepia();
        }
        else {
                return colors::gray();
        }
}

std::optional<map::MinimapAppearance> Urn::minimap_appearance() const
{
        if (!can_be_studied()) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.color = color_default();
        appearance.legend_text = "铭文物体";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

// -----------------------------------------------------------------------------
// Stalagmite
// -----------------------------------------------------------------------------
Stalagmite::Stalagmite(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Stalagmite::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion:
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

std::string Stalagmite::name(const Article article) const
{
        std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "石笋";
}

Color Stalagmite::color_default() const
{
        return colors::gray_brown();
}

// -----------------------------------------------------------------------------
// Stairs
// -----------------------------------------------------------------------------
Stairs::Stairs(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Stairs::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

void Stairs::on_new_turn_hook()
{
        ASSERT(!map::g_items.at(m_pos));
}

void Stairs::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);

        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .set_title("一段通往下方的楼梯")
                .setup_menu_mode(
                        {"(D)下楼", "(S)保存并退出"},
                        {'d', 's'},
                        popup::MenuModeShowCancelHint::yes,
                        &choice)
                .run();

        switch (choice) {
        case 0:
                map::g_player->m_pos = m_pos;

                if (is_fake()) {
                        // NOTE: This destroys this object
                        player_use_fake_stairs();

                        return;
                }

                msg_log::clear();

                msg_log::add("我走下楼梯。");

                // Always auto-save the game when descending
                //
                // NOTE: We descend one dlvl when loading the game, so
                // auto-saving should be done BEFORE descending here
                saving::save_game();

                map_travel::go_to_nxt();
                break;

        case 1:
                map::g_player->m_pos = m_pos;

                if (is_fake()) {
                        // NOTE: This destroys this object
                        player_use_fake_stairs();

                        return;
                }

                saving::save_game();

                states::pop();
                break;

        default:
                msg_log::clear();
                break;
        }
}

void Stairs::player_use_fake_stairs()
{
        const auto* const msg =
                "我走下楼梯观察四周，惊愕地发现自己竟踏在开始下行时的同一片地面上！转身一看，楼梯已无影无踪。";

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_msg(msg)
                .run();

        map::update_terrain(make(Id::floor, m_pos));

        auto* prop = prop::make(prop::Id::confused);

        prop->set_duration(8);

        map::g_player->m_properties.apply(prop);

        map::g_player->incr_shock(2.0, ShockSrc::misc);
}

std::string Stairs::name(const Article article) const
{
        std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "向下的楼梯";
}

Color Stairs::color_default() const
{
        return colors::yellow();
}

std::optional<map::MinimapAppearance> Stairs::minimap_appearance() const
{
        map::MinimapAppearance appearance;

        appearance.color = color_default();
        appearance.legend_text = "楼梯";
        appearance.symbol = map::MinimapSymbol::rectangle_filled;

        return appearance;
}

void Stairs::add_light_hook(Array2<bool>& light) const
{
        light.at(m_pos) = true;
}

// -----------------------------------------------------------------------------
// Bridge
// -----------------------------------------------------------------------------
gfx::TileId Bridge::tile() const
{
        return (
                (m_axis == Axis::hor)
                        ? gfx::TileId::hangbridge_hor
                        : gfx::TileId::hangbridge_ver);
}

void Bridge::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

char Bridge::character() const
{
        return (m_axis == Axis::hor)
                ? '|'
                : '=';
}

std::string Bridge::name(const Article article) const
{
        std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "桥";
}

Color Bridge::color_default() const
{
        return colors::dark_brown();
}

// -----------------------------------------------------------------------------
// Shallow liquid
// -----------------------------------------------------------------------------
Liquid::Liquid(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(LiquidType::water) {}

void Liquid::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

void Liquid::bump(actor::Actor& actor_bumping)
{
        const prop::PropHandler& props = actor_bumping.m_properties;

        if (props.has(prop::Id::ethereal) ||
            props.has(prop::Id::flying) ||
            props.has(prop::Id::tiny_flying) ||
            props.has(prop::Id::water_creature) ||
            actor_bumping.m_data->is_amphibian) {
                return;
        }

        if (!props.has(prop::Id::crimson_passage)) {
                actor_bumping.m_properties.apply(prop::make(prop::Id::delayed_by_liquid));

                // Print message if player, unless player is wearing torture collar (in that case
                // the message is redundant since the player is delayed on every move they make
                // anyway).
                if (actor::is_player(&actor_bumping) &&
                    !map::g_player->m_inv.has_item_in_slot(
                            SlotId::head,
                            item::Id::torture_collar)) {
                        std::string type_str;
                        std::string verb_str;

                        switch (m_type) {
                        case LiquidType::water:
                        case LiquidType::magic_water:
                                type_str = "水";
                                verb_str = "涉水";
                                break;

                        case LiquidType::mud:
                                type_str = "泥";
                                verb_str = "艰难前行";
                                break;
                        }

                        msg_log::add(
                                "我 " +
                                verb_str +
                                " 缓慢穿过齐膝深的 " +
                                type_str +
                                ".");
                }

                // The creature might also get stuck.
                if ((m_type == LiquidType::mud) && rnd::one_in(5)) {
                        prop::Prop* stuck = prop::make(prop::Id::stuck);

                        stuck->set_indefinite();

                        actor_bumping.m_properties.apply(stuck);
                }
        }

        // Make a sound, unless player with Silent trait.
        if (!player_bon::has_trait(Trait::silent) ||
            !actor::is_player(&actor_bumping)) {
                const std::string msg =
                        actor::is_player(&actor_bumping)
                        ? ""
                        : "我听见泼水声。";

                const auto alerts_mon =
                        actor::is_player(&actor_bumping)
                        ? AlertsMon::yes
                        : AlertsMon::no;

                Snd snd(
                        msg,
                        audio::SfxId::wade,
                        IgnoreMsgIfOriginSeen::yes,
                        actor_bumping.m_pos,
                        &actor_bumping,
                        SndVol::low,
                        alerts_mon);

                snd_emit::run(snd);
        }

        // "Magic pool" effects.
        if (actor::is_player(&actor_bumping) && (m_type == LiquidType::magic_water)) {
                run_magic_pool_effects_on_player();
        }
}

void Liquid::run_magic_pool_effects_on_player()
{
        std::vector<item::Item*> cursed_items;

        for (const auto& slot : map::g_player->m_inv.m_slots) {
                if (slot.item && slot.item->is_cursed()) {
                        cursed_items.push_back(slot.item);
                }
        }

        for (auto* const item : map::g_player->m_inv.m_backpack) {
                if (item->is_cursed()) {
                        cursed_items.push_back(item);
                }
        }

        map::g_player->m_properties.end_prop(prop::Id::cursed);
        map::g_player->m_properties.end_prop(prop::Id::infected);
        map::g_player->m_properties.end_prop(prop::Id::diseased);
        map::g_player->m_properties.end_prop(prop::Id::wound);

        for (auto* const item : cursed_items) {
                const auto name =
                        item->name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                msg_log::add("这 " + name + " 似乎已被净化！");

                item->current_curse().on_curse_end();

                item->remove_curse();
        }
}

std::string Liquid::name(const Article article) const
{
        std::string str;

        if (article == Article::the) {
                str += "这 ";
        }

        switch (m_type) {
        case LiquidType::water:
                str += "水";
                break;

        case LiquidType::mud:
                str += "浅泥地";
                break;

        case LiquidType::magic_water:
                str += "闪耀池";
                break;
        }

        return str;
}

Color Liquid::color_default() const
{
        switch (m_type) {
        case LiquidType::water:
                return colors::light_blue();
                break;

        case LiquidType::mud:
                return colors::brown();
                break;

        case LiquidType::magic_water:
                return colors::light_cyan();
                break;
        }

        ASSERT(false && "Failed to set color");
        return colors::yellow();
}

std::optional<map::MinimapAppearance> Liquid::minimap_appearance() const
{
        map::MinimapAppearance appearance;

        appearance.color = colors::blue();
        appearance.symbol = map::MinimapSymbol::rectangle_filled;

        return appearance;
}

// -----------------------------------------------------------------------------
// Chasm
// -----------------------------------------------------------------------------
Chasm::Chasm(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Chasm::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

std::string Chasm::name(const Article article) const
{
        std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "深渊";
}

Color Chasm::color_default() const
{
        return colors::dark_blue();
}

// -----------------------------------------------------------------------------
// Crystal Key
// -----------------------------------------------------------------------------
CrystalKey::CrystalKey(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void CrystalKey::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

std::string CrystalKey::name(const Article article) const
{
        std::string str = (article == Article::a) ? "一个" : "这";

        str += " ";

        str += m_is_active ? "闪耀的" : "死亡";

        str += " 水晶";

        return str;
}

Color CrystalKey::color_default() const
{
        return m_is_active ? colors::light_red() : colors::gray();
}

std::optional<map::MinimapAppearance> CrystalKey::minimap_appearance() const
{
        if (!m_is_active) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.color = color_default();
        appearance.legend_text = "闪耀水晶";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

gfx::TileId CrystalKey::tile() const
{
        return gfx::TileId::crystal;

        // return m_is_active ? gfx::TileId::crystal : gfx::TileId::crystal_destroyed;
}

void CrystalKey::bump(actor::Actor& actor_bumping)
{
        (void)actor_bumping;

        TRACE_FUNC_BEGIN;

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        const bool is_seen = map::g_seen.at(m_pos);

        const std::string terrain_name = text_format::first_to_lower(name(Article::the));

        if (is_seen) {
                msg_log::add("我触碰 " + terrain_name + ".");
        }
        else {
                msg_log::clear();

                msg_log::add("我摸到一个水晶物体。");
        }

        if (!m_is_active) {
                if (is_seen) {
                        msg_log::add("什么也没发生。");
                }

                return;
        }

        if (is_seen) {
                msg_log::add("内部的光芒渐渐暗去。");
        }

        player_deactivate();

        game_time::tick();

        TRACE_FUNC_END;
}

void CrystalKey::player_deactivate()
{
        audio::play(audio::SfxId::crystal_key_disable);

        msg_log::add("我感觉某处打开了一条路。");

        game::incr_player_xp(g_xp_on_deactivate_crystal_key, Verbose::yes);

        deactivate();

        map::memorize_terrain_at(m_pos);
        map::update_vision();
}

void CrystalKey::deactivate()
{
        m_is_active = false;

        // Signal that the crystal has been deactivated to the linked door.
        if (m_linked_door) {
                m_linked_door->remove_ward();
        }

        // Deactivate all siblings.
        for (CrystalKey* const sibbling : m_sibblings) {
                sibbling->m_is_active = false;
        }
}

void CrystalKey::add_light_hook(Array2<bool>& light) const
{
        if (m_is_active) {
                light.at(m_pos) = true;
        }
}

// -----------------------------------------------------------------------------
// Altar
// -----------------------------------------------------------------------------
Altar::Altar(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Altar::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("祭坛被摧毁了。");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();

                if (player_bon::is_bg(Bg::exorcist)) {
                        const auto msg =
                                rnd::element(
                                        common_text::g_exorcist_purge_phrases);

                        msg_log::add(msg);

                        game::incr_player_xp(g_xp_on_exorcist_destroy_altar);

                        actor::restore_sp(
                                *map::g_player,
                                999,
                                actor::AllowRestoreAboveMax::no,
                                Verbose::no);

                        actor::restore_exorcist_fervor(g_exorcist_fervor_destroy_altar);
                }
                break;

        default:
                break;
        }
}

void Altar::on_new_turn()
{
        if (map::g_player->m_pos.is_adjacent(m_pos) &&
            map::g_seen.at(m_pos) &&
            !player_bon::is_bg(Bg::exorcist)) {
                hints::display(hints::Id::altars);
        }
}

void Altar::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (player_bon::is_bg(Bg::exorcist) &&
            map::g_seen.at(m_pos)) {
                // Exorcist player is bumping a seen altar
                msg_log::add(
                        "一座邪恶祭坛在此拔地而起，必须将其摧毁！");

                return;
        }

        // Nothing special happening - use standard bump handling
        Terrain::bump(actor_bumping);
}

std::string Altar::name(const Article article) const
{
        std::string str = (article == Article::a) ? "一个 " : "这 ";

        return str + "坛";
}

Color Altar::color_default() const
{
        return colors::light_sepia();
}

std::optional<map::MinimapAppearance> Altar::minimap_appearance() const
{
        map::MinimapAppearance appearance;

        appearance.color = color_default();
        appearance.legend_text = "祭坛";
        appearance.symbol = map::MinimapSymbol::rectangle_filled;

        return appearance;
}

// -----------------------------------------------------------------------------
// Carpet
// -----------------------------------------------------------------------------
Carpet::Carpet(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Carpet::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

WasDestroyed Carpet::on_finished_burning()
{
        auto* const floor =
                static_cast<Floor*>(make(Id::floor, m_pos));

        floor->m_burn_state = BurnState::has_burned;

        map::update_terrain(floor);

        return WasDestroyed::yes;
}

std::string Carpet::name(const Article article) const
{
        std::string str = (article == Article::a) ? "" : "这 ";

        return str + "地毯";
}

Color Carpet::color_default() const
{
        return colors::red();
}

// -----------------------------------------------------------------------------
// Grass
// -----------------------------------------------------------------------------
Grass::Grass(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(GrassType::common)
{
        if (rnd::one_in(5)) {
                m_type = GrassType::withered;
        }
}

void Grass::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

gfx::TileId Grass::tile() const
{
        if (m_burn_state == BurnState::has_burned) {
                return gfx::TileId::scorched_ground;
        }
        else {
                return m_data->tile;
        }
}

std::string Grass::name(const Article article) const
{
        std::string str;

        if (article == Article::the) {
                str += "这 ";
        }

        switch (m_burn_state) {
        case BurnState::not_burned:
                switch (m_type) {
                case GrassType::common:
                        return str + "草地";

                case GrassType::withered:
                        return str + "枯草";
                }
                break;

        case BurnState::burning:
                return str + "燃烧的草地";

        case BurnState::has_burned:
                return str + "焦土";
        }

        ASSERT("Failed to set name" && false);
        return "";
}

Color Grass::color_default() const
{
        switch (m_type) {
        case GrassType::common:
                return colors::green();
                break;

        case GrassType::withered:
                return colors::dark_brown();
                break;
        }

        ASSERT(false && "Failed to set color");
        return colors::yellow();
}

// -----------------------------------------------------------------------------
// Bush
// -----------------------------------------------------------------------------
Bush::Bush(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(GrassType::common)
{
        if (rnd::one_in(5)) {
                m_type = GrassType::withered;
        }
}

void Bush::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

WasDestroyed Bush::on_finished_burning()
{
        auto* const grass =
                static_cast<Grass*>(make(Id::grass, m_pos));

        grass->m_burn_state = BurnState::has_burned;

        map::update_terrain(grass);

        return WasDestroyed::yes;
}

std::string Bush::name(const Article article) const
{
        std::string str = (article == Article::a) ? "一个 " : "这 ";

        switch (m_burn_state) {
        case BurnState::not_burned:
                switch (m_type) {
                case GrassType::common:
                        return str + "灌木";

                case GrassType::withered:
                        return str + "枯灌木";
                }
                break;

        case BurnState::burning:
                return str + "燃烧的灌木";

        case BurnState::has_burned:
                // Should not happen
                break;
        }

        ASSERT("Failed to set name" && false);
        return "";
}

Color Bush::color_default() const
{
        switch (m_type) {
        case GrassType::common:
                return colors::green();
                break;

        case GrassType::withered:
                return colors::dark_brown();
                break;
        }

        ASSERT(false && "Failed to set color");
        return colors::yellow();
}

// -----------------------------------------------------------------------------
// Vines
// -----------------------------------------------------------------------------
Vines::Vines(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Vines::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

WasDestroyed Vines::on_finished_burning()
{
        auto* const floor =
                static_cast<Floor*>(make(Id::floor, m_pos));

        floor->m_burn_state = BurnState::has_burned;

        map::update_terrain(floor);

        return WasDestroyed::yes;
}

std::string Vines::name(const Article article) const
{
        std::string str = (article == Article::a) ? "" : "这 ";

        switch (m_burn_state) {
        case BurnState::not_burned:
                return str + "垂挂的藤蔓";

        case BurnState::burning:
                return str + "燃烧的藤蔓";

        case BurnState::has_burned:
                // Should not happen
                break;
        }

        ASSERT("Failed to set name" && false);
        return "";
}

Color Vines::color_default() const
{
        return colors::green();
}

// -----------------------------------------------------------------------------
// Chains
// -----------------------------------------------------------------------------
Chains::Chains(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

std::string Chains::name(const Article article) const
{
        std::string a = (article == Article::a) ? "" : "这 ";

        return a + "生锈的锁链";
}

Color Chains::color_default() const
{
        return colors::gray();
}

void Chains::bump(actor::Actor& actor_bumping)
{
        if (actor_bumping.m_data->actor_size > actor::Size::floor &&
            !actor_bumping.m_properties.has(prop::Id::ethereal) &&
            !actor_bumping.m_properties.has(prop::Id::ooze)) {
                std::string msg;

                if (map::g_seen.at(m_pos)) {
                        msg = "锁链哗啦作响。";
                }
                else {
                        msg = "我听见锁链哗啦作响。";
                }

                const auto alerts_mon =
                        actor::is_player(&actor_bumping)
                        ? AlertsMon::yes
                        : AlertsMon::no;

                Snd snd(
                        msg,
                        audio::SfxId::chains,
                        IgnoreMsgIfOriginSeen::no,
                        actor_bumping.m_pos,
                        &actor_bumping,
                        SndVol::low,
                        alerts_mon);

                snd_emit::run(snd);
        }
}

void Chains::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// Grate
// -----------------------------------------------------------------------------
Grate::Grate(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Grate::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion:
                destr_all_adj_doors(m_pos);
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

std::string Grate::name(const Article article) const
{
        std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "栅栏";
}

Color Grate::color_default() const
{
        return colors::gray();
}

// -----------------------------------------------------------------------------
// Tree
// -----------------------------------------------------------------------------
Tree::Tree(const P& p, const TerrainData* const data) :
        Terrain(p, data)
{
        if (is_fungi()) {
                const std::vector<Color> base_color_bucket = {
                        colors::white(),
                        colors::cyan(),
                        colors::gray_brown(),
                        colors::orange(),
                        colors::green()};

                const std::vector<int> weights = {
                        100,
                        10,
                        10,
                        1,
                        1};

                const size_t choice = rnd::weighted_choice(weights);

                m_color = base_color_bucket[choice];

                m_color.randomize_rgb(20);
        }
        else {
                m_color = colors::dark_brown();
        }
}

void Tree::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                if (rnd::fraction(2, 3)) {
                        try_start_burning(Verbose::no);
                }
                break;

        default:
                break;
        }
}

WasDestroyed Tree::on_finished_burning()
{
        if (!map::is_pos_inside_outer_walls(m_pos)) {
                return WasDestroyed::no;
        }

        auto* const grass =
                static_cast<Grass*>(make(Id::grass, m_pos));

        grass->m_burn_state = BurnState::has_burned;

        map::update_terrain(grass);

        map::update_vision();

        return WasDestroyed::yes;
}

gfx::TileId Tree::tile() const
{
        return (
                is_fungi()
                        ? gfx::TileId::tree_fungi
                        : gfx::TileId::tree);
}

std::string Tree::name(const Article article) const
{
        std::string result = (article == Article::a) ? "一个 " : "这 ";

        switch (m_burn_state) {
        case BurnState::not_burned:
                break;

        case BurnState::burning:
                result += "燃烧 ";
                break;

        case BurnState::has_burned:
                result += "焦灼的 ";
                break;
        }

        if (is_fungi()) {
                result += "巨型真菌";
        }
        else {
                result += "树";
        }

        return result;
}

Color Tree::color_default() const
{
        return m_color;
}

bool Tree::is_fungi() const
{
        return (map::g_dlvl > 1);
}

// -----------------------------------------------------------------------------
// Brazier
// -----------------------------------------------------------------------------
std::string Brazier::name(const Article article) const
{
        std::string a = (article == Article::a) ? "一个 " : "这 ";

        return a + "火盆";
}

bool Brazier::allow_player_melee_attack(
        const DmgType dmg_type,
        const item::Item& wpn) const
{
        (void)wpn;

        switch (dmg_type) {
        case DmgType::kicking:
                return true;
                break;

        default:
                break;
        }

        return false;
}

void Brazier::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg;

        switch (dmg_type) {
        case DmgType::kicking:
        case DmgType::control_object_spell: {
                if (!actor) {
                        ASSERT(false);

                        return;
                }

                if ((dmg_type == DmgType::kicking) &&
                    actor->m_properties.has(prop::Id::weakened)) {
                        msg_log::add("它扭动了一下。");

                        return;
                }

                const Dir direction = dir_utils::dir(m_pos - from_pos);

                topple(direction, *actor);
        } break;

        case DmgType::explosion:
        case DmgType::pure: {
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

void Brazier::topple(const Dir direction, actor::Actor& actor)
{
        const auto alerts_mon =
                actor::is_player(&actor)
                ? AlertsMon::yes
                : AlertsMon::no;

        std::string snd_msg = "我听见一声巨响。";

        if (map::g_seen.at(m_pos)) {
                msg_log::add("它倒下了。");

                snd_msg = "";
        }

        Snd snd(
                snd_msg,
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::no,
                m_pos,
                &actor,
                SndVol::low,
                alerts_mon);

        snd_emit::run(snd);

        const P dst_pos = m_pos + dir_utils::offset(direction);

        const P my_pos = m_pos;

        map::update_terrain(make(Id::rubble_low, m_pos));

        // NOTE: "this" is now deleted!

        const Terrain* const tgt_terrain = map::g_terrain.at(dst_pos);

        if (tgt_terrain->id() != terrain::Id::chasm) {
                P expl_pos;

                int expl_d = 0;

                if (tgt_terrain->is_projectile_passable()) {
                        expl_pos = dst_pos;
                        expl_d = -1;
                }
                else {
                        expl_pos = my_pos;
                        expl_d = -2;
                }

                // TODO: Emit sound from explosion center

                explosion::run(
                        expl_pos,
                        ExplType::apply_prop,
                        EmitExplSnd::no,
                        expl_d,
                        ExplExclCenter::no,
                        {prop::make(prop::Id::burning)});
        }

        map::update_vision();
}

void Brazier::on_new_turn()
{
        if (map::g_player->m_pos.is_adjacent(m_pos) && map::g_seen.at(m_pos)) {
                hints::display(hints::Id::kick_brazier);
        }
}

void Brazier::add_light_hook(Array2<bool>& light) const
{
        for (const P& d : dir_utils::g_dir_list_w_center) {
                const P p(m_pos + d);

                light.at(p) = true;
        }
}

Color Brazier::color() const
{
        return m_flicker_color;
}

void Brazier::cycle_graphics_hook(const io::GraphicsCycle cycle)
{
        if (cycle == io::GraphicsCycle::fast) {
                // NOTE: The color is also affected for lit terrains when the map is drawn.
                m_flicker_color =
                        rnd::coin_toss()
                        ? colors::yellow().shaded(rnd::range(45, 60))
                        : colors::yellow();
        }
}

std::optional<map::MinimapAppearance> Brazier::minimap_appearance() const
{
        map::MinimapAppearance appearance;

        appearance.color = colors::dark_yellow();
        appearance.legend_text = "火盆";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

// -----------------------------------------------------------------------------
// Item container
// -----------------------------------------------------------------------------
ItemContainer::ItemContainer()
{
        clear();
}

ItemContainer::~ItemContainer()
{
        for (auto* item : m_items) {
                delete item;
        }
}

void ItemContainer::init(
        const terrain::Id terrain_id,
        const int nr_items_to_attempt)
{
        for (auto* item : m_items) {
                delete item;
        }

        m_items.clear();

        if (nr_items_to_attempt <= 0) {
                return;
        }

        // Try until actually succeeded to add at least one item
        while (m_items.empty()) {
                std::vector<item::Id> item_bucket;

                for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                        auto& item_d = item::g_data[i];

                        if (!item_d.allow_spawn) {
                                // Item not allowed to spawn - next item!
                                continue;
                        }

                        const bool can_spawn_in_container =
                                std::find(
                                        std::begin(item_d.native_containers),
                                        std::end(item_d.native_containers),
                                        terrain_id) !=
                                std::end(item_d.native_containers);

                        if (!can_spawn_in_container) {
                                // Item not allowed to spawn in this terrain -
                                // next item!
                                continue;
                        }

                        if (rnd::percent(item_d.chance_to_incl_in_spawn_list)) {
                                item_bucket.push_back(item::Id(i));
                        }
                }

                for (int i = 0; i < nr_items_to_attempt; ++i) {
                        if (item_bucket.empty()) {
                                break;
                        }

                        const int idx =
                                rnd::range(0, (int)item_bucket.size() - 1);

                        const auto id = item_bucket[idx];

                        // Is this item still allowed to spawn (perhaps unique)?
                        if (item::g_data[(size_t)id].allow_spawn) {
                                auto* item = item::make(item_bucket[idx]);

                                item::randomize_item_properties(*item);

                                m_items.push_back(item);
                        }
                        else {
                                // Not allowed to spawn
                                item_bucket.erase(begin(item_bucket) + idx);
                        }
                }
        }
}

void ItemContainer::open(
        const P& terrain_pos,
        actor::Actor* const actor_opening)
{
        if (!actor_opening) {
                // Not opened by an actor (probably opened by the opening spell)
                for (auto* item : m_items) {
                        item_drop::drop_item_on_map(terrain_pos, *item);
                }

                m_items.clear();

                return;
        }

        for (auto* item : m_items) {
                on_item_found(item, terrain_pos);
        }

        msg_log::add("没有更多值得关注的物品了。");

        m_items.clear();
}

void ItemContainer::on_item_found(
        item::Item* const item,
        const P& terrain_pos)
{
        msg_log::clear();

        const std::string name =
                item->name(
                        ItemNameType::plural,
                        ItemNameInfo::yes,
                        ItemNameAttackInfo::main_attack_mode);

        const std::string msg =
                "拾取 " +
                name +
                "? " +
                common_text::g_yes_or_no_hint;

        msg_log::add(
                msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const auto& data = item->data();

        auto* wpn =
                data.ranged.is_ranged_wpn
                ? static_cast<item::Wpn*>(item)
                : nullptr;

        const bool is_unloadable_wpn =
                wpn &&
                (wpn->m_ammo_loaded > 0) &&
                !data.ranged.has_infinite_ammo;

        if (is_unloadable_wpn) {
                msg_log::add("卸下？[u]");
        }

        auto answer = BinaryAnswer::no;

        if (is_unloadable_wpn) {
                answer = query::yes_or_no('u');
        }
        else {
                answer = query::yes_or_no();
        }

        msg_log::clear();

        if (answer == BinaryAnswer::yes) {
                const auto pre_pickup_result = item->pre_pickup_hook();

                switch (pre_pickup_result) {
                case ItemPrePickResult::do_pickup: {
                        audio::play(audio::SfxId::pickup);

                        map::g_player->m_inv.put_in_backpack(item);
                } break;

                case ItemPrePickResult::destroy_item: {
                        delete item;
                } break;

                case ItemPrePickResult::do_nothing: {
                } break;
                }
        }
        else if (answer == BinaryAnswer::no) {
                item_drop::drop_item_on_map(terrain_pos, *item);

                item->discover();
        }
        else {
                // Special key (unload in this case)
                ASSERT(is_unloadable_wpn);

                if (is_unloadable_wpn) {
                        audio::play(audio::SfxId::pickup);

                        auto* const spawned_ammo =
                                item_pickup::unload_ranged_wpn(*wpn);

                        map::g_player->m_inv.put_in_backpack(
                                spawned_ammo);

                        item_drop::drop_item_on_map(terrain_pos, *wpn);
                }
        }
}

void ItemContainer::clear()
{
        for (auto* item : m_items) {
                delete item;
        }

        m_items.clear();
}

void ItemContainer::destroy_single_fragile()
{
        // TODO: Generalize this (something like "is_fragile" item data)

        for (auto it = std::begin(m_items); it != std::end(m_items); ++it) {
                auto* const item = *it;

                const auto& d = item->data();

                if ((d.type == ItemType::potion) ||
                    (d.id == item::Id::molotov)) {
                        delete item;
                        m_items.erase(it);
                        msg_log::add("我听见一声沉闷的碎裂声。");
                        break;
                }
        }
}

// -----------------------------------------------------------------------------
// Tomb
// -----------------------------------------------------------------------------
Tomb::Tomb(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_push_lid_one_in_n(rnd::range(4, 10))
{
        // Contained items
        const int nr_items_min = rnd::one_in(4) ? 0 : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::tomb,
                rnd::range(nr_items_min, nr_items_max));

        // Set appearance - sometimes we base the appearance on the value of the contained items,
        // and sometimes we set a "common" appearance regardless of the items. If the tomb is nicer
        // than "common", then it ALWAYS reflects the items. I.e. if the tomb is "common" it MAY
        // contain good items, if it's nicer than "common" it's guaranteed to have good items.
        if (rnd::one_in(4)) {
                // Base appearance on value of contained items

                for (const item::Item* item : m_item_container.items()) {
                        const auto item_value = item->data().value;

                        if (item_value == item::Value::supreme_treasure) {
                                m_appearance = TombAppearance::marvelous;

                                break;
                        }
                        else if (item_value >= item::Value::minor_treasure) {
                                m_appearance = TombAppearance::ornate;
                        }
                }
        }
        else {
                // Do not base appearance on items - use a common appearance
                m_appearance = TombAppearance::common;
        }

        if (m_appearance == TombAppearance::marvelous) {
                m_trap_id = TombTrapId::monster;
        }
        else {
                // Randomized trait
                std::vector<int> weights((size_t)TombTrapId::END + 1, 0);

                weights[(size_t)TombTrapId::monster] = 8;
                weights[(size_t)TombTrapId::fumes] = 2;
                weights[(size_t)TombTrapId::cursed] = 1;
                weights[(size_t)TombTrapId::END] = 2;

                m_trap_id = TombTrapId(rnd::weighted_choice(weights));
        }
}

void Tomb::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("墓穴被摧毁了。");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

std::string Tomb::name(const Article article) const
{
        const bool is_empty = m_is_open && m_item_container.is_empty();

        const std::string empty_str =
                is_empty
                ? "空 "
                : "";

        const std::string open_str =
                (m_is_open && !is_empty)
                ? "打开 "
                : "";

        std::string a;

        if (article == Article::a) {
                a =
                        (m_is_open || (m_appearance == TombAppearance::ornate))
                        ? "一个 "
                        : "一个 ";
        }
        else {
                a = "这 ";
        }

        std::string appear_str;

        if (!is_empty) {
                switch (m_appearance) {
                case TombAppearance::common:
                case TombAppearance::END: {
                } break;

                case TombAppearance::ornate:
                        appear_str = "华丽的 ";
                        break;

                case TombAppearance::marvelous:
                        appear_str = "奇妙的 ";
                        break;
                }
        }

        return a + empty_str + open_str + appear_str + "墓穴";
}

gfx::TileId Tomb::tile() const
{
        return (
                m_is_open
                        ? gfx::TileId::tomb_open
                        : gfx::TileId::tomb_closed);
}

Color Tomb::color_default() const
{
        switch (m_appearance) {
        case TombAppearance::common:
                return colors::gray();

        case TombAppearance::ornate:
                return colors::cyan();

        case TombAppearance::marvelous:
                return colors::yellow();

        case TombAppearance::END:
                break;
        }

        ASSERT("Failed to set Tomb color" && false);
        return colors::black();
}

std::optional<map::MinimapAppearance> Tomb::minimap_appearance() const
{
        if (m_is_open) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.color = colors::gray();
        appearance.legend_text = "墓穴";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

void Tomb::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (m_item_container.is_empty() && m_is_open) {
                msg_log::add("墓穴是空的。");

                return;
        }

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("这里有一个石箱。");

                return;
        }

        if (m_is_open) {
                player_loot();

                map::memorize_terrain_at(m_pos);
                map::update_vision();

                game_time::tick();

                return;
        }

        msg_log::add("我试着推开棺盖。");

        if (actor_bumping.m_properties.has(prop::Id::weakened)) {
                msg_log::add("看来毫无希望。");

                game_time::tick();

                return;
        }

        int bon = 0;

        if (player_bon::has_trait(Trait::rugged)) {
                bon = 8;
        }
        else if (player_bon::has_trait(Trait::tough)) {
                bon = 4;
        }
        else {
                bon = 0;
        }

        TRACE
                << "Base chance to push lid is: 1 in "
                << m_push_lid_one_in_n << std::endl;

        TRACE
                << "Bonus to roll: "
                << bon << std::endl;

        const int roll_tot = rnd::range(1, m_push_lid_one_in_n) + bon;

        TRACE << "Roll + bonus = " << roll_tot << std::endl;

        bool is_success = false;

        if (roll_tot < (m_push_lid_one_in_n - 9)) {
                msg_log::add("它完全不肯让步。");
        }
        else if (roll_tot < (m_push_lid_one_in_n - 2)) {
                msg_log::add("它抵抗住了。");
        }
        else if (roll_tot == (m_push_lid_one_in_n - 2)) {
                msg_log::add("它稍微动了一下！");
                --m_push_lid_one_in_n;
        }
        else {
                is_success = true;
        }

        if (is_success) {
                open(map::g_player);
        }
        else {
                bash::try_sprain_player();
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Tomb::player_loot()
{
        msg_log::add("我朝墓穴里窥视。");

        if (m_item_container.is_empty()) {
                msg_log::add("里面没有值钱的东西。");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Tomb::open(actor::Actor* const actor_opening)
{
        if (m_is_open) {
                return DidOpen::no;
        }
        else {
                // Was not already open
                m_is_open = true;

                Snd snd(
                        "我听见沉重的石头滑动声。",
                        audio::SfxId::tomb_open,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        map::g_player,
                        SndVol::high,
                        AlertsMon::yes);

                snd.run();

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("盖子脱落了。");

                        if (!m_item_container.is_empty()) {
                                msg_log::add("里面有东西。");
                        }
                }

                trigger_trap(actor_opening);

                return DidOpen::yes;
        }
}

DidTriggerTrap Tomb::trigger_trap(actor::Actor* const actor)
{
        TRACE_FUNC_BEGIN;

        (void)actor;

        switch (m_trap_id) {
        case TombTrapId::monster: {
                trigger_trap_mon();
        } break;

        case TombTrapId::fumes: {
                trigger_trap_fumes();
        } break;

        case TombTrapId::cursed: {
                trigger_trap_cursed();

        } break;

        case TombTrapId::END:
                break;
        }

        m_trap_id = TombTrapId::END;

        m_is_trait_known = true;

        TRACE_FUNC_END;

        return DidTriggerTrap::yes;
}

void Tomb::trigger_trap_mon() const
{
        const std::string mon_id = get_random_allowed_mon_id();

        if (mon_id.empty()) {
                return;
        }

        const actor::MonSpawnResult summoned =
                actor::spawn(m_pos, {mon_id})
                        .make_aware_of_player();

        if (summoned.monsters.size() != 1) {
                ASSERT(false);

                return;
        }

        actor::Actor* const mon = summoned.monsters[0];

        if (map::g_seen.at(m_pos)) {
                const std::string appear_msg = get_mon_appear_msg(mon_id);

                msg_log::add(appear_msg);
        }

        prop::Prop* prop = prop::make(prop::Id::waiting);

        prop->set_duration(2);

        mon->m_properties.apply(prop);
}

std::string Tomb::get_random_allowed_mon_id() const
{
        TRACE_FUNC_BEGIN;

        Range mon_lvl_range(map::g_dlvl - 3, map::g_dlvl + 3);

        TRACE
                << "Allowed monster level range: "
                << "'" << mon_lvl_range.str() << "'"
                << std::endl;

        // Make a weighted monster ID bucket of non-Ghost monsters.
        WeightedItems<std::string> weighted_ids;

        for (auto& it : actor::g_data) {
                const actor::ActorData& data = it.second;

                if (!data.can_spawn_from_tomb) {
                        continue;
                }

                if (data.is_ghost) {
                        // Ghosts are found separately later with - skip them for now.
                        continue;
                }

                // NOTE: The "min" dungeon level in the monster data is used here as a general
                // "strength" of the monster. The "max" dungeon level is not considered.
                const int mon_lvl = data.spawn_min_dlvl;

                if (!mon_lvl_range.is_in_range(mon_lvl)) {
                        continue;
                }

                weighted_ids.items.push_back(data.id);
                weighted_ids.weights.push_back(data.spawn_weight);
        }

#ifndef NDEBUG
        {
                std::string mon_ids_debug_str;

                for (size_t i = 0; i < weighted_ids.items.size(); ++i) {
                        mon_ids_debug_str +=
                                "\n" +
                                weighted_ids.items[i] +
                                " (" +
                                std::to_string(weighted_ids.weights[i]) +
                                ")";
                }

                TRACE
                        << "Possible monster IDs and weights (Ghosts not included):"
                        << mon_ids_debug_str
                        << std::endl;
        }
#endif  // NDEBUG

        // Select a non-Ghost monster as the monster to spawn, if possible. This may be overriden by
        // a Ghost-type monster later.
        std::string mon_id_to_spawn;

        if (!weighted_ids.items.empty()) {
                mon_id_to_spawn = weighted_ids.roll();
        }

        // Find a Ghost-type monster to possibly spawn.
        //
        // Use whatever monster has the highest spawn dungeon level, that is also within the max
        // range of the allowed monster levels.
        //
        std::vector<std::string> highest_lvl_ghost_ids;
        int highest_ghost_lvl = -1;

        for (auto& it : actor::g_data) {
                const actor::ActorData& data = it.second;

                if (!data.can_spawn_from_tomb) {
                        continue;
                }

                if (!data.is_ghost) {
                        continue;
                }

                const int mon_lvl = data.spawn_min_dlvl;

                if (mon_lvl > mon_lvl_range.max) {
                        continue;
                }

                // Valid Ghost-type monster found.

                const std::string& id = it.first;

                if (mon_lvl < highest_ghost_lvl) {
                        // We have already found higher level Ghost-monsters.
                        ASSERT(!highest_lvl_ghost_ids.empty());
                }
                else if (mon_lvl == highest_ghost_lvl) {
                        // This is among the highest level Ghost-type monsters.
                        ASSERT(!highest_lvl_ghost_ids.empty());

                        highest_lvl_ghost_ids.push_back(id);
                }
                else {
                        // This is higher than the currently highest level Ghost-type monsters.

                        highest_lvl_ghost_ids.clear();

                        highest_lvl_ghost_ids.push_back(id);

                        highest_ghost_lvl = mon_lvl;
                }
        }

#ifndef NDEBUG
        {
                std::string mon_ids_debug_str;

                for (const std::string& ghost_id : highest_lvl_ghost_ids) {
                        mon_ids_debug_str += "\n" + ghost_id;
                }

                TRACE
                        << "Possible Ghost monsters to spawn:"
                        << mon_ids_debug_str
                        << std::endl;
        }
#endif  // NDEBUG

        ASSERT(!highest_lvl_ghost_ids.empty());

        // Randomly spawn a non-Ghost or Ghost monster (with more weight towards a Ghost monster).
        const Fraction ghost_chance(3, 4);

        if (!highest_lvl_ghost_ids.empty() &&
            (ghost_chance.roll() || weighted_ids.items.empty())) {
                const std::string ghost_id = rnd::element(highest_lvl_ghost_ids);

                mon_id_to_spawn = ghost_id;
        }

        TRACE << "Tomb monster ID to spawn: '" << mon_id_to_spawn << "'" << std::endl;

        TRACE_FUNC_END;

        return mon_id_to_spawn;
}

std::string Tomb::get_mon_appear_msg(const std::string& mon_id) const
{
        const actor::ActorData& d = actor::g_data.at(mon_id);

        // TODO: In the future it would be better to check which "base" monster it is (ghost, ooze,
        // ...) if such an inheritance system is implemented.

        if (d.is_ghost) {
                return "空气骤然变冷。";
        }

        if (d.natural_props[(size_t)prop::Id::ooze]) {
                return "有令人作呕的东西从墓穴里爬了出来！";
        }

        // Standard message.
        return "有什么东西从墓穴里升起！";
}

void Tomb::trigger_trap_fumes() const
{
        const bool is_seen = map::g_seen.at(m_pos);

        if (is_seen) {
                msg_log::add(
                        "墓穴中喷出浓烟！",
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);
        }

        Snd snd(
                "我听见一阵气体喷出的声音。",
                audio::SfxId::gas,
                IgnoreMsgIfOriginSeen::yes,
                m_pos,
                nullptr,
                SndVol::low,
                AlertsMon::yes);

        snd_emit::run(snd);

        prop::Prop* prop = nullptr;

        Color fume_color = colors::magenta();

        const int rnd = rnd::range(1, 100);

        if (rnd < 20) {
                prop = prop::make(prop::Id::poisoned);

                fume_color = colors::light_green();
        }
        else if (rnd < 40) {
                prop = prop::make(prop::Id::diseased);

                fume_color = colors::green();
        }
        else {
                prop = prop::make(prop::Id::paralyzed);

                prop->set_duration(prop->nr_turns_left() * 2);
        }

        explosion::run(
                m_pos,
                ExplType::apply_prop,
                EmitExplSnd::no,
                0,
                ExplExclCenter::no,
                {prop},
                fume_color,
                ExplIsGas::yes);
}

void Tomb::trigger_trap_cursed() const
{
        map::g_player->m_properties.apply(prop::make(prop::Id::cursed));
}

// -----------------------------------------------------------------------------
// Chest
// -----------------------------------------------------------------------------
Chest::Chest(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_open(false),
        m_is_locked(false),
        m_material(ChestMaterial::wood)
{
        if ((map::g_dlvl >= 3) && rnd::fraction(2, 3)) {
                m_material = ChestMaterial::iron;
        }

        // Contained items
        const int nr_items_min = rnd::one_in(4) ? 0 : 1;
        int nr_items_max = 2;
        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::chest,
                rnd::range(nr_items_min, nr_items_max));

        const int locked_numer =
                m_item_container.is_empty()
                ? 1
                : std::min(8, map::g_dlvl);

        m_is_locked = rnd::fraction(locked_numer, 10);
}

void Chest::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("这里有一个箱子。");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("箱子着火了。");

                return;
        }

        if (m_item_container.is_empty() && m_is_open) {
                msg_log::add("箱子是空的。");

                return;
        }

        if (m_is_locked) {
                msg_log::add("箱子锁着。");

                return;
        }

        // Not locked
        if (m_is_open) {
                player_loot();
        }
        else {
                open(map::g_player);
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Chest::player_loot()
{
        msg_log::add("我搜索箱子。");

        if (m_item_container.is_empty()) {
                msg_log::add("里面没有值钱的东西。");
        }
        else {
                // Not empty
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Chest::open(actor::Actor* const actor_opening)
{
        (void)actor_opening;

        m_is_locked = false;

        if (m_is_open) {
                return DidOpen::no;
        }
        else {
                m_is_open = true;

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("箱子打开了。");
                }

                return DidOpen::yes;
        }
}

bool Chest::allow_player_melee_attack(
        const DmgType dmg_type,
        const item::Item& wpn) const
{
        (void)wpn;

        switch (dmg_type) {
        case DmgType::kicking:
                return true;
                break;

        default:
                break;
        }

        return false;
}

void Chest::hit(
        DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        const int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::kicking:
                on_player_kick();
                break;

        default:
                break;
        }
}

void Chest::on_player_kick()
{
        if (!map::g_seen.at(m_pos)) {
                // If player is blind, call the parent hit function
                // instead (generic kicking)
                Terrain::hit(DmgType::kicking, map::g_player);

                return;
        }

        if (m_is_open) {
                msg_log::add("已经打开。");

                return;
        }

        Snd snd(
                "",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes);

        // Is seen and closed

        if (!m_is_locked) {
                msg_log::add("盖子砰地弹开，随后又落下关上。");

                snd.run();

                return;
        }

        // Is locked

        if (map::g_player->m_properties.has(prop::Id::weakened) ||
            (m_material == ChestMaterial::iron)) {
                msg_log::add("看来毫无希望。");

                snd.run();

                return;
        }

        // Chest can be bashed open
        if (rnd::one_in(3)) {
                m_item_container.destroy_single_fragile();
        }

        int open_one_in_n = 0;

        if (player_bon::has_trait(Trait::rugged)) {
                open_one_in_n = 2;
        }
        else if (player_bon::has_trait(Trait::tough)) {
                open_one_in_n = 3;
        }
        else {
                open_one_in_n = 4;
        }

        if (rnd::one_in(open_one_in_n)) {
                msg_log::add("锁断裂，盖子猛地弹开！");

                m_is_locked = false;
                m_is_open = true;
        }
        else {
                msg_log::add("锁纹丝不动。");
        }

        snd.run();
}

std::string Chest::name(const Article article) const
{
        std::string material_str;
        std::string locked_str;
        std::string empty_str;
        std::string open_str;
        std::string a;

        if (m_material == ChestMaterial::wood) {
                material_str = "木制的 ";
                a = "一个 ";
        }
        else {
                material_str = "铁制 ";
                a = "一个 ";
        }

        if (m_is_open) {
                if (m_item_container.is_empty()) {
                        empty_str = "空 ";
                }
                else {
                        open_str = "打开 ";
                }

                a = "一个 ";
        }
        else if (m_is_locked) {
                locked_str = "已锁定 ";

                a = "一个 ";
        }

        if (article == Article::the) {
                a = "这 ";
        }

        return a + locked_str + empty_str + open_str + material_str + "箱子";
}

gfx::TileId Chest::tile() const
{
        return m_is_open ? gfx::TileId::chest_open : gfx::TileId::chest_closed;
}

Color Chest::color_default() const
{
        return (m_material == ChestMaterial::wood) ? colors::dark_brown() : colors::gray();
}

// -----------------------------------------------------------------------------
// Fountain
// -----------------------------------------------------------------------------
Fountain::Fountain(const P& p, const TerrainData* const data) :
        Terrain(p, data)
{
        std::vector<int> weights = {
                4,  // Refreshing
                1,  // XP
                1,  // Bad effect
        };

        const int choice = rnd::weighted_choice(weights);

        switch (choice) {
        case 0: {
                m_fountain_effect = FountainEffect::refreshing;
        } break;

        case 1: {
                m_fountain_effect = FountainEffect::xp;
        } break;

        case 2: {
                const int min = (int)FountainEffect::START_OF_BAD_EFFECTS + 1;
                const int max = (int)FountainEffect::END - 1;

                m_fountain_effect = (FountainEffect)rnd::range(min, max);
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void Fountain::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("喷泉被摧毁了。");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

Color Fountain::color_default() const
{
        if (m_has_drinks_left) {
                if (m_is_tried) {
                        // Has drinks left, tried
                        const auto is_bad =
                                m_fountain_effect >
                                FountainEffect::START_OF_BAD_EFFECTS;

                        return is_bad ? colors::magenta() : colors::light_cyan();
                }
                else {
                        // Has drinks left, not tried
                        return colors::light_blue();
                }
        }
        else {
                // No drinks left
                return colors::gray();
        }
}

std::optional<map::MinimapAppearance> Fountain::minimap_appearance() const
{
        if (!m_has_drinks_left) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.color = color_default();
        appearance.legend_text = "喷泉";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

std::string Fountain::name(const Article article) const
{
        std::string type_str;

        std::string indefinite_article = "一个";

        if (m_has_drinks_left) {
                if (m_is_tried) {
                        type_str = type_name();

                        indefinite_article = type_indefinite_article();
                }
        }
        else {
                type_str = "干涸的";
        }

        const std::string a =
                (article == Article::a)
                ? indefinite_article
                : "这";

        if (!type_str.empty()) {
                type_str = " " + type_str;
        }

        return a + type_str + " 喷泉";
}

void Fountain::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        const bool is_seen = map::g_seen.at(m_pos);

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!m_has_drinks_left) {
                if (is_seen) {
                        msg_log::add("喷泉已干涸。");
                }
                else {
                        msg_log::add(
                                "这里有一座喷泉，不过已经干涸了。");
                }

                return;
        }

        const auto is_bad =
                m_fountain_effect >
                FountainEffect::START_OF_BAD_EFFECTS;

        if (!is_seen || (m_is_tried && is_bad)) {
                msg_log::clear();

                std::string msg;

                if (is_seen) {
                        const std::string name_the =
                                text_format::first_to_lower(
                                        name(Article::the));

                        msg = "从……饮用 " + name_the + "?";
                }
                else {
                        const std::string name_a =
                                text_format::first_to_lower(
                                        name(Article::a));

                        msg = "有 " + name_a + " 在这里。要喝一口吗？";
                }

                msg += " " + common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const auto answer = query::yes_or_no();

                if (answer == BinaryAnswer::no) {
                        msg_log::clear();

                        return;
                }
        }

        auto& properties = map::g_player->m_properties;

        if (!properties.allow_eat(Verbose::yes)) {
                return;
        }

        msg_log::clear();
        msg_log::add("我从喷泉饮水……");

        audio::play(audio::SfxId::fountain_drink);

        switch (m_fountain_effect) {
        case FountainEffect::refreshing: {
                msg_log::add("真是神清气爽。");

                actor::restore_hp(
                        *map::g_player,
                        1,
                        actor::AllowRestoreAboveMax::no,
                        Verbose::no);

                actor::restore_sp(
                        *map::g_player,
                        1,
                        actor::AllowRestoreAboveMax::no,
                        Verbose::no);

                map::g_player->restore_shock(5, true);
        } break;

        case FountainEffect::xp: {
                msg_log::add("我觉得自己更强大了！");
                game::incr_player_xp(g_xp_on_drink_from_xp_fountain);
        } break;

        case FountainEffect::curse: {
                properties.apply(prop::make(prop::Id::cursed));
        } break;

        case FountainEffect::disease: {
                properties.apply(prop::make(prop::Id::diseased));
        } break;

        case FountainEffect::poison: {
                properties.apply(prop::make(prop::Id::poisoned));
        } break;

        case FountainEffect::frenzy: {
                properties.apply(prop::make(prop::Id::frenzied));
        } break;

        case FountainEffect::paralyze: {
                properties.apply(prop::make(prop::Id::paralyzed));
        } break;

        case FountainEffect::blind: {
                properties.apply(prop::make(prop::Id::blind));
        } break;

        case FountainEffect::faint: {
                prop::Prop* prop = prop::make(prop::Id::fainted);

                prop->set_duration(10);

                properties.apply(prop);
        } break;

        case FountainEffect::START_OF_BAD_EFFECTS:
        case FountainEffect::END:
                break;
        }

        m_is_tried = true;

        const int dry_one_in_n = 3;

        if (rnd::one_in(dry_one_in_n)) {
                m_has_drinks_left = false;

                msg_log::add("喷泉干涸了。");
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Fountain::on_new_turn()
{
        if (map::g_player->m_pos.is_adjacent(m_pos) &&
            map::g_seen.at(m_pos)) {
                hints::display(hints::Id::fountains);
        }
}

void Fountain::bless()
{
        if (!has_drinks_left()) {
                return;
        }

        const bool is_bad_effect =
                m_fountain_effect >
                FountainEffect::START_OF_BAD_EFFECTS;

        if (!is_bad_effect) {
                return;
        }

        m_is_tried = false;

        m_fountain_effect = FountainEffect::refreshing;

        if (map::g_seen.at(m_pos)) {
                const std::string name_the =
                        text_format::first_to_lower(
                                name(Article::the));

                msg_log::add(
                        "……中的水 " + name_the +
                        " 似乎更清澈了。");
        }
}

void Fountain::curse()
{
        if (!has_drinks_left()) {
                return;
        }

        const bool is_good_effect =
                m_fountain_effect <
                FountainEffect::START_OF_BAD_EFFECTS;

        if (!is_good_effect) {
                return;
        }

        m_is_tried = false;

        const int min = (int)FountainEffect::START_OF_BAD_EFFECTS + 1;
        const int max = (int)FountainEffect::END - 1;

        m_fountain_effect = (FountainEffect)rnd::range(min, max);

        if (map::g_seen.at(m_pos)) {
                std::string name_the =
                        text_format::first_to_lower(
                                name(Article::the));

                msg_log::add(
                        "……中的水 " +
                        name_the +
                        " 似乎更浑浊了。");
        }
}

std::string Fountain::type_name() const
{
        switch (m_fountain_effect) {
        case FountainEffect::refreshing:
                return "令人神清气爽";
                break;

        case FountainEffect::xp:
                return "振奋";
                break;

        case FountainEffect::curse:
                return "被诅咒的";
                break;

        case FountainEffect::disease:
                return "患病";
                break;

        case FountainEffect::poison:
                return "有毒的";
                break;

        case FountainEffect::frenzy:
                return "激怒";
                break;

        case FountainEffect::paralyze:
                return "致使麻痹";
                break;

        case FountainEffect::blind:
                return "致盲";
                break;

        case FountainEffect::faint:
                return "催眠的";
                break;

        case FountainEffect::START_OF_BAD_EFFECTS:
        case FountainEffect::END:
                break;
        }

        ASSERT(false);

        return "";
}

std::string Fountain::type_indefinite_article() const
{
        switch (m_fountain_effect) {
        case FountainEffect::refreshing:
                return "一个";
                break;

        case FountainEffect::xp:
                return "一个";
                break;

        case FountainEffect::curse:
                return "一个";
                break;

        case FountainEffect::disease:
                return "一个";
                break;

        case FountainEffect::poison:
                return "一个";
                break;

        case FountainEffect::frenzy:
                return "一个";
                break;

        case FountainEffect::paralyze:
                return "一个";
                break;

        case FountainEffect::blind:
                return "一个";
                break;

        case FountainEffect::faint:
                return "一个";
                break;

        case FountainEffect::START_OF_BAD_EFFECTS:
        case FountainEffect::END:
                break;
        }

        ASSERT(false);

        return "";
}

// -----------------------------------------------------------------------------
// Cabinet
// -----------------------------------------------------------------------------
Cabinet::Cabinet(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_open(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::cabinet,
                rnd::range(nr_items_min, nr_items_max));
}

void Cabinet::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("柜子被摧毁了。");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                m_item_container.clear();
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

WasDestroyed Cabinet::on_finished_burning()
{
        if (map::g_seen.at(m_pos)) {
                msg_log::add("柜子烧毁了。");
        }

        auto* const rubble = make(Id::rubble_low, m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::update_terrain(rubble);
        map::update_vision();

        return WasDestroyed::yes;
}

void Cabinet::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("这里有一个柜子。");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("柜子着火了。");

                return;
        }

        if (m_item_container.is_empty() && m_is_open) {
                msg_log::add("柜子是空的。");

                return;
        }

        if (m_is_open) {
                Snd snd(
                        "",
                        audio::SfxId::bookshelf_rummage,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        &actor_bumping,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
                player_loot();
        }
        else {
                open(map::g_player);
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Cabinet::player_loot()
{
        msg_log::add("我搜索柜子。");

        if (m_item_container.is_empty()) {
                msg_log::add("里面没有值钱的东西。");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Cabinet::open(actor::Actor* const actor_opening)
{
        (void)actor_opening;

        if (m_is_open) {
                return DidOpen::no;
        }
        else {
                // Was not already open
                Snd snd(
                        "",
                        audio::SfxId::cabinet_open,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        actor_opening,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
                m_is_open = true;

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("柜子打开了。");
                }

                return DidOpen::yes;
        }
}

std::string Cabinet::name(const Article article) const
{
        std::string str = (article == Article::a) ? "一个 " : "这 ";

        if (m_burn_state == BurnState::burning) {
                str += "燃烧 ";
        }

        return str + "柜子";
}

gfx::TileId Cabinet::tile() const
{
        if (m_is_open) {
                return gfx::TileId::cabinet_open;
        }
        else {
                return gfx::TileId::cabinet_closed;
        }
}

Color Cabinet::color_default() const
{
        return colors::dark_brown();
}

// -----------------------------------------------------------------------------
// Bookshelf
// -----------------------------------------------------------------------------
Bookshelf::Bookshelf(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_looted(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::bookshelf,
                rnd::range(nr_items_min, nr_items_max));
}

void Bookshelf::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("书架被摧毁了。");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                m_item_container.clear();
                m_is_looted = true;
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

WasDestroyed Bookshelf::on_finished_burning()
{
        if (map::g_seen.at(m_pos)) {
                msg_log::add("书架烧毁了。");
        }

        auto* const rubble = make(Id::rubble_low, m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::update_terrain(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void Bookshelf::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("这里有一个书架。");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("书架着火了。");

                return;
        }

        if (m_item_container.is_empty() && m_is_looted) {
                msg_log::add("书架是空的。");

                return;
        }

        if (!m_is_looted) {
                Snd snd(
                        "",
                        audio::SfxId::bookshelf_rummage,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        &actor_bumping,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
                player_loot();
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Bookshelf::player_loot()
{
        msg_log::add(
                "我搜索书架。",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        m_is_looted = true;

        if (m_item_container.is_empty()) {
                msg_log::add("没有什么值得关注。");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

std::string Bookshelf::name(const Article article) const
{
        std::string str = (article == Article::a) ? "一个 " : "这 ";

        if (m_burn_state == BurnState::burning) {
                str += "燃烧 ";
        }

        return str + "书架";
}

gfx::TileId Bookshelf::tile() const
{
        return (
                m_is_looted
                        ? gfx::TileId::bookshelf_empty
                        : gfx::TileId::bookshelf_full);
}

Color Bookshelf::color_default() const
{
        return colors::dark_brown();
}

// -----------------------------------------------------------------------------
// AlchemistBench
// -----------------------------------------------------------------------------
AlchemistBench::AlchemistBench(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_looted(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::alchemist_bench,
                rnd::range(nr_items_min, nr_items_max));
}

void AlchemistBench::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("炼金术士的工作台被摧毁了。");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                m_item_container.clear();
                m_is_looted = true;
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

WasDestroyed AlchemistBench::on_finished_burning()
{
        if (map::g_seen.at(m_pos)) {
                msg_log::add("炼金术士的工作台烧毁了。");
        }

        auto* const rubble = make(Id::rubble_low, m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::update_terrain(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void AlchemistBench::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("这里有一张炼金术士的工作台。");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("炼金术士的工作台着火了。");

                return;
        }

        if (m_item_container.is_empty() && m_is_looted) {
                msg_log::add("炼金术士的工作台是空的。");

                return;
        }

        if (!m_is_looted) {
                Snd snd(
                        "",
                        audio::SfxId::alchemy_rummage,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        &actor_bumping,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
                player_loot();
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void AlchemistBench::player_loot()
{
        msg_log::add(
                "我搜索炼金术士的工作台。",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        m_is_looted = true;

        if (m_item_container.is_empty()) {
                msg_log::add("没有什么值得关注。");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

std::string AlchemistBench::name(const Article article) const
{
        std::string a = (article == Article::a) ? "一个 " : "这 ";

        std::string mod;

        if (m_burn_state == BurnState::burning) {
                if (article == Article::a) {
                        a = "一个 ";
                }

                mod = "燃烧 ";
        }

        return a + mod + "炼金术士工作台";
}

gfx::TileId AlchemistBench::tile() const
{
        return (
                m_is_looted
                        ? gfx::TileId::alchemist_bench_empty
                        : gfx::TileId::alchemist_bench_full);
}

Color AlchemistBench::color_default() const
{
        return colors::brown();
}

// -----------------------------------------------------------------------------
// Cocoon
// -----------------------------------------------------------------------------
Cocoon::Cocoon(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_trapped(rnd::fraction(6, 10)),
        m_is_open(false)
{
        if (m_is_trapped) {
                m_item_container.init(terrain::Id::cocoon, 0);
        }
        else {
                const bool is_treasure_hunter =
                        player_bon::has_trait(Trait::treasure_hunter);

                const Fraction fraction_empty(6, 10);

                const int nr_items_min =
                        fraction_empty.roll()
                        ? 0
                        : 1;

                const int nr_items_max =
                        nr_items_min +
                        (is_treasure_hunter
                                 ? 1
                                 : 0);

                m_item_container.init(
                        terrain::Id::cocoon,
                        rnd::range(nr_items_min, nr_items_max));
        }
}

void Cocoon::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("茧被摧毁了。");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                m_item_container.clear();
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

WasDestroyed Cocoon::on_finished_burning()
{
        if (map::g_seen.at(m_pos)) {
                msg_log::add("茧烧毁了。");
        }

        auto* const rubble = make(Id::rubble_low, m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::update_terrain(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void Cocoon::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("这里有一个茧。");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("茧着火了。");

                return;
        }

        if (m_item_container.is_empty() && m_is_open) {
                msg_log::add("茧是空的。");

                return;
        }

        if (insanity::has_sympt(InsSymptId::phobia_spider)) {
                map::g_player->m_properties.apply(prop::make(prop::Id::terrified));
        }

        if (m_is_open) {
                player_loot();
        }
        else {
                open(map::g_player);
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

DidTriggerTrap Cocoon::trigger_trap(actor::Actor* const actor)
{
        (void)actor;

        if (!m_is_trapped) {
                return DidTriggerTrap::no;
        }

        const int rnd = rnd::range(1, 100);

        if (rnd < 15) {
                // A dead body

                if (player_bon::is_bg(Bg::ghoul)) {
                        m_is_trapped = false;

                        return DidTriggerTrap::no;
                }

                msg_log::add("里面有一具半溶解的人体！");

                map::g_player->incr_shock(12.0, ShockSrc::misc);

                m_is_trapped = false;

                return DidTriggerTrap::yes;
        }
        else if (rnd < 50) {
                // Spiders
                TRACE << "Attempting to spawn spiders" << std::endl;
                std::vector<std::string> spawn_bucket;

                for (const auto& it : actor::g_data) {
                        const actor::ActorData& d = it.second;

                        if (d.is_spider &&
                            (d.actor_size == actor::Size::floor) &&
                            d.is_auto_spawn_allowed &&
                            !d.is_unique) {
                                spawn_bucket.push_back(d.id);
                        }
                }

                const int nr_candidates = (int)spawn_bucket.size();

                if (nr_candidates > 0) {
                        TRACE << "Spawn candidates found, attempting to place"
                              << std::endl;

                        msg_log::add("里面有蜘蛛！");

                        const auto nr_spiders = (size_t)rnd::range(2, 5);

                        const auto idx = rnd::range(0, nr_candidates - 1);

                        const auto actor_id_to_summon = spawn_bucket[idx];

                        actor::spawn(m_pos, {nr_spiders, actor_id_to_summon})
                                .make_aware_of_player();

                        m_is_trapped = false;

                        return DidTriggerTrap::yes;
                }
        }

        return DidTriggerTrap::no;
}

void Cocoon::player_loot()
{
        msg_log::add("我搜索茧。");

        if (m_item_container.is_empty()) {
                msg_log::add("里面是空的。");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Cocoon::open(actor::Actor* const actor_opening)
{
        if (m_is_open) {
                return DidOpen::no;
        }
        else {
                // Was not already open
                m_is_open = true;

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("茧打开了。");
                }

                trigger_trap(actor_opening);

                return DidOpen::yes;
        }
}

std::string Cocoon::name(const Article article) const
{
        std::string str = (article == Article::a) ? "一个 " : "这 ";

        if (m_burn_state == BurnState::burning) {
                str += "燃烧 ";
        }

        return str + "茧";
}

gfx::TileId Cocoon::tile() const
{
        return (
                m_is_open
                        ? gfx::TileId::cocoon_open
                        : gfx::TileId::cocoon_closed);
}

Color Cocoon::color_default() const
{
        return colors::white();
}

}  // namespace terrain
