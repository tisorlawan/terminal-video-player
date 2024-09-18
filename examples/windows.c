#include <ncurses.h>
#include <string.h>

WINDOW *start_ncurses();
void end_ncurses();
WINDOW *create_win(int height, int width, int starty, int startx);
void destroy_win(WINDOW *win);

int main() {
    start_ncurses();

    int height = 10;
    int width = 50;

    int starty = (LINES - height) / 2;
    int startx = (COLS - width) / 2;

    refresh();
    WINDOW *win = create_win(height, width, starty, startx);

    int ch;
    while ((ch = getch()) != KEY_F(1)) {
        switch (ch) {
        case KEY_LEFT:
            destroy_win(win);
            win = create_win(height, width, starty, --startx);
            break;
        case KEY_RIGHT:
            destroy_win(win);
            win = create_win(height, width, starty, ++startx);
            break;
        case KEY_UP:
            destroy_win(win);
            win = create_win(height, width, --starty, startx);
            break;
        case KEY_DOWN:
            destroy_win(win);
            win = create_win(height, width, ++starty, startx);
            break;
        }
    }
    end_ncurses();

    return 0;
}

WINDOW *start_ncurses() {
    WINDOW *win = initscr();
    cbreak();
    noecho();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    return win;
}

void end_ncurses() {
    endwin();
    nocbreak();
}

WINDOW *create_win(int height, int width, int starty, int startx) {
    WINDOW *win = newwin(height, width, starty, startx);
    box(win, 0, 0);

    const char *message = "Include your message";

    int y = height / 2;
    int x = (width - strlen(message)) / 2;

    mvwprintw(win, y, x, "%s", message);

    wrefresh(win);

    return win;
}

void destroy_win(WINDOW *win) {
    wborder(win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    wrefresh(win);
    delwin(win);
}
