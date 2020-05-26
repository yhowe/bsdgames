
/*-
 * Copyright (c) 2019 Y. Howe <yhowe01@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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

#ifdef ESP32
#include <odroid_go.h>
#endif

#include <curses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef sigmask
#define sigmask(s) (1 << ((s) - 1))
#endif

#include "screen.h"
#include "tetris.h"

static cell *curscreen;	/* 1 => standout (or otherwise marked) */
static int curscore;
static int curlives;
static int isset;		/* true => terminal is in game mode */
static void (*tstp)(int);

static	void	scr_stop(int);

int B_COLS, B_ROWS, B_SIZE, SCREENSIZE;

/*
 * putstr() is for unpadded strings (either as in termcap(5) or
 * simply literal strings); putpad() is for padded strings with
 * count=1.  (See screen.h for putpad().)
 */
#define	putstr(s)	printw(s)

static void
moveto(int r, int c)
{
	move(r, c);
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
	B_COLS = COLS;
	B_ROWS = (LINES - 3);
	B_SIZE = (B_ROWS * B_COLS);
	SCREENSIZE = sizeof(cell) * B_SIZE;

	board = (cell *)malloc(SCREENSIZE);
	curscreen = (cell *)malloc(SCREENSIZE);
#ifdef ESP32
	timeout(2);
#else
	timeout(0);
#endif
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
	Offset = 1;
	curscore = 0;
	curlives = lives;
	memset(curscreen, 0, SCREENSIZE);
	moveto(0, 0);
	setcolor(0);

}

/*
 * End screen mode.
 */
void
scr_end(void)
{
	endwin();
	free(curscreen);
	free(board);
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

typedef cell regcell;

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

	/* always leave cursor after last displayed point */
	curscreen[D_LAST * B_COLS - 1] = -1;

	if (score != curscore || lives != curlives) {
		moveto(0, 0);
		setcolor(0);
#ifdef ESP32
		printw("Score: %d    Lives: %d    BATT: %3d%%",
		    score, lives, GO.battery.getPercentage());
#else
		printw("Score: %d    Lives: %d", score, lives);
#endif
		curscore = score;
		curlives = lives;
	}

	bp = &board[0];
	sp = &curscreen[0];
	for (j = 0; j < B_ROWS; j++) {
		ccol = -1;
		for (i = 0; i < B_COLS; bp++, sp++, i++) {
			if (*sp == (so = *bp))
				continue;
			*sp = so;
			if (i != ccol) {
				moveto((j + Offset ), (i));
			}
			if (so != cur_so) {
				setcolor(so);
				cur_so = so;
			}
			putstr(" ");
			ccol = i + 1;
		}
	}
	if (cur_so)
		setcolor(0);
	(void) fflush(stdout);
}

void
scr_ball(int oldy, int oldx, int newy, int newx, char ball)
{
		moveto(oldy + Offset, oldx);
		printw("%c", ' ');
		moveto(newy + Offset, newx);
		printw("%c", ball);
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
