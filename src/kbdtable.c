/* Copyright (C) 2025 Rico Pajarola

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
 * kbdtable.c -- keyboard translation tables
 ************************************************************/

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int table_spec = 0;
unsigned char
    in_table[256],
    out_table[256];

#ifdef DEBUG_TRANSLATE_TABLE
void DisplayHex(void *buf, int buf_len, char *dump_id)
{ /*DisplayHex*/
#define CHAR_PER_LINE		(16)
  int
    printOffset = 0,
    offset = 0,
    nChars = CHAR_PER_LINE,
    nLines,
    iLine,
    iChar;
  unsigned char
    *charPtr,
    *ptr,
    *msgPtr,
    msg[81];

  fprintf(stderr, "[ %s ]\n", dump_id);
  if (buf_len < 0) {
    buf_len = -buf_len;
    printOffset = 1;
  }
  nLines = buf_len / CHAR_PER_LINE;
  charPtr = (unsigned char*)buf;
  for (iLine = 0; iLine <= nLines; iLine++) {
    if (iLine == nLines)
      nChars = buf_len % CHAR_PER_LINE;
    memset((void*)msg, ' ', 80);
    ptr = &msg[(CHAR_PER_LINE * 3) + 2];
    msgPtr = msg;
    for (iChar = 0; iChar < nChars; iChar++) {
	  sprintf((char*)msgPtr, "%02x ", *charPtr);
	  msgPtr += 3;
	  *msgPtr = ' ';
	  *ptr = (unsigned char)((isprint(*charPtr) && isascii(*charPtr)) ? *charPtr : '#');
	  ++charPtr;
	  *(++ptr) = '\0';
	}
    if (nChars > 0) {
	  if (printOffset) {
	    fprintf(stderr, "%04X: ", offset);
	    offset += CHAR_PER_LINE;
      }
	  fprintf(stderr, "%s\n", msg);
	}
  }
} /*DisplayHex*/
#endif /*DEBUG_TRANSLATE_TABLE*/

int LoadKeybdTable(char *file_name, int i_type)
{ /*LoadKeybdTable*/
  char
    charSpec[256];
  int
    i = -1,
    idx = 0,
    fd = 0;

  memset(charSpec, 0, 256);
  if ((fd = open(file_name, O_RDONLY, 0)) == -1) {
    perror("open");
    return 1;
  }
  if (read(fd, in_table, 256) != 256) {
    perror("read");
    return 1;
  }
  close(fd);

  for (i=0; i<32; i++) {
    if (in_table[i] != (unsigned char)i) {
      fprintf(stderr, "Cannot change the first 32 values\n");
      return 1;
    }
  }

  if (i_type == 1) {
    for (i=0; i<256; i++) {
	  idx = ((int)in_table[i]) & 0x00FF;
	  if (charSpec[idx]) {
        fprintf(stderr, "Translate table contains duplicate entries\n");
	    return(1);
	  }
	  out_table[idx] = (unsigned char)i;
	  charSpec[idx] = 1;
	}
  } else {
    memcpy(out_table, in_table, 256);
  }

#ifdef DEBUG_TRANSLATE_TABLE
  DisplayHex(in_table, -256, "in");
  DisplayHex(out_table, -256, "out");
#endif
  table_spec = i_type;
  return 0;
} /*LoadKeybdTable*/
