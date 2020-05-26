
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
#include "Arduino.h"
#include "odroid_go.h"
#include <curses.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"

/* Game tile definitions. */

char picHeight = 23;
char picStride = 26;

bool colourOK; 
char picSpacingY;
char picSpacingX;

char promptLine;
char promptCol = 2;

float initialWin = 10.00;
float maxWin = 10000.00;
float money;
float tmpMoney;

char resultLabel[2][10][50] = {{
"OO                  OO  OOOOOOOO    OO         OO",
" OO      OOO       OO   OOOOOOOO    OOOOO      OO",
"  OO   OO   OO    OO      OOOO      OOOOOO     OO",
"  OO   OO   OO    OO      OOOO      OO OOOO    OO",
"  OO   OO   OO   OO       OOOO      OO  OOOO   OO",
"  OO   OO   OO   OO       OOOO      OO   OOOO  OO",
"  OO   OO   OO   OO       OOOO      OO    OOOO OO",
"   OO OO     OO OO        OOOO      OO     OOOOOO",
"   OO OO     OO OO     OOOOOOOOO    OO      OOOOO",
"     O         O       OOOOOOOOO    OO       OOOO"
},{
"OO             OOOOOO        OOOOOO     OOOOOOOOO",
"OO           OOOOOOOOOO    OOOOOOOOOO   OOOOOOOOO",
"OO          OOOO    OOOO  OOOO    OOOO  OOO      ",
"OO          OOOO    OOOO  OOOO          OOO      ",
"OO          OOOO    OOOO   OOOOOOOOO    OOOOOOOOO",
"OO          OOOO    OOOO    OOOOOOOOO   OOOOOOOOO",
"OO          OOOO    OOOO          OOOO  OOO      ",
"OO          OOOO    OOOO  OOOO    OOOO  OOO      ",
"OOOOOOOOOO   OOOOOOOOOO    OOOOOOOOOO   OOOOOOOOO",
"OOOOOOOOOO     OOOOOO        OOOOOO     OOOOOOOOO"
}};

char cardSet[5][23][27] = {{
"    HHHHHH     HHHHHH    ",
"  HHHHHHHHH   HHHHHHHHH  ",
"  HHHHHHHHH   HHHHHHHHH  ",
" HHHHHHHHHHH HHHHHHHHHHH ",
" HHHHHHHHHHHHHHHHHHHHHHH ",
" HHHHHHHHHHHHHHHHHHHHHHH ",
" HHHHHHHHHHHHHHHHHHHHHHH ",
" HHHHHHHHHHHHHHHHHHHHHHH ", 
" HHHHHHHHHHHHHHHHHHHHHHH ",
" HHHHHHHHHHHHHHHHHHHHHHH ",
" HHHHHHHHHHHHHHHHHHHHHHH ",
"  HHHHHHHHHHHHHHHHHHHHH  ",
"  HHHHHHHHHHHHHHHHHHHHH  ",
"   HHHHHHHHHHHHHHHHHHH   ",
"   HHHHHHHHHHHHHHHHHHH   ",
"    HHHHHHHHHHHHHHHHH    ",
"     HHHHHHHHHHHHHHH     ",
"      HHHHHHHHHHHHH      ",
"       HHHHHHHHHHH       ",
"        HHHHHHHHH        ",
"         HHHHHHH         ",
"          HHHHH          ",
"           HHH           "
}, { 
"           SSS           ",
"          SSSSS          ",
"         SSSSSSS         ",
"        SSSSSSSSS        ",
"       SSSSSSSSSSS       ",
"      SSSSSSSSSSSSS      ",
"     SSSSSSSSSSSSSSS     ",
"    SSSSSSSSSSSSSSSSS    ", 
"   SSSSSSSSSSSSSSSSSSS   ",
"   SSSSSSSSSSSSSSSSSSS   ",
"  SSSSSSSSSSSSSSSSSSSSS  ",
"  SSSSSSSSSSSSSSSSSSSSS  ",
" SSSSSSSSSSSSSSSSSSSSSSS ",
" SSSSSSSSSSSSSSSSSSSSSSS ",
" SSSSSSSSSSSSSSSSSSSSSSS ",
" SSSSSSSSSSSSSSSSSSSSSSS ",
" SSSSSSSSSSSSSSSSSSSSSSS ",
" SSSSSSSSSSSSSSSSSSSSSSS ",
"  SSSSSSSS SSS SSSSSSSS  ",
"   SSSSSS  SSS  SSSSSS   ",
"    SSS    SSS    SSS    ",
"          SSSSS          ",
"         SSSSSSS         "
}, { 
"          CCCCC          ",
"         CCCCCCC         ",
"        CCCCCCCCC        ",
"       CCCCCCCCCCC       ",
"       CCCCCCCCCCC       ",
"       CCCCCCCCCCC       ",
"       CCCCCCCCCCC       ",
"        CCCCCCCCC        ", 
"         CCCCCCC         ",
"   CCC    CCCCC    CCC   ",
"  CCCCC CCCCCCCCC CCCCC  ",
" CCCCCCCCCCCCCCCCCCCCCCC ",
" CCCCCCCCCCCCCCCCCCCCCCC ",
"CCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCC",
"CCCCCCCCCCCCCCCCCCCCCCCCC",
" CCCCCCCCCCCCCCCCCCCCCCC ",
" CCCCCCCCC CCC CCCCCCCCC ",
"  CCCCCCC  CCC  CCCCCCC  ",
"   CCCCC   CCC   CCCCC   ",
"          CCCCC          ",
"         CCCCCCC         ",
"        CCCCCCCCC        "
}, { 
"           DDD           ",
"          DDDDD          ",
"         DDDDDDD         ",
"        DDDDDDDDD        ",
"       DDDDDDDDDDD       ",
"      DDDDDDDDDDDDD      ",
"     DDDDDDDDDDDDDDD     ",
"    DDDDDDDDDDDDDDDDD    ",
"   DDDDDDDDDDDDDDDDDDD   ",
"  DDDDDDDDDDDDDDDDDDDDD  ",
" DDDDDDDDDDDDDDDDDDDDDDD ",
"DDDDDDDDDDDDDDDDDDDDDDDDD",
" DDDDDDDDDDDDDDDDDDDDDDD ",
"  DDDDDDDDDDDDDDDDDDDDD  ",
"   DDDDDDDDDDDDDDDDDDD   ",
"    DDDDDDDDDDDDDDDDD    ",
"     DDDDDDDDDDDDDDD     ",
"      DDDDDDDDDDDDD      ",
"       DDDDDDDDDDD       ",
"        DDDDDDDDD        ",
"         DDDDDDD         ",
"          DDDDD          ",
"           DDD           "
},{
"                         ",
"   %%%%%%%%%%%%%%%%%%%   ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
" %%%%%%%%%%%%%%%%%%%%%%% ",
"   %%%%%%%%%%%%%%%%%%%   ",
"                         "
}};

int vol;

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

void setup()
{
	GO.begin();
	GO.battery.setProtection(true);
	vol = 0;
}

void
display_vol(int vol)
{
	int vollabel = 8;
	mvprintw(LINES - 3, COLS - vollabel, "Vol:%.3d%c", vol * 10, '%');
}

void
displayPercent(int numerator, int denominator) {

	div_t tmpPercentStorage;
	int wholePercent;
	int decimalPercent;

	if (denominator != 0) {
		tmpPercentStorage = div (( numerator * 100 ), denominator );
		wholePercent = tmpPercentStorage.quot;
		tmpPercentStorage = div (( tmpPercentStorage.rem * 100 ),     \
		    denominator );
		decimalPercent= tmpPercentStorage.quot;
	}
	else {
		wholePercent = 0;
		decimalPercent = 0;
	}

	printw ("(%3d.%1d%c)", wholePercent, decimalPercent,   \
	    '%');

	return;
}

void updateMoneyTotal()
{
	if(colourOK)
		attrset(COLOR_PAIR(1));

	mvprintw(picSpacingY, promptCol, "Total: $%10.2f", money);

	mvprintw(picHeight + picSpacingY, promptCol,
	    "Gambling: $%8.2f", tmpMoney);

	return;
}

void updateHistory(int result)
{
	static unsigned int history = 0;
	unsigned int tmpHistory;
	char displayCharacter = 'H';
	char displayColour = 3;
	int i;

	tmpHistory = history;
	
	if(colourOK)
		attrset(COLOR_PAIR(1));

	mvprintw(picSpacingY, picSpacingX + picStride, "History: ");

	for (i = 0; i < 5; i++) {
		switch (tmpHistory & 0x3) {
		case 0:
			displayCharacter = 'H';
			displayColour = 3;
			break;
		case 1:
			displayCharacter = 'S';
			displayColour = 2;
			break;
		case 2:
			displayCharacter = 'C';
			displayColour = 2;
			break;
		case 3:
			displayCharacter = 'D';
			displayColour = 3;
			break;
		}

		if(colourOK)
			attrset(COLOR_PAIR(displayColour));

		mvprintw(picSpacingY, picSpacingX + picStride + i + 9, "%c",
	    	    displayCharacter);

		tmpHistory = tmpHistory >> 2;
	}

	history = (history << 2) | result;

	if(colourOK)
		attrset(COLOR_PAIR(1));

	return;
}

void updateGuessCounter(int count, int total)
{
	if(colourOK)
		attrset(COLOR_PAIR(1));

	mvprintw(picSpacingY + picHeight - 1 , picSpacingX + picStride 
	    , "Turn: %5d/%d", count, total);

	return;
}

void loop()
{
	/* Runtime game variables. */
	int turn;
	int turns = -1;
	char card = 0;
	char guess = 0;
	char result = 0;
	char correct = 1;
	char oldkey = 0;

	money = 0.0;

	/* Intermediate variables. */
	char buff[10];
	div_t mystery;
	div_t mysteryTime;
	struct timeval timesx; 
	long usecs;
	int i;

	tmpMoney = initialWin;

	/* Display variables. */
	char answer[10]; 
	char picLine;

	/* Statistical Variables. */

	int jackpot_counter = 0;
	int guessesClubs = 0;
	int guessesHearts = 0;
	int guessesDiamonds = 0;
	int guessesSpades = 0;

	int guessesRed = 0;
	int guessesBlack = 0;

	int guessesCorrect = 0;

	int guessesClubsCorrect = 0;
	int guessesDiamondsCorrect = 0;
	int guessesHeartsCorrect = 0;
	int guessesSpadesCorrect = 0;

	int guessesRedCorrect = 0;
	int guessesBlackCorrect = 0;
  
	colourOK = has_colors();
	promptLine = LINES - 1;
	picSpacingX = (COLS - picStride) / 2;
	picSpacingY = (LINES - 2 - picHeight) / 2;

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

	timeout(-1);
	if (colourOK) {
		start_color();
 
		init_pair(1, COLOR_WHITE, COLOR_BLACK);
		init_pair(2, COLOR_BLACK, COLOR_WHITE);
		init_pair(3, COLOR_RED, COLOR_WHITE);
		init_pair(4, COLOR_BLUE, COLOR_WHITE);
		init_pair(5, COLOR_WHITE, COLOR_GREEN);
		init_pair(6, COLOR_WHITE, COLOR_RED);

		attrset(COLOR_PAIR(1));
	}

	turns = 50;

	clear();

	/* Display blank card. */
	if (colourOK)
		attrset(COLOR_PAIR(4));

	move(0, 0);

	for (picLine = 0; picLine < picHeight; picLine++) {
		mvprintw(picLine + picSpacingY, picSpacingX, "%s",            \
		    &cardSet[4][picLine][0]);
	}

	updateMoneyTotal();

	refresh();

	/* Main loop. */
	for (turn = 0; turn < turns; turn++) {
		gettimeofday(&timesx, 0);
		usecs = timesx.tv_usec;

		usecs = usecs >> 3;
		if (usecs == 0)
			usecs = 1;

		mystery = div(millis() , 4);
		card = mystery.rem;

		if(colourOK)
			attrset(COLOR_PAIR(1));
		oldkey = getch();
		guess = oldkey;
select_again:
		guess = 0;
		oldkey = 0;
		mvprintw(promptLine-1, promptCol,                       \
		    "Guess d-black u-red b-clubs r-diamonds l-hearts");
		mvprintw(promptLine, promptCol,                       \
		    "    a-spades s-BANK or m-quit: BATT: %3d%% ",
		    GO.battery.getPercentage());
		while (guess == oldkey || (guess != 'l' && guess != 'r' && guess != 'u' && guess  \
		    != 'd' && guess !='a' && guess !='b' && guess != 'm'
		    && guess != 's' && guess != 'v')) {
			guess = oldkey;
			fflush(stdout);
			refresh();

			fflush(stdin);
			oldkey = getch();

		}

		if (guess == 'v') {
			vol += 2;
			if (vol > 10)
				vol = 0;
			display_vol(vol);
			goto select_again;
		}
		/* 'j' pressed. update money guess again. */
		if (guess == 's') {
			if (jackpot_counter > 0) {
				money += tmpMoney;
				tmpMoney = initialWin;
				jackpot_counter = 0;

				updateMoneyTotal();
			}
			goto select_again;
		}


		clear();
		/* 'q' pressed. Exit, show statistics. */
		if (guess == 'm')
			break;

		/* Check guess, alter statistics counters. */
		switch (card) {
		case 0:
			guessesHearts++;
			guessesRed++;

			if (guess == 'l') {
				guessesHeartsCorrect++;
				guessesRedCorrect++;
				result = correct;
				tmpMoney = tmpMoney * 4;
			}

			if (guess == 'u') {
				guessesRedCorrect++;
				result = correct;
				tmpMoney = tmpMoney * 2;
			}
			break;
		case 1:
			guessesSpades++;
			guessesBlack++;

			if (guess == 'a') {
				guessesSpadesCorrect++;
				guessesBlackCorrect++;
				result = correct;
				tmpMoney = tmpMoney * 4;
			}

			if (guess == 'd') {
				guessesBlackCorrect++;
				result = correct;
				tmpMoney = tmpMoney * 2;
			}
			break;
		case 2:
			guessesClubs++;
			guessesBlack++;

			if (guess == 'b') {
				guessesClubsCorrect++;
				guessesBlackCorrect++;
				result = correct;
				tmpMoney = tmpMoney * 4;
			}

			if (guess == 'd') {
				guessesBlackCorrect++;
				result = correct;
				tmpMoney = tmpMoney * 2;
			}
			break;
		case 3:
			guessesDiamonds++;
			guessesRed++;

			if (guess == 'r') {
				guessesDiamondsCorrect++;
				guessesRedCorrect++;
				result = correct;
				tmpMoney = tmpMoney * 4;
			}

			if (guess == 'u') {
				guessesRedCorrect++;
				result = correct;
				tmpMoney = tmpMoney * 2;
			}
			break;
		default:
			endwin();
		}

		/* Display card. */
		if (colourOK) {
			if (card == 0 || card == 3)
				attrset(COLOR_PAIR(3));
			else
				attrset(COLOR_PAIR(2));
		}

		move(0, 0);

		for (picLine = 0; picLine < picHeight; picLine++)
			mvprintw(picLine + picSpacingY, picSpacingX, "%s",    \
			    &cardSet[card][picLine][0]);

		refresh();
    
		updateHistory(card);
		updateGuessCounter(turn + 1, turns);

		/* Display result label. */
    		GO.Speaker.setVolume(vol);
		if (result == correct)
			GO.Speaker.tone(1000, 200);
		else
			GO.Speaker.tone(300, 200);
    		//GO.Speaker.mute();
		if (result == correct) {
			jackpot_counter++;
			if (colourOK)
				attrset(COLOR_PAIR(5));
			card = 0;
		}
		else {
			jackpot_counter = 0;
			tmpMoney = initialWin;
			if (colourOK)
				attrset(COLOR_PAIR(6));
			card = 1;
		}
    
		move(0, 0);

    		for (picLine = 0; picLine < 10; picLine++) {
      			for (i = 0; i < 49; i ++) {
				if (resultLabel[card][picLine][i] != ' ')
          				mvprintw(picLine + picSpacingY + 6,   \
					    i, "+");
      			}
    		}

		/* Display outcome, update statistics counters. */
		if (result == correct) {
			strncpy(answer, "Correct!", sizeof(answer));
			guessesCorrect++;

			/* Reset result flag. */
			result = 0;
		}
		else
			strncpy(answer, "INCORRECT", sizeof(answer));

		if (colourOK)
			attrset(COLOR_PAIR(1));

		mvprintw(picHeight + picSpacingY, picSpacingX                \
		    + (picStride / 2), "      %s\n", answer);

		updateMoneyTotal();
		refresh();

		if (jackpot_counter > 4) {
			jackpot_counter = 0;
			if (tmpMoney > maxWin)
				money +=maxWin;
			else
				money += tmpMoney;
			tmpMoney = initialWin;

			updateMoneyTotal();
		}
	}

	if (jackpot_counter > 0)
		money += tmpMoney;
  
	/* Statistical display. */

	if (colourOK)
		attrset(COLOR_PAIR(1));

	mvprintw(promptLine, promptCol,                                       \
	    "Press [any key] to display statistics: ");
	fflush(stdout);
	refresh();

	fflush(stdin);
	oldkey = getch();
	guess = oldkey;
	while (guess == oldkey) {
		guess = oldkey;
		oldkey = getch();
	}

	clear();
	move( 0, 0 );

	printw("\n       TEST STATISTICS");
	printw("\n=============================");
	printw("\n");
	printw("\n Total money      : $%10.2f", money);
	printw("\n Total shown      : %5d.  Correct: %5d.", turn,            \
	    guessesCorrect);
	displayPercent(guessesCorrect, turn);
	printw("\n");
	printw("\n   Card Colour Statistics.");
	printw("\n=============================");
	printw("\n");
	printw("\n Black cards shown: %2d.  Correct: %2d.", guessesBlack,    \
	    guessesBlackCorrect);
	displayPercent(guessesBlackCorrect, guessesBlack);
	printw("\n");
	printw("\n Red cards shown  : %2d.  Correct: %2d.", guessesRed,      \
	    guessesRedCorrect);
	displayPercent(guessesRedCorrect, guessesRed);
	printw("\n");
	printw("\n Individual Suit Statistics.");
	printw("\n=============================");
	printw("\n");
	printw("\n Clubs shown      : %2d.  Correct: %2d.", guessesClubs,    \
	    guessesClubsCorrect);
	displayPercent(guessesClubsCorrect, guessesClubs);
	printw("\n");
	printw("\n Diamonds shown   : %2d.  Correct: %2d.", guessesDiamonds, \
	    guessesDiamondsCorrect);
	displayPercent(guessesDiamondsCorrect, guessesDiamonds);
	printw("\n");
	printw("\n Hearts shown     : %2d.  Correct: %2d.", guessesHearts,   \
	    guessesHeartsCorrect);
	displayPercent(guessesHeartsCorrect, guessesHearts);
	printw("\n");
	printw("\n Spades shown     : %2d.  Correct: %2d.", guessesSpades,   \
	    guessesSpadesCorrect);
	displayPercent(guessesSpadesCorrect, guessesSpades);
	printw("\n");

	mvprintw(promptLine, promptCol,"Press [any key] to exit: ");
	fflush(stdout);
	refresh();

	fflush(stdin);
	oldkey = getch();
	guess = oldkey;
	while (guess == oldkey) {
		guess = oldkey;
		oldkey = getch();
	}

	guess = oldkey;
	while (guess == oldkey) {
		guess = oldkey;
		oldkey = getch();
	}

	endwin();
}

