/*	$NetBSD: tetris.c,v 1.32 2016/03/03 21:38:55 nat Exp $	*/

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
 *	@(#)tetris.c	8.1 (Berkeley) 5/31/93
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1992, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

/*
 * Tetris (or however it is spelled).
 */

#include <sys/time.h>

#include <odroid_go.h>
#include <Arduino.h>
#include "err.h"
#include <curses.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "input.h"
#include "screen.h"
#include "tetris.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

cell	board[B_SIZE];		/* 1 => occupied, 0 => empty */

int	Rows, Cols;		/* current screen size */
int	Offset;			/* used to center board & shapes */

static const struct shape *curshape;
const struct shape *nextshape;

long	fallrate;		/* less than 1 million; smaller => faster */

int	score;			/* the obvious thing */
gid_t	gid, egid;

char	key_msg[100];
int	showpreview;
int	nocolor;

static void elide(void);
static void setup_board(void);

void odroid_system_application_set(int slot)
{
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP,
        static_cast<esp_partition_subtype_t>(ESP_PARTITION_SUBTYPE_APP_OTA_MIN + slot),
        NULL);
    if (partition != NULL)
    {
        esp_err_t err = esp_ota_set_boot_partition(partition);
        if (err != ESP_OK)
        {
            printf("odroid_system_application_set: esp_ota_set_boot_partition failed.\n");
            abort();
        }
    }
}

/*
 * Set up the initial board.  The bottom display row is completely set,
 * along with another (hidden) row underneath that.  Also, the left and
 * right edges are set.
 */
static void
setup_board(void)
{
	int i;
	cell *p;

	memset(board, 0, sizeof(board));
	p = board;
	for (i = B_SIZE; i; i--)
		*p++ = (i <= (2 * B_COLS) || (i % B_COLS) < 2) ? 7 : 0;
}

/*
 * Elide any full active rows.
 */
static void
elide(void)
{
	int i, j, base;
	cell *p;

	for (i = A_FIRST; i < A_LAST; i++) {
		base = i * B_COLS + 1;
		p = &board[base];
		for (j = B_COLS - 2; *p++ != 0;) {
			if (--j <= 0) {
				/* this row is to be elided */
				memset(&board[base], 0, B_COLS - 2);
				scr_update();
				tsleep();
				while (--base != 0)
					board[base + B_COLS] = board[base];
				/* don't forget to clear 0th row */
				memset(&board[1], 0, B_COLS - 2);
				scr_update();
				tsleep();
				break;
			}
		}
	}
}

void
setup(void)
{
	GO.begin();
	GO.battery.setProtection(true);
}

void
loop(void)
{
	int pos, c;
	const char *keys;
	int level = 1;
#define NUMKEYS 7
	char key_write[NUMKEYS][10];
	int ch, i, j;
	int fd;


    switch (esp_sleep_get_wakeup_cause())
    {
        case ESP_SLEEP_WAKEUP_EXT0:
        {
            printf("app_main: ESP_SLEEP_WAKEUP_EXT0 deep sleep reset\n");
            break;
        }

        case ESP_SLEEP_WAKEUP_EXT1:
        case ESP_SLEEP_WAKEUP_TIMER:
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
        case ESP_SLEEP_WAKEUP_ULP:
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        {
            printf("app_main: Unexpected deep sleep reset\n");

	    timeout(10);
            if (getch() == 'm')
            {
                // Force return to menu to recover from
                // ROM loading crashes

                // Set menu application
                odroid_system_application_set(0);

                // Reset
                esp_restart();
            }

        }
            break;

        default:
            printf("app_main: Not a deep sleep reset\n");
            break;
    }

	keys = "larbtmd";

	fallrate = 1000000 / level;

	for (i = 0; i <= (NUMKEYS-1); i++) {
		for (j = i+1; j <= (NUMKEYS-1); j++) {
			if (keys[i] == keys[j]) {
				errx(1, "duplicate command keys specified.");
			}
		}
		if (keys[i] == ' ')
			strcpy(key_write[i], "<space>");
		else {
			key_write[i][0] = keys[i];
			key_write[i][1] = '\0';
		}
	}

	snprintf(key_msg, sizeof(key_msg),
"%s - left  %s - rotate  %s - right  %s - drop  %s - pause  %s - quit  %s - down",
		key_write[0], key_write[1], key_write[2], key_write[3],
		key_write[4], key_write[5], key_write[6]);

	scr_init();
	setup_board();

	pos = A_FIRST*B_COLS + (B_COLS/2)-1;
	nextshape = randshape();
	curshape = randshape();

	scr_msg(key_msg, 1);

	timeval now;
	long int lasttime;
	long int next_update = now.tv_sec * 1000000 + now.tv_usec + fallrate;
	for (;;) {
		place(curshape, pos, 1);
		scr_update();
		place(curshape, pos, 0);
		c = getch();
		gettimeofday(&now, NULL);
		lasttime = now.tv_sec * 1000000 + now.tv_usec;
		if (lasttime >= next_update) {
			if (lasttime >= next_update)
				next_update = lasttime + fallrate;
			/*
			 * Timeout.  Move down if possible.
			 */
			if (fits_in(curshape, pos + B_COLS)) {
				pos += B_COLS;
				continue;
			}

			/*
			 * Put up the current shape `permanently',
			 * bump score, and elide any full rows.
			 */
			place(curshape, pos, 1);
			score++;
			elide();

			/*
			 * Choose a new shape.  If it does not fit,
			 * the game is over.
			 */
			curshape = nextshape;
			nextshape = randshape();
			pos = A_FIRST*B_COLS + (B_COLS/2)-1;
			if (!fits_in(curshape, pos))
				break;
			continue;
		}

		/*
		 * Handle command keys.
		 */
		if (c == keys[5]) {
			/* quit */
			break;
		}
		if (c == keys[4]) {
			static char msg[] =
			    "paused - press [ANY KEY] to continue";

			place(curshape, pos, 1);
			scr_update();
			scr_msg(key_msg, 0);
			scr_msg(msg, 1);
			do {
				usleep(250000);
			} while (getch() == 0);
			scr_msg(msg, 0);
			scr_msg(key_msg, 1);
			place(curshape, pos, 0);
			continue;
		}
		if (c == keys[0]) {
			/* move left */
			if (fits_in(curshape, pos - 1))
				pos--;
			continue;
		}
		if (c == keys[1]) {
			/* turn */
			const struct shape *newblock = &shapes[curshape->rot];

			if (fits_in(newblock, pos))
				curshape = newblock;
			continue;
		}
		if (c == keys[2]) {
			/* move right */
			if (fits_in(curshape, pos + 1))
				pos++;
			continue;
		}
		if (c == keys[3]) {
			/* move to bottom */
			while (fits_in(curshape, pos + B_COLS)) {
				pos += B_COLS;
				score++;
			}
			continue;
		}
		if (c == keys[6]) {
			/* move down */
			if (fits_in(curshape, pos + B_COLS)) {
				pos += B_COLS;
				score++;
			}
			continue;
		}
		if (c == '\f') {
			scr_clear();
			scr_msg(key_msg, 1);
		}
	}

	scr_clear();
	scr_end();

	printw("Your score:  %d point%s  x  level %d  =  %d\n",
	    score, score == 1 ? "" : "s", level, score * level);

	printw("\nHit A to start over\n");

	score = 0;
	while ((i = getch()) != 'a')
		if (i == EOF)
			break;

}

