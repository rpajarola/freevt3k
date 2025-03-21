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
 * x11glue.c  --  X11 window interface
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
#include "config.h"
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <stdio.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <netinet/in.h>
#endif

#  define USE_9X15     1

#ifdef USE_9X15
#  define FONT_NAME	"9x15"
#else
#  define FONT_NAME	"-adobe-courier-medium-r-normal--*-140-75-75-m-*-iso8859-1"
#  define FONT_NAME0	"-bitstream-r_ansi-medium-r-normal--*-120-75-75-m-*-iso8859-1"
#endif

#include "x11glue.h"
#include "conmgr.h"
#include "hpterm.h"
#include "logging.h"
#include "vtconn.h"
#include "kbdtable.h"
#include "terminal.bm"

#define DEBUG_KEYSYMS 0

#define BITMAPDEPTH 1

/* These are used as arguments to nearly every Xlib routine, so it saves
 * routine arguments to declare them global.  If there were
 * additional source files, they would be declared extern there. */
Display *display;
int screen_num;
Window win;

unsigned int width, height;	/* window size (pixels) */
GC gc_normal;
GC gc_inverse;
GC gc_halfbright;
GC gc_red;

XFontStruct *font_info;

static struct hpterm *term;

struct conmgr *con = 0;
int logging = 0;

int must_logoff = 0;

char *termid = NULL;

char *version_str = "B.00.A0";

/*******************************************************************/
int get_colors(int nb_colors, char **color_names, unsigned long *color_codes);
void getGC(Window win, GC * gc, XFontStruct * font_info);
void getGC_Inverse(Window win, GC * gc, XFontStruct * font_info);
void getGC_Halfbright(Window win, GC * gc, XFontStruct * font_info);
void getGC_Red(Window win, GC * gc, XFontStruct * font_info);
/*******************************************************************/
#define GRAY_INDEX 0
#define RED_INDEX 1
#define NB_COLORS 2
unsigned long color_codes[NB_COLORS];
char *color_names[NB_COLORS] =
{"light gray", "red"};
char *progname;			/* name this program was invoked by */

/*******************************************************************/
void init_disp (int argc, char **argv, char *wintitle, char *fontname)
{
  int x, y;		/* window position */
  unsigned int border_width = 4;	/* four pixels */
  char *icon_name = "freevt3k";
  Pixmap icon_pixmap;
  XSizeHints size_hints;
  XIconSize *size_list;
  int count;
  char *display_name = NULL;
  int nbrows = 26;
  int nbcols = 80;

  progname = argv[0];

  /* connect to X server */
  if ((display = XOpenDisplay (display_name)) == NULL)
  {
    (void) fprintf (stderr, "%s: cannot connect to X server %s\n",
		    progname, XDisplayName (display_name));
    exit (-1);
  }

  /* get screen size from display structure macro */
  screen_num = DefaultScreen (display);

  /* Note that in a real application, x and y would default to 0
   * but would be settable from the command line or resource database.
   */
  x = y = 0;

/*
 *  fetch font info to ensure metrics
 */

  load_font (&font_info, fontname);

  width = nbcols * font_info->max_bounds.width;
  height = nbrows * (font_info->ascent + font_info->descent);

  /* create opaque window */
  win = XCreateSimpleWindow (display, RootWindow (display, screen_num),
		     x, y, width, height, border_width, BlackPixel (display,
			     screen_num), WhitePixel (display, screen_num));

  /* Get available icon sizes from Window manager */

  if (XGetIconSizes (display, RootWindow (display, screen_num),
		     &size_list, &count) == 0)
    (void) fprintf (stderr, "%s: Window manager didn't set icon sizes - using default.\n", progname);
  else
  {
    ;
    /* A real application would search through size_list
     * here to find an acceptable icon size, and then
     * create a pixmap of that size.  This requires
     * that the application have data for several sizes
     * of icons. */
  }

  /* Create pixmap of depth 1 (bitmap) for icon */

  icon_pixmap = XCreateBitmapFromData (display, win, (const char *) terminal_bits,
				       terminal_width, terminal_height);

  /* Set size hints for window manager.  The window manager may
   * override these settings.  Note that in a real
   * application if size or position were set by the user
   * the flags would be UPosition and USize, and these would
   * override the window manager's preferences for this window. */

#ifdef X11R3
  size_hints.flags = PPosition | PSize | PMinSize;
  size_hints.x = x;
  size_hints.y = y;
  size_hints.width = width;
  size_hints.height = height;
  size_hints.min_width = 300;
  size_hints.min_height = 200;
#else /* X11R4 or later */

  /* x, y, width, and height hints are now taken from
   * the actual settings of the window when mapped. Note
   * that PPosition and PSize must be specified anyway. */

  size_hints.flags = PPosition | PSize | PMinSize;
  size_hints.min_width = 300;
  size_hints.min_height = 200;

#endif

#ifdef X11R3
  /* set Properties for window manager (always before mapping) */
  XSetStandardProperties (display, win, wintitle, icon_name,
			  icon_pixmap, argv, argc, &size_hints);

#else /* X11R4 or later */

  {
    XWMHints wm_hints;
    XClassHint class_hints;

    /* format of the window name and icon name
     * arguments has changed in R4 */
    XTextProperty windowName, iconName;

    /* These calls store wintitle (window_name) and icon_name into
     * XTextProperty structures and set their other
     * fields properly. */
    if (XStringListToTextProperty (&wintitle, 1, &windowName) == 0)
    {
      (void) fprintf (stderr, "%s: structure allocation for windowName failed.\n",
		      progname);
      exit (-1);
    }

    if (XStringListToTextProperty (&icon_name, 1, &iconName) == 0)
    {
      (void) fprintf (stderr, "%s: structure allocation for iconName failed.\n",
		      progname);
      exit (-1);
    }

    wm_hints.initial_state = NormalState;
    wm_hints.input = True;
    wm_hints.icon_pixmap = icon_pixmap;
    wm_hints.flags = StateHint | IconPixmapHint | InputHint;

    class_hints.res_name = progname;
    class_hints.res_class = "Basicwin";

    XSetWMProperties (display, win, &windowName, &iconName,
		      argv, argc, &size_hints, &wm_hints,
		      &class_hints);
  }
#endif

  /* Select event types wanted */
  XSelectInput (display, win, ExposureMask | KeyPressMask |
		ButtonPressMask | StructureNotifyMask);

  /* get colors that we need */
  get_colors (NB_COLORS, color_names, color_codes);

  /* create GC for text and drawing */
  getGC (win, &gc_normal, font_info);

  /* create GC for Inverse video */
  getGC_Inverse (win, &gc_inverse, font_info);

  /* create GC for halfbright video */
  getGC_Halfbright (win, &gc_halfbright, font_info);

  /* create GC for red video */
  getGC_Red (win, &gc_red, font_info);

  /* Display window */
  XMapWindow (display, win);

}
void event_loop (void)
{

  int xsocket, nfds, i;		/* select stuff */
  int dsocket;
  fd_set readfds, readmask;
  int count;

  XComposeStatus compose;
  KeySym keysym;
  int bufsize = 20;
  int charcount;
  char buffer[20];
  int nbrows, nbcols;

  XEvent report;

  while (!con->eof)
  {
    term_update ();		/* flush deferred screen updates */
    XFlush (display);		/* and send them to the server */

    /* Use select() to wait for traffic from X or remote computer */
    xsocket = ConnectionNumber (display);
    dsocket = con->socket;
    nfds = (dsocket < xsocket) ? (1 + xsocket) : (1 + dsocket);

    FD_ZERO (&readmask);
    if (!(con->eof))
      FD_SET (dsocket, &readmask);
    FD_SET (xsocket, &readmask);
    readfds = readmask;
    i = select (nfds, (void *) &readfds, 0, 0, 0);

    if (i < 0)
    {
      perror ("select failed");
    }
    else if (i == 0)
    {
      printf ("select timed out\n");
    }
    else if ((FD_ISSET (dsocket, &readmask)) &&
	     (FD_ISSET (dsocket, &readfds)))
    {

      conmgr_read (con);

      /*
         **  Wait up to 10 ms for more data from host
         **  This should prevent excessive re-draws
       */
      i = 1;
      count = 0;
      while (i > 0 && count < 10 && !(con->eof))
      {
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 10000;

	FD_ZERO (&readfds);
	FD_SET (dsocket, &readfds);

	i = select (nfds, (void *) &readfds, 0, 0, (struct timeval *) &timeout);
	if (i < 0)
	{
	  perror ("select failed");
	}
	else if (i == 0)
	{
	  /* time out */

	}
	else if (FD_ISSET (dsocket, &readfds))
	{
	  /* more data */
	  conmgr_read (con);
	}
	count++;		/* Can't ignore X for too long */
      }
    }
    while (XPending (display))
    {
      XNextEvent (display, &report);
      switch (report.type)
      {
      case Expose:
	/* unless this is the last contiguous expose,
	 * don't draw the window */
	if (report.xexpose.count != 0)
	  break;

	/* redraw term emulator stuff */
	term_redraw ();
	break;
      case ConfigureNotify:
	/* window has been resized
	 * notify hpterm.c */
	width = report.xconfigure.width;
	height = report.xconfigure.height;
	nbcols = width / font_info->max_bounds.width;
	nbrows = height / (font_info->ascent + font_info->descent);
	hpterm_winsize (nbrows, nbcols);
	break;
      case ButtonPress:
	if (report.xbutton.button == 1)
	{
	  int r, c;
	  c = report.xbutton.x / font_info->max_bounds.width;
	  r = report.xbutton.y / (font_info->ascent + font_info->descent);
	  hpterm_mouse_click (r, c);
	}
	/* right mouse button causes program to exit */
	if ((report.xbutton.button == 3) && (!must_logoff))
	{
	  return;
	}
	break;
      case KeyPress:
	charcount = XLookupString (&report.xkey, buffer,
				   bufsize, &keysym, &compose);

	if (DEBUG_KEYSYMS)
	{
	  int ii;
	  printf ("(%lx)", (unsigned long) keysym);
	  printf ("<");
	  for (ii = 0; ii < charcount; ii++)
	  {
	    if (ii)
	      printf (",");
	    printf ("%d", buffer[ii]);
	  }
	  printf (">");
	  if (report.xkey.state)
	  {
	    printf ("[%x,%lx]\n", report.xkey.state, keysym);
	  }
	  else
	  {
	    printf ("[%lx]\n", keysym);
	  }
	  printf ("KeySym  [%s]\n", XKeysymToString (keysym));
	  fflush (stdout);
	}

	if (!keymapper (keysym, report.xkey.state, buffer, charcount))
	{
	  if (charcount == 1)
	  {
	    hpterm_kbd_ascii (buffer[0]);
	  }
	}
	break;
      default:
	/* all events selected by StructureNotifyMask
	 * except ConfigureNotify are thrown away here,
	 * since nothing is done with them */
	break;
      }				/* end switch */
    }
  }				/* end while */
}

void getGC (Window win, GC *gc, XFontStruct *font_info)
{
  unsigned long valuemask = 0;	/* ignore XGCvalues and use defaults */
  XGCValues values;
  unsigned int line_width = 6;
  int line_style = LineOnOffDash;
  int cap_style = CapRound;
  int join_style = JoinRound;
  int dash_offset = 0;
  static char dash_list[] =
  {12, 24};
  int list_length = 2;

  /* Create default Graphics Context */
  *gc = XCreateGC (display, win, valuemask, &values);

  /* specify font */
  XSetFont (display, *gc, font_info->fid);

  /* specify black foreground since default window background is
   * white and default foreground is undefined. */
  XSetForeground (display, *gc, BlackPixel (display, screen_num));

  /* set line attributes */
  XSetLineAttributes (display, *gc, line_width, line_style,
		      cap_style, join_style);

  /* set dashes */
  XSetDashes (display, *gc, dash_offset, dash_list, list_length);
}

void getGC_Inverse (Window win, GC *gc, XFontStruct *font_info)
{
  unsigned long valuemask = 0;	/* ignore XGCvalues and use defaults */
  XGCValues values;

  /* Create default Graphics Context */
  *gc = XCreateGC (display, win, valuemask, &values);

  /* specify font */
  XSetFont (display, *gc, font_info->fid);

  /* Set foreground and background swapped for Inverse Video */
  XSetForeground (display, *gc, WhitePixel (display, screen_num));
  XSetBackground (display, *gc, BlackPixel (display, screen_num));
}

void getGC_Halfbright (Window win, GC *gc, XFontStruct *font_info)
{
  unsigned long valuemask = 0;	/* ignore XGCValues and use defaults */
  XGCValues values;

  /* Create default Graphics Context */
  *gc = XCreateGC (display, win, valuemask, &values);

  /* specify font */
  XSetFont (display, *gc, font_info->fid);

  /* Set foreground gray and background white for halfbright video */
  XSetForeground (display, *gc, color_codes[GRAY_INDEX]);
  XSetBackground (display, *gc, WhitePixel (display, screen_num));
}

void getGC_Red (Window win, GC *gc, XFontStruct *font_info)
{
  unsigned long valuemask = 0;	/* ignore XGCValues and use defaults */
  XGCValues values;

  /* Create default Graphics Context */
  *gc = XCreateGC (display, win, valuemask, &values);

  /* specify font */
  XSetFont (display, *gc, font_info->fid);

  /* Set foreground red and background white */
  XSetForeground (display, *gc, color_codes[RED_INDEX]);
  XSetBackground (display, *gc, WhitePixel (display, screen_num));
}

void load_font (XFontStruct **font_info, char *font1)
{
  char *fontname = FONT_NAME;
  if(font1 != NULL)
  {
    /* Load font and get font information structure. */
    if ((*font_info = XLoadQueryFont (display, font1)) == NULL)
	{
	    (void) fprintf (stderr, "%s: Cannot open %s font, switching to DEFAULT\n",
			    progname, font1);
	    if ((*font_info = XLoadQueryFont (display, fontname)) == NULL)
	    {
	      (void) fprintf (stderr, "%s: Cannot open %s font\n",
			    progname, fontname);
	      exit (-1);
	    }
	}
  }
  else
  {
	  if ((*font_info = XLoadQueryFont (display, fontname)) == NULL)
	  {
	    (void) fprintf (stderr, "%s: Cannot open %s font\n",
			    progname, fontname);
	    exit (-1);
	  }
  }
}

static struct km
{
  char *keyname;
  KeySym keysym;
  void (*keyfunc) (void);
}
keymap[] =
{
    {"Break",     XK_Break,    hpterm_kbd_Break},
    {"Menu",      XK_Menu,     hpterm_kbd_Menu},
    {"F1",        XK_F1,       hpterm_kbd_F1},
    {"F2",        XK_F2,       hpterm_kbd_F2},
    {"F3",        XK_F3,       hpterm_kbd_F3},
    {"F4",        XK_F4,       hpterm_kbd_F4},
    {"F5",        XK_F5,       hpterm_kbd_F5},
    {"F6",        XK_F6,       hpterm_kbd_F6},
    {"F7",        XK_F7,       hpterm_kbd_F7},
    {"F8",        XK_F8,       hpterm_kbd_F8},
    {"Home",      XK_Home,     hpterm_kbd_Home},
    {"Left",      XK_Left,     hpterm_kbd_Left},
    {"Right",     XK_Right,    hpterm_kbd_Right},
    {"Down",      XK_Down,     hpterm_kbd_Down},
    {"Up",        XK_Up,       hpterm_kbd_Up},
    {"Prev",      XK_Prior,    hpterm_kbd_Prev},
    {"Next",      XK_Next,     hpterm_kbd_Next},
    {"Select",    XK_Select,   hpterm_kbd_Select},
    {"KP_Enter",  XK_KP_Enter, hpterm_kbd_KP_Enter},
    {"Enter",     XK_Execute,  hpterm_kbd_Enter},
    {"Clear",     XK_F12,      hpterm_kbd_Clear},
    {"PrintScrn", XK_Print,    dump_display},
/*
   **  Following group needed for HP 715 workstation
 */
#ifdef hpXK_Reset
    {"Reset",      hpXK_Reset,      hpterm_kbd_Reset},
    {"User",       hpXK_User,       hpterm_kbd_User},
    {"System",     hpXK_System,     hpterm_kbd_System},
    {"ClearLine",  hpXK_ClearLine,  hpterm_kbd_ClearLine},
    {"InsertLine", hpXK_InsertLine, hpterm_kbd_InsertLine},
    {"DeleteLine", hpXK_DeleteLine, hpterm_kbd_DeleteLine},
    {"InsertChar", hpXK_InsertChar, hpterm_kbd_InsertChar},
    {"DeleteChar", hpXK_DeleteChar, hpterm_kbd_DeleteChar},
    {"BackTab",    hpXK_BackTab,    hpterm_kbd_BackTab},
    {"KP_BackTab", hpXK_KP_BackTab, hpterm_kbd_KP_BackTab},
#endif
/*
   **  Following group needed for Tatung Mariner 4i with AT keyboard
 */
    {"F9",         XK_F9,     hpterm_kbd_Menu},
    {"F10",        XK_F10,    hpterm_kbd_User},
    {"F11",        XK_F11,    hpterm_kbd_System},
    {"Break",      XK_Pause,  hpterm_kbd_Break},
    {"PageUp",     XK_F29,    hpterm_kbd_Prev},
    {"PageDown",   XK_F35,    hpterm_kbd_Next},
    {"Home",       XK_F27,    hpterm_kbd_Home},
    {"End",        XK_F33,    hpterm_kbd_HomeDown},
    {"InsertChar", XK_Insert, hpterm_kbd_InsertChar},
    {"DeleteChar", XK_Delete, hpterm_kbd_DeleteChar},
    { 0,           0,         0}
};


int keymapper (KeySym keysym, unsigned int state, char *buffer, int charcount)
{
/*
   **  Attempt to map the key to a special function
   **  If key maps, call the function and return 1
   **  If key does not map, return 0
 */
  int ii;

/*
   **  Following group needed for HP 715 workstation
 */
  if (state & ShiftMask)
  {
    if (keysym == XK_Up)
    {
      hpterm_kbd_RollUp ();
      return (1);
    }
    if (keysym == XK_Down)
    {
      hpterm_kbd_RollDown ();
      return (1);
    }
    if (keysym == XK_Home)
    {
      hpterm_kbd_HomeDown ();
      return (1);
    }
  }
/*
   **  Following group needed for Tatung Mariner 4i with AT keyboard
 */
  if (state & ShiftMask)
  {
    if (keysym == XK_KP_8)
    {
      hpterm_kbd_RollUp ();
      return (1);
    }
    if (keysym == XK_KP_2)
    {
      hpterm_kbd_RollDown ();
      return (1);
    }
    if (keysym == XK_Tab)
    {
      hpterm_kbd_BackTab ();
      return (1);
    }
  }
/*
 **  Following to allow Reflections compatible ALT accelerators
 **   ...but is doesn't work...  fixed!
 */
  if (state & Mod1Mask)
  {
    if (keysym == XK_1)
    {
      hpterm_kbd_F1 ();
      return (1);
    }
    else if (keysym == XK_2)
    {
      hpterm_kbd_F2 ();
      return (1);
    }
    else if (keysym == XK_U || keysym == XK_u)
    {
      hpterm_kbd_Menu ();
      return (1);
    }
    else if (keysym == XK_M || keysym == XK_m)
    {
      hpterm_kbd_Modes ();
      return (1);
    }
    else if (keysym == XK_S || keysym == XK_s)
    {
      hpterm_kbd_System ();
      return (1);
    }
    else if (keysym == XK_J || keysym == XK_j)
    {
      hpterm_kbd_Clear ();
      return (1);
    }
    else if (keysym == XK_R || keysym == XK_r)
    {
      hpterm_kbd_Reset ();
      return (1);
    }
    else if (keysym == XK_D || keysym == XK_d)
    {
      hpterm_kbd_DeleteLine ();
      return (1);
    }
    else if (keysym == XK_I || keysym == XK_i)
    {
      hpterm_kbd_InsertLine ();
      return (1);
    }
    else if (keysym == XK_P || keysym == XK_p)
    {
      dump_display ();
      return (1);
    }
  }
/*
 **  No special cases - now search the table
 */
  for (ii = 0; keymap[ii].keyname; ii++)
  {
    if (keymap[ii].keysym == keysym)
    {
      (*(keymap[ii].keyfunc)) ();
      return (1);
    }
  }
/*
   **  Following to allow use of lower case letters with CapsLock on
   **  and using the Shift key in combination with a letter 
 */
  if ((state & LockMask) && (state & ShiftMask) && (charcount == 1))
  {
    if ((keysym >= XK_A) && (keysym <= XK_Z))
      buffer[0] = tolower (buffer[0]);
  }
  return (0);
}

void disp_drawtext (
     int style,			/* Low order 4 bits of display enhancements escape code */
     int row,			/* Row number of 1st char of string, 0..23 (or more) */
     int col,			/* Column number of 1st char of string, 0..79 (or more) */
     char *buf,			/* String to display */
     int nbuf)			/* Number of chars to display */
{
  int font_height, font_width;
  GC gc;

  font_height = font_info->ascent + font_info->descent;
  font_width = font_info->max_bounds.width;

  if (style & HPTERM_BLINK_MASK)
  {
    gc = gc_red;
  }
  else if (style & HPTERM_HALFBRIGHT_MASK)
  {
    gc = gc_halfbright;
  }
  else
  {
    gc = gc_normal;
  }
  if (style & HPTERM_INVERSE_MASK)
  {
    XFillRectangle (display, win, gc,
		    col * font_width,
		    row * font_height,
		    nbuf * font_width,
		    1 * font_height);
    gc = gc_inverse;
  }

  XDrawString (display, win, gc, col * font_width,
	       font_info->ascent + row * font_height, buf, nbuf);

  if (style & HPTERM_UNDERLINE_MASK)
  {
    XFillRectangle (display, win, gc,
		    col * font_width,
		    row * font_height + font_info->ascent + 1,
		    nbuf * font_width, 1);
  }
/*
   **      Simulate half-bright attribute by re-drawing string one pixel
   **      to the right -- This creates a phony 'bold' effect
 */
#if NO_COLOR
  if (style & HPTERM_HALFBRIGHT_MASK)
  {
    XDrawString (display, win, gc, col * font_width + 1,
		 font_info->ascent + row * font_height, buf, nbuf);
  }
#endif
}


void disp_erasetext (int row, int col, int nchar)
{
  int font_height, font_width;

  font_width = (font_info->max_bounds.width);
  font_height = font_info->ascent + font_info->descent;

  XFillRectangle (display, win, gc_inverse,
		  col * font_width, row * font_height,
		  nchar * font_width, font_height);
}

void disp_drawcursor (int style, int row, int col)
{
  int font_height, font_width;

  font_height = font_info->ascent + font_info->descent;
  font_width = font_info->max_bounds.width;
  if (style & HPTERM_INVERSE_MASK)
  {

    XFillRectangle (display, win, gc_inverse, col * font_width,
		    font_info->ascent + (row * font_height) + 1,
		    font_width, 2);
  }
  else
  {

    XFillRectangle (display, win, gc_normal, col * font_width,
		    font_info->ascent + (row * font_height) + 1,
		    font_width, 2);
  }
}

void  doXBell (void)
{
  int strength = 50;
  XBell (display, strength);
}



void Usage (void)
{ /*Usage */

  printf ("xhpterm version %s\n\n",version_str);
  printf ("Usage: xhpterm [opts] [-rlogin] <hostname>\n");
  printf ("   or: xhpterm [opts] -tty <devicefile> -speed <cps> -parity {E|O|N}\n");
  printf ("opts:\n");
  printf ("   -li|-lo|-lio    - specify input|output logging options\n");
  printf ("   -lp             - logging output has a prefix\n");
  printf ("   -termid str     - override terminal ID [X-hpterm]\n");
  printf ("   -clean          - right-click exit disabled\n");
  printf ("   -f file         - destination for logging (default: stdout)\n");
  printf ("   -a file         - read initial commands from file.\n");
  printf ("   -df             - start with Display Functions enabled.\n");
  printf ("   -font fontname  - override default font with fontname.\n");
  printf ("   -title title    - override default window title.\n");

} /*Usage */


int main (int argc, char **argv)
{
  int
    use_rlogin = 0, display_fns = 0, speed = 9600, parm_error = 0, log_mask = 0;
  char
   parity = 'N', *input_file = NULL, *hostname = NULL, *log_file = NULL, *ttyname = NULL;
  int
    ipPort = kVT_PORT;

  char *font1 = NULL;
  char *wintitle = NULL;
  char *wtprefix = "FreeVT3K Terminal Emulator";

  /* Start the datacomm module */
  ++argv;
  while ((--argc) && ((*argv)[0] == '-'))
  {
    if (!strcmp (*argv, "-help"))
    {
      Usage ();
      return (0);
    }
    if (!strcmp (*argv, "-rlogin"))
    {
      if ((--argc) && (argv[1][0] != '-'))
      {
	use_rlogin = 1;
	hostname = *(++argv);
      }
      else
	++parm_error;
    }
    else if (!strcmp (*argv, "-clean"))
      must_logoff = 1;
    else if (!strcmp (*argv, "-df"))
      display_fns = 1;
    else if (!strcmp (*argv, "-tty"))
    {
      if ((--argc) && (argv[1][0] != '-'))
	ttyname = *(++argv);
      else
	++parm_error;
    }
    else if (!strcmp (*argv, "-speed"))
    {
      if ((--argc) && (argv[1][0] != '-'))
      {
	++argv;
	speed = atoi (*argv);
      }
      else
	++parm_error;
    }
    else if (!strcmp (*argv, "-parity"))
    {
      if ((--argc) && (argv[1][0] != '-'))
      {
	char temp[32];
	++argv;
	parity = *argv[0];
	if (islower (parity))
	  parity = (char) toupper (parity);
	strcpy (temp, "EON");
	if (strchr (temp, parity) == (char *) NULL)
	  ++parm_error;
      }
      else
	++parm_error;
    }
    else if (!strcmp (*argv, "-termid"))
    {
      if ((--argc) && (argv[1][0] != '-'))
	termid = *(++argv);
      else
	++parm_error;
    }
    else if (!strcmp (*argv, "-font") || !strcmp (*argv, "-fn"))
    {
      if ((--argc))
	font1 = *(++argv);
      else
	++parm_error;
    }
    else if (!strcmp (*argv, "-T") || !strcmp(*argv, "-title"))
    {
      if ((--argc))
	wintitle = *(++argv);
      else
	++parm_error;
    }
    else if (!strcmp (*argv, "-a"))
    {
      if ((--argc) && (argv[1][0] != '-'))
	input_file = *(++argv);
      else
	++parm_error;
    }
    else if (!strcmp (*argv, "-f"))
    {
      if ((--argc) && (argv[1][0] != '-'))
	log_file = *(++argv);
      else
	++parm_error;
    }
    else if (!strncmp (*argv, "-l", 2))
    {
      char *ptr;
      ptr = *argv;
      ptr += 2;
      log_mask = ParseLogMask(ptr);
      if (log_mask == -1) {
        ++parm_error;
        break;
      }
    }
    else if ((strcmp(*argv, "-X") == 0) ||
	     (strcmp(*argv, "-otable") == 0) ||
	     (strcmp(*argv, "-table") == 0))
      {
	int i_type = 1;
	if (strcmp(*argv, "-otable") == 0)
	  ++i_type;
	if (--argc)
	  {
	    ++argv;
	    if (*argv[0] == '-')
	      ++parm_error;
	    else if (LoadKeybdTable(*argv, i_type))
	      ++parm_error;
	  }
	else
	  ++parm_error;
      }
    else if (!strcmp(*argv, "-p"))
      {
	if (--argc)
	  {
	    ++argv;
	    if (*argv[0] == '-')
	      ++parm_error;
	    else
	      ipPort = atoi(*argv);
	  }
	else
	  ++parm_error;
      }
    else
      ++parm_error;
    if (parm_error)
      break;
    ++argv;
  }

  if (parm_error)
  {
    fprintf (stderr, "Invalid parm [%s]\n", *argv);
    Usage ();
    return (1);
  }

  if (argc)
  {
    hostname = *argv;
  }

  if (wintitle == NULL)
  {
    if (hostname != NULL)
    {
      wintitle = calloc (1, strlen(hostname) + 3 + strlen(wtprefix));
      if (wintitle == NULL)
      {
	fprintf(stderr, "wintitle calloc\n");
	return 1;
      }
      sprintf (wintitle, "%s: %s", wtprefix, hostname);
    }
    else if (ttyname != NULL)
    {
      wintitle = calloc (1, strlen(ttyname) + 3 + strlen(wtprefix));
      if (wintitle == NULL)
      {
	fprintf(stderr, "wintitle calloc\n");
	return 1;
      }
      sprintf (wintitle, "%s: %s", wtprefix, ttyname);
    }
    else {
      wintitle = wtprefix;
    }
  }
  
  /* Start the X11 driver */
  init_disp (argc, argv, wintitle, font1);

  /* Start the terminal emulator */
  term = init_hpterm ();
  if (display_fns)
    set_display_functions ();

  if (input_file)
  {
    FILE *input;
    char buf[128], *ptr;
    if ((input = fopen (input_file, "r")) == (FILE *) NULL)
    {
      char buf[128];
      sprintf (buf, "fopen [%s]:", input_file);
      perror (buf);
      return (1);
    }
    for (;;)
    {
      if (fgets (buf, sizeof (buf) - 1, input) == NULL)
	break;
      ptr = buf;
      while (*ptr)
      {
	if (*ptr == '\n')
	  PutQ ('\r');
	else
	  PutQ (*ptr);
	++ptr;
      }
    }
    fclose (input);
  }

  if (LogOpen(log_file, log_mask) != 0) {
    return 1;
  }

  if (ttyname)
  {
    char ttyinfo[256];
    sprintf (ttyinfo, "%s|%d|%c", ttyname, speed, parity);
    con = conmgr_connect (e_tty, ttyinfo, 0);
  }
  else if (hostname)
  {
    if (use_rlogin)
	con = conmgr_connect (e_rlogin, hostname, 0);
    else
        con = conmgr_connect (e_vt3k, hostname, ipPort);
  }
  else
  {
    fprintf (stderr, "Missing hostname\n");
    Usage ();
    con = 0;
  }

  term->dccon = con;
  
  if (!con)
    return (1);

  event_loop ();

  XUnloadFont (display, font_info->fid);
  XFreeGC (display, gc_normal);
  XFreeGC (display, gc_inverse);
  XFreeGC (display, gc_halfbright);
  XFreeGC (display, gc_red);
  XCloseDisplay (display);

  conmgr_close (con);
}				/* main */
