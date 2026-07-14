#define _POSIX_C_SOURCE 200809L
#include <vfd/theme.h>
#include <vfd/ui.h>
#include <vfd/widgets.h>
#include <X11/keysym.h>
#include <dirent.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define MAX_ITEMS 4096
#define MAX_VISIBLE 256
#define MAX_LABEL 160
#define MAX_EXEC 1024

typedef enum {
    VIEW_ROOT,
    VIEW_APPS,
    VIEW_FILES,
    VIEW_THEMES,
    VIEW_WORKSPACES,
    VIEW_POWER,
    VIEW_DESKTOP
} View;

typedef enum { ITEM_ACTION, ITEM_APP, ITEM_SUBMENU } ItemType;

typedef struct {
    char label[MAX_LABEL];
    char detail[MAX_LABEL];
    char exec[MAX_EXEC];
    ItemType type;
    View target;
    int score;
} Item;

typedef struct {
    VfdUi *ui;
    VfdWindow *win;
    VfdTheme theme;
    VfdColor bg, fg, dim, accent, selection;
    int width, height;
    bool dirty;
    View view;
    Item items[MAX_ITEMS];
    size_t item_count;
    Item *visible[MAX_VISIBLE];
    const char *labels[MAX_VISIBLE];
    size_t visible_count;
    VfdListView list;
    VfdTextInput input;
} App;

static volatile sig_atomic_t running = 1;
static void stop_signal(int sig) { (void)sig; running = 0; }

static const char *theme_path(void) {
    static char path[PATH_MAX];
    const char *home = getenv("HOME");
    snprintf(path, sizeof path, "%s/.config/vfd/themes/lain/theme.ini", home ? home : "");
    return path;
}

static const char *view_name(View view) {
    switch (view) {
        case VIEW_APPS: return "APPS";
        case VIEW_FILES: return "FILES";
        case VIEW_THEMES: return "THEMES";
        case VIEW_WORKSPACES: return "WORKSPACES";
        case VIEW_POWER: return "POWER";
        case VIEW_DESKTOP: return "DESKTOP";
        default: return "COMMAND";
    }
}

static void trim(char *s) {
    size_t n;
    while (*s == ' ' || *s == '\t') memmove(s, s + 1, strlen(s));
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = 0;
}

static void strip_exec_codes(char *out, size_t out_size, const char *in) {
    size_t j = 0;
    bool space = false;
    for (size_t i = 0; in[i] && j + 1 < out_size; i++) {
        if (in[i] == '%' && in[i + 1]) { i++; continue; }
        if (in[i] == ' ' || in[i] == '\t') {
            if (!space && j) out[j++] = ' ';
            space = true;
        } else {
            out[j++] = in[i];
            space = false;
        }
    }
    while (j && out[j - 1] == ' ') j--;
    out[j] = 0;
}

static void add_item(App *a, ItemType type, View target, const char *label,
                     const char *detail, const char *exec) {
    if (a->item_count >= MAX_ITEMS || !label || !*label) return;
    Item *item = &a->items[a->item_count++];
    snprintf(item->label, sizeof item->label, "%s", label);
    snprintf(item->detail, sizeof item->detail, "%s", detail ? detail : "");
    snprintf(item->exec, sizeof item->exec, "%s", exec ? exec : "");
    item->type = type;
    item->target = target;
}

static void add_action(App *a, const char *label, const char *detail, const char *exec) {
    add_item(a, ITEM_ACTION, VIEW_ROOT, label, detail, exec);
}

static void add_submenu(App *a, View target, const char *label, const char *detail) {
    add_item(a, ITEM_SUBMENU, target, label, detail, NULL);
}

static void load_root(App *a) {
    add_submenu(a, VIEW_APPS, "Applications", "Installed desktop applications");
    add_submenu(a, VIEW_FILES, "Files", "Locations and file manager actions");
    add_submenu(a, VIEW_THEMES, "Themes", "Switch the VFD visual theme");
    add_submenu(a, VIEW_WORKSPACES, "Workspaces", "Jump to an i3 workspace");
    add_submenu(a, VIEW_DESKTOP, "Desktop", "VFD and window manager controls");
    add_submenu(a, VIEW_POWER, "Power", "Lock, log out, reboot or shut down");
    add_action(a, "Terminal", "Open Alacritty", "alacritty");
}

static void parse_desktop_file(App *a, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[2048], name[MAX_LABEL] = "", generic[MAX_LABEL] = "", exec_raw[MAX_EXEC] = "";
    bool desktop = false, hidden = false, nodisplay = false, terminal = false;
    while (fgets(line, sizeof line, f)) {
        trim(line);
        if (line[0] == '[') { desktop = !strcmp(line, "[Desktop Entry]"); continue; }
        if (!desktop || line[0] == '#' || !strchr(line, '=')) continue;
        char *eq = strchr(line, '='); *eq++ = 0; trim(line); trim(eq);
        if (!strcmp(line, "Name") && !name[0]) snprintf(name, sizeof name, "%s", eq);
        else if (!strcmp(line, "GenericName") && !generic[0]) snprintf(generic, sizeof generic, "%s", eq);
        else if (!strcmp(line, "Exec")) snprintf(exec_raw, sizeof exec_raw, "%s", eq);
        else if (!strcmp(line, "Hidden")) hidden = !strcasecmp(eq, "true");
        else if (!strcmp(line, "NoDisplay")) nodisplay = !strcasecmp(eq, "true");
        else if (!strcmp(line, "Terminal")) terminal = !strcasecmp(eq, "true");
        else if (!strcmp(line, "Type") && strcmp(eq, "Application")) { fclose(f); return; }
    }
    fclose(f);
    if (!name[0] || !exec_raw[0] || hidden || nodisplay) return;
    char clean[MAX_EXEC], command[MAX_EXEC];
    strip_exec_codes(clean, sizeof clean, exec_raw);
    if (!clean[0]) return;
    if (terminal) snprintf(command, sizeof command, "alacritty -e sh -lc \"%.*s\"", (int)sizeof(command) - 24, clean);
    else snprintf(command, sizeof command, "%s", clean);
    add_item(a, ITEM_APP, VIEW_ROOT, name, generic[0] ? generic : "Application", command);
}

static void scan_apps_dir(App *a, const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d))) {
        size_t n = strlen(de->d_name);
        if (de->d_name[0] == '.' || n < 8 || strcmp(de->d_name + n - 8, ".desktop")) continue;
        char path[PATH_MAX];
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        parse_desktop_file(a, path);
    }
    closedir(d);
}

static void load_apps(App *a) {
    const char *home = getenv("HOME");
    if (home) {
        char dir[PATH_MAX];
        snprintf(dir, sizeof dir, "%s/.local/share/applications", home);
        scan_apps_dir(a, dir);
    }
    scan_apps_dir(a, "/usr/local/share/applications");
    scan_apps_dir(a, "/usr/share/applications");
}

static void load_files(App *a) {
    const char *home = getenv("HOME");
    char command[MAX_EXEC];
    if (!home) home = "~";
    add_action(a, "Home", home, "vfdfm ~");
    add_action(a, "Projects", "~/Projects", "vfdfm ~/Projects");
    add_action(a, "Downloads", "~/Downloads", "vfdfm ~/Downloads");
    add_action(a, "Documents", "~/Documents", "vfdfm ~/Documents");
    add_action(a, "Pictures", "~/Pictures", "vfdfm ~/Pictures");
    add_action(a, "Music", "VFD Music", "/home/adam/.local/bin/vfdmusic");
    add_action(a, "Root", "/", "vfdfm /");
    snprintf(command, sizeof command, "vfdfm '%s/.config'", home);
    add_action(a, "Configuration", "~/.config", command);
}

static void load_themes(App *a) {
    const char *home = getenv("HOME");
    char root[PATH_MAX];
    snprintf(root, sizeof root, "%s/.config/vfd/themes", home ? home : "");
    DIR *d = opendir(root);
    if (!d) {
        add_action(a, "Lain", "Current built-in theme", "vfdctl theme lain; pkill -HUP vfdbar 2>/dev/null || true");
        return;
    }
    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char config[PATH_MAX];
        int written = snprintf(config, sizeof config, "%s/%s/theme.ini", root, de->d_name);
        if (written < 0 || (size_t)written >= sizeof config || access(config, R_OK) != 0) continue;
        char command[MAX_EXEC];
        snprintf(command, sizeof command,
                 "vfdctl theme '%s'; pkill -HUP vfdbar 2>/dev/null || true", de->d_name);
        add_action(a, de->d_name, config, command);
    }
    closedir(d);
}

static void load_workspaces(App *a) {
    for (int i = 1; i <= 10; i++) {
        char label[32], detail[64], command[64];
        snprintf(label, sizeof label, "Workspace %d", i);
        snprintf(detail, sizeof detail, "Switch to workspace number %d", i);
        snprintf(command, sizeof command, "i3-msg 'workspace number %d'", i);
        add_action(a, label, detail, command);
    }
}

static void load_desktop(App *a) {
    add_action(a, "Reload VFD", "Reload daemon configuration", "vfdctl reload");
    add_action(a, "Restart Bar", "Restart the graphical status bar", "pkill -x vfdbar; sleep 0.2; vfdbar");
    add_action(a, "Reload i3", "Reload i3 configuration", "i3-msg reload");
    add_action(a, "Restart i3", "Restart i3 in place", "i3-msg restart");
    add_action(a, "Open VFD Config", "Edit the VFD project", "alacritty -e nvim ~/Projects/vfd");
    add_action(a, "Daemon Status", "Show vfdd status in a terminal", "alacritty -e sh -lc 'vfdctl status; echo; read -r -p \"Press Enter...\"'");
}

static void load_power(App *a) {
    add_action(a, "Lock", "Lock the current session", "i3lock");
    add_action(a, "Log Out", "Exit i3 after confirmation",
               "i3-nagbar -t warning -m 'Log out of i3?' -B 'Log out' 'i3-msg exit'");
    add_action(a, "Reboot", "Restart the machine after confirmation",
               "i3-nagbar -t warning -m 'Reboot the machine?' -B 'Reboot' 'systemctl reboot'");
    add_action(a, "Shut Down", "Power off after confirmation",
               "i3-nagbar -t warning -m 'Shut down the machine?' -B 'Shut down' 'systemctl poweroff'");
}

static void load_items(App *a) {
    a->item_count = 0;
    switch (a->view) {
        case VIEW_APPS: load_apps(a); break;
        case VIEW_FILES: load_files(a); break;
        case VIEW_THEMES: load_themes(a); break;
        case VIEW_WORKSPACES: load_workspaces(a); break;
        case VIEW_POWER: load_power(a); break;
        case VIEW_DESKTOP: load_desktop(a); break;
        default: load_root(a); break;
    }
}

static int fuzzy_score(const char *label, const char *query) {
    if (!query[0]) return 1;
    int score = 0, run = 0, pos = 0;
    const unsigned char *l = (const unsigned char *)label;
    const unsigned char *q = (const unsigned char *)query;
    while (*q) {
        unsigned char qc = *q >= 'A' && *q <= 'Z' ? *q + 32 : *q;
        bool found = false;
        while (*l) {
            unsigned char lc = *l >= 'A' && *l <= 'Z' ? *l + 32 : *l;
            if (lc == qc) {
                run++;
                score += 20 + run * 8 - pos;
                l++; q++; pos++;
                found = true;
                break;
            }
            run = 0; l++; pos++;
        }
        if (!found) return -1;
    }
    if (!strncasecmp(label, query, strlen(query))) score += 200;
    return score;
}

static int compare_visible(const void *pa, const void *pb) {
    const Item *a = *(Item * const *)pa;
    const Item *b = *(Item * const *)pb;
    if (a->score != b->score) return b->score - a->score;
    if (a->type != b->type) return b->type - a->type;
    return strcasecmp(a->label, b->label);
}

static void filter_items(App *a) {
    a->visible_count = 0;
    for (size_t i = 0; i < a->item_count && a->visible_count < MAX_VISIBLE; i++) {
        int score = fuzzy_score(a->items[i].label, a->input.text);
        if (score < 0 && a->items[i].detail[0]) {
            int detail_score = fuzzy_score(a->items[i].detail, a->input.text);
            score = detail_score < 0 ? -1 : detail_score / 2;
        }
        if (score < 0) continue;
        a->items[i].score = score;
        a->visible[a->visible_count++] = &a->items[i];
    }
    qsort(a->visible, a->visible_count, sizeof a->visible[0], compare_visible);
    for (size_t i = 0; i < a->visible_count; i++) a->labels[i] = a->visible[i]->label;
    vfd_list_set_items(&a->list, a->labels, a->visible_count);
    vfd_list_first(&a->list);
    a->dirty = true;
}

static void set_view(App *a, View view) {
    a->view = view;
    vfd_text_input_clear(&a->input);
    load_items(a);
    filter_items(a);
}

static void go_back(App *a) {
    if (a->view == VIEW_ROOT) running = 0;
    else set_view(a, VIEW_ROOT);
}

static void launch(const char *command) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-lc", command, (char *)NULL);
        _exit(127);
    }
}

static void activate(App *a) {
    if (!a->visible_count) return;
    size_t selected = vfd_list_selected(&a->list);
    if (selected >= a->visible_count) return;
    Item *item = a->visible[selected];
    if (item->type == ITEM_SUBMENU) {
        set_view(a, item->target);
        return;
    }
    if (item->exec[0]) launch(item->exec);
    running = 0;
}

static int load_colors(App *a) {
    return vfd_color_parse(a->win, a->theme.background, &a->bg) ||
           vfd_color_parse(a->win, a->theme.foreground, &a->fg) ||
           vfd_color_parse(a->win, a->theme.dim, &a->dim) ||
           vfd_color_parse(a->win, a->theme.accent, &a->accent) ||
           vfd_color_parse(a->win, a->theme.selection, &a->selection);
}

static void render(App *a) {
    vfd_begin_frame(a->win, a->bg);
    vfd_draw_rect(a->win, 0, 0, a->width, 1, a->accent);
    vfd_label_draw(a->win, 16, 25, view_name(a->view), a->accent);
    vfd_text_input_draw(&a->input, a->win, 118, 25, "> ", a->fg, a->accent);
    vfd_draw_rect(a->win, 12, 36, a->width - 24, 1, a->selection);

    VfdRect list_bounds = {12, 48, a->width - 24, a->height - 84};
    vfd_list_draw(&a->list, a->win, list_bounds, 17, a->fg, a->accent, a->selection);
    size_t rows = (size_t)(list_bounds.height / a->list.row_height);
    for (size_t row = 0; row < rows && a->list.scroll + row < a->visible_count; row++) {
        size_t index = a->list.scroll + row;
        Item *item = a->visible[index];
        int baseline = list_bounds.y + (int)row * a->list.row_height + 17;
        const char *kind = item->type == ITEM_SUBMENU ? ">" : item->type == ITEM_APP ? "APP" : "RUN";
        vfd_draw_text(a->win, a->width - 58, baseline, kind, a->dim);
    }

    vfd_draw_rect(a->win, 0, a->height - 24, a->width, 24, a->selection);
    char left[96];
    snprintf(left, sizeof left, "%zu RESULTS", a->visible_count);
    const char *help = a->view == VIEW_ROOT ? "ENTER OPENS  ·  ESC CLOSES" : "ENTER RUNS  ·  ESC BACK";
    VfdStatusBar status = {left, help, "LAUNCHER 0.2", {0, a->height - 24, a->width, 24}};
    vfd_status_draw(&status, a->win, a->height - 7, a->selection, a->dim, a->accent);
    vfd_end_frame(a->win);
    a->dirty = false;
}

static void key(App *a, const VfdEvent *event) {
    unsigned long key = event->keysym;
    if (key == XK_Escape) { go_back(a); return; }
    if (key == XK_Return || key == XK_KP_Enter) { activate(a); return; }
    if (key == XK_Down || (key == XK_j && !a->input.text[0])) { vfd_list_move(&a->list, 1); a->dirty = true; return; }
    if (key == XK_Up || (key == XK_k && !a->input.text[0])) { vfd_list_move(&a->list, -1); a->dirty = true; return; }
    if (key == XK_Page_Down) { vfd_list_move(&a->list, 8); a->dirty = true; return; }
    if (key == XK_Page_Up) { vfd_list_move(&a->list, -8); a->dirty = true; return; }
    if (key == XK_BackSpace) {
        if (!a->input.text[0]) go_back(a);
        else { vfd_text_input_backspace(&a->input); filter_items(a); }
        return;
    }
    if (event->text[0]) { vfd_text_input_append(&a->input, event->text); filter_items(a); }
}

int main(void) {
    signal(SIGINT, stop_signal);
    signal(SIGTERM, stop_signal);
    App app = {.width = 650, .height = 440, .dirty = true, .view = VIEW_ROOT};
    vfd_theme_load(theme_path(), &app.theme);
    app.ui = vfd_ui_open();
    if (!app.ui) { fprintf(stderr, "Launcher: cannot open display\n"); return 1; }

    int mx = 0, my = 0, mw = 0, mh = 0;
    vfd_ui_monitor_geometry(app.ui, 0, &mx, &my, &mw, &mh);
    VfdWindowConfig config = {
        mx + (mw - app.width) / 2, my + (mh - app.height) / 3,
        app.width, app.height, "Launcher", "vfdlauncher", app.theme.font, false, 0
    };
    app.win = vfd_window_create(app.ui, &config);
    if (!app.win || load_colors(&app)) { fprintf(stderr, "Launcher: window initialization failed\n"); return 1; }

    vfd_list_init(&app.list, 27);
    app.input.active = true;
    load_items(&app);
    filter_items(&app);

    while (running) {
        struct pollfd fd = {vfd_ui_fd(app.ui), POLLIN, 0};
        poll(&fd, 1, 100);
        VfdEvent event;
        while (vfd_poll_event(app.ui, &event)) {
            if (event.window != app.win) continue;
            if (event.type == VFD_EVENT_EXPOSE) app.dirty = true;
            else if (event.type == VFD_EVENT_RESIZE) { app.width = event.width; app.height = event.height; app.dirty = true; }
            else if (event.type == VFD_EVENT_KEY) key(&app, &event);
            else if (event.type == VFD_EVENT_BUTTON && event.button == 1) {
                VfdRect bounds = {12, 48, app.width - 24, app.height - 84};
                int index = vfd_list_index_at(&app.list, bounds, event.x, event.y);
                if (index >= 0) { vfd_list_select(&app.list, (size_t)index); activate(&app); }
            } else if (event.type == VFD_EVENT_CLOSE) running = 0;
        }
        if (app.dirty) render(&app);
    }
    vfd_window_destroy(app.win);
    vfd_ui_close(app.ui);
    return 0;
}
