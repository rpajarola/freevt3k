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

/*
 * x11glue.h  --  X11 window interface
 *
 * Derived from O'Reilly and Associates example 'basicwin.c'
 * Copyright 1989 O'Reilly and Associates, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * online documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of O'Reilly
 * and Associates, Inc. not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  O'Reilly and Associates, Inc. makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * O'REILLY AND ASSOCIATES, INC. DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL O'REILLY AND ASSOCIATES, INC.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <stdio.h>

#define LOG_INPUT               (0x01)
#define LOG_OUTPUT              (0x02)
#define LOG_PREFIX              (0x04)

extern FILE
* logFd;

extern int
  log_type;

void init_disp (int argc, char **argv, char *hostname, char *font1);
void event_loop (void);
void getGC (Window win, GC * gc, XFontStruct * font_info);
void load_font (XFontStruct ** font_info, char *font1);
int keymapper (KeySym keysym, unsigned int state, char *buffer, int charcount);
void disp_drawtext (int style, int row, int col, char *buf, int nbuf);
void disp_erasetext (int row, int col, int nchar);
void disp_drawcursor (int style, int row, int col);
void Logit (int typ, char *ptr, size_t len, bool special_dc1);
void doXBell (void);
