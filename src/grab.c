/* Grab the keyboard on quickshell's own X connection.
 * Copyright (C) 2026 Kikyo987
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>

/* Loaded into the locker with LD_PRELOAD. Qt maps windows through libxcb on
 * the same Display connection, so the keyboard grab has to happen here.
 * A second X connection would own the grab and steal the keystrokes.
 */

static __thread int in_hook;
static pthread_mutex_t cover_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t raiser_once = PTHREAD_ONCE_INIT;
static xcb_window_t covers[8];
static int ncovers;
static xcb_window_t lock_window;
static int grab_done;
static xcb_atom_t atom_wm_name;
static xcb_atom_t atom_net_wm_name;

static int enabled(void) {
    const char *value = getenv("CTOS_X11_LOCK");
    return value && value[0] == '1';
}

static void drop_self_from_preload(void) {
    const char *old = getenv("LD_PRELOAD");
    if (!old || !strstr(old, "libctos-x11-lock-grab.so"))
        return;

    char copy[4096];
    char next[4096];
    snprintf(copy, sizeof copy, "%s", old);
    next[0] = '\0';
    char *save = NULL;
    for (char *tok = strtok_r(copy, ":", &save); tok; tok = strtok_r(NULL, ":", &save)) {
        if (strstr(tok, "libctos-x11-lock-grab.so"))
            continue;
        if (next[0])
            strncat(next, ":", sizeof next - strlen(next) - 1);
        strncat(next, tok, sizeof next - strlen(next) - 1);
    }
    if (next[0])
        setenv("LD_PRELOAD", next, 1);
    else
        unsetenv("LD_PRELOAD");
}

static void mark_ready(void) {
    const char *path = getenv("CTOS_LOCK_READY");
    if (!path || !path[0])
        return;
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "ctos-lock-grab: ready file: %s\n", strerror(errno));
        return;
    }
    if (write(fd, "ok\n", 3) != 3)
        fprintf(stderr, "ctos-lock-grab: ready write failed\n");
    close(fd);
}

static void *raiser(void *unused) {
    (void)unused;
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    if (!conn || xcb_connection_has_error(conn)) {
        fprintf(stderr, "ctos-lock-grab: raiser could not open display\n");
        return NULL;
    }

    for (;;) {
        xcb_window_t local[8];
        int count;
        pthread_mutex_lock(&cover_mu);
        count = ncovers;
        memcpy(local, covers, sizeof local);
        pthread_mutex_unlock(&cover_mu);

        uint32_t above = XCB_STACK_MODE_ABOVE;
        for (int i = 0; i < count; i++)
            xcb_configure_window(conn, local[i], XCB_CONFIG_WINDOW_STACK_MODE, &above);
        xcb_flush(conn);
        usleep(300000);
    }
}

static void start_raiser(void) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, raiser, NULL) == 0)
        pthread_detach(thread);
    else
        fprintf(stderr, "ctos-lock-grab: raiser thread failed\n");
}

static void remember(xcb_window_t window) {
    pthread_mutex_lock(&cover_mu);
    int known = 0;
    for (int i = 0; i < ncovers; i++) {
        if (covers[i] == window)
            known = 1;
    }
    if (!known && ncovers < 8)
        covers[ncovers++] = window;
    pthread_mutex_unlock(&cover_mu);
    pthread_once(&raiser_once, start_raiser);
}

static void ensure_atoms(xcb_connection_t *conn) {
    if (atom_wm_name)
        return;
    xcb_intern_atom_cookie_t wm = xcb_intern_atom(conn, 0, 7, "WM_NAME");
    xcb_intern_atom_cookie_t net = xcb_intern_atom(conn, 0, 12, "_NET_WM_NAME");
    xcb_intern_atom_reply_t *wm_reply = xcb_intern_atom_reply(conn, wm, NULL);
    xcb_intern_atom_reply_t *net_reply = xcb_intern_atom_reply(conn, net, NULL);
    if (wm_reply) {
        atom_wm_name = wm_reply->atom;
        free(wm_reply);
    }
    if (net_reply) {
        atom_net_wm_name = net_reply->atom;
        free(net_reply);
    }
}

static int is_lock_name(const void *data, uint32_t len, uint8_t format) {
    if (format != 8 || !data)
        return 0;
    const char *text = data;
    if (len == 9 && memcmp(text, "ctos-lock", 9) == 0)
        return 1;
    if (len > 9 && memcmp(text, "ctos-lock", 9) == 0 && text[9] == '\0')
        return 1;
    return 0;
}

static int is_viewable(xcb_connection_t *conn, xcb_window_t window) {
    xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(conn, window);
    xcb_get_window_attributes_reply_t *reply = xcb_get_window_attributes_reply(conn, cookie, NULL);
    if (!reply)
        return 0;
    int viewable = reply->map_state == XCB_MAP_STATE_VIEWABLE;
    free(reply);
    return viewable;
}

static int grab_devices(xcb_connection_t *conn, xcb_window_t window) {
    for (int attempt = 0; attempt < 3; attempt++) {
        xcb_grab_keyboard_cookie_t keyboard = xcb_grab_keyboard(
            conn, 1, window, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        xcb_grab_keyboard_reply_t *keyboard_reply = xcb_grab_keyboard_reply(conn, keyboard, NULL);
        int keyboard_status = keyboard_reply ? keyboard_reply->status : -1;
        free(keyboard_reply);

        uint16_t mask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
            | XCB_EVENT_MASK_POINTER_MOTION;
        xcb_grab_pointer_cookie_t pointer = xcb_grab_pointer(
            conn, 1, window, mask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
            XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);
        xcb_grab_pointer_reply_t *pointer_reply = xcb_grab_pointer_reply(conn, pointer, NULL);
        int pointer_status = pointer_reply ? pointer_reply->status : -1;
        free(pointer_reply);

        fprintf(stderr, "ctos-lock-grab: attempt %d keyboard %d pointer %d window %u\n",
            attempt, keyboard_status, pointer_status, window);
        fflush(stderr);

        if (keyboard_status == 0 && pointer_status == 0) {
            xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
            xcb_flush(conn);
            return 1;
        }
        if (keyboard_status == 0)
            xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
        if (pointer_status == 0)
            xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        xcb_flush(conn);
        usleep(40000);
    }
    return 0;
}

static void finish_grab(xcb_connection_t *conn, xcb_window_t window) {
    if (grab_done || !is_viewable(conn, window))
        return;
    if (!grab_devices(conn, window))
        return;
    grab_done = 1;
    mark_ready();
    fprintf(stderr, "ctos-lock-grab: locked\n");
    fflush(stderr);
}

static void note_title(xcb_connection_t *conn, xcb_window_t window, xcb_atom_t property,
    const void *data, uint32_t len, uint8_t format) {
    if (!enabled() || grab_done)
        return;
    ensure_atoms(conn);
    if (property != atom_wm_name && property != atom_net_wm_name)
        return;
    if (!is_lock_name(data, len, format))
        return;
    lock_window = window;
    fprintf(stderr, "ctos-lock-grab: titled window %u\n", window);
    fflush(stderr);
    remember(window);
    finish_grab(conn, window);
}

static void prepare_window(xcb_connection_t *conn, xcb_window_t window) {
    uint32_t redirect = 1;
    xcb_change_window_attributes(conn, window, XCB_CW_OVERRIDE_REDIRECT, &redirect);

    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(conn, window);
    xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(conn, cookie, NULL);
    if (!reply)
        return;
    fprintf(stderr, "ctos-lock-grab: map %u %ux%u\n", window, reply->width, reply->height);
    fflush(stderr);
    if (reply->width >= 200 && reply->height >= 200)
        remember(window);
    free(reply);
}

static void after_map(xcb_connection_t *conn, xcb_window_t window) {
    if (lock_window && window == lock_window)
        finish_grab(conn, window);
}

xcb_void_cookie_t xcb_map_window(xcb_connection_t *conn, xcb_window_t window) {
    static xcb_void_cookie_t (*real_map)(xcb_connection_t *, xcb_window_t) = NULL;
    if (!real_map)
        real_map = dlsym(RTLD_NEXT, "xcb_map_window");

    if (!enabled() || in_hook)
        return real_map(conn, window);

    in_hook = 1;
    prepare_window(conn, window);
    in_hook = 0;
    xcb_void_cookie_t cookie = real_map(conn, window);
    in_hook = 1;
    after_map(conn, window);
    in_hook = 0;
    return cookie;
}

xcb_void_cookie_t xcb_change_property(xcb_connection_t *conn, uint8_t mode, xcb_window_t window,
    xcb_atom_t property, xcb_atom_t type, uint8_t format, uint32_t data_len, const void *data) {
    static xcb_void_cookie_t (*real_change)(xcb_connection_t *, uint8_t, xcb_window_t, xcb_atom_t,
        xcb_atom_t, uint8_t, uint32_t, const void *) = NULL;
    if (!real_change)
        real_change = dlsym(RTLD_NEXT, "xcb_change_property");

    xcb_void_cookie_t cookie = real_change(conn, mode, window, property, type, format, data_len, data);
    if (enabled() && !in_hook) {
        in_hook = 1;
        note_title(conn, window, property, data, data_len, format);
        in_hook = 0;
    }
    return cookie;
}

__attribute__((constructor)) static void ctos_lock_grab_init(void) {
    drop_self_from_preload();
    if (enabled()) {
        fprintf(stderr, "ctos-lock-grab: loaded\n");
        fflush(stderr);
    }
}
