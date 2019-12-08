/* NetHack 3.6	context.h	$NHDT-Date: 1455907260 2016/02/19 18:41:00 $  $NHDT-Branch: NetHack-3.6.0 $:$NHDT-Revision: 1.30 $ */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/*-Copyright (c) Michael Allison, 2006. */
/* NetHack may be freely redistributed.  See license for details. */

#ifndef CONTEXT_H
#define CONTEXT_H

struct polearm_info {
    struct monst *hitmon; /* the monster we tried to hit last */
    unsigned m_id;        /* monster id of hitmon, in save file */
};

extern struct polearm_info polearm;

#endif /* CONTEXT_H */
