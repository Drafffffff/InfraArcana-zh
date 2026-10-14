// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "player_bon.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_player_state.hpp"
#include "colors.hpp"
#include "create_character.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "global.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "player_spells.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "spells.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
struct TraitData
{
        Trait id {Trait::END};
        std::string title {};
        std::string descr {};
        std::string extra_descr_when_picking {};
        std::function<void()> on_picked {};
        std::function<void()> on_removed {};
        std::vector<Trait> trait_prereqs {};
        Bg bg_prereq {Bg::END};
        std::vector<Bg> blocked_for_bgs {};
        std::vector<OccultistDomain> blocked_for_occultist_domains {};
};

static TraitData s_trait_data[(size_t)Trait::END];

// NOTE: This is stored separately from the trait data since we sometimes need
// to update the trait data (e.g. to update trait descriptions containing
// information on the player's current spirit for spell traits). Bundling the
// picked state with the other trait data would be confusing and inconvenient.
static bool s_traits_picked[(size_t)Trait::END];

static std::vector<player_bon::TraitLogEntry> s_trait_log;

static auto s_player_bg = Bg::END;
static auto s_player_occultist_domain = OccultistDomain::END;

static const int s_exorcist_bon_trait_lvl_1 = 2;
static const int s_exorcist_bon_trait_lvl_2 = 4;
static const int s_exorcist_bon_trait_lvl_3 = 6;

static const int s_spell_upgrade_lvl_1 = 4;
static const int s_spell_upgrade_lvl_2 = 8;

static std::string trait_descr_for_spell(
        const SpellId spell_id,
        const SpellSkill skill)
{
        std::unique_ptr<Spell> spell(spells::make(spell_id));

        std::string str =
                "获得施放“" +
                spell->name() +
                "\"";

        if (spell->can_be_improved_with_skill()) {
                str +=
                        " 位于 " +
                        spells::skill_to_str(skill) +
                        " 等级";
        }

        str += " -";

        const auto descr = spell->descr_specific(skill);

        for (const auto& line : descr) {
                str += " " + line;
        }

        // Assert that the player character has been initialized, as it is used
        // below, and also spell costs might be affected by whether the caster
        // is the player or not.
        ASSERT(actor::is_player(map::g_player));

        const auto cost_str = spell->cost_range(skill, map::g_player).str();

        str += (" 此法术消耗 " +
                cost_str +
                " 点精神值才能施法。");

        return str;
}

static std::string get_player_available_sp_str()
{
        const std::string sp_str = std::to_string(map::g_player->m_sp);
        const std::string max_sp_str = std::to_string(actor::max_sp(*map::g_player));

        std::string descr = "你目前有 " + sp_str + "/" + max_sp_str + " 精神值";

        if (player_bon::is_bg(Bg::exorcist)) {
                const std::string fp_str = std::to_string(actor::player_state::g_exorcist_fervor);
                const std::string max_fp_str = std::to_string(actor::player_exorcist_max_fervor());

                descr += " 和 " + fp_str + "/" + max_fp_str + " 狂热值";
        }

        descr += ".";

        return descr;
}

static TraitData& trait_data(const Trait id)
{
        ASSERT(id != Trait::END);

        return s_trait_data[(size_t)id];
}

static void set_trait_data(TraitData& d)
{
        ASSERT(d.id != Trait::END);

        s_trait_data[(size_t)d.id] = d;

        d = {};
}

static void update_trait_data()
{
        for (auto& d : s_trait_data) {
                d = {};
        }

        TraitData d;

        // --- Adept Melee Fighter ---
        d.id = Trait::adept_melee;
        d.title = "近战行家";
        d.descr = "近战攻击命中率提高 +10%，伤害 +1";
        set_trait_data(d);

        // --- Expert Melee Fighter ---
        d = trait_data(Trait::adept_melee);
        d.id = Trait::expert_melee;
        d.title = "近战专家";
        d.trait_prereqs = {Trait::adept_melee};
        d.blocked_for_bgs = {Bg::exorcist};
        set_trait_data(d);

        // --- Master Melee Fighter ---
        d = trait_data(Trait::adept_melee);
        d.id = Trait::master_melee;
        d.title = "近战大师";
        d.trait_prereqs = {Trait::expert_melee};
        d.blocked_for_bgs = {Bg::exorcist, Bg::occultist};
        set_trait_data(d);

        // --- Adept Marksman ---
        d.id = Trait::adept_marksman;
        d.title = "神射手";
        d.descr = (
                "枪械和投掷武器命中率提高 +10%，最低伤害 +1（无法提高最高伤害）");
        d.blocked_for_bgs = {Bg::ghoul};
        set_trait_data(d);

        // --- Expert Marksman ---
        d = trait_data(Trait::adept_marksman);
        d.id = Trait::expert_marksman;
        d.title = "神射手";
        d.trait_prereqs = {Trait::adept_marksman};
        d.blocked_for_bgs = {Bg::ghoul, Bg::exorcist};
        set_trait_data(d);

        // --- Master Marksman ---
        d = trait_data(Trait::adept_marksman);
        d.id = Trait::master_marksman;
        d.title = "神枪手";
        d.trait_prereqs = {Trait::expert_marksman};
        d.blocked_for_bgs = {
                Bg::ghoul,
                Bg::exorcist,
                Bg::occultist,
                Bg::flagellant,
        };
        set_trait_data(d);

        // --- Cool-headed ---
        d.id = Trait::cool_headed;
        d.title = "沉着冷静";
        d.descr = "精神震慑抗性 +20%";
        set_trait_data(d);

        // --- Courageous ---
        d = trait_data(Trait::cool_headed);
        d.id = Trait::courageous;
        d.title = "勇敢";
        d.trait_prereqs = {Trait::cool_headed};
        set_trait_data(d);

        // --- Dexterous ---
        d.id = Trait::dexterous;
        d.title = "灵巧";
        d.descr = "闪避攻击的几率 +25%";
        set_trait_data(d);

        // --- Lithe ---
        d = trait_data(Trait::dexterous);
        d.id = Trait::lithe;
        d.title = "灵巧";
        d.trait_prereqs = {Trait::dexterous};
        set_trait_data(d);

        // --- Crippling Strikes ---
        d.id = Trait::crippling_strikes;
        d.title = "致残打击";
        d.descr =
                "近战攻击有60%几率令目标生物虚弱2至3回合（近战伤害减半）";
        d.trait_prereqs = {Trait::dexterous, Trait::adept_melee};
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Fearless ---
        d.id = Trait::fearless;
        d.title = "无畏";
        d.descr = "你不会陷入恐惧，精神震慑抗性提高10%";
        d.on_picked = []() {
                prop::Prop* prop = prop::make(prop::Id::r_fear);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->m_properties.end_prop(prop::Id::r_fear);
        };
        d.trait_prereqs = {Trait::cool_headed};
        set_trait_data(d);

        // --- Stealthy ---
        d.id = Trait::stealthy;
        d.title = "潜行";
        d.descr = "避免被视觉发现的几率 +45%";
        set_trait_data(d);

        // --- Imperceptible ---
        d = trait_data(Trait::stealthy);
        d.id = Trait::imperceptible;
        d.title = "难以察觉";
        d.trait_prereqs = {Trait::stealthy};
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Silent ---
        d.id = Trait::silent;
        d.title = "寂静";
        d.descr =
                "你的所有近战攻击都不会发出声音（无论使用何种武器）；打开或关上门、涉水时也不会惊动生物";
        d.trait_prereqs = {Trait::stealthy};
        set_trait_data(d);

        // --- Vigilant ---
        d.id = Trait::vigilant;
        d.title = "警觉的";
        d.descr = "你总能察觉附近的生物";
        d.blocked_for_occultist_domains = {OccultistDomain::clairvoyant};
        set_trait_data(d);

        // --- Treasure Hunter ---
        d.id = Trait::treasure_hunter;
        d.title = "寻宝者";
        d.descr = "你通常能找到更多物品";
        d.blocked_for_bgs = {
                Bg::exorcist,
                Bg::ghoul,
                Bg::war_vet,
                Bg::flagellant};
        d.blocked_for_occultist_domains = {
                OccultistDomain::enchanter,
                OccultistDomain::invoker,
                OccultistDomain::transmuter};
        set_trait_data(d);

        // --- Self-aware ---
        d.id = Trait::self_aware;
        d.title = "自我意识";
        d.descr =
                "你不会陷入困惑；状态效果剩余回合数会显示出来";
        d.on_picked = []() {
                prop::Prop* prop = prop::make(prop::Id::r_conf);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->m_properties.end_prop(prop::Id::r_conf);
        };
        d.trait_prereqs = {Trait::stout_spirit, Trait::cool_headed};
        set_trait_data(d);

        // --- Healer ---
        d.id = Trait::healer;
        d.title = "治疗者";
        d.descr =
                "使用医疗器材只需一半时间与补给";
        d.blocked_for_bgs = {Bg::ghoul};
        set_trait_data(d);

        // --- Rapid Recoverer ---
        d.id = Trait::rapid_recoverer;
        d.title = "快速恢复者";
        d.descr = "每三回合恢复1点生命值";
        d.trait_prereqs = {Trait::tough, Trait::healer};
        d.blocked_for_bgs = {Bg::ghoul};
        set_trait_data(d);

        // --- Survivalist ---
        d.id = Trait::survivalist;
        d.title = "生存专家";
        d.descr =
                "你不会患病，创伤计数减半并向下取整（也就是说，计算战斗、生命值和恢复惩罚时创伤数量减半；达到6处创伤而非3处时行走速度才会变慢；受到10处而非5处创伤才会死亡）";
        d.on_picked = []() {
                prop::Prop* prop = prop::make(prop::Id::r_disease);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->m_properties.end_prop(prop::Id::r_disease);
        };
        d.blocked_for_bgs = {Bg::ghoul, Bg::flagellant};
        set_trait_data(d);

        // --- Stout Spirit ---
        d.id = Trait::stout_spirit;
        d.title = "强健精神";
        d.descr =
                "精神值 +2，精神值恢复速度提高，可抵御有害法术（法术被抵挡后，需 125-150 回合才能恢复法术抗性）";
        d.on_picked = []() {
                prop::Prop* prop = prop::make(prop::Id::r_spell);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);

                const int spi_incr = 2;

                actor::change_max_sp(
                        *map::g_player,
                        spi_incr,
                        Verbose::no);

                actor::restore_sp(
                        *map::g_player,
                        spi_incr,
                        actor::AllowRestoreAboveMax::no,
                        Verbose::no);
        };
        d.on_removed = []() {
                actor::change_max_sp(*map::g_player, -2, Verbose::no);
        };
        set_trait_data(d);

        // --- Strong Spirit ---
        d = trait_data(Trait::stout_spirit);
        d.id = Trait::strong_spirit;
        d.title = "强大的精神";
        d.descr =
                "精神值 +2，精神值恢复速度提高；法术被抵挡后，需 75-100 回合才能恢复法术抗性";
        d.trait_prereqs = {Trait::stout_spirit};
        set_trait_data(d);

        // --- Mighty Spirit ---
        d = trait_data(Trait::stout_spirit);
        d.id = Trait::mighty_spirit;
        d.title = "强大的精神";
        d.descr =
                "精神值 +2，精神值恢复速度提高；法术被抵挡后，需 25-50 回合才能恢复法术抗性";
        d.trait_prereqs = {Trait::strong_spirit};
        set_trait_data(d);

        // --- Meditative ---
        d.id = Trait::meditative;
        d.title = "沉思者";
        d.descr =
                "进入专注状态，使下一次施法无需消耗回合，施法费用减少 1 点；施法后需要 125–150 回合才能重新恢复此状态";
        d.on_picked = []() {
                prop::Prop* prop = prop::make(prop::Id::meditative_focused);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.trait_prereqs = {Trait::stout_spirit, Trait::cool_headed};
        d.blocked_for_bgs = {Bg::ghoul, Bg::war_vet, Bg::rogue};
        set_trait_data(d);

        // --- Sage ---
        d.id = Trait::sage;
        d.title = "贤者";
        d.descr =
                "专注时，法术也会以更高技能等级施放；恢复专注状态所需时间缩短至75至100回合。";
        d.trait_prereqs = {Trait::stout_spirit, Trait::meditative};
        d.blocked_for_bgs = trait_data(Trait::meditative).blocked_for_bgs;
        // TODO: Consider allowing it for Exorcists (and have third level spells
        // for them, probably also traits for the third level spells).
        d.blocked_for_bgs.push_back(Bg::exorcist);
        d.blocked_for_bgs.push_back(Bg::flagellant);
        set_trait_data(d);

        // --- Absorption ---
        d.id = Trait::absorbtion;
        d.title = "吸收";
        d.descr =
                "- 法术抗性（来自精神特质或法术护盾）抵抗一次法术时，恢复 1–6 点精神值";
        d.trait_prereqs = {Trait::strong_spirit};
        set_trait_data(d);

        // --- Tough ---
        d.id = Trait::tough;
        d.title = "强壮";
        d.descr =
                "生命值 +6，抵抗燃烧、中毒和麻痹的几率 +10%；踢击时较不易扭伤；力量相关物品互动（如撞开物体）更容易成功";
        d.on_picked = []() {
                const int hp_incr = 6;

                actor::change_max_hp(*map::g_player, hp_incr, Verbose::no);

                actor::restore_hp(
                        *map::g_player,
                        hp_incr,
                        actor::AllowRestoreAboveMax::no,
                        Verbose::no);
        };
        d.on_removed = []() {
                actor::change_max_hp(*map::g_player, -6, Verbose::no);
        };
        set_trait_data(d);

        // --- Rugged ---
        d = trait_data(Trait::tough);
        d.id = Trait::rugged;
        d.title = "坚韧";
        d.trait_prereqs = {Trait::tough};
        set_trait_data(d);

        // --- Unbreakable ---
        d = trait_data(Trait::rugged);
        d.id = Trait::unbreakable;
        d.title = "坚不可摧";
        d.bg_prereq = Bg::flagellant;
        d.trait_prereqs = {Trait::rugged};
        set_trait_data(d);

        // --- Thick Skinned ---
        d.id = Trait::thick_skinned;
        d.title = "厚皮";
        d.descr = "+1 点护甲（物理伤害减少 1 点）";
        d.trait_prereqs = {Trait::tough};
        set_trait_data(d);

        // --- Callous ---
        d = trait_data(Trait::thick_skinned);
        d.id = Trait::callous;
        d.title = "冷酷";
        d.bg_prereq = Bg::flagellant;
        d.trait_prereqs = {Trait::thick_skinned};
        set_trait_data(d);

        // --- Resistant ---
        d.id = Trait::resistant;
        d.title = "抗性";
        d.descr =
                "抵抗燃烧、中毒和麻痹的几率 +25%，且这些效果的持续时间减半";
        d.trait_prereqs = {Trait::tough};
        set_trait_data(d);

        // --- Strong-backed ---
        d.id = Trait::strong_backed;
        d.title = "体格健壮";
        d.descr = "负重上限 +50%";
        d.trait_prereqs = {Trait::tough};
        set_trait_data(d);

        // --- Bane of the Undead ---
        d.id = Trait::undead_bane;
        d.title = "亡灵克星";
        d.descr =
                "对所有不死怪物的近战与远程攻击伤害 +2；对灵体不死怪物的命中率 +50%";
        d.trait_prereqs =
                {Trait::tough, Trait::fearless, Trait::stout_spirit};
        set_trait_data(d);

        // --- Electrically Inclined ---
        d.id = Trait::elec_incl;
        d.title = "电能亲和";
        d.descr =
                "法杖充能速度翻倍，奇异装置发生故障或损坏的几率降低，电灯笼续航翻倍，电系武器伤害+1";
        d.blocked_for_bgs = {Bg::ghoul};
        set_trait_data(d);

        // --- Cast Bless ---
        d.id = Trait::cast_bless_i;
        d.title = "施放祝福";
        d.descr = trait_descr_for_spell(SpellId::bless, SpellSkill::basic);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::learn_spell(
                        SpellId::bless,
                        Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::bless);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Bless II ---
        d.id = Trait::cast_bless_ii;
        d.title = "施放祝福 II";
        d.descr = trait_descr_for_spell(SpellId::bless, SpellSkill::expert);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::incr_spell_skill(SpellId::bless, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(SpellId::bless, SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_bless_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Cleansing Fire ---
        d.id = Trait::cast_cleansing_fire_i;
        d.title = "施放净化之火";
        d.descr = trait_descr_for_spell(SpellId::cleansing_fire, SpellSkill::basic);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::learn_spell(SpellId::cleansing_fire, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::cleansing_fire);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Cleansing Fire II ---
        d.id = Trait::cast_cleansing_fire_ii;
        d.title = "施放净化之火 II";
        d.descr = trait_descr_for_spell(SpellId::cleansing_fire, SpellSkill::expert);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::incr_spell_skill(SpellId::cleansing_fire, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(
                        SpellId::cleansing_fire,
                        SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_cleansing_fire_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Heal ---
        d.id = Trait::cast_heal_i;
        d.title = "施放治疗";
        d.descr = trait_descr_for_spell(SpellId::heal, SpellSkill::basic);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::learn_spell(SpellId::heal, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::heal);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Heal II ---
        d.id = Trait::cast_heal_ii;
        d.title = "施放治疗 II";
        d.descr = trait_descr_for_spell(SpellId::heal, SpellSkill::expert);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::incr_spell_skill(SpellId::heal, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(SpellId::heal, SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_heal_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Light ---
        d.id = Trait::cast_light_i;
        d.title = "施放光亮术";
        d.descr = trait_descr_for_spell(SpellId::light, SpellSkill::basic);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::learn_spell(SpellId::light, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::light);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Light II ---
        d.id = Trait::cast_light_ii;
        d.title = "施放光亮术 II";
        d.descr = trait_descr_for_spell(SpellId::light, SpellSkill::expert);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::incr_spell_skill(SpellId::light, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(SpellId::light, SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_light_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Sanctuary ---
        d.id = Trait::cast_sanctuary_i;
        d.title = "施放圣所";
        d.descr = trait_descr_for_spell(SpellId::sanctuary, SpellSkill::basic);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::learn_spell(SpellId::sanctuary, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::sanctuary);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast Sanctuary II ---
        d.id = Trait::cast_sanctuary_ii;
        d.title = "施放圣所 II";
        d.descr = trait_descr_for_spell(SpellId::sanctuary, SpellSkill::expert);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::incr_spell_skill(SpellId::sanctuary, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(SpellId::sanctuary, SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_sanctuary_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast See Invisible ---
        d.id = Trait::cast_see_invisible_i;
        d.title = "施放识破隐形";
        d.descr = trait_descr_for_spell(SpellId::see_invis, SpellSkill::basic);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::learn_spell(SpellId::see_invis, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::remove_learned_spell(SpellId::see_invis);
        };
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Cast See Invisible II ---
        d.id = Trait::cast_see_invisible_ii;
        d.title = "施放识破隐形 II";
        d.descr = trait_descr_for_spell(SpellId::see_invis, SpellSkill::expert);
        d.extra_descr_when_picking = get_player_available_sp_str();
        d.on_picked = []() {
                player_spells::incr_spell_skill(SpellId::see_invis, Verbose::no);
        };
        d.on_removed = []() {
                player_spells::set_spell_skill(SpellId::see_invis, SpellSkill::basic);
        };
        d.trait_prereqs = {Trait::cast_see_invisible_i};
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Prolonged Life ---
        d.id = Trait::prolonged_life;
        d.title = "延寿";
        d.descr =
                "受到的任何致命伤害都会改为扣除你的狂热值";
        d.bg_prereq = Bg::exorcist;
        set_trait_data(d);

        // --- Ravenous ---
        d.id = Trait::ravenous;
        d.title = "贪婪的";
        d.descr =
                "用爪攻击时，偶尔会吞食活的受害者";
        d.trait_prereqs = {Trait::adept_melee};
        d.bg_prereq = Bg::ghoul;
        set_trait_data(d);

        // --- Foul ---
        d.id = Trait::foul;
        d.title = "污秽";
        d.descr =
                "+1 爪击伤害；用爪子攻击时，受害者尸体偶尔会冒出凶猛蠕虫来攻击你的敌人";
        d.bg_prereq = Bg::ghoul;
        set_trait_data(d);

        // --- Toxic ---
        d.id = Trait::toxic;
        d.title = "有毒";
        d.descr =
                "+1 爪击伤害，免疫毒素；爪击经常会使受害者中毒";
        d.on_picked = []() {
                prop::Prop* prop = prop::make(prop::Id::r_poison);

                prop->set_indefinite();

                map::g_player->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        };
        d.on_removed = []() {
                map::g_player->m_properties.end_prop(prop::Id::r_poison);
        };
        d.trait_prereqs = {Trait::foul};
        d.bg_prereq = Bg::ghoul;
        set_trait_data(d);

        // --- Indomitable Fury ---
        d.id = Trait::indomitable_fury;
        d.title = "不屈之怒";
        d.descr =
                "狂乱期间，你免疫创伤，爪击还会造成恐惧";
        d.trait_prereqs = {Trait::adept_melee, Trait::tough};
        d.bg_prereq = Bg::ghoul;
        set_trait_data(d);

        // --- Elusive ---
        d.id = Trait::elusive;
        d.title = "难以捉摸";
        d.descr =
                "生物记住你的时间只有正常时长的一半（向上取整）。";
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Vicious ---
        d.id = Trait::vicious;
        d.title = "凶狠的";
        d.descr = "背刺伤害 +100%（在原有 +50% 之外额外增加）";
        d.trait_prereqs = {Trait::stealthy, Trait::dexterous};
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Ruthless ---
        d.id = Trait::ruthless;
        d.title = "冷酷无情";
        d.descr = "背刺伤害 +100%";
        d.trait_prereqs = {Trait::vicious};
        d.bg_prereq = Bg::rogue;
        set_trait_data(d);

        // --- Steady Aimer ---
        d.id = Trait::steady_aimer;
        d.title = "稳健瞄准者";
        d.descr =
                "站立不动后，下一回合的远程攻击会造成最大伤害并获得10%命中加成，受到伤害时除外";
        d.bg_prereq = Bg::war_vet;
        set_trait_data(d);

        // --- Galvanization ---
        d.id = Trait::galvanization;
        d.title = "电化";
        d.descr =
                "施放任何血之领域法术后，若施法导致生命值损失，则获得持续 4–6 回合的再生效果（每回合额外恢复 1 点生命值）";
        d.bg_prereq = Bg::flagellant;
        set_trait_data(d);

        d.id = Trait::enthusiasm;
        d.title = "热忱";
        d.descr =
                "濒死效果的所有加成翻倍";
        d.bg_prereq = Bg::flagellant;
        set_trait_data(d);

        // --- Memento Mori ---
        d.id = Trait::memento_mori;
        d.title = "死亡纪念";
        d.descr =
                "将濒死状态的生命值阈值提高到8点，并使效果持续时间延长50%（向下取整）";
        d.bg_prereq = Bg::flagellant;
        set_trait_data(d);
}

static bool is_trait_blocked_for_bg(
        const Trait trait,
        const Bg bg,
        const OccultistDomain occultist_domain)
{
        const auto d = trait_data(trait);

        const bool is_blocked_for_bg =
                std::find(
                        std::begin(d.blocked_for_bgs),
                        std::end(d.blocked_for_bgs),
                        bg) != std::end(d.blocked_for_bgs);

        const bool is_blocked_for_domain =
                std::find(
                        std::begin(d.blocked_for_occultist_domains),
                        std::end(d.blocked_for_occultist_domains),
                        occultist_domain) !=
                std::end(d.blocked_for_occultist_domains);

        return is_blocked_for_bg || is_blocked_for_domain;
}

static void incr_spell_skills(const SpellDomain spell_domain)
{
        for (int i = 0; i < (int)SpellId::END; ++i) {
                const auto id = (SpellId)i;

                const std::unique_ptr<Spell> spell(spells::make(id));

                if (spell->player_can_learn() &&
                    (spell->domain() == spell_domain)) {
                        player_spells::incr_spell_skill(id, Verbose::yes);
                }
        }
}

static bool is_spell_upgrade_clvl(const int clvl)
{
        return (
                (clvl == s_spell_upgrade_lvl_1) ||
                (clvl == s_spell_upgrade_lvl_2));
}

// -----------------------------------------------------------------------------
// player_bon
// -----------------------------------------------------------------------------
namespace player_bon
{
void init()
{
        s_player_bg = Bg::END;

        s_player_occultist_domain = OccultistDomain::END;

        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                s_traits_picked[i] = false;
        }

        update_trait_data();

        s_trait_log.clear();
}

void save()
{
        saving::put_int((int)s_player_bg);

        saving::put_int((int)s_player_occultist_domain);

        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                saving::put_bool(s_traits_picked[i]);
        }

        saving::put_int((int)s_trait_log.size());

        for (const auto& e : s_trait_log) {
                saving::put_int(e.clvl);

                saving::put_int((int)e.trait_id);

                saving::put_bool(e.is_removal);
        }
}

void load()
{
        s_player_bg = (Bg)saving::get_int();

        s_player_occultist_domain = (OccultistDomain)saving::get_int();

        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                s_traits_picked[i] = saving::get_bool();
        }

        const int nr_trait_log_entries = saving::get_int();

        s_trait_log.resize(nr_trait_log_entries);

        for (player_bon::TraitLogEntry& e : s_trait_log) {
                e.clvl = saving::get_int();

                e.trait_id = (Trait)saving::get_int();

                e.is_removal = saving::get_bool();
        }
}

std::string bg_title(const Bg id)
{
        switch (id) {
        case Bg::exorcist:
                return "驱魔师";

        case Bg::flagellant:
                return "苦修者";

        case Bg::ghoul:
                return "食尸鬼";

        case Bg::occultist:
                return "神秘学者";

        case Bg::rogue:
                return "游荡者";

        case Bg::war_vet:
                return "老兵";

        case Bg::END:
                break;
        }

        ASSERT(false);

        return "";
}

std::string occultist_profession_title(const OccultistDomain domain)
{
        switch (domain) {
        case OccultistDomain::clairvoyant:
                return "千里眼";

        case OccultistDomain::enchanter:
                return "附魔师";

        case OccultistDomain::invoker:
                return "召咒者";

        case OccultistDomain::transmuter:
                return "炼金术士";

        case OccultistDomain::END:
                break;
        }

        ASSERT(false);

        return "";
}

SpellDomain occultist_to_spell_domain(
        const OccultistDomain occultist_domain)
{
        switch (occultist_domain) {
        case OccultistDomain::clairvoyant:
                return SpellDomain::clairvoyance;
                break;

        case OccultistDomain::enchanter:
                return SpellDomain::enchantment;
                break;

        case OccultistDomain::invoker:
                return SpellDomain::invocation;
                break;

        case OccultistDomain::transmuter:
                return SpellDomain::transmutation;
                break;

        case OccultistDomain::END:
                break;
        }

        return SpellDomain::END;
}

std::string trait_title(const Trait id)
{
        return trait_data(id).title;
}

std::vector<ColoredString> bg_descr(const Bg id)
{
        std::vector<ColoredString> descr;

        auto put = [&descr](const std::string& str) {
                descr.emplace_back(str, colors::text());
        };

        auto put_trait = [&descr](const Trait trait_id) {
                const auto t = trait_title(trait_id);
                const auto d = trait_descr(trait_id);

                descr.emplace_back(
                        "{COLOR_WHITE}" + t + "{color_reset}： " + d,
                        colors::gray());
        };

        switch (id) {
        case Bg::exorcist:
                put("无法使用手稿、祭坛、石碑或锣；但摧毁这些物品可获得经验和狂热值（拾取手稿时会将其摧毁）。狂热值可用于施法；精神值不足以施法时会自动消耗狂热值。");
                put("");
                put("初始携带圣徽，可恢复精神值，并提高对精神震慑与恐惧的抗性。");
                put("");
                put("在角色等级达到以下级别时获得额外特质： " +
                    std::to_string(s_exorcist_bon_trait_lvl_1) +
                    ", " +
                    std::to_string(s_exorcist_bon_trait_lvl_2) +
                    "，以及 " +
                    std::to_string(s_exorcist_bon_trait_lvl_3) +
                    ".");
                put("");
                put_trait(Trait::stout_spirit);
                put("");
                put_trait(Trait::undead_bane);
                break;

        case Bg::flagellant:
                put("受到伤害时不会承受精神震慑。");
                put("");
                put("若受到伤害时生命值降至6点或以下，就会陷入濒死状态，持续5至7回合（近战伤害+3、近战命中率+30%、护甲+3）。");
                put("");
                put("戴着无法取下的刑具项圈；行走需要额外回合，潜行与闪避降低20%。然而，项圈令苦修者更能忍受肉体痛苦，护甲提高3点。");
                put("");
                put("专精于血之领域的法术。角色达到以下等级时 " +
                    std::to_string(s_spell_upgrade_lvl_1) +
                    " 和 " +
                    std::to_string(s_spell_upgrade_lvl_2) +
                    "，此领域中的所有法术都会以更高技能等级施放。");
                put("");
                put("- 施放已记忆的血之领域法术时，承受的精神震慑降低 25%。");
                put("");
                put_trait(Trait::self_aware);
                put("");
                put_trait(Trait::tough);
                break;

        case Bg::ghoul:
                put("不会恢复生命值，也无法使用医疗器材——只能通过啃食尸体治疗（等待在尸体旁即可进食）。");
                put("");
                put("可以随意激起狂暴，狂暴结束后也不会变虚弱。");
                put("");
                put("生命值 +8。");
                put("");
                put("免疫疾病和感染。");
                put("");
                put("不会扭伤。");
                put("");
                put("可以在黑暗中视物。");
                put("");
                put("- 见到怪物和身处黑暗时，承受的精神震慑降低 50%；但光照带来的震慑减免加成也减半。");
                put("");
                put("- 使用枪械和投掷武器时命中率降低 15%。");
                put("");
                put("所有食尸鬼都与你结盟。");
                break;

        case Bg::occultist:
                put("专精于一种法术领域（创建角色时选择）。角色达到以下等级时 " +
                    std::to_string(s_spell_upgrade_lvl_1) +
                    " 和 " +
                    std::to_string(s_spell_upgrade_lvl_2) +
                    "，所选领域中的所有法术都会以更高技能等级施放。此选择也会决定开局法术。");
                put("");
                put("- 施放已记忆的法术，以及使用或鉴定奇异物品（例如喝药剂或施放手稿法术）时，承受的精神震慑降低 50%。");
                put("");
                put("初始携带数枚骨护符，可用于获得法术抗性或驱散魔法陷阱。");
                put("");
                put("精神值 +3（在“坚韧精神”基础上额外增加）。");
                put("");
                put_trait(Trait::stout_spirit);
                break;

        case Bg::rogue:
                put("随着时间流逝而被动承受的精神震慑减少25%。");
                put("");
                put("发现隐藏怪物、门和陷阱的几率提高 +10%。");
                put("");
                put("更久地保持对其他生物存在的警觉。");
                put("");
                put("可以感知独特怪物或强大圣物的存在。");
                put("");
                put("获得了一件神器，可迷惑所有敌人的心智，使它们忘记使用者的存在。");
                put("");
                put_trait(Trait::stealthy);
                break;

        case Bg::war_vet:
                put("立即切换到预备武器。");
                put("");
                put("初始携带防弹衣。");
                put("");
                put("护甲的耐久度延长一倍。");
                put("");
                put_trait(Trait::adept_marksman);
                put("");
                put_trait(Trait::adept_melee);
                put("");
                put_trait(Trait::tough);
                put("");
                put_trait(Trait::healer);
                break;

        case Bg::END:
                ASSERT(false);
                break;
        }

        return descr;
}

std::string occultist_domain_descr(const OccultistDomain domain)
{
        switch (domain) {
        case OccultistDomain::clairvoyant:
                return (
                        "专精于侦测与学习。天生能够探测周围区域的门、陷阱、楼梯及其他值得留意的地点。角色达到4级时，该能力还会揭示物品；达到8级时，则会揭示生物");

        case OccultistDomain::enchanter:
                return "专精于辅助、削弱、迷惑和蛊惑";

        case OccultistDomain::invoker:
                return "专精于引导破坏性力量";

        case OccultistDomain::transmuter:
                return "专精于操纵物质、能量与时间";

        case OccultistDomain::END:
                ASSERT(false);
                break;
        }

        return "";
}

std::string trait_descr(const Trait id)
{
        return trait_data(id).descr;
}

std::string trait_descr_extra_when_picking(const Trait id)
{
        return trait_data(id).extra_descr_when_picking;
}

TraitPrereqData trait_prereqs(
        const Trait trait,
        const Bg bg,
        const OccultistDomain occultist_domain)
{
        const auto& d = trait_data(trait);

        TraitPrereqData result;

        result.traits = d.trait_prereqs;
        result.bg = d.bg_prereq;

        // Remove traits which are blocked for this background (prerequisites
        // are considered fulfilled).
        for (auto it = std::begin(result.traits);
             it != std::end(result.traits);) {
                if (is_trait_blocked_for_bg(*it, bg, occultist_domain)) {
                        it = result.traits.erase(it);
                }
                else {
                        // Not blocked
                        ++it;
                }
        }

        // Sort lexicographically.
        std::sort(
                std::begin(result.traits),
                std::end(result.traits),
                [](const Trait& t1, const Trait& t2) {
                        const std::string str1 = trait_title(t1);
                        const std::string str2 = trait_title(t2);
                        return str1 < str2;
                });

        return result;
}

Bg bg()
{
        return s_player_bg;
}

OccultistDomain occultist_domain()
{
        return s_player_occultist_domain;
}

bool is_bg(Bg bg)
{
        ASSERT(bg != Bg::END);

        return bg == s_player_bg;
}

bool has_trait(const Trait id)
{
        return s_traits_picked[(size_t)id];
}

std::vector<Bg> pickable_bgs()
{
        std::vector<Bg> result;

        result.reserve((int)Bg::END);

        for (int i = 0; i < (int)Bg::END; ++i) {
                result.push_back((Bg)i);
        }

        // Sort lexicographically.
        std::sort(
                std::begin(result),
                std::end(result),
                [](const Bg bg1, const Bg bg2) {
                        const std::string str1 = bg_title(bg1);
                        const std::string str2 = bg_title(bg2);
                        return str1 < str2;
                });

        return result;
}

std::vector<OccultistDomain> pickable_occultist_domains()
{
        std::vector<OccultistDomain> result;

        result.reserve((int)OccultistDomain::END);

        for (int i = 0; i < (int)OccultistDomain::END; ++i) {
                result.push_back((OccultistDomain)i);
        }

        // Sort lexicographically.
        std::sort(
                std::begin(result),
                std::end(result),
                [](
                        const OccultistDomain domain_1,
                        const OccultistDomain domain_2) {
                        const SpellDomain spell_domain_1 =
                                occultist_to_spell_domain(domain_1);

                        const SpellDomain spell_domain_2 =
                                occultist_to_spell_domain(domain_2);

                        const std::string str1 =
                                spells::spell_domain_title(spell_domain_1);

                        const std::string str2 =
                                spells::spell_domain_title(spell_domain_2);

                        return str1 < str2;
                });

        return result;
}

UnpickedTraitsData unpicked_traits(
        const Bg bg,
        const OccultistDomain occultist_domain)
{
        update_trait_data();

        UnpickedTraitsData result;

        for (const auto& d : s_trait_data) {
                if (s_traits_picked[(size_t)d.id]) {
                        continue;
                }

                // Check if trait is explicitly blocked for this background.
                const bool is_blocked_for_bg =
                        is_trait_blocked_for_bg(
                                d.id,
                                bg,
                                occultist_domain);

                if (is_blocked_for_bg) {
                        continue;
                }

                // Check trait prerequisites (traits and background).

                // NOTE: Traits blocked for the current background are not
                // considered prerequisites.
                const auto prereq_data =
                        trait_prereqs(
                                d.id,
                                bg,
                                occultist_domain);

                const bool is_bg_ok =
                        (s_player_bg == prereq_data.bg) ||
                        (prereq_data.bg == Bg::END);

                if (!is_bg_ok) {
                        continue;
                }

                bool is_trait_prereqs_ok = true;

                for (const auto& prereq : prereq_data.traits) {
                        if (!s_traits_picked[(size_t)prereq]) {
                                is_trait_prereqs_ok = false;
                                break;
                        }
                }

                if (is_trait_prereqs_ok) {
                        result.traits_can_be_picked.push_back(d.id);
                }
                else {
                        result.traits_prereqs_not_met.push_back(d.id);
                }

        }  // Trait loop

        // Sort lexicographically
        std::sort(
                std::begin(result.traits_can_be_picked),
                std::end(result.traits_can_be_picked),
                [](const Trait& t1, const Trait& t2) {
                        const std::string str1 = trait_title(t1);
                        const std::string str2 = trait_title(t2);
                        return str1 < str2;
                });

        std::sort(
                std::begin(result.traits_prereqs_not_met),
                std::end(result.traits_prereqs_not_met),
                [](const Trait& t1, const Trait& t2) {
                        const std::string str1 = trait_title(t1);
                        const std::string str2 = trait_title(t2);
                        return str1 < str2;
                });

        return result;
}  // unpicked_traits

std::vector<Trait> traits_can_be_removed()
{
        update_trait_data();

        std::vector<Trait> result;

        for (const auto& d : s_trait_data) {
                if (!s_traits_picked[(size_t)d.id]) {
                        continue;
                }

                bool is_prereq_for_other_trait = false;

                for (const auto& d_other : s_trait_data) {
                        if (!s_traits_picked[(size_t)d_other.id]) {
                                continue;
                        }

                        const auto match =
                                std::find(
                                        std::begin(d_other.trait_prereqs),
                                        std::end(d_other.trait_prereqs),
                                        d.id);

                        if (match != std::end(d_other.trait_prereqs)) {
                                is_prereq_for_other_trait = true;
                                break;
                        }
                }

                if (is_prereq_for_other_trait) {
                        continue;
                }

                result.push_back(d.id);
        }

        return result;
}

void pick_bg(const Bg bg)
{
        TRACE_FUNC_BEGIN;

        ASSERT(bg != Bg::END);

        s_player_bg = bg;

        switch (s_player_bg) {
        case Bg::exorcist: {
                pick_trait(Trait::stout_spirit);
                pick_trait(Trait::undead_bane);

                // Mark all scrolls as found, so that they do not yield XP.
                for (auto& d : item::g_data) {
                        if (d.type == ItemType::scroll) {
                                d.is_found = true;
                        }
                }
        } break;

        case Bg::flagellant: {
                pick_trait(Trait::self_aware);
                pick_trait(Trait::tough);

                prop::Prop* flagellant_prop = prop::make(prop::Id::flagellant);

                flagellant_prop->set_indefinite();

                map::g_player->m_properties.apply(
                        flagellant_prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        } break;

        case Bg::ghoul: {
                prop::Prop* r_disease = prop::make(prop::Id::r_disease);

                r_disease->set_indefinite();

                map::g_player->m_properties.apply(
                        r_disease,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);

                prop::Prop* darkvis = prop::make(prop::Id::darkvision);

                darkvis->set_indefinite();

                map::g_player->m_properties.apply(
                        darkvis,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);

                player_spells::learn_spell(SpellId::frenzy, Verbose::no);

                actor::change_max_hp(*map::g_player, 8, Verbose::no);
        } break;

        case Bg::occultist: {
                pick_trait(Trait::stout_spirit);

                actor::change_max_sp(*map::g_player, 3, Verbose::no);
        } break;

        case Bg::rogue: {
                pick_trait(Trait::stealthy);
        } break;

        case Bg::war_vet: {
                pick_trait(Trait::adept_marksman);
                pick_trait(Trait::adept_melee);
                pick_trait(Trait::tough);
                pick_trait(Trait::healer);
        } break;

        case Bg::END:
                break;
        }

        TRACE_FUNC_END;
}

void pick_occultist_domain(const OccultistDomain domain)
{
        ASSERT(domain != OccultistDomain::END);

        s_player_occultist_domain = domain;

        switch (domain) {
        case OccultistDomain::clairvoyant: {
                auto* prop =
                        static_cast<prop::MagicSearching*>(
                                prop::make(
                                        prop::Id::magic_searching));

                prop->set_indefinite();

                prop->set_range(g_fov_radi_int);

                map::g_player->m_properties.apply(
                        prop,
                        prop::PropSrc::intr,
                        true,
                        Verbose::no);
        } break;

        case OccultistDomain::enchanter: {
        } break;

        case OccultistDomain::invoker: {
        } break;

        case OccultistDomain::transmuter: {
        } break;

        case OccultistDomain::END: {
                ASSERT(false);
        } break;
        }
}

void on_player_gained_lvl(const int new_lvl)
{
        TRACE_FUNC_BEGIN;

        switch (s_player_bg) {
        case Bg::exorcist: {
                const bool is_exorcist_extra_trait =
                        (new_lvl == s_exorcist_bon_trait_lvl_1) ||
                        (new_lvl == s_exorcist_bon_trait_lvl_2) ||
                        (new_lvl == s_exorcist_bon_trait_lvl_3);

                if (is_exorcist_extra_trait) {
                        states::push(
                                std::make_unique<PickTraitState>(
                                        "你获得了一个额外特质！",
                                        IsCharacterCreationTraitPick::no));
                }
        } break;

        case Bg::flagellant: {
                if (is_spell_upgrade_clvl(new_lvl)) {
                        incr_spell_skills(SpellDomain::blood);
                }
        } break;

        case Bg::ghoul: {
        } break;

        case Bg::occultist: {
                switch (s_player_occultist_domain) {
                case OccultistDomain::clairvoyant: {
                        if (is_spell_upgrade_clvl(new_lvl)) {
                                incr_spell_skills(SpellDomain::clairvoyance);
                        }

                        prop::Prop* const prop =
                                map::g_player->m_properties.prop(
                                        prop::Id::magic_searching);

                        ASSERT(prop);

                        auto* const searching =
                                static_cast<prop::MagicSearching*>(prop);

                        if (new_lvl == s_spell_upgrade_lvl_1) {
                                searching->set_allow_reveal_items();
                        }
                        else if (new_lvl == s_spell_upgrade_lvl_2) {
                                searching->set_allow_reveal_creatures();
                        }
                } break;

                case OccultistDomain::enchanter: {
                        if (is_spell_upgrade_clvl(new_lvl)) {
                                incr_spell_skills(SpellDomain::enchantment);
                        }
                } break;

                case OccultistDomain::invoker: {
                        if (is_spell_upgrade_clvl(new_lvl)) {
                                incr_spell_skills(SpellDomain::invocation);
                        }
                } break;

                case OccultistDomain::transmuter: {
                        if (is_spell_upgrade_clvl(new_lvl)) {
                                incr_spell_skills(SpellDomain::transmutation);
                        }
                } break;

                case OccultistDomain::END: {
                        ASSERT(false);
                } break;
                }
        } break;

        case Bg::rogue: {
        } break;

        case Bg::war_vet: {
        } break;

        case Bg::END: {
                ASSERT(false);
        } break;
        }

        TRACE_FUNC_END;
}

void set_all_traits_to_picked()
{
        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                s_traits_picked[i] = true;
        }
}

void pick_trait(const Trait id)
{
        TRACE_FUNC_BEGIN;

        ASSERT(id != Trait::END);

        s_traits_picked[(size_t)id] = true;

        TraitLogEntry trait_log_entry;

        trait_log_entry.trait_id = id;
        trait_log_entry.clvl = game::clvl();
        trait_log_entry.is_removal = false;

        s_trait_log.push_back(trait_log_entry);

        const TraitData& d = trait_data(id);

        if (d.on_picked) {
                // Has trait pick function
                trait_data(id).on_picked();
        }

        TRACE_FUNC_END;
}

void remove_trait(const Trait id)
{
        TRACE_FUNC_BEGIN;

        ASSERT(id != Trait::END);

        s_traits_picked[(size_t)id] = false;

        TraitLogEntry trait_log_entry;

        trait_log_entry.trait_id = id;
        trait_log_entry.clvl = game::clvl();
        trait_log_entry.is_removal = true;

        s_trait_log.push_back(trait_log_entry);

        const TraitData& d = trait_data(id);

        // If the trait applies effects when picked, it must also revert those
        ASSERT(!(d.on_picked && !d.on_removed));

        if (d.on_removed) {
                // Has trait removal function
                trait_data(id).on_removed();
        }

        TRACE_FUNC_END;
}

std::vector<TraitLogEntry> trait_log()
{
        return s_trait_log;
}

}  // namespace player_bon
