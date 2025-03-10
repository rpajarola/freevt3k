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
 * vt3kglue.c -- Interface between character based terminal
 *               emulator and record based vt3k protocol
 ************************************************************/

#include "config.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>

#include "typedef.h"
#include "vt.h"
#include "freevt3k.h"
#include "vtconn.h"

#include "conmgr.h"

/* Useful macros */

#ifndef MAX
#  define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* Global variables */



tVTConnection * open_vt3k_connection (char *hostname, int port)
{
    long  ipAddress;
    int   ipPort = port;
    struct hostent * theHost;
    tVTConnection * theConnection;
    int             vtError;
    char	messageBuffer[128];
    int         term_type = 10;

    /* First, validate the destination. If the destination can be	*/
    /* validated, create a connection structure and try to open the     */
    /* connection.							*/

    ipAddress = inet_addr(hostname);
    if (ipAddress == INADDR_NONE)
	{
	theHost = gethostbyname(hostname);
	if (theHost == NULL)
	    {
            printf("Unable to resolve %s.\n", hostname);
	    theConnection = NULL;
	    goto Last;
            }
        memcpy((char *) &ipAddress, theHost->h_addr, sizeof(ipAddress));
	}

    theConnection = (tVTConnection *) calloc(1, sizeof(tVTConnection));
    if (theConnection == NULL)
        {
        printf("Unable to allocate a connection.\n");
        goto Last;
        }

    if ((vtError = VTInitConnection(theConnection, ipAddress, ipPort)))
        {
        printf("Unable to initialize the connection.\n");
        VTErrorMessage(theConnection, vtError,
				messageBuffer, sizeof(messageBuffer));
        printf("%s\n", messageBuffer);
	free (theConnection);
	theConnection = NULL;
        goto Last;
        }

    printf("Connection initialized.\n");

    if (term_type == 10) {
	theConnection->fBlockModeSupported = true;      /* RM 960411 */
    }

    theConnection->fDataOutProc = conmgr_rxfunc;

    if ((vtError = VTConnect(theConnection)))
	{
        printf("Unable to connect to host.\n");
        VTErrorMessage(theConnection, vtError,
				messageBuffer, sizeof(messageBuffer));
        printf("%s\n", messageBuffer);
	VTCleanUpConnection(theConnection);
	free (theConnection);
	theConnection = NULL;
        goto Last;
	}
Last:
    return (theConnection);
}


int read_vt3k_data (tVTConnection * theConnection) {

    int whichError;
    bool done=false;
    char messageBuffer[128];
    static char trigger[] = { 17 };
    int eof=0;


    whichError = VTReceiveDataReady(theConnection);
    if (whichError == kVTCVTOpen)
	{
	/* Now the connection is _really_ open */
	}
    else if (whichError != kVTCNoError) {
	{
	if (whichError == kVTCStartShutdown) {
            done=true;
            eof=1;
	} else
	    {
	    printf ("VT error!:\n");
	    VTErrorMessage(theConnection, whichError,
			  messageBuffer, sizeof(messageBuffer));
            printf ("%s\n", messageBuffer);
	    done=true;
	    }
        }
    }
/*
 *  Check for start of a new read
 */
    if (!done && theConnection->fReadStarted) {
	theConnection->fReadStarted = false;
/*
 *      Check for need to flush the type-ahead buffer
 */
	if (theConnection->fReadFlush)  /* RM 960403 */
	    {
	    theConnection->fReadFlush = false;
	    FlushQ();
	    }
/*
 *      Send read trigger that is needed by hpterm
 */
	theConnection->fDataOutProc (theConnection->fDataOutRefCon,
				     trigger, sizeof(trigger));
/*
 *      As we just got a read request, check for any typed-ahead data and
 *      process it now.
 */
	ProcessQueueToHost(theConnection, 0);
    }
    return (eof);
}
/**********************************************************************/
int send_vt3k_data (tVTConnection * theConnection, char *buf, size_t nbuf) {

    int ii;
    char ch;

    for (ii=0; ii<nbuf; ii++) {
	ch = buf[ii];
	if (PutQ(ch) == -1) break;
    }

    if (theConnection->fReadInProgress) {
        ProcessQueueToHost(theConnection, 0);
    }

    return (0);
}
/*********************************************************************/
void send_vt3k_break (tVTConnection * theConnection) {

    theConnection->fSysBreakEnabled = true;   /* Should not be needed */
    ProcessQueueToHost(theConnection, -2);
}
/*********************************************************************/
void close_vt3k (tVTConnection * theConnection) {

    VTCleanUpConnection(theConnection);
    free (theConnection);
}
/*********************************************************************/
