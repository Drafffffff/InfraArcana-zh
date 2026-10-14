// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_potion.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>

#include "actor.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "insanity.hpp"
#include "inventory_handling.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "state.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"

struct P;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
struct PotionAppearance
{
        std::string name_plain;
        std::string name_a;
        Color color;
};

static std::vector<PotionAppearance> s_potion_appearances;

static std::vector<actor::Actor*> actors_reached_by_onyx_drop()
{
        const int max_dist = 6;

        std::vector<actor::Actor*> actors;

        for (actor::Actor* const actor : game_time::g_actors) {
                if (actor::is_player(actor)) {
                        continue;
                }

                if (king_dist(map::g_player->m_pos, actor->m_pos) > max_dist) {
                        continue;
                }

                actors.push_back(actor);
        }

        return actors;
}

static void apply_properties_with_equal_duration(
        const std::vector<prop::Id> ids,
        actor::Actor& actor)
{
        std::vector<prop::Prop*> props_applied;

        for (prop::Id id : ids) {
                props_applied.push_back(prop::make(id));
        }

        int duration = -1;

        // The duration of all properties is decided by the randomized duration of the first.
        for (prop::Prop* const prop : props_applied) {
                if (duration == -1) {
                        duration = prop->nr_turns_left();
                }
                else {
                        prop->set_duration(duration);
                }
        }

        for (prop::Prop* const prop : props_applied) {
                actor.m_properties.apply(prop);
        }
}

// -----------------------------------------------------------------------------
// potion
// -----------------------------------------------------------------------------
namespace potion
{
void init()
{
        TRACE_FUNC_BEGIN;

        // Init possible potion colors and fake names
        s_potion_appearances.assign(
                {{"金的", "一个金色的", colors::yellow()},
                 {"黄色的", "黄色", colors::yellow()},
                 {"黑暗的", "一个黑暗", colors::gray()},
                 {"黑色的", "一个黑色", colors::gray()},
                 {"油性", "油腻的", colors::gray()},
                 {"烟", "烟熏的", colors::white()},
                 {"粘糊糊的", "一个粘糊糊的", colors::green()},
                 {"绿色的", "一个绿色", colors::light_green()},
                 {"火热", "火热的", colors::light_red()},
                 {"阴暗的", "阴暗的", colors::dark_brown()},
                 {"浑", "泥泞的", colors::brown()},
                 {"紫色", "一朵紫罗兰", colors::violet()},
                 {"橙子", "一个橙子", colors::orange()},
                 {"水汪汪的", "水汪汪的", colors::light_blue()},
                 {"金属色", "金属色", colors::gray()},
                 {"清除", "清晰的", colors::light_white()},
                 {"蒙蒙", "一片迷蒙", colors::light_white()},
                 {"血腥", "血腥的", colors::red()},
                 {"品红", "洋红色", colors::magenta()},
                 {"凝结的", "凝结的", colors::green()},
                 {"发霉", "发霉的", colors::brown()},
                 {"泡沫", "泡沫状", colors::white()}});

        for (auto& d : item::g_data) {
                if (d.type != ItemType::potion) {
                        continue;
                }

                // Color and false name
                const size_t idx = rnd::range(0, (int)s_potion_appearances.size() - 1);

                auto& look = s_potion_appearances[idx];

                d.base_name_un_id.names[(size_t)ItemNameType::plain] =
                        look.name_plain + " 药水";

                d.base_name_un_id.names[(size_t)ItemNameType::plural] =
                        look.name_plain + " 药水";

                d.base_name_un_id.names[(size_t)ItemNameType::a] = look.name_a + " 药水";

                d.color = look.color;

                s_potion_appearances.erase(std::begin(s_potion_appearances) + (int)idx);

                // True name
                const auto* const potion =
                        static_cast<const Potion*>(
                                item::make(d.id, 1));

                const std::string real_type_name = potion->real_name();

                delete potion;

                const std::string real_name = "药水 " + real_type_name;
                const std::string real_name_plural = "药水 " + real_type_name;
                const std::string real_name_a = "一剂药水 " + real_type_name;

                d.base_name.names[(size_t)ItemNameType::plain] = real_name;
                d.base_name.names[(size_t)ItemNameType::plural] = real_name_plural;
                d.base_name.names[(size_t)ItemNameType::a] = real_name_a;
        }

        TRACE_FUNC_END;
}

void save()
{
        for (int i = 0; i < (int)item::Id::END; ++i) {
                auto& d = item::g_data[i];

                if (d.type != ItemType::potion) {
                        continue;
                }

                saving::put_str(d.base_name_un_id.names[(size_t)ItemNameType::plain]);
                saving::put_str(d.base_name_un_id.names[(size_t)ItemNameType::plural]);
                saving::put_str(d.base_name_un_id.names[(size_t)ItemNameType::a]);
                saving::put_str(colors::color_to_name(d.color));
        }
}

void load()
{
        for (int i = 0; i < (int)item::Id::END; ++i) {
                auto& d = item::g_data[i];

                if (d.type != ItemType::potion) {
                        continue;
                }

                d.base_name_un_id.names[(size_t)ItemNameType::plain] = saving::get_str();
                d.base_name_un_id.names[(size_t)ItemNameType::plural] = saving::get_str();
                d.base_name_un_id.names[(size_t)ItemNameType::a] = saving::get_str();
                d.color = colors::name_to_color(saving::get_str());
        }
}

Potion::Potion(item::ItemData* const item_data) :
        Item(item_data)
{
}

ConsumeItem Potion::activate(actor::Actor* const actor)
{
        ASSERT(actor);

        if (!actor->m_properties.allow_eat(Verbose::yes)) {
                return ConsumeItem::no;
        }

        if (actor::is_player(actor)) {
                // Really quaff a known malign potion?
                if ((alignment() == PotionAlignment::bad) &&
                    m_data->is_alignment_known &&
                    config::warn_on_drink_malign_potion()) {
                        const std::string name = this->name(ItemNameType::a);

                        const std::string msg =
                                "喝 " +
                                name +
                                "? " +
                                common_text::g_yes_or_no_hint;

                        msg_log::add(
                                msg,
                                colors::light_white(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);

                        auto result = query::yes_or_no();

                        msg_log::clear();

                        if (result == BinaryAnswer::no) {
                                return ConsumeItem::no;
                        }
                }

                m_data->is_tried = true;

                audio::play(audio::SfxId::potion_quaff);

                if (m_data->is_identified) {
                        const std::string potion_name =
                                name(ItemNameType::a, ItemNameInfo::none);

                        msg_log::add("我喝 " + potion_name + "...");
                }
                else {
                        // Not identified
                        const std::string potion_name =
                                name(ItemNameType::plain, ItemNameInfo::none);

                        msg_log::add("我喝着不知名的酒 " + potion_name + "...");
                }

                map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

                if (!actor::is_alive(*map::g_player)) {
                        return ConsumeItem::yes;
                }
        }

        quaff_impl(*actor);

        // Also apply effect to nearby creatures if this is a malign potion and
        // the player has the Onyx Drop artifact.
        if ((alignment() == PotionAlignment::bad) &&
            actor::is_player(actor) &&
            actor->m_inv.has_item_in_backpack(item::Id::onyx_drop)) {
                for (actor::Actor* const other_actor : actors_reached_by_onyx_drop()) {
                        quaff_impl(*other_actor);
                }
        }

        if (actor::is_alive(*map::g_player)) {
                game_time::tick();
        }

        return ConsumeItem::yes;
}

void Potion::identify(const Verbose verbose)
{
        if (m_data->is_identified) {
                return;
        }

        m_data->is_identified = true;

        m_data->is_alignment_known = true;

        if (verbose == Verbose::yes) {
                const std::string name_after =
                        name(ItemNameType::a, ItemNameInfo::none);

                msg_log::add("我已确定 " + name_after + ".");

                game::add_history_event("已鉴定 " + name_after);
        }
}

std::vector<std::string> Potion::descr_hook() const
{
        if (m_data->is_identified) {
                return {descr_identified()};
        }
        else {
                auto lines = m_data->base_descr;

                if (m_data->is_alignment_known) {
                        lines.push_back(
                                "这药水是 " +
                                text_format::first_to_lower(alignment_str()) +
                                ".");
                }
                else {
                        lines.emplace_back(
                                "也许保留一段时间会揭示一些关于它的信息。");
                }

                return lines;
        }
}

std::string Potion::alignment_str() const
{
        return ((alignment() == PotionAlignment::good) ? "良性" : "诽谤");
}

void Potion::reveal_alignment() const
{
        if (m_data->is_alignment_known || m_data->is_identified) {
                return;
        }

        TRACE << "Potion alignment discovered" << std::endl;

        const std::string name_plural =
                m_data->base_name_un_id.names[(size_t)ItemNameType::plural];

        const std::string align_str =
                text_format::first_to_lower(alignment_str());

        msg_log::add(
                std::string(
                        "我觉得 " +
                        name_plural +
                        " 是 " +
                        align_str +
                        "."));

        m_data->is_alignment_known = true;
}

void Potion::on_collide(const P& pos, actor::Actor* const actor)
{
        const bool is_seen = map::g_seen.at(pos);
        const auto* const terrain = map::g_terrain.at(pos);

        if (actor) {
                ASSERT(actor::is_alive(*actor));

                if (actor::is_player_aware_of_me(*actor)) {
                        const std::string actor_name =
                                actor::can_player_see_actor(*actor)
                                ? actor::name_the(*actor)
                                : "它";

                        msg_log::add("药水碎裂在 " + actor_name + ".");

                        actor->make_player_aware_of_me();
                }

                collide_hook(pos, actor);
        }
        else if (is_seen) {
                const std::vector<terrain::Id> deep_terrains = {
                        terrain::Id::chasm};

                if (!map_parsers::IsAnyOfTerrains(deep_terrains).run(pos)) {
                        msg_log::add(
                                "药水碎裂在 " +
                                terrain->name(Article::the) +
                                ".");
                }
        }
}

std::string Potion::name_info_str(const ItemNameIdentified id_type) const
{
        std::string str;

        // Only display benign/malign status if alignment is known, and the
        // potion is unidentified.
        if (data().is_alignment_known &&
            (!data().is_identified && (id_type != ItemNameIdentified::force_identified))) {
                str = "(" + alignment_str() + ")";
        }

        return str;
}

Color Potion::interface_color() const
{
        return colors::light_blue();
}

std::string Vitality::real_name() const
{
        return "活力";
}

std::string Vitality::descr_identified() const
{
        return (
                "这种灵药可完全恢复生命值、治愈所有创伤，并治愈失明、失聪、中毒、感染、疾病、虚弱和生命流失。此外，服用后的一段时间内，每回合额外恢复 1 点生命值，而且每回合有 10% 几率治愈一处创伤。");
}

PotionAlignment Vitality::alignment() const
{
        return PotionAlignment::good;
}

void Vitality::quaff_impl(actor::Actor& actor)
{
        std::vector<prop::Id> props_can_heal = {
                prop::Id::blind,
                prop::Id::deaf,
                prop::Id::poisoned,
                prop::Id::infected,
                prop::Id::diseased,
                prop::Id::weakened,
                prop::Id::hp_sap,
                prop::Id::wound};

        for (prop::Id prop_id : props_can_heal) {
                actor.m_properties.end_prop(prop_id);
        }

        actor::restore_hp(actor, 999, actor::AllowRestoreAboveMax::no);

        actor.m_properties.apply(prop::make(prop::Id::regenerating));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Vitality::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Spirit::real_name() const
{
        return "精神";
}

std::string Spirit::descr_identified() const
{
        return "恢复精神，治疗精神衰弱。";
}

PotionAlignment Spirit::alignment() const
{
        return PotionAlignment::good;
}

void Spirit::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.end_prop(prop::Id::spi_sap);

        // SP is always restored at least up to maximum, but can go beyond
        const int sp_max = actor::max_sp(actor);

        const int sp_restored = std::max(10, sp_max - actor.m_sp);

        actor::restore_sp(
                actor,
                sp_restored,
                actor::AllowRestoreAboveMax::yes);

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Spirit::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Blindness::real_name() const
{
        return "失明";
}

std::string Blindness::descr_identified() const
{
        return "导致暂时性视力丧失。";
}

PotionAlignment Blindness::alignment() const
{
        return PotionAlignment::bad;
}

void Blindness::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(prop::make(prop::Id::blind));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Blindness::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Paral::real_name() const
{
        return "瘫痪";
}

std::string Paral::descr_identified() const
{
        return "导致瘫痪。";
}

PotionAlignment Paral::alignment() const
{
        return PotionAlignment::bad;
}

void Paral::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(prop::make(prop::Id::paralyzed));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Paral::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Disease::real_name() const
{
        return "疾病";
}

std::string Disease::descr_identified() const
{
        return "引起疾病。";
}

PotionAlignment Disease::alignment() const
{
        return PotionAlignment::bad;
}

void Disease::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(prop::make(prop::Id::diseased));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Disease::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Conf::real_name() const
{
        return "困惑";
}

std::string Conf::descr_identified() const
{
        return "造成混乱。";
}

PotionAlignment Conf::alignment() const
{
        return PotionAlignment::bad;
}

void Conf::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(prop::make(prop::Id::confused));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Conf::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Fortitude::real_name() const
{
        return "刚毅";
}

std::string Fortitude::descr_identified() const
{
        return (
                "给消费者带来完全的平静和清醒的头脑，并治愈精神衰弱。");
}

PotionAlignment Fortitude::alignment() const
{
        return PotionAlignment::good;
}

void Fortitude::quaff_impl(actor::Actor& actor)
{
        apply_properties_with_equal_duration(
                {prop::Id::r_fear,
                 prop::Id::r_conf,
                 prop::Id::r_sleep},
                actor);

        actor.m_properties.end_prop(prop::Id::frenzied);
        actor.m_properties.end_prop(prop::Id::hallucinating);
        actor.m_properties.end_prop(prop::Id::mind_sap);

        if (actor::is_player(&actor)) {
                const std::vector<const InsSympt*> sympts = insanity::active_sympts();

                if (!sympts.empty()) {
                        const InsSymptId id = rnd::element(sympts)->id();

                        insanity::end_sympt(id);
                }

                map::g_player->restore_shock(999, false);

                msg_log::add("我感觉更安心了。");
        }

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Fortitude::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Poison::real_name() const
{
        return "毒";
}

std::string Poison::descr_identified() const
{
        return "一种险恶的酿造。";
}

PotionAlignment Poison::alignment() const
{
        return PotionAlignment::bad;
}

void Poison::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(prop::make(prop::Id::poisoned));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Poison::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Insight::real_name() const
{
        return "洞察力";
}

std::string Insight::descr_identified() const
{
        return (
                "这种奇怪的混合物引起了直觉的突然闪现。");
}

PotionAlignment Insight::alignment() const
{
        return PotionAlignment::good;
}

void Insight::quaff_impl(actor::Actor& actor)
{
        (void)actor;

        identify(Verbose::yes);

        // Run identify selection menu

        // NOTE: We push this state BEFORE giving any XP (directly or via
        // identifying stuff), because if the player gains a new level in the
        // process, the trait selection should occur first
        states::push(std::make_unique<SelectIdentify>());

        // Insight gives some extra XP, to avoid making the potion worthless if
        // the player identifies all items)
        msg_log::add("我感觉很有洞察力。");

        game::incr_player_xp(g_xp_on_drink_insight_potion);

        msg_log::more_prompt();
}

void Insight::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;
        (void)actor;
}

std::string Curing::real_name() const
{
        return "养护";
}

std::string Curing::descr_identified() const
{
        return (
                "恢复 3 点生命值，并治愈失明、耳聋、中毒、感染、疾病、虚弱和生命削弱。");
}

PotionAlignment Curing::alignment() const
{
        return PotionAlignment::good;
}

void Curing::quaff_impl(actor::Actor& actor)
{
        std::vector<prop::Id> props_can_heal = {
                prop::Id::blind,
                prop::Id::deaf,
                prop::Id::poisoned,
                prop::Id::infected,
                prop::Id::diseased,
                prop::Id::weakened,
                prop::Id::hp_sap};

        bool is_noticable = false;

        for (prop::Id prop_id : props_can_heal) {
                if (actor.m_properties.end_prop(prop_id)) {
                        is_noticable = true;
                }
        }

        const bool did_restore_hp =
                actor::restore_hp(
                        actor,
                        3,
                        actor::AllowRestoreAboveMax::no);

        if (did_restore_hp) {
                is_noticable = true;
        }

        if (!is_noticable && actor::is_player(&actor)) {
                msg_log::add("我感觉很好。");

                is_noticable = true;
        }

        if (is_noticable &&
            actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Curing::collide_hook(const P& pos, actor::Actor* actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Resistance::real_name() const
{
        return "反抗";
}

std::string Resistance::descr_identified() const
{
        return (
                "完全保护消费者免受电、火和毒害 - 并防止瘫痪。");
}

PotionAlignment Resistance::alignment() const
{
        return PotionAlignment::good;
}

void Resistance::quaff_impl(actor::Actor& actor)
{
        apply_properties_with_equal_duration(
                {prop::Id::r_elec,
                 prop::Id::r_fire,
                 prop::Id::r_poison,
                 prop::Id::r_para},
                actor);

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Resistance::collide_hook(const P& pos, actor::Actor* actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Descent::real_name() const
{
        return "血统";
}

std::string Descent::descr_identified() const
{
        return (
                "一种奇怪的液体，会导致消费者非物质化并沉入地下。");
}

PotionAlignment Descent::alignment() const
{
        return PotionAlignment::good;
}

void Descent::quaff_impl(actor::Actor& actor)
{
        (void)actor;

        if (map::g_dlvl < (g_dlvl_last + 1)) {
                if (!map::g_player->m_properties.has(prop::Id::descend)) {
                        map::g_player->m_properties.apply(prop::make(prop::Id::descend));
                }
        }
        else {
                // Cannot bypass the boss level.
                msg_log::add("我感觉到一种淡淡的下沉感，但很快就消失了……");
        }

        identify(Verbose::yes);
}

void Descent::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;
        (void)actor;
}

std::string Skill::real_name() const
{
        return "技能";
}

std::string Skill::descr_identified() const
{
        return (
                "消费者变得更加熟练（命中率、躲避、潜行和搜索+10%）。");
}

PotionAlignment Skill::alignment() const
{
        return PotionAlignment::good;
}

void Skill::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(prop::make(prop::Id::extra_skill));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Skill::collide_hook(const P& pos, actor::Actor* actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Carapace::real_name() const
{
        return "甲壳";
}

std::string Carapace::descr_identified() const
{
        return (
                "使消费者的皮肤上长出坚韧的甲壳，提供对物理攻击的保护（+3护甲点），以及一定程度的抗燃烧能力（+25% c抵抗燃烧的机会）。");
}

PotionAlignment Carapace::alignment() const
{
        return PotionAlignment::good;
}

void Carapace::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(prop::make(prop::Id::magic_carapace));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Carapace::collide_hook(const P& pos, actor::Actor* actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Blinking::real_name() const
{
        return "闪烁";
}

std::string Blinking::descr_identified() const
{
        return (
                "导致消费者迅速从存在中消失，并再次出现在他们选择的附近位置。");
}

PotionAlignment Blinking::alignment() const
{
        return PotionAlignment::good;
}

void Blinking::quaff_impl(actor::Actor& actor)
{
        if (actor::is_player(&actor)) {
                msg_log::add("我消失并重新出现。");
        }

        const int max_dist = 4;

        teleport(actor, ShouldCtrlTele::always, max_dist);

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Blinking::collide_hook(const P& pos, actor::Actor* actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

std::string Burrowing::real_name() const
{
        return "掘地";
}

std::string Burrowing::descr_identified() const
{
        return (
                "它注入了经过炼金术处理的冥神之血，赋予使用者在泥土和岩石中挖掘的能力（可以挖掘墙壁和瓦砾）。");
}

PotionAlignment Burrowing::alignment() const
{
        return PotionAlignment::good;
}

void Burrowing::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(prop::make(prop::Id::burrowing));

        if (actor::can_player_see_actor(actor)) {
                identify(Verbose::yes);
        }
}

void Burrowing::collide_hook(const P& pos, actor::Actor* actor)
{
        (void)pos;

        if (actor) {
                quaff_impl(*actor);
        }
}

}  // namespace potion
