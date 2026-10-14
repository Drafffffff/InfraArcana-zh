#include "utf8.hpp"
// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property_data.hpp"

#include <algorithm>
#include <unordered_map>

#include "panel.hpp"
#include "property.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, prop::Id> s_str_to_prop_id_map = {
        {"PROP_ACTOR_SPECTRAL_WPN", prop::Id::spectral_wpn},
        {"PROP_ALLIES_GHOUL_PLAYER", prop::Id::allies_ghoul_player},
        {"PROP_ALTERS_ENVIRONMENT", prop::Id::alters_env},
        {"PROP_ALWAYS_AWARE", prop::Id::always_aware},
        {"腐朽光环", prop::Id::aura_of_decay},
        {"PROP_BLESSED", prop::Id::blessed},
        {"PROP_BLIND", prop::Id::blind},
        {"PROP_BREEDS", prop::Id::breeds},
        {"PROP_BREEDS_SCATTERED", prop::Id::breeds_scattered},
        {"PROP_BURNING", prop::Id::burning},
        {"PROP_BURROWING", prop::Id::burrowing},
        {"PROP_CLOAKED", prop::Id::cloaked},
        {"PROP_CONFLICT", prop::Id::conflict},
        {"PROP_CONFUSED", prop::Id::confused},
        {"PROP_CONFUSES_ADJACENT", prop::Id::confuses_adjacent},
        {"PROP_CORPSE_EATER", prop::Id::corpse_eater},
        {"PROP_CORPSE_RISES", prop::Id::corpse_rises},
        {"PROP_CORRUPTS_ENVIRONMENT_COLOR", prop::Id::corrupts_env_color},
        {"PROP_CURSED", prop::Id::cursed},
        {"PROP_DARKVISION", prop::Id::darkvision},
        {"PROP_DEAF", prop::Id::deaf},
        {"PROP_DISEASED", prop::Id::diseased},
        {"PROP_ENTANGLED", prop::Id::entangled},
        {"PROP_ERUDITION", prop::Id::erudition},
        {"PROP_ETHEREAL", prop::Id::ethereal},
        {"PROP_EXPLODES_ON_DEATH", prop::Id::explodes_on_death},
        {"PROP_EXTRA_SKILL", prop::Id::extra_skill},
        {"PROP_FAINTED", prop::Id::fainted},
        {"PROP_FLAMMABLE", prop::Id::flammable},
        {"PROP_FLYING", prop::Id::flying},
        {"PROP_FRENZIED", prop::Id::frenzied},
        {"PROP_FRENZIES_FOLLOWERS", prop::Id::frenzies_followers},
        {"PROP_FRENZIES_SELF", prop::Id::frenzies_self},
        {"PROP_FRENZY_PLAYER_ON_SEEN", prop::Id::frenzy_player_on_seen},
        {"PROP_HALLUCINATING", prop::Id::hallucinating},
        {"PROP_HASTED", prop::Id::hasted},
        {"PROP_INFECTED", prop::Id::infected},
        {"PROP_INVIS", prop::Id::invis},
        {"PROP_LIGHT_SENSITIVE", prop::Id::light_sensitive},
        {"PROP_MAGIC_CARAPACE", prop::Id::magic_carapace},
        {"PROP_MAGIC_SEARCHING", prop::Id::magic_searching},
        {"PROP_MAJOR_CLAPHAM_SUMMON", prop::Id::major_clapham_summon},
        {"PROP_MELEE_COOLDOWN", prop::Id::melee_cooldown},
        {"PROP_MOVES_THROUGH_EARTH", prop::Id::moves_through_earth},
        {"PROP_OOZE", prop::Id::ooze},
        {"PROP_OTHERS_TERRIFIED_ON_DEATH", prop::Id::others_terrified_on_death},
        {"PROP_PARALYZED", prop::Id::paralyzed},
        {"PROP_POISONED", prop::Id::poisoned},
        {"PROP_PREMONITION", prop::Id::premonition},
        {"PROP_RADIANT_ADJACENT", prop::Id::radiant_adjacent},
        {"PROP_RADIANT_FOV", prop::Id::radiant_fov},
        {"PROP_RADIANT_SELF", prop::Id::radiant_self},
        {"PROP_RECLOAKS", prop::Id::recloaks},
        {"PROP_REDUCED_PIERCE_DMG", prop::Id::reduced_pierce_dmg},
        {"PROP_REGENERATING", prop::Id::regenerating},
        {"PROP_R_BLIND", prop::Id::r_blind},
        {"PROP_R_BREATH", prop::Id::r_breath},
        {"PROP_R_CONF", prop::Id::r_conf},
        {"PROP_R_DISEASE", prop::Id::r_disease},
        {"PROP_R_ELEC", prop::Id::r_elec},
        {"PROP_R_FEAR", prop::Id::r_fear},
        {"PROP_R_FIRE", prop::Id::r_fire},
        {"PROP_R_PARA", prop::Id::r_para},
        {"PROP_R_PHYS", prop::Id::r_phys},
        {"PROP_R_POISON", prop::Id::r_poison},
        {"PROP_R_SHOCK", prop::Id::r_shock},
        {"PROP_R_SLEEP", prop::Id::r_sleep},
        {"PROP_R_SLOW", prop::Id::r_slow},
        {"PROP_R_SPELL", prop::Id::r_spell},
        {"PROP_SEE_INVIS", prop::Id::see_invis},
        {"PROP_SHAPESHIFTS", prop::Id::shapeshifts},
        {"PROP_SHORT_HEARING_RANGE", prop::Id::short_hearing_range},
        {"PROP_SLOWED", prop::Id::slowed},
        {"PROP_SMALL_CRAWLING", prop::Id::small_crawling},
        {"PROP_SPAWNS_ZOMBIE_PARTS_ON_DESTROYED", prop::Id::spawns_zombie_parts_on_destroyed},
        {"PROP_SPELL_REFLECT", prop::Id::spell_reflect},
        {"PROP_SPLITS_ON_DEATH", prop::Id::splits_on_death},
        {"PROP_STUCK", prop::Id::stuck},
        {"PROP_STUNNED", prop::Id::stunned},
        {"PROP_SUMMONS_LOCUSTS", prop::Id::summons_locusts},
        {"PROP_TELEPORTS", prop::Id::teleports},
        {"PROP_TELEPORTS_AWAY", prop::Id::teleports_away},
        {"PROP_TELE_CTRL", prop::Id::tele_ctrl},
        {"PROP_TERRIFIED", prop::Id::terrified},
        {"PROP_TINY_FLYING", prop::Id::tiny_flying},
        {"PROP_VOMITS_OOZE", prop::Id::vomits_ooze},
        {"PROP_VORTEX", prop::Id::vortex},
        {"PROP_WATER_CREATURE", prop::Id::water_creature},
        {"PROP_WEAKENED", prop::Id::weakened},
        {"PROP_ZEALOT_STOP", prop::Id::zealot_stop},
        {"PROP_ZUUL_POSSESS_PRIEST", prop::Id::zuul_possess_priest},
};

static void add(prop::PropData& d)
{
#ifndef NDEBUG
        const std::string worst_case_str = d.name_short + prop::g_property_ending_suffix;

        const size_t worst_case_w = utf8::width(worst_case_str);

        const size_t panel_w = panels::w(Panel::map_gui_stats);

        if (worst_case_w > panel_w) {
                TRACE
                        << "The string '"
                        << worst_case_str
                        << "' of length '"
                        << worst_case_w
                        << "' will not fit in panel of width '"
                        << panel_w
                        << "'"
                        << std::endl;

                PANIC;
        }

#endif  // NDEBUG

        prop::g_data[(size_t)d.id] = d;

        d = {};
}

static void init_data_list()
{
        prop::PropData d;

        d.id = prop::Id::r_phys;
        d.std_rnd_turns = Range(50, 100);
        d.name = "物理抗性";
        d.name_short = "物抗";
        d.descr = "不会受到物理攻击伤害。";
        d.msg_start_player = "我觉得自己能抵御物理攻击。";
        d.msg_start_mon = "看起来坚如钢铁。";
        d.msg_end_player = "我觉得自己容易受到物理攻击伤害。";
        d.msg_end_mon = "看起来没那么强壮了。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_fire;
        d.std_rnd_turns = Range(50, 100);
        d.name = "火焰抗性";
        d.name_short = "火抗";
        d.descr = "不会受到火焰伤害。";
        d.msg_start_player = "我觉得自己对火焰有了抗性。";
        d.msg_start_mon = "对火焰有抗性。";
        d.msg_end_player = "我觉得自己容易被火焰伤害。";
        d.msg_end_mon = "容易被火焰伤害。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_poison;
        d.std_rnd_turns = Range(50, 100);
        d.name = "毒素抗性";
        d.name_short = "毒抗";
        d.descr = "不会受到毒素伤害。";
        d.msg_start_player = "我觉得自己对毒素有了抗性。";
        d.msg_start_mon = "对毒素有抗性。";
        d.msg_end_player = "我觉得自己容易受到毒素伤害。";
        d.msg_end_mon = "容易受到毒素伤害。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_elec;
        d.std_rnd_turns = Range(50, 100);
        d.name = "电击抗性";
        d.name_short = "电抗";
        d.descr = "不会受到电击伤害。";
        d.msg_start_player = "我觉得自己对电击有了抗性。";
        d.msg_start_mon = "对电击有抗性。";
        d.msg_end_player = "我觉得自己容易受到电击伤害。";
        d.msg_end_mon = "容易受到电击伤害。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_sleep;
        d.std_rnd_turns = Range(50, 100);
        d.name = "睡眠抗性";
        d.name_short = "睡抗";
        d.descr = "不会昏厥或被催眠。";
        d.msg_start_player = "我精神抖擞。";
        d.msg_start_mon = "精神抖擞。";
        d.msg_end_player = "我没那么清醒了。";
        d.msg_end_mon = "没那么清醒了。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_fear;
        d.std_rnd_turns = Range(50, 100);
        d.name = "恐惧抗性";
        d.name_short = "恐抗";
        d.descr = "不受恐惧影响。";
        d.msg_start_player = "我不会被恐惧动摇。";
        d.msg_start_mon = "对恐惧有抗性。";
        d.msg_end_player = "我觉得自己容易受到恐惧影响。";
        d.msg_end_mon = "容易受到恐惧影响。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_slow;
        d.std_rnd_turns = Range(50, 100);
        d.name = "减速抗性";
        d.name_short = "缓抗";
        d.descr = "不会被魔法减速。";
        d.msg_start_player = "我觉得自己坚定不移。";
        d.msg_end_player = "我觉得自己更容易受到时间影响。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_conf;
        d.std_rnd_turns = Range(50, 100);
        d.name = "困惑抗性";
        d.name_short = "困抗";
        d.descr = "不会陷入困惑。";
        d.msg_start_player = "我觉得自己对困惑有了抗性。";
        d.msg_start_mon = "对困惑有抗性。";
        d.msg_end_player = "我觉得自己容易陷入困惑。";
        d.msg_end_mon = "容易陷入困惑。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_disease;
        d.std_rnd_turns = Range(50, 100);
        d.name = "疾病抗性";
        d.name_short = "病抗";
        d.descr = "不会感染疾病。";
        d.msg_start_player = "我觉得自己对疾病有了抗性。";
        d.msg_start_mon = "对疾病有抗性。";
        d.msg_end_player = "我觉得自己容易感染疾病。";
        d.msg_end_mon = "容易感染疾病。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_blind;
        d.name = "失明抗性";
        d.name_short = "盲抗";
        d.descr = "不会失明。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_para;
        d.std_rnd_turns = Range(50, 100);
        d.name = "麻痹抗性";
        d.name_short = "麻抗";
        d.descr = "不会陷入麻痹。";
        d.msg_start_player = "我觉得更稳了。";
        d.msg_start_mon = "看起来更稳了。";
        d.msg_end_player = "我觉得没那么稳了。";
        d.msg_end_mon = "看起来没那么稳了。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_breath;
        d.std_rnd_turns = Range(50, 100);
        d.descr = "不会因呼吸受阻而受伤。";
        d.msg_start_player = "我可以安然呼吸。";
        d.msg_start_mon = "可以安然呼吸。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_spell;
        d.name = "法术抗性";
        d.name_short = "法抗";
        d.descr = "不会受到有害法术影响。";
        d.msg_start_player = "我能抵御有害法术！";
        d.msg_start_mon = "正在抵御有害法术。";
        d.msg_end_player = "我觉得自己容易受到法术影响。";
        d.msg_end_mon = "容易受到法术影响。";
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::r_shock;
        d.std_rnd_turns = Range(8, 12);
        d.name = "震慑抗性";
        d.name_short = "震抗";
        d.descr = "不受震慑事件影响。";
        d.msg_start_player = "没有什么能扰乱我的心智！";
        d.msg_start_mon = "";
        d.msg_end_player = "我又开始感受到此地的恐怖。";
        d.msg_end_mon = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::light_sensitive;
        d.std_rnd_turns = Range(30, 60);
        d.name = "光敏感";
        d.name_short = "光敏";
        d.descr = "容易受到光线影响。";
        d.msg_start_player = "我觉得自己容易受到光线影响！";
        d.msg_start_mon = "容易受到光线影响。";
        d.msg_end_player = "我不再觉得自己容易受到光线影响。";
        d.msg_end_mon = "不再容易受到光线影响。";
        d.allow_display_turns = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::blind;
        d.std_rnd_turns = Range(6, 12);
        d.name = "失明";
        d.name_short = "失明";
        d.descr = "无法视物，命中率 -20%，闪避攻击的几率 -50%。";
        d.msg_start_player = "我失明了！";
        d.msg_start_mon = "失明了。";
        d.msg_end_player = "我又能看见了！";
        d.msg_end_mon = "可以重新看见。";
        d.historic_msg_start_permanent = "永久失明";
        d.historic_msg_end_permanent = "我恢复视力了。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::deaf;
        d.std_rnd_turns = Range(50, 100);
        d.name = "失聪";
        d.name_short = "失聪";
        d.descr = "听不见声音。";
        d.msg_start_player = "我失聪了！";
        d.msg_start_mon = "不再对任何声音作出反应";
        d.msg_end_player = "我又能听见了。";
        d.msg_end_mon = "又能对声音作出反应了。";
        d.historic_msg_start_permanent = "永久失聪";
        d.historic_msg_end_permanent = "我恢复听力了。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::fainted;
        d.std_rnd_turns = Range(30, 60);
        d.name = "昏厥";
        d.name_short = "昏厥";
        d.descr =
                "暂时失去意识；受到任何伤害或经过足够时间后就会醒来。";
        d.msg_start_player = "我昏厥了！";
        d.msg_start_mon = "昏厥。";
        d.msg_end_player = "我醒着。";
        d.msg_end_mon = "醒来了。";
        d.msg_res_player = "我抵抗住了昏厥。";
        d.msg_res_mon = "抵抗昏厥。";
        d.allow_display_turns = true;
        d.force_interrupt_player_on_start = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::burning;
        d.std_rnd_turns = Range(6, 8);
        d.name = "燃烧";
        d.name_short = "燃烧";
        d.descr =
                "每回合受到伤害；尝试阅读或施放法术时有 50% 几率失败。";
        d.msg_start_player = "我着火了！";
        d.msg_start_mon = "着火了。";
        d.msg_end_player = "火焰熄灭了。";
        d.msg_end_mon = "不再燃烧了。";
        d.msg_res_player = "我抵抗住了燃烧。";
        d.msg_res_mon = "抵抗燃烧。";
        d.allow_display_turns = true;
        d.force_interrupt_player_on_start = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::poisoned;
        d.std_rnd_turns = Range(40, 80);
        d.name = "中毒";
        d.name_short = "中毒";
        d.descr =
                "生命值高于最大生命值的 50% 时，会缓慢受到伤害；生命值不会自然恢复；近战伤害 -25%，近战命中率 -10%，闪避 -10%。再次施加毒素时，毒素持续时间会叠加。";
        d.msg_start_player = "我中毒了！";
        d.msg_start_mon = "中毒了。";
        d.msg_end_player = "我的身体已从毒素中净化！";
        d.msg_end_mon = "毒性已被清除。";
        d.msg_res_player = "我抵抗住了中毒。";
        d.msg_res_mon = "抵抗中毒。";
        d.allow_display_turns = true;
        d.duration_on_more = prop::DurationOnMoreBehavior::stacked;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::paralyzed;
        d.std_rnd_turns = Range(3, 5);
        d.name = "麻痹";
        d.name_short = "麻痹";
        d.descr = "无法移动。";
        d.msg_start_player = "我陷入麻痹了！";
        d.msg_start_mon = "陷入麻痹了。";
        d.msg_end_player = "我又能动了！";
        d.msg_end_mon = "可以重新移动。";
        d.msg_res_player = "我抵抗住了麻痹。";
        d.msg_res_mon = "抵抗麻痹。";
        d.allow_display_turns = true;
        d.force_interrupt_player_on_start = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::delayed_by_liquid;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::terrified;
        d.std_rnd_turns = Range(20, 30);
        d.name = "恐惧";
        d.name_short = "恐惧";
        d.descr =
                "无法进行近战攻击，远程命中率 -20%，闪避攻击的几率 +20%。";
        d.msg_start_player = "我吓坏了！";
        d.msg_start_mon = "看起来很恐惧。";
        d.msg_end_player = "我不再恐惧了！";
        d.msg_end_mon = "不再恐惧了。";
        d.msg_res_player = "我抵抗住了恐惧。";
        d.msg_res_mon = "抵抗恐惧。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::confused;
        d.std_rnd_turns = Range(30, 60);
        d.name = "困惑";
        d.name_short = "困惑";
        d.descr =
                "偶尔会朝随机方向移动，无法阅读或施放法术，也无法搜寻隐藏门或陷阱。";
        d.msg_start_player = "我困惑了！";
        d.msg_start_mon = "看起来很困惑。";
        d.msg_end_player = "我恢复清醒了。";
        d.msg_end_mon = "不再困惑了。";
        d.msg_res_player = "我设法保持头脑清醒。";
        d.msg_res_mon = "抵抗困惑。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::hallucinating;
        d.std_rnd_turns = Range(75, 150);
        d.name = "产生幻觉";
        d.name_short = "幻觉";
        d.descr = "感官并不总是可信。";
        d.msg_start_player = "我开始怀疑自己的感官。";
        d.msg_start_mon = "";
        d.msg_end_player = "我觉得自己的感官更可靠了。";
        d.msg_end_mon = "";
        d.msg_res_player = "我设法分清现实与幻象。";
        d.msg_res_mon = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::astral_opium_addiction;
        d.std_rnd_turns = Range(100, 200);
        d.std_rnd_dlvls = Range(3, 6);
        d.name = "星界鸦片成瘾";
        d.name_short = "成瘾者";
        d.descr =
                "沉溺于星界鸦片——若不再使用星界鸦片，成瘾最终会消退；但戒断很快会引发症状（最低震慑值上升）。这种成瘾力量过于强大、过于异界，坚毅药水也无法治愈。";
        d.msg_start_player = "那感觉太棒了！";
        d.msg_start_mon = "";
        d.msg_end_player =
                "我突然意识到自己不再渴求星界鸦片。";
        d.msg_end_mon = "";
        d.msg_res_player = "";
        d.msg_res_mon = "";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::meditative_focused;
        d.name = "专注";
        d.name_short = "专注";
        d.descr =
                "下一个法术无需消耗一个回合即可施放，且消耗减少 1 点。";
        d.msg_start_player = "我感到十分专注。";
        d.msg_end_player = "我不再那么专注了。";
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::thorns;
        d.name = "荆棘";
        d.name_short = "荆棘";
        d.descr =
                "施法者每次受到近战、远程或伤害性法术攻击时，攻击者都会遭到一股不可抗拒的力量反击。";
        d.msg_start_player = "反击效果被封印了！";
        d.msg_end_player = "我不再反击伤害。";
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::crimson_passage;
        d.name = "绯红通道";
        d.name_short = "绯红通道";
        d.descr =
                "行走不消耗时间，但每走一步都会消耗 2 点生命值（原地等待仍照常进行）。走过一定步数后，或生命值不足以继续扣除时，效果结束。";
        d.msg_start_player = "前方有一条骇人的道路。";
        d.msg_end_player = "我的行动恢复正常了。";
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::stunned;
        d.std_rnd_turns = Range(5, 9);
        d.name = "眩晕";
        d.name_short = "眩晕";
        d.msg_start_player = "我眩晕了！";
        d.msg_start_mon = "眩晕了。";
        d.msg_end_player = "我不再眩晕了。";
        d.msg_end_mon = "不再眩晕了。";
        d.msg_res_player = "我抵抗住了眩晕。";
        d.msg_res_mon = "抵抗眩晕。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::slowed;
        d.std_rnd_turns = Range(16, 24);
        d.name = "减速";
        d.name_short = "减速";
        d.descr = "移动速度减慢。";
        d.msg_start_player = "周围的一切似乎都快了起来。";
        d.msg_start_mon = "速度减慢了。";
        d.msg_end_player = "周围的一切似乎都慢了下来。";
        d.msg_end_mon = "速度加快了。";
        d.msg_res_player = "我抵抗住了减速。";
        d.msg_res_mon = "抵抗减速。";
        d.historic_msg_start_permanent = "永久减速";
        d.historic_msg_end_permanent = "我的减速效果消退了。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::hasted;
        d.std_rnd_turns = Range(12, 16);
        d.name = "加速";
        d.name_short = "加速";
        d.descr = "移动速度加快。";
        d.msg_start_player = "周围的一切似乎都慢了下来。";
        d.msg_start_mon = "速度加快了。";
        d.msg_end_player = "周围的一切似乎都快了起来。";
        d.msg_end_mon = "速度减慢了。";
        d.historic_msg_start_permanent = "永久获得加速";
        d.historic_msg_end_permanent = "我的加速效果消退了。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::extra_hasted;
        d.std_rnd_turns = Range(7, 11);
        d.name = "极度加速";
        d.name_short = "极度加速";
        d.descr = "移动速度极快。";
        d.msg_start_player = "周围的一切突然静止不动了。";
        d.msg_end_player = "周围的一切似乎都快了许多。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::summoned;
        d.std_rnd_turns = Range(80, 120);
        d.msg_end_mon = "突然消失了。";
        d.name = "被召唤";
        d.descr = "曾被魔法召唤至此。";
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::nailed;
        d.name = "被钉住";
        d.descr =
                "被尖刺钉住了。拔出来会相当痛苦。";
        d.msg_start_player = "我被尖刺钉住了！";
        d.msg_start_mon = "被尖刺钉住了。";
        d.msg_end_player = "我挣脱了！";
        d.msg_end_mon = "挣脱了！";
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::wound;
        d.name = "受伤";
        d.descr =
                "每处创伤：近战命中率 -5%、闪避攻击的几率 -5%、生命值 -10%，并减慢生命值恢复速度。身上同时有 3 处创伤时，行走会额外消耗回合；同时有 5 处创伤会致命。";
        d.msg_start_player = "我受伤了！";
        d.msg_res_player = "我抵抗创伤！";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::hp_sap;
        d.name = "生命力被汲取";
        d.descr = "生命值减少。";
        d.msg_start_player = "我的生命力被汲取了！";
        d.msg_start_mon = "生命力被汲取。";
        d.msg_end_player = "我的生命力恢复了。";
        d.msg_end_mon = "看起来恢复了。";
        d.msg_res_player = "我抵抗生命汲取。";
        d.msg_res_mon = "抵抗生命汲取。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::spi_sap;
        d.name = "精神值被汲取";
        d.descr = "精神值减少。";
        d.msg_start_player = "我的精神值被汲取了！";
        d.msg_start_mon = "精神值被汲取。";
        d.msg_end_player = "我的精神值恢复了。";
        d.msg_end_mon = "看起来恢复了。";
        d.msg_res_player = "我抵抗生命汲取。";
        d.msg_res_mon = "抵抗生命汲取。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::mind_sap;
        d.name = "心智受到汲取";
        d.descr = "震慑增加。";
        d.msg_start_player = "我的心智被汲取了！";
        d.msg_end_player = "我的心智恢复了。";
        d.msg_res_player = "我抵抗生命汲取。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::infected;
        d.std_rnd_turns = Range(50, 100);
        d.name = "已感染";
        d.name_short = "已感染";
        d.descr = "一种可怕的感染，若不治疗，病情会不断恶化。";
        d.msg_start_player = "我感染了！";
        d.msg_start_mon = "感染了。";
        d.msg_end_player = "我的感染痊愈了！";
        d.msg_end_mon = "感染已治愈。";
        d.allow_display_turns = false;
        // Never increase duration if more infection is applied:
        d.duration_on_more = prop::DurationOnMoreBehavior::shortest;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::diseased;
        d.std_rnd_turns = Range(30, 60);
        d.name = "患病";
        d.name_short = "患病";
        d.descr = "最大生命值 -50%。";
        d.msg_start_player = "我患病了！";
        d.msg_start_mon = "患病了。";
        d.msg_end_player = "我的疾病痊愈了！";
        d.msg_end_mon = "疾病已痊愈。";
        d.msg_res_player = "我抵抗疾病。";
        d.msg_res_mon = "抵抗疾病。";
        d.historic_msg_start_permanent = "染上了可怕的疾病";
        d.historic_msg_end_permanent = "从可怕的疾病中痊愈";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::descend;
        d.std_rnd_turns = Range(20, 30);
        d.name = "正在下楼";
        d.name_short = "正在下楼";
        d.descr = "很快就会前往更深一层。";
        d.msg_start_player = "我感到一阵下坠。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::weakened;
        d.std_rnd_turns = Range(30, 60);
        d.name = "虚弱";
        d.name_short = "虚弱";
        d.descr =
                "近战伤害 -50%，无法发动背刺攻击，无法撞开门或箱子、撞倒重物等。";
        d.msg_start_player = "我感觉变弱了。";
        d.msg_start_mon = "看起来变弱了。";
        d.msg_end_player = "我感觉更强壮了！";
        d.msg_end_mon = "看起来更强壮了！";
        d.msg_res_player = "我抵抗虚弱。";
        d.msg_res_mon = "抵抗虚弱。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::frenzied;
        d.std_rnd_turns = Range(30, 60);
        d.name = "狂乱";
        d.name_short = "狂乱";
        d.descr =
                "无法远离已发现的敌人，移动速度提高，近战伤害 +1、近战命中率 +10%；免疫困惑、昏厥、恐惧和虚弱；无法阅读或施法；狂乱结束后会陷入虚弱。";
        d.msg_start_player = "我感觉凶猛无比！！！";
        d.msg_start_mon = "看起来凶猛无比！";
        d.msg_end_player = "我平静下来。";
        d.msg_end_mon = "稍稍平静下来。";
        d.allow_display_turns = true;
        d.force_interrupt_player_on_start = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::blessed;
        d.std_rnd_turns = Range(400, 600);
        d.name = "受祝福";
        d.name_short = "受祝福";
        d.descr = "命中率、闪避、潜行和搜索能力 +10%。";
        d.msg_start_player = "我感觉运气变好了。";
        d.msg_end_player = "我的运气恢复正常。";
        d.historic_msg_start_permanent = "获得永恒的祝福";
        d.historic_msg_end_permanent = "我的大福泽消失了";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::cursed;
        d.std_rnd_turns = Range(30, 60);
        d.name = "被诅咒";
        d.name_short = "被诅咒";
        d.descr = "命中率、闪避、潜行和搜索能力 -10%。";
        d.msg_start_player = "我感觉时运不济。";
        d.msg_start_mon = "被诅咒了。";
        d.msg_end_player = "我感觉运气更好了。";
        d.msg_end_mon = "诅咒已解除。";
        d.msg_res_player = "我抵挡住了厄运。";
        d.msg_res_mon = "抵挡厄运。";
        d.historic_msg_start_permanent = "我被施加了一个永恒的诅咒";
        d.historic_msg_end_permanent = "一个可怕的诅咒从我身上解除";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::doomed;
        d.std_rnd_turns = Range(30, 60);
        d.name = "命定厄运";
        d.name_short = "命定厄运";
        d.descr =
                "命中率、闪避、潜行和搜索能力 -20%；阅读或施放法术时有 10% 的失败几率。";
        d.msg_start_player = "我感觉厄运将至！";
        d.msg_start_mon = "厄运降临！";
        d.msg_end_player = "我的厄运似乎不再那么确定。";
        d.msg_end_mon = "厄运已解除。";
        d.msg_res_player = "我抵挡住了一场大灾厄。";
        d.historic_msg_start_permanent = "我的厄运早已注定";
        d.historic_msg_end_permanent = "希望再次回归";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::extra_skill;
        d.std_rnd_turns = Range(200, 300);
        d.name = "额外技能";
        d.name_short = "技能";
        d.descr = "命中率、闪避、潜行和搜索能力 +10%。";
        d.msg_start_player = "我感觉技巧更娴熟了。";
        d.msg_start_mon = "看起来更加娴熟。";
        d.msg_end_player = "我感觉技巧变差了。";
        d.msg_end_player = "看起来不那么娴熟了。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::magic_carapace;
        d.std_rnd_turns = Range(25, 50);
        d.name = "甲壳";
        d.name_short = "甲壳";
        d.descr = "护甲值 +3，抵抗燃烧的几率 +25%。";
        d.msg_start_player = "一层保护性甲壳在我周围形成。";
        d.msg_start_mon = "被保护性甲壳包裹。";
        d.msg_end_player = "我的甲壳裂开并崩落。";
        d.msg_end_player = "脱落了一层甲壳。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::premonition;
        d.std_rnd_turns = Range(5, 9);
        d.name = "预兆";
        d.name_short = "预兆";
        d.descr = "闪避攻击的几率 +75%。";
        d.msg_start_player = "我感觉坚不可摧。";
        d.msg_start_mon = "看起来坚不可摧。";
        d.msg_end_player = "我感觉自己更脆弱了。";
        d.msg_end_mon = "现在看起来更容易击中。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::erudition;
        d.name = "博学";
        d.name_short = "博学";
        d.descr = "法术技能提升了一级。";
        d.msg_start_player = "神秘的秘密向我显现！";
        d.msg_end_player = "我感到愚昧无知。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::magic_searching;
        d.std_rnd_turns = Range(20, 60);
        d.name = "魔法探查";
        d.name_short = "魔法搜寻";
        d.descr =
                "可以用魔法探测周围区域内的物品和生物。";
        d.msg_start_player = "隐藏的秘密向我显现。";
        d.msg_end_player = "我再也看不见隐藏之物了。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::entangled;
        d.name = "被缠住";
        d.name_short = "被缠住";
        d.descr = "被什么东西缠住了。";
        d.msg_start_player = "我被缠住了！";
        d.msg_start_mon = "被缠住了。";
        d.msg_end_player = "我挣脱了！";
        d.msg_end_mon = "挣脱了！";
        d.allow_display_turns = false;
        d.force_interrupt_player_on_start = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::stuck;
        d.name = "受困";
        d.name_short = "受困";
        d.descr = "被什么东西困住了。";
        d.msg_start_player = "我被困住了！";
        d.msg_start_mon = "被困住了。";
        d.msg_end_player = "我挣脱了！";
        d.msg_end_mon = "挣脱了！";
        d.allow_display_turns = false;
        d.force_interrupt_player_on_start = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::radiant_self;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::radiant_adjacent;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::radiant_fov;
        d.std_rnd_turns = Range(50, 100);
        d.name = "辉耀";
        d.name_short = "辉耀";
        d.descr = "散发着明亮的光芒。";
        d.msg_start_player = "一道明亮的光芒在我周围闪耀。";
        d.msg_end_player = "周围骤然变暗。";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::invis;
        d.std_rnd_turns = Range(50, 100);
        d.name = "隐形";
        d.name_short = "隐形";
        d.descr = "无法被肉眼发现。";
        d.msg_start_player = "我消失在视野之外！";
        d.msg_start_mon = "消失在视野之外！";
        d.msg_end_player = "我不再隐形了。";
        d.msg_end_mon = "";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::cloaked;
        d.std_rnd_turns = Range(5, 7);
        d.name = "隐身";
        d.name_short = "隐身";
        d.descr =
                "无法被肉眼发现；攻击或施法会使效果结束。";
        d.msg_start_player = "我隐去了身形！";
        d.msg_start_mon = "隐去了身形！";
        d.msg_end_player = "我的隐身效果正在消退。";
        d.msg_end_mon = "";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::recloaks;
        add(d);

        d.id = prop::Id::see_invis;
        d.std_rnd_turns = Range(50, 100);
        d.name = "看见隐形之物";
        d.name_short = "看见隐形之物";
        d.descr = "可以看见隐形生物，且不会失明。";
        d.msg_start_player = "我的双眼看见了隐形之物。";
        d.msg_start_mon = "似乎格外警觉。";
        d.msg_end_player = "我的双眼再也看不见隐形之物。";
        d.msg_end_mon = "似乎没那么警觉了。";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::darkvision;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = true;
        d.update_vision_on_toggled = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::tele_ctrl;
        d.std_rnd_turns = Range(50, 100);
        d.name = "传送控制";
        d.name_short = "传送控制";
        d.descr = "可以控制传送目的地。";
        d.msg_start_player = "我感觉一切尽在掌控。";
        d.msg_end_player = "我感觉不再那么有把握。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::spell_reflect;
        d.std_rnd_turns = Range(50, 100);
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::aiming;
        d.std_rnd_turns = Range(1, 1);
        d.name = "瞄准";
        d.name_short = "瞄准";
        d.descr = "远程攻击效果提高。";
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::conflict;
        d.name = "心神冲突";
        d.name_short = "心神冲突";
        d.descr = "将所有生物都视为敌人。";
        d.std_rnd_turns = Range(10, 20);
        d.msg_start_mon = "看起来心神不宁。";
        d.msg_end_mon = "看起来更加坚定。";
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::aura_of_decay;
        d.std_rnd_turns = Range(6, 12);
        d.name = "腐朽光环";
        d.name_short = "腐朽光环";
        d.descr = "两步距离内的生物每个标准回合都会受到伤害。";
        d.msg_start_player = "凋零之力笼罩着我。";
        d.msg_start_mon = "散发着死亡与腐朽的气息。";
        d.msg_end_player = "腐朽之力平息了。";
        d.msg_end_mon = "不再散发腐朽气息。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::reduced_pierce_dmg;
        add(d);

        d.id = prop::Id::flammable;
        add(d);

        d.id = prop::Id::vortex;
        add(d);

        d.id = prop::Id::explodes_on_death;
        add(d);

        d.id = prop::Id::splits_on_death;
        add(d);

        d.id = prop::Id::corpse_eater;
        add(d);

        d.id = prop::Id::teleports;
        add(d);

        d.id = prop::Id::teleports_away;
        add(d);

        d.id = prop::Id::always_aware;
        add(d);

        d.id = prop::Id::corrupts_env_color;
        add(d);

        d.id = prop::Id::alters_env;
        add(d);

        d.id = prop::Id::regenerating;
        d.std_rnd_turns = Range(50, 100);
        d.name = "正在再生";
        d.name_short = "正在再生";
        d.descr = "每回合额外恢复 +1 点生命值。";
        d.msg_start_player = "我的身体开始快速自愈。";
        d.msg_start_mon = "开始快速恢复伤势。";
        d.msg_end_player = "我的身体恢复得更慢了。";
        d.msg_end_mon = "不再快速恢复伤势。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::corpse_rises;
        add(d);

        d.id = prop::Id::breeds;
        add(d);

        d.id = prop::Id::breeds_scattered;
        add(d);

        d.id = prop::Id::frenzies_self;
        add(d);

        d.id = prop::Id::frenzies_followers;
        add(d);

        d.id = prop::Id::others_terrified_on_death;
        add(d);

        d.id = prop::Id::summons_locusts;
        add(d);

        d.id = prop::Id::vomits_ooze;
        add(d);

        d.id = prop::Id::confuses_adjacent;
        add(d);

        d.id = prop::Id::frenzy_player_on_seen;
        add(d);

        d.id = prop::Id::zuul_possess_priest;
        add(d);

        d.id = prop::Id::possessed_by_zuul;
        add(d);

        d.id = prop::Id::shapeshifts;
        add(d);

        d.id = prop::Id::zealot_stop;
        add(d);

        d.id = prop::Id::major_clapham_summon;
        add(d);

        d.id = prop::Id::allies_ghoul_player;
        add(d);

        d.id = prop::Id::flying;
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::tiny_flying;
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::ethereal;
        d.name = "以太化";
        d.name_short = "以太化";
        d.descr = "可以穿过实体，闪避攻击的几率 +50%。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::ooze;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::small_crawling;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::burrowing;
        d.std_rnd_turns = Range(4, 8);
        d.name = "掘地";
        d.name_short = "掘地";
        d.descr = "可以穿过墙壁和瓦砾掘地。";
        d.msg_start_player = "土石在我面前崩解消散。";
        d.msg_start_mon = "可以穿行于土层中。";
        d.msg_end_player = "大地再次坚实如初。";
        d.msg_end_mon = "不再能穿行于土层中。";
        d.allow_display_turns = true;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::water_creature;
        add(d);

        d.id = prop::Id::moves_through_earth;
        add(d);

        d.id = prop::Id::waiting;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::disabled_attack;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::disabled_melee;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::disabled_ranged;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::melee_cooldown;
        d.std_rnd_turns = Range(1, 1);
        d.allow_display_turns = false;
        d.alignment = prop::PropAlignment::neutral;
        add(d);

        d.id = prop::Id::hit_chance_penalty_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "我的瞄准似乎更差了。";
        d.msg_end_player = "我的瞄准似乎更准了。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::increased_shock_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "我更加焦虑了！";
        d.msg_end_player = "我不再那么焦虑了。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::cannot_read_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player = "我觉得自己目不识丁！";
        d.msg_end_player = "我又能阅读了。";
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        // NOTE: This property reuses messages from 'light_sensitive', so order
        // is important here
        d.id = prop::Id::light_sensitive_curse;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.msg_start_player =
                prop::g_data[(size_t)prop::Id::light_sensitive]
                        .msg_start_player;
        d.msg_end_player =
                prop::g_data[(size_t)prop::Id::light_sensitive]
                        .msg_end_player;
        d.allow_display_turns = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::disabled_hp_regen;
        d.std_rnd_turns = Range(1, 1);
        d.name = "";
        d.name_short = "";
        d.descr = "";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::bad;
        add(d);

        d.id = prop::Id::sanctuary;
        d.std_rnd_turns = Range(5, 7);
        d.name = "庇护所";
        d.name_short = "庇护所";
        d.descr =
                "所有敌对生物都会忽视它。移动或进行近战、远程攻击会使效果结束。";
        d.msg_start_player = "我感到非常安心。";
        d.msg_end_player = "我感觉安全感大减。";
        d.allow_display_turns = true;
        d.update_vision_on_toggled = false;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::flagellant;
        d.allow_display_turns = false;
        d.allow_test_on_bot = false;
        d.alignment = prop::PropAlignment::good;
        add(d);

        d.id = prop::Id::moribund;
        d.descr = "近战伤害 +3，近战命中率 +30%，护甲值 +3。";
        d.name = "濒死";
        d.name_short = "濒死";
        d.allow_display_turns = true;
        d.allow_test_on_bot = true;
        d.alignment = prop::PropAlignment::good;
        add(d);
}

// -----------------------------------------------------------------------------
// property_data
// -----------------------------------------------------------------------------
namespace prop
{
PropData g_data[(size_t)prop::Id::END];

void init()
{
        init_data_list();
}

prop::Id str_to_prop_id(const std::string& str)
{
        return s_str_to_prop_id_map.at(str);
}

std::string descr(prop::Id id)
{
        ASSERT(id != prop::Id::END);

        return g_data[(size_t)id].descr;
}

}  // namespace prop
