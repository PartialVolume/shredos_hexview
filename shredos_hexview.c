#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <ncurses.h>

#define MAX_PATH 512

#define CP_BORDER 1
#define CP_TITLE  2

typedef struct {
    char name[64];
    char path[MAX_PATH];
    char model_serial[128];
    int fd;
    off_t size;
    off_t offset;
} drive_info_t;

// Function declarations
void init_ncurses_ui(void);
void get_model_and_serial(const char *dev_name, char *dest, size_t max_len);
int get_drive_list(drive_info_t drives[], int max_drives);
void close_drives(drive_info_t drives[], int count);
void render_hex_view(WINDOW *win, drive_info_t *drive);
int get_dynamic_bytes_per_row(WINDOW *win);
off_t handle_goto_popup(drive_info_t *drive, int bytes_per_row);

int main(int argc, char *argv[]) {
    drive_info_t drives[64];
    int drive_count = 0;
    int selected_drive = 0;
    int scroll_offset = 0;
    int ch;

    // Initialize ncurses & colors
    init_ncurses_ui();

    // Initial drive discovery
    drive_count = get_drive_list(drives, 64);

    // Layout configuration
    int height = LINES;
    int width = COLS;
    int title_height = 3;
    int help_height = 1; // Frameless bottom line
    int main_height = height - title_height - help_height;
    int left_width = 32;

    // Create windows
    WINDOW *win_title = newwin(title_height, width, 0, 0);
    WINDOW *win_left  = newwin(main_height, left_width, title_height, 0);
    WINDOW *win_right = newwin(main_height, width - left_width, title_height, left_width);
    WINDOW *win_help  = newwin(help_height, width, height - 1, 0);

    keypad(stdscr, TRUE);
    keypad(win_left, TRUE);
    keypad(win_right, TRUE);

    int running = 1;
    while (running) {
        // Ensure selected index is within bounds
        if (drive_count > 0 && selected_drive >= drive_count) {
            selected_drive = drive_count - 1;
        }
        if (selected_drive < 0) {
            selected_drive = 0;
        }

        // --- Render Top Title Box ---
        wclear(win_title);
        wattron(win_title, COLOR_PAIR(CP_BORDER));
        box(win_title, 0, 0);
        wattroff(win_title, COLOR_PAIR(CP_BORDER));

        wattron(win_title, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(win_title, 1, 2, " ShredOS hex/ascii viewer v1.1");
        wattroff(win_title, COLOR_PAIR(CP_TITLE) | A_BOLD);
        wrefresh(win_title);

        // --- Render Left Pane (Scrollable Drive List with Model / Serial) ---
        wclear(win_left);
        wattron(win_left, COLOR_PAIR(CP_BORDER));
        box(win_left, 0, 0);
        wattroff(win_left, COLOR_PAIR(CP_BORDER));

        wattron(win_left, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(win_left, 0, 2, " Devices ");
        wattroff(win_left, COLOR_PAIR(CP_TITLE) | A_BOLD);

        // Calculate how many drive items (2 lines each) fit in the left pane
        int visible_slots = (main_height - 2) / 2;
        if (visible_slots < 1) visible_slots = 1;

        // Adjust scroll offset to keep selected drive in view
        if (selected_drive < scroll_offset) {
            scroll_offset = selected_drive;
        } else if (selected_drive >= scroll_offset + visible_slots) {
            scroll_offset = selected_drive - visible_slots + 1;
        }

        // Top "More" indicator if items exist above view
        if (scroll_offset > 0) {
            wattron(win_left, COLOR_PAIR(CP_TITLE));
            mvwprintw(win_left, 0, 11, "(More ^)");
            wattroff(win_left, COLOR_PAIR(CP_TITLE));
        }

        if (drive_count == 0) {
            mvwprintw(win_left, 2, 2, "No drives found.");
        } else {
            int current_row = 1;
            for (int i = scroll_offset; i < drive_count && (current_row + 1 < main_height - 1); i++) {
                if (i == selected_drive) {
                    wattron(win_left, A_REVERSE);
                }

                // Line 1: Device Name (e.g. sda)
                mvwprintw(win_left, current_row, 1, " %-29s", drives[i].name);
                current_row++;

                // Line 2: Model / Serial
                if (current_row < main_height - 1) {
                    mvwprintw(win_left, current_row, 1, " %-29s", drives[i].model_serial);
                    current_row++;
                }

                if (i == selected_drive) {
                    wattroff(win_left, A_REVERSE);
                }
            }

            // Bottom "More" indicator if items exist below view
            if (scroll_offset + visible_slots < drive_count) {
                wattron(win_left, COLOR_PAIR(CP_TITLE));
                mvwprintw(win_left, main_height - 1, left_width - 10, "(More v)");
                wattroff(win_left, COLOR_PAIR(CP_TITLE));
            }
        }
        wrefresh(win_left);

        // --- Render Right Pane (Dynamic Hex & ASCII View) ---
        if (drive_count > 0) {
            render_hex_view(win_right, &drives[selected_drive]);
        } else {
            wclear(win_right);
            wattron(win_right, COLOR_PAIR(CP_BORDER));
            box(win_right, 0, 0);
            wattroff(win_right, COLOR_PAIR(CP_BORDER));
            mvwprintw(win_right, 1, 2, "No drive selected.");
            wrefresh(win_right);
        }

        // --- Render Frameless Help Bar on Bottom Line ---
        wclear(win_help);
        mvwprintw(win_help, 0, 0, "   r=Reload Devices | g=goto | PgUp/PgDn=Scroll Hex View | <=Start | >=End | q=Quit");
        wrefresh(win_help);

        // --- Handle Input (Blocking) ---
        ch = wgetch(win_left);

        if (ch == KEY_RESIZE) {
            // Update terminal geometry
            resize_term(0, 0);

            height = LINES;
            width = COLS;
            main_height = height - title_height - help_height;

            // Recreate windows with new dimensions
            delwin(win_title);
            delwin(win_left);
            delwin(win_right);
            delwin(win_help);

            win_title = newwin(title_height, width, 0, 0);
            win_left  = newwin(main_height, left_width, title_height, 0);
            win_right = newwin(main_height, width - left_width, title_height, left_width);
            win_help  = newwin(help_height, width, height - 1, 0);

            keypad(win_left, TRUE);
            keypad(win_right, TRUE);
            continue;
        }

        int right_rows, right_cols;
        getmaxyx(win_right, right_rows, right_cols);
        int rows_visible = right_rows - 2;
        if (rows_visible < 1) rows_visible = 1;
        int dynamic_bpr = get_dynamic_bytes_per_row(win_right);
        off_t page_bytes = rows_visible * dynamic_bpr;

        switch (ch) {
            case KEY_DOWN:
                if (drive_count > 0 && selected_drive < drive_count - 1) {
                    selected_drive++;
                }
                break;

            case KEY_UP:
                if (drive_count > 0 && selected_drive > 0) {
                    selected_drive--;
                }
                break;

            case KEY_NPAGE: // Page Down
                if (drive_count > 0 && drives[selected_drive].offset + page_bytes < drives[selected_drive].size) {
                    drives[selected_drive].offset += page_bytes;
                }
                break;

            case KEY_PPAGE: // Page Up
                if (drive_count > 0 && drives[selected_drive].offset >= page_bytes) {
                    drives[selected_drive].offset -= page_bytes;
                } else if (drive_count > 0) {
                    drives[selected_drive].offset = 0;
                }
                break;

            case '<': // Start of drive
                if (drive_count > 0) {
                    drives[selected_drive].offset = 0;
                }
                break;

            case '>': // End of drive
                if (drive_count > 0) {
                    if (drives[selected_drive].size > page_bytes) {
                        drives[selected_drive].offset = drives[selected_drive].size - page_bytes;
                    } else {
                        drives[selected_drive].offset = 0;
                    }
                }
                break;

            case 'g':
            case 'G':
                if (drive_count > 0) {
                    drives[selected_drive].offset = handle_goto_popup(&drives[selected_drive], dynamic_bpr);
                }
                break;

            case 'r':
            case 'R': {
                // Manual reload/refresh of drive list and descriptors
                char selected_name[64] = "";
                if (drive_count > 0) {
                    strncpy(selected_name, drives[selected_drive].name, sizeof(selected_name));
                }

                close_drives(drives, drive_count);
                drive_count = get_drive_list(drives, 64);

                int found = 0;
                if (strlen(selected_name) > 0) {
                    for (int i = 0; i < drive_count; i++) {
                        if (strcmp(drives[i].name, selected_name) == 0) {
                            selected_drive = i;
                            found = 1;
                            break;
                        }
                    }
                }
                if (!found && drive_count > 0) {
                    selected_drive = 0;
                    scroll_offset = 0;
                }
                break;
            }

            case 'q':
            case 'Q':
                running = 0;
                break;
        }
    }

    // Cleanup
    close_drives(drives, drive_count);
    delwin(win_title);
    delwin(win_left);
    delwin(win_right);
    delwin(win_help);
    endwin();

    return 0;
}

void init_ncurses_ui(void) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    use_default_colors();

    if (COLORS >= 256) {
        init_pair(CP_BORDER, 33, -1);   // Vibrant Blue for borders
        init_pair(CP_TITLE, 205, -1);   // Vivid Hot Pink for titles
    } else {
        init_pair(CP_BORDER, COLOR_BLUE, -1);
        init_pair(CP_TITLE, COLOR_MAGENTA, -1);
    }
}

void get_model_and_serial(const char *dev_name, char *dest, size_t max_len) {
    char model[64] = "UnknownModel";
    char serial[64] = "UnknownSerial";
    char path[MAX_PATH];
    FILE *f;

    // Retrieve Model from sysfs
    snprintf(path, sizeof(path), "/sys/block/%s/device/model", dev_name);
    f = fopen(path, "r");
    if (f) {
        if (fgets(model, sizeof(model), f)) {
            model[strcspn(model, "\r\n")] = 0;
        }
        fclose(f);
    } else {
        snprintf(path, sizeof(path), "/sys/block/%s/device/../model", dev_name);
        f = fopen(path, "r");
        if (f) {
            if (fgets(model, sizeof(model), f)) {
                model[strcspn(model, "\r\n")] = 0;
            }
            fclose(f);
        }
    }

    // Retrieve Serial from sysfs
    snprintf(path, sizeof(path), "/sys/block/%s/device/serial", dev_name);
    f = fopen(path, "r");
    if (f) {
        if (fgets(serial, sizeof(serial), f)) {
            serial[strcspn(serial, "\r\n")] = 0;
        }
        fclose(f);
    } else {
        snprintf(path, sizeof(path), "/sys/block/%s/serial", dev_name);
        f = fopen(path, "r");
        if (f) {
            if (fgets(serial, sizeof(serial), f)) {
                serial[strcspn(serial, "\r\n")] = 0;
            }
            fclose(f);
        }
    }

    // Trim trailing whitespace
    for (int i = strlen(model) - 1; i >= 0 && model[i] == ' '; i--) model[i] = '\0';
    for (int i = strlen(serial) - 1; i >= 0 && serial[i] == ' '; i--) serial[i] = '\0';

    snprintf(dest, max_len, "%s / %s", model, serial);
}

int get_drive_list(drive_info_t drives[], int max_drives) {
    DIR *dir = opendir("/sys/block");
    if (!dir) return 0;

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_drives) {
        if (entry->d_name[0] == '.') continue;

        // Filter out loops, ram disks, and optical drives
        if (strncmp(entry->d_name, "loop", 4) == 0) continue;
        if (strncmp(entry->d_name, "ram", 3) == 0) continue;
        if (strncmp(entry->d_name, "sr", 2) == 0) continue;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "/dev/%s", entry->d_name);

        int fd = open(path, O_RDONLY);
        if (fd < 0) continue; // Skip if currently unreadable/locked

        strncpy(drives[count].name, entry->d_name, sizeof(drives[count].name));
        strncpy(drives[count].path, path, MAX_PATH);
        drives[count].offset = 0;

        // Fetch Model / Serial string
        get_model_and_serial(entry->d_name, drives[count].model_serial, sizeof(drives[count].model_serial));

        if (ioctl(fd, BLKGETSIZE64, &drives[count].size) < 0) {
            drives[count].size = lseek(fd, 0, SEEK_END);
        }
        drives[count].fd = fd;
        count++;
    }
    closedir(dir);
    return count;
}

int get_dynamic_bytes_per_row(WINDOW *win) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    int inner_width = cols - 4; // account for borders/margins
    if (inner_width < 32) inner_width = 32;

    int bpr = (inner_width - 16) / 4;
    if (bpr < 4) bpr = 4;
    if (bpr > 128) bpr = 128;
    return bpr;
}

off_t handle_goto_popup(drive_info_t *drive, int bytes_per_row) {
    int p_height = 9;
    int p_width = 54;
    int starty = (LINES - p_height) / 2;
    int startx = (COLS - p_width) / 2;

    WINDOW *pop = newwin(p_height, p_width, starty, startx);
    keypad(pop, TRUE);

    box(pop, 0, 0);
    wattron(pop, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(pop, 0, 2, " Go To ");
    wattroff(pop, COLOR_PAIR(CP_TITLE) | A_BOLD);

    mvwprintw(pop, 2, 3, "Select Goto Method:");
    mvwprintw(pop, 4, 5, "[P] Percentage of disk (0-100%%)");
    mvwprintw(pop, 5, 5, "[B] Offset bytes (Decimal / Hex)");
    mvwprintw(pop, 6, 5, "[Esc] Cancel");
    wrefresh(pop);

    int ch = wgetch(pop);
    off_t new_offset = drive->offset;

    if (ch == 'p' || ch == 'P') {
        wclear(pop);
        box(pop, 0, 0);
        wattron(pop, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(pop, 0, 2, " Go To: Percentage ");
        wattroff(pop, COLOR_PAIR(CP_TITLE) | A_BOLD);

        char input_buf[32] = {0};
        mvwprintw(pop, 3, 3, "Enter percentage (0-100): ");
        wrefresh(pop);

        echo();
        curs_set(1);
        wgetnstr(pop, input_buf, 31);
        noecho();
        curs_set(0);

        double percent = atof(input_buf);
        if (percent < 0.0) percent = 0.0;
        if (percent > 100.0) percent = 100.0;
        new_offset = (off_t)(((double)drive->size * percent) / 100.0);
    }
    else if (ch == 'b' || ch == 'B') {
        wclear(pop);
        box(pop, 0, 0);
        wattron(pop, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(pop, 0, 2, " Go To: Byte Offset ");
        wattroff(pop, COLOR_PAIR(CP_TITLE) | A_BOLD);

        char input_buf[64] = {0};
        mvwprintw(pop, 3, 3, "Enter offset (e.g. 0x1000 or 4096): ");
        wrefresh(pop);

        echo();
        curs_set(1);
        wgetnstr(pop, input_buf, 63);
        noecho();
        curs_set(0);

        unsigned long long val = 0;
        if (strncmp(input_buf, "0x", 2) == 0 || strncmp(input_buf, "0X", 2) == 0) {
            sscanf(input_buf, "%llx", &val);
        } else {
            sscanf(input_buf, "%llu", &val);
        }
        new_offset = (off_t)val;
    }

    delwin(pop);
    touchwin(stdscr);

    // Clamp and align to row boundary
    if (new_offset > drive->size) {
        new_offset = drive->size;
    }
    new_offset = (new_offset / bytes_per_row) * bytes_per_row;
    if (new_offset < 0) {
        new_offset = 0;
    }

    return new_offset;
}

void render_hex_view(WINDOW *win, drive_info_t *drive) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    int display_rows = rows - 2;
    if (display_rows < 1) display_rows = 1;

    int bytes_per_row = get_dynamic_bytes_per_row(win);

    unsigned char *buffer = malloc(display_rows * bytes_per_row);
    if (!buffer) return;
    memset(buffer, 0, display_rows * bytes_per_row);

    if (drive->fd >= 0) {
        lseek(drive->fd, drive->offset, SEEK_SET);
        read(drive->fd, buffer, display_rows * bytes_per_row);
    }

    wclear(win);
    wattron(win, COLOR_PAIR(CP_BORDER));
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(CP_BORDER));

    // Render pane title
    wattron(win, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(win, 0, 2, " Hex View: %s, Offset: 0x%012lX ", drive->name, drive->offset);
    wattroff(win, COLOR_PAIR(CP_TITLE) | A_BOLD);

    char *hex_buf = malloc(bytes_per_row * 3 + 1);
    char *ascii_buf = malloc(bytes_per_row + 1);
    if (!hex_buf || !ascii_buf) {
        free(buffer);
        free(hex_buf);
        free(ascii_buf);
        return;
    }

    for (int i = 0; i < display_rows; i++) {
        off_t row_addr = drive->offset + (i * bytes_per_row);
        if (row_addr >= drive->size) break;

        hex_buf[0] = '\0';
        ascii_buf[0] = '\0';

        for (int j = 0; j < bytes_per_row; j++) {
            int idx = (i * bytes_per_row) + j;
            unsigned char c = buffer[idx];
            char byte_str[4];
            sprintf(byte_str, "%02X ", c);
            strcat(hex_buf, byte_str);

            if (c >= 32 && c <= 126) {
                sprintf(ascii_buf + strlen(ascii_buf), "%c", c);
            } else {
                strcat(ascii_buf, ".");
            }
        }

        mvwprintw(win, i + 1, 2, "%012lX:  %-*s   %s",
                  row_addr,
                  bytes_per_row * 3,
                  hex_buf,
                  ascii_buf);
    }

    free(hex_buf);
    free(ascii_buf);
    free(buffer);
    wrefresh(win);
}

void close_drives(drive_info_t drives[], int count) {
    for (int i = 0; i < count; i++) {
        if (drives[i].fd >= 0) {
            close(drives[i].fd);
        }
    }
}
