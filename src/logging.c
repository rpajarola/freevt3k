/*
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
 * logging.c -- logging functions
 ************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "logging.h"
#include "hpterm.h"

FILE
    *logFd = NULL;
int
    log_mask = 0;
FILE
	*debug_fd = NULL;

static char *asc_logvalue[] =
{
  "<nul>", "<soh>", "<stx>", "<etx>", "<eot>", "<enq>", "<ack>",
  "<bel>", "<bs>", "<ht>", "<lf>", "<vt>", "<ff>", "<cr>",
  "<so>", "<si>", "<dle>", "<dc1>", "<dc2>", "<dc3>", "<dc4>",
  "<nak>", "<syn>", "<etb>", "<can>", "<em>", "<sub>", "<esc>",
  "<fs>", "<gs>", "<rs>", "<us>", "<del>"
};

void DumpBuffer(void *buf, long buf_len, char *dump_id)
{ /*DumpBuffer*/

#define CHAR_PER_LINE		(16)
    int
	printOffset = 0,
	offset = 0,
	nChars = CHAR_PER_LINE,
	iLine,
	iChar;
    long
	nLines;
    unsigned char
	*charPtr,
	*ptr,
	*msgPtr,
	msg[81];
    extern int
	debug_need_crlf;

    if (debug_fd == (FILE*)NULL)
	{
	debug_fd = fopen("freevt3k.debug", "w");
	if (debug_fd == (FILE*)NULL)
	    debug_fd = stderr;
	}

    if (debug_need_crlf)
	{
	debug_need_crlf = 0;
	fprintf(debug_fd, "\n");
	}
    fprintf(debug_fd, "[ %s ]\n", dump_id);
    if (buf_len < 0)
	{
	buf_len = -buf_len;
	printOffset = 1;
	}
    nLines = buf_len / CHAR_PER_LINE;
    charPtr = (unsigned char*)buf;
    for (iLine = 0; iLine <= nLines; iLine++)
	{
	if (iLine == nLines)
	    nChars = buf_len % CHAR_PER_LINE;
	memset((void*)msg, ' ', 80);
	ptr = &msg[(CHAR_PER_LINE * 3) + 2];
	msgPtr = msg;
	for (iChar = 0; iChar < nChars; iChar++)
	    {
	    sprintf((char*)msgPtr, "%02x ", *charPtr);
	    msgPtr += 3;
	    *msgPtr = ' ';
	    *ptr = (unsigned char)
		((isprint(*charPtr) && isascii(*charPtr)) ? *charPtr : '#');
	    ++charPtr;
	    *(++ptr) = '\0';
	    }
	if (nChars > 0)
	    {
	    if (printOffset)
		{
		fprintf(debug_fd, "%04X: ", offset);
		offset += CHAR_PER_LINE;
		}
	    fprintf(debug_fd, "%s\n", msg);
	    }
	}
    fflush(debug_fd);
    
} /*DumpBuffer*/

int ParseLogMask(char *optarg) {
  int log_mask = 0;
  while (*optarg) {
    switch (*optarg) {
    case 'i':
      log_mask |= LOG_INPUT;
      break;
    case 'o':
      log_mask |= LOG_OUTPUT;
      break;
    case 'p':
      log_mask |= LOG_PREFIX;
      break;
    case 0:
      break;
    default:
      return -1;
    }
    optarg++;
  }
  return log_mask;
}

int LogOpen (char *log_file, int log_mask_)
{ /*LogOpen*/
  log_mask = log_mask_;
  if (!log_file) {
    if (log_mask != 0) {
      logFd = stdout;
    }
    return 0;
  }

  if ((logFd = fopen(log_file, "w")) == (FILE*)NULL)
  {
    perror("fopen");
    return 1;
  }
  return 0;
} /*LogOpen*/

void Logit (int log_type, char *ptr, size_t len, bool special_dc1)
{ /*Logit*/
  if (logFd == NULL)
    return;
  if (!(log_mask & log_type)) {
    return;
  }
  if (log_mask & LOG_PREFIX) {
    switch (log_type) {
    case LOG_INPUT:
      fprintf (logFd, "in:  ");
      break;
    case LOG_OUTPUT:
      fprintf (logFd, "out: ");
      break;
    default:
      fprintf (logFd, "???: ");
    }
  }

  while (len--)
    {
      if (((int) *ptr < 32) || ((int) *ptr == 127))
	{
	  int index = (int) *ptr;
	  if (index == 127)
	    index = 33;
	  fprintf (logFd, "%s", asc_logvalue[index]);
	  if (index == ASC_LF)
	    putc ('\n', logFd);
	}
      else
	putc ((int)*ptr, logFd);

      if (special_dc1 && (*ptr == ASC_DC1))	/* Ugh */
	putc ('\n', logFd);

      ++ptr;
    }

  putc ('\n', logFd);

  fflush (logFd);

} /* Logit */

int IsLogging() {
  return log_mask;
}
