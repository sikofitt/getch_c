#include "ctest.h"
#include <stdio.h>
#include <linux/input.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>

#include "../getch.h"

#define EV_PRESSED 1
#define EV_RELEASED 0

inline static void writeToEventDevice(__u16 code)
{
    glob_t globResult;
    glob("/dev/input/by-path/*-event-kbd", GLOB_NOSORT, NULL, &globResult);

    if(globResult.gl_pathc == 0) {
        fprintf(stderr, "Failed to find input device.");
        globfree(&globResult);
        exit(EXIT_FAILURE);
    }

    char device[strlen(globResult.gl_pathv[0])];
    strncpy(device, globResult.gl_pathv[0], strlen(globResult.gl_pathv[0]) + 1);
    globfree(&globResult);

    struct input_event inputEvent[2];
    int eventDevice = open(device, O_WRONLY);

    if(eventDevice == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    for(int i=0; i<2;i++) {
        gettimeofday(&inputEvent[i].time, NULL);
        inputEvent[i].type = EV_KEY;
        inputEvent[i].code = code;
        inputEvent[i].value = i ? EV_RELEASED : EV_PRESSED;
    }

    write(eventDevice, &inputEvent, sizeof(inputEvent));


    close(eventDevice);

    sync();

    fflush(NULL);

}

inline static void reverseString(char * str)
{
    if (str)
    {
        char * end = str + strlen(str) - 1;

        // swap the values in the two given variables
        // XXX: fails when a and b refer to same memory location
#   define XOR_SWAP(a,b) do\
    {\
      (a) ^= (b);\
      (b) ^= (a);\
      (a) ^= (b);\
    } while (0)

        // walk inwards from both ends of the string,
        // swapping until we get to the middle
        while (str < end)
        {
            XOR_SWAP(*str, *end);
            str++;
            end--;
        }
#   undef XOR_SWAP
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
        "\x1bOPP",
        "\x1b[H",
        "\x1bOJ"
};

static int scanCodes[3] = { KEY_F1, KEY_HOME, KEY_F5 };

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

    writeStringToStdin(asciiCodes[0]);
    writeToEventDevice(scanCodes[0]);

    int key = _getch();
    int code = _getch();

    ASSERT_EQUAL(0, key);
    ASSERT_EQUAL(scanCodes[0], code);
}

CTEST(specialKeyTests, Home)
{

    writeStringToStdin(asciiCodes[1]);
    writeToEventDevice(KEY_HOME);

    int key = _getch();
    int code = _getch();

    ASSERT_EQUAL(224, key);
    ASSERT_EQUAL(KEY_KP7, code);
}

CTEST(escapeReturnsTest, Esc)
{
    ungetc(27, stdin);
    writeToEventDevice(KEY_ESC);
    int key = _getch();

    ASSERT_EQUAL(27, key);
}