#define _DEFAULT_SOURCE

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define MAX_TRACKED_WINDOWS 64
#define OPACITY_FULL 0xffffffffUL
#define OPACITY_MIN 0x19191919UL
#define OPACITY_STEP_PERCENT 10

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD 1

typedef struct {
    Window window;
    unsigned long opacity;
    bool click_through;
    bool pinned;
} WindowState;

typedef struct {
    KeyCode transparency_up;
    KeyCode transparency_down;
    KeyCode unlock_click_through;
    KeyCode pin_window;
    KeyCode center_window;
} Hotkeys;

static Display *g_display = NULL;
static Window g_root = None;
static int g_screen = 0;
static volatile sig_atomic_t g_running = 1;
static unsigned int g_numlock_mask = 0;
static WindowState g_states[MAX_TRACKED_WINDOWS];
static int g_state_count = 0;
static Hotkeys g_hotkeys = { 0 };

static Atom g_atom_active_window;
static Atom g_atom_opacity;
static Atom g_atom_wm_state;
static Atom g_atom_wm_state_above;
static Atom g_atom_workarea;
static Atom g_atom_current_desktop;
static Atom g_atom_net_wm_name;
static Atom g_atom_utf8_string;

static int x_error_handler(Display *display, XErrorEvent *event)
{
    char message[256];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    fprintf(stderr, "X11 warning: %s (request %d, resource 0x%lx)\n",
        message, event->request_code, event->resourceid);
    return 0;
}

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static void init_atoms(void)
{
    g_atom_active_window = XInternAtom(g_display, "_NET_ACTIVE_WINDOW", False);
    g_atom_opacity = XInternAtom(g_display, "_NET_WM_WINDOW_OPACITY", False);
    g_atom_wm_state = XInternAtom(g_display, "_NET_WM_STATE", False);
    g_atom_wm_state_above = XInternAtom(g_display, "_NET_WM_STATE_ABOVE", False);
    g_atom_workarea = XInternAtom(g_display, "_NET_WORKAREA", False);
    g_atom_current_desktop = XInternAtom(g_display, "_NET_CURRENT_DESKTOP", False);
    g_atom_net_wm_name = XInternAtom(g_display, "_NET_WM_NAME", False);
    g_atom_utf8_string = XInternAtom(g_display, "UTF8_STRING", False);
}

static const char *window_title(Window window)
{
    static char title[256];
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *data = NULL;

    title[0] = '\0';
    if (XGetWindowProperty(g_display, window, g_atom_net_wm_name, 0, 255, False,
            g_atom_utf8_string, &actual_type, &actual_format, &nitems,
            &bytes_after, &data) == Success && data) {
        snprintf(title, sizeof(title), "%s", (char *)data);
        XFree(data);
        return title;
    }

    if (data) {
        XFree(data);
    }

    XTextProperty text;
    if (XGetWMName(g_display, window, &text) && text.value) {
        snprintf(title, sizeof(title), "%s", (char *)text.value);
        XFree(text.value);
        return title;
    }

    snprintf(title, sizeof(title), "0x%lx", window);
    return title;
}

static bool is_valid_window(Window window)
{
    if (window == None || window == g_root) {
        return false;
    }

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(g_display, window, &attrs)) {
        return false;
    }

    return attrs.class == InputOutput && attrs.map_state != IsUnmapped;
}

static WindowState *state_for(Window window, bool create)
{
    for (int i = 0; i < g_state_count; i++) {
        if (g_states[i].window == window) {
            return &g_states[i];
        }
    }

    if (!create || g_state_count >= MAX_TRACKED_WINDOWS) {
        return NULL;
    }

    WindowState *state = &g_states[g_state_count++];
    memset(state, 0, sizeof(*state));
    state->window = window;
    state->opacity = OPACITY_FULL;
    return state;
}

static void drop_state_if_unused(WindowState *state)
{
    if (!state || state->click_through || state->pinned || state->opacity < OPACITY_FULL) {
        return;
    }

    int index = (int)(state - g_states);
    if (index < 0 || index >= g_state_count) {
        return;
    }

    for (int i = index + 1; i < g_state_count; i++) {
        g_states[i - 1] = g_states[i];
    }
    g_state_count--;
}

static Window active_window(void)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *data = NULL;
    Window window = None;

    if (XGetWindowProperty(g_display, g_root, g_atom_active_window, 0, 1, False,
            XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after,
            &data) == Success && data && nitems == 1) {
        window = *(Window *)data;
        XFree(data);
    }

    if (!is_valid_window(window)) {
        int revert = 0;
        XGetInputFocus(g_display, &window, &revert);
    }

    return is_valid_window(window) ? window : None;
}

static Window last_click_through_window(void)
{
    for (int i = g_state_count - 1; i >= 0; i--) {
        if (g_states[i].click_through && is_valid_window(g_states[i].window)) {
            return g_states[i].window;
        }
    }
    return None;
}

static Window pin_target_window(void)
{
    Window window = last_click_through_window();
    if (window != None) {
        return window;
    }
    return active_window();
}

static void set_window_opacity(Window window, unsigned long opacity)
{
    WindowState *state = state_for(window, true);
    if (!state) {
        return;
    }

    state->opacity = opacity;
    if (opacity >= OPACITY_FULL) {
        XDeleteProperty(g_display, window, g_atom_opacity);
        state->opacity = OPACITY_FULL;
    } else {
        XChangeProperty(g_display, window, g_atom_opacity, XA_CARDINAL, 32,
            PropModeReplace, (unsigned char *)&opacity, 1);
    }
    XFlush(g_display);
    drop_state_if_unused(state);
}

static void set_click_through(Window window, bool enable)
{
    WindowState *state = state_for(window, true);
    if (!state) {
        return;
    }

    if (enable) {
        XserverRegion empty = XFixesCreateRegion(g_display, NULL, 0);
        XFixesSetWindowShapeRegion(g_display, window, ShapeInput, 0, 0, empty);
        XFixesDestroyRegion(g_display, empty);
        state->click_through = true;
    } else {
        XFixesSetWindowShapeRegion(g_display, window, ShapeInput, 0, 0, None);
        state->click_through = false;
    }

    XFlush(g_display);
    drop_state_if_unused(state);
}

static void send_wm_state(Window window, long action, Atom state_atom)
{
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.display = g_display;
    event.xclient.window = window;
    event.xclient.message_type = g_atom_wm_state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = action;
    event.xclient.data.l[1] = state_atom;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1;
    event.xclient.data.l[4] = 0;

    XSendEvent(g_display, g_root, False,
        SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(g_display);
}

static void set_pinned(Window window, bool enable)
{
    WindowState *state = state_for(window, true);
    if (!state) {
        return;
    }

    send_wm_state(window, enable ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE,
        g_atom_wm_state_above);
    if (enable) {
        XRaiseWindow(g_display, window);
    }

    state->pinned = enable;
    XFlush(g_display);
    drop_state_if_unused(state);
}

static void adjust_transparency(bool more_transparent)
{
    Window window = active_window();
    if (!is_valid_window(window)) {
        fprintf(stderr, "No active X11 window found.\n");
        return;
    }

    WindowState *state = state_for(window, true);
    if (!state) {
        return;
    }

    unsigned long step = OPACITY_FULL / (100 / OPACITY_STEP_PERCENT);
    unsigned long opacity = state->opacity;

    if (more_transparent) {
        opacity = (opacity > OPACITY_MIN + step) ? opacity - step : OPACITY_MIN;
    } else {
        opacity = (opacity + step < OPACITY_FULL) ? opacity + step : OPACITY_FULL;
    }

    set_window_opacity(window, opacity);
    set_click_through(window, opacity < OPACITY_FULL);
    printf("%s: opacity %.0f%%, click-through %s\n",
        window_title(window), (double)opacity * 100.0 / (double)OPACITY_FULL,
        opacity < OPACITY_FULL ? "on" : "off");
}

static void unlock_click_through(void)
{
    Window window = last_click_through_window();
    if (!is_valid_window(window)) {
        fprintf(stderr, "No click-through window to unlock.\n");
        return;
    }

    set_click_through(window, false);
    printf("%s: click-through off\n", window_title(window));
}

static void toggle_pin(void)
{
    Window window = pin_target_window();
    if (!is_valid_window(window)) {
        fprintf(stderr, "No window to pin.\n");
        return;
    }

    WindowState *state = state_for(window, true);
    bool enable = state && !state->pinned;
    set_pinned(window, enable);
    printf("%s: always-on-top %s\n", window_title(window), enable ? "on" : "off");
}

static bool read_workarea(long *x, long *y, long *width, long *height)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *data = NULL;
    unsigned long current_desktop = 0;

    if (XGetWindowProperty(g_display, g_root, g_atom_current_desktop, 0, 1, False,
            XA_CARDINAL, &actual_type, &actual_format, &nitems, &bytes_after,
            &data) == Success && data && nitems == 1) {
        current_desktop = *(unsigned long *)data;
    }
    if (data) {
        XFree(data);
        data = NULL;
    }

    if (XGetWindowProperty(g_display, g_root, g_atom_workarea, 0, 4 * 32, False,
            XA_CARDINAL, &actual_type, &actual_format, &nitems, &bytes_after,
            &data) != Success || !data || nitems < (current_desktop + 1) * 4) {
        if (data) {
            XFree(data);
        }
        *x = 0;
        *y = 0;
        *width = DisplayWidth(g_display, g_screen);
        *height = DisplayHeight(g_display, g_screen);
        return false;
    }

    unsigned long *workareas = (unsigned long *)data;
    unsigned long offset = current_desktop * 4;
    *x = (long)workareas[offset];
    *y = (long)workareas[offset + 1];
    *width = (long)workareas[offset + 2];
    *height = (long)workareas[offset + 3];
    XFree(data);
    return true;
}

static void center_window(void)
{
    Window window = active_window();
    if (!is_valid_window(window)) {
        fprintf(stderr, "No active X11 window found.\n");
        return;
    }

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(g_display, window, &attrs)) {
        return;
    }

    long work_x;
    long work_y;
    long work_w;
    long work_h;
    read_workarea(&work_x, &work_y, &work_w, &work_h);

    int x = (int)(work_x + (work_w - attrs.width) / 2);
    int y = (int)(work_y + (work_h - attrs.height) / 2);
    XMoveWindow(g_display, window, x, y);
    XFlush(g_display);
    printf("%s: centered\n", window_title(window));
}

static unsigned int numlock_mask(void)
{
    unsigned int mask = 0;
    XModifierKeymap *modmap = XGetModifierMapping(g_display);
    if (!modmap) {
        return 0;
    }

    KeyCode numlock = XKeysymToKeycode(g_display, XK_Num_Lock);
    if (numlock == 0) {
        XFreeModifiermap(modmap);
        return 0;
    }

    for (int mod = 0; mod < 8; mod++) {
        for (int key = 0; key < modmap->max_keypermod; key++) {
            KeyCode code = modmap->modifiermap[mod * modmap->max_keypermod + key];
            if (code == numlock) {
                mask = 1u << mod;
                break;
            }
        }
    }

    XFreeModifiermap(modmap);
    return mask;
}

static void grab_key(KeyCode keycode, unsigned int modifiers)
{
    unsigned int variants[] = {
        0,
        LockMask,
        g_numlock_mask,
        g_numlock_mask | LockMask
    };

    for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); i++) {
        XGrabKey(g_display, keycode, modifiers | variants[i], g_root, True,
            GrabModeAsync, GrabModeAsync);
    }
}

static void grab_hotkeys(void)
{
    g_hotkeys.transparency_up = XKeysymToKeycode(g_display, XK_Left);
    g_hotkeys.transparency_down = XKeysymToKeycode(g_display, XK_Right);
    g_hotkeys.unlock_click_through = XKeysymToKeycode(g_display, XK_Up);
    g_hotkeys.pin_window = XKeysymToKeycode(g_display, XK_Down);
    g_hotkeys.center_window = XKeysymToKeycode(g_display, XK_KP_5);

    grab_key(g_hotkeys.transparency_up, Mod1Mask);
    grab_key(g_hotkeys.transparency_down, Mod1Mask);
    grab_key(g_hotkeys.unlock_click_through, Mod1Mask);
    grab_key(g_hotkeys.pin_window, Mod1Mask);
    grab_key(g_hotkeys.center_window, ControlMask);
    XFlush(g_display);
}

static void cleanup(void)
{
    for (int i = g_state_count - 1; i >= 0; i--) {
        if (!is_valid_window(g_states[i].window)) {
            continue;
        }
        if (g_states[i].click_through) {
            XFixesSetWindowShapeRegion(g_display, g_states[i].window, ShapeInput, 0, 0, None);
        }
        if (g_states[i].pinned) {
            send_wm_state(g_states[i].window, NET_WM_STATE_REMOVE, g_atom_wm_state_above);
        }
    }
    XFlush(g_display);
}

static void handle_keypress(XKeyEvent *event)
{
    unsigned int state = event->state & ~(LockMask | g_numlock_mask);

    if (event->keycode == g_hotkeys.transparency_up && (state & Mod1Mask)) {
        adjust_transparency(true);
    } else if (event->keycode == g_hotkeys.transparency_down && (state & Mod1Mask)) {
        adjust_transparency(false);
    } else if (event->keycode == g_hotkeys.unlock_click_through && (state & Mod1Mask)) {
        unlock_click_through();
    } else if (event->keycode == g_hotkeys.pin_window && (state & Mod1Mask)) {
        toggle_pin();
    } else if (event->keycode == g_hotkeys.center_window && (state & ControlMask)) {
        center_window();
    }
}

static void print_startup(void)
{
    printf("transTags Linux started. This build requires an X11 session.\n");
    printf("Hotkeys:\n");
    printf("  Alt+Left/Alt+Right : adjust active window opacity and click-through\n");
    printf("  Alt+Up             : unlock the last click-through window\n");
    printf("  Alt+Down           : pin/unpin last click-through window, or active window\n");
    printf("  Ctrl+Numpad5       : center active window\n");
    printf("Press Ctrl+C in this terminal to exit and restore click-through/topmost states.\n");
    fflush(stdout);
}

int main(void)
{
    const char *session_type = getenv("XDG_SESSION_TYPE");
    if (session_type && strcmp(session_type, "x11") != 0) {
        fprintf(stderr, "Warning: current session is '%s'. Use 'Ubuntu on Xorg' for this tool.\n",
            session_type);
    }

    g_display = XOpenDisplay(NULL);
    if (!g_display) {
        fprintf(stderr, "Cannot open X display. Run this inside an Ubuntu Xorg desktop session.\n");
        return 1;
    }
    XSetErrorHandler(x_error_handler);

    g_screen = DefaultScreen(g_display);
    g_root = RootWindow(g_display, g_screen);
    g_numlock_mask = numlock_mask();
    init_atoms();
    grab_hotkeys();

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    print_startup();

    while (g_running) {
        while (XPending(g_display) > 0) {
            XEvent event;
            XNextEvent(g_display, &event);
            if (event.type == KeyPress) {
                handle_keypress(&event.xkey);
            }
        }
        usleep(10000);
    }

    cleanup();
    XCloseDisplay(g_display);
    return 0;
}
