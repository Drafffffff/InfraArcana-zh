// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_scroll.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>

#include "actor.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "spells.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<std::string> s_fake_names;

static SpellSkill player_skill_for_scroll(const SpellId spell_id)
{
        SpellSkill skill = actor::spell_skill(*map::g_player, spell_id);

        const bool has_necronomicon =
                map::g_player->m_inv.has_item_in_backpack(
                        item::Id::necronomicon);

        const auto skill_cap =
                has_necronomicon
                ? SpellSkill::transcendent
                : SpellSkill::master;

        if (skill < skill_cap) {
                skill = (SpellSkill)((int)skill + 1);
        }

        return skill;
}

// -----------------------------------------------------------------------------
// scroll
// -----------------------------------------------------------------------------
namespace scroll
{
void init()
{
        TRACE_FUNC_BEGIN;

        // Randomize scroll fake names
        s_fake_names.clear();

        // Fixed fake names:
        s_fake_names.emplace_back("克鲁恩西斯吉特");
        s_fake_names.emplace_back("红花属");
        s_fake_names.emplace_back("红米诺克斯");
        s_fake_names.emplace_back("杂草");
        s_fake_names.emplace_back("海傍");
        s_fake_names.emplace_back("普雷蒂娅·克鲁恩托");
        s_fake_names.emplace_back("克鲁恩托鼠疫");
        s_fake_names.emplace_back("鼠疫杆菌");
        s_fake_names.emplace_back("多莫斯-巴瓦");
        s_fake_names.emplace_back("槭树");
        s_fake_names.emplace_back("普雷塔安鲁克斯");
        s_fake_names.emplace_back("普兰西莱努斯");
        s_fake_names.emplace_back("曲皮派克斯");
        s_fake_names.emplace_back("洛克蒙杜克斯");
        s_fake_names.emplace_back("普罗法努克斯");
        s_fake_names.emplace_back("沙安提图斯");
        s_fake_names.emplace_back("格罗帕亚蒂");
        s_fake_names.emplace_back("维洛马克斯");
        s_fake_names.emplace_back("布乌德斯科");
        s_fake_names.emplace_back("杜本蒂亚");
        s_fake_names.emplace_back("布埃斯科");
        s_fake_names.emplace_back("马拉维塔");
        s_fake_names.emplace_back("因菲尔克斯");

        // Words that can be combined into names:
        std::vector<std::string> combinable_names;
        combinable_names.clear();
        combinable_names.emplace_back("克鲁奥");
        combinable_names.emplace_back("克鲁尼特");
        combinable_names.emplace_back("克鲁恩图");
        combinable_names.emplace_back("马拉纳");
        combinable_names.emplace_back("多莫斯");
        combinable_names.emplace_back("马拉克斯");
        combinable_names.emplace_back("凯库克斯");
        combinable_names.emplace_back("埃克西姆哈");
        combinable_names.emplace_back("沃罗克斯");
        combinable_names.emplace_back("比盒");
        combinable_names.emplace_back("帕莱克斯");
        combinable_names.emplace_back("普罗凡克斯");
        combinable_names.emplace_back("隐形");
        combinable_names.emplace_back("隐形眼镜");
        combinable_names.emplace_back("奥迪奥苏");
        combinable_names.emplace_back("奥迪苏克斯");
        combinable_names.emplace_back("维格拉");
        combinable_names.emplace_back("克鲁杜克斯");
        combinable_names.emplace_back("德斯科");
        combinable_names.emplace_back("埃斯科");
        combinable_names.emplace_back("下吕");
        combinable_names.emplace_back("克拉图");
        combinable_names.emplace_back("巴拉达");
        combinable_names.emplace_back("尼克托");

        const size_t nr_combinable_names = combinable_names.size();

        for (size_t i = 0; i < nr_combinable_names; ++i) {
                for (size_t ii = 0; ii < nr_combinable_names; ii++) {
                        if (i != ii) {
                                s_fake_names.push_back(
                                        combinable_names[i] +
                                        " " +
                                        combinable_names[ii]);
                        }
                }
        }

        std::vector<item::ItemData*> scroll_data;

        for (item::ItemData& d : item::g_data) {
                if (d.type == ItemType::scroll) {
                        scroll_data.push_back(&d);
                }
        }

        for (item::ItemData* d : scroll_data) {
                // False name
                const size_t idx = rnd::idx(s_fake_names);

                const std::string& title = s_fake_names[idx];

                d->base_name_un_id.names[(size_t)ItemNameType::plain] =
                        "手稿标题为 " + title;

                d->base_name_un_id.names[(size_t)ItemNameType::plural] =
                        "手稿标题为 " + title;

                d->base_name_un_id.names[(size_t)ItemNameType::a] =
                        "手稿标题为 " + title;

                s_fake_names.erase(s_fake_names.begin() + (int)idx);

                // True name
                const std::unique_ptr<const Scroll> scroll(
                        static_cast<const Scroll*>(
                                item::make(d->id, 1)));

                const std::string real_type_name = scroll->real_name();

                const std::string real_name =
                        "手稿 " + real_type_name;

                const std::string real_name_plural =
                        "手稿 " + real_type_name;

                const std::string real_name_a =
                        "的手稿 " + real_type_name;

                d->base_name.names[(size_t)ItemNameType::plain] = real_name;
                d->base_name.names[(size_t)ItemNameType::plural] = real_name_plural;
                d->base_name.names[(size_t)ItemNameType::a] = real_name_a;
        }

        // Randomize scroll spawning chances - some scrolls have a "high"
        // chance of spawning, and some have a "low" chance. The effect of this
        // should be that there is less chance to find all spells.
        rnd::shuffle(scroll_data);

        const size_t nr_scrolls = scroll_data.size();
        const size_t nr_high_chance = nr_scrolls / 2;

        for (size_t i = 0; i < nr_scrolls; ++i) {
                scroll_data[i]->chance_to_incl_in_spawn_list =
                        (i < nr_high_chance)
                        ? g_high_spawn_chance
                        : g_low_spawn_chance;
        }

        TRACE_FUNC_END;
}

void save()
{
        for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                if (item::g_data[i].type != ItemType::scroll) {
                        continue;
                }

                auto& names = item::g_data[i].base_name_un_id.names;

                saving::put_str(names[(size_t)ItemNameType::plain]);
                saving::put_str(names[(size_t)ItemNameType::plural]);
                saving::put_str(names[(size_t)ItemNameType::a]);
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                if (item::g_data[i].type != ItemType::scroll) {
                        continue;
                }

                auto& names = item::g_data[i].base_name_un_id.names;

                names[(size_t)ItemNameType::plain] = saving::get_str();
                names[(size_t)ItemNameType::plural] = saving::get_str();
                names[(size_t)ItemNameType::a] = saving::get_str();
        }
}

Scroll::Scroll(item::ItemData* const item_data) :
        Item(item_data)
{
}

std::string Scroll::real_name() const
{
        const std::unique_ptr<const Spell> spell(make_spell());

        return spell->name();
}

std::vector<std::string> Scroll::descr_hook() const
{
        const std::unique_ptr<const Spell> spell(make_spell());

        if (m_data->is_identified) {
                const auto skill = player_skill_for_scroll(spell->id());

                return spell->descr(skill, SpellSrc::manuscript);
        }
        else {
                // Not identified
                auto lines = m_data->base_descr;

                if (m_data->is_spell_domain_known) {
                        const std::string domain_str = spell->domain_descr();

                        if (!domain_str.empty()) {
                                lines.push_back(domain_str);
                        }
                }
                else {
                        lines.emplace_back(
                                "也许保留一段时间会揭示一些关于它的信息。");
                }

                return lines;
        }
}

void Scroll::reveal_domain() const
{
        if (m_data->is_spell_domain_known || m_data->is_identified) {
                return;
        }

        TRACE << "Scroll domain discovered" << std::endl;

        const std::string name_plural =
                m_data->base_name_un_id.names[(size_t)ItemNameType::plural];

        const std::unique_ptr<const Spell> spell(make_spell());

        const SpellDomain domain = spell->domain();

        if (domain != SpellDomain::END) {
                const std::string domain_str =
                        text_format::first_to_lower(
                                spells::spell_domain_title(
                                        spell->domain()));

                if (!domain_str.empty()) {
                        msg_log::add(
                                std::string(
                                        "我觉得 " +
                                        name_plural +
                                        " 属于 " +
                                        domain_str +
                                        " 领域。"));
                }
        }

        m_data->is_spell_domain_known = true;
}

ItemPrePickResult Scroll::pre_pickup_hook()
{
        if (!player_bon::is_bg(Bg::exorcist)) {
                return ItemPrePickResult::do_pickup;
        }

        // Is exorcist

        const std::string noun = (m_nr_items == 1) ? "文本" : "文本";

        msg_log::add("我摧毁亵渎 " + noun + "!");

        game::incr_player_xp(g_xp_on_exorcist_destroy_scroll * m_nr_items);

        actor::restore_sp(
                *map::g_player,
                999,
                actor::AllowRestoreAboveMax::no,
                Verbose::no);

        actor::restore_exorcist_fervor(g_exorcist_fervor_destroy_scroll * m_nr_items);

        return ItemPrePickResult::destroy_item;
}

ConsumeItem Scroll::activate(actor::Actor* const actor)
{
        TRACE_FUNC_BEGIN;

        // Check properties which NEVER allows reading or speaking
        if (!actor->m_properties.allow_read_absolute(Verbose::yes) ||
            !actor->m_properties.allow_speak(Verbose::yes)) {
                return ConsumeItem::no;
        }

        const auto& player_pos = map::g_player->m_pos;

        if (map::g_dark.at(player_pos) &&
            !map::g_light.at(player_pos) &&
            !map::g_player->m_properties.has(prop::Id::darkvision)) {
                msg_log::add("这里太黑了，不适合读书。");

                TRACE_FUNC_END;

                return ConsumeItem::no;
        }

        // OK, we can try to cast

        const bool is_identified_before = m_data->is_identified;

        if (is_identified_before) {
                const std::string scroll_name =
                        name(
                                ItemNameType::a,
                                ItemNameInfo::none);

                msg_log::add("我读了 " + scroll_name + "...");
        }
        else {
                // Not already identified
                msg_log::add(
                        "我背诵手稿上的禁忌咒语……");
        }

        const std::string crumble_str = "手稿化为灰尘。";

        // Check properties which MAY allow reading, with a random chance
        if (!actor->m_properties.allow_read_chance(Verbose::yes)) {
                msg_log::add(crumble_str);

                TRACE_FUNC_END;

                return ConsumeItem::yes;
        }

        // OK, we are fully allowed to read the scroll - cast the spell

        const std::unique_ptr<const Spell> spell(make_spell());

        const SpellId id = spell->id();

        const auto skill = player_skill_for_scroll(id);

        const auto seen_foes = actor::seen_foes(*map::g_player);

        spell->cast(
                map::g_player,
                skill,
                SpellSrc::manuscript,
                seen_foes);

        msg_log::add(crumble_str);

        identify(Verbose::yes);

        // Learn and recall spell.
        if (spell->player_can_learn()) {
                player_spells::learn_spell(id, Verbose::yes);
                player_spells::recall_spell(id);
        }

        TRACE_FUNC_END;

        return ConsumeItem::yes;
}

Spell* Scroll::make_spell() const
{
        return spells::make(m_data->spell_cast_from_scroll);
}

void Scroll::identify(const Verbose verbose)
{
        if (m_data->is_identified) {
                return;
        }

        m_data->is_identified = true;

        if (verbose == Verbose::yes) {
                const std::string name_after =
                        name(
                                ItemNameType::a,
                                ItemNameInfo::none);

                msg_log::add("我已确定 " + name_after + ".");

                game::add_history_event("已鉴定 " + name_after);
        }
}

std::string Scroll::domain_str() const
{
        const std::unique_ptr<const Spell> spell(make_spell());

        const SpellDomain domain = spell->domain();

        if (domain == SpellDomain::END) {
                return "";
        }

        return spells::spell_domain_title(domain);
}

std::string Scroll::name_info_str(const ItemNameIdentified id_type) const
{
        if ((m_data->is_spell_domain_known && !m_data->is_identified) ||
            (id_type == ItemNameIdentified::force_identified)) {
                const std::string str = domain_str();

                if (!str.empty()) {
                        return "(" + str + ")";
                }
        }

        return "";
}

}  // namespace scroll
