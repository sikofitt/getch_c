/***********************************************************************
 * This Source Code Form is subject to the terms of the Mozilla Public *
 * License, v. 2.0. If a copy of the MPL was not distributed with this *
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.            *
 ***********************************************************************/
  
#ifndef GETCH_H
#define GETCH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int _getch(void);
int _ungetch(int ch);

/* Non-underscore aliases; define GETCH_ENABLE_ALIASES to enable them.
 * They are off by default because they collide with ncurses and conio. */
#ifdef GETCH_ENABLE_ALIASES
int getch(void);
int ungetch(int ch);
#endif

/* Like std::cin.peek(): blocks until a key is available (no Enter needed), returns it
 * without removing it from stdin, or EOF at end of input. */
int cinPeek(void);

/* Blocks for the first byte, then also peeks whatever else is already available, up to
 * count bytes. Returns a malloc'd array (caller frees) and sets *length. Returns NULL at
 * end of input, or with errno == ENOBUFS if the C library could not push all the bytes
 * back onto stdin. */
int *cinPeekCount(unsigned short count, size_t *length);

#ifdef __cplusplus
}
#endif

#endif //GETCH_H
