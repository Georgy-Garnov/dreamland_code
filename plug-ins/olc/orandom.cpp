#include "olc.h"
#include "olcflags.h"
#include "olcstate.h"
#include "security.h"
#include "argparser.h"
#include "core/object.h"
#include "weapons.h"
#include "math_utils.h"
#include "comm.h"
#include "interp.h"
#include "loadsave.h"
#include "mercdb.h"
#include "act.h"
#include "def.h"

namespace pegtl = TAO_PEGTL_NAMESPACE;

namespace TAO_PEGTL_NAMESPACE::mud {
    struct MyArgs : public args_level, public args_tier, public args_wclass {};

    struct level   : seq< one<'l'>, level_value > {};
    struct tier    : seq< one<'t'>, tier_value > {};
    struct anything: sor< level, tier, wclass > {};
    struct grammar : must< list_must< anything, spaces >, eof > {};
}

CMD(orandom, 50, "орандом", POS_DEAD, 103, LOG_ALWAYS, 
        "Random weapon generator.")
{
    pegtl::mud::MyArgs myargs = { -1, -1, -1 };

    if (!parse_input<pegtl::mud::grammar, pegtl::mud::MyArgs>(ch, argument, myargs)) {
        ch->println("Формат: {Worandom{x <weapon class> {Wl{x<level> {Wt{x<tier>");
        return;
    }

    ostringstream buf;
    int minLevel = myargs.level == -1 ? 1 : myargs.level;
    int maxLevel = myargs.level == -1 ? MAX_LEVEL : myargs.level;
    int minTier  = myargs.tier == -1  ? 1 : myargs.tier;
    int maxTier  = myargs.tier == -1  ? 5 : myargs.tier;
    bitnumber_t minFlag  = myargs.wclass == -1 ? 0 : myargs.wclass;
    bitnumber_t maxFlag  = myargs.wclass == -1 ? WEAPON_MAX-1 : myargs.wclass;
    bool create = myargs.level != -1 && myargs.tier != -1 && myargs.wclass != -1;

    if (create) {
        ch->printf("{WСоздаю оружие типа '%s' уровня %d и крутости %d.{x\r\n", 
            weapon_class.message(myargs.wclass).c_str(), myargs.level, myargs.tier);

        Object *obj = create_object(get_obj_index(104), 0);
        obj->value0(myargs.wclass);
        obj->level = myargs.level;
        obj->setShortDescr(str_empty);
        obj_to_char(obj, ch);

        WeaponGenerator()
            .item(obj)
            .hitrollTier(myargs.tier)
            .damrollTier(myargs.tier)
            .valueTier(myargs.tier)
            .assignHitroll()
            .assignDamroll()
            .assignValues()
            .assignNames()
            .assignDamageType();

        interpret_fmt(ch, "stat obj %lld", obj->getID());
        return;
    }

    buf << dlprintf("{C%15s{x {WLVL  V1  V2  AVE  REAL  DR{x\r\n", "");

    for (bitnumber_t f = minFlag; f <= maxFlag; f++) {
        for (int t = minTier; t <= maxTier; t++) {
            buf << dlprintf("{CTier %1d: {y%7s{x  ", t, weapon_class.name(f).c_str());
            for (int l = minLevel; l <= maxLevel; l++) {
                int v2 = weapon_value2(f);
                int v1 = weapon_value1(l, t, v2, f);
                int ave = weapon_ave(l, t, f);
                int real_ave = dice_ave(v1, v2);
                int dr = weapon_damroll(l, t, f);

                if (l != minLevel)
                    buf << dlprintf("%15s ", "");
                buf << dlprintf("{C%3d{w  %2d  %2d  %3d   %3d  %2d\r\n", l, v1, v2, ave, real_ave, dr);
            }
        }
    }

    page_to_char(buf.str().c_str(), ch);
}

