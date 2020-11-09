/* $Id$
 *
 * ruffina, 2004
 */
#include "room.h"
#include "affecthandler.h"
#include "affect.h"
#include "character.h"
#include "core/object.h"
#include "feniamanager.h"

#include "dreamland.h"
#include "roombehaviormanager.h"
#include "loadsave.h"
#include "mercdb.h"
#include "merc.h"
#include "def.h"

WEARLOC(none);

extra_exit_data * extra_exit_data::create()
{
    EXTRA_EXIT_DATA *peexit = new EXTRA_EXIT_DATA;

    peexit->description = str_dup(description);
    peexit->exit_info_default = peexit->exit_info = exit_info_default;
    peexit->key = key;
    peexit->u1.vnum = u1.vnum;
    peexit->level = level;

    peexit->keyword = str_dup(keyword);
    peexit->short_desc_from = str_dup(short_desc_from);
    peexit->short_desc_to = str_dup(short_desc_to);
    peexit->room_description = str_dup(room_description);
    peexit->max_size_pass = max_size_pass;

    peexit->moving_from = moving_from;
    peexit->moving_mode_from = moving_mode_from;
    peexit->moving_to = moving_to;
    peexit->moving_mode_to = moving_mode_to;

    return peexit;
}        

exit_data *exit_data::create()
{
    EXIT_DATA *pexit = (EXIT_DATA*)alloc_perm(sizeof(EXIT_DATA));

    pexit->keyword = str_dup(keyword);
    pexit->short_descr = str_dup(short_descr);
    pexit->description = str_dup(description);
    pexit->exit_info_default = pexit->exit_info = exit_info_default;
    pexit->key = key;
    pexit->u1.vnum = u1.vnum; 
    pexit->level = 0;

    return pexit;
}

Room * RoomIndexData::create()
{
    LogStream::sendNotice() << "Creating instance of room " << vnum << endl;
    if (room) // FIXME allow multiple instances
        throw Exception("Attempt to create second instance of a room.");

    room = new Room;
    
    room->area = area;
    room->extra_descr = extra_descr;
    room->name = name;
    room->description = description;
    room->vnum = vnum;
    room->room_flags = room_flags;
    room->sector_type = sector_type;
    room->heal_rate = heal_rate;
    room->mana_rate = mana_rate;
    room->pIndexData = this;    
    room->setID( dreamland->genID( ) );    

    RoomBehaviorManager::assign(room);

    for (int i = 0; i < DIR_SOMEWHERE; i++)
        if (exit[i])
            room->exit[i] = exit[i]->create();

    for (EXTRA_EXIT_DATA *eexit = extra_exit; eexit; eexit = eexit->next) {
        EXTRA_EXIT_DATA *newExit = eexit->create();
        newExit->next = room->extra_exit;
        room->extra_exit = newExit;
    }

    room->rnext = room_list;
    room_list = room;

    room->area->rooms[vnum] = room;

    if (FeniaManager::wrapperManager)
        FeniaManager::wrapperManager->linkWrapper( room );    

    return room;
}


/*
 * Apply or remove an affect to a room.
 */
void Room::affectModify( Affect *paf, bool fAdd )
{
    int mod;

    mod = paf->modifier;

    if ( fAdd )
    {
        switch (paf->where)
        {
        case TO_ROOM_AFFECTS:
              SET_BIT(affected_by, paf->bitvector);
            break;
        case TO_ROOM_FLAGS:
              SET_BIT(room_flags, paf->bitvector);
            break;
        case TO_ROOM_CONST:
            break;
        }
    }
    else
    {
        switch (paf->where)
        {
        case TO_ROOM_AFFECTS:
              REMOVE_BIT(affected_by, paf->bitvector);
            break;
        case TO_ROOM_FLAGS:
              REMOVE_BIT(room_flags, paf->bitvector);
            break;
        case TO_ROOM_CONST:
            break;
        }
        mod = 0 - mod;
    }

    switch ( paf->location )
    {
    default:
        bug( "Affect_modify_room: unknown location %d.", paf->location );
        return;

    case APPLY_ROOM_NONE:                                        break;
    case APPLY_ROOM_HEAL:        heal_rate += mod;                break;
    case APPLY_ROOM_MANA:        mana_rate += mod;                break;
    }
}

/*
 * Give an affect to a room.
 */
void Room::affectTo( Affect *paf )
{
    Affect *paf_new;
    Room *pRoomIndex;

    if (!affected) {
        if (top_affected_room) {
            for ( pRoomIndex  = top_affected_room; pRoomIndex->aff_next != 0; pRoomIndex  = pRoomIndex->aff_next )
                continue;
                
            pRoomIndex->aff_next = this;        
        }
        else 
            top_affected_room = this;

        aff_next = 0;
    }

    paf_new = dallocate( Affect );

    *paf_new                = *paf;
    paf_new->next        = affected;
    affected        = paf_new;

    affectModify( paf_new, true );
}

void Room::affectCheck( int where, int vector )
{
    Affect *paf;

    if (vector == 0)
        return;

    for (paf = affected; paf != 0; paf = paf->next)
        if (paf->where == where && paf->bitvector == vector)
        {
            switch (where)
            {
                case TO_ROOM_AFFECTS:
                      SET_BIT(affected_by,vector);
                    break;
                case TO_ROOM_FLAGS:
                            SET_BIT(room_flags, vector);
                        break;
                case TO_ROOM_CONST:
                    break;
            }
            return;
        }
}

/*
 * Remove an affect from a room.
 */
void Room::affectRemove( Affect *paf )
{
    int where;
    int vector;

    if ( affected == 0 )
    {
        bug( "Affect_remove_room: no affect.", 0 );
        return;
    }

    affectModify( paf, false );
    where = paf->where;
    vector = paf->bitvector;

    if (paf == affected) {
        affected        = paf->next;
    }
    else {
        Affect *prev;

        for (prev = affected; prev != 0; prev = prev->next)
            if (prev->next == paf) {
                prev->next = paf->next;
                break;
            }

        if (prev == 0) {
            bug( "Affect_remove_room: cannot find paf.", 0 );
            return;
        }
    }

    if (!affected) {
        Room *prev;

        if (top_affected_room  == this)
            top_affected_room = aff_next;
        else {
            for(prev = top_affected_room; prev->aff_next; prev = prev->aff_next )
                if ( prev->aff_next == this ) {
                    prev->aff_next = aff_next;
                    break;
                }

            if ( prev == 0 ) {
                bug( "Affect_remove_room: cannot find room.", 0 );
                return;
            }
        }
        
        aff_next = 0;
     }

    ddeallocate( paf );

    affectCheck( where, vector );
}

/*
 * Strip all affects of a given sn.
 */
void Room::affectStrip( int sn )
{
    Affect *paf;
    Affect *paf_next;

    for (paf = affected; paf != 0; paf = paf_next )
    {
        paf_next = paf->next;
        if ( paf->type == sn )
            affectRemove( paf );
    }
}


/*
 * Add or enhance an affect.
 */
void Room::affectJoin( Affect *paf )
{
    Affect *paf_old;

    for ( paf_old = affected; paf_old != 0; paf_old = paf_old->next )
    {
        if ( paf_old->type == paf->type )
        {
            paf->level += paf_old->level;
            paf->level /= 2;
            paf->duration += paf_old->duration;
            paf->modifier += paf_old->modifier;
            affectRemove( paf_old );
            break;
        }
    }

    affectTo( paf );
    return;
}


/*
 * Return true if a room is affected by a spell.
 */
bool Room::isAffected( int sn ) const
{
    Affect *paf;

    for ( paf = affected; paf != 0; paf = paf->next )
    {
        if ( paf->type == sn )
            return true;
    }

    return false;
}

/** Whether an item can contribute to the light in the room. */
static bool is_lantern(Object *obj)
{
    if (obj->item_type != ITEM_LIGHT)
        return false;
        
    // Expired lanterns don't count.
    if (obj->value2() == 0)
        return false;

    // Worn item is ok.
    if (obj->carried_by)
        return obj->wear_loc != wear_none;

    // Item on the floor ok if it's a permanent fixture.
    if (obj->in_room)
        return !IS_SET(obj->wear_flags, ITEM_TAKE);

    return false;
}

/** Recalculate light in the room when an object is worn or destroyed. */
void Room::updateLight()
{
    light = 0;

    for (Character *rch = people; rch; rch = rch->next_in_room)
        for (Object *obj = rch->carrying; obj; obj = obj->next_content)
            if (is_lantern(obj))
                light++;

    for (Object *obj = contents; obj; obj = obj->next_content)
        if (is_lantern(obj))
            light++;
}
