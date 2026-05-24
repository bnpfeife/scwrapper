#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <errno.h>
#include <linux/hidraw.h>
#include <time.h>

#include "virtual_gamepad.h"
#include "virtual_mouse.h"
#include "sc_gamepad_state.h"

static int safe_strcpy(char* const dst, char const* const src, size_t count) {
    if ((count == 0) || (count <= strlen(src))) {
        return 1;
    }

    strcpy(dst, src);

    return 0;
}

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

static int epoll_ctl_add(int epoll_fd, int fd, uint32_t events) {
    struct epoll_event event = {
        .data.fd = fd,
        .events  = events,
    };
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event)) {
        perror("failed to add file-descriptor to epoll");
        return 1;
    }
    return 0;
}

static int epoll_ctl_remove(int epoll_fd, int fd) {
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL)) {
        perror("failed to remove file-descriptor from epoll");
        return 1;
    }
    return 0;
}

struct gamepad {
    int hidraw_fd;
    int ui_gamepad_fd;
    int ui_mouse_fd;
    struct sc_gamepad_state state;
    struct timespec ts_config;
};

int gamepad_destroy(struct gamepad* gamepad, int epoll_fd) {
    int result = 0;

    // epoll
    if (gamepad->hidraw_fd != -1) {
        if (epoll_ctl_remove(epoll_fd, gamepad->hidraw_fd)) {
            result = 1;
        }
    }
    if (gamepad->ui_gamepad_fd != -1) {
        if (epoll_ctl_remove(epoll_fd, gamepad->ui_gamepad_fd)) {
            result = 1;
        }
    }

    // hidraw
    if (gamepad->hidraw_fd != -1) {
        if (close(gamepad->hidraw_fd)) {
            perror("failed to close fd");
            result = 1;
        }
    }

    // uinput
    if (gamepad->ui_gamepad_fd != -1) {
        if (virtual_gamepad_destroy(gamepad->ui_gamepad_fd)) {
            result = 1;
        }
    }
    if (gamepad->ui_mouse_fd != -1) {
        if (virtual_mouse_destroy(gamepad->ui_mouse_fd)) {
            result = 1;
        }
    }

    return result;
}

int gamepad_init(struct gamepad* gamepad, int epoll_fd, int hidraw_fd) {
    gamepad->hidraw_fd     = hidraw_fd;
    gamepad->ui_gamepad_fd = -1;
    gamepad->ui_mouse_fd   = -1;
    memset(&gamepad->state, 0, sizeof(gamepad->state));

    if (virtual_gamepad_setup(&gamepad->ui_gamepad_fd)) {
        gamepad_destroy(gamepad, epoll_fd);
        return 1;
    }

    if (virtual_mouse_setup(&gamepad->ui_mouse_fd)) {
        gamepad_destroy(gamepad, epoll_fd);
        return 1;
    }

    if (epoll_ctl_add(epoll_fd, gamepad->hidraw_fd, EPOLLIN | EPOLLRDHUP)) {
        gamepad_destroy(gamepad, epoll_fd);
        return 1;
    }

    if (epoll_ctl_add(epoll_fd, gamepad->ui_gamepad_fd, EPOLLIN | EPOLLRDHUP)) {
        gamepad_destroy(gamepad, epoll_fd);
        return 1;
    }

    return 0;
}


#define GAMEPAD_MAX_SLOTS 16

struct gamepad_slots {
    struct gamepad_slot {
        char device[NAME_MAX];
        struct gamepad gamepad;
    } inner[GAMEPAD_MAX_SLOTS];
    size_t count;
};

int gamepad_slot_insert(
    struct gamepad_slots* const slots,
    char const* const device,
    struct gamepad* gamepad
) {
    for (size_t i = 0; i < slots->count; i++) {
        if (!strcmp(slots->inner[i].device, device)) {
            fprintf(stderr, "gamepad device already monitored\n");
            return 1;
        }
    }

    if (slots->count < GAMEPAD_MAX_SLOTS) {
        struct gamepad_slot* const slot = &slots->inner[slots->count];
        if (safe_strcpy(slot->device, device, NAME_MAX)) {
            fprintf(stderr, "gamepad device name too long\n");
            return 1;
        }
        memcpy(&slot->gamepad, gamepad, sizeof(slot->gamepad));
        slots->count += 1;
        printf("added %s...\n", device);
        return 0;
    }

    fprintf(stderr, "failed to monitor gamepad device\n");
    return 1;
}

int gamepad_slot_remove(struct gamepad_slots* const slots, size_t index) {
    printf("removing %s...\n", slots->inner[index].device);

    if (index >= slots->count) {
        fprintf(stderr, "failed to remove non-existent gamepad slot\n");
        return 1;
    }
    memmove(
        &slots->inner[index],
        &slots->inner[slots->count - 1],
        sizeof(struct gamepad_slot)
    );
    slots->count -= 1;
    return 0;
}

int try_add_gamepad(
    struct gamepad_slots* const slots,
    int epoll_fd,
    char const* const device,
    bool retry
) {
    if (strncmp(device, "hidraw", strlen("hidraw"))) {
        return 1;
    }

    char path[PATH_MAX + 1] = { 0 };
    if (snprintf(path, PATH_MAX, "/dev/%s", device) >= PATH_MAX) {
        perror("failed to open hidraw device");
        return 1;
    }

    int hidraw_fd = -1;
    // `inotify` can report filesystem events before `udev` has finished
    // applying permissions to the corresponding `hidraw` device. If the
    // device cannot be opened immediately, block and retry. This may
    // cause a noticeable delay when processing game controller input,
    // but it should occur somewhat rarely.
    for (size_t i = 0; i < 10; i++) {
        if (((hidraw_fd = open(path, O_RDWR)) != -1) || !retry) {
            break;
        }
        sleep(1);
    }
    if (hidraw_fd == -1) {
        perror("failed to open hidraw device");
        return 1;
    }

    struct hidraw_devinfo info;
    if (ioctl(hidraw_fd, HIDIOCGRAWINFO, &info) < 0) {
        cleanup_fd(&hidraw_fd);
        return 1;
    }

    // 28de:1302 Steam Controller
    // 28de:1304 Steam Controller Puck
    if (!(info.vendor == 0x28de && info.product == 0x1302) &&
        !(info.vendor == 0x28de && info.product == 0x1304)) {
        cleanup_fd(&hidraw_fd);
        return 1;
    }

    struct gamepad gamepad;
    memset(&gamepad, 0, sizeof(gamepad));
    if (gamepad_init(&gamepad, epoll_fd, hidraw_fd)) {
        return 1;
    }

    if (gamepad_slot_insert(slots, device, &gamepad)) {
        gamepad_destroy(&gamepad, epoll_fd);
        return 1;
    }

    return 0;
}

int gamepad_configure(struct gamepad* gamepad) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
        return 1;
    }

    if (ts.tv_sec - gamepad->ts_config.tv_sec < 5) {
        return 0;
    }
    memcpy(&gamepad->ts_config, &ts, sizeof(ts));

    {
        // https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/steam/controller_constants.h
        uint8_t buffer[64] = { 0 };
        buffer[0] = 0x01; // Report ID 1
        buffer[1] = 0x87; // ID_SET_SETTINGS_VALUES
        buffer[2] = 0x03; // LENGTH
        buffer[3] = 0x09; // SETTING_LIZARD_MODE
        buffer[4] = 0x00; // OFF

        if (ioctl(gamepad->hidraw_fd, HIDIOCSFEATURE(sizeof(buffer)), buffer) < 0) {
            perror("could not configure hidraw device");
            return 1;
        }
    }

    {
        // https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/steam/controller_constants.h
        uint8_t buffer[64] = { 0 };
        buffer[0] = 0x01; // Report ID 1
        buffer[1] = 0x81; // ID_CLEAR_DIGITAL_MAPPINGS
        buffer[2] = 0x00; // LENGTH

        if (ioctl(gamepad->hidraw_fd, HIDIOCSFEATURE(sizeof(buffer)), buffer) < 0) {
            perror("could not configure hidraw device");
            return 1;
        }
    }

    return 0;
}

int handle_gamepad_event(struct gamepad_slots* slots, int epoll_fd, size_t index) {
    struct gamepad_slot* const slot = &slots->inner[index];

    uint8_t buffer[128] = { 0 };

    ssize_t length = read(slot->gamepad.hidraw_fd, buffer, sizeof(buffer));
    if (length == -1) {
        if (errno != EIO) {
            perror("failed to read input from gamepad");
        }

        int result = 0;
        if (gamepad_destroy(&slot->gamepad, epoll_fd)) {
            result = 1;
        }

        if (gamepad_slot_remove(slots, index)) {
            result = 1;
        }

        return result;
    }

    struct sc_gamepad_state state;
    memcpy(&state, &slot->gamepad.state, sizeof(state));
    sc_gamepad_state_update(&state, buffer);
    sc_gamepad_state_send(
        slot->gamepad.ui_gamepad_fd,
        slot->gamepad.ui_mouse_fd,
        &slot->gamepad.state,
        &state);
    // sc_gamepad_state_print(&state);
    memcpy(&slot->gamepad.state, &state, sizeof(state));

    gamepad_configure(&slot->gamepad);

    return 0;
}

#define INOTIFY_BUFFER_SIZE 32 * (sizeof(struct inotify_event) + NAME_MAX)

int handle_inotify_event(
    struct gamepad_slots* const slots,
    int epoll_fd,
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
        return 1;
    }

    for (size_t i = 0; i < length; ) {
        struct inotify_event *event = (struct inotify_event*)&buffer[i];

        // When `len` is zero-length, the event applies to the watched
        // directory rather than a specific file within that directory.
        // In this case `name` will not be null terminated, so it
        // cannot be passed into `try_add_gamepad`.
        if (event->len) {
            (void)try_add_gamepad(slots, epoll_fd, event->name, true);
        }

        // Advance to the next event by skipping the `inotify_event`
        // data and its variable-length name field.
        i += sizeof(struct inotify_event) + event->len;
    }

    return 0;
}

int handle_epoll_event(
    struct gamepad_slots* const slots,
    int epoll_fd,
    int inotify_fd,
    struct epoll_event const* const event
) {
    if (event->data.fd == inotify_fd) {
        return handle_inotify_event(
            slots,
            epoll_fd,
            inotify_fd
        );
    }

    for (size_t i = 0; i < slots->count; i++) {
        struct gamepad_slot* const slot = &slots->inner[i];

        if (event->data.fd == slot->gamepad.hidraw_fd) {
            return handle_gamepad_event(slots, epoll_fd, i);
        }
    }

    return 1;
}


int process_initial(struct gamepad_slots* slots, int epoll_fd) {
    DIR *dir __attribute__((cleanup(cleanup_dir))) = opendir("/dev");
    if (dir == NULL) {
        perror("failed to open /dev directory");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        (void)try_add_gamepad(slots, epoll_fd, entry->d_name, false);
    }

    return 0;
}

#define MAX_EPOLL_EVENTS 32

int main (int argc, char** argv) {
    struct gamepad_slots slots;
    memset(&slots, 0, sizeof(slots));

    // This application is structured so that only `epoll_wait` may block.
    // At least one blocking point is necessary to avoid busy-waiting and
    // excessive CPU usage. However, introducing additional blocking
    // points may cause noticeable delays when processing
    // game controller input.
    int epoll_fd __attribute__((cleanup(cleanup_fd))) = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("failed to create epoll file-descriptor");
        return 1;
    }

    int inotify_fd __attribute__((cleanup(cleanup_fd))) = inotify_init();
    if (inotify_fd == -1) {
        perror("failed to create inotify file-descriptor");
        return 1;
    }

    // Watching "/dev" is required to support game controller hot-plugging.
    // Connecting a Steam Controller creates one new "hidraw" device, while
    // connecting a Steam Controller Puck creates four new "hidraw" devices,
    // even if fewer than four controllers are attached to the puck. These
    // devices are filtered and assigned to controller slots later.
    int watchd = inotify_add_watch(inotify_fd, "/dev", IN_CREATE);
    if (watchd == -1) {
        perror("failed to create inotify watch-descriptor");
        return 1;
    }

    // Watch `inotify` for events...
    if (epoll_ctl_add(epoll_fd, inotify_fd, EPOLLIN)) {
        return 1;
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
    if (process_initial(&slots, epoll_fd)) {
        return 1;
    }

    struct epoll_event events[MAX_EPOLL_EVENTS];
    for (;;) {
        // Wait for changes...
        int event_count = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        if (event_count == -1) {
            perror("failed to wait for epoll events");
            return 1;
        }

        // Due to the `epoll` API, there isn't a convenient way to determine
        // if a specific file-descriptor has changed. Instead, this iterates
        // through the events returned by `epoll_wait` and compares each
        // event's file-descriptor against the target descriptors.
        for (size_t i = 0; i < event_count; i++) {
            handle_epoll_event(&slots, epoll_fd, inotify_fd, &events[i]);
        }
    }
}
