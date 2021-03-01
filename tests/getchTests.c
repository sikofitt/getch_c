#include "ctest.h"
#include <stdio.h>
#include <linux/input.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <glob.h>

#include "../getch.h"
#define XOR_SWAP(a,b) do\
    {\
      (a) ^= (b);\
      (b) ^= (a);\
      (a) ^= (b);\
    } while (0)

#define EV_PRESSED 1
#define EV_RELEASED 0

inline static void writeToEventDevice(__u16 code)
{
    struct libevdev *dev = NULL;
    glob_t globResult;
    glob("/dev/input/by-path/*-event-kbd", GLOB_NOSORT, NULL, &globResult);

    if(globResult.gl_pathc == 0) {
        fprintf(stderr, "Failed to find input device.");
        globfree(&globResult);
        exit(EXIT_FAILURE);
    }

    char device[strlen(globResult.gl_pathv[0])+1];
    strcpy(device, globResult.gl_pathv[0]);
    globfree(&globResult);

    struct input_event inputEvent[3];
    int eventDevice = open(device, O_WRONLY|O_NONBLOCK);

    if(eventDevice == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    inputEvent[0].time.tv_usec = 0;
    inputEvent[0].time.tv_sec = 0;
    inputEvent[0].type = 4;
    inputEvent[0].code = 4;
    inputEvent[0].value = code;

    inputEvent[1].time.tv_usec = 0;
    inputEvent[1].time.tv_sec = 0;
    inputEvent[1].type = EV_KEY;
    inputEvent[1].code = code;
    inputEvent[1].value = EV_PRESSED;

    inputEvent[2].time.tv_usec = 0;
    inputEvent[2].time.tv_sec = 0;
    inputEvent[2].type = EV_KEY;
    inputEvent[2].code = code;
    inputEvent[2].value = EV_RELEASED;


    int res;

    res = write(eventDevice, &inputEvent, sizeof(inputEvent));
    if(res == -1) {
        perror("Write");
    }

   close(eventDevice);
}

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

inline static void writeStringToStdin(char * string)
{
    size_t stringLength = strlen(string);
    char reversed[stringLength];
    strncpy(reversed, string, stringLength);

    reverseString(reversed);

    for(int i = 0; i<stringLength;i++) {
        ungetc(reversed[i], stdin);
    }
}

static char *asciiCodes[3] = {
        "\x1bOP",
        "\x1b[H",
        "\x1bOQ"
};

static int scanCodes[3] = { KEY_F1, KEY_HOME, KEY_F2 };

CTEST(getchTests, simpleGetchSetReturn_q)
{
    int ungetch = _ungetch('q');
    int getch = _getch();

    ASSERT_EQUAL(getch, ungetch);
    ASSERT_EQUAL(113, ungetch);
    ASSERT_EQUAL(113, getch);
}

CTEST(getchTests, SpecialKey)
{
    writeToEventDevice(scanCodes[0]);
    writeStringToStdin(asciiCodes[0]);

    int key = _getch();
    int code = _getch();

    ASSERT_EQUAL(0, key);
    ASSERT_EQUAL(scanCodes[0], code);
}

CTEST(specialKeyTests, Home)
{
    writeToEventDevice(scanCodes[1]);
    writeStringToStdin(asciiCodes[1]);
    int key = _getch();
    int code = _getch();

    ASSERT_EQUAL(224, key);
    ASSERT_EQUAL(KEY_KP7, code);
}

CTEST(escapeReturnsTest, Esc)
{
    writeToEventDevice(KEY_ESC);
    ungetc(27, stdin);


    int key = _getch();

    ASSERT_EQUAL(27, key);
}