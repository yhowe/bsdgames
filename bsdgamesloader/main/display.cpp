#include <stdio.h>
#include <string.h>
#include <curses.h>

#include "./display.h"

#if defined(ESP32)
#include <odroid_go.h>
#endif

#include "debug.h"
#if defined(BT_DEBUG) && defined(ESP32)
#include "btdebug.h"
#endif

int dispLine, currentLine, totalLines;
int promptLine;
bool help_menu = false;

void
printSeperator(int line, int color, const char sep)
{
	int i;

	attrset(COLOR_PAIR(color));
	for (i = 0;i < COLS;i++)
		mvprintw(line, i, "%c", sep);

	return;
}

void
printPrompt(int line, int color, char background, char **prompts, int numPrompts)
{
	if (numPrompts < 1)
		return;

	int i, promptPosition;
	int totalPromptSize = 0;
	for (i = 0; i < numPrompts;i++)
		totalPromptSize += static_cast<int>(strlen(prompts[i]));

	int initSpacing = ((COLS - totalPromptSize)/(numPrompts + 1));
	size_t gap = 0, spacing = static_cast<size_t>(initSpacing);

	printSeperator(line, color, background);
	for( promptPosition = 0; promptPosition < numPrompts;
	    promptPosition++ ) {
		mvprintw(line, static_cast<int>(gap + spacing), "%s", prompts[promptPosition]);
		gap += spacing + strlen(prompts[promptPosition]);
	}

	return;
}

void
printTitle(int color, char *headline)
{
#if defined(ESP32)
	char tmp[255];
	int bpercent;

	bpercent = GO.battery.getPercentage();
	snprintf(tmp, sizeof(tmp), "%s BATT:%03.0d%%\n", headline, bpercent);
	if (bpercent < 60)
		printHeadline(0, 6, tmp);
	else if (bpercent < 80)
		printHeadline(0, color, tmp);
	else
		printHeadline(0, 5, tmp);
#else
	printHeadline(0, color, headline);
#endif

	return;
}

void
printHeadline(int line, int color, char *headline)
{
	printPrompt(line, color, '-', &headline, 1);

	return;
}

void
errorPrint(int line, int color, char *headline)
{
	printPrompt(line, color, ' ', &headline, 1);
	DPRINTF("%s", headline);

	return;
}

void
prettyPrint(int line, int color, char *headline)
{
	printPrompt(line, color, ' ', &headline, 1);

	return;
}

int
clearHome(int orig)
{
	if (help_menu) {
		help_menu = false;
		clear();

		return 2;
	}

	return orig;
}
