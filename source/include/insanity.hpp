// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef INSANITY_HPP
#define INSANITY_HPP

#include <string>
#include <vector>

namespace actor
{
class Actor;
}  // namespace actor

enum class InsSymptId
{
        reduce_xp,
        scream,
        babbling,
        faint,
        laugh,
        phobia_rat,
        phobia_spider,
        phobia_reptile_and_amph,
        phobia_canine,
        phobia_dead,
        phobia_deep,
        phobia_dark,
        sadism,
        shadows,
        paranoia,  // Invisible stalker spawned
        confusion,
        frenzy,
        strange_sensation,
        END
};

enum class InsSymptType
{
        phobia,
        misc
};

class InsSympt
{
public:
        InsSympt() = default;

        virtual ~InsSympt() = default;

        virtual InsSymptId id() const = 0;

        virtual InsSymptType type() const = 0;

        virtual void save() const {}

        virtual void load() {}

        virtual bool is_permanent() const = 0;

        virtual bool is_allowed() const
        {
                return true;
        }

        void on_start();

        void on_end();

        virtual void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors)
        {
                (void)seen_actors;
        }

        virtual void on_permanent_rfear() {}

        virtual std::string char_descr_msg() const
        {
                return "";
        }

        virtual std::string game_over_summary_msg() const
        {
                return "";
        }

protected:
        virtual void on_start_hook() {}

        virtual std::string start_msg() const = 0;
        virtual std::string start_heading() const = 0;

        virtual std::string end_msg() const
        {
                return "";
        }

        virtual std::string history_msg() const = 0;

        virtual std::string history_msg_end() const
        {
                return "";
        }
};

class InsReduceXp : public InsSympt
{
public:
        InsReduceXp() = default;

        InsSymptId id() const override
        {
                return InsSymptId::reduce_xp;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override;

        std::string start_heading() const override
        {
                return "经验已被抹去！";
        }

        std::string history_msg() const override
        {
                return "记忆中的经验被抹去了";
        }
};

class InsScream : public InsSympt
{
public:
        InsScream() = default;

        InsSymptId id() const override
        {
                return InsSymptId::scream;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override;

        std::string start_heading() const override
        {
                return "尖叫！";
        }

        std::string history_msg() const override
        {
                return "惊恐地尖叫了";
        }
};

class InsBabbling : public InsSympt
{
public:
        InsBabbling() = default;

        InsSymptId id() const override
        {
                return InsSymptId::babbling;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors) override;

        std::string char_descr_msg() const override
        {
                return "胡言乱语";
        }

        std::string game_over_summary_msg() const override
        {
                return "有喋喋不休的倾向";
        }

        void babble() const;

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "我发现自己在语无伦次地喃喃自语。";
        }

        std::string start_heading() const override
        {
                return "胡言乱语！";
        }

        std::string end_msg() const override
        {
                return "我觉得自己能控制言语。";
        }

        std::string history_msg() const override
        {
                return "开始语无伦次地喃喃自语";
        }

        std::string history_msg_end() const override
        {
                return "我的怪异喃喃自语治愈了";
        }
};

class InsFaint : public InsSympt
{
public:
        InsFaint() = default;

        InsSymptId id() const override
        {
                return InsSymptId::faint;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "眼前的一切渐渐陷入黑暗。";
        }

        std::string start_heading() const override
        {
                return "即将昏厥！";
        }

        std::string history_msg() const override
        {
                return "昏厥";
        }
};

class InsLaugh : public InsSympt
{
public:
        InsLaugh() = default;

        InsSymptId id() const override
        {
                return InsSymptId::laugh;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "我发出疯狂的笑声。";
        }

        std::string start_heading() const override
        {
                return "哈哈哈！";
        }

        std::string history_msg() const override
        {
                return "疯狂地大笑";
        }
};

class InsPhobiaRat : public InsSympt
{
public:
        InsPhobiaRat() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_rat;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "恐鼠症";
        }

        std::string game_over_summary_msg() const override
        {
                return "曾患有恐鼠症";
        }

protected:
        std::string start_msg() const override
        {
                return "老鼠突然显得十分可怕。";
        }

        std::string start_heading() const override
        {
                return "恐鼠症！";
        }

        std::string end_msg() const override
        {
                return "我不再害怕老鼠了。";
        }

        std::string history_msg() const override
        {
                return "患上了恐鼠症";
        }

        std::string history_msg_end() const override
        {
                return "我的恐鼠症治愈了";
        }
};

class InsPhobiaSpider : public InsSympt
{
public:
        InsPhobiaSpider() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_spider;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "蜘蛛恐惧症";
        }

        std::string game_over_summary_msg() const override
        {
                return "曾患有蜘蛛恐惧症";
        }

protected:
        std::string start_msg() const override
        {
                return "蜘蛛突然显得十分可怕。";
        }

        std::string start_heading() const override
        {
                return "蜘蛛恐惧症！";
        }

        std::string end_msg() const override
        {
                return "我不再害怕蜘蛛了。";
        }

        std::string history_msg() const override
        {
                return "患上了蜘蛛恐惧症";
        }

        std::string history_msg_end() const override
        {
                return "我的蜘蛛恐惧症治愈了";
        }
};

class InsPhobiaReptileAndAmph : public InsSympt
{
public:
        InsPhobiaReptileAndAmph() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_reptile_and_amph;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "爬行动物与两栖动物恐惧症";
        }

        std::string game_over_summary_msg() const override
        {
                return "曾患有爬行动物与两栖动物恐惧症";
        }

protected:
        std::string start_msg() const override
        {
                return "爬行动物与两栖动物突然显得十分可怕。";
        }

        std::string start_heading() const override
        {
                return "爬虫恐惧症！";
        }

        std::string end_msg() const override
        {
                return "我不再害怕爬行动物和两栖动物了。";
        }

        std::string history_msg() const override
        {
                return "患上了爬行动物与两栖动物恐惧症";
        }

        std::string history_msg_end() const override
        {
                return "我的爬行动物与两栖动物恐惧症治愈了";
        }
};

class InsPhobiaCanine : public InsSympt
{
public:
        InsPhobiaCanine() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_canine;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "恐犬症";
        }

        std::string game_over_summary_msg() const override
        {
                return "曾患有恐犬症";
        }

protected:
        std::string start_msg() const override
        {
                return "犬类突然显得令人恐惧。";
        }

        std::string start_heading() const override
        {
                return "恐犬症！";
        }

        std::string end_msg() const override
        {
                return "我不再害怕犬类了。";
        }

        std::string history_msg() const override
        {
                return "患上了恐犬症";
        }

        std::string history_msg_end() const override
        {
                return "我的恐犬症治愈了";
        }
};

class InsPhobiaDead : public InsSympt
{
public:
        InsPhobiaDead() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_dead;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "尸骸恐惧症";
        }

        std::string game_over_summary_msg() const override
        {
                return "曾患有尸骸恐惧症";
        }

protected:
        std::string start_msg() const override
        {
                return "死者骤然显得更加可怖。";
        }

        std::string start_heading() const override
        {
                return "恐尸症！";
        }

        std::string end_msg() const override
        {
                return "我不再害怕尸骸了。";
        }

        std::string history_msg() const override
        {
                return "患上了尸骸恐惧症";
        }

        std::string history_msg_end() const override
        {
                return "我的尸骸恐惧症治愈了";
        }
};

class InsPhobiaDeep : public InsSympt
{
public:
        InsPhobiaDeep() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_deep;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "幽闭深渊恐惧症";
        }

        std::string game_over_summary_msg() const override
        {
                return "曾患有幽闭深渊恐惧症";
        }

protected:
        std::string start_msg() const override
        {
                return "深入下方似乎突然变得可怕得多。";
        }

        std::string start_heading() const override
        {
                return "幽闭恐惧症！";
        }

        std::string end_msg() const override
        {
                return "我不再害怕幽闭深渊了。";
        }

        std::string history_msg() const override
        {
                return "患上了幽闭深渊恐惧症";
        }

        std::string history_msg_end() const override
        {
                return "我的幽闭深渊恐惧症治愈了";
        }
};

class InsPhobiaDark : public InsSympt
{
public:
        InsPhobiaDark() = default;

        InsSymptId id() const override
        {
                return InsSymptId::phobia_dark;
        }

        InsSymptType type() const override
        {
                return InsSymptType::phobia;
        }

        bool is_permanent() const override
        {
                return true;
        }

        void on_new_player_turn(
                const std::vector<actor::Actor*>& seen_actors) override;

        void on_permanent_rfear() override;

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "黑暗恐惧症";
        }

        std::string game_over_summary_msg() const override
        {
                return "曾患有黑暗恐惧症";
        }

protected:
        std::string start_msg() const override
        {
                return "黑暗骤然变得更加可怖。";
        }

        std::string start_heading() const override
        {
                return "恐黑症！";
        }

        std::string end_msg() const override
        {
                return "我不再害怕黑暗了。";
        }

        std::string history_msg() const override
        {
                return "患上了黑暗恐惧症";
        }

        std::string history_msg_end() const override
        {
                return "我的黑暗恐惧症治愈了";
        }
};

class InsSadism : public InsSympt
{
public:
        InsSadism() = default;

        InsSymptId id() const override
        {
                return InsSymptId::sadism;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return true;
        }

        bool is_allowed() const override;

        std::string char_descr_msg() const override
        {
                return "虐待狂式的执念";
        }

        std::string game_over_summary_msg() const override
        {
                return "曾有虐待狂式的执念";
        }

protected:
        std::string start_msg() const override
        {
                return "令我惊恐的是，我发现自己竟从造成他人痛苦中得到鼓舞。每夺去一个重要生命，我都会感到一丝宽慰。然而，我堕落的心智永远无法获得彻底平静。";
        }

        std::string start_heading() const override
        {
                return "虐待狂式的执念！";
        }

        std::string end_msg() const override
        {
                return "我的虐待狂执念治愈了。";
        }

        std::string history_msg() const override
        {
                return "患上了虐待狂式的执念";
        }

        std::string history_msg_end() const override
        {
                return "我的虐待狂执念治愈了";
        }
};

class InsShadows : public InsSympt
{
public:
        InsShadows() = default;

        InsSymptId id() const override
        {
                return InsSymptId::shadows;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "阴影正向我逼近！";
        }

        std::string start_heading() const override
        {
                return "阴影缠身！";
        }

        std::string history_msg() const override
        {
                return "被阴影纠缠";
        }
};

class InsParanoia : public InsSympt
{
public:
        InsParanoia() = default;

        InsSymptId id() const override
        {
                return InsSymptId::paranoia;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "有人在跟踪我吗？还是恐慌正在吞噬我？";
        }

        std::string start_heading() const override
        {
                return "偏执狂！";
        }

        std::string history_msg() const override
        {
                return "强烈感觉有人在跟踪我";
        }
};

class InsConfusion : public InsSympt
{
public:
        InsConfusion() = default;

        InsSymptId id() const override
        {
                return InsSymptId::confusion;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "我发现自己陷入一种奇异的恍惚。我努力回想自己身在何处、正在发生什么。";
        }

        std::string start_heading() const override
        {
                return "困惑！";
        }

        std::string history_msg() const override
        {
                return "毫无缘由地突然感到极度困惑";
        }
};

class InsFrenzy : public InsSympt
{
public:
        InsFrenzy() = default;

        InsSymptId id() const override
        {
                return InsSymptId::frenzy;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

        bool is_allowed() const override;

protected:
        void on_start_hook() override;

        std::string start_msg() const override
        {
                return "我陷入无法控制的狂怒！";
        }

        std::string start_heading() const override
        {
                return "狂暴！";
        }

        std::string history_msg() const override
        {
                return "陷入无法控制的狂怒";
        }
};

class InsStrangeSensation : public InsSympt
{
public:
        InsStrangeSensation() = default;

        InsSymptId id() const override
        {
                return InsSymptId::strange_sensation;
        }

        InsSymptType type() const override
        {
                return InsSymptType::misc;
        }

        bool is_permanent() const override
        {
                return false;
        }

protected:
        std::string start_msg() const override
        {
                return "我感到一阵奇痒，仿佛有什么东西正在脖颈后面爬动。";
        }

        std::string start_heading() const override
        {
                return "奇异的感觉！";
        }

        std::string history_msg() const override
        {
                return "感觉有什么东西在我的脖子上爬";
        }
};

namespace insanity
{
void init();
void cleanup();

void save();
void load();

void run_sympt();

bool has_sympt(InsSymptId id);

bool has_sympt_type(InsSymptType type);

std::vector<const InsSympt*> active_sympts();

void on_new_player_turn(const std::vector<actor::Actor*>& seen_actors);

void on_permanent_rfear();

void end_sympt(InsSymptId id);

}  // namespace insanity

#endif  // INSANITY_HPP
