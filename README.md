# Getch for linux

This is an attempt at replicating the non standard _getch function for windows from conio.h on linux

It should work mostly the same as the windows function.

For special keys you need to call _getch twice.  The first time it will return either a 0 or 0xE0 (224) depending on the key pressed.

See https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/getch-getwch?view=msvc-160#remarks

Difference is that _getch will read CTRL+C, or any control characters.

There is also an extra function `int cinPeek()` that returns the next character in stdin without removing it.

### Note
Make sure that the user calling the function is in the group "input"

Only tested on Gentoo with XTerm and /dev/tty?.


