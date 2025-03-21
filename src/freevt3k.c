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
#include "vtcommon.h"
#include "hpterm.h"
#include "hpvt100.h"
#include "vtconn.h"
#include "logging.h"
#include "timers.h"
#include "kbdtable.h"

/* Useful macros */

#ifndef MAX
#  define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* Global variables */

#define DFLT_BREAK_MAX		(3)
#define DFLT_BREAK_TIMER	(1)
int
	break_max = DFLT_BREAK_MAX,
	break_sigs = DFLT_BREAK_MAX,
	break_char = -1,
	break_timer = DFLT_BREAK_TIMER;
int
	stdin_fd = 0,
	stdin_tty = 0;
bool
	send_break = false;
bool
	type_ahead = false;
TERMIO
	old_termios;

/* Miscellaneous stuff */
bool
	generic = false,
	vt100 = false,
	vt52 = false,
	eight_none = false;
int
	term_type = 10;
bool
	disable_xon_xoff = false;
int32_t
	first_break_time = 0;

static void PrintUsage(int detail)
{ /*PrintUsage*/

  printf("freevt3k - version %s", VERSION_ID);
#if defined(__DATE__) && defined(__TIME__)
  printf(" (%s-%s)", __DATE__, __TIME__);
#endif
  printf("\n\n");
    
  printf("Usage: freevt3k [-li|-lo|-lio] [-f file] [-x] [-tt n] [-t]\n");
  printf("                [-C breakchar] ");
  printf("[-B count] [-T timer]\n");
  printf("                [-X file] [-a|-I file] [-d[d]] host\n");
  if (!detail)
    return;
  printf("   -li|-lo|-lio    - specify input|output logging options\n");
  printf("   -lp             - put a prefix on logging output\n");
  printf("   -f file         - destination for logging [stdout]\n");
  printf("   -x              - disable xon/xoff flow control\n");
  printf("   -tt n           - 'n'->10 (default) generates DC1 read triggers\n");
  printf("   -t              - enable type-ahead\n");
  printf("   -C breakchar    - use 'breakchar' (integer) as break trigger [BREAK or nul]\n");
  printf("   -B count        - change number of breaks for command mode [%d]\n",
	 DFLT_BREAK_MAX);
  printf("   -T timer        - change -B time interval in seconds [%d]\n",
	 DFLT_BREAK_TIMER);
  printf("   -vt100          - emulate hp2392 on vt100 terminals.\n");
  printf("   -vt52           - emulate hp2392 on vt52 terminals.\n");
  printf("   -generic        - translate hp escape sequences to tokens\n");
  printf("   -X file         - specify 256-byte translation table.\n");
  printf("   -a file         - read initial commands from file.\n");
  printf("   -I file         - like -a, but stops when end-of-file reached\n");
  printf("   -d[d]           - enable debug output to freevt3k.debug\n");
  printf("   host            - name/IP address of target HP 3000\n");

} /*PrintUsage*/

static int SetTtyAttributes(int fd, PTERMIO termio_buf)
{ /*SetTtyAttributes*/

#ifdef HAVE_TERMIOS_H
  if (tcsetattr(fd, TCSANOW, termio_buf) == -1)
    return(-1);
#else
  if (ioctl(fd, TCSETA, termio_buf) == -1)
    return(-1);
#endif
  return(0);

} /*SetTtyAttributes*/

static int GetTtyAttributes(int fd, PTERMIO termio_buf)
{ /*GetTtyAttributes*/

#ifdef HAVE_TERMIOS_H
  if (tcgetattr(fd, termio_buf) == -1)
    return(-1);
#else
  if (ioctl(fd, TCGETA, termio_buf) == -1)
    return(-1);
#endif
  return(0);

} /*GetTtyAttributes*/

void ProcessInterrupt(void)
{/*ProcessInterrupt*/
    
  TERMIO
    curr_termios,
    temp_termios;
  char
    ans[32];
    
  if (stdin_tty)
    {
      GetTtyAttributes(STDIN_FILENO, &curr_termios);
      temp_termios = old_termios;
      SetTtyAttributes(STDIN_FILENO, &temp_termios);
    }
  printf("\n");
  for (;;)
    {
      printf("Please enter FREEVT3K command (Exit or Continue) : ");
      if (fgets(ans, sizeof(ans), stdin) == NULL) continue;
      if (strlen(ans) == 0) continue;
      if (islower(*ans))
	*ans = (char)toupper(*ans);
      if (*ans == 'E')
	{
	  done = false;
	  printf("\r\nTerminating\r\n");
	  break;
	}
      if (*ans == 'C')
	{
	  break_sigs = break_max;
	  break;
	}
    }
  if (stdin_tty)
    SetTtyAttributes(STDIN_FILENO, &curr_termios);
} /*ProcessInterrupt*/

#ifdef USE_CTLC_INTERRUPTS
typedef void (*SigfuncInt)(int);

void CatchCtlC(int sig_type)
{ /*CatchCtlC*/

  SigfuncInt
    signalPtr;

#  ifdef BREAK_VIA_SIG
  if (!(--break_sigs))
    ProcessInterrupt();
#  endif
  signalPtr = (SigfuncInt)CatchCtlC;
  if (signal(SIGINT, signalPtr) == SIG_ERR)
    {
      perror("signal");
      exit(1);
    }

} /*CatchCtlC*/

void RestoreCtlC(void)
{ /*RestoreCtlC*/

  SigfuncInt
    signalPtr;

  signalPtr = (SigfuncInt)SIG_DFL;
  if (signal(SIGINT, signalPtr) == SIG_ERR)
    perror("signal");

} /*RestoreCtlC*/
#endif /* USE_CTLC_INTERRUPTS */

int ProcessSocket(tVTConnection * conn)
{/*ProcessSocket*/

  int
    whichError = 0;
  static char trigger[] = { ASC_DC1 };                                   /* JCM 040296 */
    
  whichError = VTReceiveDataReady(conn);
  if (whichError == kVTCVTOpen)
    {
/*
 * The connection is open now, so initialize for
 * terminal operations. This means setting up
 * the TTY for "raw" operation. (Or, it will
 * once we get that set up.)
 */
    }
  else if (whichError != kVTCNoError)
    {
      char	messageBuffer[128];
      if (whichError == kVTCStartShutdown)
	return(1);
      VTErrorMessage(conn, whichError,
		     messageBuffer, sizeof(messageBuffer));
      fprintf(stderr, "VT error:\r\n%s\r\n", messageBuffer);
      return(-1);
    }
  if (conn->fReadStarted)
    {
      conn->fReadStarted = false;
      if (conn->fReadFlush)	/* RM 960403 */
	{
	  conn->fReadFlush = false;
	  FlushQ();
	}
      if (term_type == 10)
	conn->fDataOutProc(conn->fDataOutRefCon,
			   trigger, sizeof(trigger));
/*
 * As we just got a read request, check for any typed-ahead data and
 *   process it now.
 */
      ProcessQueueToHost(conn, 0);
    }
  return(0);

}/*ProcessSocket*/

int ProcessTTY(tVTConnection * conn, char *buf, ssize_t len)
{/*ProcessTTY*/
  struct timeval
    timeout;
  ssize_t
    readCount = 1;
  fd_set
    readfds;
  if (len > 0)
    {
      if (debug > 1)
	{
	  fprintf(debug_fd, "read: ");
	  debug_need_crlf = 1;
	}
/*
 * Once we get the signal that at least one byte is ready, sit and read
 *   bytes from stdin until the select timer goes off after 10000 microsecs
 */
      for (;;)
	{
	  if (!readCount)
	    {
	      timeout.tv_sec = 0;
	      timeout.tv_usec = 10000;
	      FD_ZERO(&readfds);
	      FD_SET(stdin_fd, &readfds);
	      switch (select(stdin_fd+1, (void*)&readfds, NULL, NULL, (struct timeval *)&timeout))
		{
		case -1:	/* Error */
		  if (errno == EINTR)
		    {
		      errno = 0;
		      continue;
		    }
		  fprintf(stderr, "Error on select: %d.\n", errno);
		  return(-1);
		case 0:		/* Timeout */
		  readCount = -1;
		  if (debug > 1)
		    {
		      if (debug_need_crlf)
			{
			  fprintf(debug_fd, "\n");
			  debug_need_crlf = 0;
			}
		    }
		  break;
		default:
		  if (FD_ISSET(stdin_fd, &readfds))
		    {
		      if ((readCount = read(stdin_fd, buf, 1)) != 1)
			{
			  fprintf(stderr, "Error on read: %d.\n", errno);
			  return(-1);
			}
		    }
		}
	      if (readCount == -1)
		break;
	    }
#  ifndef BREAK_VIA_SIG
	  if (((break_char != -1) && (*buf == (char)break_char)) ||
	      ((break_char == -1) && (*buf == (conn->fSysBreakChar & 0xFF))))
	    { /* Break */
	      send_break = true;
/* Check for consecutive breaks - 'break_max'-in-a-row to get out */
	      if (debug > 1)
		{
		  if (debug_need_crlf)
		    fprintf(debug_fd, "\n");
		  fprintf(debug_fd, "break: ");
		  DEBUG_PRINT_CH(*buf);
		}
	      if (break_sigs == break_max)
		first_break_time = MyGettimeofday();
	      if (ElapsedTime(first_break_time) > break_timer)
		{
		  break_sigs = break_max;
		  first_break_time = MyGettimeofday();
		}
	      if (!(--break_sigs))
		ProcessInterrupt();
	      if (send_break)
		{
		  if (conn->fSysBreakEnabled)
		    ProcessQueueToHost(conn, -2);
		  send_break = false;
		}
	      readCount = 0;
	      continue;
	    }
#  endif
	  if (debug > 1)
	    DEBUG_PRINT_CH(*buf);
	  break_sigs = break_max;
	  if ((type_ahead) || (conn->fReadInProgress))
	    {
	      if (PutQ(*buf) == -1)
		return(-1);
	    }
/*
 * If a read is in progress and we've gathered enough data to satisfy it,
 *    get out of the loop.
 */
	  if ((conn->fReadInProgress) &&
	      ((input_rec_len + input_queue_len) >= conn->fReadLength))
	    {
	      if (debug > 1)
		{
		  fprintf(debug_fd, " len\n");
		  debug_need_crlf = 0;
		}
	      break;
	    }
	  readCount = 0;
	} /* for (;;) */
    } /* if (len > 0) */
  if (conn->fReadInProgress)
    ProcessQueueToHost(conn, len);
  return(0);

} /*ProcessTTY*/

int OpenTTY(PTERMIO new_termio, PTERMIO old_termio)
{ /*OpenTTY*/

  long
    posix_vdisable = 0;
  int
    fd = 0;

  if (isatty(STDIN_FILENO))
    stdin_tty = 1;

  fd = STDIN_FILENO;
  if (!stdin_tty)
    return(fd);

  if (GetTtyAttributes(fd, old_termio))
    {
      fprintf(stderr, "Unable to get terminal attributes.\n");
      return(-1);
    }

  *new_termio = *old_termio;

/* Raw mode */
  new_termio->c_lflag = 0;
/* Setup for raw single char I/O */
  new_termio->c_cc[VMIN] = 1;
  new_termio->c_cc[VTIME] = 0;
/* Don't do output post-processing */
  new_termio->c_oflag = 0;
/* Don't convert CR to NL */
  new_termio->c_iflag &= ~(ICRNL);
/* Character formats */
  if (eight_none)
    {
      new_termio->c_cflag &= ~(CSIZE | PARENB | CSTOPB);
      new_termio->c_cflag |= (CS8 | CREAD);
    }
#ifdef BREAK_VIA_SIG
/* Break handling */
  new_termio->c_iflag &= ~IGNBRK;
  new_termio->c_iflag |= BRKINT;
#endif /*BREAK_VIA_SIG*/
#ifdef _PC_VDISABLE
  if ((posix_vdisable = fpathconf(fd, _PC_VDISABLE)) == -1)
    {
      errno = 0;
      posix_vdisable = 0377;
    }
#elif !defined(_POSIX_VDISABLE)
  posix_vdisable = 0377;
#else
  posix_vdisable = _POSIX_VDISABLE;
#endif
  if (disable_xon_xoff)
    {
#ifdef VSTART
      new_termio->c_cc[VSTART]= (unsigned char)posix_vdisable;
#endif
#ifdef VSTOP
      new_termio->c_cc[VSTOP]	= (unsigned char)posix_vdisable;
#endif
    }
#ifdef BREAK_VIA_SIG
  new_termio->c_lflag |= ISIG;
#  ifdef VSUSP
  new_termio->c_cc[VSUSP]	= (unsigned char)posix_vdisable;
#  endif
#  ifdef VDSUSP
  new_termio->c_cc[VDSUSP]	= (unsigned char)posix_vdisable;
#  endif
  new_termio->c_cc[VINTR]	= (unsigned char)posix_vdisable;
  new_termio->c_cc[VQUIT]	= (unsigned char)posix_vdisable;
  new_termio->c_cc[VERASE]	= (unsigned char)posix_vdisable;
  new_termio->c_cc[VKILL]	= (unsigned char)posix_vdisable;
#  ifdef VEOF
  new_termio->c_cc[VEOF]	= (unsigned char)posix_vdisable;
#  endif
#  ifdef VSWTCH
  new_termio->c_cc[VSWTCH]	= (unsigned char)posix_vdisable;
#  endif
#endif /*BREAK_VIA_SIG*/

  SetTtyAttributes(fd, new_termio);

  return(fd);

} /*OpenTTY*/

void CloseTTY(int fd, PTERMIO old_termio)
{ /*CloseTTY*/

  if (stdin_tty)
    SetTtyAttributes(fd, old_termio);
  if (fd != STDIN_FILENO)
    close(fd);

} /*CloseTTY*/

int DoMessageLoop(tVTConnection * conn)
{ /*DoMessageLoop*/
  int
    returnValue = 0;
  ssize_t
    readCount;
  struct timeval
    timeout,
    *time_ptr;
  fd_set
    readfds;
  TERMIO
    new_termios;
  bool
    oldTermiosValid = false;
  int
    nfds = 0;
  char
    termBuffer[2];
  int32_t
    start_time = 0,
    read_timer = 0,
    time_remaining = 0;
  bool
    timed_read = false;
  int
    vtSocket;
  extern FILE
    *debug_fd;

  if ((stdin_fd = OpenTTY(&new_termios, &old_termios)) == -1)
    {
      returnValue = 1;
      goto Last;
    }
  oldTermiosValid = true;    /* We can clean up now. */

/*
 * Setup a read loop waiting for I/O on either fd.  For connection I/O,
 *   process the data using VTReceiveDataReady.  For tty data, add the
 *   data to the outbound queue and call ProcessQueueToHost if a read is
 *   in progress.
 */
    
#ifdef USE_CTLC_INTERRUPTS
/* Dummy call up front to prime the pump */
  break_sigs = break_max;
  CatchCtlC(0);
#endif
  break_sigs = break_max;

  vtSocket = VTSocket(conn);
  if (stdin_tty)
    nfds = 1 + MAX(stdin_fd, vtSocket);
  else
    nfds = 1 + vtSocket;
  while (!done)
    {
      FD_ZERO(&readfds);
      if (stdin_tty)
	FD_SET(stdin_fd, &readfds);
      FD_SET(vtSocket, &readfds);
/*
 * If a read timer has been specified, use it in the select()
 *   call.
 */
      if ((conn->fReadInProgress) && (conn->fReadTimeout))
	{
	  if (!timed_read)
	    { /* First time timer was specified */
	      timed_read = true;
	      start_time = MyGettimeofday();
	      read_timer = conn->fReadTimeout * 1000;
	      time_remaining = read_timer;
	    }
	  timeout.tv_sec = time_remaining / 1000;
	  timeout.tv_usec = (time_remaining % 1000) * 1000;
	  time_ptr = (struct timeval*)&timeout;
	  if (debug)
	    {
		    fprintf(debug_fd, "timer: %ld.%06ld\n",
			    timeout.tv_sec, (long) timeout.tv_usec);
	      debug_need_crlf = 0;
	    }
	}
      else
	{
	  timed_read = false;
	  time_ptr = (struct timeval*)NULL;
	}

      switch (select(nfds, (void*)&readfds, NULL, NULL, time_ptr))
	{
	case -1:	/* Error */
	  if (errno == EINTR)
	    {
#  ifdef BREAK_VIA_SIG
	      if (send_break)
		{
		  ProcessQueueToHost(conn, -2);
		  send_break = false;
		}
#  endif
	      errno = 0;
	      continue;
	    }
	  fprintf(stderr, "Error on select: %d.\n", errno);
	  returnValue = 1;
	  goto Last;
	case 0:		/* Timeout */
	  if (ProcessTTY(conn, termBuffer, -1) == -1)
	    {
	      returnValue = 1;
	      goto Last;
	    }
	  timed_read = false;
	  continue;
	default:
	  if (timed_read)
	    time_remaining = read_timer - ElapsedTime(start_time);
	  if (FD_ISSET(vtSocket, &readfds))
	    {
	      switch (ProcessSocket(conn))
		{
		case -1: returnValue = 1;	/* fall through */
		case 1:  done = true;
		}
	    }
	  if ((!done) && (FD_ISSET(stdin_fd, &readfds)))
	    {
	      if ((readCount = read(stdin_fd, termBuffer, 1)) != 1)
		{
		  returnValue = 1;
		  goto Last;
		}
	      if (ProcessTTY(conn, termBuffer, readCount) == -1)
		{
		  returnValue = 1;
		  goto Last;
		}
	    }
	} /* switch */
    }  /* End read loop */

Last:
#ifdef USE_CTLC_INTERRUPTS
  RestoreCtlC();
#endif
  if (oldTermiosValid)
    CloseTTY(stdin_fd, &old_termios);
  return(returnValue);

} /*DoMessageLoop*/

int main(int argc, char *argv[])
{ /*main*/
  long
    ipAddress;
  int
    ipPort = kVT_PORT;
  struct hostent
    *theHost;
  tVTConnection
    *conn;
  bool
    parm_error = false;
  int
    vtError,
    log_mask = 0,
    returnValue = 0;
  char
    messageBuffer[128],
    *hostname = NULL,
    *input_file = NULL,
    *log_file = NULL,
    *ptr;

  if (argc < 2)
    {
      PrintUsage(1);
      return(2);
    }

  ++argv;
  --argc;
  while ((argc > 0) && (*argv[0] == '-'))
    {
      if (!strncmp(*argv, "-d", 2))
	{
	  ++debug;
	  ptr = *argv;
	  ptr += 2;
	  while (*ptr == 'd')
	    {
	      ++debug;
	      ++ptr;
	    }
	}
      else if (!strcmp(*argv, "-t"))
	type_ahead = true;
      else if ((!strcmp(*argv, "-a")) ||
	       (!strcmp(*argv, "-I")))
	{
	  stop_at_eof = (!strcmp(*argv, "-I")) ? true : false;
	  if (--argc)
	    {
	      ++argv;
	      if (*argv[0] == '-')
		parm_error = true;
	      else
		input_file = *argv;
	    }
	  else
	    parm_error = true;
	}
      else if (!strcmp(*argv, "-8"))
	eight_none = true;
      else if (!strcmp(*argv, "-7"))
	eight_none = false;
      else if (!strcmp(*argv, "-generic"))
	translate = generic = true;
      else if (!strcmp(*argv, "-vt100"))
	translate = vt100 = true;
      else if (!strcmp(*argv, "-vt52"))
	translate = vt52 = true;
      else if (!strcmp(*argv, "-x"))
	disable_xon_xoff = true;
      else if (!strcmp(*argv, "-f"))
	{
	  if (--argc)
	    {
	      ++argv;
	      if (*argv[0] == '-')
		parm_error = true;
	      else
		log_file = *argv;
	    }
	  else
	    parm_error = true;
	}
      else if ((strcmp(*argv, "-X") == 0) ||
	       (strcmp(*argv, "-otable") == 0) ||
	       (strcmp(*argv, "-table") == 0))
	{
	  char *file_name;
	  int i_type = 1;
	  if (strcmp(*argv, "-otable") == 0)
	    ++i_type;
	  if (--argc)
	    {
	      ++argv;
	      if (*argv[0] == '-')
		parm_error = true;
	      else
		{
		  file_name = *argv;
		  if (LoadKeybdTable(file_name, i_type))
		    return(1);
		}
	    }
	  else
	    parm_error = true;
	}
      else if (!strcmp(*argv, "-p"))
	{
	  if (--argc)
	    {
	      ++argv;
	      if (*argv[0] == '-')
		parm_error = true;
	      else
		ipPort = atoi(*argv);
	    }
	  else
	    parm_error = true;
	}
      else if (!strcmp(*argv, "-tt"))
	{
	  if (--argc)
	    {
	      ++argv;
	      if (*argv[0] == '-')
		parm_error = true;
	      else
		term_type = atoi(*argv);
	    }
	  else
	    parm_error = true;
	}
      else if (!strncmp(*argv, "-l", 2))
	{
	  ptr = *argv;
	  ptr += 2;
      log_mask = ParseLogMask(ptr);
      if (log_mask == -1) {
        parm_error = true;
        break;
      }
	}
      else if ((!strcmp(*argv, "-B")) ||
	       (!strcmp(*argv, "-breaks")))
	{
	  if (--argc)
	    {
	      ++argv;
	      if (*argv[0] == '-')
		parm_error = true;
	      else
		break_max = atoi(*argv);
	    }
	  else
	    parm_error = true;
	}
      else if ((!strcmp(*argv, "-T")) ||
	       (!strcmp(*argv, "-breaktimer")))
	{
	  if (--argc)
	    {
	      ++argv;
	      if (*argv[0] == '-')
		parm_error = true;
	      else
		break_timer = atoi(*argv);
	    }
	  else
	    parm_error = true;
	}
      else if ((!strcmp(*argv, "-C")) ||
	       (!strcmp(*argv, "-breakchar")))
	{
	  if (--argc)
	    {
	      ++argv;
	      if (*argv[0] == '-')
		parm_error = true;
	      else
		{
		  break_char = atoi(*argv) & 0x00FF;
		  if (!break_char)
		    break_char = -1;
		}
	    }
	  else
	    parm_error = true;
	}
      else
	parm_error = true;
      if (parm_error)
	{
	  PrintUsage(0);
	  return(2);
	}
      --argc;
      ++argv;
    }
  if (argc > 0)
    hostname = *argv;

  if (!hostname)
    {
      PrintUsage(0);
      return(2);
    }

  if (LogOpen(log_file, log_mask) != 0) {
    return 1;
  }

  if (input_file)
    {
      FILE *input;
      char buf[128], *ptr;
      if ((input = fopen(input_file, "r")) == (FILE*)NULL)
	{
	  perror("fopen");
	  return(1);
	}
      for (;;)
	{
	  if (fgets(buf, sizeof(buf)-1, input) == NULL)
	    break;
	  ptr = buf;
	  while (*ptr)
	    {
	      if (*ptr == '\n')
		PutQ(ASC_CR);
	      else
		PutQ(*ptr);
	      ++ptr;
	    }
	}
      fclose(input);
    }

    /* First, validate the destination. If the destination can be	*/
    /* validated, create a connection structure and try to open the     */
    /* connection.							*/

  ipAddress = (long)inet_addr(hostname);
  if (ipAddress == INADDR_NONE)
    {
      theHost = gethostbyname(hostname);
      if (theHost == NULL)
	{
	  fprintf(stderr, "Unable to resolve %s.\n", hostname);
	  return(1);
	}
      memcpy((char *) &ipAddress, theHost->h_addr, sizeof(ipAddress));
    }

  conn = (tVTConnection *) calloc(1, sizeof(tVTConnection));
  if (conn == NULL)
    {
      fprintf(stderr, "Unable to allocate a connection.\n");
      return(1);
    }

  if ((vtError = VTInitConnection(conn, ipAddress, ipPort)))
    {
      VTErrorMessage(conn, vtError,
		     messageBuffer, sizeof(messageBuffer));
      fprintf(stderr, "Unable to initialize the connection.\n%s\n", messageBuffer);
      VTCleanUpConnection(conn);
      return(1);
    }

  if (term_type == 10)
      conn->fBlockModeSupported = true;	/* RM 960411 */
  
  conn->fDataOutProc =
    ((vt100) ? vt3kHPtoVT100 :
     ((vt52) ? vt3kHPtoVT52 :
      ((generic) ? vt3kHPtoGeneric: vt3kDataOutProc)));

  if ((vtError = VTConnect(conn)))
    {
      VTErrorMessage(conn, vtError,
		     messageBuffer, sizeof(messageBuffer));
      fprintf(stderr, "Unable to connect to host.\n%s\n", messageBuffer);
      VTCleanUpConnection(conn);
      return(1);
    }

  if (stdin_tty)
    {
      char break_desc[32];
      
      if (break_char == -1)
	sprintf(break_desc, "break");
      else if (isprint((char)break_char))
	sprintf(break_desc, "%c", break_char);
      else if (break_char < ' ')
	sprintf(break_desc, "ctl-%c", break_char+'@');
      else
	sprintf(break_desc, "0x%02X", break_char);
      printf("To suspend to FREEVT3K command mode press '%s' %d times in a %d second period.\n",
	     break_desc, break_max, break_timer);
      printf("To send a Break, press '%s' once.\n\n", break_desc);
    }

  break_timer *= 1000;	/* Convert to ms */

  returnValue = DoMessageLoop(conn);

  VTCleanUpConnection(conn);

  return(returnValue);
} /*main*/
