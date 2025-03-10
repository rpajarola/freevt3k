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
 * hpvt100.h -- Header file for VT100 translation
 ************************************************************/

void vt3kHPtoVT100(int32_t refCon, char *buf, size_t buf_len);
void vt3kHPtoVT52(int32_t refCon, char *buf, size_t buf_len);
void vt3kHPtoGeneric(int32_t refCon, char *buf, size_t buf_len);
void TranslateKeyboard(char *buf, int *buf_len);
