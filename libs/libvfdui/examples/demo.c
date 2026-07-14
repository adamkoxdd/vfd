#define _POSIX_C_SOURCE 200809L
#include "vfdui/vfdui.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void frame_sleep(void) {
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = 16000000
    };
    nanosleep(&delay, NULL);
}

int main(void) {
    VfdTheme theme = vfd_theme_default();
    VfdWindow *window = vfd_window_create(
        "VFD UI Stage 2",
        1180,
        760,
        &theme
    );

    if (!window) {
        fprintf(stderr, "failed to create VFD window\n");
        return 1;
    }

    bool telemetry = true;
    bool glow = true;
    double volume = 0.72;

    char search_buffer[128] = "";
    VfdTextboxState search = {
        .buffer = search_buffer,
        .capacity = sizeof search_buffer,
        .length = 0,
        .cursor = 0,
        .focused = false,
        .password = false
    };

    const char *tracks[] = {
        "Duvet // bôa",
        "Digital Bath // Deftones",
        "Enjoy the Silence // Depeche Mode",
        "Nightcall // Kavinsky",
        "Oblivion // Grimes",
        "Resonance // HOME",
        "Teardrop // Massive Attack",
        "The Perfect Girl // Mareux",
        "Windowlicker // Aphex Twin",
        "Xtal // Aphex Twin",
        "Young // Vacations",
        "Zero // Smashing Pumpkins"
    };

    VfdListState track_list = {
        .selected = 0,
        .hovered = -1,
        .scroll = 0
    };

    const char *modes[] = {
        "LIBRARY",
        "VISUALIZER",
        "QUEUE"
    };

    VfdDropdownState mode = {
        .open = false,
        .selected = 0,
        .hovered = -1
    };

    VfdTreeRow tree[] = {
        {"VFD", 0, true, true},
        {"apps", 1, true, true},
        {"music", 2, false, false},
        {"lock", 2, false, false},
        {"shell", 2, false, false},
        {"libs", 1, true, true},
        {"libvfdui", 2, false, false},
        {"deploy", 1, false, false}
    };
    int tree_selected = 0;

    while (vfd_window_is_open(window)) {
        VfdEvent event = vfd_window_poll(window);
        if (event.quit) vfd_window_close(window);

        vfd_window_begin(window);

        int width = vfd_window_width(window);
        int height = vfd_window_height(window);

        vfd_panel(
            window,
            (VfdRect){20, 20, width - 40, height - 40},
            true
        );

        vfd_title(
            window,
            "VFD UI ENGINE // STAGE 2",
            (VfdRect){46, 44, width - 92, 36}
        );

        vfd_label(
            window,
            "INTERACTIVE CONTROLS // APPLICATION TOOLKIT",
            (VfdRect){46, 78, width - 92, 24},
            12, false, VFD_ALIGN_LEFT, theme.dim
        );

        vfd_separator(window, 46, 112, width - 46, 112);

        vfd_textbox(
            window,
            (VfdRect){46, 136, 350, 62},
            "SEARCH",
            &search,
            &event
        );

        vfd_dropdown(
            window,
            (VfdRect){420, 136, 260, 48},
            "VIEW",
            modes,
            3,
            &mode,
            &event
        );

        vfd_checkbox(
            window,
            (VfdRect){710, 140, 190, 30},
            "TELEMETRY",
            &telemetry,
            &event
        );

        vfd_switch(
            window,
            (VfdRect){920, 140, 190, 30},
            "GLOW",
            &glow,
            &event
        );

        vfd_slider(
            window,
            (VfdRect){46, 246, 634, 34},
            "VOLUME",
            &volume,
            0.0,
            1.0,
            &event
        );

        vfd_label(
            window,
            "TRACK LIST",
            (VfdRect){46, 306, 330, 24},
            13, true, VFD_ALIGN_LEFT, theme.phosphor
        );

        vfd_list(
            window,
            (VfdRect){46, 336, 634, height - 382},
            tracks,
            (int)(sizeof tracks / sizeof tracks[0]),
            42,
            &track_list,
            &event
        );

        vfd_label(
            window,
            "PROJECT TREE",
            (VfdRect){714, 306, 300, 24},
            13, true, VFD_ALIGN_LEFT, theme.phosphor
        );

        vfd_tree(
            window,
            (VfdRect){714, 336, width - 760, 260},
            tree,
            (int)(sizeof tree / sizeof tree[0]),
            34,
            &tree_selected,
            &event
        );

        char status[256];
        snprintf(
            status,
            sizeof status,
            "SELECTED // %s",
            tracks[track_list.selected]
        );

        vfd_panel(
            window,
            (VfdRect){714, 616, width - 760, 74},
            true
        );

        vfd_label(
            window,
            status,
            (VfdRect){732, 638, width - 796, 28},
            12, true, VFD_ALIGN_LEFT, theme.glow
        );

        vfd_window_end(window);
        frame_sleep();
    }

    vfd_window_destroy(window);
    return 0;
}
