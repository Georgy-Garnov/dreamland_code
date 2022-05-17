
/* $Id: group_fightmaster.cpp,v 1.1.2.24.6.20 2010-09-01 21:20:45 rufina Exp $
 *
 * ruffina, 2004
 */
/***************************************************************************
 * Все права на этот код 'Dream Land' пренадлежат Igor {Leo} и Olga {Varda}*
 * Некоторую помощь в написании этого кода, а также своими идеями помогали:*
 *    Igor S. Petrenko     {NoFate, Demogorgon}                            *
 *    Koval Nazar          {Nazar, Redrum}                                 *
 *    Doropey Vladimir     {Reorx}                                         *
 *    Kulgeyko Denis       {Burzum}                                        *
 *    Andreyanov Aleksandr {Manwe}                                         *
 *    и все остальные, кто советовал и играл в этот MUD                    *
 ***************************************************************************/

#include "logstream.h"
#include "skill.h"
#include "skillcommandtemplate.h"
#include "skillmanager.h"

#include "act_move.h"
#include "affect.h"
#include "commonattributes.h"

#include "mercdb.h"
#include "npcharacter.h"
#include "object.h"
#include "pcharacter.h"
#include "race.h"
#include "room.h"

#include "act.h"
#include "clanreference.h"
#include "damage.h"
#include "def.h"
#include "fight.h"
#include "handler.h"
#include "interp.h"
#include "magic.h"
#include "material.h"
#include "merc.h"
#include "mercdb.h"
#include "morphology.h"
#include "onehit.h"
#include "onehit_weapon.h"
#include "skill_utils.h"
#include "vnum.h"

GSN(area_attack);
GSN(bash);
GSN(bash_door);
GSN(cavalry);
GSN(crush);
GSN(double_kick);
GSN(kick);
GSN(protective_shield);
GSN(smash);

CLAN(shalafi);
PROF(anti_paladin);
PROF(samurai);

/*
 * 'bash door' skill command
 */

SKILL_RUNP(bashdoor)
{
    char arg[MAX_INPUT_LENGTH];
    Character *gch;
    int chance = 0;
    EXTRA_EXIT_DATA *peexit = 0;
    int damage_bash, door = 0;
    Room *room = ch->in_room;

    one_argument(argument, arg);

    if (!gsn_bash_door->getEffective(ch)) {
        ch->pecho("Выбить дверь? Как это?");
        return;
    }

    if (MOUNTED(ch)) {
        ch->pecho("Только не верхом!");
        return;
    }

    if (RIDDEN(ch)) {
        ch->pecho("Ты не можешь выбить дверь, когда оседлан{Sfа{Sx.");
        return;
    }

    if (arg[0] == '\0') {
        ch->pecho("Выбить дверь в каком направлении?");
        return;
    }

    if (ch->fighting) {
        ch->pecho("Сначала закончи сражение.");
        return;
    }

    peexit = room->extra_exits.find(arg);
    if ((!peexit || !ch->can_see(peexit)) && (door = find_exit(ch, arg, FEX_NO_INVIS | FEX_DOOR | FEX_NO_EMPTY)) < 0) {
        ch->pecho("Но тут нечего выбивать!");
        return;
    }

    int slevel = skill_level(*gsn_bash_door, ch);

    /* look for guards */
    for (gch = room->people; gch; gch = gch->next_in_room) {
        if (gch->is_npc() && IS_AWAKE(gch) && slevel + 5 < gch->getModifyLevel()) {
            oldact("$C1 стоит слишком близко к двери.", ch, 0, gch, TO_CHAR);
            return;
        }
    }

    // 'bash door'
    EXIT_DATA *pexit = 0;
    EXIT_DATA *pexit_rev = 0;
    int exit_info;

    if (peexit != 0) {
        door = DIR_SOMEWHERE;
        exit_info = peexit->exit_info;
    } else {
        pexit = room->exit[door];
        exit_info = pexit->exit_info;
    }

    if (!IS_SET(exit_info, EX_CLOSED)) {
        ch->pecho("Здесь уже открыто.");
        return;
    }

    if (!IS_SET(exit_info, EX_LOCKED)) {
        ch->pecho("Просто попробуй открыть.");
        return;
    }

    if (IS_SET(exit_info, EX_NOPASS) && !IS_SET(exit_info, EX_BASH_ONLY)) {
        ch->pecho("Эту дверь невозможно вышибить.");
        return;
    }

    /* modifiers */

    /* size  and weight */
    chance += ch->getCarryWeight() / 100;

    chance += (ch->size - 2) * 20;

    /* stats */
    chance += ch->getCurrStat(STAT_STR);

    if (is_flying(ch))
        chance -= 10;

    /* level
        chance += ch->getModifyLevel() / 10;
        */

    chance += (gsn_bash_door->getEffective(ch) - 90 + skill_level_bonus(*gsn_bash_door, ch));
    const char *doorname = peexit ? peexit->short_desc_from : direction_doorname(pexit);
    oldact("Ты бьешь в $N4, пытаясь выбить!", ch, 0, doorname, TO_CHAR);
    oldact("$c1 бьет в $N4, пытаясь выбить!", ch, 0, doorname, TO_ROOM);

    if (room->isDark() && !IS_AFFECTED(ch, AFF_INFRARED))
        chance /= 2;

    chance = URANGE(3, chance, 98);

    /* now the attack */
    if (number_percent() < chance) {
        gsn_bash_door->improve(ch, true);

        if (peexit != 0) {
            REMOVE_BIT(peexit->exit_info, EX_LOCKED);
            REMOVE_BIT(peexit->exit_info, EX_CLOSED);
            oldact("$c1 выбивает дверь.", ch, 0, 0, TO_ROOM);
            oldact("Ты выбиваешь дверь!", ch, 0, 0, TO_CHAR);
        } else {
            REMOVE_BIT(pexit->exit_info, EX_LOCKED);
            REMOVE_BIT(pexit->exit_info, EX_CLOSED);
            oldact("$c1 выбивает дверь.", ch, 0, 0, TO_ROOM);
            oldact("Ты выбиваешь дверь!", ch, 0, 0, TO_CHAR);

            /* open the other side */
            if ((pexit_rev = direction_reverse(room, door))) {
                REMOVE_BIT(pexit_rev->exit_info, EX_CLOSED);
                REMOVE_BIT(pexit_rev->exit_info, EX_LOCKED);
                direction_target(room, door)->echo(POS_RESTING, "%^N1 с грохотом вылетает.", doorname);
            }
        }

        ch->setWait(gsn_bash_door->getBeats(ch));
    } else {
        oldact("Обессилев, ты падаешь лицом вниз!", ch, 0, 0, TO_CHAR);
        oldact("Обессилев, $c1 упа$gло|л|ла лицом вниз.", ch, 0, 0, TO_ROOM);
        gsn_bash_door->improve(ch, false);
        ch->position = POS_RESTING;
        ch->setWait(gsn_bash_door->getBeats(ch) * 3 / 2);
        damage_bash = ch->damroll + number_range(4, 4 + 4 * ch->size + chance / 5);
        damage(ch, ch, damage_bash, gsn_bash_door, DAM_BASH, true, DAMF_WEAPON);
        if (IS_CHARMED(ch) && ch->master->getPC()) {
            DLString petName = Syntax::noun(ch->getNameP('1'));
            ch->master->pecho(fmt(0, "%1$^C1 упа%1$Gло|л|ла и не может ходить и выполнять некоторые команды. Напиши {y{hc{lRприказать %2$s встать{lEorder %2$s stand{x, если хочешь продолжить выбивать %1$Gим|им|ей двери.", ch, petName.c_str()));
        }
    }

    return;
}

/*
 * 'bash' skill command
 */
SKILL_RUNP(bash)
{
    char arg[MAX_INPUT_LENGTH];
    Character *victim;
    int wait;
    bool FightingCheck;

    if ((MOUNTED(ch)) && (!gsn_cavalry->available(ch))) {
        ch->pecho("Ты не знаешь, как применять такие навыки верхом.");
        return;
    }

    if (ch->fighting != 0)
        FightingCheck = true;
    else
        FightingCheck = false;

    argument = one_argument(argument, arg);

    if (arg[0] != '\0' && !str_cmp(arg, "door")) {
        interpret_fmt(ch, "bashdoor %s", argument);
        return;
    }

    if (gsn_bash->getEffective(ch) <= 1) {
        ch->pecho("Ударить щитом? Но как это сделать?");
        return;
    }

    if (arg[0] == '\0') {
        victim = ch->fighting;
        if (victim == 0) {
            ch->pecho("Сейчас ты не сражаешься!");
            return;
        }
    } else if ((victim = get_char_room(ch, arg)) == 0) {
        ch->pecho("Этого нет здесь.");
        return;
    }

    if ((victim->position < POS_FIGHTING) && (FightingCheck)) {
        oldact("Подожди пока $E встанет.", ch, 0, victim, TO_CHAR);
        return;
    }

    if (victim == ch) {
        ch->pecho("Ударить щитом себя?");
        return;
    }

    if (get_eq_char(ch, wear_shield) == 0) {
        ch->pecho("Тебе нужен щит чтобы сделать это!");
        if (IS_CHARMED(ch) && ch->master->getPC() && ch->canCarryNumber() > 0)
            ch->master->pecho("Для этого умения твоему последователю потребуется надеть щит.");
        return;
    }

    if (is_safe(ch, victim))
        return;

    if (IS_CHARMED(ch) && ch->master == victim) {
        oldact("Но $C1 твой друг!!!", ch, 0, victim, TO_CHAR);
        return;
    }

    if (SHADOW(ch)) {
        ch->pecho("Ты безуспешно пытаешься сбить с ног свою тень.");
        oldact("$c1 бьет щитом свою тень.", ch, 0, 0, TO_ROOM);
        return;
    }

    if (!ch->is_npc() && !ch->move) {
        ch->pecho("Ты слишком уста%Gло|л|ла для этого.", ch);
        return;
    }

    if (MOUNTED(victim)) {
        if (victim->mount->is_npc() && gsn_cavalry->available(victim)) {
            ch->pecho("Ты не можешь сбить с ног того, кто верхом!");
            return;
        }

        interpret_raw(victim, "dismount");
    }

    ch->move -= move_dec(ch);

    if (victim->isAffected(gsn_protective_shield)) {
        oldact_p("{YТы пытаешься сбить с ног $C4, но что-то тебе мешает сделать это.{x",
                 ch, 0, victim, TO_CHAR, POS_FIGHTING);
        oldact_p("{Y$c1 пытается сбить тебя с ног, но твоя защита мешает сделать это.{x",
                 ch, 0, victim, TO_VICT, POS_FIGHTING);
        oldact_p("{Y$c1 пытается сбить с ног $C4, но что-то мешает сделать это.{x",
                 ch, 0, victim, TO_NOTVICT, POS_FIGHTING);

        ch->setWait(gsn_bash->getBeats(ch));
        return;
    }

    int chance;
    // Calculate success chance.
    {
        chance = gsn_bash->getEffective(ch);

        /* modifiers */
        chance = chance * 4 / 5;

        /* size  and weight */
        chance += min(ch->canCarryWeight(), ch->carry_weight) / 25;
        chance -= min(victim->canCarryWeight(), victim->carry_weight) / 20;

        if (ch->size < victim->size)
            chance += (ch->size - victim->size) * 25;
        else
            chance += (ch->size - victim->size) * 10;

        /* stats */
        chance += ch->getCurrStat(STAT_STR);
        chance -= victim->getCurrStat(STAT_DEX) * 4 / 3;

        /* speed */
        if (IS_QUICK(ch))
            chance += 10;
        if (IS_QUICK(victim))
            chance -= 20;

        /* level */
        chance += (skill_level(*gsn_bash, ch) - victim->getModifyLevel()) * 2;

        if (is_flying(victim))
            chance -= 10;

        if (ch->getProfession() == prof_anti_paladin && ch->getClan() == clan_shalafi)
            chance /= 2;
    }

    /* now the attack */

    if (number_percent() < chance) {
        if (number_percent() < 50) {
            oldact_p("Сильнейшим ударом щита $c1 сбивает тебя с ног и ты падаешь!",
                     ch, 0, victim, TO_VICT, POS_RESTING);
            oldact("Ты сбиваешь $C4 с ног ударом щита!", ch, 0, victim, TO_CHAR);
            oldact_p("$c1 сильнейшим ударом щита сбивает $C4 с ног.",
                     ch, 0, victim, TO_NOTVICT, POS_RESTING);

            wait = 3;

            switch (number_bits(2)) {
            case 0:
                wait = 1;
                break;
            case 1:
                wait = 2;
                break;
            case 2:
                wait = 4;
                break;
            case 3:
                wait = 3;
                break;
            }

            victim->setWaitViolence(wait);
            victim->position = POS_RESTING;
        } else {
            oldact("$c1 наносит тебе удар щитом!", ch, 0, victim, TO_VICT);
            oldact("Ты наносишь удар щитом $C3!", ch, 0, victim, TO_CHAR);
            oldact("$c1 наносит удар щитом $C3.", ch, 0, victim, TO_NOTVICT);
        }

        gsn_bash->improve(ch, true, victim);
        ch->setWait(gsn_bash->getBeats(ch));

        // Calculate bash damage.
        int dam = (ch->damroll / 2) + number_range(4, 4 + 4 * ch->size + chance / 10);

        if (is_flying(victim))
            dam += dam / 3;

        damapply_class(ch, dam);

        try {
            damage_nocatch(ch, victim, dam, gsn_bash, DAM_BASH, true, DAMF_WEAPON);
        } catch (const VictimDeathException &e) {
            return;
        }

    } else {
        if (number_percent() < 50) {
            oldact("Ты промахиваешься и падаешь!", ch, 0, victim, TO_CHAR);
            oldact("$c1 промахивается и падает.", ch, 0, victim, TO_NOTVICT);
            oldact_p("$c1 пытается сбить тебя с ног, но промахивается и падает.",
                     ch, 0, victim, TO_VICT, POS_RESTING);
            ch->position = POS_RESTING;
            ch->setWait(gsn_bash->getBeats(ch) * 3 / 2);
        } else {
            oldact_p("Ты промахиваешься и едва не падаешь!",
                     ch, 0, victim, TO_CHAR, POS_RESTING);
            oldact_p("$c1 промахивается и едва не падает.",
                     ch, 0, victim, TO_NOTVICT, POS_RESTING);
            oldact_p("$c1 пытается сбить тебя с ног, но промахивается и едва не падает.",
                     ch, 0, victim, TO_VICT, POS_RESTING);
        }

        damage(ch, victim, 0, gsn_bash, DAM_BASH, true, DAMF_WEAPON);
        gsn_bash->improve(ch, false, victim);
        ch->setWait(gsn_bash->getBeats(ch) * 1 / 2);
    }

    if (!FightingCheck)
        yell_panic(ch, victim,
                   "Помогите! Кто-то бьет меня щитом!",
                   "Помогите! %1$^C1 бьет меня щитом!");
}


/*
 * 'kick' skill command
 */
SKILL_RUNP(kick)
{
    Character *victim;
    int chance;
    char arg[MAX_INPUT_LENGTH];
    bool FightingCheck;

    if (gsn_kick->getEffective(ch) <= 1) {
        ch->pecho("У тебя плохая растяжка.");
        return;
    }

    if (MOUNTED(ch)) {
        ch->pecho("Ты не можешь ударить ногой, если ты верхом!");
        return;
    }

    if (ch->fighting != 0)
        FightingCheck = true;
    else
        FightingCheck = false;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0') {
        victim = ch->fighting;
        if (victim == 0) {
            ch->pecho("Сейчас ты не сражаешься!");
            return;
        }
    } else if ((victim = get_char_room(ch, arg)) == 0) {
        ch->pecho("Этого нет здесь.");
        return;
    }

    if (victim == ch) {
        ch->pecho("Ударить себя ногой? Довольно тяжело...");
        return;
    }

    if (is_safe(ch, victim)) {
        return;
    }

    if (IS_CHARMED(ch) && ch->master == victim) {
        oldact("Но $C1 твой друг!!!", ch, 0, victim, TO_CHAR);
        return;
    }

    if (!ch->is_npc() && !ch->move) {
        ch->pecho("Ты слишком уста%Gло|л|ла для этого.", ch);
        return;
    } else
        ch->move -= move_dec(ch);

    if (SHADOW(ch)) {
        ch->pecho("Твоя нога вязнет в твоей тени...");
        oldact_p("$c1 выделывает балетные па перед своей тенью.",
                 ch, 0, 0, TO_ROOM, POS_RESTING);
        return;
    }

    chance = number_percent();

    if (is_flying(ch))
        chance = (int)(chance * 1.1);

    ch->setWait(gsn_kick->getBeats(ch));

    if (chance < gsn_kick->getEffective(ch)) {
        gsn_kick->improve(ch, true, victim);

        Object *on_feet;
        int dam = number_range(1, ch->getModifyLevel());

        if ((ch->getProfession() == prof_samurai) && IS_SET(ch->parts, PART_FEET) && ((on_feet = get_eq_char(ch, wear_feet)) == 0 || (on_feet != 0 && !material_is_typed(on_feet, MAT_METAL)))) {
            dam *= 2;
        }

        dam += ch->damroll / 2;
        damapply_class(ch, dam);

        //10% extra damage for every skill level
        dam += dam * skill_level_bonus(*gsn_kick, ch) / 10;

        if (IS_SET(ch->parts, PART_TWO_HOOVES))
            dam = 3 * dam / 2;
        else if (IS_SET(ch->parts, PART_FOUR_HOOVES))
            dam *= 2;

        try {
            damage_nocatch(ch, victim, dam, gsn_kick, DAM_BASH, true, DAMF_WEAPON);
        } catch (const VictimDeathException &) {
            return;
        }

        if (number_percent() < (gsn_double_kick->getEffective(ch) * 8) / 10) {
            gsn_double_kick->improve(ch, true, victim);

            Object *on_feet;
            int dam = number_range(1, ch->getModifyLevel());

            if ((ch->getProfession() == prof_samurai) && IS_SET(ch->parts, PART_FEET) && ((on_feet = get_eq_char(ch, wear_feet)) == 0 || (on_feet != 0 && !material_is_typed(on_feet, MAT_METAL)))) {
                dam *= 2;
            }

            dam += ch->damroll / 2;
            damapply_class(ch, dam);

            //10% extra damage for every skill level
            dam += dam * skill_level_bonus(*gsn_double_kick, ch) / 10;

            if (IS_SET(ch->parts, PART_TWO_HOOVES))
                dam = 3 * dam / 2;
            else if (IS_SET(ch->parts, PART_FOUR_HOOVES))
                dam *= 2;

            try {
                damage_nocatch(ch, victim, dam, gsn_double_kick, DAM_BASH, true, DAMF_WEAPON);
            } catch (const VictimDeathException &) {
                return;
            }
        }

    } else {
        damage(ch, victim, 0, gsn_kick, DAM_BASH, true, DAMF_WEAPON);
        gsn_kick->improve(ch, false, victim);
    }

    if (!FightingCheck) {
        if (IS_SET(ch->parts, PART_TWO_HOOVES | PART_FOUR_HOOVES))
            yell_panic(ch, victim,
                       "Помогите! Кто-то ударил меня копытом!",
                       "Помогите! %1$^C1 удари%1$Gло|л|ла меня копытом!");
        else
            yell_panic(ch, victim,
                       "Помогите! Кто-то ударил меня ногой!",
                       "Помогите! %1$^C1 удари%1$Gло|л|ла меня ногой!");
    }
}


/*
 * 'crush' skill command
 */

SKILL_RUNP(crush)
{
    Character *victim;
    int chance = 100, wait = 0;
    int damage_crush;

    if (MOUNTED(ch))
        return;

    if (!gsn_crush->usable(ch))
        return;

    if ((victim = ch->fighting) == 0)
        return;

    if (victim->position < POS_FIGHTING)
        return;

    if (is_safe(ch, victim))
        return;

    if (IS_CHARMED(ch) && ch->master == victim)
        return;

    if (SHADOW(ch)) {
        return;
    }

    if (victim->isAffected(gsn_protective_shield)) {
        oldact("{YТвой мощный удар как будто соскальзывает c $C2, не причиняя вреда.",
               ch, 0, victim, TO_CHAR);
        oldact("{YМощный удар $c2 скользит по поверхности твоего охранного щита.{x",
               ch, 0, victim, TO_VICT);
        oldact("{YМощный удар $c2 как будто соскальзывает с $C2.{x",
               ch, 0, victim, TO_NOTVICT);

        ch->setWait(gsn_crush->getBeats(ch));
        return;
    }

    /* modifiers */

    /* size  and weight */
    chance += min(ch->canCarryWeight(), ch->carry_weight) / 25;
    chance -= min(victim->canCarryWeight(), victim->carry_weight) / 20;

    if (ch->size < victim->size)
        chance += (ch->size - victim->size) * 25;
    else
        chance += (ch->size - victim->size) * 10;

    /* stats */
    chance += ch->getCurrStat(STAT_STR);
    chance -= victim->getCurrStat(STAT_DEX) * 4 / 3;

    if (is_flying(ch))
        chance -= 10;

    /* speed */
    if (IS_QUICK(ch))
        chance += 10;
    if (IS_QUICK(victim))
        chance -= 20;

    /* level */
    chance += (ch->getModifyLevel() - victim->getModifyLevel()) * 2;

    /* now the attack */
    if (number_percent() < chance) {
        oldact_p("$c1 сбивает тебя с ног мощнейшим ударом!",
                 ch, 0, victim, TO_VICT, POS_RESTING);
        oldact("Ты бросаешься на $C4, и сбиваешь $S с ног!", ch, 0, victim, TO_CHAR);
        oldact_p("$c1 сбивает $C4 с ног мощнейшим ударом.",
                 ch, 0, victim, TO_NOTVICT, POS_RESTING);

        wait = 3;

        switch (number_bits(2)) {
        case 0:
            wait = 1;
            break;
        case 1:
            wait = 2;
            break;
        case 2:
            wait = 4;
            break;
        case 3:
            wait = 3;
            break;
        }

        victim->setWaitViolence(wait);
        ch->setWait(gsn_crush->getBeats(ch));
        victim->position = POS_RESTING;
        damage_crush = (ch->damroll / 2) + number_range(4, 4 + 4 * ch->size + chance / 10);
        damage(ch, victim, damage_crush, gsn_crush, DAM_BASH, true, DAMF_WEAPON);
    } else {
        damage(ch, victim, 0, gsn_crush, DAM_BASH, true, DAMF_WEAPON);
        oldact("Ты промахиваешься и падаешь!", ch, 0, victim, TO_CHAR);
        oldact("$c1 делает резкое движение и падает.", ch, 0, victim, TO_NOTVICT);
        oldact_p("$c1 пытается сбить тебя с ног, но ты делаешь шаг в сторону, и $e падает!",
                 ch, 0, victim, TO_VICT, POS_RESTING);
        ch->position = POS_RESTING;
        ch->setWait(gsn_crush->getBeats(ch) * 3 / 2);
    }
}



/*
 * 'smash' skill command
 */
SKILL_RUNP(smash)
{
    char arg[MAX_INPUT_LENGTH];
    Character *victim;
    int wait;
    bool FightingCheck;

    if (MOUNTED(ch)) {
        ch->pecho("Ты не можешь сбить с ног, если ты верхом!");
        return;
    }

    if (ch->fighting != NULL)
        FightingCheck = true;
    else
        FightingCheck = false;

    argument = one_argument(argument, arg);

    if (gsn_smash->getEffective(ch) <= 1) {
        ch->pecho("Сбить с ног? Но как это сделать?");
        return;
    }

    if (arg[0] == '\0') {
        victim = ch->fighting;
        if (victim == NULL) {
            ch->pecho("Сейчас ты не сражаешься!");
            return;
        }
    } else if ((victim = get_char_room(ch, arg)) == NULL) {
        ch->pecho("Этого нет здесь.");
        return;
    }

    if (victim->position < POS_FIGHTING) {
        oldact("Подожди пока $E встанет.", ch, NULL, victim, TO_CHAR);
        return;
    }

    if (victim == ch) {
        ch->pecho("Сбить с ног себя??? Не получится...");
        return;
    }

    if (is_safe(ch, victim))
        return;

    if (IS_CHARMED(ch) && ch->master == victim) {
        oldact("Но $C1 твой друг!!!", ch, NULL, victim, TO_CHAR);
        return;
    }

    if (!ch->is_npc() && !ch->move) {
        oldact("Ты слишком уста$gло|л|ла для этого.", ch, 0, 0, TO_CHAR);
        return;
    }

    if (SHADOW(ch)) {
        ch->pecho("Ты безуспешно пытаешься бороться со своей тенью.");
        oldact("$c1 обнимается со своей тенью.", ch, NULL, NULL, TO_ROOM);
        return;
    }

    if (MOUNTED(victim)) {
        if (victim->mount->is_npc() && gsn_cavalry->available(victim)) {
            ch->pecho("Ты не можешь сбить с ног того, кто верхом!");
            return;
        }

        interpret_raw(victim, "dismount");
    }

    ch->move -= move_dec(ch);

    if (victim->isAffected(gsn_protective_shield)) {
        oldact_p("{YТы пытаешься сбить с ног $C4, но что-то тебе мешает сделать это.{x",
                 ch, NULL, victim, TO_CHAR, POS_FIGHTING);
        oldact_p("{Y$c1 пытается сбить тебя с ног, но твоя защита мешает сделать это.{x",
                 ch, NULL, victim, TO_VICT, POS_FIGHTING);
        oldact_p("{Y$c1 пытается сбить с ног $C4, но что-то мешает сделать это.{x",
                 ch, NULL, victim, TO_NOTVICT, POS_FIGHTING);

        ch->setWait(gsn_smash->getBeats(ch));
        return;
    }

    int chance;
    // Calculate smash chance.
    {
        chance = gsn_smash->getEffective(ch);
        /* modifiers */
        chance = chance * 4 / 5;

        /* size  and weight */
        chance += min(ch->canCarryWeight(), ch->carry_weight) / 25;
        chance -= min(victim->canCarryWeight(), victim->carry_weight) / 20;

        if (ch->size < victim->size)
            chance += (ch->size - victim->size) * 25;
        else
            chance += (ch->size - victim->size) * 10;

        /* stats */
        chance += ch->getCurrStat(STAT_STR);
        chance -= victim->getCurrStat(STAT_DEX) * 4 / 3;

        if (is_flying(ch))
            chance -= 10;

        /* speed */
        if (IS_QUICK(ch))
            chance += 10;
        if (IS_QUICK(victim))
            chance -= 20;

        /* level */
        chance += skill_level(*gsn_smash, ch) - victim->getModifyLevel();
    }

    /* now the attack */
    if (number_percent() < chance) {
        oldact_p("Сильнейшим ударом $c1 сбивает тебя с ног и ты падаешь на землю!",
                 ch, NULL, victim, TO_VICT, POS_RESTING);
        oldact_p("Ты сбиваешь $C4 с ног, посылая $S на землю!",
                 ch, NULL, victim, TO_CHAR, POS_RESTING);
        oldact_p("$c1 сильнейшим ударом сбивает $C4 с ног.",
                 ch, NULL, victim, TO_NOTVICT, POS_RESTING);
        gsn_smash->improve(ch, true, victim);

        if (!FightingCheck)
            yell_panic(ch, victim,
                       "Помогите! Кто-то сбил меня с ног!",
                       "Помогите! %1$^C1 сбивает меня с ног!");

        wait = 3;

        switch (number_bits(2)) {
        case 0:
            wait = 1;
            break;
        case 1:
            wait = 2;
            break;
        case 2:
            wait = 4;
            break;
        case 3:
            wait = 3;
            break;
        }

        victim->setWaitViolence(wait);
        ch->setWait(gsn_smash->getBeats(ch));

        // Calculate smash damage.
        int dam = (ch->damroll / 2) + number_range(4, 4 + 5 * ch->size + chance / 10);
        damapply_class(ch, dam);

        try {
            
            damage_nocatch(ch, victim, dam, gsn_smash, DAM_BASH, true, DAMF_WEAPON); 

            if (number_percent() < gsn_smash->getEffective(ch) - 40) {
                if (number_percent() > 30)
                    victim->position = POS_SITTING;
                else
                    victim->position = POS_RESTING;
            }
        } catch (const VictimDeathException &) {
            return;
        }

    } else {
        damage(ch, victim, 0, gsn_smash, DAM_BASH, true, DAMF_WEAPON);
        oldact_p("Ты промахиваешься и падаешь лицом на пол!",
                 ch, NULL, victim, TO_CHAR, POS_RESTING);
        oldact_p("$c1 промахивается и падает лицом на пол.",
                 ch, NULL, victim, TO_NOTVICT, POS_RESTING);
        oldact_p("$c1 пытается ударить тебя, но промахивается и падает на пол.",
                 ch, NULL, victim, TO_VICT, POS_RESTING);
        gsn_smash->improve(ch, false, victim);

        if (number_percent() > 5)
            ch->position = POS_SITTING;
        else
            ch->position = POS_RESTING;
        ch->setWait(gsn_smash->getBeats(ch) * 3 / 2);
    }
}

/*
 * 'area attack' skill command
 */
SKILL_DECL(areaattack);
SKILL_APPLY(areaattack)
{
    int count = 0, max_count;
    Character *vch, *vch_next;

    if (number_percent() >= gsn_area_attack->getEffective(ch))
        return false;

    gsn_area_attack->improve(ch, true, victim);

    int slevel = skill_level(*gsn_area_attack, ch);

    if (slevel < 70)
        max_count = 1;
    else if (slevel < 80)
        max_count = 2;
    else if (slevel < 90)
        max_count = 3;
    else
        max_count = 4;

    for (vch = ch->in_room->people; vch != 0; vch = vch_next) {
        vch_next = vch->next_in_room;
        if (vch != victim && vch->fighting == ch) {
            one_hit_nocatch(ch, vch);
            count++;
        }
        if (count == max_count)
            break;
    }

    return true;
}
