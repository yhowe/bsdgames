#include <Arduino.h>
#include <odroid_go.h>
#include <esp_err.h>
#include <curses.h>
#include <dirent.h>
#include "err.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#include "display.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

extern "C" {
#include "odroid_settings.h"
}

bool colourOK; 

char *emulatorNames[] = { "Guess", "Brick", "BSDTetris", "Colorbars" };
char *configMenuItems[] = { "EXIT TO GAMES" };
char *emuGamesDir[] = { "roms/nes", "roms", "roms", "roms/a26" };
int oldposition;
volatile bool pressEvent = false;
int position;
int current_directory_count;
int nested_directory_index;
File tmpdir;
char nested_directory_strings[32][256];
char baseDirectory[256];
struct dirent *entries;
char *directory_to_open;
char *file_to_open;
char *command_to_run;
int  program_to_launch;
int  cols;

/* Default Joystick Button Map */
int P1_NUMAXIS = 2;
int P1_NUMBUTTONS = 10;
int P2_NUMAXIS = 2;
int P2_NUMBUTTONS = 10;

char P1_A = 'a';
char P1_B = 'b';
char P1_START = 's';
char P1_SELECT = 't';
int P1_X = 0;
int P1_Y = 3;
char P1_LEFT = 'l';
char P1_RIGHT = 'r';
int P2_A = 1;
int P2_B = 2;
int P2_START = 9;
int P2_SELECT = 8;
int P2_X = 0;
int P2_Y = 3;
int P2_LEFT = 4;
int P2_RIGHT = 5;

bool redo_disp = true;

enum buttonType {
	UP,
	DOWN,
	LEFT,
	RIGHT,
	START,
	SELECT,
	A,
	B,
	X,
	Y,
	SLEFT,
	SRIGHT,
	MAX_JOYBUTTON
};

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
}

static int percentage[2] = { 0, 100 };

char anyJoystickAxis(void) {
	char key;
	key = getch();
	if (key == 'r')
		return RIGHT;
	if (key == 'l')
		return LEFT;
	if (key == 'd')
		return DOWN;
	if (key == 'u')
		return UP;

	return -1;
}

char anyJoystickButton(void) {
	char button;
	if ((button = getch()) == 'm' || button == 't' || button == 'v' ||
	    button == 's' || button == 'a' || button == 'b')
			return button;

	return -1;
}

void calGreeter() {
	clear();

	printSeperator(0, 4, '-');
	prettyPrint(2, 3, "TO ENTER CONFIGURATION MENU");
	prettyPrint(5, 3, "HOLD DPAD RIGHT AND ANY BUTTON");
	prettyPrint(promptLine, 3, "Please Wait...");
	refresh();

	return;
}

void greeter() {
	clear();

	printSeperator(0, 4, '-');
	prettyPrint( 2, 3, "GAMES / EMULATORS");
	prettyPrint( 5, 5, "NESEMU    - NES     ");
	prettyPrint( 6, 5, "GNUBOY    - GB/GBC  ");
	prettyPrint( 8, 5, "SMSPLUSGX - SMS/GG  ");
	prettyPrint( 7, 5, "STELLA    - 2600/VCS");
	prettyPrint(promptLine, 3, "Please Wait...");
	refresh();

	return;
}

void configMenuDisplay() {
	int i, positionOnScreen, startEntry;

	startEntry = ((int)position / (LINES - 3)) * (LINES - 3);
	positionOnScreen = (int)position % (LINES - 3);

	clear();

	attrset(COLOR_PAIR(4));
	printHeadline(0, 4, "Configure / Test Menu");
	printSeperator(LINES - 2, 4, '-');

	prettyPrint(promptLine, 6, "ANY BUTTON TO SELECT ITEM.");
	attrset(COLOR_PAIR(1));
	for (i = 0; i < __arraycount(configMenuItems); i++) {
			mvprintw(i + 1, 0, "%s", configMenuItems[i]);
	}
	attrset(COLOR_PAIR(5));
		mvprintw(positionOnScreen + 1, 0, "%s", configMenuItems[position]);

	refresh();

	return;
}
void emuSelectDisplay() {
	static int oldstart = -1;
	static int oldposition = -1;
	int i, positionOnScreen, startEntry;
#ifdef ALLOW_SHUTDOWN
	char *mainPrompt[] = {"A-Select", "B-Shutdown", "L-Network Config"};
#else
	char *mainPrompt[] = {"A-Select"};
#endif
	startEntry = ((int)position / (LINES - 3)) * (LINES - 3);
	positionOnScreen = (int)position % (LINES - 3);

	if (redo_disp || startEntry != oldstart) {
	redo_disp = false;
	clear();

	printTitle(4, "Select Game");
	printSeperator(LINES - 2, 4, '-');

	printPrompt(promptLine, 6, ' ', mainPrompt, __arraycount(mainPrompt));
	attrset(COLOR_PAIR(1));
	for (i = 0; i < __arraycount(emulatorNames); i++) {
			mvprintw(i + 1, 0, "%s", emulatorNames[i]);
	}
	attrset(COLOR_PAIR(5));
		mvprintw(positionOnScreen + 1, 0, "%s", emulatorNames[position]);

	} else {
	attrset(COLOR_PAIR(1));
		mvprintw(oldposition + 1, 0, "%s", emulatorNames[oldposition]);
	attrset(COLOR_PAIR(5));
		mvprintw(position + 1, 0, "%s", emulatorNames[position]);
	}
	oldposition = position;
	oldstart = startEntry;
	refresh();

	return;
}

void updateDisplay() {
	static int oldstart = -1;
	static int oldposition = -1;
	int i, positionOnScreen, startEntry;
	char toDisp[256];
	int sizetoDisp;
	char *updatePrompt[] = {"A-Select", "B-Back"};
	char *updateRootPrompt[] = {"A-Select", "B-Emulator Selection"};

	sizetoDisp = sizeof(toDisp);
	if (sizetoDisp > cols)
		sizetoDisp = cols;
		
	startEntry = ((int)position / (LINES - 3)) * (LINES - 3);
	positionOnScreen = (int)position % (LINES - 3);

	if (redo_disp || startEntry != oldstart) {
	redo_disp = false;
	clear();

	printTitle(4,"Select Game");
	printSeperator(LINES - 2, 4, '-');

	if (nested_directory_index > 1)
		printPrompt(promptLine, 6, ' ', updatePrompt,
		    __arraycount(updatePrompt));
	else {
		printPrompt(promptLine, 6, ' ', updateRootPrompt,
		    __arraycount(updateRootPrompt));
	}

	attrset(COLOR_PAIR(1));
	for (i = 0; i < LINES-3; i++) {
		if ((i + startEntry) >= current_directory_count) {
			break;
		}
		if (strcmp(entries[startEntry + i].d_name, "") != 0) {
			snprintf(toDisp, sizetoDisp, "%s", entries[startEntry + i].
			    d_name);
			mvprintw(i + 1, 0, "%s", toDisp);
		}
	}
	attrset(COLOR_PAIR(5));
	if (strcmp(entries[position].d_name, "") != 0 &&
	    position < current_directory_count) {
		snprintf(toDisp, sizetoDisp, "%s", entries[position].d_name);
		mvprintw(positionOnScreen + 1, 0, "%s", toDisp);
	}

	} else {
	attrset(COLOR_PAIR(1));
	positionOnScreen = (int)oldposition % (LINES - 3);
	if (strcmp(entries[oldposition].d_name, "") != 0 &&
	    oldposition < current_directory_count) {
		snprintf(toDisp, sizetoDisp, "%s", entries[oldposition].d_name);
		mvprintw(positionOnScreen + 1, 0, "%s", toDisp);
	}

	attrset(COLOR_PAIR(5));
	positionOnScreen = (int)position % (LINES - 3);
	if (strcmp(entries[position].d_name, "") != 0 &&
	    position < current_directory_count) {
		snprintf(toDisp, sizetoDisp, "%s", entries[position].d_name);
		mvprintw(positionOnScreen + 1, 0, "%s", toDisp);
	}
	}
	oldposition = position;
	oldstart = startEntry;
	refresh();

	return;
}

void (*configDisp[])() = { NULL };

void readDirEntries() {

	dirent tmp_entry;
	int i, off;

	off = 0;
	for (i = 0; i < 32 && i < nested_directory_index; i++) {
		snprintf(directory_to_open + off, 4096 - off, "%s/", nested_directory_strings[i]);
		fprintf(stderr, "%s %s %d\n", nested_directory_strings[i], directory_to_open, off);
		off = strlen(directory_to_open);
	}
	directory_to_open[off - 1] = '\0';

		fprintf(stderr, "%s\n", directory_to_open);
	if ((tmpdir = SD.open(directory_to_open, FILE_READ)) == 0) {
		current_directory_count = 0;
		fprintf(stderr, "HERE %s\n", directory_to_open);
	}
	else {
		i = 0;
		while (i < 4096) {
next_entry:
    			File result_entry = tmpdir.openNextFile();
			if (result_entry == 0) {
				break;
			}
			snprintf(entries[i].d_name, sizeof(entries[i].d_name),
			    "%s", result_entry.name() + off);
        		if(result_entry.isDirectory())
				entries[i].d_type = DT_DIR;
			else
				entries[i].d_type = DT_REG;
			if (strcmp(entries[i].d_name, ".") == 0 ||
			    strcmp(entries[i].d_name, "..") == 0)
				goto next_entry;
			i++;
		}

		current_directory_count = i;
	}

	/* Sort Entries */
	int sortpos;

	for (sortpos = 0; sortpos < current_directory_count; sortpos++) {
		for (i = sortpos; i < current_directory_count; i++) {
		if (strcmp(entries[i].d_name, entries[sortpos].d_name) < 0) {
			tmp_entry = entries[sortpos];
			entries[sortpos] = entries[i];
			entries[i] = tmp_entry;
		}
		}
	}
			
	return;
}

void configTestMenu() {
	char axis, button, previousButton = -1, quit;

configAgain:

	quit = 0;
	position = 0;
	oldposition = -1;

	while (!quit) {
		if (position != oldposition) {
			configMenuDisplay();
			oldposition = position;
		}

		axis = anyJoystickAxis();
		button = anyJoystickButton();

		if (axis == DOWN && pressEvent == false) {
			pressEvent = true;
			position++;
			if (position >= __arraycount(configMenuItems))
				position = __arraycount(configMenuItems) - 1;
		} else if (axis == UP && pressEvent == false) {
			pressEvent = true;
			position--;
			if (position < 0)
				position = 0;
		} else if (button != -1 && pressEvent == false) {
				pressEvent = true;
				quit = 1;
		} else if (axis == -1 && button == -1)
			pressEvent = false;
		usleep(100000);
	}

	if ((configDisp[position])) {
		(*configDisp[position])();
		goto configAgain;
	}

	return;
}

void emulatorSelection()
{
	char axis, button, quit = 0;
	emuSelectDisplay();

	while (!quit) {
		if (position != oldposition) {
			emuSelectDisplay();
			oldposition = position;
		}

		axis = anyJoystickAxis();

		button = anyJoystickButton();

		if (axis == DOWN && pressEvent == false) {
			pressEvent = true;
			position++;
			if (position >= __arraycount(emulatorNames))
				position = __arraycount(emulatorNames) - 1;
		} else if (axis == UP && pressEvent == false) {
			pressEvent = true;
			position--;
			if (position < 0)
				position = 0;
#ifdef ALLOW_SHUTDOWN
		} else if (button == P1_B && pressEvent == false) {
			pressEvent = true;
			exit(0);
			break;
#endif
		} else if (button == P1_LEFT && pressEvent == false) {
			pressEvent = true;
			//netInfo();
			oldposition = -1;
		} else if (button == P1_A && pressEvent == false) {
			pressEvent = true;
			quit = 1;
		} else
			pressEvent = false;
		usleep(100000);
	}

	program_to_launch = position + 1;

	odroid_system_application_set(program_to_launch);
	esp_restart();

	return;
}

void quitFunc() {

	endwin();

	return;
}

void
loop(void) {
	bool quit = false;
	char axis, button;

	int configAttempts = 3;
	int configRetries;

	int argc = 2;
	char *argv[2] = { "loader", "" };

	if (argc != 2) {
		errx(EXIT_FAILURE, "usage: %s program root-directory", argv[0]);
	}

	atexit(quitFunc);
	entries = (struct dirent*)malloc(4096 * sizeof(struct dirent));
	directory_to_open = (char*)malloc(4096);
	file_to_open = (char*)malloc(4096);
	command_to_run = (char*)malloc(4096);
	sprintf(baseDirectory,"%s", argv[1]);

	position = oldposition = 0;
	nested_directory_index = 1;
	current_directory_count = -1;

	if (!(initscr())) {
		errx(EXIT_FAILURE, "\nUnknown terminal type.");
	}

	timeout(10);
	colourOK = has_colors();
	promptLine = LINES - 1;
	cols = COLS;

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

#if 0
	calGreeter();
	sleep(2);

	for (configRetries = 0; configRetries < configAttempts; configRetries++) {

		if (anyJoystickAxis() == RIGHT &&
		    anyJoystickButton() != -1) {
			pressEvent = true;
			configRetries = configAttempts;
			configTestMenu();
		}
		usleep(10000);
	}
#endif

	while (!quit) {
		emulatorSelection();
		usleep(10000);
	}
}
