/***********************************************************************
 * This Source Code Form is subject to the terms of the Mozilla Public *
 * License, v. 2.0. If a copy of the MPL was not distributed with this *
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.            *
 ***********************************************************************/

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <glob.h>
#include "getch.h"

#define FKEY(k) ((k) >= KEY_F1 && (k) <= KEY_F10) || (k) == KEY_F12 || (k) == KEY_F11
#define NOTNUMPAD(k) ((k) >= KEY_HOME && (k) <= KEY_DELETE)

#define EVENT_DEVICE_GLOB "/dev/input/by-path/*-event-kbd"

static struct termios oldTermAttributes;

static void setRawMode(void);
static void setNormalMode(void);

inline static int discardRead(unsigned int length)
{
    char buffer[length];
    ssize_t bytes_read;

    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags|O_NONBLOCK);

    if( (bytes_read = fread(buffer, sizeof(char), length, stdin)) == -1) {
        perror("discardRead");
    }

    fcntl(STDIN_FILENO, F_SETFL, flags);

    return (int)bytes_read;
}

int getEventDevice(const char * device)
{
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

    size_t pathLength;

    for(int i = 0; i<search.gl_pathc;i++) {
        if(access(search.gl_pathv[i], F_OK) == 0) {
            pathLength = strlen(search.gl_pathv[i]) + 1;
            strncpy((char *)device, search.gl_pathv[i], pathLength);
            globfree(&search);
            return 1;
        }
    }

    globfree(&search);
    return -1;
}

unsigned short getScanCode()
{
    struct input_event inputEvent[5];
    int fd;

    const char device[FILENAME_MAX];
    if(-1 == getEventDevice(device)) {
        return KEY_RESERVED;
    }

    if( (fd = open(device, O_RDONLY)) == -1 ) {
        return KEY_RESERVED;
    };

    if( read(fd, &inputEvent, sizeof (inputEvent)) == -1 ) {
        return KEY_RESERVED;
    }

    close(fd);

    for(int i = 0; i<=5; i++) {
        if(inputEvent[i].type == EV_KEY) {
            return inputEvent[i].code;
        }
    }

    return KEY_RESERVED;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-narrowing-conversions"
inline static char *reverseString(char *str)
{
      char *p1, *p2;

      if (! str || ! *str)
            return str;
      for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
      {
            *p1 ^= *p2;
            *p2 ^= *p1;
            *p1 ^= *p2;
      }
      return str;
}
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma ide diagnostic ignored "bugprone-reserved-identifier"
inline static void pushStdin(int c) {
    ungetc(c, stdin);
}
#pragma clang diagnostic pop

inline static void setNormalMode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermAttributes);
}

inline static void setRawMode(void)
{
    tcgetattr(STDIN_FILENO, &oldTermAttributes);

    struct termios raw = oldTermAttributes;

    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

}

unsigned short resolveScanCode(unsigned short key)
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

int readKey(void) {

    unsigned short scanCode;
    int key = getchar();

    if (key == 27) {

        scanCode = getScanCode();

        if (scanCode == KEY_ESC) {
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

    atexit(setNormalMode);
    setRawMode();

    int key = readKey();

    setNormalMode();

    return key;
}

int _ungetch(int ch)
{
    return ungetc(ch, stdin);
}