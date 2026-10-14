// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_data.hpp"

#include <memory>
#include <unordered_map>

#include "colors.hpp"
#include "debug.hpp"
#include "item_att_property.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "sound.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
using StrToIdMap = std::unordered_map<std::string, item::Id>;

static const StrToIdMap s_str_to_intr_item_id_map = {
        {"ITEMINTR_BITE", item::Id::intr_bite},
        {"ITEMINTR_CLAW", item::Id::intr_claw},
        {"ITEMINTR_DUST_ENGULF", item::Id::intr_dust_engulf},
        {"ITEMINTR_EARTH_BREATH", item::Id::intr_earth_breath},
        {"ITEMINTR_ENERGY_ENGULF", item::Id::intr_energy_engulf},
        {"ITEMINTR_FIRE_BREATH", item::Id::intr_fire_breath},
        {"ITEMINTR_FIRE_ENGULF", item::Id::intr_fire_engulf},
        {"ITEMINTR_GHOST_TOUCH", item::Id::intr_ghost_touch},
        {"ITEMINTR_HEADBUTT", item::Id::intr_headbutt},
        {"ITEMINTR_KICK", item::Id::intr_kick},
        {"ITEMINTR_LIGHTNING_BREATH", item::Id::intr_lightning_breath},
        {"ITEMINTR_MAUL", item::Id::intr_maul},
        {"ITEMINTR_MIND_LEECH_STING", item::Id::intr_mind_leech_sting},
        {"ITEMINTR_NET_THROW", item::Id::intr_net_throw},
        {"ITEMINTR_PUNCH", item::Id::intr_punch},
        {"ITEMINTR_PUNCH_KNOCKBACK", item::Id::intr_punch_knockback},
        {"ITEMINTR_PUS_SPEW", item::Id::intr_pus_spew},
        {"ITEMINTR_PUTRID_SPIT", item::Id::intr_putrid_spit},
        {"ITEMINTR_RAVEN_PECK", item::Id::intr_raven_peck},
        {"ITEMINTR_SNAKE_VENOM_SPIT", item::Id::intr_snake_venom_spit},
        {"ITEMINTR_SPEAR_THRUST", item::Id::intr_spear_thrust},
        {"ITEMINTR_SPORES", item::Id::intr_spores},
        {"ITEMINTR_STING", item::Id::intr_sting},
        {"ITEMINTR_STRANGE_COLOR_TOUCH", item::Id::intr_strange_color_touch},
        {"ITEMINTR_STRANGLE", item::Id::intr_strangle},
        {"ITEMINTR_STRIKE", item::Id::intr_strike},
        {"ITEMINTR_VAMPIRIC_BITE", item::Id::intr_vampiric_bite},
        {"ITEMINTR_WATER_BREATH", item::Id::intr_water_breath},
        {"ITEMINTR_WEB_BOLA", item::Id::intr_web_bola},
};

using StrToItemSetIdMap = std::unordered_map<std::string, item::ItemSetId>;

static const StrToItemSetIdMap s_str_to_item_set_id_map = {
        {"ITEMSET_MINOR_TREASURE", item::ItemSetId::minor_treasure},
        {"ITEMSET_MAJOR_TREASURE", item::ItemSetId::major_treasure},
        {"ITEMSET_SUPREME_TREASURE", item::ItemSetId::supreme_treasure},
        {"ITEMSET_FIREARM", item::ItemSetId::firearm},
        {"ITEMSET_SPIKE_GUN", item::ItemSetId::spike_gun},
        {"ITEMSET_WITCH_EYE", item::ItemSetId::witch_eye},
        {"ITEMSET_FLUCTUATING_MATERIAL", item::ItemSetId::fluctuating_material},
        {"ITEMSET_ZEALOT_SPIKED_MACE", item::ItemSetId::zealot_spiked_mace},
        {"ITEMSET_PRIEST_DAGGER", item::ItemSetId::priest_dagger},
        {"ITEMSET_ELECTRIC_GUN", item::ItemSetId::electric_gun},
        {"ITEMSET_MORPHIC_BLASTER", item::ItemSetId::morphic_blaster},
        {"ITEMSET_MI_GO_ARMOR", item::ItemSetId::mi_go_armor},
        {"ITEMSET_HIGH_PRIEST_GUARD_WAR_VET", item::ItemSetId::high_priest_guard_war_vet},
        {"ITEMSET_HIGH_PRIEST_GUARD_ROGUE", item::ItemSetId::high_priest_guard_rogue}};

static const std::string s_electric_gun_hp_drained_str =
        std::to_string(g_electric_gun_hp_drained);

static const std::string s_electric_gun_hp_disable_range_str =
        Range(
                g_electric_gun_regen_disabled_min_turns,
                g_electric_gun_regen_disabled_max_turns)
                .str();

static const std::string s_morphic_blaster_hp_drained_str =
        std::to_string(g_morphic_blaster_hp_drained);

static const std::string s_morphic_blaster_hp_disable_range_str =
        Range(
                g_morphic_blaster_regen_disabled_min_turns,
                g_morphic_blaster_regen_disabled_max_turns)
                .str();

static void mod_spawn_chance(item::ItemData& data, const double factor)
{
        data.chance_to_incl_in_spawn_list =
                (int)((double)data.chance_to_incl_in_spawn_list * factor);
}

// Item archetypes (defaults)
static void reset_data(item::ItemData& d, ItemType const item_type)
{
        switch (item_type) {
        case ItemType::general:
                d = {};
                break;

        case ItemType::melee_wpn:
                reset_data(d, ItemType::general);
                d.type = ItemType::melee_wpn;
                d.is_stackable = false;
                d.weight = item::Weight::medium;
                d.character = ')';
                d.color = colors::white();
                d.main_attack_mode = AttackMode::melee;
                d.melee.hit_chance_mod = 0;
                d.melee.is_melee_wpn = true;
                d.melee.miss_sfx = audio::SfxId::miss_medium;
                d.melee.hit_small_sfx = audio::SfxId::hit_small;
                d.melee.hit_medium_sfx = audio::SfxId::hit_medium;
                d.melee.hit_hard_sfx = audio::SfxId::hit_hard;
                d.ranged.is_throwable_wpn = true;
                d.ranged.throw_hit_chance_mod = -25;
                d.ranged.effective_range = {0, 3};
                d.ranged.max_range = d.ranged.effective_range.max + 3;
                d.land_on_hard_snd_msg = "我听到叮叮当当的声音。";
                d.land_on_hard_sfx = audio::SfxId::metal_clank;
                break;

        case ItemType::melee_wpn_intr:
                reset_data(d, ItemType::melee_wpn);
                d.type = ItemType::melee_wpn_intr;
                d.is_intr = true;
                d.spawn_std_range = Range(-1, -1);
                d.chance_to_incl_in_spawn_list = 0;
                d.allow_spawn = false;
                d.melee.hit_small_sfx = audio::SfxId::hit_small;
                d.melee.hit_medium_sfx = audio::SfxId::hit_medium;
                d.melee.hit_hard_sfx = audio::SfxId::hit_hard;
                d.melee.miss_sfx = audio::SfxId::END;
                d.ranged.is_throwable_wpn = false;
                break;

        case ItemType::ranged_wpn:
                reset_data(d, ItemType::general);
                d.type = ItemType::ranged_wpn;
                d.is_stackable = false;
                d.weight = item::Weight::medium;
                d.character = '}';
                d.color = colors::white();
                d.melee.is_melee_wpn = true;
                d.melee.dmg = WpnDmg(1, 3);
                d.melee.dmg_type = DmgType::blunt;
                d.main_attack_mode = AttackMode::ranged;
                d.ranged.is_ranged_wpn = true;
                d.ranged.projectile_character = '/';
                d.ranged.projectile_color = colors::white();
                d.spawn_std_range.max = g_dlvl_last_mid_game;
                d.melee.hit_small_sfx = audio::SfxId::hit_small;
                d.melee.hit_medium_sfx = audio::SfxId::hit_medium;
                d.melee.hit_hard_sfx = audio::SfxId::hit_hard;
                d.melee.miss_sfx = audio::SfxId::miss_medium;
                d.ranged.snd_vol = SndVol::high;
                break;

        case ItemType::ranged_wpn_intr:
                reset_data(d, ItemType::ranged_wpn);
                d.type = ItemType::ranged_wpn_intr;
                d.is_intr = true;
                d.ranged.has_infinite_ammo = true;
                d.spawn_std_range = Range(-1, -1);
                d.chance_to_incl_in_spawn_list = 0;
                d.allow_spawn = false;
                d.melee.is_melee_wpn = false;
                d.ranged.projectile_character = '*';
                d.ranged.snd_vol = SndVol::low;
                break;

        case ItemType::throwing_wpn:
                reset_data(d, ItemType::general);
                d.type = ItemType::throwing_wpn;
                d.weight = item::Weight::extra_light;
                d.is_stackable = true;
                d.spawn_std_range.max = g_dlvl_last_mid_game;
                d.ranged.snd_vol = SndVol::low;
                d.ranged.is_throwable_wpn = true;
                break;

        case ItemType::ammo:
                reset_data(d, ItemType::general);
                d.type = ItemType::ammo;
                d.weight = item::Weight::extra_light;
                d.character = '{';
                d.color = colors::white();
                d.tile = gfx::TileId::ammo;
                d.spawn_std_range.max = g_dlvl_last_mid_game;
                break;

        case ItemType::ammo_mag:
                reset_data(d, ItemType::ammo);
                d.type = ItemType::ammo_mag;
                d.weight = item::Weight::light;
                d.is_stackable = false;
                d.spawn_std_range.max = g_dlvl_last_mid_game;
                break;

        case ItemType::scroll:
                // NOTE: Scroll spawning chances are set elsewhere
                reset_data(d, ItemType::general);
                d.type = ItemType::scroll;
                d.has_std_activate = true;
                d.base_descr = {
                        "一段可怕咒语的简短转录。上面有一种奇异的气息，仿佛纸张本身蕴含着某种力量。",
                        "应该可以正确发音，但目的不明。"};
                d.value = item::Value::minor_treasure;
                d.weight = item::Weight::none;
                d.is_identified = false;
                d.is_spell_domain_known = false;
                d.xp_on_found = 8;
                d.character = '?';
                d.color = colors::white();
                d.tile = gfx::TileId::scroll;
                d.max_stack_at_spawn = 1;
                d.land_on_hard_snd_msg = "";
                d.native_containers.push_back(terrain::Id::chest);
                d.native_containers.push_back(terrain::Id::tomb);
                d.native_containers.push_back(terrain::Id::cabinet);
                d.native_containers.push_back(terrain::Id::bookshelf);
                d.native_containers.push_back(terrain::Id::cocoon);
                break;

        case ItemType::potion:
                reset_data(d, ItemType::general);
                d.type = ItemType::potion;
                d.has_std_activate = true;
                d.base_descr = {
                        "一个小玻璃瓶，里面装有神秘的混合物。"};
                d.value = item::Value::minor_treasure;
                d.chance_to_incl_in_spawn_list = 60;
                d.weight = item::Weight::light;
                d.is_identified = false;
                d.is_alignment_known = false;
                d.xp_on_found = 8;
                d.character = '!';
                d.tile = gfx::TileId::potion;
                d.ranged.throw_hit_chance_mod = 15;
                d.ranged.dmg = WpnDmg(1, 3);
                d.ranged.dmg_type = DmgType::blunt;
                d.ranged.always_break_on_throw = true;
                d.max_stack_at_spawn = 1;
                d.land_on_hard_snd_msg = "";
                d.ranged.is_throwable_wpn = true;
                d.native_containers.push_back(terrain::Id::chest);
                d.native_containers.push_back(terrain::Id::tomb);
                d.native_containers.push_back(terrain::Id::cabinet);
                d.native_containers.push_back(terrain::Id::alchemist_bench);
                d.native_containers.push_back(terrain::Id::cocoon);
                break;

        case ItemType::device:
                reset_data(d, ItemType::general);
                d.type = ItemType::device;
                d.value = item::Value::major_treasure;
                d.has_std_activate = true;
                d.base_name_un_id = {
                        "奇怪的装置",
                        "奇怪的设备",
                        "奇怪的装置"};
                d.base_descr = {
                        "一个小机械。它不可能是由人类大脑设计的。尽管它的尺寸很小，但它看起来却非常复杂。通过正常手段是无法了解其目的或功能的。"};
                d.weight = item::Weight::light;
                d.is_identified = false;
                d.character = '%';
                d.tile = gfx::TileId::device1;
                d.is_stackable = false;
                d.land_on_hard_snd_msg = "我听到叮叮当当的声音。";
                d.land_on_hard_sfx = audio::SfxId::metal_clank;
                d.chance_to_incl_in_spawn_list = 7;
                d.native_containers.push_back(terrain::Id::chest);
                d.native_containers.push_back(terrain::Id::cocoon);
                break;

        case ItemType::rod:
                reset_data(d, ItemType::general);
                d.type = ItemType::rod;
                d.value = item::Value::major_treasure;
                d.has_std_activate = true;
                d.base_descr = {
                        "圆柱形金属装置。它似乎是为人类的双手而设计的，无论出于什么邪恶的目的。"};
                d.weight = item::Weight::light;
                d.is_identified = false;
                d.xp_on_found = 15;
                d.character = '%';
                d.tile = gfx::TileId::rod;
                d.is_stackable = false;
                d.land_on_hard_snd_msg = "我听到叮叮当当的声音。";
                d.land_on_hard_sfx = audio::SfxId::metal_clank;
                d.chance_to_incl_in_spawn_list = 7;
                d.native_containers.push_back(terrain::Id::chest);
                d.native_containers.push_back(terrain::Id::cocoon);
                break;

        case ItemType::armor:
                reset_data(d, ItemType::general);
                d.type = ItemType::armor;
                d.character = '[';
                d.tile = gfx::TileId::armor;
                d.is_stackable = false;
                break;

        case ItemType::head_wear:
                reset_data(d, ItemType::general);
                d.type = ItemType::head_wear;
                d.character = '[';
                d.is_stackable = false;
                break;

        case ItemType::explosive:
                reset_data(d, ItemType::general);
                d.type = ItemType::explosive;
                d.has_std_activate = true;
                d.weight = item::Weight::light;
                d.character = '-';
                d.max_stack_at_spawn = 2;
                d.land_on_hard_snd_msg = "";
                break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
ItemData g_data[(size_t)Id::END];

void init()
{
        TRACE_FUNC_BEGIN;

        ItemData d;

        reset_data(d, ItemType::general);
        d.id = Id::trapezohedron;
        d.base_name = {
                "闪亮的偏方体",
                "闪亮的偏方体",
                "闪亮的偏方体"};
        d.spawn_std_range = Range(-1, -1);
        d.chance_to_incl_in_spawn_list = 0;
        d.allow_spawn = false;
        d.is_stackable = false;
        d.character = '*';
        d.color = colors::light_red();
        d.tile = gfx::TileId::trapez;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::sawed_off;
        d.base_name = {
                "短截霰弹枪",
                "锯短霰弹枪",
                "一把锯短的霰弹枪"};
        d.base_descr = {
                "与标准霰弹枪相比，锯短型霰弹枪的有效射程较短，但在近距离时更具破坏性。它装有两个枪管，两个枪管都卸完后需要重新装弹。"};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::shotgun;
        d.ranged.is_shotgun = true;
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.max_ammo = 2;
        d.ranged.dmg = WpnDmg(8, 24);
        d.ranged.hit_chance_mod = 0;
        d.ranged.effective_range = {0, 3};
        d.ranged.dmg_type = DmgType::shotgun;
        d.ranged.ammo_item_id = Id::shotgun_shell;
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到霰弹枪爆炸声。";
        d.ranged.attack_sfx = audio::SfxId::shotgun_sawed_off_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::shotgun_reload;
        d.spawn_std_range.min = 2;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::pump_shotgun;
        d.base_name = {"泵动霰弹枪", "泵动霰弹枪", "泵动霰弹枪"};
        d.base_descr = {
                "泵动式霰弹枪有一个可以前后泵动的手柄，以便弹出用完的弹药并装入新的弹药。它在管状弹匣上方有一个枪管，炮弹可插入其中。该弹匣容量为8发。"};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::shotgun;
        d.ranged.is_shotgun = true;
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.max_ammo = 8;
        d.ranged.dmg = WpnDmg(6, 18);
        d.ranged.hit_chance_mod = 0;
        d.ranged.effective_range = {0, 5};
        d.ranged.dmg_type = DmgType::shotgun;
        d.ranged.ammo_item_id = Id::shotgun_shell;
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到霰弹枪爆炸声。";
        d.ranged.attack_sfx = audio::SfxId::shotgun_pump_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::shotgun_reload;
        d.spawn_std_range.min = 2;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo);
        d.id = Id::shotgun_shell;
        d.base_name = {"霰弹枪外壳", "霰弹枪弹壳", "猎枪弹壳"};
        d.base_descr = {
                "设计用于从霰弹枪发射的弹药筒。"};
        d.color = colors::light_red();
        d.max_stack_at_spawn = 10;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::morphic_blaster;
        d.base_name = {"变形冲击波", "变形冲击波", "变形冲击波"};
        d.base_descr = {
                "Mi-Go 创造的武器。它发射的射弹在撞击时会释放爆炸性能量。该武器会自我调整以融入其使用者的生物学特征。制导系统与大脑集成，以确保弹丸击中预定目标，而与瞄准技能无关（尽管它沿着路径与意外目标发生碰撞的可能性很小）。",

                "当缺乏 Mi-Go 所使用的特殊能量源的生物使用时，这种武器会从使用者的生命力中汲取能量（" +
                        s_morphic_blaster_hp_drained_str +
                        " 每次攻击都会消耗生命值，被动生命值再生会被禁用 " +
                        s_morphic_blaster_hp_disable_range_str +
                        " 轮，无法执行下一轮）。"};
        d.weight = Weight::moderately_heavy;
        d.tile = gfx::TileId::morphic_blaster;
        d.is_unique = true;
        d.allow_spawn = false;
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.max_ammo = 5;
        d.ranged.dmg = WpnDmg(1, 3);
        d.ranged.effective_range = {0, 999};
        d.allow_display_dmg = false;
        d.ranged.has_infinite_ammo = true;
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到发射弹丸的爆炸声。";
        d.ranged.attack_sfx = audio::SfxId::morphic_blaster;
        d.ranged.projectile_character = '*';
        d.ranged.projectile_color = colors::light_blue();
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.reload_sfx = audio::SfxId::machine_gun_reload;
        d.spawn_std_range.min = g_dlvl_first_mid_game;
        d.chance_to_incl_in_spawn_list = 35;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::tommy_gun;
        d.base_name = {"汤米枪", "汤米枪", "一把汤米枪"};
        d.base_descr = {
                "“汤米枪”是汤普森冲锋枪的昵称 - 一种带有弹鼓和垂直前握把的自动枪支。它发射 0.45 ACP 弹药。弹鼓容量为50发。"};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::tommy_gun;
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.is_machine_gun = true;
        d.ranged.max_ammo = 50;
        d.ranged.dmg = WpnDmg(4, 6);
        d.ranged.hit_chance_mod = -10;
        d.ranged.effective_range = {0, 5};
        d.ranged.ammo_item_id = Id::drum_of_bullets;
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到机关枪的扫射声。";
        d.ranged.attack_sfx = audio::SfxId::machine_gun_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::machine_gun_reload;
        d.spawn_std_range.min = 2;
        d.chance_to_incl_in_spawn_list = 75;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo_mag);
        d.id = Id::drum_of_bullets;
        d.base_name = {
                ".45 ACP 鼓",
                ".45 ACP 鼓",
                "0.45 ACP 的鼓"};
        d.base_descr = {
                "汤米枪使用的弹药。"};
        d.ranged.max_ammo = g_data[(size_t)Id::tommy_gun].ranged.max_ammo;
        d.chance_to_incl_in_spawn_list = 50;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::revolver;
        d.base_name = {
                "S&W 左轮手枪",
                "S&W 左轮手枪",
                "S&W 左轮手枪"};
        d.base_descr = {
                "六发双动左轮手枪。"};
        d.weight = Weight::moderately_light;
        d.tile = gfx::TileId::revolver;
        d.ranged.max_ammo = 6;
        d.ranged.dmg = WpnDmg(5, 10);
        d.ranged.hit_chance_mod = 5;
        d.ranged.effective_range = {0, 5};
        d.ranged.ammo_item_id = Id::revolver_bullet;
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到左轮手枪开火的声音。";
        d.ranged.attack_sfx = audio::SfxId::revolver_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::rifle_revolver_reload;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo);
        d.id = Id::revolver_bullet;
        d.base_name = {
                "左轮手枪.38 子弹",
                "左轮手枪 .38 子弹",
                ".38 子弹左轮手枪"};
        d.base_descr = {
                "S&W 10 型左轮手枪使用的弹药。"};
        d.color = colors::dark_yellow();
        d.max_stack_at_spawn = 10;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::pistol;
        d.base_name = {
                "M1911柯尔特",
                "M1911 小马队",
                "M1911 小马"};
        d.base_descr = {
                "半自动弹匣供弹手枪，可装 0.45 ACP 子弹。"};
        d.weight = Weight::moderately_light;
        d.tile = gfx::TileId::pistol;
        d.ranged.max_ammo = 7;
        d.ranged.dmg = WpnDmg(5, 12);
        d.ranged.hit_chance_mod = 0;
        d.ranged.effective_range = {0, 5};
        d.ranged.ammo_item_id = Id::pistol_mag;
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到手枪开火的声音。";
        d.ranged.attack_sfx = audio::SfxId::pistol_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::pistol_reload;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo_mag);
        d.id = Id::pistol_mag;
        d.base_name = {
                "柯尔特 .45 ACP 弹匣",
                "柯尔特 .45 ACP 弹匣",
                "一个柯尔特 .45 ACP 弹匣"};
        d.base_descr = {
                "柯尔特手枪使用的弹药。"};
        d.ranged.max_ammo = g_data[(size_t)Id::pistol].ranged.max_ammo;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::rifle;
        d.base_name = {
                "温彻斯特步枪", "温彻斯特步枪队", "温彻斯特步枪"};
        d.base_descr = {
                "杠杆式连发步枪。",

                "该武器在近距离时精度会受到影响。"};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::rifle;
        // d.color = colors::dark_brown();
        d.ranged.max_ammo = 7;
        d.ranged.dmg = WpnDmg(10, 16);
        d.ranged.hit_chance_mod = 15;
        d.ranged.effective_range = {4, 8};
        d.ranged.ammo_item_id = Id::rifle_bullet;
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到步枪开火的声音。";
        d.ranged.attack_sfx = audio::SfxId::rifle_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::rifle_revolver_reload;
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo);
        d.id = Id::rifle_bullet;
        d.base_name = {
                "温彻斯特 .30 子弹",
                "温彻斯特 .30 子弹",
                "温彻斯特 .30 子弹"};
        d.base_descr = {
                "温彻斯特步枪使用的弹药。"};
        d.color = colors::dark_yellow();
        d.max_stack_at_spawn = 10;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::spike_gun;
        d.base_name = {"长钉枪", "长钉枪", "长钉枪"};
        d.base_descr = {
                "一种非常奇怪和粗糙的武器，能够发射铁钉，其力量足以刺穿肉体（甚至岩石）。它似乎是为了残酷而故意设计的，而不是纯粹的制止力量。"};
        d.weight = (Weight::medium * 3) / 4;
        d.tile = gfx::TileId::tommy_gun;
        d.color = colors::dark_brown();
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.max_ammo = 12;
        d.ranged.dmg = WpnDmg(1, 7);
        d.ranged.hit_chance_mod = 0;
        d.ranged.effective_range = {0, 4};
        d.ranged.dmg_type = DmgType::piercing;
        d.ranged.knocks_back = true;
        d.ranged.ammo_item_id = Id::iron_spike;
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到一种非常简陋的武器正在开火。";
        d.ranged.makes_ricochet_snd = true;
        d.ranged.projectile_color = colors::gray();
        d.spawn_std_range.min = 4;
        d.ranged.attack_sfx = audio::SfxId::spike_gun;
        d.ranged.snd_vol = SndVol::low;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::electric_gun;
        d.base_name = {
                "电动枪", "电动枪", "电动枪"};
        d.base_descr = {
                "Mi-Go 创造的武器。它会发射毁灭性的闪电。",

                "当缺乏 Mi-Go 所使用的特殊能量源的生物使用时，这种武器会从使用者的生命力中汲取能量（" +
                        s_electric_gun_hp_drained_str +
                        " 每次攻击都会消耗生命值，被动生命值再生会被禁用 " +
                        s_electric_gun_hp_disable_range_str +
                        " 轮）。"};
        d.spawn_std_range = Range(-1, -1);
        d.weight = Weight::medium;
        d.tile = gfx::TileId::electric_gun;
        d.color = colors::yellow();
        d.ranged.dmg = WpnDmg(8, 12);
        d.ranged.hit_chance_mod = 5;
        d.ranged.effective_range = {0, 4};
        {
                prop::Prop* prop = prop::make(prop::Id::paralyzed);

                prop->set_duration(2);

                d.ranged.prop_applied = ItemAttackProp(prop);
        }
        d.ranged.dmg_type = DmgType::electric;
        d.ranged.has_infinite_ammo = true;
        d.ranged.projectile_leaves_trail = true;
        d.ranged.projectile_color = colors::yellow();
        d.melee.attack_msgs = {"击中", "击中"};
        d.ranged.attack_msgs = {"开火", "射击"};
        d.ranged.snd_msg = "我听到一道闪电声。";
        d.ranged.attack_sfx = audio::SfxId::electric_gun;
        d.ranged.makes_ricochet_snd = false;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::trap_dart;
        d.allow_spawn = false;
        d.ranged.has_infinite_ammo = true;
        d.ranged.dmg = WpnDmg(1, 8);
        d.ranged.hit_chance_mod = 70;
        d.ranged.effective_range = {0, 6};
        d.ranged.snd_msg = "我听到射弹发射的声音。";
        // TODO: Make a sound effect for this
        d.ranged.attack_sfx = audio::SfxId::END;
        d.ranged.makes_ricochet_snd = true;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d = g_data[(size_t)Id::trap_dart];
        d.id = Id::trap_dart_poison;
        d.ranged.prop_applied = ItemAttackProp(prop::make(prop::Id::poisoned));
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::trap_spear;
        d.allow_spawn = false;
        d.weight = Weight::heavy;
        d.melee.dmg = WpnDmg(6, 8);
        d.melee.hit_chance_mod = 85;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d = g_data[(size_t)Id::trap_spear];
        d.id = Id::trap_spear_poison;
        d.ranged.prop_applied = ItemAttackProp(prop::make(prop::Id::poisoned));
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::explosive);
        d.id = Id::dynamite;
        d.base_name = {"炸药", "炸药棒", "一根炸药棒"};
        d.base_descr = {
                "一种基于硝化甘油的爆炸材料。这个名字来源于古希腊语中的“力量”一词。"};
        d.weight = Weight::light;
        d.tile = gfx::TileId::dynamite;
        d.color = colors::light_red();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::explosive);
        d.id = Id::flare;
        d.base_name = {"耀斑", "照明弹", "耀斑"};
        d.base_descr = {
                "一种烟火，能产生明亮的光或强烈的热量而不爆炸。"};
        d.weight = Weight::light;
        d.tile = gfx::TileId::flare;
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::explosive);
        d.id = Id::molotov;
        d.base_name = {
                "燃烧弹鸡尾酒", "燃烧弹鸡尾酒", "燃烧弹鸡尾酒"};
        d.base_descr = {
                "一种简易燃烧武器，由装有易燃液体的玻璃瓶和一些点火布制成。在行动中，布被点燃，瓶子被扔向目标，立即引起火球，随后引发熊熊大火。"};
        d.weight = Weight::light;
        d.tile = gfx::TileId::molotov;
        d.color = colors::white();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::explosive);
        d.id = Id::smoke_grenade;
        d.base_name = {"烟雾弹", "烟雾弹", "烟雾弹"};
        d.base_descr = {
                "带有发射孔的钢板圆筒，当手榴弹被点燃时会释放烟雾。它们的主要用途是制造烟幕以进行隐蔽。产生的烟雾会伤害眼睛、喉咙和肺部——因此建议佩戴防护面罩。"};
        d.weight = Weight::light;
        d.tile = gfx::TileId::flare;
        d.color = colors::green();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::throwing_wpn);
        d.id = Id::thr_knife;
        d.base_name = {"飞刀", "飞刀", "一把飞刀"};
        d.base_descr = {
                "一把经过专门设计和加重的刀，可以有效地投掷。"};
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::dagger;
        d.character = '/';
        d.color = colors::white();
        d.ranged.dmg = WpnDmg(2, 6);
        d.ranged.throw_hit_chance_mod = 10;
        d.ranged.effective_range = {0, 4};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.max_stack_at_spawn = 6;
        d.land_on_hard_snd_msg = "我听到叮叮当当的声音。";
        d.land_on_hard_sfx = audio::SfxId::metal_clank;
        d.main_attack_mode = AttackMode::thrown;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::throwing_wpn);
        d.id = Id::rock;
        d.base_name = {"岩石", "岩石", "一块岩石"};
        d.base_descr = {
                "虽然不是一种令人印象深刻的武器，但只要有技巧，它们的使用就能产生一些效果。"};
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::rock;
        d.character = '*';
        d.color = colors::gray();
        d.ranged.dmg = WpnDmg(1, 3);
        d.ranged.effective_range = {0, 3};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.ranged.dmg_type = DmgType::blunt;
        d.max_stack_at_spawn = 3;
        d.main_attack_mode = AttackMode::thrown;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::dagger;
        d.base_name = {"匕首", "匕首", "一把匕首"};
        d.base_descr = {
                "通常与欺骗、秘密行动和背叛有关。许多暗杀都是使用匕首进行的。",

                "使用匕首对不知情的对手进行近战攻击会产生 +200% d 法力（除了隐形攻击的正常 +50% d 法力之外）。",

                "用匕首进行的近战攻击是无声的。"};
        d.weight = Weight::light;
        d.tile = gfx::TileId::dagger;
        d.melee.attack_msgs = {"刺击", "刺伤"};
        d.melee.dmg = WpnDmg(2, 4);
        d.melee.hit_chance_mod = 20;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.is_noisy = false;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_hard_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_light;
        d.ranged.dmg_type = DmgType::piercing;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::hatchet;
        d.base_name = {"斧头", "斧头", "斧头"};
        d.base_descr = {
                "短柄小斧头。斧头是可靠的武器 - 它们易于使用，并且重量轻，可造成相当大的伤害。",

                "使用斧头的近战攻击是无声的。"};
        d.weight = Weight::light;
        d.tile = gfx::TileId::axe;
        d.melee.attack_msgs = {"击中", "击中"};
        d.melee.dmg = WpnDmg(1, 6);
        d.melee.hit_chance_mod = 15;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.is_noisy = false;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_hard_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_light;
        d.ranged.throw_hit_chance_mod = -10;
        d.ranged.effective_range = {0, 4};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.ranged.dmg_type = DmgType::slashing;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::club;
        d.base_name = {"俱乐部", "俱乐部", "一个俱乐部"};
        d.base_descr = {
                "自史前时代起就被使用。",

                "用棍棒进行近战攻击是无声的。"};
        d.spawn_std_range = Range(g_dlvl_first_mid_game, g_dlvl_last);
        d.weight = Weight::medium;
        d.tile = gfx::TileId::club;
        d.color = colors::brown();
        d.melee.attack_msgs = {"击中", "击中"};
        d.melee.dmg = WpnDmg(3, 6);
        d.melee.hit_chance_mod = 15;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::blunt;
        d.melee.is_noisy = false;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.ranged.throw_hit_chance_mod = -10;
        d.ranged.effective_range = {0, 4};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.ranged.dmg_type = DmgType::blunt;
        d.land_on_hard_snd_msg = "我听到一声重击的声音。";
        d.land_on_hard_sfx = audio::SfxId::END;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::hammer;
        d.base_name = {"锤", "锤子", "一把锤子"};
        d.base_descr = {
                "通常用于建筑，但作为武器使用时可能具有相当大的破坏性。",

                "用锤子进行近战攻击会产生噪音。"};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::hammer;
        d.melee.attack_msgs = {"重击", "重击"};
        d.melee.dmg = WpnDmg(4, 7);
        d.melee.hit_chance_mod = 10;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::blunt;
        d.melee.is_noisy = true;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.ranged.dmg_type = DmgType::blunt;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::machete;
        d.base_name = {"弯刀", "砍刀", "砍刀"};
        d.base_descr = {
                "一把像切肉刀一样的大刀。它既可以用作切割工具，也可以用作武器。",

                "用砍刀进行近战攻击会产生噪音。"};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::machete;
        d.melee.attack_msgs = {"劈", "排骨"};
        d.melee.dmg = WpnDmg(3, 9);
        d.melee.hit_chance_mod = 5;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.melee.is_noisy = true;
        d.ranged.dmg_type = DmgType::slashing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::axe;
        d.base_name = {"斧头", "轴", "一把斧头"};
        d.base_descr = {
                "一种用于砍伐树木、劈木等的工具。用作武器时，它可以造成毁灭性的打击，但需要一定的技巧才能有效使用。",

                "用斧头进行近战攻击会很吵。"};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::axe;
        d.melee.attack_msgs = {"击中", "击中"};
        d.melee.dmg = WpnDmg(3, 12);
        d.melee.hit_chance_mod = 0;
        d.melee.can_attack_corpse = true;
        d.melee.can_attack_door_wood = true;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.melee.is_noisy = true;
        d.ranged.dmg_type = DmgType::slashing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::spiked_mace;
        d.base_name = {"尖刺狼牙棒", "尖刺狼牙棒", "尖刺狼牙棒"};
        d.base_descr = {
                "一种残酷的武器，结合了钝器和穿刺。",

                "使用此武器的攻击有 25% c 几率使受害者昏迷，使其短时间内无法行动。",

                "使用带刺狼牙棒的近战攻击会产生噪音。"};
        d.weight = Weight::moderately_heavy;
        d.tile = gfx::TileId::spiked_mace;
        d.melee.attack_msgs = {"击中", "击中"};
        d.melee.dmg = WpnDmg(1, 14);
        d.melee.hit_chance_mod = -10;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        d.melee.is_noisy = true;
        {
                prop::Prop* prop = prop::make(prop::Id::paralyzed);

                prop->set_duration(2);

                d.melee.prop_applied.prop.reset(prop);
                d.melee.prop_applied.pct_chance_to_apply = 25;
        }
        d.ranged.dmg_type = DmgType::piercing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::pitchfork;
        d.base_name = {"叉", "干草叉", "干草叉"};
        d.base_descr = {
                "一根长棍，末端有叉状四爪。",

                "干草叉对于阻止攻击者很有用——受害者被刺伤后会被推回原处。"};
        d.weight = Weight::moderately_heavy;
        d.tile = gfx::TileId::pitchfork;
        d.melee.attack_msgs = {"击中", "击中"};
        d.melee.dmg = WpnDmg(1, 7);
        d.melee.hit_chance_mod = -10;
        d.melee.can_attack_corpse = true;
        d.melee.reach = 2;
        d.melee.knocks_back = true;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.is_noisy = true;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        d.ranged.dmg_type = DmgType::piercing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::spear;
        d.base_name = {"矛", "斯皮尔斯", "矛"};
        d.base_descr = {
                "一种由木柄和钢头组成的长杆武器。"};
        d.weight = Weight::moderately_heavy;
        d.tile = gfx::TileId::spear;
        d.color = colors::brown();
        d.melee.attack_msgs = {"击中", "击中"};
        d.melee.dmg = WpnDmg(1, 9);
        d.melee.hit_chance_mod = 0;
        d.melee.can_attack_corpse = true;
        d.melee.reach = 2;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.is_noisy = true;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        d.ranged.dmg_type = DmgType::piercing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::sledgehammer;
        d.base_name = {"大锤", "大锤", "大锤"};
        d.base_descr = {
                "它可以造成毁灭性的伤害，尽管携带起来很麻烦，并且需要一些技巧才能有效使用。"};
        d.weight = Weight::heavy;
        d.tile = gfx::TileId::sledgehammer;
        d.melee.attack_msgs = {"重击", "重击"};
        d.melee.dmg = WpnDmg(4, 15);
        d.melee.hit_chance_mod = -10;
        d.melee.can_attack_corpse = true;
        d.melee.can_attack_door_wood = true;
        d.melee.can_attack_door_gate = true;
        d.melee.dmg_type = DmgType::blunt;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        d.ranged.dmg_type = DmgType::blunt;
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::throwing_wpn);
        d.id = Id::iron_spike;
        d.base_name = {"铁钉", "铁钉", "铁钉"};
        d.base_descr = {
                "对于楔入封闭的物体很有用。"};
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::iron_spike;
        d.is_stackable = true;
        d.color = colors::gray();
        d.character = '/';
        d.ranged.throw_hit_chance_mod = -5;
        d.ranged.dmg = WpnDmg(1, 4);
        d.ranged.effective_range = {0, 3};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.max_stack_at_spawn = 12;
        d.land_on_hard_snd_msg = "我听到叮叮当当的声音。";
        d.land_on_hard_sfx = audio::SfxId::metal_clank;
        d.main_attack_mode = AttackMode::thrown;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::player_kick;
        d.melee.attack_msgs = {"踢", ""};
        d.melee.hit_chance_mod = 15;
        d.melee.dmg = WpnDmg(1, 2);
        d.melee.knocks_back = true;
        d.melee.dmg_type = DmgType::kicking;
        d.melee.can_attack_door_wood = true;
        d.melee.can_attack_door_gate = true;
        d.melee.can_attack_corpse = true;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::player_stomp;
        d.melee.attack_msgs = {"跺脚", ""};
        d.melee.hit_chance_mod =
                g_data[(size_t)Id::player_kick].melee.hit_chance_mod;
        d.melee.dmg =
                g_data[(size_t)Id::player_kick].melee.dmg;
        d.melee.miss_sfx =
                g_data[(size_t)Id::player_kick].melee.miss_sfx;
        d.melee.dmg_type = DmgType::kicking;
        d.melee.knocks_back = false;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::player_punch;
        d.base_name = {"拳头", "", "一拳"};
        d.melee.attack_msgs = {"出拳", ""};
        d.melee.hit_chance_mod = 20;
        d.melee.dmg = WpnDmg(1, 1);
        d.melee.miss_sfx = audio::SfxId::miss_light;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::player_ghoul_claw;
        d.base_name = {"爪", "", "抓挠"};
        d.melee.attack_msgs = {"抓挠", ""};
        d.melee.hit_chance_mod = 20;
        d.melee.dmg = WpnDmg(1, 8);
        d.melee.is_noisy = false;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_kick;
        d.melee.attack_msgs = {"", "踢"};
        d.melee.dmg_type = DmgType::blunt;
        d.melee.knocks_back = true;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_bite;
        d.melee.attack_msgs = {"", "咬伤"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_claw;
        d.melee.attack_msgs = {"", "抓挠"};
        d.melee.dmg_type = DmgType::slashing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_strike;
        d.melee.attack_msgs = {"", "击中"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_punch;
        d.melee.attack_msgs = {"", "拳打"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_punch_knockback;
        d.melee.attack_msgs = {"", "拳打"};
        d.melee.dmg_type = DmgType::blunt;
        d.melee.knocks_back = true;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_headbutt;
        d.melee.attack_msgs = {"", "猛烈地撞入"};
        d.melee.dmg_type = DmgType::blunt;
        d.melee.knocks_back = true;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_putrid_spit;
        d.ranged.attack_msgs = {"", "吐脓"};
        d.ranged.snd_msg = "我听到吐口水的声音。";
        d.ranged.projectile_color = colors::light_green();
        d.ranged.dmg_type = DmgType::blunt;
        d.ranged.projectile_character = '*';
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_snake_venom_spit;
        d.ranged.attack_msgs = {"", "吐毒液"};
        d.ranged.snd_msg = "我听到嘶嘶声和吐口水的声音。";
        d.ranged.projectile_color = colors::light_green();
        d.ranged.dmg_type = DmgType::piercing;
        d.ranged.projectile_character = '*';
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_earth_breath;
        d.ranged.attack_msgs = {"", "呼吸出巨大的密度"};
        d.ranged.snd_msg = "我听到敲击声。";
        d.ranged.attack_sfx = audio::SfxId::earth_breath;
        d.ranged.projectile_color = colors::brown();
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_leaves_trail = false;
        d.ranged.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_water_breath;
        d.ranged.attack_msgs = {"", "喷吐着汹涌的洪流"};
        d.ranged.snd_msg = "我听到涌出的声音。";
        d.ranged.attack_sfx = audio::SfxId::water_breath;
        d.ranged.projectile_color = colors::light_blue();
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_leaves_trail = true;
        d.ranged.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_fire_breath;
        d.ranged.attack_msgs = {"", "喷火"};
        d.ranged.snd_msg = "我听到一阵火焰的声音。";
        d.ranged.attack_sfx = audio::SfxId::fire_breath;
        d.ranged.projectile_color = colors::light_red();
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_leaves_trail = true;
        d.ranged.dmg_type = DmgType::fire;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_lightning_breath;
        d.ranged.attack_msgs = {"", "呼吸闪电"};
        d.ranged.snd_msg = "我听到一阵闪电。";
        d.ranged.attack_sfx = audio::SfxId::lightning_breath;
        d.ranged.projectile_color = colors::yellow();
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_leaves_trail = true;
        d.ranged.dmg_type = DmgType::electric;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_raven_peck;
        d.melee.attack_msgs = {"", "啄"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_vampiric_bite;
        d.melee.attack_msgs = {"", "咬伤"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_strangle;
        d.melee.attack_msgs = {"", "勒死"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_ghost_touch;
        d.melee.attack_msgs = {"", "达到"};
        d.melee.dmg_type = DmgType::spirit;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_sting;
        d.melee.attack_msgs = {"", "蜇伤"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_mind_leech_sting;
        d.melee.attack_msgs = {"", "蜇伤"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_spear_thrust;
        d.melee.attack_msgs = {"", "击中"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_net_throw;
        d.ranged.attack_msgs = {"", "撒网"};
        d.ranged.snd_msg = "我听到嘶嘶的声音。";
        d.ranged.projectile_color = colors::brown();
        d.ranged.dmg_type = DmgType::blunt;
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::web;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_maul;
        d.melee.attack_msgs = {"", "大槌"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_pus_spew;
        d.melee.attack_msgs = {"", "喷出脓液"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_strange_color_touch;
        d.melee.attack_msgs = {"", "触摸"};
        d.melee.dmg_type = DmgType::pure;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_dust_engulf;
        d.melee.attack_msgs = {"", "吞没"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_fire_engulf;
        d.melee.attack_msgs = {"", "吞没"};
        d.melee.dmg_type = DmgType::fire;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_energy_engulf;
        d.melee.attack_msgs = {"", "吞没"};
        d.melee.dmg_type = DmgType::electric;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_spores;
        d.melee.attack_msgs = {"", "释放孢子"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_web_bola;
        d.ranged.attack_msgs = {"", "射出网状波拉"};
        d.ranged.snd_msg = "";
        d.ranged.projectile_color = colors::light_white();
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_character = '*';
        d.ranged.dmg_type = DmgType::blunt;
        d.ranged.snd_vol = SndVol::low;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_leather_jacket;
        d.base_name = {"皮夹克", "", "一件皮夹克"};
        d.base_descr = {
                "它提供了一些保护。"};
        d.weight = Weight::light;
        d.color = colors::brown();
        d.spawn_std_range.min = 1;
        d.armor.armor_points = 1;
        d.armor.dmg_to_durability_factor = 1.0;
        d.land_on_hard_snd_msg = "";
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_heavy_coat;
        d.base_name = {"厚外套", "", "厚外套"};
        d.base_descr = {
                "它提供了不错的防护，但穿戴后行动会稍显迟缓（潜行 -5%，闪避 -5%）。"};
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -5;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -5;
        d.weight = Weight::medium;
        d.color = colors::light_blue();
        d.spawn_std_range.min = 1;
        d.armor.armor_points = 2;
        d.armor.dmg_to_durability_factor = 1.0;
        d.land_on_hard_snd_msg = "";
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_iron_suit;
        d.base_name = {"铁套装", "", "一套铁衣"};
        d.base_descr = {
                "由金属板、螺栓和皮带制成的粗糙盔甲。",

                "它能吸收大量伤害，但会使移动更加困难（潜行 -20%，闪避 -20%）。"};
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -20;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -20;
        d.weight = Weight::extra_heavy;
        d.color = colors::white();
        d.spawn_std_range.min = 2;
        d.armor.armor_points = 5;
        d.armor.dmg_to_durability_factor = 0.3;
        d.land_on_hard_snd_msg = "我听到一声撞击声。";
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_flak_jacket;
        d.base_name = {"防弹夹克", "", "防弹衣"};
        d.base_descr = {
                "由缝在背心中的钢板组成的盔甲。",

                "以其重量而言，它提供的防护相当出色，但穿戴起来略显笨重（潜行 -10%，闪避 -10%）。"};
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -10;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -10;
        d.weight = Weight::medium;
        d.color = colors::green();
        d.spawn_std_range.min = 3;
        d.armor.armor_points = 3;
        d.armor.dmg_to_durability_factor = 0.5;
        d.land_on_hard_snd_msg = "我听到一声重击的声音。";
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_asb_suit;
        d.base_name = {"石棉套装", "", "石棉套装"};
        d.base_descr = {
                "一件式石棉织物，包括头罩、炉面罩、手套和鞋子。",

                "它可以保护佩戴者免受火、电、烟雾和气体的侵害。",

                "穿戴起来有些笨重（潜行 -10%，闪避 -10%）。"};
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -10;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -10;
        d.weight = Weight::medium;
        d.color = colors::light_red();
        d.spawn_std_range.min = 3;
        d.armor.armor_points = 1;
        d.armor.dmg_to_durability_factor = 1.0;
        d.land_on_hard_snd_msg = "";
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::chest);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_mi_go;
        d.base_name = {"Mi-Go生物装甲", "", "Mi-Go 生物装甲"};
        d.base_descr = {
                "Mi-Go 打造的极其耐用的生物装甲。"};
        d.spawn_std_range = Range(-1, -1);
        d.weight = Weight::medium;
        d.color = colors::magenta();
        d.tile = gfx::TileId::mi_go_armor;
        d.armor.armor_points = 3;
        d.armor.dmg_to_durability_factor = 0.1;
        d.land_on_hard_snd_msg = "";
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::head_wear);
        d.id = Id::gas_mask;
        d.base_name = {"防毒面具", "", "防毒面具"};
        d.base_descr = {
                "保护眼睛、喉咙和肺部免受烟雾和烟雾的侵害。它的使用寿命有限，与过滤器的吸收能力有关。 ",

                "由于眼睛窗口较小，瞄准稍微困难一些，并且更难发现潜行的敌人和隐藏的物体（-10% 近战和远程命中几率，-6% s 搜索）。"};
        d.ability_mods_while_equipped[(size_t)AbilityId::melee] = -10;
        d.ability_mods_while_equipped[(size_t)AbilityId::ranged] = -10;
        d.ability_mods_while_equipped[(size_t)AbilityId::searching] = -6;
        d.is_stackable = false;
        d.color = colors::brown();
        d.tile = gfx::TileId::gas_mask;
        d.character = '[';
        d.spawn_std_range = Range(1, g_dlvl_last_early_game);
        d.chance_to_incl_in_spawn_list = 50;
        d.weight = Weight::light;
        d.land_on_hard_snd_msg = "";
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::head_wear);
        d.id = Id::torture_collar;
        d.base_name = {"酷刑项圈", "", "酷刑项圈"};
        d.base_descr = {
                "一种可怕的酷刑装置，其尖刺刺入佩戴者的颈部。起飞是不可能的。",

                "佩戴项圈行走需要额外回合，潜行和闪避各降低 20%。不过，项圈能让鞭笞者更能忍受肉体痛苦，护甲值增加 3 点。"};
        d.is_stackable = false;
        d.color = colors::red();
        d.tile = gfx::TileId::torture_collar;
        d.character = '[';
        d.weight = Weight::light;
        d.is_unique = true;
        d.value = Value::supreme_treasure;
        d.chance_to_incl_in_spawn_list = 0;
        d.allow_spawn = false;
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -20;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -20;
        d.armor.armor_points = 3;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_aura_of_decay;
        d.spell_cast_from_scroll = SpellId::aura_of_decay;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_cataclysm;
        d.spell_cast_from_scroll = SpellId::cataclysm;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_telep;
        d.spell_cast_from_scroll = SpellId::teleport;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_pestilence;
        d.spell_cast_from_scroll = SpellId::pestilence;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_enfeeble;
        d.spell_cast_from_scroll = SpellId::enfeeble;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_slow;
        d.spell_cast_from_scroll = SpellId::slow;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_terrify;
        d.spell_cast_from_scroll = SpellId::terrify;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_bless;
        d.spell_cast_from_scroll = SpellId::bless;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_darkbolt;
        d.spell_cast_from_scroll = SpellId::darkbolt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_aza_gaze;
        d.spell_cast_from_scroll = SpellId::aza_gaze;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_control_object;
        d.spell_cast_from_scroll = SpellId::control_object;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_light;
        d.spell_cast_from_scroll = SpellId::light;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_spectral_wpns;
        d.spell_cast_from_scroll = SpellId::spectral_weapons;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_transmut;
        d.spell_cast_from_scroll = SpellId::transmut;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_thorns;
        d.spell_cast_from_scroll = SpellId::thorns;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_blood_temper;
        d.spell_cast_from_scroll = SpellId::blood_tempering;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_sacrifice_life;
        d.spell_cast_from_scroll = SpellId::sacrifice_life;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_crimson_passage;
        d.spell_cast_from_scroll = SpellId::crimson_passage;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_heal;
        d.spell_cast_from_scroll = SpellId::heal;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_invis;
        d.spell_cast_from_scroll = SpellId::invis;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_see_invis;
        d.spell_cast_from_scroll = SpellId::see_invis;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_premonition;
        d.spell_cast_from_scroll = SpellId::premonition;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_erudition;
        d.spell_cast_from_scroll = SpellId::erudition;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_haste;
        d.spell_cast_from_scroll = SpellId::haste;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_spell_shield;
        d.spell_cast_from_scroll = SpellId::spell_shield;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_skill;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_carapace;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_blinking;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_burrowing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_vitality;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_spirit;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_blindness;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_fortitude;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_paralyze;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_conf;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_poison;
        mod_spawn_chance(d, 0.66);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_insight;
        d.chance_to_incl_in_spawn_list = 100;
        d.spawn_std_range.max = g_dlvl_last_mid_game;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_resistance;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_curing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_descent;
        mod_spawn_chance(d, 0.15);
        d.spawn_std_range.max = g_dlvl_last_mid_game;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_blaster;
        d.base_name = {"冲击波装置", "冲击波装置", "冲击波装置"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_rejuvenator;
        d.base_name = {
                "回春仪",
                "回春设备",
                "回春装置"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_translocator;
        d.base_name = {
                "转运装置",
                "转运装置",
                "转运装置"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_sentry_drone;
        d.base_name = {
                "哨兵无人机设备",
                "哨兵无人机设备",
                "哨兵无人机设备"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_force_field;
        d.base_name = {
                "力场装置",
                "力场设备",
                "力场装置"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_cloud_minds;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_deafening;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_displacement;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_door_creation;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_mi_go_hypno;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_mist;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_opening;
        d.spawn_std_range.max = g_dlvl_first_mid_game;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_shockwave;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_unbinding;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::medical_bag;
        d.has_std_activate = true;
        d.is_prio_in_backpack_list = true;
        d.base_name = {"医疗袋", "医疗袋", "医疗包"};
        d.base_descr = {
                "一个装有医疗用品的便携式袋子。可用于治疗创伤或感染。"};
        d.weight = Weight::medium;
        d.spawn_std_range = Range(1, g_dlvl_last_mid_game);
        d.is_stackable = false;
        d.character = '%';
        d.color = colors::dark_brown();
        d.tile = gfx::TileId::medical_bag;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::lantern;
        d.has_std_activate = true;
        d.is_prio_in_backpack_list = true;
        d.base_name = {
                "电灯笼", "电灯笼", "电灯笼"};
        d.base_descr = {
                "便携式光源。"};
        d.weight = item::Weight::light;
        d.character = '%';
        d.spawn_std_range = Range(1, 10);
        d.is_stackable = false;
        d.chance_to_incl_in_spawn_list = 100;
        d.tile = gfx::TileId::lantern;
        d.color = colors::yellow();
        d.land_on_hard_snd_msg = "我听到叮叮当当的声音。";
        d.land_on_hard_sfx = audio::SfxId::metal_clank;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::pharaoh_staff;
        d.base_name = {
                "法老王的权杖",
                "",
                "法老王的权杖"};
        d.base_descr = {
                "这件强大的神器由古代统治者所使用，对那些曾经受其束缚的人拥有力量。任何看到主人的木乃伊最终都会被转化（携带武器时每回合 10% c 机会）。",

                "另外，被该武器击中的人可能会受到毁灭性的诅咒（50% c几率施加厄运，大大降低受害者的命中率、闪避率和搜索能力，并且还有小概率施法失败）。"};
        d.color = colors::magenta();
        d.weight = Weight::medium;
        d.tile = gfx::TileId::pharaoh_staff;
        d.melee.attack_msgs = {"击中", "击中"};
        d.melee.dmg = WpnDmg(1, 12);
        d.melee.hit_chance_mod = 0;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.melee.dmg_type = DmgType::blunt;
        d.melee.reach = 2;
        d.ranged.dmg_type = DmgType::blunt;
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::flagellant_whip;
        d.base_name = {
                "天灾",
                "",
                "天灾"};
        d.base_descr = {
                "一种残酷的鞭子，上面附有锋利的骨头和金属尖刺。",

                "被它撕裂血肉的咬击击中后，受害者可能会因疼痛而瘫痪（20% 几率）。"};
        d.color = colors::red();
        d.weight = Weight::light;
        d.tile = gfx::TileId::whip_scourge;
        d.melee.attack_msgs = {"击中", "击中"};
        d.melee.dmg = WpnDmg(1, 10);
        d.melee.hit_chance_mod = 10;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.melee.hit_small_sfx = audio::SfxId::hit_whip_scourge;
        d.melee.hit_medium_sfx = audio::SfxId::hit_whip_scourge;
        d.melee.hit_hard_sfx = audio::SfxId::hit_whip_scourge;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.reach = 2;
        {
                prop::Prop* prop = prop::make(prop::Id::paralyzed);

                prop->set_duration(2);

                d.melee.prop_applied.prop.reset(prop);
                d.melee.prop_applied.pct_chance_to_apply = 20;
        }
        d.ranged.is_throwable_wpn = false;
        d.is_unique = true;
        d.value = Value::supreme_treasure;
        d.chance_to_incl_in_spawn_list = 0;
        d.allow_spawn = false;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::onyx_drop;
        d.base_name = {
                "玛瑙掉落",
                "",
                "玛瑙掉落"};
        d.base_descr = {
                "饮用邪恶药水也会对附近所有生物施加效果（最大距离为 6）。"};
        d.color = colors::violet();
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::drop;
        d.character = '*';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::refl_talisman;
        d.base_name = {
                "反射护符",
                "",
                "反思护身符"};
        d.base_descr = {
                "每当敌对法术因法术抗力而被阻挡时，它也会被反射。恢复法术抗力的回合数减半。"};
        d.color = colors::light_blue();
        d.weight = Weight::light;
        d.tile = gfx::TileId::amulet;
        d.character = '"';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::resurrect_talisman;
        d.base_name = {
                "复活护身符",
                "",
                "复活护符"};
        d.base_descr = {
                "这种强大的魅力可以使主人在身体死亡后复活。然而，护身符在此过程中被破坏，因此只能被带回来一次。"};
        d.color = colors::light_white();
        d.weight = Weight::light;
        d.tile = gfx::TileId::amulet;
        d.character = '"';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::tele_ctrl_talisman;
        d.base_name = {
                "传送控制符",
                "",
                "传送控制护身符"};
        d.base_descr = {
                "赋予所有者传送时控制目的地的能力。"};
        d.color = colors::orange();
        d.weight = Weight::light;
        d.tile = gfx::TileId::amulet;
        d.character = '"';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::holy_symbol;
        d.base_name = {
                "圣徽",
                "",
                "神圣的象征",
        };
        d.base_descr = {
                "为精神和心智提供力量与指引的焦点。向此象征祈祷可获得 1–4 点精神值，并在 6–12 回合内抵抗震慑与恐惧。",

                "祈祷必须经过一段时间才能保证再次产生效果，但是可以在这段时间过去之前尝试（有 25% c 成功的机会）。如果早期尝试失败，人们就会暂时失去对该符号的信心，并且必须经过很长时间才能再次使用该符号。"};
        d.color = colors::gold();
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::holy_symbol;
        d.character = '%';
        d.is_unique = true;
        d.value = Value::supreme_treasure;
        d.has_std_activate = true;
        d.chance_to_incl_in_spawn_list = 0;
        d.allow_spawn = false;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::clockwork;
        d.base_name = {
                "奥术发条",
                "",
                "奥术发条装置",
        };
        d.base_descr = {
                "一个主发条驱动的发条装置，具有不可思议的品质和美丽。当上紧发条时，它会导致所有者在短时间内快速移动。"};
        d.color = colors::yellow();
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::clockwork;
        d.character = '%';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.has_std_activate = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::chest);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::horn_of_malice;
        d.base_name = {
                "恶意号角",
                "",
                "恶意号角"};
        d.base_descr = {
                "当吹响时，这件邪恶的神器会发出一种奇怪的共鸣，这种共鸣会腐蚀所有在听觉范围内的人的心灵（不包括吹号者）——导致他们对所有其他生物抱有强烈的仇恨和不信任。"};
        d.color = colors::gray();
        d.weight = Weight::light;
        d.tile = gfx::TileId::horn;
        d.character = '%';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.has_std_activate = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::horn_of_banishment;
        d.base_name = {
                "放逐号角",
                "",
                "放逐的号角"};
        d.base_descr = {
                "当吹响时，这个乐器会迫使所有在听觉范围内的魔法召唤生物回到原来的领域。"};
        d.color = colors::magenta();
        d.weight = Weight::light;
        d.tile = gfx::TileId::horn;
        d.character = '%';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.has_std_activate = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::shadow_dagger;
        d.base_name = {
                "黑匕首加哈纳",
                "",
                "黑匕首加哈纳"};
        d.base_descr = {
                "一把漆黑的匕首，上面有精致的装饰。剑刃看起来模糊不清，仿佛永远笼罩在黑暗的薄雾之中。",

                "被这种武器击中的生物会被诅咒永远生活在黑暗中，或者遭受巨大的痛苦（变得永久对光敏感，并受到 +1 额外的光伤害）。自然发光的生物（例如火焰生物或能量生物）会受到 1-4 点不可抗拒的伤害。",

                "用匕首攻击不知情的对手会产生+200% d法力（除了隐形攻击的正常+50% d法力之外）。",
        };
        d.weight = Weight::light;
        d.tile = gfx::TileId::dagger;
        d.color = colors::violet();
        d.melee.attack_msgs = {"刺击", "刺伤"};
        d.melee.dmg = WpnDmg(4, 8);
        d.melee.hit_chance_mod = 20;
        d.melee.is_noisy = false;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_hard_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_light;
        d.melee.dmg_type = DmgType::piercing;
        d.ranged.dmg_type = DmgType::piercing;
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::orb_of_life;
        d.base_name = {
                "生命宝珠",
                "",
                "生命之球"};
        d.base_descr = {
                "+4 生命值，赋予对毒药和疾病的抵抗力。"};
        d.color = colors::light_white();
        d.weight = Weight::light;
        d.tile = gfx::TileId::orb;
        d.character = '"';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::necronomicon;
        d.base_name = {
                "死灵之书",
                "",
                "死灵之书"};
        d.base_descr = {
                "这就是可怕的《死灵之书》——死亡之书！它的页面包含许多关于深奥问题的可怕知识。携带时，所有法术都会以更高的技能等级施展，并且有可能达到第四级“超凡”。",

                "施法承受的震慑全部加倍，而且咨询此类知识之人的存在会被强烈感知（潜行 -20%，每回合有 2% 几率惊动附近生物）。"};
        d.color = colors::dark_sepia();
        d.weight = Weight::light;
        d.tile = gfx::TileId::tome;
        d.character = '?';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::bookshelf);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::throwing_wpn);
        d.id = Id::zombie_dust;
        d.base_name = {
                "僵尸尘埃",
                "一把僵尸灰​​尘",
                "一把僵尸尘埃"};
        d.base_descr = {
                "当向活体（非不死）生物投掷时，这种粉末会导致麻痹。"};
        d.spawn_std_range.max = g_dlvl_last;
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::zombie_dust;
        d.character = '*';
        d.color = colors::brown();
        d.ranged.dmg = WpnDmg(0, 0);
        d.ranged.throw_hit_chance_mod = 15;
        d.ranged.always_break_on_throw = true;
        d.ranged.effective_range = {-1, -1};
        d.ranged.max_range = 3;
        d.ranged.dmg_type = DmgType::blunt;
        d.max_stack_at_spawn = 1;
        d.main_attack_mode = AttackMode::thrown;
        d.chance_to_incl_in_spawn_list = 35;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::witch_eye;
        d.base_name = {"女巫之眼", "女巫的眼睛", "女巫之眼"};
        d.base_descr = {
                "强大魔女的眼睛。将其握在手中将暂时获得魔法视觉——在周围区域中检测到门、陷阱、楼梯和其他感兴趣的位置，并显示所有物品和生物的存在。"};
        d.type = ItemType::general;
        d.value = item::Value::major_treasure;
        d.weight = Weight::extra_light;
        d.has_std_activate = true;
        d.color = colors::light_green();
        d.tile = gfx::TileId::witch_eye;
        d.character = '%';
        d.max_stack_at_spawn = 1;
        d.chance_to_incl_in_spawn_list = 0;
        d.spawn_std_range = Range(-1, -1);
        d.allow_spawn = false;
        d.is_stackable = false;
        d.land_on_hard_snd_msg = "";
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::bone_charm;
        d.base_name = {"骨符", "骨头符咒", "骨咒"};
        d.base_descr = {
                "一根古老的指骨，上面刻有微小的符号。",

                "将其折成两半可以在 6-12 回合内提供针对有害法术的保护，或者直到法术被格挡为止。",

                "它还消除了所有看到的魔法陷阱（地板上的“奇怪形状”）。每消除一个陷阱，就会获得 1-6 点精神值，这可能会将精神提升到最高等级以上。"};
        d.type = ItemType::general;
        d.value = item::Value::minor_treasure;
        d.weight = Weight::extra_light;
        d.has_std_activate = true;
        d.color = colors::gray_brown();
        d.tile = gfx::TileId::bone_charm;
        d.character = '%';
        d.max_stack_at_spawn = 3;
        d.chance_to_incl_in_spawn_list = 60;
        d.is_stackable = true;
        d.land_on_hard_snd_msg = "";
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::fluctuating_material;
        d.base_name = {
                "波动材料",
                "波动材料块",
                "波动的材料"};
        d.base_descr = {
                "很难判断它是石头、金属，还是有机物。它看似千变万化，内在扭曲、转动、流动。",

                "浏览这块材料就像看万花筒——仿佛看得太深会改变观察者的本质（选择一个特质将其移除，然后选择一个新特质）。"};
        d.type = ItemType::general;
        d.value = item::Value::minor_treasure;
        d.weight = Weight::extra_light;
        d.has_std_activate = true;
        d.color = colors::yellow();
        d.tile = gfx::TileId::fluctuating_material;
        d.character = '%';
        d.max_stack_at_spawn = 1;
        d.chance_to_incl_in_spawn_list = 0;
        d.spawn_std_range = Range(-1, -1);
        d.allow_spawn = false;
        d.is_stackable = true;
        d.land_on_hard_snd_msg = "我听到砰的一声。";
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::astral_opium;
        d.base_name = {
                "星辰鸦片",
                "星体鸦片剂量",
                "星界鸦片剂量"};
        d.base_descr = {
                "从外星植物中提取的药物。它让用户处于完全平静的状态，没有任何恐惧。",

                "然而，这是有代价的，因为它会引起强烈的致幻妄想，而且还会严重上瘾。"};
        d.type = ItemType::general;
        d.is_stackable = true;
        d.chance_to_incl_in_spawn_list = 40;
        d.allow_spawn = true;
        d.max_stack_at_spawn = 2;
        d.value = item::Value::minor_treasure;
        d.weight = Weight::extra_light;
        d.has_std_activate = true;
        d.color = colors::white();
        d.tile = gfx::TileId::astral_opium;
        d.character = '%';
        d.land_on_hard_snd_msg = "我听到叮叮当当的声音。";
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        TRACE_FUNC_END;
}

void cleanup()
{
        TRACE_FUNC_BEGIN;

        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                ItemData& d = g_data[i];

                d.melee.prop_applied = ItemAttackProp();

                d.ranged.prop_applied = ItemAttackProp();
        }

        TRACE_FUNC_END;
}

void save()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                const ItemData& d = g_data[i];

                saving::put_bool(d.is_identified);
                saving::put_bool(d.is_alignment_known);
                saving::put_bool(d.is_spell_domain_known);
                saving::put_bool(d.is_tried);
                saving::put_bool(d.is_found);
                saving::put_bool(d.allow_spawn);
                saving::put_int(d.chance_to_incl_in_spawn_list);
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                ItemData& d = g_data[i];

                d.is_identified = saving::get_bool();
                d.is_alignment_known = saving::get_bool();
                d.is_spell_domain_known = saving::get_bool();
                d.is_tried = saving::get_bool();
                d.is_found = saving::get_bool();
                d.allow_spawn = saving::get_bool();
                d.chance_to_incl_in_spawn_list = saving::get_int();
        }
}

ItemSetId str_to_item_set_id(const std::string& str)
{
        return s_str_to_item_set_id_map.at(str);
}

Id str_to_intr_item_id(const std::string& str)
{
        return s_str_to_intr_item_id_map.at(str);
}

MeleeData::MeleeData() :
        is_melee_wpn(false),

        hit_chance_mod(0),
        is_noisy(true),

        dmg_type(DmgType::slashing),
        reach(1),
        knocks_back(false),
        can_attack_door_wood(false),
        can_attack_door_gate(false),
        can_attack_corpse(false),
        hit_small_sfx(audio::SfxId::END),
        hit_medium_sfx(audio::SfxId::END),
        hit_hard_sfx(audio::SfxId::END),
        miss_sfx(audio::SfxId::END)
{}

RangedData::RangedData() :
        is_ranged_wpn(false),
        is_throwable_wpn(false),
        is_machine_gun(false),
        is_shotgun(false),
        max_ammo(0),
        hit_chance_mod(0),
        throw_hit_chance_mod(0),
        always_break_on_throw(false),
        effective_range({0, 6}),
        max_range(g_fov_radi_int * 2),
        knocks_back(false),
        ammo_item_id(Id::END),
        dmg_type(DmgType::piercing),
        has_infinite_ammo(false),
        projectile_character('/'),
        projectile_tile(gfx::TileId::projectile_std_front_slash),
        projectile_color(colors::white()),
        projectile_leaves_trail(false),

        snd_vol(SndVol::low),
        makes_ricochet_snd(false),
        attack_sfx(audio::SfxId::END),
        reload_sfx(audio::SfxId::END)

{
}

ArmorData::ArmorData() :
        armor_points(0),
        dmg_to_durability_factor(0.0) {}

ItemData::ItemData() :
        id(Id::END),
        type(ItemType::general),
        is_intr(false),
        has_std_activate(false),
        is_prio_in_backpack_list(false),
        value(Value::normal),
        allow_cursed(false),
        weight(Weight::none),
        is_unique(false),
        allow_spawn(true),
        spawn_std_range(Range(1, g_dlvl_last)),
        max_stack_at_spawn(1),
        chance_to_incl_in_spawn_list(100),
        is_stackable(true),
        is_identified(true),
        is_alignment_known(true),
        is_spell_domain_known(true),
        is_tried(false),
        is_found(false),
        xp_on_found(0),
        character('X'),
        color(colors::white()),
        tile(gfx::TileId::END),
        main_attack_mode(AttackMode::none),
        spell_cast_from_scroll(SpellId::END),
        land_on_hard_snd_msg("我听到一声重击的声音。"),
        land_on_hard_sfx(audio::SfxId::END),
        allow_display_dmg(true)
{
        for (size_t i = 0; i < (size_t)AbilityId::END; ++i) {
                ability_mods_while_equipped[i] = 0;
        }

        base_descr.clear();
        native_rooms.clear();
        native_containers.clear();
}

}  // namespace item
