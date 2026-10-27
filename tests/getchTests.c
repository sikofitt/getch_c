#include "ctest.h"
#include <stdio.h>
#include <linux/input.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <glob.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>

#include "../getch.h"
#define EV_PRESSED 1
#define EV_RELEASED 0

static char eventFile[] = "/tmp/getchEventsXXXXXX";

static void removeEventFile(void)
{
    unlink(eventFile);
}

/* Records input events for getch.c to read via GETCH_EVENT_DEVICE instead of
 * injecting events into the real keyboard. */
static void writeEventFile(const struct input_event * events, size_t count)
{
    static int created = 0;
    int fd;

    if(!created) {
        fd = mkstemp(eventFile);
        atexit(removeEventFile);
        created = 1;
    } else {
        fd = open(eventFile, O_WRONLY|O_TRUNC);
    }

    if(fd == -1) {
        perror("event file");
        exit(EXIT_FAILURE);
    }

    size_t size = count * sizeof(*events);

    if(write(fd, events, size) != (ssize_t)size) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    close(fd);
    setenv("GETCH_EVENT_DEVICE", eventFile, 1);
}

inline static void writeToEventDevice(__u16 code)
{
    struct input_event inputEvent[3] = {
        { .type = EV_MSC, .code = MSC_SCAN, .value = code },
        { .type = EV_KEY, .code = code, .value = EV_PRESSED },
        { .type = EV_KEY, .code = code, .value = EV_RELEASED },
    };

    writeEventFile(inputEvent, 3);
}

inline static void writeStringToStdin(const char * string)
{
    size_t stringLength = strlen(string);

    for(size_t i = stringLength; i > 0; i--) {
        ungetc((unsigned char)string[i - 1], stdin);
    }
}

/* Gives a test a clean slate: no pending pushback, no sticky EOF, no event override. */
static void resetState(void)
{
    unsetenv("GETCH_EVENT_DEVICE");

    if(freopen("/dev/null", "r", stdin) == NULL) {
        perror("freopen");
        exit(EXIT_FAILURE);
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
    int pushed = _ungetch('q');
    int got = _getch();

    ASSERT_EQUAL(got, pushed);
    ASSERT_EQUAL(113, pushed);
    ASSERT_EQUAL(113, got);
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

CTEST(cinPeekTests, EmptyStdinReturnsEof)
{
    resetState();

    ASSERT_EQUAL(EOF, cinPeek());
}

CTEST(cinPeekTests, PeekDoesNotConsume)
{
    resetState();

    writeStringToStdin("xy");

    ASSERT_EQUAL('x', cinPeek());
    ASSERT_EQUAL('x', cinPeek());
    ASSERT_EQUAL('x', getchar());
    ASSERT_EQUAL('y', getchar());
}

CTEST(cinPeekTests, PeekCountPreservesOrder)
{
    resetState();

    writeStringToStdin("abc");

    size_t length = 0;
    int *peeked = cinPeekCount(3, &length);

    ASSERT_NOT_NULL(peeked);
    ASSERT_EQUAL(3, (int)length);
    ASSERT_EQUAL('a', peeked[0]);
    ASSERT_EQUAL('b', peeked[1]);
    ASSERT_EQUAL('c', peeked[2]);
    free(peeked);

    ASSERT_EQUAL('a', getchar());
    ASSERT_EQUAL('b', getchar());
    ASSERT_EQUAL('c', getchar());
}

CTEST(cinPeekTests, PeekCountZeroOrEmptyReturnsNull)
{
    resetState();

    size_t length = 99;

    ASSERT_NULL(cinPeekCount(0, &length));
    ASSERT_EQUAL(0, (int)length);
    ASSERT_NULL(cinPeekCount(4, &length));
    ASSERT_EQUAL(0, (int)length);
}

/* Reads from fd into out until needle appears or timeoutMs passes; returns 1 if found. */
static int readUntil(int fd, char * out, size_t size, const char * needle, int timeoutMs)
{
    size_t used = strlen(out);

    while(strstr(out, needle) == NULL && used < size - 1) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };

        if(poll(&pfd, 1, timeoutMs) <= 0) {
            break;
        }

        ssize_t n = read(fd, out + used, size - 1 - used);

        if(n <= 0) {
            break;
        }

        used += (size_t)n;
        out[used] = '\0';
    }

    return strstr(out, needle) != NULL;
}

/* Runs cinPeek() in a child attached to a real pty, like a user at a terminal. */
CTEST(cinPeekTests, PeekReturnsKeyWithoutEnter)
{
    int master;
    pid_t pid = forkpty(&master, NULL, NULL, NULL);

    ASSERT_TRUE(pid != -1);

    if(pid == 0) {
        // Fresh stdio on the pty; the test harness replaced stdin with /dev/null.
        stdin = fdopen(STDIN_FILENO, "r");

        dprintf(STDOUT_FILENO, "ready\n");

        int peeked = cinPeek();
        int again = cinPeek();
        int got = _getch();

        dprintf(STDOUT_FILENO, "result=%d,%d,%d\n", peeked, again, got);
        _exit(0);
    }

    char out[256] = "";

    ASSERT_TRUE(readUntil(master, out, sizeof(out), "ready", 5000));

    // No key yet: cinPeek must still be blocking, like cin.peek().
    ASSERT_FALSE(readUntil(master, out, sizeof(out), "result=", 300));

    // One key and no newline: canonical mode would hold it back until Enter.
    ASSERT_EQUAL(1, (int)write(master, "x", 1));

    int found = readUntil(master, out, sizeof(out), "result=120,120,120", 5000);

    if(!found) {
        kill(pid, SIGKILL);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    close(master);

    ASSERT_TRUE(found);
    ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    // Raw mode turns echo off, so the key must not be echoed back.
    ASSERT_NULL(strchr(out, 'x'));
}

/* ---- pty harness ------------------------------------------------------- */

typedef struct {
    const char * send;
    size_t sendLen;
    const char * expect;
} PtyStep;

typedef struct {
    char out[512];
    int ok;
    int exitedClean;
} PtyResult;

/* Time for the child to get from "ready" into its blocking read in raw mode. Keys
 * written earlier would be echoed and line-buffered by the pty like a typed-ahead key. */
#define PTY_SETTLE_MS 150

static void sleepMs(int ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* Runs child() attached to a real pty. For each step: wait, write the bytes, then wait
 * for the expected text in the child's output. A hung child is killed after 5 seconds. */
static PtyResult runInPty(void (*child)(void), const PtyStep * steps, size_t stepCount)
{
    PtyResult result = { .ok = 0, .exitedClean = 0 };
    int master;
    pid_t pid = forkpty(&master, NULL, NULL, NULL);

    if(pid == -1) {
        perror("forkpty");
        return result;
    }

    if(pid == 0) {
        // Fresh stdio on the pty; the test harness replaced stdin with /dev/null.
        stdin = fdopen(STDIN_FILENO, "r");
        dprintf(STDOUT_FILENO, "ready\n");
        child();
        _exit(0);
    }

    result.ok = readUntil(master, result.out, sizeof(result.out), "ready", 5000);

    for(size_t i = 0; result.ok && i < stepCount; i++) {
        sleepMs(PTY_SETTLE_MS);

        if(write(master, steps[i].send, steps[i].sendLen) != (ssize_t)steps[i].sendLen) {
            result.ok = 0;
        } else {
            result.ok = readUntil(master, result.out, sizeof(result.out), steps[i].expect, 5000);
        }
    }

    if(!result.ok) {
        kill(pid, SIGKILL);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    close(master);

    result.exitedClean = WIFEXITED(status) && WEXITSTATUS(status) == 0;

    return result;
}

#define ONE_STEP(bytes, expected) \
    { .send = (bytes), .sendLen = sizeof(bytes) - 1, .expect = (expected) }

/* ---- A. _getch contract on a pty --------------------------------------- */

static void childGetchOnce(void)
{
    dprintf(STDOUT_FILENO, "result=%d\n", _getch());
}

CTEST(getchPtyTests, GetchReturnsKeyWithoutEnter)
{
    PtyStep steps[] = { ONE_STEP("x", "result=120") };
    PtyResult r = runInPty(childGetchOnce, steps, 1);

    ASSERT_TRUE(r.ok);
    ASSERT_TRUE(r.exitedClean);
}

CTEST(getchPtyTests, GetchDoesNotEcho)
{
    PtyStep steps[] = { ONE_STEP("x", "result=120") };
    PtyResult r = runInPty(childGetchOnce, steps, 1);

    ASSERT_TRUE(r.ok);
    ASSERT_NULL(strchr(r.out, 'x'));
}

static void childGetchTwice(void)
{
    int first = _getch();
    dprintf(STDOUT_FILENO, "first=%d\n", first);

    int second = _getch();
    dprintf(STDOUT_FILENO, "second=%d\n", second);
}

/* The README promises _getch reads control characters; they must not raise SIGINT or EOF. */
CTEST(getchPtyTests, GetchReadsControlChars)
{
    PtyStep steps[] = {
        ONE_STEP("\x03", "first=3"),
        ONE_STEP("\x04", "second=4"),
    };
    PtyResult r = runInPty(childGetchTwice, steps, 2);

    ASSERT_TRUE(r.ok);
    ASSERT_TRUE(r.exitedClean);
}

static int sameTerminalMode(const struct termios * a, const struct termios * b)
{
    return a->c_iflag == b->c_iflag && a->c_oflag == b->c_oflag
        && a->c_cflag == b->c_cflag && a->c_lflag == b->c_lflag;
}

static void childModeRestored(void)
{
    struct termios before, after;
    tcgetattr(STDIN_FILENO, &before);

    _getch();
    tcgetattr(STDIN_FILENO, &after);
    dprintf(STDOUT_FILENO, "getch=%s\n", sameTerminalMode(&before, &after) ? "restored" : "changed");

    cinPeek();
    tcgetattr(STDIN_FILENO, &after);
    dprintf(STDOUT_FILENO, "peek=%s\n", sameTerminalMode(&before, &after) ? "restored" : "changed");
}

CTEST(getchPtyTests, TerminalModeRestored)
{
    PtyStep steps[] = {
        ONE_STEP("a", "getch=restored"),
        ONE_STEP("b", "peek=restored"),
    };
    PtyResult r = runInPty(childModeRestored, steps, 2);

    ASSERT_TRUE(r.ok);
    ASSERT_NULL(strstr(r.out, "changed"));
}

/* Regression for registering atexit and re-saving termios on every call. */
static void childManyCalls(void)
{
    struct termios before, after;
    tcgetattr(STDIN_FILENO, &before);

    int allMatched = 1;

    for(int i = 0; i < 200; i++) {
        _ungetch('k');
        allMatched &= (_getch() == 'k');
    }

    tcgetattr(STDIN_FILENO, &after);
    dprintf(STDOUT_FILENO, "result=%s\n",
            allMatched && sameTerminalMode(&before, &after) ? "ok" : "bad");
}

CTEST(getchPtyTests, ManyCallsKeepTerminalIntact)
{
    PtyStep none[] = { { .send = "", .sendLen = 0, .expect = "result=" } };
    PtyResult r = runInPty(childManyCalls, none, 1);

    ASSERT_TRUE(r.ok);
    ASSERT_NOT_NULL(strstr(r.out, "result=ok"));
    ASSERT_TRUE(r.exitedClean);
}

static void childTwoGetch(void)
{
    int first = _getch();
    int second = _getch();
    dprintf(STDOUT_FILENO, "result=%d,%d\n", first, second);
}

/* Escape sequences arriving over a pty, with the scan code supplied by the event file. */
CTEST(getchPtyTests, LoneEscapeReturns27)
{
    resetState();
    writeToEventDevice(KEY_ESC);

    PtyStep steps[] = { ONE_STEP("\x1b", "result=27") };
    PtyResult r = runInPty(childGetchOnce, steps, 1);

    ASSERT_TRUE(r.ok);
    ASSERT_TRUE(r.exitedClean);
}

CTEST(getchPtyTests, F1ReturnsZeroThenScanCode)
{
    resetState();
    writeToEventDevice(KEY_F1);

    char expected[32];
    snprintf(expected, sizeof(expected), "result=0,%d", KEY_F1);

    PtyStep steps[] = { ONE_STEP("\x1bOP", expected) };
    steps[0].expect = expected;
    PtyResult r = runInPty(childTwoGetch, steps, 1);

    ASSERT_TRUE(r.ok);
}

CTEST(getchPtyTests, HomeReturns224ThenKeypad7)
{
    resetState();
    writeToEventDevice(KEY_HOME);

    char expected[32];
    snprintf(expected, sizeof(expected), "result=224,%d", KEY_KP7);

    PtyStep steps[] = { ONE_STEP("\x1b[H", expected) };
    steps[0].expect = expected;
    PtyResult r = runInPty(childTwoGetch, steps, 1);

    ASSERT_TRUE(r.ok);
}

/* ---- B. cinPeek / cinPeekCount edge cases ------------------------------ */

static void childPeekThenGetch(void)
{
    size_t length = 0;
    int * peeked = cinPeekCount(2, &length);
    int a = _getch();
    int b = _getch();
    int c = _getch();

    dprintf(STDOUT_FILENO, "result=%zu,%d,%d,%d,%d,%d\n",
            length, peeked ? peeked[0] : -1, peeked ? peeked[1] : -1, a, b, c);
}

CTEST(cinPeekTests, PeekCountThenGetchSeeSameBytes)
{
    PtyStep steps[] = { ONE_STEP("abc", "result=2,97,98,97,98,99") };
    PtyResult r = runInPty(childPeekThenGetch, steps, 1);

    ASSERT_TRUE(r.ok);
    ASSERT_TRUE(r.exitedClean);
}

static void childPeekCountClamps(void)
{
    size_t length = 0;
    int * peeked = cinPeekCount(8, &length);

    dprintf(STDOUT_FILENO, "result=%zu\n", peeked ? length : 0);
}

/* Asking for more than is available must return what is there, not wait for the rest. */
CTEST(cinPeekTests, PeekCountReturnsWhatIsAvailable)
{
    PtyStep steps[] = { ONE_STEP("abc", "result=3") };
    PtyResult r = runInPty(childPeekCountClamps, steps, 1);

    ASSERT_TRUE(r.ok);
    ASSERT_TRUE(r.exitedClean);
}

CTEST(cinPeekTests, PeekCountHighBytesAreNotNegative)
{
    resetState();
    writeStringToStdin("\x80\xff");

    size_t length = 0;
    int * peeked = cinPeekCount(2, &length);

    ASSERT_NOT_NULL(peeked);
    ASSERT_EQUAL(2, (int)length);
    ASSERT_EQUAL(128, peeked[0]);
    ASSERT_EQUAL(255, peeked[1]);
    free(peeked);

    ASSERT_EQUAL(128, getchar());
    ASSERT_EQUAL(255, getchar());
}

CTEST(cinPeekTests, PeekKeepsRealEof)
{
    resetState();

    ASSERT_EQUAL(EOF, cinPeek());
    ASSERT_TRUE(feof(stdin) != 0);
}

CTEST(cinPeekTests, PeekLeavesDescriptorFlagsAlone)
{
    resetState();
    writeStringToStdin("ab");

    int before = fcntl(STDIN_FILENO, F_GETFL);

    cinPeek();
    int * peeked = cinPeekCount(2, NULL);
    free(peeked);

    ASSERT_EQUAL(before, fcntl(STDIN_FILENO, F_GETFL));
}

CTEST(cinPeekTests, PeekCountAcceptsNullLength)
{
    resetState();
    writeStringToStdin("a");

    int * peeked = cinPeekCount(1, NULL);

    ASSERT_NOT_NULL(peeked);
    ASSERT_EQUAL('a', peeked[0]);
    free(peeked);
}

/* ---- C. event-device override and scan-code parsing -------------------- */

CTEST(eventDeviceTests, ShortEventFileWithoutKeyReturnsReserved)
{
    resetState();

    // One event, and it is not a key: nothing else in the file may be read as one.
    struct input_event onlyScan[1] = {
        { .type = EV_MSC, .code = MSC_SCAN, .value = KEY_F1 },
    };
    writeEventFile(onlyScan, 1);
    writeStringToStdin("\x1b");

    ASSERT_EQUAL(0, _getch());
    ASSERT_EQUAL(KEY_RESERVED, _getch());
}

CTEST(eventDeviceTests, SingleKeyEventIsEnough)
{
    resetState();

    struct input_event onlyKey[1] = {
        { .type = EV_KEY, .code = KEY_F1, .value = EV_PRESSED },
    };
    writeEventFile(onlyKey, 1);
    writeStringToStdin("\x1bOP");

    ASSERT_EQUAL(0, _getch());
    ASSERT_EQUAL(KEY_F1, _getch());
}

CTEST(eventDeviceTests, MissingEventFileDoesNotHang)
{
    resetState();
    setenv("GETCH_EVENT_DEVICE", "/nonexistent/getch-event-device", 1);
    writeStringToStdin("\x1b");

    ASSERT_EQUAL(0, _getch());
    ASSERT_EQUAL(KEY_RESERVED, _getch());
}

typedef struct {
    __u16 key;
    const char * sequence;
    int prefix;
    int code;
} ScanCase;

static const ScanCase scanCases[] = {
    { KEY_UP,       "\x1b[A",    224, KEY_KP8 },
    { KEY_DOWN,     "\x1b[B",    224, KEY_KP2 },
    { KEY_RIGHT,    "\x1b[C",    224, KEY_KP6 },
    { KEY_LEFT,     "\x1b[D",    224, KEY_KP4 },
    { KEY_HOME,     "\x1b[H",    224, KEY_KP7 },
    { KEY_END,      "\x1b[F",    224, KEY_KP1 },
    { KEY_INSERT,   "\x1b[2~",   224, KEY_KP0 },
    { KEY_DELETE,   "\x1b[3~",   224, KEY_KPDOT },
    { KEY_PAGEUP,   "\x1b[5~",   224, KEY_KP9 },
    { KEY_PAGEDOWN, "\x1b[6~",   224, KEY_KP3 },
    { KEY_F1,       "\x1bOP",    0,   KEY_F1 },
    { KEY_F2,       "\x1bOQ",    0,   KEY_F2 },
    { KEY_F3,       "\x1bOR",    0,   KEY_F3 },
    { KEY_F4,       "\x1bOS",    0,   KEY_F4 },
    { KEY_F5,       "\x1b[15~",  0,   KEY_F5 },
    { KEY_F6,       "\x1b[17~",  0,   KEY_F6 },
    { KEY_F7,       "\x1b[18~",  0,   KEY_F7 },
    { KEY_F8,       "\x1b[19~",  0,   KEY_F8 },
    { KEY_F9,       "\x1b[20~",  0,   KEY_F9 },
    { KEY_F10,      "\x1b[21~",  0,   KEY_F10 },
    { KEY_F11,      "\x1b[23~",  0,   KEY_F11 },
    { KEY_F12,      "\x1b[24~",  0,   KEY_F12 },
};

/* Every special key: the two _getch results, and that the whole sequence was consumed. */
CTEST(eventDeviceTests, ScanCodeTable)
{
    for(size_t i = 0; i < sizeof(scanCases) / sizeof(scanCases[0]); i++) {
        resetState();
        writeToEventDevice(scanCases[i].key);
        writeStringToStdin(scanCases[i].sequence);

        int prefix = _getch();
        int code = _getch();
        int leftover = cinPeek();

        if(prefix != scanCases[i].prefix || code != scanCases[i].code || leftover != EOF) {
            fprintf(stderr, "scan case %zu (key %d): got %d,%d leftover %d, want %d,%d\n",
                    i, scanCases[i].key, prefix, code, leftover,
                    scanCases[i].prefix, scanCases[i].code);
            ASSERT_FAIL();
        }
    }
}
