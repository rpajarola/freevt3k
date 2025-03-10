/* Copyright (C) 1996 Office Products Technology, Inc.

This file is part of FreeVT3k.

FreeVT3k is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

FreeVT3k is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with FreeVT3k. If not, see <https://www.gnu.org/licenses/>.
*/

/************************************************************
 * freevt3k.h -- External and connection structure definitions.
 ************************************************************/

#ifndef _FREEVT3K_H
#define _FREEVT3K_H

#define VERSION_ID "1.0"

#include "typedef.h"

#define LOG_INPUT		(0x01)
#define LOG_OUTPUT		(0x02)
#define LOG_PREFIX              (0x04)

extern int debug;

int PutImmediateQ(char ch);
void vt3kDataOutProc(int32_t refCon, char * buffer, size_t bufferLength);

#endif

