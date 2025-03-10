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
 * vt3kglue.h -- Interface between character based terminal
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

#include "hpterm.h"

void myDataOutProc (int32_t refCon, char *buf, int nbuf);
tVTConnection * open_vt3k_connection (char *hostname, int port);
int read_vt3k_data (tVTConnection * theConnection);
int send_vt3k_data (tVTConnection * theConnection, char *buf, int nbuf);
void send_vt3k_break (tVTConnection * theConnection);
void close_vt3k (tVTConnection * theConnection);
