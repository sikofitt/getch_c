/***********************************************************************
 * This Source Code Form is subject to the terms of the Mozilla Public *
 * License, v. 2.0. If a copy of the MPL was not distributed with this *
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.            *
 ***********************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <glob.h>
#include "getch.h"

#define EVENT_DEVICE_GLOB "/dev/input/by-path/*-event-kbd"

#define FKEY(k) ((k) >= KEY_F1 && (k) <= KEY_F10) || (k) == KEY_F12 || (k) == KEY_F11
#define NOTNUMPAD(k) ((k) >= KEY_HOME && (k) <= KEY_DELETE)

static struct termios oldTermAttributes;
static int rawModeActive = 0;

inline static int pushStdin(int c) {
    return ungetc(c, stdin);
}

/* Reads up to length bytes from stdin without blocking; returns the count read. */
static size_t readNonBlocking(void * buffer, size_t length)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL);

    if(flags == -1) {
        return 0;
    }

    fcntl(STDIN_FILENO, F_SETFL, flags|O_NONBLOCK);

    errno = 0;
    size_t bytesRead = fread(buffer, sizeof(char), length, stdin);

    // Only forget the EAGAIN caused by O_NONBLOCK; keep real EOF and I/O errors.
    if(ferror(stdin) && errno == EAGAIN) {
        clearerr(stdin);
    }

    fcntl(STDIN_FILENO, F_SETFL, flags);

    return bytesRead;
}

/* Pushes buffer back onto stdin so it is read again in its original order. */
static int pushBackBytes(const unsigned char * buffer, size_t length)
{
    for(size_t i = length; i > 0; i--) {
        if(pushStdin(buffer[i - 1]) == EOF) {
            return -1;
        }
    }

    return 0;
}

static int discardRead(unsigned int length)
{
    char buffer[length];

    return (int)readNonBlocking(buffer, length);
}

static int getEventDevice(char ** device)
{
    // Lets tests (or users) point at a different input event device or recorded file.
    // secure_getenv ignores it in setuid/setgid processes.
    const char *override = secure_getenv("GETCH_EVENT_DEVICE");

    if(override != NULL && *override != '\0') {
        *device = strdup(override);
        return *device != NULL ? 1 : -1;
    }

    glob_t search;

    int globResult = glob(
            EVENT_DEVICE_GLOB,
            GLOB_NOSORT,
            NULL,
            &search
        );

    if(0 != globResult) {
        globfree(&search);
        return -1;
    }

    if(search.gl_pathc == 0) {
        globfree(&search);
        return -1;
    }

    for(size_t i = 0; i<search.gl_pathc;i++) {
        if(access(search.gl_pathv[i], F_OK) == 0) {
            *device = strdup(search.gl_pathv[i]);
            globfree(&search);
            return *device != NULL ? 1 : -1;
        }
    }

    globfree(&search);
    return -1;
}

static unsigned short getScanCode()
{
    struct input_event inputEvent[5];
    int eventDevice;

    char *device = NULL;
    if(getEventDevice(&device) == -1) {
        fputs("getch: no keyboard event device found\n", stderr);
        return KEY_RESERVED;
    }

    if( ( eventDevice = open(device, O_RDONLY)) == -1 ) {
        free(device);
        perror("open");
        return KEY_RESERVED;
    };

    free(device);

    ssize_t bytesRead = read(eventDevice, &inputEvent, sizeof(inputEvent));

    close(eventDevice);

    if(bytesRead == -1) {
        perror("read");
        return KEY_RESERVED;
    }

    size_t eventCount = (size_t)bytesRead / sizeof(struct input_event);

    for(size_t i = 0; i < eventCount; i++) {
        if(inputEvent[i].type == EV_KEY && inputEvent[i].code != KEY_ENTER) {
            return inputEvent[i].code;
        }
    }

    return KEY_RESERVED;
}

static void setNormalMode(void)
{
    if(rawModeActive) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTermAttributes);
        rawModeActive = 0;
    }
}

static void setRawMode(void)
{
    if(rawModeActive || tcgetattr(STDIN_FILENO, &oldTermAttributes) == -1) {
        return;
    }

    struct termios raw = oldTermAttributes;

    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    rawModeActive = 1;
}

static unsigned short resolveScanCode(unsigned short key)
{
    switch(key) {
        case KEY_DOWN: return KEY_KP2;
        case KEY_LEFT: return KEY_KP4;
        case KEY_RIGHT: return KEY_KP6;
        case KEY_UP: return KEY_KP8;
        case KEY_HOME: return KEY_KP7;
        case KEY_END: return KEY_KP1;
        case KEY_INSERT: return KEY_KP0;
        case KEY_DELETE: return KEY_KPDOT;
        case KEY_PAGEUP: return KEY_KP9;
        case KEY_PAGEDOWN: return KEY_KP3;
        default: return key;
    };
}

static int readKey(void) {

    int key = getchar();

    if (key == 27) {

        unsigned short scanCode = getScanCode();

        if (scanCode == KEY_ESC ) {
            return 27;
        }

        int returnResult = 0;

        if (NOTNUMPAD(scanCode)) {
            scanCode = resolveScanCode(scanCode);
            returnResult = 224;
        }

        u_short discardBytes;

        switch (scanCode) {
            case KEY_F1:
            case KEY_F2:
            case KEY_F3:
            case KEY_F4:
            case KEY_KP1: // END
            case KEY_KP2: // DOWN
            case KEY_KP4: // LEFT
            case KEY_KP6: // RIGHT
            case KEY_KP7: // HOME
            case KEY_KP8: // UP
                discardBytes = 2;
                break;
            case KEY_KP0: // INSERT
            case KEY_KPDOT: // DELETE
            case KEY_KP9: // PAGEUP
            case KEY_KP3: // PAGEDOWN
                discardBytes = 3;
                break;
            case KEY_F5:
            case KEY_F6:
            case KEY_F7:
            case KEY_F8:
            case KEY_F9:
            case KEY_F10:
            case KEY_F11:
            case KEY_F12:
                discardBytes = 4;
                break;

            default:
                discardBytes = 0;
                break;
        }

        if (discardBytes != 0) {
            discardRead(discardBytes);
        }

        pushStdin(scanCode);

        return returnResult;
    }

    return key;
}

int _getch(void) {

    static int exitHandlerRegistered = 0;

    if(!exitHandlerRegistered) {
        atexit(setNormalMode);
        exitHandlerRegistered = 1;
    }

    setRawMode();

    int key = readKey();

    setNormalMode();

    return key;
}

#ifdef GETCH_ENABLE_ALIASES
int getch(void)
{
    return _getch();
}
#endif

int _ungetch(int ch)
{
    return ungetc(ch, stdin);
}

#ifdef GETCH_ENABLE_ALIASES
int ungetch(int ch)
{
    return _ungetch(ch);
}
#endif

/* Like std::cin.peek(): blocks until a byte is available, returns it without
 * consuming it, and returns EOF only at end of input. No Enter is required. */
int cinPeek(void)
{
    setRawMode();
    int result = getchar();
    setNormalMode();

    if(result != EOF) {
        pushStdin(result);
    }

    return result;
}

/* Blocks for the first byte like cinPeek(), then also takes whatever else is
 * already available, up to count bytes. */
int *cinPeekCount(unsigned short count, size_t *length)
{
    if(length) {
        *length = 0;
    }

    if(count == 0) {
        return NULL;
    }

    unsigned char buffer[count];

    setRawMode();

    int first = getchar();
    size_t byteCount = 0;

    if(first != EOF) {
        buffer[byteCount++] = (unsigned char)first;

        if(count > 1) {
            byteCount += readNonBlocking(buffer + 1, count - 1);
        }
    }

    setNormalMode();

    if(byteCount == 0) {
        return NULL;
    }

    // ISO C guarantees one byte of pushback; glibc allows more.
    if(pushBackBytes(buffer, byteCount) == -1) {
        errno = ENOBUFS;
        return NULL;
    }

    int *res = (int *)malloc(byteCount * sizeof *res);

    if(res == NULL) {
        return NULL;
    }

    for(size_t i = 0; i < byteCount; i++) {
        res[i] = buffer[i];
    }

    if(length) {
        *length = byteCount;
    }

    return res;
}
