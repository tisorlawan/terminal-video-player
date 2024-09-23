#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ARR_LENGTH(arr) ((sizeof(arr) / sizeof((arr)[0])))

static int colors[] = {
    COLOR_BLACK, COLOR_RED,     COLOR_GREEN, COLOR_YELLOW,
    COLOR_BLUE,  COLOR_MAGENTA, COLOR_CYAN,  COLOR_WHITE,
};

int main() {
    srand(time(NULL));
    initscr();
    if (!has_colors()) {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }

    cbreak();
    noecho();
    start_color();
    curs_set(0);

    for (size_t i = 0; i < ARR_LENGTH(colors); i++) {
        init_pair(i + 1, colors[i], COLOR_BLACK);
    }

    clear();
    for (int y = 0; y < LINES; y++) {
        for (int x = 0; x < COLS; x++) {
            int pair = (rand() % ARR_LENGTH(colors)) + 1;
            move(y, x);
            attron(COLOR_PAIR(pair));
            addch('a');
            attroff(COLOR_PAIR(pair));
        }
    }
    refresh();

    getch();

    endwin();
    return 0;
}
