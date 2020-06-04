
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

#ifdef ESP32
#include <odroid_go.h>
#include <Arduino.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"
#else
#include <stdlib.h>
#endif
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

#ifndef ESP32
#include <sys/poll.h>
#endif

cell	*board;		/* 1 => occupied, 0 => empty */

int 	accel;
int	Rows, Cols;		/* current screen size */
int	Offset;			/* used to center board & shapes */
int 	Pos = 1;
int	Blocks = 6;
int	BlockSize = 2;
char	Ball = 'o';
int	VertAccel = 1;
int	HorizAccel = 0;
int	PaddleRow, GameStart;

static const struct shape *curshape;
const struct shape *nextshape;

long	fallrate;		/* less than 1 million; smaller => faster */

int	score;			/* the obvious thing */
int	lives;			/* number of turns */
int	level = 1;
gid_t	gid, egid;

char	key_msg[100];
int	showpreview;
int	nocolor;

int 	vol = 0;

static int collision(void);
static void display_vol(void);
static void setup_board(void);
static void redraw_paddle(int, int);
int xpos, ypos, paddlepos, ball_hold, Direction, Horiz;

#ifdef ESP32
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
#endif

static void
display_vol(int vol)
{
	int vollabel = 8;
	mvprintw(LINES - 1, COLS - vollabel, "Vol:%.3d%c", vol * 10, '%');
}

/*
 * Set up the initial board.  The bottom display row is completely set,
 * along with another (hidden) row underneath that.  Also, the left and
 * right edges are set.
 */
static void
redraw_paddle(int oldpos, int newpos)
{
	int i;
	cell *p = &board[B_COLS * (B_ROWS - 1)];
	p += oldpos - 2;
	for (i = 0; i < 5; i++)
		*p++ = 0;
	p = &board[B_COLS * (B_ROWS - 1)];
	p += newpos - 2;
	for (i = 0; i < 5; i++)
		*p++ = 7;
}

static void
setup_board(void)
{
	int i, n;
	cell *p;

	memset(board, 0, SCREENSIZE);
	p = board + B_COLS;
	for (i = 0; i < GameStart; i++) {
		p += 2;
		for (n = 2; n < B_COLS - 2; n++)
			*p++ = i+1;
		p += 2;
	}
}

/*
 * Elide any full active rows.
 */
static int
collision(void)
{
	cell *p;
	int total = 0;
	int mypos;
	int result = 0;
	static bool next_level = false;

	if (ball_hold == 1)
		return result;
	if (ypos <= GameStart) {
		if (accel)
			accel = 1;
	}

	if (xpos <= 0) {
		xpos = 0;
		Horiz = RIGHT;
		accel = 1;
		result = 3;
	}
	if (xpos >= B_COLS - 1) {
		Horiz = LEFT;
		xpos = B_COLS - 1;
		accel = 1;
		result = 3;
	}
	if (ypos <= 0) {
		ypos = 0;
		if (Direction == UP)
			Direction = DOWN;
		else
			Direction = UP;
		result = 3;
	}
	mypos = (xpos / 2) * 2;
	if (ypos > B_ROWS - 2 && Direction == DOWN) {
		lives--;
		ball_hold = 1;
		accel = 0;
		result = 2;
	}

	if (ypos <= GameStart && board[(ypos) * B_COLS + mypos] != 0) {
		board[(ypos) * B_COLS + mypos] = 0;
		board[(ypos) * B_COLS + mypos + 1] = 0;
		total = 0;
		p = board + B_COLS;
		for (int i = 0; i < GameStart; i++) {
			for (int n = 0; n < B_COLS; n++)
				total += *p++;
		}
		if (total == 0)
			next_level = true;
		score += 2 * level;
		if (Direction == DOWN)
			Direction = UP;
		else
			Direction = DOWN;
		if (accel)
			accel = 1;
		result = 1;
		if (vol) {
    			GO.Speaker.setVolume(vol);
			beep();
		}
	}

	if (ypos == B_ROWS - 2  && Direction == DOWN && (xpos >= paddlepos - 2) && xpos <= paddlepos + 2) {
		if (next_level) {
			level++;
			lives++;
			fallrate -= 1000;
			if (fallrate <= 0)
				fallrate = 1000;
			setup_board();
			next_level = false;
		}

		Direction = UP;
		score++;

		return 1;
	}
	return result;
}

#ifdef ESP32
void
setup(void)
{
	GO.begin();
	GO.battery.setProtection(true);
	vol = 0;
}

void
loop(void)
{
#else
int
main(void)
{
#endif
	int pos, c;
	const char *keys;
#define NUMKEYS 7
	char key_write[NUMKEYS][10];
	int ch, i, j;
	int fd;
	int oldpos,newpos;
	Direction = UP;
	Horiz = LEFT;
	ball_hold = 1;
	GameStart = 6;

#ifdef ESP32
	keys = "larbtmd";
#else
	keys = "jklb qd";
#endif
	fallrate = 70000 / level;
	accel = 0;
	lives = 5;

#ifdef ESP32
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

	    timeout(2);
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
#endif
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
"%s - left  %s - fire  %s - right %s - pause  %s - quit",
		key_write[0], key_write[1], key_write[2],
		key_write[4], key_write[5]);

	int oldxpos, oldypos;
	int oldlives, oldscore;
	scr_init();
	setup_board();
	paddlepos = B_COLS / 2;
	xpos = oldpos = paddlepos;
	ypos = B_ROWS - 2;
	oldxpos = xpos;
	oldypos = ypos;
	scr_update();
	redraw_paddle(oldpos, paddlepos);
	scr_ball(oldypos, oldxpos, ypos, xpos, Ball);
	scr_update();

	pos = A_FIRST*B_COLS + (B_COLS/2)-1;

	scr_msg(key_msg, 1);

	timeval now;
	gettimeofday(&now, NULL);
	long int lasttime, next_update;
	lasttime = now.tv_sec * 1000000 + now.tv_usec;
	next_update = lasttime + fallrate;
	char repeat, prev_key = 0;
	while (lives > 0) {
		if (ball_hold) {
			xpos = paddlepos;
			Direction = UP;
			ypos = B_ROWS - 2;
			accel = (millis() % 7) - 3;
		}
		scr_update();
		scr_ball(oldypos, oldxpos, ypos, xpos, Ball);
		redraw_paddle(oldpos, paddlepos);
		oldxpos = xpos;
		oldypos = ypos;
		move(0, COLS - 1);
		c = getch();
#ifndef ESP32
		if (c == ERR && (prev_key == 'j' || prev_key == 'l'))
			ungetch(prev_key);
		prev_key = c;
#endif
		gettimeofday(&now, NULL);
		lasttime = now.tv_sec * 1000000 + now.tv_usec;
		if (lasttime >= next_update) {
			next_update = lasttime + fallrate;
			/*
			 * Timeout.  Move down if possible.
			 */

			/*
			 * Put up the current shape `permanently',
			 * bump score, and elide any full rows.
			 */
			if (ball_hold == 0) {
				collision();
				if (Direction == DOWN)
					ypos++;
				else
					ypos--;
				if (Horiz == RIGHT)
					xpos += accel;
				else
					xpos -= accel;
			}
			continue;
		}

		/*
		 * Handle command keys.
		 */
		if (c == 'v') {
			if (prev_key == 'v')
				continue;
			prev_key = c;
			vol += 2;
			if (vol > 10)
				vol = 0;
			display_vol(vol);
			continue;
		}
		if (c == keys[5]) {
			/* quit */
			break;
		}
		if (c == keys[4]) {
			static char msg[] =
			    "paused - press [ANY KEY] to continue";

			scr_update();
			scr_msg(key_msg, 0);
			scr_msg(msg, 1);
			do {
				usleep(250000);
			} while (getch() == 255);
			scr_msg(msg, 0);
			scr_msg(key_msg, 1);
			continue;
		}
		if (c == keys[0]) {
			/* move left */
			oldpos = paddlepos;
			paddlepos--;
			if (paddlepos <= 2)
				paddlepos = 2;
			if (ypos >= B_ROWS - 2)
			accel--;
			if (accel < -3)
				accel = -3;
			continue;
		}
		if (c == keys[1]) {
			/* turn */
			ball_hold = 0;
			continue;
		}
		if (c == keys[2]) {
			/* move right */
			oldpos = paddlepos;
			paddlepos++;
			if (paddlepos >= B_COLS - 3)
				paddlepos = B_COLS - 3;
			if (ypos >= B_ROWS - 2)
			accel++;
			if (accel > 3)
				accel = 3;
			continue;
		}
		if (c == '\f') {
			scr_clear();
			scr_msg(key_msg, 1);
		}
		prev_key = c;
		usleep(10);
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

#ifndef ESP32
	return EXIT_SUCCESS;
#endif
}

