/*	$NetBSD: colorbars.c,v 1.1 2012/06/06 00:13:36 christos Exp $	*/

/*-
 * Copyright (c) 2012 Nathanial Sloss <nathanialsloss@yahoo.com.au>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: colorbars.c,v 1.1 2012/06/06 00:13:36 christos Exp $");

#include <odroid_go.h>
#include <Arduino.h>
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include "err.h"
#include <stdlib.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"

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

void
setup(void)
{
	GO.begin();
	GO.battery.setProtection(true);
}

void
loop(void)
{
	static struct colorInfo {
		const char *name;
		int color;
	} colorInfo[] = {
		{ "Black", COLOR_BLACK },
		{ "Red", COLOR_RED },
		{ "Green", COLOR_GREEN },
		{ "Yellow", COLOR_YELLOW },
		{ "Blue", COLOR_BLUE },
		{ "Magenta", COLOR_MAGENTA },
		{ "Cyan", COLOR_CYAN },
		{ "White", COLOR_WHITE },
	};
	size_t lengths[__arraycount(colorInfo)];

	static const size_t numcolors = __arraycount(colorInfo);
	size_t labelwidth;

	int colorOK;
	int spacing, offsetx, labeloffsety, labeloffsetx;

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
	if (!initscr())
		errx(EXIT_FAILURE, "Cannot initialize curses");

	colorOK = has_colors();
	if (!colorOK) {
		endwin();
		errx(EXIT_FAILURE, "Terminal cannot display color");
	}

	if (COLS < 45 || LINES < 10) {
		endwin();
		errx(EXIT_FAILURE, "Terminal size must be at least 45x10.");
	}

	spacing = COLS / numcolors;
	offsetx = (COLS - (spacing * numcolors)) / 2;


	start_color();

	labelwidth = 0;
	for (size_t i = 0; i < numcolors; i++) {
		lengths[i] = strlen(colorInfo[i].name);
		if (lengths[i] > labelwidth)
			labelwidth = lengths[i];
		init_pair(i, COLOR_WHITE, colorInfo[i].color);
	}
		init_pair(7, COLOR_BLACK, COLOR_WHITE);

	labeloffsetx = spacing / 2;
	labeloffsety = (LINES - 1 - labelwidth) / 2;
	clear();

	move(0, 0);

	for (size_t i = 0; i < numcolors; i++) {
		int xoffs = offsetx + spacing * i;

		attrset(COLOR_PAIR(i));

		for (int line = 0; line < LINES - 1; line++)
			for (int xpos = 0; xpos < spacing; xpos++)
			       mvprintw(line, xoffs + xpos, " ");

		attrset(COLOR_PAIR(0));

		xoffs += labeloffsetx;
		for (size_t line = 0; line < lengths[i]; line++)
			mvprintw(line + labeloffsety, xoffs, "%c",
			    colorInfo[i].name[line]);
	}

	attrset(COLOR_PAIR(0));

	mvprintw(LINES - 1, 0, "ANSI Color chart - Press any key to exit: ");

	refresh();

	while (true)
		getch();

	endwin();

}

