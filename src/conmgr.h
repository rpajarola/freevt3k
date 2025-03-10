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
 * conmgr.h -- Connection manager
 ************************************************************/

enum e_contype {
    e_none = 0,       /* Connection is not established */
    e_tty = 1,        /* Connection is to a tty port */
    e_rlogin = 2,     /* Connection is via rlogin */
    e_vt3k = 3        /* Connection is via vt3k */
};

struct conmgr {
    enum e_contype type;
    char *hostname;
    void *ptr;
    int socket;
    int eof;
};

struct conmgr * conmgr_connect (enum e_contype type, char *hostname, int port);
void conmgr_read (struct conmgr *con);
void conmgr_send (struct conmgr *con, char *buf, size_t nbuf);
void conmgr_send_break (struct conmgr *con);
void conmgr_close (struct conmgr *con);
void conmgr_rxfunc (int32_t refcon, char *buf, size_t nbuf);
