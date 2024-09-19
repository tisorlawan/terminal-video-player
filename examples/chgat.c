#include "ncurses.h"
#include <stdlib.h>

int main() {
    initscr();
    start_color();

    init_pair(1, COLOR_BLUE, COLOR_GREEN);

    for (int i = 0; i < LINES; i++) {
        mvchgat(i, 0, -1, A_NORMAL, 1, NULL);
    }

    mvchgat(0, 0, -1, A_BLINK, 1, NULL);
    printw("NICE ONE");

    refresh();
    getch();
    endwin();
}
