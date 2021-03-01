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

#define EVENT_DEVICE_GLOB "/dev/input/by-path/*-event-kbd"

#define FKEY(k) ((k) >= KEY_F1 && (k) <= KEY_F10) || (k) == KEY_F12 || (k) == KEY_F11
#define NOTNUMPAD(k) ((k) >= KEY_HOME && (k) <= KEY_DELETE)
#define XOR_SWAP(a,b) do\
    {\
      (a) ^= (b);\
      (b) ^= (a);\
      (a) ^= (b);\
    } while (0)

static struct termios oldTermAttributes;

inline static void reverseString(char * str)
{
    if (str)
    {
        char * end = str + strlen(str) - 1;

        while (str < end)
        {
            XOR_SWAP(*str, *end);
            str++;
            end--;
        }
    }
}

static int discardRead(unsigned int length)
{
    char buffer[length];
    ssize_t bytesRead;

    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags|O_NONBLOCK);

    if( (bytesRead = fread(buffer, sizeof(char), length, stdin)) == -1) {
        perror("discardRead");
    }

    fcntl(STDIN_FILENO, F_SETFL, flags);

    return (int)bytesRead;
}

static int getEventDevice(char ** device)
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
            *device = (char *)malloc(pathLength);
            strcpy(*device, search.gl_pathv[i]);
            globfree(&search);
            return 1;
        }
    }

    globfree(&search);
    return -1;
}

static unsigned short getScanCode()
{
    struct input_event inputEvent[5];
    int eventDevice;

    char *device;
    if(getEventDevice(&device) == -1) {
         perror("getEventDevice");
         free(device);
        return KEY_RESERVED;
    }

    if( ( eventDevice = open(device, O_RDONLY)) == -1 ) {
        free(device);
        perror("open");
        return KEY_RESERVED;
    };

    free(device);

    if( read(eventDevice, &inputEvent, sizeof(inputEvent)) == -1) {
        close(eventDevice);
        perror("read");
        return KEY_RESERVED;
    }

    close(eventDevice);

    for(int i = 0;i<5;i++) {
        if(inputEvent[i].type == EV_KEY && inputEvent[i].code != KEY_ENTER) {
            return inputEvent[i].code;
        }
    }

    return KEY_RESERVED;
}

inline static void pushStdin(int c) {
    ungetc(c, stdin);
}

static void setNormalMode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTermAttributes);
}

static void setRawMode(void)
{
    tcgetattr(STDIN_FILENO, &oldTermAttributes);

    struct termios raw = oldTermAttributes;

    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

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

    atexit(setNormalMode);
    setRawMode();

    int key = readKey();

    setNormalMode();

    return key;
}

int getch(void)
{
    return _getch();
}

int _ungetch(int ch)
{
    return ungetc(ch, stdin);
}

int ungetch(int ch)
{
    return _ungetch(ch);
}

int *cinPeekCount(ushort count)
{
    char buffer[count];

    int flags = fcntl(STDIN_FILENO, F_GETFL);

    fcntl(STDIN_FILENO, F_SETFL, flags|O_NONBLOCK);

    size_t byteCount = fread(&buffer, sizeof(char), count, stdin);

    fcntl(STDIN_FILENO, F_SETFL, flags);

    int *res = (int*)malloc(byteCount);

    for(int i=0;i<byteCount;i++)
    {
            res[i] = (int)buffer[i];
    }

    reverseString(buffer);

    for(int i=0;i<byteCount;i++) {
        pushStdin(buffer[i]);
    }

    return res;
}

int cinPeek()
{
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags|O_NONBLOCK);
    int result = fgetc(stdin);
    fcntl(STDIN_FILENO, F_SETFL, flags);
    ungetc(result, stdin);
    return result;
}