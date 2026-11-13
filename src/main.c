#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <termios.h>
#include <stdint.h>

#include "utils/types.h"
#include "memory/mem_arena.h"

#define VERSION "0.0.1"
#define ABUF_INIT {     \
    .b = NULL,          \
    .len = 0,           \
    .arena = NULL,      \
}

typedef struct {
    char *b;
    u32 len;
    mem_arena *arena;
} abuf;

typedef struct {
    u32 x, y;
} pos;

typedef struct {
    u32 size;
    char *chars;
} erow;

struct editor_config
{
    u32 cx, cy;
    u32 vx, vy;
    u32 rowoff;
    u32 screenrows, screencols;
    u32 numrows;
    erow *row;
    i8 mode;
    bool is_new_file;
    char *cmd;
    char *cmd_info;
    struct termios orig_termios;
};

struct editor_config E;

void debug_start();

void die(const char *s);

void disable_raw_mode();
void enable_raw_mode();

char read_key();

void mark_file_old();

void editor_insert_char(const char c);
void editor_add_line_below();

u32 get_max_cx();

u32 is_anchor_before_cursor(void);

pos get_editor_selection_start(void);
pos get_editor_selection_end(void);

void editor_move_to_next_word();
void editor_move_to_next_word_end();
void editor_move_to_prev_word();

void editor_move_cursor(const char key);

void handle_key_press();

void editor_append_row(char *s, size_t len);

void editor_open(char *filename);

u32 get_window_size(u32 *rows, u32 *cols);

void ab_append(abuf *ab, const char *s, u32 len);
void ab_free(abuf *ab);

void editor_scroll();

void editor_draw_rows(abuf *ab);

void editor_draw_command_line(abuf *ab);

void editor_clear_screen(mem_arena *arena);

void init_editor();

int main(int argc, char *argv[]) {
    bool debug = 0;
    char *file_path = NULL;

    for(u32 i = 0; i < argc; i++) {
        if(strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        }

        if(strcmp(argv[i], "--f") == 0 && (i + 1 < argc)) {
            file_path = argv[i + 1];
            i++;
        }
    }

    if(debug == 1) {
        debug_start();
    }

    enable_raw_mode();

    init_editor();

    if(file_path != NULL) {
        editor_open(file_path);
    } else {
        editor_append_row("~", 0);
    }

    mem_arena *arena = create_arena(MiB(1), 1);

    while(1) {
        editor_clear_screen(arena);
        handle_key_press();
    }

    arena_destroy(arena);

    return 0;
}

void debug_start() {
    mkfifo("/tmp/log", 0600);
    system("i3-sensible-terminal -- cat /tmp/log &");
    freopen("/tmp/log", "w", stderr);
    setvbuf(stderr, NULL, _IONBF, 0);
    unlink("/tmp/log");
    signal(SIGPIPE, SIG_IGN);
}

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disable_raw_mode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tsetattr");
    }
}

void enable_raw_mode() {
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }

    atexit(disable_raw_mode);

    struct termios raw = E.orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

char read_key() {
    long nread;
    char c;

    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1)
        {
            die("read");
        }
    }

    return c;
}

void mark_file_old() {
    if(E.is_new_file) {
        E.is_new_file = false;
    }
}

void editor_insert_char(const char c) {
    mark_file_old();

    erow *row = &E.row[E.cy];

    fprintf(stderr, "Insert %c\n", c);

    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[E.cx + 1], &row->chars[E.cx], row->size - E.cx + 1);
    row->chars[E.cx] = c;

    row->size++;
    E.cx++;
}

void editor_add_line_below() {
    mark_file_old();

    u32 at = E.cy + 1;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    E.row[at].size = 0;
    E.row[at].chars = malloc(1);
    E.row[at].chars[0] = '\0';
    E.numrows += 1;
    E.cy += 1;
    E.cx = 0;
}

u32 get_max_cx() {
    u32 size = E.row[E.cy].size;

    return size > 0 ? size - 1 : 0;
}

u32 is_anchor_before_cursor() {
    return E.vy < E.cy || (E.vy == E.cy && E.vx < E.cx);
}

pos get_editor_selection_start() {
    if (is_anchor_before_cursor()) {
        return (pos){ E.vx, E.vy };
    }

    return (pos){ E.cx, E.cy };
}

pos get_editor_selection_end() {
    if (is_anchor_before_cursor()) {
        return (pos){ E.cx, E.cy };
    }

    return (pos){ E.vx, E.vy };
}

void editor_move_to_next_word() {
    bool skipped_line = false;

    if(E.cx == E.row[E.cy].size - 1 && E.cy < E.numrows - 1) {
        skipped_line = true;
        E.cx = 0;
        E.cy += 1;
    }

    for(u32 i = E.cx; i < E.row[E.cy].size; i++) {
        if(E.row[E.cy].chars[E.cx] == ' ' && E.row[E.cy].chars[i] != ' ') {
            E.cx = fmin(i, get_max_cx());
            break;
        }

        if(E.row[E.cy].chars[E.cx] != ' ' && E.row[E.cy].chars[i] == ' ') {
            while(E.row[E.cy].chars[i] == ' ' && i < E.row[E.cy].size) {
                i += 1;
            }

            E.cx = fmin(i, get_max_cx());
            break;
        }

        if(skipped_line == false && i == E.row[E.cy].size - 1) {
            E.cx = fmin(E.row[E.cy].size, get_max_cx());
            break;
        }
    }
}

void editor_move_to_next_word_end() {
    if(E.cx == E.row[E.cy].size - 1 && E.cy < E.numrows - 1) {
        E.cx = 0;
        E.cy += 1;
    }

    for(u32 i = E.cx; i < E.row[E.cy].size; i++) {
        if(E.row[E.cy].chars[E.cx] == ' ' && E.row[E.cy].chars[i] != ' ') {
            i += 1;
            while(E.row[E.cy].chars[i] != ' ' && i < E.row[E.cy].size) {
                i += 1;
            }

            E.cx = fmin(i - 1, get_max_cx());
            break;
        }

        if(E.row[E.cy].chars[E.cx] != ' ' && E.row[E.cy].chars[i] == ' ') {
            if(E.cx == i - 1) {
                while(E.row[E.cy].chars[i] == ' ' && i < E.row[E.cy].size) {
                    i += 1;
                }

                i += 1;

                while(E.row[E.cy].chars[i] != ' ' && i < E.row[E.cy].size) {
                    i += 1;
                }

                E.cx = fmin(i - 1, get_max_cx());
            } else {
                E.cx = fmin(i - 1, get_max_cx());
            }

            break;
        }

        if(i == E.row[E.cy].size - 1) {
            E.cx = fmin(E.row[E.cy].size, get_max_cx());
            break;
        }
    }
}

void editor_move_to_prev_word() {
    if(E.cx == 0 && E.cy > 0) {
        E.cy -= 1;
        E.cx = get_max_cx();
        return;
    }

    for(u32 i = E.cx; i > 0; i--) {
        if(E.row[E.cy].chars[E.cx] == ' ' && E.row[E.cy].chars[i] != ' ') {
            E.cx = fmin(i, get_max_cx());
            break;
        }

        if(E.row[E.cy].chars[E.cx] != ' ' && E.row[E.cy].chars[i] == ' ') {
            while(E.row[E.cy].chars[i] == ' ' && i > 0) {
                i -= 1;
            }

            if(i == 0 && E.cy > 0) {
                E.cy -= 1;
                E.cx = get_max_cx();
            } else {
                E.cx = i;
            }

            break;
        }

        if(i == 1 && E.cy > 1) {
            E.cy -= 1;
            E.cx = get_max_cx();
            break;
        }

        if(i == 1 && E.cy == 0) {
            E.cx = 0;
        }
    }
}

void editor_move_cursor(const char key) {
    switch(key) {
        case 'h': {
            if(E.cx != 0) {
               E.cx--;
            }

            break;
        }
        case 'j': {
            if(E.cy < E.numrows - 1) {
               E.cy++;
            }

            if(E.cx >= get_max_cx()) {
                E.cx = get_max_cx();
            }

            break;
        }
        case 'k': {
            if(E.cy != 0) {
               E.cy--;
            }

            if(E.cx >= get_max_cx()) {
                E.cx = get_max_cx();
            }

            break;
        }
        case 'l': {
            if(E.cx != E.screencols - 1 && E.cx < get_max_cx()) {
               E.cx++;
            }

            break;
        }
    }
}

void handle_key_press() {
    char c = read_key();

    if(strlen(E.cmd_info) > 0) {
        E.cmd_info = "";
    }

    fprintf(stderr, "Pressed %d\n", (unsigned char)c);

    if(E.mode == -1) {
        switch(c) {
            // <esc>
            case 27:
                E.mode = 0;
                break;
            // <enter>
            case 10:
                if(strcmp(E.cmd, "w") == 0) {
                    E.cmd_info = "FILE SAVED";

                    E.cmd = realloc(E.cmd, 1);
                    E.cmd[0] = '\0';
                    E.mode = 0;
                } else if(strcmp(E.cmd, "q") == 0) {
                    write(STDOUT_FILENO, "\x1b[2J", 4);
                    write(STDOUT_FILENO, "\x1b[H", 3);
                    exit(0);
                } else {
                    E.cmd_info = "INVALID COMMAND";

                    E.cmd = realloc(E.cmd, 1);
                    E.cmd[0] = '\0';
                    E.mode = 0;
                }
                break;
            default:
                if(isprint((unsigned char)c)) {
                    u32 len = strlen(E.cmd);
                    E.cmd = realloc(E.cmd, len + 2);
                    E.cmd[len] = c;
                    E.cmd[len + 1] = '\0';

                    fprintf(stderr, "CMD: %s\n", E.cmd);
                }
        }
    }

    if(E.mode == 0) {
        switch(c) {
            // :
            case 58:
                E.mode = -1;
                break;
            case 'q':
                write(STDOUT_FILENO, "\x1b[2J", 4);
                write(STDOUT_FILENO, "\x1b[H", 3);
                exit(0);
                break;
            case 'i':
                E.mode = 1;
                break;
            case 'a':
                E.mode = 1;
                E.cx += 1;
                break;
            case 'v':
                E.mode = 2;
                E.vx = E.cx;
                E.vy = E.cy;
                break;
            case 'h':
            case 'j':
            case 'k':
            case 'l':
                editor_move_cursor(c);

                if(E.cy < E.numrows && E.cx < E.row[E.cy].size) {
                    fprintf(stderr, "Current char: %c\n", E.row[E.cy].chars[E.cx]);
                }

                break;
            case 'w':
                editor_move_to_next_word();

                break;
            case 'e':
                editor_move_to_next_word_end();

                break;
            case 'b':
                editor_move_to_prev_word();

                break;
            case 'o': {
                editor_add_line_below();
                E.mode = 1;

                break;
            }
        }

        return;
    }

    if(E.mode == 1) {
        switch(c) {
            // <esc>
            case 27:
                E.mode = 0;
                E.cx = get_max_cx();
                break;
            default:
                if(isprint((unsigned char)c)) {
                    editor_insert_char(c);
                };
                break;
        }

        return;
    }

    if(E.mode == 2) {
        switch(c) {
            // <esc>
            case 27:
                E.mode = 0;
                break;
            case 'h':
            case 'j':
            case 'k':
            case 'l':
                editor_move_cursor(c);
                break;
            case 'w':
                editor_move_to_next_word();
                break;
            case 'e':
                editor_move_to_next_word_end();
                break;
            case 'b':
                editor_move_to_prev_word();
                break;
        }
    }
}

void editor_append_row(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    u32 at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.numrows++;
}

void editor_open(char *filename) {
    E.is_new_file = false;

    FILE *fp = fopen(filename, "r");
    if(!fp) {
        die("fopen");
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line, &linecap, fp)) != -1) {
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            linelen--;
        }

        editor_append_row(line, linelen);
    }

    free(line);
    fclose(fp);
}

u32 get_window_size(u32 *rows, u32 *cols) {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;

        return 0;
    }
}

void ab_append(abuf *ab, const char *s, u32 len) {
    char *new = arena_push(ab->arena, len);
    // char *new = realloc(ab->b, ab->len + len);

    if(new == NULL) {
        return;
    }

    if(ab->b == NULL) {
        ab->b = new;
    }

    memcpy(new, s, len);
    ab->len += len;
}

void ab_free(abuf *ab) {
    arena_clear(ab->arena);
}

void editor_scroll() {
    if(E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if(E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
}

void editor_draw_rows(abuf *ab) {
    for (u32 y = 0; y < E.screenrows; y++) {
        u32 filerow = y + E.rowoff;
        if(filerow >= E.numrows) {
            if(E.is_new_file == true && y == E.screenrows / 3) {
               char welcome[80];
               u32 welcome_len = snprintf(welcome, sizeof(welcome), "Editor -- version %s", VERSION);

               if(welcome_len > E.screencols) {
                   welcome_len = E.screencols;
               }

               u32 padding = (E.screencols - welcome_len) / 2;

               if(padding) {
                  ab_append(ab, "~", 1);
                  padding--;
               }

               while(padding--) {
                   ab_append(ab, " ", 1);
               }

               ab_append(ab, welcome, welcome_len);
            } else {
                ab_append(ab, "~", 1);
            }
        } else {
            pos s = get_editor_selection_start();
            pos e = get_editor_selection_end();

            erow *row = &E.row[filerow];
            u32 len = row->size;

            if(len > E.screencols) {
                len = E.screencols;
            }

            if(E.mode != 2 || filerow < s.y || filerow > e.y) {
                ab_append(ab, row->chars, len);
            } else {
                u32 from = (filerow == s.y) ? s.x : 0;
                u32 to   = (filerow == e.y) ? e.x + 1 : len;

                if(from > len) from = len;
                if(to > len)   to = len;

                ab_append(ab, row->chars, from);
                ab_append(ab, "\x1b[7m", 4);
                ab_append(ab, row->chars + from, to - from);
                ab_append(ab, "\x1b[27m", 5);
                ab_append(ab, row->chars + to, len - to);
            }
        }

        ab_append(ab, "\x1b[K", 3);
        ab_append(ab, "\r\n", 2);
    }
}

void editor_draw_command_line(abuf *ab) {
    if(strlen(E.cmd_info) > 0) {
        ab_append(ab, ":", 1);
        ab_append(ab, E.cmd_info, strlen(E.cmd_info));
        ab_append(ab, "\x1b[K", 3);
        return;
    }

    if(E.mode == -1) {
        ab_append(ab, ":", 1);
        ab_append(ab, E.cmd, strlen(E.cmd));
    }

    if(E.mode == 0) {
        ab_append(ab, "-- NORMAL --", 12);
    }

    if(E.mode == 1) {
        ab_append(ab, "-- INSERT --", 12);
    }

    if(E.mode == 2) {
        ab_append(ab, "-- VISUAL --", 12);
    }

    ab_append(ab, "\x1b[K", 3);
}

void editor_clear_screen(mem_arena *arena) {
    editor_scroll();

    abuf ab = ABUF_INIT;
    ab.arena = arena;

    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);
    editor_draw_command_line(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, E.cx + 1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

void init_editor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.numrows = 0;
    E.mode = 0;
    E.is_new_file = true;
    E.cmd = calloc(1, 1);
    E.cmd_info = "";
    E.row = NULL;

    if(get_window_size(&E.screenrows, &E.screencols) == -1) {
        die("get_window_size");
    }

    E.screenrows -= 5;
}
