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
 * rlogin.c -- remote login via rlogin
 ************************************************************/

#include "config.h"
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "conmgr.h"

static void show_network_error (char * funcname, int errnum)
/*
**  Show error condition
*/
{
    fprintf (stderr, "A networking error has occurred\n");
    fprintf (stderr, "Function name: %s\n", funcname);
    fprintf (stderr, "Error number:  %d\n", errnum);
    perror  ("Error message");

    if (errnum == EACCES) {
	fprintf (stderr, "\n");
	fprintf (stderr, "Unix requires rlogin clients to be setuid root.\n");
#if 0
	fprintf (stderr, "Try the following:\n");
	fprintf (stderr, "    su\n");
	fprintf (stderr, "    chown root xhpterm\n");
	fprintf (stderr, "    chmod 4555 xhpterm\n");
	fprintf (stderr, "    exit\n");
#endif
    }
    fflush (stderr);
}

/***************************************************************/
static struct hostent * gethostbynumbers (char * s)
/*
**  Parse an address using xxx.xxx.xxx.xxx syntax
**  Returns null pointer if syntax error occurs.
*/
{
    static char ha[4];
    static char *hap[2];
    static struct hostent h;
    int i,j,k;

    i=0;
    for (j=0; j<4; j++) {
        if (!isdigit(s[i])) return (0);
        k=0;
        while (isdigit(s[i])) {
            k = k * 10 + s[i++] - '0';
            if (k > 255) return (0);
        }
        ha[j] = k;
        if (j<3) {
            if (s[i++] != '.') return (0);
        } else {
            if (s[i]) return (0);
        }
    }
    hap[0] = ha;
    hap[1] = 0;
    h.h_addr_list = hap;
    return (&h);
}
/***************************************************************/
int open_client_connection (char * hostname, int portnum)
/*
**  Create client socket, connect to server,
**  return socket number.
*/
{
    int s,af,type,protocol;   /* socket() */
    struct sockaddr_in addr;
    struct hostent *h;
    unsigned char ip_addr[4];
    int addrlen;
    int errn;
    int myport;

    printf ("Finding %s... ", hostname);
    fflush (stdout);

    h = gethostbynumbers (hostname);
    if (!h) {
        h = gethostbyname (hostname);
    }
    if (!h) {
        printf ("open_rlogin_connection: hostname=%s\n", hostname);
        printf ("error from gethostbyname()\n");
        printf ("Unable to resolve hostname\n");
        return (0);
    }
    ip_addr[0] = h->h_addr_list[0][0];
    ip_addr[1] = h->h_addr_list[0][1];
    ip_addr[2] = h->h_addr_list[0][2];
    ip_addr[3] = h->h_addr_list[0][3];

    printf ("Trying %d.%d.%d.%d\n",
               ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

    af = AF_INET;
    type = SOCK_STREAM;
    protocol = 0;
    s = socket (af, type, protocol);
    if (s == -1) {
        show_network_error ("socket()", errno);
        return (0);
    }
/*
**  Search for an unused local port
*/
    myport = 1022;
    do {
        memset ((void*)&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(myport); 

        addrlen = sizeof(addr);
        errn = bind (s, (void*)&addr, addrlen);
	if (errn) {
	    if (errno==EADDRINUSE && myport>1000) {
	        myport--;
            } else {
                show_network_error ("bind()", errno);
                return (0);
            }
        }
    } while (errn);

    memset ((void*)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(portnum); /* loopback port 1260, telnet port is 23 */
    memcpy ((void*)&(addr.sin_addr), (void*)ip_addr, 4);
    addrlen = sizeof(addr);

printf ("Doing connect...\n");
fflush (stdout);

    errn = connect (s, (void*)&addr, addrlen);
    if (errn) {
        show_network_error ("connect()", errno);
        return (0);
    }

printf ("Got the connection!\n");
fflush (stdout);
    return (s);
}
/***************************************************************/
int read_rlogin_data (int s /* socket number */)
/*
**  Read data from the rlogin server
**  Send it to the terminal emulator
**  Returns non-zero on socket eof
*/
{
    int flags,len;
    char buf[2048];
    size_t bufsize = sizeof(buf) / sizeof(buf[0]);

    flags = 0;
    len = recv (s, buf, bufsize, flags);
    if (len < 0) {
	show_network_error ("recv()", errno);
	return (1);
    } else if (!len) {
	return (1);
    }

    conmgr_rxfunc (0, buf, len);
    return (0);
}
/***************************************************************/
int send_rlogin_data (int s /* socket number */,
		      char * buf /* character buffer */,
		      size_t nbuf /* character count */)
/*
**  Send data to the rlogin server
**  Returns non-zero on socket eof
*/
{
    int flags,len;

    flags = 0;
    len = send (s, buf, nbuf, flags);
    if (len < nbuf) {
        show_network_error ("send()", errno);
        return (1);
    }
    return (0);
}
/***************************************************************/
int open_rlogin_connection (hostname)
/*
**  Create an rlogin connection to a remote computer
*/
    char *hostname;
{
    int s;
    static char username[100] = "";
    static char termname[100] = "hpterm/9600";
    char buf[100];
    int nbuf;

    s = open_client_connection (hostname, 513); /* rlogin=513, echo=7 */
    if (!s) return (0);
/*
**  Send the rlogin startup messages
*/
    nbuf=0;
    buf[nbuf++] = 0;

    strcpy (buf+nbuf, username);
    nbuf += strlen(username);
    buf[nbuf++] = 0;

    strcpy (buf+nbuf, username);
    nbuf += strlen(username);
    buf[nbuf++] = 0;

    strcpy (buf+nbuf, termname);
    nbuf += strlen(termname);
    buf[nbuf++] = 0;

    send_rlogin_data (s, buf, nbuf);

    return (s);
}
