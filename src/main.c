#include "config.h"
#include "constants.h"
#include "gamepad.h"
#include "sc_epoll.h"
#include "sc_string.h"

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/hidraw.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

// Steam vendor/product IDs
#define STEAM_VENDOR_ID 0x28de
#define STEAM_PRODUCT_CONTROLLER 0x1302
#define STEAM_PRODUCT_CONTROLLER_BLUETOOTH 0x1303
#define STEAM_PRODUCT_PUCK 0x1304

static void cleanup_fd(int *fd) {
    if (close(*fd)) {
        perror("failed to close file-descriptor");
    }
}

static void cleanup_dir(DIR** dir) {
    if (closedir(*dir)) {
        perror("failed to close directory");
    }
}

#define GAMEPAD_MAX_SLOTS 16

struct gamepad_slots {
    struct gamepad_slot {
        char device[NAME_MAX];
        struct Gamepad gamepad;
    } inner[GAMEPAD_MAX_SLOTS];
    size_t count;
};

int gamepad_slot_insert(
    struct gamepad_slots* const slots,
    char const* const device,
    struct Gamepad* gamepad
) {
    for (size_t i = 0; i < slots->count; i++) {
        if (!strcmp(slots->inner[i].device, device)) {
            fprintf(stderr, "gamepad device already monitored\n");
            return RET_ERROR;
        }
    }

    if (slots->count < GAMEPAD_MAX_SLOTS) {
        struct gamepad_slot* const slot = &slots->inner[slots->count];
        if (safe_strcpy(slot->device, device, NAME_MAX)) {
            fprintf(stderr, "gamepad device name too long\n");
            return RET_ERROR;
        }
        memcpy(&slot->gamepad, gamepad, sizeof(slot->gamepad));
        slots->count += 1;
        printf("added %s...\n", device);
        return RET_OKAY;
    }

    fprintf(stderr, "failed to monitor gamepad device\n");
    return RET_ERROR;
}

int gamepad_slot_remove(struct gamepad_slots* const slots, size_t index) {
    printf("removing %s...\n", slots->inner[index].device);

    if (index >= slots->count) {
        fprintf(stderr, "failed to remove non-existent gamepad slot\n");
        return RET_ERROR;
    }
    memmove(
        &slots->inner[index],
        &slots->inner[slots->count - 1],
        sizeof(struct gamepad_slot)
    );
    slots->count -= 1;
    return RET_OKAY;
}

int try_add_gamepad(
    struct gamepad_slots* const slots,
    char const* const device,
    bool retry
) {
    if (strncmp(device, "hidraw", strlen("hidraw"))) {
        return RET_ERROR;
    }

    char path[PATH_MAX + 1] = { 0 };
    if (snprintf(path, PATH_MAX, "/dev/%s", device) >= PATH_MAX) {
        perror("failed to open hidraw device");
        return RET_ERROR;
    }

    int hidraw_fd = -1;
    // `inotify` can report filesystem events before `udev` has finished
    // applying permissions to the corresponding `hidraw` device. If the
    // device cannot be opened immediately, block and retry. This may
    // cause a noticeable delay when processing game controller input,
    // but it should occur somewhat rarely.
    for (size_t i = 0; i < 10; i++) {
        if (((hidraw_fd = open(path, O_RDWR | O_NONBLOCK)) != -1) || !retry) {
            break;
        }
        sleep(1);
    }
    if (hidraw_fd == -1) {
        perror("failed to open hidraw device");
        return RET_ERROR;
    }

    struct hidraw_devinfo info;
    memset(&info, 0, sizeof(info));
    if (ioctl(hidraw_fd, HIDIOCGRAWINFO, &info) < 0) {
        cleanup_fd(&hidraw_fd);
        return RET_ERROR;
    }

    bool is_steam = (info.vendor == STEAM_VENDOR_ID) && (
        (info.product == STEAM_PRODUCT_CONTROLLER) ||
        (info.product == STEAM_PRODUCT_CONTROLLER_BLUETOOTH) ||
        (info.product == STEAM_PRODUCT_PUCK));

    if (!is_steam) {
        cleanup_fd(&hidraw_fd);
        return RET_ERROR;
    }

    struct Gamepad gamepad;
    memset(&gamepad, 0, sizeof(gamepad));
    if (Gamepad_init(&gamepad, hidraw_fd)) {
        return RET_ERROR;
    }

    if (gamepad_slot_insert(slots, device, &gamepad)) {
        (void)Gamepad_free(&gamepad);
        return RET_ERROR;
    }

    return RET_OKAY;
}

#define INOTIFY_BUFFER_SIZE 32 * (sizeof(struct inotify_event) + NAME_MAX)

int handle_inotify_event(
    struct gamepad_slots* const slots,
    int inotify_fd
) {
    // `inotify_event` contains a flexible array member (`name`), which
    // prevents constructing an array of events directly. Instead, this
    // allocates a buffer large enough to hold `MAX_INOTIFY_EVENTS`,
    // each with the maximum possible name length (`NAME_MAX`).
    uint8_t buffer[INOTIFY_BUFFER_SIZE] = { 0 };

    ssize_t length = read(inotify_fd, buffer, INOTIFY_BUFFER_SIZE);
    if (length == -1) {
        perror("failed to read inotify events");
        return RET_ERROR;
    }

    for (size_t i = 0; i < length; ) {
        struct inotify_event *event = (struct inotify_event*)&buffer[i];

        // When `len` is zero-length, the event applies to the watched
        // directory rather than a specific file within that directory.
        // In this case `name` will not be null terminated, so it
        // cannot be passed into `try_add_gamepad`.
        if (event->len) {
            (void)try_add_gamepad(slots, event->name, true);
        }

        // Advance to the next event by skipping the `inotify_event`
        // data and its variable-length name field.
        i += sizeof(struct inotify_event) + event->len;
    }

    return RET_OKAY;
}

int handle_epoll_event(
    struct gamepad_slots* const slots,
    int inotify,
    struct epoll_event const* const event
) {
    if (event->data.fd == inotify) {
        return handle_inotify_event(slots, inotify);
    }

    for (size_t i = 0; i < slots->count; i++) {
        struct gamepad_slot* const slot = &slots->inner[i];

        if (Gamepad_get_hidraw(&slot->gamepad) == event->data.fd) {
            if (Gamepad_hidraw_event(&slot->gamepad)) {
                (void)Gamepad_free(&slot->gamepad);
                if (gamepad_slot_remove(slots, i)) {
                    return RET_ERROR;
                }
            }
            return RET_OKAY;
        }

        if (Gamepad_get_uinput(&slot->gamepad) == event->data.fd) {
            if (Gamepad_uinput_event(&slot->gamepad)) {
                return RET_ERROR;
            }
            return RET_OKAY;
        }
    }

    return RET_ERROR;
}

int process_initial(struct gamepad_slots* slots) {
    DIR *dir __attribute__((cleanup(cleanup_dir))) = opendir("/dev");
    if (dir == NULL) {
        perror("failed to open /dev directory");
        return RET_ERROR;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        (void)try_add_gamepad(slots, entry->d_name, false);
    }

    return RET_OKAY;
}

#define MAX_EPOLL_EVENTS 32

int main (int argc, char** argv) {
    if (Config_init("config.ini")) {
        fprintf(stderr, "failed to load config.ini\n");
        return RET_ERROR;
    }

    struct gamepad_slots slots;
    memset(&slots, 0, sizeof(slots));

    // This application is structured so that only `epoll_wait` may block.
    // At least one blocking point is necessary to avoid busy-waiting and
    // excessive CPU usage. However, introducing additional blocking
    // points may cause noticeable delays when processing
    // game controller input.
    if (sc_epoll_init()) {
        return RET_ERROR;
    }

    int inotify __attribute__((cleanup(cleanup_fd))) = inotify_init();
    if (inotify == -1) {
        perror("failed to create inotify file-descriptor");
        return RET_ERROR;
    }

    // Watching "/dev" is required to support game controller hot-plugging.
    // Connecting a Steam Controller creates one new "hidraw" device, while
    // connecting a Steam Controller Puck creates four new "hidraw" devices,
    // even if fewer than four controllers are attached to the puck. These
    // devices are filtered and assigned to controller slots later.
    int watchd = inotify_add_watch(inotify, "/dev", IN_CREATE);
    if (watchd == -1) {
        perror("failed to create inotify watch-descriptor");
        return RET_ERROR;
    }

    // Watch `inotify` for events...
    if (sc_epoll_ctl_add(inotify, EPOLLIN)) {
        return RET_ERROR;
    }

    // This performs an initial pass looking for matching "hidraw" devices.
    // This is necessary because the application may start after the Steam
    // Controller or Steam Controller Puck have already been connected.
    //
    // There is a small race window during startup: if either device is
    // connected immediately before application startup, `udev` may not
    // have finished applying permissions to the corresponding "hidraw"
    // device yet. In that case, we will fail to listen to the device
    // for this session, though this situation
    // should be extremely rare.
    if (process_initial(&slots)) {
        return RET_ERROR;
    }

    struct epoll_event events[MAX_EPOLL_EVENTS];
    memset(events, 0, sizeof(events));

    for (;;) {
        // Wait for changes...
        int event_count = epoll_wait(epoll, events, MAX_EPOLL_EVENTS, 1);
        if (event_count == -1) {
            perror("failed to wait for epoll events");
            return RET_ERROR;
        }

        // Due to the `epoll` API, there isn't a convenient way to determine
        // if a specific file-descriptor has changed. Instead, this iterates
        // through the events returned by `epoll_wait` and compares each
        // event's file-descriptor against the target descriptors.
        for (size_t i = 0; i < event_count; i++) {
            (void)handle_epoll_event(&slots, inotify, &events[i]);
        }

        for (size_t i = 0; i < slots.count; i++) {
            (void)Gamepad_update(&slots.inner[i].gamepad);
            (void)Gamepad_update_haptics(&slots.inner[i].gamepad);
        }
    }

    return RET_OKAY;
}
