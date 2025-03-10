/* Copyright (C) 1996 Office Products Technology, Inc.

   freevt3k: a VT emulator for Unix. 

   freevt3k.h      

   External and connection structure definitions.

   This file is distributed under the GNU General Public License.
   You should have received a copy of the GNU General Public License
   along with this file; if not, write to the Free Software Foundation,
   Inc., 675 Mass Ave, Cambridge MA 02139, USA. 
*/

#ifndef _FREEVT3K_H
#define _FREEVT3K_H

#include "typedef.h"

#define LOG_INPUT		(0x01)
#define LOG_OUTPUT		(0x02)
#define LOG_PREFIX              (0x04)

extern int debug;

int PutImmediateQ(char ch);
void vt3kDataOutProc(int32_t refCon, char * buffer, size_t bufferLength);

#endif

