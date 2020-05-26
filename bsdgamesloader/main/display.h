extern bool help_menu;
extern int dispLine, currentLine, totalLines;
extern int promptLine;
//static char logbuffer[512][20 * 1024];

void printSeperator(int line, int color, const char sep);
void printPrompt(int line, int color, char background,
			char **prompts, int numPrompts);
void printHeadline(int line, int color, char *headline);
void prettyPrint(int line, int color, char *headline);
void errorPrint(int line, int color, char *headline);
void printTitle(int color, char *headline);
int clearHome(int orig);

