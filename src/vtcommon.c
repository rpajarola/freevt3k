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
 * freevt3k.c -- text mode hp3000 terminal emulator
 ************************************************************/

#include "config.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#ifdef HAVE_TERMIOS_H
# include <termios.h>
typedef struct termios TERMIO, *PTERMIO;
#else
# include <termio.h>
typedef struct termio TERMIO, *PTERMIO;
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "vt.h"
#include "freevt3k.h"
#include "hpterm.h"
#include "hpvt100.h"
#include "vtconn.h"
#include "logging.h"
#include "timers.h"
#include "kbdtable.h"

/* Global variables */

#define DFLT_BREAK_MAX		(3)
#define DFLT_BREAK_TIMER	(1)

bool done = false;
bool stop_at_eof = false;

/* Current line awaiting send to satisfy an FREAD request */
#define MAX_INPUT_REC		(kVT_MAX_BUFFER)
char
	input_rec[MAX_INPUT_REC];
int input_rec_len = 0;

/* Circular input queue parms */
#define MAX_INPUT_QUEUE		(kVT_MAX_BUFFER)
char
	input_queue[MAX_INPUT_QUEUE],
	*inq_rptr = input_queue,
	*inq_wptr = input_queue;
int
	input_queue_len = 0;

/* Immediate queue for things like status requests, etc. */
#define MAX_IMM_INPUT_QUEUE	(256)
char
	imm_input_queue[MAX_IMM_INPUT_QUEUE],
	*imm_inq_rptr = imm_input_queue,
	*imm_inq_wptr = imm_input_queue;
int
	imm_input_queue_len = 0;

/* Miscellaneous stuff */
bool
	translate = false;

void FlushQ(void)
{ /*FlushQ*/

    input_queue_len = 0;
    inq_rptr = inq_wptr = input_queue;
    imm_input_queue_len = 0;
    imm_inq_rptr = imm_inq_wptr = imm_input_queue;

} /*FlushQ*/

int GetQ(void)
{ /*GetQ*/

/*
 * Get a byte from the immediate queue if one's present, else
 *   get it from the normal circular queue.
 */
  if (imm_input_queue_len)
    {
      if (++imm_inq_rptr == &imm_input_queue[MAX_IMM_INPUT_QUEUE])
	imm_inq_rptr = imm_input_queue;
      --imm_input_queue_len;
      return(*imm_inq_rptr);
    }
  if (input_queue_len)
    {
      if (++inq_rptr == &input_queue[MAX_INPUT_QUEUE])
	inq_rptr = input_queue;
      --input_queue_len;
      return(*inq_rptr);
    }
  return(-1);
    
} /*GetQ*/

int PutQ(char ch)
{ /*PutQ*/

  if (++inq_wptr == &input_queue[MAX_INPUT_QUEUE])
    inq_wptr = input_queue;
  if (inq_wptr == inq_rptr)
    {
      fprintf(stderr, "<queue overflow>\n");
      return(-1);
    }
  ++input_queue_len;
  *inq_wptr = ch;
  return(0);

} /*PutQ*/

int PutImmediateQ(char ch)
{ /*PutImmediateQ*/

  if (++imm_inq_wptr == &imm_input_queue[MAX_IMM_INPUT_QUEUE])
    imm_inq_wptr = imm_input_queue;
  if (imm_inq_wptr == imm_inq_rptr)
    {
      fprintf(stderr, "<immediate queue overflow>\n");
      return(-1);
    }
  ++imm_input_queue_len;
  *imm_inq_wptr = ch;
  return(0);

} /*PutImmediateQ*/

bool AltEol(tVTConnection *conn, char ch)
{ /*AltEol*/

/*
 * 961126: Don't check for alt eol if in unedited mode
 */
  if ((conn->fUneditedMode) || (conn->fBinaryMode))
    return(false);
  if ((conn->fAltLineTerminationChar) &&
      (conn->fAltLineTerminationChar !=
       conn->fLineTerminationChar) &&
      (ch == conn->fAltLineTerminationChar))
    return(true);
  return(false);

} /*AltEol*/

bool PrimEol(tVTConnection *conn, char ch)
{ /*PrimEol*/

/*
 * 961126: Don't check for prim eol if in binary mode
 */
  if (conn->fBinaryMode)
    return(false);
  if (ch == conn->fLineTerminationChar)
    return(true);
  return(false);

} /*PrimEol*/

int ProcessQueueToHost(tVTConnection *conn, ssize_t len)
{/*ProcessQueueToHost*/

/*
#define TRANSLATE_INPUT	(1)
 */

  static char
    cr = '\r',
    lf = '\n';
  char
    ch;
  bool
    vt_fkey = false,
    alt = false,
    prim = false;
  int
    int_ch = 0,
    whichError = 0,
    send_index = -1,
    comp_mask = kVTIOCSuccessful;

  if (len == -2)
    { /* Break - flush all queues */
      if (conn->fSysBreakEnabled)
	{
	  send_index = kDTCSystemBreakIndex;
	  FlushQ();
	}
    }
  else if (len == -1)
    comp_mask = kVTIOCTimeout;
  else if (len >= 0)
    {
      for (;;)
	{
	  if ((int_ch = GetQ()) == -1)
	    {
	      if (stop_at_eof)
		done = true;
	      return(0);	/* Ran out of characters */
	    }
	  ch = (char)int_ch;
	  if ((!(conn->fUneditedMode)) && (!(conn->fBinaryMode)))
	    {
	      if ((ch == conn->fCharDeleteChar) ||
		  (ch == (char)127))
		{
		  if (input_rec_len)
		    {
		      char	bs_buf[8];
		      int	bs_len = 0;
		      --input_rec_len;
		      switch (conn->fCharDeleteEcho)
			{
			case kAMEchoBackspace:
			  bs_buf[0] = ASC_BS;
			  bs_len = 1;
			  break;
			case kAMEchoBSSlash:
			  bs_buf[0] = '\\';
			  bs_buf[1] = ASC_LF;
			  bs_len = 2;
			  break;
			case kAMEchoBsSpBs:
			  bs_buf[0] = ASC_BS;
			  bs_buf[1] = ' ';
			  bs_buf[2] = ASC_BS;
			  bs_len = 3;
			  break;
			default:bs_len = 0;
			}
		      if ((bs_len) && (conn->fEchoControl != 1))
			conn->fDataOutProc(conn->fDataOutRefCon,
					   bs_buf, bs_len);
		    }
		  continue;
		}
	      if (ch == conn->fLineDeleteChar)
		{
		  input_rec_len = 0;
/* Don't echo if line delete echo disabled */
		  if (conn->fDisableLineDeleteEcho)
		    continue;
		  conn->fDataOutProc(conn->fDataOutRefCon,
				     conn->fLineDeleteEcho,
				     conn->fLineDeleteEchoLength);
		  continue;
		}
	    }
	  if (conn->fDriverMode == kDTCBlockMode)
	    {
	      if ((!input_rec_len) && (ch == ASC_DC2))
		{
		  input_rec[0] = ASC_ESC;
		  input_rec[1] = 'h';
		  input_rec[2] = ASC_ESC;
		  input_rec[3] = 'c';
		  input_rec[4] = ASC_DC1;
		  conn->fDataOutProc(conn->fDataOutRefCon,
				     (void*)input_rec, 5);
		  while (GetQ() != -1);
		  return(0);
		}
	    }
	  input_rec[input_rec_len++] = ch;
	  if ((conn->fEchoControl != 1) &&
	      (conn->fDriverMode == kDTCVanilla))
	    {
	      char ch1 = ch;
	      if (table_spec == 1)
		ch1 = in_table[((int)ch1) & 0x00FF];
	      conn->fDataOutProc(conn->fDataOutRefCon, (void*)&ch1, 1);
	    }
	  if ((conn->fSubsysBreakEnabled) &&
/*
 * 961126: Don't check for ctl-y if in binary mode
 */
	      (!(conn->fBinaryMode)) &&
	      (ch == conn->fSubsysBreakChar))
	    send_index = kDTCCntlYIndex;
#ifdef TRANSLATE_INPUT
	  if ((translate) && (ch == '~') && (input_rec[0] == ASC_ESC))
	    vt_fkey = true;
	  else
#endif
	  if (conn->fDriverMode != kDTCBlockMode)
	    prim = PrimEol(conn, ch);
	  alt = AltEol(conn, ch);
/*
	    if (debug)
		{
		extern FILE *debug_fd;
		fprintf(debug_fd,
			"ch=%02x, alt=%d, prim=%d, mode=%d, UneditedMode=%d, LTC=%02x, ALTC=%02x\n",
			ch, alt, prim,
			conn->fDriverMode,
			(int) conn->fUneditedMode,
			conn->fLineTerminationChar,
			conn->fAltLineTerminationChar);
		debug_need_crlf = 0;
		}
 */
	  if ((send_index == kDTCCntlYIndex) ||
	      (input_rec_len >= conn->fReadLength) ||
	      (prim) || (alt) || (vt_fkey))
	    {
	      if (send_index == kDTCCntlYIndex)
		--input_rec_len;
	      else
		{
		  if (alt)
		    {
		      comp_mask = kVTIOCBreakRead;
		      if ((conn->fEchoControl != 1) &&
			  (conn->fDriverMode == kDTCVanilla))
			conn->fDataOutProc(conn->fDataOutRefCon,
					   (void*)&cr, 1);
		    }
		  else if (input_rec_len <= conn->fReadLength)
		    {
		      if (prim)
			--input_rec_len;
		    }
		  if ((conn->fEchoCRLFOnCR) &&
		      (conn->fDriverMode == kDTCVanilla) &&
		      (!(conn->fBinaryMode)))
		    {
		      if (!prim) /* Echo cr if read didn't include one */
			conn->fDataOutProc(conn->fDataOutRefCon,
					   (void*)&cr, 1);
		      conn->fDataOutProc(conn->fDataOutRefCon,
					 (void*)&lf, 1);
		    }
		}

		Logit (LOG_INPUT, input_rec, input_rec_len, false);
		    
	      break;
	    }
	}
    }

  if (send_index == -1)
    {
#ifdef TRANSLATE_INPUT
      if (translate)
	TranslateKeyboard(input_rec, &input_rec_len);
#endif
/*
 * Do input translation here
 */
      if (table_spec == 1)
	for (send_index=0; send_index<input_rec_len; send_index++)
	  input_rec[send_index] = in_table[((int)input_rec[send_index]) & 0x00FF];
      whichError = VTSendData(conn, input_rec, input_rec_len, comp_mask);
    }
  else
    whichError = VTSendBreak(conn, send_index);
  if (whichError)
    {
      char	messageBuffer[128];
      VTErrorMessage(conn, whichError,
		     messageBuffer, sizeof(messageBuffer));
      fprintf(stderr, "Unable to send to host:\n%s\n", messageBuffer);
      return(-1);
    }

  conn->fReadInProgress = false;
  input_rec_len = 0;
  return(0);

}/*ProcessQueueToHost*/

void vt3kDataOutProc(int32_t refCon, char * buffer, size_t bufferLength)
{ /*vt3kDataOutProc*/

  Logit (LOG_OUTPUT, buffer, bufferLength, true);

  if (write(STDOUT_FILENO, buffer, bufferLength)) {}
} /*vt3kDataOutProc*/
