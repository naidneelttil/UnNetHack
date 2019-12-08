/*  SCCS Id: @(#)do_name.c  3.4 2003/01/14  */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

STATIC_DCL char *NDECL(nextmbuf);
static void FDECL(getpos_help, (BOOLEAN_P, const char *));
static void call_object(int, char *);
static void call_input(int, char *);

extern const char what_is_an_unknown_object[];      /* from pager.c */

#define NUMMBUF 5

/* manage a pool of BUFSZ buffers, so callers don't have to */
STATIC_OVL char *
nextmbuf()
{
    static char NEARDATA bufs[NUMMBUF][BUFSZ];
    static int bufidx = 0;

    bufidx = (bufidx + 1) % NUMMBUF;
    return bufs[bufidx];
}

/* the response for '?' help request in getpos() */
static void
getpos_help(force, goal)
boolean force;
const char *goal;
{
    char sbuf[BUFSZ];
    boolean doing_what_is;
    winid tmpwin = create_nhwindow(NHW_MENU);

    Sprintf(sbuf, "Use [%s] to move the cursor to %s.",
            iflags.num_pad ? "2468" : "hjkl", goal);
    putstr(tmpwin, 0, sbuf);
    putstr(tmpwin, 0, "Use [HJKL] to move the cursor 8 units at a time.");
    putstr(tmpwin, 0, "Or enter a background symbol (ex. <).");
    /* disgusting hack; the alternate selection characters work for any
       getpos call, but they only matter for dowhatis (and doquickwhatis) */
    putstr(tmpwin, 0, "Use m and M to select a monster.");
    putstr(tmpwin, 0, "Use @ to select yourself.");
    doing_what_is = (goal == what_is_an_unknown_object);
    Sprintf(sbuf, "Type a .%s when you are at the right place.",
            doing_what_is ? " or , or ; or :" : "");
    putstr(tmpwin, 0, sbuf);
    if (!force)
        putstr(tmpwin, 0, "Type Space or Escape when you're done.");
    putstr(tmpwin, 0, "");
    display_nhwindow(tmpwin, TRUE);
    destroy_nhwindow(tmpwin);
}

struct _getpos_monarr {
    coord pos;
    long du;
};
static int getpos_monarr_len = 0;
static int getpos_monarr_idx = 0;
static struct _getpos_monarr *getpos_monarr_pos = NULL;

void
getpos_freemons()
{
    if (getpos_monarr_pos) free(getpos_monarr_pos);
    getpos_monarr_pos = NULL;
    getpos_monarr_len = 0;
}

static int
getpos_monarr_cmp(a, b)
const void *a;
const void *b;
{
    const struct _getpos_monarr *m1 = (const struct _getpos_monarr *)a;
    const struct _getpos_monarr *m2 = (const struct _getpos_monarr *)b;
    return (m1->du - m2->du);
}

void
getpos_initmons()
{
    struct monst *mtmp = fmon;
    if (getpos_monarr_pos) getpos_freemons();
    while (mtmp) {
        if (!DEADMONSTER(mtmp) && canspotmon(mtmp)) getpos_monarr_len++;
        mtmp = mtmp->nmon;
    }
    if (getpos_monarr_len) {
        int idx = 0;
        getpos_monarr_pos = (struct _getpos_monarr *)malloc(sizeof(struct _getpos_monarr) * getpos_monarr_len);
        mtmp = fmon;
        while (mtmp) {
            if (!DEADMONSTER(mtmp) && canspotmon(mtmp)) {
                getpos_monarr_pos[idx].pos.x = mtmp->mx;
                getpos_monarr_pos[idx].pos.y = mtmp->my;
                getpos_monarr_pos[idx].du = distu(mtmp->mx, mtmp->my);
                idx++;
            }
            mtmp = mtmp->nmon;
        }
        qsort(getpos_monarr_pos, getpos_monarr_len, sizeof(struct _getpos_monarr), getpos_monarr_cmp);
    }
}

struct monst *
getpos_nextmon()
{
    if (!getpos_monarr_pos) {
        getpos_initmons();
        if (getpos_monarr_len < 1) return NULL;
        getpos_monarr_idx = -1;
    }
    if (getpos_monarr_idx >= -1 && getpos_monarr_idx < getpos_monarr_len) {
        struct monst *mon;
        getpos_monarr_idx = (getpos_monarr_idx + 1) % getpos_monarr_len;
        mon = m_at(getpos_monarr_pos[getpos_monarr_idx].pos.x,
                   getpos_monarr_pos[getpos_monarr_idx].pos.y);
        return mon;
    }
    return NULL;
}

struct monst *
getpos_prevmon()
{
    if (!getpos_monarr_pos) {
        getpos_initmons();
        if (getpos_monarr_len < 1) return NULL;
        getpos_monarr_idx = getpos_monarr_len;
    }
    if (getpos_monarr_idx >= 0 && getpos_monarr_idx <= getpos_monarr_len) {
        struct monst *mon;
        getpos_monarr_idx = (getpos_monarr_idx - 1);
        if (getpos_monarr_idx < 0) getpos_monarr_idx = getpos_monarr_len - 1;
        mon = m_at(getpos_monarr_pos[getpos_monarr_idx].pos.x,
                   getpos_monarr_pos[getpos_monarr_idx].pos.y);
        return mon;
    }
    return NULL;
}


int
getpos(cc, force, goal)
coord *cc;
boolean force;
const char *goal;
{
    int result = 0;
    int cx, cy, i, c;
    int sidx, tx, ty;
    boolean msg_given = TRUE;   /* clear message window by default */
    static const char pick_chars[] = ".,;:";
    const char *cp;
    const char *sdp;
    if(iflags.num_pad) sdp = ndir; else sdp = sdir; /* DICE workaround */

    if (flags.verbose) {
        pline("(For instructions type a ?)");
        msg_given = TRUE;
    }
    cx = cc->x;
    cy = cc->y;
#ifdef CLIPPING
    cliparound(cx, cy);
#endif
    curs(WIN_MAP, cx, cy);
    flush_screen(0);
#ifdef MAC
    lock_mouse_cursor(TRUE);
#endif
    for (;;) {
        c = nh_poskey(&tx, &ty, &sidx);
        if (c == '\033') {
            cx = cy = -10;
            msg_given = TRUE; /* force clear */
            result = -1;
            break;
        }
        if(c == 0) {
            if (!isok(tx, ty)) continue;
            /* a mouse click event, just assign and return */
            cx = tx;
            cy = ty;
            break;
        }
        if ((cp = index(pick_chars, c)) != 0) {
            /* '.' => 0, ',' => 1, ';' => 2, ':' => 3 */
            result = cp - pick_chars;
            break;
        }
        for (i = 0; i < 8; i++) {
            int dx, dy;

            if (sdp[i] == c) {
                /* a normal movement letter or digit */
                dx = xdir[i];
                dy = ydir[i];
            } else if (sdir[i] == lowc((char)c)) {
                /* a shifted movement letter */
                dx = 8 * xdir[i];
                dy = 8 * ydir[i];
            } else
                continue;

            /* truncate at map edge; diagonal moves complicate this... */
            if (cx + dx < 1) {
                dy -= sgn(dy) * (1 - (cx + dx));
                dx = 1 - cx; /* so that (cx+dx == 1) */
            } else if (cx + dx > COLNO-1) {
                dy += sgn(dy) * ((COLNO-1) - (cx + dx));
                dx = (COLNO-1) - cx;
            }
            if (cy + dy < 0) {
                dx -= sgn(dx) * (0 - (cy + dy));
                dy = 0 - cy; /* so that (cy+dy == 0) */
            } else if (cy + dy > ROWNO-1) {
                dx += sgn(dx) * ((ROWNO-1) - (cy + dy));
                dy = (ROWNO-1) - cy;
            }
            cx += dx;
            cy += dy;
            goto nxtc;
        }

        if(c == '?') {
            getpos_help(force, goal);
        } else if (c == 'm' || c == 'M') {
            struct monst *tmpmon = (c == 'm') ? getpos_nextmon() : getpos_prevmon();
            if (tmpmon) {
                cx = tmpmon->mx;
                cy = tmpmon->my;
                goto nxtc;
            }
        } else if (c == '@') {
            cx = u.ux;
            cy = u.uy;
            goto nxtc;
        } else {
            if (!index(quitchars, c)) {
                char matching[MAXPCHARS];
                int pass, lo_x, lo_y, hi_x, hi_y, k = 0;
                (void)memset((genericptr_t)matching, 0, sizeof matching);
                for (sidx = 1; sidx < MAXPCHARS; sidx++)
                    if (c == defsyms[sidx].sym || c == (int)showsyms[sidx])
                        matching[sidx] = (char) ++k;
                if (k) {
                    for (pass = 0; pass <= 1; pass++) {
                        /* pass 0: just past current pos to lower right;
                           pass 1: upper left corner to current pos */
                        lo_y = (pass == 0) ? cy : 0;
                        hi_y = (pass == 0) ? ROWNO - 1 : cy;
                        for (ty = lo_y; ty <= hi_y; ty++) {
                            lo_x = (pass == 0 && ty == lo_y) ? cx + 1 : 1;
                            hi_x = (pass == 1 && ty == hi_y) ? cx : COLNO - 1;
                            for (tx = lo_x; tx <= hi_x; tx++) {
                                /* look at dungeon feature, not at user-visible glyph */
                                k = back_to_glyph(tx, ty);
                                /* uninteresting background glyph */
                                if (glyph_is_cmap(k) &&
                                    (IS_DOOR(levl[tx][ty].typ) || /* monsters mimicking a door */
                                     glyph_to_cmap(k) == S_darkroom ||
                                     glyph_to_cmap(k) == S_room ||
                                     glyph_to_cmap(k) == S_corr ||
                                     glyph_to_cmap(k) == S_litcorr)) {
                                    /* what the user remembers to be at tx,ty */
                                    k = glyph_at(tx, ty);
                                }
                                /* TODO: - open doors are only matched with '-' */
                                /* should remembered or seen items be matched? */
                                if (glyph_is_cmap(k) &&
                                    matching[glyph_to_cmap(k)] &&
                                    levl[tx][ty].seenv && /* only if already seen */
                                    (!IS_WALL(levl[tx][ty].typ) &&
                                     (levl[tx][ty].typ != SDOOR) &&
                                     glyph_to_cmap(k) != S_room &&
                                     glyph_to_cmap(k) != S_corr &&
                                     glyph_to_cmap(k) != S_litcorr)
                                    ) {
                                    cx = tx,  cy = ty;
                                    if (msg_given) {
                                        clear_nhwindow(WIN_MESSAGE);
                                        msg_given = FALSE;
                                    }
                                    goto nxtc;
                                }
                            } /* column */
                        } /* row */
                    } /* pass */
                    pline("Can't find dungeon feature '%c'.", c);
                    msg_given = TRUE;
                    goto nxtc;
                } else {
                    pline("Unknown direction: '%s' (%s).",
                          visctrl((char)c),
                          !force ? "aborted" :
                          iflags.num_pad ? "use 2468 or ." : "use hjkl or .");
                    msg_given = TRUE;
                } /* k => matching */
            } /* !quitchars */
            if (force) goto nxtc;
            pline("Done.");
            msg_given = FALSE; /* suppress clear */
            cx = -1;
            cy = 0;
            result = 0; /* not -1 */
            break;
        }
nxtc:   ;
#ifdef CLIPPING
        cliparound(cx, cy);
#endif
        curs(WIN_MAP, cx, cy);
        flush_screen(0);
    }
#ifdef MAC
    lock_mouse_cursor(FALSE);
#endif
    if (msg_given) clear_nhwindow(WIN_MESSAGE);
    cc->x = cx;
    cc->y = cy;
    getpos_freemons();
    return result;
}

/* allocate space for a monster's name; removes old name if there is one */
void
new_mname(mon, lth)
struct monst *mon;
int lth; /* desired length (caller handles adding 1 for terminator) */
{
    if (lth) {
        /* allocate mextra if necessary; otherwise get rid of old name */
        if (!mon->mextra) {
            mon->mextra = newmextra();
        } else {
            free_mname(mon); /* already has mextra, might also have name */
        }
        MNAME(mon) = (char *) alloc((unsigned) lth);
    } else {
        /* zero length: the new name is empty; get rid of the old name */
        if (has_mname(mon))
            free_mname(mon);
    }
}

/* release a monster's name; retains mextra even if all fields are now null */
void
free_mname(mon)
struct monst *mon;
{
    if (has_mname(mon)) {
        free((genericptr_t) MNAME(mon));
        MNAME(mon) = (char *) 0;
    }
}

/* allocate space for an object's name; removes old name if there is one */
void
new_oname(obj, lth)
struct obj *obj;
int lth; /* desired length (caller handles adding 1 for terminator) */
{
    if (lth) {
        /* allocate oextra if necessary; otherwise get rid of old name */
        if (!obj->oextra)
            obj->oextra = newoextra();
        else
            free_oname(obj); /* already has oextra, might also have name */
        ONAME(obj) = (char *) alloc((unsigned) lth);
    } else {
        /* zero length: the new name is empty; get rid of the old name */
        if (has_oname(obj))
            free_oname(obj);
    }
}

/* release an object's name; retains oextra even if all fields are now null */
void
free_oname(obj)
struct obj *obj;
{
    if (has_oname(obj)) {
        free((genericptr_t) ONAME(obj));
        ONAME(obj) = (char *) 0;
    }
}

/*  safe_oname() always returns a valid pointer to
 *  a string, either the pointer to an object's name
 *  if it has one, or a pointer to an empty string
 *  if it doesn't.
 */
const char *
safe_oname(obj)
struct obj *obj;
{
    if (has_oname(obj)) {
        return ONAME(obj);
    }
    return "";
}

/* historical note: this returns a monster pointer because it used to
   allocate a new bigger block of memory to hold the monster and its name */
struct monst *
christen_monst(mtmp, name)
struct monst *mtmp;
const char *name;
{
    int lth;
    char buf[PL_PSIZ];

    /* dogname & catname are PL_PSIZ arrays; object names have same limit */
    lth = (name && *name) ? ((int) strlen(name) + 1) : 0;
    if(lth > PL_PSIZ) {
        lth = PL_PSIZ;
        name = strncpy(buf, name, PL_PSIZ - 1);
        buf[PL_PSIZ - 1] = '\0';
    }
    new_mname(mtmp, lth); /* removes old name if one is present */
    if (lth) {
        Strcpy(MNAME(mtmp), name);
    }
    return mtmp;
}

int
do_mname()
{
    char buf[BUFSZ];
    coord cc;
    register int cx, cy;
    register struct monst *mtmp;
    char qbuf[QBUFSZ];

    if (Hallucination) {
        You("would never recognize it anyway.");
        return 0;
    }
    cc.x = u.ux;
    cc.y = u.uy;
    if (getpos(&cc, FALSE, "the monster you want to name") < 0 ||
        (cx = cc.x) < 0)
        return 0;
    cy = cc.y;

    if (cx == u.ux && cy == u.uy) {
#ifdef STEED
        if (u.usteed && canspotmon(u.usteed))
            mtmp = u.usteed;
        else {
#endif
        pline("This %s creature is called %s and cannot be renamed.",
              beautiful(),
              plname);
        return(0);
#ifdef STEED
    }
#endif
    } else
        mtmp = m_at(cx, cy);

    if (!mtmp || (!sensemon(mtmp) &&
                  (!(cansee(cx, cy) || see_with_infrared(mtmp)) || mtmp->mundetected
                   || mtmp->m_ap_type == M_AP_FURNITURE
                   || mtmp->m_ap_type == M_AP_OBJECT
                   || (mtmp->minvis && !See_invisible)))) {
        pline("I see no monster there.");
        return(0);
    }
    /* special case similar to the one in lookat() */
    (void) distant_monnam(mtmp, ARTICLE_THE, buf);
    Sprintf(qbuf, "What do you want to call %s?", buf);
    getlin(qbuf, buf);
    if(!*buf || *buf == '\033') return(0);
    /* strip leading and trailing spaces; unnames monster if all spaces */
    (void)mungspaces(buf);

    if (mtmp->data->geno & G_UNIQ)
        pline("%s doesn't like being called names!", Monnam(mtmp));
    else
        (void) christen_monst(mtmp, buf);
    return(0);
}

STATIC_VAR int via_naming = 0;

/*
 * This routine changes the address of obj. Be careful not to call it
 * when there might be pointers around in unknown places. For now: only
 * when obj is in the inventory.
 */
void
do_oname(obj)
register struct obj *obj;
{
    char *bufp, buf[BUFSZ], bufcpy[BUFSZ], qbuf[QBUFSZ];
    const char *aname;
    short objtyp;

#ifdef SPE_NOVEL
    /* Do this now because there's no point in even asking for a name */
    if (obj->otyp == SPE_NOVEL) {
        pline("%s already has a published name.", Ysimple_name2(obj));
        return;
    }
#endif

    Sprintf(qbuf, "What do you want to name %s ",
            is_plural(obj) ? "these" : "this");
    (void) safe_qbuf(qbuf, qbuf, "?", obj, xname, simpleonames, "item");
    buf[0] = '\0';
#ifdef EDIT_GETLIN
    /* if there's an existing name, make it be the default answer */
    if (has_oname(obj)) {
        Strcpy(buf, ONAME(obj));
    }
#endif
    getlin(qbuf, buf);
    if (!*buf || *buf == '\033') {
        return;
    }

    /* strip leading and trailing spaces; unnames item if all spaces */
    (void)mungspaces(buf);

    /*
     * We don't violate illiteracy conduct here, although it is
     * arguable that we should for anything other than "X".  Doing so
     * would make attaching player's notes to hero's inventory have an
     * in-game effect, which may or may not be the correct thing to do.
     *
     * We do violate illiteracy in oname() if player creates Sting or
     * Orcrist, clearly being literate (no pun intended...).
     */

    /* relax restrictions over proper capitalization for artifacts */
    if ((aname = artifact_name(buf, &objtyp)) != 0 && objtyp == obj->otyp)
        Strcpy(buf, aname);

    if (obj->oartifact) {
        pline_The("artifact seems to resist the attempt.");
        return;
    } else if (restrict_name(obj, buf, FALSE) || exist_artifact(obj->otyp, buf)) {
        int n = rn2((int)strlen(buf));
        register char c1, c2;

        c1 = lowc(buf[n]);
        do c2 = 'a' + rn2('z'-'a'); while (c1 == c2);
        buf[n] = (buf[n] == c1) ? c2 : highc(c2);  /* keep same case */
        pline("While engraving your %s slips.", body_part(HAND));
        display_nhwindow(WIN_MESSAGE, FALSE);
        You("engrave: \"%s\".", buf);
    }
    ++via_naming; /* This ought to be an argument rather than a static... */
    obj = oname(obj, buf);
    ++via_naming; /* This ought to be an argument rather than a static... */
}

struct obj *
oname(obj, name)
struct obj *obj;
const char *name;
{
    int lth;
    char buf[PL_PSIZ];

    lth = *name ? (int)(strlen(name) + 1) : 0;
    if (lth > PL_PSIZ) {
        lth = PL_PSIZ;
        name = strncpy(buf, name, PL_PSIZ - 1);
        buf[PL_PSIZ - 1] = '\0';
    }
    /* If named artifact exists in the game, do not create another.
     * Also trying to create an artifact shouldn't de-artifact
     * it (e.g. Excalibur from prayer). In this case the object
     * will retain its current name. */
    if (obj->oartifact || (lth && exist_artifact(obj->otyp, name))) {
        return obj;
    }

    new_oname(obj, lth); /* removes old name if one is present */
    if (lth) {
        Strcpy(ONAME(obj), name);
    }
    if (lth) {
        artifact_exists(obj, name, TRUE);
    }
    if (obj->oartifact) {
        /* can't dual-wield with artifact as secondary weapon */
        if (obj == uswapwep) {
            untwoweapon();
        }
        /* activate warning if you've just named your weapon "Sting" */
        if (obj == uwep) {
            set_artifact_intrinsic(obj, TRUE, W_WEP);
        }
        /* if obj is owned by a shop, increase your bill */
        if (obj->unpaid) {
            alter_cost(obj, 0L);
        }
        if (via_naming) {
            /* violate illiteracy conduct since successfully wrote arti-name */
            violated(CONDUCT_ILLITERACY);
        }
    }
    if (carried(obj)) {
        update_inventory();
    }
    return obj;
}

static NEARDATA const char callable[] = {
    SCROLL_CLASS, POTION_CLASS, WAND_CLASS,  RING_CLASS, AMULET_CLASS,
    GEM_CLASS,    SPBOOK_CLASS, ARMOR_CLASS, TOOL_CLASS, 0
};

int
ddocall()
{
    register struct obj *obj;
#ifdef REDO
    char ch;
#endif
    char allowall[2];

    switch(
#ifdef REDO
        ch =
#endif
        ynq("Name an individual object?")) {
    case 'q':
        break;
    case 'y':
#ifdef REDO
        savech(ch);
#endif
        allowall[0] = ALL_CLASSES; allowall[1] = '\0';
        obj = getobj(allowall, "name");
        if(obj) do_oname(obj);
        break;
    default:
#ifdef REDO
        savech(ch);
#endif
        obj = getobj(callable, "call");
        if (obj) {
            /* behave as if examining it in inventory;
               this might set dknown if it was picked up
               while blind and the hero can now see */
            (void) xname(obj);

            if (!obj->dknown) {
                You("would never recognize another one.");
                return 0;
            }
            docall(obj);
        }
        break;
    }
    return 0;
}

/* for use by safe_qbuf() */
STATIC_PTR char *
docall_xname(obj)
struct obj *obj;
{
    struct obj otemp;

    otemp = *obj;
    otemp.oextra = (struct oextra *) 0;
    otemp.quan = 1L;
    /* in case water is already known, convert "[un]holy water" to "water" */
    otemp.blessed = otemp.cursed = 0;
    /* remove attributes that are doname() caliber but get formatted
       by xname(); most of these fixups aren't really needed because the
       relevant type of object isn't callable so won't reach this far */
    if (otemp.oclass == WEAPON_CLASS)
        otemp.opoisoned = 0; /* not poisoned */
    else if (otemp.oclass == POTION_CLASS)
        otemp.odiluted = 0; /* not diluted */
    else if (otemp.otyp == TOWEL || otemp.otyp == STATUE)
        otemp.spe = 0; /* not wet or historic */
    else if (otemp.otyp == TIN)
        otemp.known = 0; /* suppress tin type (homemade, &c) and mon type */
    else if (otemp.otyp == FIGURINE)
        otemp.corpsenm = NON_PM; /* suppress mon type */
    else if (otemp.otyp == HEAVY_IRON_BALL)
        otemp.owt = objects[HEAVY_IRON_BALL].oc_weight; /* not "very heavy" */
    else if (otemp.oclass == FOOD_CLASS && otemp.globby)
        otemp.owt = 120; /* 6*20, neither a small glob nor a large one */

    return an(xname(&otemp));
}

void
docall(obj)
struct obj *obj;
{
    char qbuf[QBUFSZ];

    if (!obj->dknown) {
        /* probably blind */
        return;
    }
    flush_screen(1); /* buffered updates might matter to player's response */
    check_tutorial_message(QT_T_CALLITEM);

    if (obj->oclass == POTION_CLASS && obj->fromsink) {
        /* kludge, meaning it's sink water */
        Sprintf(qbuf, "Call a stream of %s fluid:",
                OBJ_DESCR(objects[obj->otyp]));
    } else {
        (void) safe_qbuf(qbuf, "Call ", ":", obj, docall_xname, simpleonames, "thing");
    }
    call_input(obj->otyp, qbuf);
}

void
docall_input(int obj_otyp)
{
    char qbuf[QBUFSZ];
    struct obj otemp = {0};

    otemp.otyp = obj_otyp;
    otemp.oclass = objects[obj_otyp].oc_class;
    otemp.quan = 1L;
    Sprintf(qbuf, "Call %s:", an(xname(&otemp)));
    call_input(obj_otyp, qbuf);
}

/* Using input from player to name an object type. */
static void
call_input(int obj_otyp, char *prompt)
{
    char buf[BUFSZ];

    getlin(prompt, buf);

    if(!*buf || *buf == '\033') {
        flags.last_broken_otyp = obj_otyp;
        return;
    } else {
        flags.last_broken_otyp = STRANGE_OBJECT;
    }

    call_object(obj_otyp, buf);
}

static void
call_object(int obj_otyp, char *buf)
{
    register char **str1;
    /* clear old name */
    str1 = &(objects[obj_otyp].oc_uname);
    if(*str1) free((genericptr_t)*str1);

    /* strip leading and trailing spaces; uncalls item if all spaces */
    (void)mungspaces(buf);
    if (!*buf) {
        if (*str1) {    /* had name, so possibly remove from disco[] */
            /* strip name first, for the update_inventory() call
               from undiscover_object() */
            *str1 = (char *)0;
            undiscover_object(obj_otyp);
        }
    } else {
        *str1 = strcpy((char *) alloc((unsigned)strlen(buf)+1), buf);
        discover_object(obj_otyp, FALSE, TRUE); /* possibly add to disco[] */
    }
}

static const char * const ghostnames[] = {
    /* these names should have length < PL_NSIZ */
    /* Capitalize the names for aesthetics -dgk */
    "Adri", "Andries", "Andreas", "Bert", "David", "Dirk", "Emile",
    "Frans", "Fred", "Greg", "Hether", "Jay", "John", "Jon", "Karnov",
    "Kay", "Kenny", "Kevin", "Maud", "Michiel", "Mike", "Peter", "Pipes", "Robert",
    "Ron", "Tom", "Wilmar", "Nick Danger", "Phoenix", "Jiro", "Mizue",
    "Stephan", "Lance Braccus", "Shadowhawk"
};

/* ghost names formerly set by x_monnam(), now by makemon() instead */
const char *
rndghostname()
{
    return rn2(7) ? ghostnames[rn2(SIZE(ghostnames))] : (const char *)plname;
}

/* Monster naming functions:
 * x_monnam is the generic monster-naming function.
 *        seen        unseen       detected       named
 * mon_nam: the newt    it  the invisible orc   Fido
 * noit_mon_nam:the newt (as if detected) the invisible orc Fido
 * l_monnam:    newt        it  invisible orc       dog called fido
 * Monnam:  The newt    It  The invisible orc   Fido
 * noit_Monnam: The newt (as if detected) The invisible orc Fido
 * Adjmonnam:   The poor newt   It  The poor invisible orc  The poor Fido
 * Amonnam: A newt      It  An invisible orc    Fido
 * a_monnam:    a newt      it  an invisible orc    Fido
 * m_monnam:    newt        xan orc         Fido
 * y_monnam:    your newt     your xan  your invisible orc  Fido
 */

/* Bug: if the monster is a priest or shopkeeper, not every one of these
 * options works, since those are special cases.
 */
char *
x_monnam(mtmp, article, adjective, suppress, called)
register struct monst *mtmp;
int article;
/* ARTICLE_NONE, ARTICLE_THE, ARTICLE_A: obvious
 * ARTICLE_YOUR: "your" on pets, "the" on everything else
 *
 * If the monster would be referred to as "it" or if the monster has a name
 * _and_ there is no adjective, "invisible", "saddled", etc., override this
 * and always use no article.
 */
const char *adjective;
int suppress;
/* SUPPRESS_IT, SUPPRESS_INVISIBLE, SUPPRESS_HALLUCINATION, SUPPRESS_SADDLE.
 * EXACT_NAME: combination of all the above
 * SUPPRESS_NAME: omit monster's assigned name (unless uniq w/ pname).
 */
boolean called;
{
    char *buf = nextmbuf();
    struct permonst *mdat = mtmp->data;
    const char *pm_name = mdat->mname;
    boolean do_hallu, do_invis, do_it, do_saddle, do_name;
    boolean name_at_start, has_adjectives;
    char *bp;

    if (program_state.gameover)
        suppress |= SUPPRESS_HALLUCINATION;
    if (article == ARTICLE_YOUR && !mtmp->mtame)
        article = ARTICLE_THE;

    do_hallu = Hallucination && !(suppress & SUPPRESS_HALLUCINATION);
    do_invis = mtmp->minvis && !(suppress & SUPPRESS_INVISIBLE);
    do_it = !canspotmon(mtmp) &&
            article != ARTICLE_YOUR &&
            !program_state.gameover &&
            mtmp != u.usteed &&
            !(u.uswallow && mtmp == u.ustuck) &&
            !(suppress & SUPPRESS_IT);
    do_saddle = !(suppress & SUPPRESS_SADDLE);
    do_name = !(suppress & SUPPRESS_NAME) || type_is_pname(mdat);

    buf[0] = '\0';

    /* unseen monsters, etc.  Use "it" */
    if (do_it) {
        Strcpy(buf, "it");
        return buf;
    }

    /* priests and minions: don't even use this function */
    if (mtmp->ispriest || mtmp->isminion) {
        char priestnambuf[BUFSZ];
        char *name;
        long save_prop = EHalluc_resistance;
        unsigned save_invis = mtmp->minvis;

        /* when true name is wanted, explicitly block Hallucination */
        if (!do_hallu) {
            EHalluc_resistance = 1L;
        }
        if (!do_invis) {
            mtmp->minvis = 0;
        }
        name = priestname(mtmp, priestnambuf);
        EHalluc_resistance = save_prop;
        mtmp->minvis = save_invis;
        if (article == ARTICLE_NONE && !strncmp(name, "the ", 4))
            name += 4;
        return strcpy(buf, name);
    }
    /* an "aligned priest" not flagged as a priest or minion should be
       "priest" or "priestess" (normally handled by priestname()) */
    if (mdat == &mons[PM_ALIGNED_PRIEST]) {
        pm_name = mtmp->female ? "priestess" : "priest";
    } else if (mdat == &mons[PM_HIGH_PRIEST] && mtmp->female) {
        pm_name = "high priestess";
    }

    /* Shopkeepers: use shopkeeper name.  For normal shopkeepers, just
     * "Asidonhopo"; for unusual ones, "Asidonhopo the invisible
     * shopkeeper" or "Asidonhopo the blue dragon".  If hallucinating,
     * none of this applies.
     */
    if (mtmp->isshk && !do_hallu
#ifdef BLACKMARKET
        && mtmp->data != &mons[PM_ONE_EYED_SAM]
#endif /* BLACKMARKET */
        ) {
        if (adjective && article == ARTICLE_THE) {
            /* pathological case: "the angry Asidonhopo the blue dragon"
               sounds silly */
            Strcpy(buf, "the ");
            Strcat(strcat(buf, adjective), " ");
            Strcat(buf, shkname(mtmp));
            return buf;
        }
        Strcat(buf, shkname(mtmp));
        if (mdat == &mons[PM_SHOPKEEPER] && !do_invis)
            return buf;
        Strcat(buf, " the ");
        if (do_invis)
            Strcat(buf, "invisible ");
        Strcat(buf, pm_name);
        return buf;
    }

    /* Put the adjectives in the buffer */
    if (adjective)
        Strcat(strcat(buf, adjective), " ");
    if (do_invis)
        Strcat(buf, "invisible ");
    if (do_saddle && (mtmp->misc_worn_check & W_SADDLE) &&
        !Blind && !Hallucination)
        Strcat(buf, "saddled ");
    has_adjectives = (buf[0] != '\0');

    /* Put the actual monster name or type into the buffer now.
       Remember whether the buffer starts with a personal name. */
    if (do_hallu) {
        Strcat(buf, rndmonnam());
        name_at_start = FALSE;
    } else if (do_name && has_mname(mtmp)) {
        char *name = MNAME(mtmp);

        if (mdat == &mons[PM_GHOST]) {
            Sprintf(eos(buf), "%s ghost", s_suffix(name));
            name_at_start = TRUE;
        } else if (called) {
            Sprintf(eos(buf), "%s called %s", pm_name, name);
            name_at_start = (boolean)type_is_pname(mdat);
        } else if (is_mplayer(mdat) && (bp = strstri(name, " the ")) != 0) {
            /* <name> the <adjective> <invisible> <saddled> <rank> */
            char pbuf[BUFSZ];

            Strcpy(pbuf, name);
            pbuf[bp - name + 5] = '\0'; /* adjectives right after " the " */
            if (has_adjectives)
                Strcat(pbuf, buf);
            Strcat(pbuf, bp + 5); /* append the rest of the name */
            Strcpy(buf, pbuf);
            article = ARTICLE_NONE;
            name_at_start = TRUE;
        } else {
            Strcat(buf, name);
            name_at_start = TRUE;
        }
    } else if (is_mplayer(mdat) && !In_endgame(&u.uz)) {
        char pbuf[BUFSZ];
        Strcpy(pbuf, rank_of((int)mtmp->m_lev,
                             monsndx(mdat),
                             (boolean)mtmp->female));
        Strcat(buf, lcase(pbuf));
        name_at_start = FALSE;
    } else if (is_rider(mtmp->data) &&
               (distu(mtmp->mx, mtmp->my) > 2) &&
               !(canseemon(mtmp)) &&
               /* for livelog reporting */
               !(suppress & SUPPRESS_IT)) {
        /* prevent the three horsemen to be identified from afar */
        Strcat(buf, "Rider");
        name_at_start = FALSE;
    } else {
        Strcat(buf, pm_name);
        name_at_start = (boolean)type_is_pname(mdat);
    }

    if (name_at_start && (article == ARTICLE_YOUR || !has_adjectives)) {
        if (mdat == &mons[PM_WIZARD_OF_YENDOR])
            article = ARTICLE_THE;
        else
            article = ARTICLE_NONE;
    } else if ((mdat->geno & G_UNIQ) && article == ARTICLE_A) {
        article = ARTICLE_THE;
    }

    {
        char buf2[BUFSZ];

        switch(article) {
        case ARTICLE_YOUR:
            Strcpy(buf2, "your ");
            Strcat(buf2, buf);
            Strcpy(buf, buf2);
            return buf;
        case ARTICLE_THE:
            Strcpy(buf2, "the ");
            Strcat(buf2, buf);
            Strcpy(buf, buf2);
            return buf;
        case ARTICLE_A:
            return(an(buf));
        case ARTICLE_NONE:
        default:
            return buf;
        }
    }
}

char *
l_monnam(mtmp)
struct monst *mtmp;
{
    return x_monnam(mtmp, ARTICLE_NONE, (char *) 0,
                    (has_mname(mtmp)) ? SUPPRESS_SADDLE : 0, TRUE);
}

char *
mon_nam(mtmp)
struct monst *mtmp;
{
    return x_monnam(mtmp, ARTICLE_THE, (char *) 0,
                    (has_mname(mtmp)) ? SUPPRESS_SADDLE : 0, FALSE);
}

/* print the name as if mon_nam() was called, but assume that the player
 * can always see the monster--used for probing and for monsters aggravating
 * the player with a cursed potion of invisibility
 */
char *
noit_mon_nam(mtmp)
struct monst *mtmp;
{
    return x_monnam(mtmp, ARTICLE_THE, (char *) 0,
                    (has_mname(mtmp)) ? (SUPPRESS_SADDLE | SUPPRESS_IT)
                                      : SUPPRESS_IT,
                    FALSE);
}

char *
Monnam(mtmp)
struct monst *mtmp;
{
    register char *bp = mon_nam(mtmp);

    *bp = highc(*bp);
    return(bp);
}

char *
noit_Monnam(mtmp)
struct monst *mtmp;
{
    register char *bp = noit_mon_nam(mtmp);

    *bp = highc(*bp);
    return(bp);
}

/* return "a dog" rather than "Fido", honoring hallucination and visibility */
char *
noname_monnam(mtmp, article)
struct monst *mtmp;
int article;
{
    return x_monnam(mtmp, article, (char *) 0, SUPPRESS_NAME, FALSE);
}

/* monster's own name -- overrides hallucination and [in]visibility
   so shouldn't be used in ordinary messages (mainly for disclosure) */
char *
m_monnam(mtmp)
struct monst *mtmp;
{
    return x_monnam(mtmp, ARTICLE_NONE, (char *)0, EXACT_NAME, FALSE);
}

/* pet name: "your little dog" */
char *
y_monnam(mtmp)
struct monst *mtmp;
{
    int prefix, suppression_flag;

    prefix = mtmp->mtame ? ARTICLE_YOUR : ARTICLE_THE;
    suppression_flag = (has_mname(mtmp) ||
                        /* "saddled" is redundant when mounted */
                        mtmp == u.usteed) ? SUPPRESS_SADDLE : 0;

    return x_monnam(mtmp, prefix, (char *)0, suppression_flag, FALSE);
}

char *
Adjmonnam(mtmp, adj)
struct monst *mtmp;
const char *adj;
{
    char *bp = x_monnam(mtmp, ARTICLE_THE, adj,
                        has_mname(mtmp) ? SUPPRESS_SADDLE : 0, FALSE);

    *bp = highc(*bp);
    return(bp);
}

char *
a_monnam(mtmp)
register struct monst *mtmp;
{
    return x_monnam(mtmp, ARTICLE_A, (char *)0,
                    has_mname(mtmp) ? SUPPRESS_SADDLE : 0, FALSE);
}

char *
Amonnam(mtmp)
struct monst *mtmp;
{
    char *bp = a_monnam(mtmp);

    *bp = highc(*bp);
    return(bp);
}

/* used for monster ID by the '/', ';', and 'C' commands to block remote
   identification of the endgame altars via their attending priests */
char *
distant_monnam(mon, article, outbuf)
struct monst *mon;
int article;    /* only ARTICLE_NONE and ARTICLE_THE are handled here */
char *outbuf;
{
    /* high priest(ess)'s identity is concealed on the Astral Plane,
       unless you're adjacent (overridden for hallucination which does
       its own obfuscation) */
    if (mon->data == &mons[PM_HIGH_PRIEST] && !Hallucination &&
        Is_astralevel(&u.uz) && distu(mon->mx, mon->my) > 2) {
        Strcpy(outbuf, article == ARTICLE_THE ? "the " : "");
        Strcat(outbuf, mon->female ? "high priestess" : "high priest");
    } else {
        Strcpy(outbuf, x_monnam(mon, article, (char *)0, 0, TRUE));
    }
    return outbuf;
}

/* returns mon_nam(mon) relative to other_mon; normal name unless they're
   the same, in which case the reference is to {him|her|it} self */
char *
mon_nam_too(mon, other_mon)
struct monst *mon, *other_mon;
{
    char *outbuf;

    if (mon != other_mon) {
        outbuf = mon_nam(mon);
    } else {
        outbuf = nextmbuf();
        switch (pronoun_gender(mon, FALSE)) {
        case 0:
            Strcpy(outbuf, "himself");
            break;
        case 1:
            Strcpy(outbuf, "herself");
            break;
        default:
            Strcpy(outbuf, "itself");
            break;
        }
    }
    return outbuf;
}

/* for debugging messages, where data might be suspect and we aren't
   taking what the hero does or doesn't know into consideration */
char *
minimal_monnam(mon, ckloc)
struct monst *mon;
boolean ckloc;
{
    struct permonst *ptr;
    char *outbuf = nextmbuf();

    if (!mon) {
        Strcpy(outbuf, "[Null monster]");
    } else if ((ptr = mon->data) == 0) {
        Strcpy(outbuf, "[Null mon->data]");
#if NEXT_VERSION
    } else if (ptr < &mons[0]) {
        Sprintf(outbuf, "[Invalid mon->data %s < %s]",
                fmt_ptr((genericptr_t) mon->data),
                fmt_ptr((genericptr_t) &mons[0]));
    } else if (ptr >= &mons[NUMMONS]) {
        Sprintf(outbuf, "[Invalid mon->data %s >= %s]",
                fmt_ptr((genericptr_t) mon->data),
                fmt_ptr((genericptr_t) &mons[NUMMONS]));
#endif
    } else if (ckloc && ptr == &mons[PM_LONG_WORM]
               && level.monsters[mon->mx][mon->my] != mon) {
        Sprintf(outbuf, "%s <%d,%d>",
                mons[PM_LONG_WORM_TAIL].mname, mon->mx, mon->my);
    } else {
        Sprintf(outbuf, "%s%s <%d,%d>",
                mon->mtame ? "tame " : mon->mpeaceful ? "peaceful " : "",
                mon->data->mname, mon->mx, mon->my);
        if (mon->cham != NON_PM)
            Sprintf(eos(outbuf), "{%s}", mons[mon->cham].mname);
    }
    return outbuf;
}

static const char * const bogusmons[] = {
    "jumbo shrimp", "giant pigmy", "gnu", "killer penguin",
    "giant cockroach", "giant slug", "maggot", "pterodactyl",
    "tyrannosaurus rex", "basilisk", "beholder", "nightmare",
    "efreeti", "marid", "rot grub", "bookworm", "master lichen",
    "shadow", "hologram", "jester", "attorney", "sleazoid",
    "killer tomato", "amazon", "robot", "battlemech",
    "rhinovirus", "harpy", "lion-dog", "rat-ant", "Y2K bug",
    /* misc. */
    "grue", "Christmas-tree monster", "luck sucker", "paskald",
    "brogmoid", "dornbeast",        /* Quendor (Zork, &c.) */
    "Ancient Multi-Hued Dragon", "Evil Iggy",
    /* Moria */
    "rattlesnake", "ice monster", "phantom",
    "quagga", "aquator", "griffin",
    "emu", "kestrel", "xeroc", "venus flytrap",
    /* Rogue V5 http://rogue.rogueforge.net/vade-mecum/ */
    "creeping coins",           /* Wizardry */
    "hydra", "siren",           /* Greek legend */
    "killer bunny",             /* Monty Python */
    "rodent of unusual size",       /* The Princess Bride */
    "Smokey the bear",          /* "Only you can prevent forest fires!" */
    "Luggage",              /* Discworld */
    "Ent",                  /* Lord of the Rings */
    "tangle tree", "nickelpede", "wiggle",  /* Xanth */
    "white rabbit", "snark",        /* Lewis Carroll */
    "pushmi-pullyu",            /* Dr. Dolittle */
    "smurf",                /* The Smurfs */
    "tribble", "Klingon", "Borg",       /* Star Trek */
    "Ewok",                 /* Star Wars */
    "Totoro",               /* Tonari no Totoro */
    "ohmu",                 /* Nausicaa */
    "youma",                /* Sailor Moon */
    "nyaasu",               /* Pokemon (Meowth) */
    "Godzilla", "King Kong",        /* monster movies */
    "earthquake beast",         /* old L of SH */
    "Invid",                /* Robotech */
    "Terminator",               /* The Terminator */
    "boomer",               /* Bubblegum Crisis */
    "Dalek",                /* Dr. Who ("Exterminate!") */
    "microscopic space fleet", "Ravenous Bugblatter Beast of Traal",
    /* HGttG */
    "teenage mutant ninja turtle",      /* TMNT */
    "samurai rabbit",           /* Usagi Yojimbo */
    "aardvark",             /* Cerebus */
    "Audrey II",                /* Little Shop of Horrors */
    "witch doctor", "one-eyed one-horned flying purple people eater",
    /* 50's rock 'n' roll */
    "Barney the dinosaur",          /* saccharine kiddy TV */
    "Morgoth",              /* Angband */
    "Vorlon",               /* Babylon 5 */
    "questing beast",           /* King Arthur */
    "Predator",             /* Movie */
    "mother-in-law",            /* common pest */
    /* from NAO */
    "one-winged dewinged stab-bat",     /* KoL */
    "praying mantis",
    "arch-pedant",
    "beluga whale",
    "bluebird of happiness",
    "bouncing eye", "floating nose", "wandering eye",
    "buffer overflow", "dangling pointer", "walking disk drive", "floating point",
    "cacodemon", "scrag",
    "cardboard golem", "duct tape golem",
    "chess pawn",
    "chicken",
    "chocolate pudding",
    "coelacanth",
    "corpulent porpoise",
    "Crow T. Robot",
    "diagonally moving grid bug",
    "dropbear",
    "Dudley",
    "El Pollo Diablo",
    "evil overlord",
    "existential angst",
    "figment of your imagination", "flash of insight",
    "flying pig",
    "gazebo",
    "gonzo journalist",
    "gray goo", "magnetic monopole",
    "heisenbug",
    "lag monster",
    "loan shark",
    "Lord British",
    "newsgroup troll",
    "ninja pirate zombie robot",
    "octarine dragon",
    "particle man",
    "possessed waffle iron",
    "poultrygeist",
    "raging nerd",
    "roomba",
    "sea cucumber",
    "spelling bee",
    "Strong Bad",
    "stuffed raccoon puppet",
    "tapeworm",
    "liger",
    "velociraptor",
    "vermicious knid",
    "viking",
    "voluptuous ampersand",
    "wee green blobbie",
    "wereplatypus",
    "zergling",
    "hag of bolding",
    "grind bug",
    "enderman",
    "wight supremacist",
    "Magical Trevor",
    "first category perpetual motion device",

    /* soundex and typos of monsters */
    "gloating eye",
    "flush golem",
    "martyr orc",
    "mortar orc",
    "acute blob",
    "aria elemental",
    "aliasing priest",
    "aligned parasite",
    "aligned parquet",
    "aligned proctor",
    "baby balky dragon",
    "baby blues dragon",
    "baby caricature",
    "baby crochet",
    "baby grainy dragon",
    "baby bong worm",
    "baby long word",
    "baby parable worm",
    "barfed devil",
    "beer wight",
    "boor wight",
    "brawny mold",
    "rave spider",
    "clue golem",
    "bust vortex",
    "errata elemental",
    "elastic eel",
    "electrocardiogram eel",
    "fir elemental",
    "tire elemental",
    "flamingo sphere",
    "fallacy golem",
    "frizzed centaur",
    "forest centerfold",
    "fierceness sphere",
    "frosted giant",
    "geriatric snake",
    "gnat ant",
    "giant bath",
    "grant beetle",
    "giant mango",
    "glossy golem",
    "gnome laureate",
    "gnome dummy",
    "gooier ooze",
    "green slide",
    "guardian nacho",
    "hell hound pun",
    "high purist",
    "hairnet devil",
    "ice trowel",
    "feather golem",
    "lounge worm",
    "mountain lymph",
    "pager golem",
    "pie fiend",
    "prophylactic worm",
    "sock mole",
    "rogue piercer",
    "seesawing sphere",
    "simile mimic",
    "moldier ant",
    "stain vortex",
    "scone giant",
    "umbrella hulk",
    "vampire mace",
    "verbal jabberwock",
    "water lemon",
    "winged grizzly",
    "yellow wight",

    /* from UnNetHack */
    "apostrophe golem", "Bob the angry flower",
    "bonsai-kitten", "Boxxy", "lonelygirl15",
    "tie-thulu", "Domo-kun", "nyan cat",
    "Zalgo", "common comma",
    "looooooooooooong cat",         /* internet memes */
    "bohrbug", "mandelbug", "schroedinbug", /* bugs */
    "Gerbenok",             /* Monty Python killer rabbit */
    "doenertier",               /* Erkan & Stefan */
    "Invisible Pink Unicorn",
    "Flying Spaghetti Monster",     /* deities */
    "Bluebear", "Professor Abdullah Nightingale",
    "Qwerty Uiop", "troglotroll",       /* Zamonien */
    "wolpertinger", "elwedritsche", "skvader",
    "Nessie", "tatzelwurm", "dahu",     /* european cryptids */
    "three-headed monkey",          /* Monkey Island */
    "little green man",         /* modern folklore */
    "weighted Companion Cube",  "defective turret", /* Portal */
    "/b/tard",              /* /b/ */
    "manbearpig",               /* South Park */
    "ceiling cat", "basement cat",
    "monorail cat",             /* the Internet is made for cat pix */
    "stoned golem", "wut golem", "nice golem",
    "flash golem",  "trash golem",          /* silly golems */
    "tridude",              /* POWDER */
    "orcus cosmicus",           /* Radomir Dopieralski */
    "yeek", "quylthulg",
    "Greater Hell Beast",           /* Angband */
    "Vendor of Yizard",         /* Souljazz */
    "Sigmund", "lernaean hydra", "Ijyb",
    "Gloorx Vloq", "Blork the orc",     /* Dungeon Crawl Stone Soup */
    "unicorn pegasus kitten",       /* Wil Wheaton, John Scalzi */
    "dwerga nethackus", "dwerga castrum",   /* Ask ASCII Ponies */
    "Irrenhaus the Third",          /* http://www.youtube.com/user/Irrenhaus3 */
    "semipotent demidog", "shale imp",  /* Homestuck */
    "mercury imp", "Betty Crocker",
    "Spades Slick",
    "patriarchy", "bourgeiose",     /* talking points */
    "mainstream media",
    "Demonhead Mobster Kingpin",        /* Problem Sleuth */
    "courtesan angel", "fractal bee",
    "Weasel King",
    "GLaDOS", "dangerous mute lunatic", /* Portal */
    "skag", "thresher", "LoaderBot", "varkid",  /* Borderlands */
    "Yarakeen", "Veil Warden",                       /*  War of Omens  */
    "Trogdor the Burninator",                        /* Homestar Runner */
    "Guy made of Bees", "Toot Oriole",               /* Kingdom of Loathing */
    "Magnus Burnsides the Fighter", "Taako the Wizard", "Merle Highchurch the Cleric", /* The Adventure Zone */
    "rubber duck", /* rubber ducks are a programmer's best friend */

    /* bogus UnNetHack monsters */
    "weeping angle",
    "gelatinous sphere", "gelatinous pyramid",
    "gelatinous Klein bottle", "gelatinous Mandelbrot set",
    "robot unicorn",

    /* Welcome to Night Vale*/
    "John Peters, you know, the farmer",

    /* Fallen London and Sunless Sea */
    "Lorn Fluke",
    "sorrow spider", "spider council",
    "chair of the department of antiquarian esquivalience",
    "the Starveling Cat",
    "the Tree of Ages",

    /* from Slashem Extended */
    "amphitheatre", /* evil patch idea by yasdorian */
    "banana peel golem", /* evil patch idea from DCSS */
    "dissolved undead potato",
    "dragonbreath nymph"
    "fart elemental", /* evil patch idea from DCSS */
    "floating nose", /* evil patch idea by K2 */
    "goldfish", /* evil patch idea from DCSS */
    "kamikaze tribble", /* evil patch idea by jonadab */
    "speedotrice", /* evil patch idea by jonadab */
    "techno ant",
    "drum stint reluctance",
    "tackle deice",
};


/* Return a random monster name, for hallucination.
 * KNOWN BUG: May be a proper name (Godzilla, Barney), may not
 * (the Terminator, a Dalek).  There's no elegant way to deal
 * with this without radically modifying the calling functions.
 */
const char *
rndmonnam()
{
    int name;

    do {
        name = rn1(SPECIAL_PM + SIZE(bogusmons) - LOW_PM, LOW_PM);
    } while (name < SPECIAL_PM &&
             (type_is_pname(&mons[name]) || (mons[name].geno & G_NOGEN)));

    if (name >= SPECIAL_PM) return bogusmons[name - SPECIAL_PM];
    return mons[name].mname;
}

/* check bogusmon prefix to decide whether it's a personal name */
boolean
bogon_is_pname(code)
char code;
{
    if (!code)
        return FALSE;
    return index("-+=", code) ? TRUE : FALSE;
}

#ifdef REINCARNATION
/* Name of a Rogue player */
const char *
roguename()
{
    char *i, *opts;

    if ((opts = nh_getenv("ROGUEOPTS")) != 0) {
        for (i = opts; *i; i++)
            if (!strncmp("name=", i, 5)) {
                char *j;
                if ((j = index(i+5, ',')) != 0)
                    *j = (char)0;
                return i+5;
            }
    }
    return rn2(3) ? (rn2(2) ? "Michael Toy" : "Kenneth Arnold")
           : "Glenn Wichman";
}
#endif /* REINCARNATION */

static NEARDATA const char * const hcolors[] = {
    "ultraviolet", "infrared", "bluish-orange",
    "reddish-green", "dark white", "light black", "sky blue-pink",
    "salty", "sweet", "sour", "bitter", "umami",
    "striped", "spiral", "swirly", "plaid", "checkered", "argyle",
    "paisley", "blotchy", "guernsey-spotted", "polka-dotted",
    "square", "round", "triangular",
    "cabernet", "sangria", "fuchsia", "wisteria",
    "lemon-lime", "strawberry-banana", "peppermint",
    "romantic", "incandescent",
    "octarine", /* Discworld: the Colour of Magic */
};

const char *
hcolor(colorpref)
const char *colorpref;
{
    return (Hallucination || !colorpref) ?
           hcolors[rn2(SIZE(hcolors))] : colorpref;
}

/* return a random real color unless hallucinating */
const char *
rndcolor()
{
    int k = rn2(CLR_MAX);

    return Hallucination ? hcolor((char *)0) : (k == NO_COLOR) ?
           "colorless" : c_obj_colors[k];
}

static NEARDATA const char *const hliquids[] = {
    "yoghurt", "oobleck", "clotted blood", "diluted water", "purified water",
    "instant coffee", "tea", "herbal infusion", "liquid rainbow",
    "creamy foam", "mulled wine", "bouillon", "nectar", "grog", "flubber",
    "ketchup", "slow light", "oil", "vinaigrette", "liquid crystal", "honey",
    "caramel sauce", "ink", "aqueous humour", "milk substitute",
    "fruit juice", "glowing lava", "gastric acid", "mineral water",
    "cough syrup", "quicksilver", "sweet vitriol", "grey goo", "pink slime",
};

const char *
hliquid(liquidpref)
const char *liquidpref;
{
    return (Hallucination || !liquidpref) ? hliquids[rn2(SIZE(hliquids))]
                                          : liquidpref;
}

/* Aliases for road-runner nemesis
 */
static const char * const coynames[] = {
    "Carnivorous Vulgaris", "Road-Runnerus Digestus",
    "Eatibus Anythingus", "Famishus-Famishus",
    "Eatibus Almost Anythingus", "Eatius Birdius",
    "Famishius Fantasticus", "Eternalii Famishiis",
    "Famishus Vulgarus", "Famishius Vulgaris Ingeniusi",
    "Eatius-Slobbius", "Hardheadipus Oedipus",
    "Carnivorous Slobbius", "Hard-Headipus Ravenus",
    "Evereadii Eatibus", "Apetitius Giganticus",
    "Hungrii Flea-Bagius", "Overconfidentii Vulgaris",
    "Caninus Nervous Rex", "Grotesques Appetitus",
    "Nemesis Ridiculii", "Canis latrans"
};

char *
coyotename(mtmp, buf)
struct monst *mtmp;
char *buf;
{
    if (mtmp && buf) {
        Sprintf(buf, "%s - %s",
                x_monnam(mtmp, ARTICLE_NONE, (char *)0, 0, TRUE),
                mtmp->mcan ? coynames[SIZE(coynames)-1]
                           : coynames[mtmp->m_id % (SIZE(coynames)-1)]);
    }
    return buf;
}

char *
rndorcname(s)
char *s;
{
    static const char *v[] = { "a", "ai", "og", "u" };
    static const char *snd[] = { "gor", "gris", "un", "bane", "ruk",
                                 "oth","ul", "z", "thos","akh","hai" };
    int i, iend = rn1(2, 3), vstart = rn2(2);

    if (s) {
        *s = '\0';
        for (i = 0; i < iend; ++i) {
            vstart = 1 - vstart;                /* 0 -> 1, 1 -> 0 */
            Sprintf(eos(s), "%s%s", (i > 0 && !rn2(30)) ? "-" : "",
                    vstart ? v[rn2(SIZE(v))] : snd[rn2(SIZE(snd))]);
        }
    }
    return s;
}

struct monst *
christen_orc(mtmp, gang, other)
struct monst *mtmp;
const char *gang, *other;
{
    int sz = 0;
    char buf[BUFSZ], buf2[BUFSZ], *orcname;

    orcname = rndorcname(buf2);
    sz = (int) strlen(orcname);
    if (gang) {
        sz += (int) (strlen(gang) + sizeof " of " - sizeof "");
    } else if (other) {
        sz += (int) strlen(other);
    }

    if (sz < BUFSZ) {
        char gbuf[BUFSZ];
        boolean nameit = FALSE;

        if (gang && orcname) {
            Sprintf(buf, "%s of %s", upstart(orcname),
                    upstart(strcpy(gbuf, gang)));
            nameit = TRUE;
        } else if (other && orcname) {
            Sprintf(buf, "%s%s", upstart(orcname), other);
            nameit = TRUE;
        }
        if (nameit) {
            mtmp = christen_monst(mtmp, buf);
        }
    }
    return mtmp;
}

/* make sure "The Colour of Magic" remains the first entry in here */
static const char *const sir_Terry_novels[] = {
    "The Colour of Magic", "The Light Fantastic", "Equal Rites", "Mort",
    "Sourcery", "Wyrd Sisters", "Pyramids", "Guards! Guards!", "Eric",
    "Moving Pictures", "Reaper Man", "Witches Abroad", "Small Gods",
    "Lords and Ladies", "Men at Arms", "Soul Music", "Interesting Times",
    "Maskerade", "Feet of Clay", "Hogfather", "Jingo", "The Last Continent",
    "Carpe Jugulum", "The Fifth Elephant", "The Truth", "Thief of Time",
    "The Last Hero", "The Amazing Maurice and His Educated Rodents",
    "Night Watch", "The Wee Free Men", "Monstrous Regiment",
    "A Hat Full of Sky", "Going Postal", "Thud!", "Wintersmith",
    "Making Money", "Unseen Academicals", "I Shall Wear Midnight", "Snuff",
    "Raising Steam", "The Shepherd's Crown"
};

const char *
noveltitle(novidx)
int *novidx;
{
    int j, k = SIZE(sir_Terry_novels);

    j = rn2(k);
    if (novidx) {
        if (*novidx == -1)
            *novidx = j;
        else if (*novidx >= 0 && *novidx < k)
            j = *novidx;
    }
    return sir_Terry_novels[j];
}

const char *
lookup_novel(lookname, idx)
const char *lookname;
int *idx;
{
    int k;

    /* Take American or U.K. spelling of this one */
    if (!strcmpi(The(lookname), "The Color of Magic"))
        lookname = sir_Terry_novels[0];

    for (k = 0; k < SIZE(sir_Terry_novels); ++k) {
        if (!strcmpi(lookname, sir_Terry_novels[k])
            || !strcmpi(The(lookname), sir_Terry_novels[k])) {
            if (idx)
                *idx = k;
            return sir_Terry_novels[k];
        }
    }
    /* name not found; if novelidx is already set, override the name */
    if (idx && *idx >= 0 && *idx < SIZE(sir_Terry_novels))
        return sir_Terry_novels[*idx];

    return (const char *) 0;
}

/*do_name.c*/
