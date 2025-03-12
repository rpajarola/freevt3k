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
 * logging.h -- logging functions
 ************************************************************/

#include <stdbool.h>

#define LOG_INPUT               (0x01)
#define LOG_OUTPUT              (0x02)
#define LOG_PREFIX              (0x04)

void DumpBuffer(void *buf, long buf_len, char *dump_id);
int ParseLogMask(char *optarg);
int LogOpen (char *log_file, int log_mask_);
void Logit (int log_type, char *ptr, size_t len, bool special_dc1);
int IsLogging(void);

