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
 * timers.c -- gettimeofday wrapper
 ************************************************************/

#include <stdint.h>
#include <sys/time.h>

int32_t MyGettimeofday(void)
{ /*MyGettimeofday*/


    static long
	baseSec = 0;
    static int
	baseSet = 0;
    long
	sec,
	ms;
    struct timeval
	tp;
#  ifdef SHORT_GETTIMEOFDAY
    (void)gettimeofday(&tp);
#  else
    struct timezone
	tzp;

    (void)gettimeofday(&tp, &tzp);
#  endif
    sec = tp.tv_sec;
    ms = tp.tv_usec/1000;
    if (!baseSet)
	{
	baseSet = 1;
	baseSec = sec;
	sec = 0;
	}
    else
	sec = sec - baseSec;
    return((int32_t)((sec*1000)+ms));

} /*MyGettimeofday*/

int32_t ElapsedTime(int32_t start_time)
{ /*ElapsedTime*/

    return(MyGettimeofday() - start_time);

} /*ElapsedTime*/
#ifdef INCLUDE_WALLTIME

void WallTime(void)
{ /*WallTime*/

    time_t
	ltime;
    struct tm
	*tmNow;
    int
	ms;
    extern FILE
	*debug_fd;

    time(&ltime);
    tmNow = localtime(&ltime);
    ms = (int)(MyGettimeofday() % 1000);
    fprintf(debug_fd, "%02d:%02d:%02d.%03d: ",
	    tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec, ms);
    
} /*WallTime*/
#endif /*INCLUDE_WALLTIME*/
