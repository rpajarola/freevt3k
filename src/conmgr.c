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
 * conmgr.c -- Connection manager
 ************************************************************/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "vt3kglue.h"
#include "tty.h"
#include "rlogin.h"
#include "conmgr.h"
/*******************************************************************/
#define SHOW_RX_DATA 0
#define SHOW_TX_DATA 0
/*******************************************************************/
void conmgr_rxfunc (int32_t refcon, char *buf, size_t nbuf) {
/*
**  Callback function which receives characters from the connection
*/
#if SHOW_RX_DATA
    {
	int ii,ch;
	for (ii=0; ii<nbuf; ii++) {
	    ch = buf[ii];
	    if (ch<32 || ch>126) {
	        printf ("<%d>", ch);
	    } else {
		printf ("<%c>", ch);
	    }
	    hpterm_rxfunc (0, &buf[ii], 1);
	}
	fflush (stdout);
    }
#else
    hpterm_rxfunc (0, buf, nbuf);
#endif
}
/*******************************************************************/
struct conmgr * conmgr_connect (enum e_contype type, char *hostname, int port) {
/*
**  Establish a connection
*/
    void *ptr=0;
    int s=0;
    struct conmgr *out=0;

    switch (type) {
	case e_tty:
	    s = open_tty_connection (hostname);
	    if (s == -1)
	      return (0);
	    break;
        case e_rlogin:
	    s = open_rlogin_connection (hostname);
	    if (!s) return (0);
	    break;
        case e_vt3k:
	    ptr = open_vt3k_connection (hostname, port);
	    if (!ptr) return (0);
	    s = VTSocket(ptr);
	    break;
        default:
	    printf ("conmgr_connect: unknown contype %d\n", type);
	    return (0);
    }
    out = (struct conmgr*)calloc(1,sizeof(struct conmgr));
    out->type = type;
    out->hostname = strcpy(malloc(strlen(hostname)+1),hostname);
    out->ptr = ptr;
    out->socket = s;
    out->eof = 0;
    return (out);
}
/*********************************************************************/
void conmgr_read (struct conmgr *con) {
/*
**  Read from connection, send to term_rxchar
*/
    int stat=0;

    switch (con->type) {
	case e_tty:
	    stat = read_tty_data (con->socket);
	    break;
        case e_rlogin:
	    stat = read_rlogin_data (con->socket);
	    break;
        case e_vt3k:
	    stat = read_vt3k_data (con->ptr);
	    break;
        default:
	    printf ("conmgr_read: type=%d\n", con->type);
	    stat = 1;
	    break;
    }
    if (stat) con->eof=1;
}
/***********************************************************************/
void conmgr_send (struct conmgr *con, char *buf, size_t nbuf) {

    int stat;

#if SHOW_TX_DATA
    {
	int ii,ch;
	for (ii=0; ii<nbuf; ii++) {
	    ch = buf[ii];
	    if (ch<32 || ch>126) {
		printf ("{%d}", ch);
            } else {
		printf ("{%c}", ch);
            }
        }
	fflush (stdout);
    }
#endif

    switch (con->type) {
	case e_tty:
	    stat = send_tty_data (con->socket, buf, nbuf);
	    break;
        case e_rlogin:
	    stat = send_rlogin_data (con->socket, buf, nbuf);
	    break;
        case e_vt3k:
	    stat = send_vt3k_data (con->ptr, buf, nbuf);
	    break;
        default:
	    printf ("conmgr_send: type=%d\n", con->type);
	    stat = 1;
	    break;
    }
    if (stat) con->eof=1;
}
/*********************************************************************/
void conmgr_send_break (struct conmgr *con) {

    switch (con->type) {
        case e_tty:
	    break;
        case e_rlogin:
	    break;
        case e_vt3k:
	    send_vt3k_break (con->ptr);
	    break;
	default:
	    break;
    }
}
/*********************************************************************/
void conmgr_close (struct conmgr *con) {

    switch (con->type) {
	case e_tty:
	    break;
        case e_rlogin:
	    break;
        case e_vt3k:
	    close_vt3k (con->ptr);
	    break;
        default:
	    break;
    }
}
/********************************************************************/
