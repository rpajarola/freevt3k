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
 * typedef.h -- Compiler-dependent typedefs
 ************************************************************/

#ifndef _TYPEDEF_H
#define _TYPEDEF_H

/* C99 standard integer types */
#include <stdint.h>

/* C99 standard boolean type */
#include <stdbool.h>

typedef short int16;
typedef unsigned short unsigned16;
typedef long int32;
typedef unsigned long unsigned32;
typedef unsigned char unsigned8;
typedef char tBoolean;

#include <unistd.h>
#include <netinet/in.h>

#endif

