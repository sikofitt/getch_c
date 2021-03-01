/***********************************************************************
 * This Source Code Form is subject to the terms of the Mozilla Public *
 * License, v. 2.0. If a copy of the MPL was not distributed with this *
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.            *
 ***********************************************************************/
  
#ifndef GETCH_H
#define GETCH_H

int _getch(void);
int getch(void);
int _ungetch(int ch);
int ungetch(int ch);
int cinPeek();

#endif