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

/************************************************************
 * getcolor.c -- get color information for display
 ************************************************************/

#include "config.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG_GET_COLORS 0 
#define BLACKWHITE_OK 0
#define GRAYSCALE_OK 0

extern Display *display;
extern int screen_num;
extern Screen *screen_ptr;
extern char *progname;


#if DEBUG_GET_COLORS		
static char *visual_class[] = {
"StaticGray",
"GrayScale",
"StaticColor",
"PseudoColor",
"TrueColor",
"DirectColor"
};
#endif

int get_colors(int nb_colors, char **color_names, unsigned long *color_codes)
{
        int default_depth;
        Visual *default_visual;
	XColor exact_def;
	Colormap default_cmap;
#if DEBUG_GET_COLORS
	int ncolors = 0;
#endif
	int i = 5;
	XVisualInfo visual_info;
	
	/* Try to allocate colors for PseudoColor, TrueColor, 
	 * DirectColor, and StaticColor.  Use black and white
	 * for StaticGray and GrayScale */

	default_depth = DefaultDepth(display, screen_num);
        default_visual = DefaultVisual(display, screen_num);
	default_cmap   = DefaultColormap(display, screen_num);
	if (default_depth == 1) {
		/* must be StaticGray, use black and white */
#if BLACKWHITE_OK
		border_pixel = BlackPixel(display, screen_num);
		background_pixel = WhitePixel(display, screen_num);
		foreground_pixel = BlackPixel(display, screen_num);
		return(0);
#else
                fprintf (stderr,"Need a color monitor\n");
                exit(0);
#endif
	}

	while (!XMatchVisualInfo(display, screen_num, default_depth, /* visual class */i--, &visual_info)) ;
#if DEBUG_GET_COLORS		
	printf("%s: found a %s class visual at default_depth.\n", 
                progname, visual_class[++i]);
#endif	
	if (i < 2) {
		/* No color visual available at default_depth.
		 * Some applications might call XMatchVisualInfo
		 * here to try for a GrayScale visual 
		 * if they can use gray to advantage, before 
		 * giving up and using black and white.
		 */
#if GRAYSCALE_OK
		border_pixel = BlackPixel(display, screen_num);
		background_pixel = WhitePixel(display, screen_num);
		foreground_pixel = BlackPixel(display, screen_num);
		return(0);
#else
                fprintf (stderr,"Need a color monitor\n");
                exit (0);
#endif
	}

	/* otherwise, got a color visual at default_depth */

	/* The visual we found is not necessarily the 
	 * default visual, and therefore it is not necessarily
	 * the one we used to create our window.  However,
	 * we now know for sure that color is supported, so the
	 * following code will work (or fail in a controlled way).
	 * Let's check just out of curiosity: */
	if (visual_info.visual != default_visual)
		printf("%s: PseudoColor visual at default depth is not default visual!\nContinuing anyway...\n", progname);

	for (i = 0; i < nb_colors; i++) {
#if DEBUG_GET_COLORS
		printf("allocating %s\n", color_names[i]);
#endif
		if (!XParseColor (display, default_cmap, color_names[i],
                                 &exact_def)) {
			fprintf(stderr, "%s: color name %s not in database",
                                 progname, color_names[i]);
			exit(0);
		}
#if DEBUG_GET_COLORS
		printf("The RGB values from the database are %d, %d, %d\n", exact_def.red, exact_def.green, exact_def.blue);
#endif
   		if (!XAllocColor(display, default_cmap, &exact_def)) {
			fprintf(stderr, "%s: can't allocate color: all colorcells allocated and no matching cell found.\n", progname);
		exit(0);
		}
#if DEBUG_GET_COLORS
		printf("The RGB values actually allocated are %d, %d, %d\n", exact_def.red, exact_def.green, exact_def.blue);
#endif
		color_codes[i] = exact_def.pixel;
#if DEBUG_GET_COLORS
		ncolors++;
#endif
	}
#if DEBUG_GET_COLORS
	printf("%s: allocated %d read-only color cells\n", progname, ncolors);
#endif
	return(1);
}
