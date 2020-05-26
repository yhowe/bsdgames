/*	$NetBSD: screen.c,v 1.33 2017/03/20 22:05:27 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek and Darren F. Provine.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)screen.c	8.1 (Berkeley) 5/31/93
 */

/*
 * Tetris screen control.
 */

#include <sys/cdefs.h>
#include <sys/ioctl.h>

#include <odroid_go.h>

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <unistd.h>

#ifndef sigmask
#define sigmask(s) (1 << ((s) - 1))
#endif

#include "screen.h"
#include "tetris.h"

static cell curscreen[B_SIZE];	/* 1 => standout (or otherwise marked) */
static int curscore;
static int isset;		/* true => terminal is in game mode */
static void (*tstp)(int);

static	void	scr_stop(int);


/*

/*
 * putstr() is for unpadded strings (either as in termcap(5) or
 * simply literal strings); putpad() is for padded strings with
 * count=1.  (See screen.h for putpad().)
 */
#define	putstr(s)	printw(s)

static void
moveto(int r, int c)
{
	move(r,c);
}

static void
setcolor(int c)
{
	attrset(COLOR_PAIR(c));
}

/*
 * Set up from termcap.
 */
void
scr_init(void)
{

	initscr();
	timeout(100);
		start_color();
 
		init_pair(0, COLOR_WHITE, COLOR_BLACK);
		init_pair(1, COLOR_WHITE, COLOR_MAGENTA);
		init_pair(2, COLOR_WHITE, COLOR_CYAN);
		init_pair(3, COLOR_WHITE, COLOR_BLUE);
		init_pair(4, COLOR_WHITE, COLOR_GREEN);
		init_pair(5, COLOR_WHITE, COLOR_RED);
		init_pair(6, COLOR_WHITE, COLOR_YELLOW);
		init_pair(7, COLOR_BLACK, COLOR_WHITE);

		attrset(COLOR_PAIR(1));
	Rows = LINES;
	Cols = COLS;
	curscore = 0;
	memset(curscreen, 0, sizeof(curscreen));
	moveto(0, 0);
	setcolor(0);

}

/* this foolery is needed to modify tty state `atomically' */
static jmp_buf scr_onstop;

/*
 * End screen mode.
 */
void
scr_end(void)
{
	endwin();
}

void
stop(const char *why)
{
	scr_end();
}

/*
 * Clear the screen, forgetting the current contents in the process.
 */
void
scr_clear(void)
{
	curscore = -1;
	clear();
}

#if vax && !__GNUC__
typedef int regcell;	/* pcc is bad at `register char', etc */
#else
typedef cell regcell;
#endif

/*
 * Update the screen.
 */
void
scr_update(void)
{
	cell *bp, *sp;
	regcell so, cur_so = 0;
	int i, ccol, j;
	sigset_t nsigset, osigset;
	static const struct shape *lastshape;

	/* always leave cursor after last displayed point */
	curscreen[D_LAST * B_COLS - 1] = -1;

	if (score != curscore) {
		moveto(0, 0);
		setcolor(0);
		printw("Score: %d", score);
		mvprintw(0, COLS - 11,"BATT: %3d%%",
		    GO.battery.getPercentage());
		curscore = score;
	}

	/* draw preview of nextpattern */
	if (showpreview && (nextshape != lastshape)) {
		static int r=5, c=2;
		int tr, tc, t; 

		lastshape = nextshape;
		
		/* clean */
		setcolor(0);
		moveto(r-1, c-1); putstr("          ");
		moveto(r,   c-1); putstr("          ");
		moveto(r+1, c-1); putstr("          ");
		moveto(r+2, c-1); putstr("          ");

		moveto(r-3, c-2);
		putstr("Next shape:");
						
		/* draw */
		setcolor(nextshape->color);
		moveto(r, 2*c);
		putstr("  ");
		for(i=0; i<3; i++) {
			t = c + r*B_COLS;
			t += nextshape->off[i];

			tr = t / B_COLS;
			tc = t % B_COLS;

			moveto(tr, 2*tc);
			putstr("  ");
		}
		setcolor(0);
	}
	
	bp = &board[D_FIRST * B_COLS];
	sp = &curscreen[D_FIRST * B_COLS];
	for (j = D_FIRST; j < D_LAST; j++) {
		ccol = -1;
		for (i = 0; i < B_COLS; bp++, sp++, i++) {
			if (*sp == (so = *bp))
				continue;
			*sp = so;
			if (i != ccol) {
				moveto(RTOD(j + Offset), CTOD(i));
			}
			if (1) {
				if (so != cur_so) {
					setcolor(so);
					cur_so = so;
				}
#ifdef DEBUG
				char buf[3];
				snprintf(buf, sizeof(buf), "%d%d", so, so);
				putstr(buf);
#else
				putstr("  ");
#endif
			} else
				putstr(so ? "XX" : "  ");
			ccol = i + 1;
			/*
			 * Look ahead a bit, to avoid extra motion if
			 * we will be redrawing the cell after the next.
			 * Motion probably takes four or more characters,
			 * so we save even if we rewrite two cells
			 * `unnecessarily'.  Skip it all, though, if
			 * the next cell is a different color.
			 */
#define	STOP (B_COLS - 3)
			if (i > STOP || sp[1] != bp[1] || so != bp[1])
				continue;
			if (sp[2] != bp[2])
				sp[1] = -1;
			else if (i < STOP && so == bp[2] && sp[3] != bp[3]) {
				sp[2] = -1;
				sp[1] = -1;
			}
		}
	}
	if (cur_so)
		setcolor(0);
	(void) fflush(stdout);
}

/*
 * Write a message (set!=0), or clear the same message (set==0).
 * (We need its length in case we have to overwrite with blanks.)
 */
void
scr_msg(char *s, int set)
{
	
		int len = strlen(s);
		char blank[255];
		memset(blank, ' ', len);
		blank[len] = 0;
		moveto(Rows - 2, 0);
		printw("%s", set ? s: blank);

}
