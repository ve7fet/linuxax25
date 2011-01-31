/*****************************************************************************/

/*
 *	smdiag.c  -- kernel soundcard radio modem driver diagnostics utility.
 *
 *	Copyright (C) 1996,1997,2000  Thomas Sailer (t.sailer@alumni.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 *
 * History:
 *   0.1  26.06.1996  Started
 *   0.2  11.05.1997  introduced hdrvcomm.h
 *   0.3  05.01.2000  glibc compile fixes
 */

/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <net/if.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "hdrvcomm.h"

/* ---------------------------------------------------------------------- */

static char *progname;
static Display *display = NULL;
static Window window;
static Pixmap pixmap;
static unsigned long col_zeroline;
static unsigned long col_background;
static unsigned long col_trace;
static GC gr_context;
static int xmul;

/* ---------------------------------------------------------------------- */

static int x_error_handler(Display *disp, XErrorEvent *evt)
{
    char err_buf[256], mesg[256], number[256];
    char *mtype = "XlibMessage";

    XGetErrorText(disp, evt->error_code, err_buf, sizeof(err_buf));
    fprintf(stderr, "X Error: %s\n", err_buf);
    XGetErrorDatabaseText(disp, mtype, "MajorCode", "Request Major code %d", 
			  mesg, sizeof(mesg));
    fprintf(stderr, mesg, evt->request_code);
    sprintf(number, "%d", evt->request_code);
    XGetErrorDatabaseText(disp, "XRequest", number, "", err_buf, 
			  sizeof(err_buf));
    fprintf(stderr, " (%s)\n", err_buf);
    abort();
}

/* ---------------------------------------------------------------------- */

#define WIDTH 512
#define HEIGHT (constell ? 512 : 256)

static int openwindow(char *disp, int constell, int samplesperbit)
{
        XSetWindowAttributes attr;
        XGCValues gr_values;
        XColor color, dummy;
        XSizeHints sizehints;

        if (!(display = XOpenDisplay(NULL)))
                return -1;
        XSetErrorHandler(x_error_handler);
        XAllocNamedColor(display, DefaultColormap(display, 0), "red",
                         &color, &dummy);
	col_zeroline = color.pixel;
	col_background = WhitePixel(display, 0);
	col_trace = BlackPixel(display, 0);
        attr.background_pixel = col_background;
        if (!(window = XCreateWindow(display, XRootWindow(display, 0), 
				     200, 200, WIDTH, HEIGHT, 5, 
				     DefaultDepth(display, 0), 
				     InputOutput, DefaultVisual(display, 0),
				     CWBackPixel, &attr))) {
		fprintf(stderr, "smdiag: unable to open X window\n");
		exit(1);
	}
	if (!(pixmap = XCreatePixmap(display, window, WIDTH, HEIGHT,
				     DefaultDepth(display, 0)))) {
		fprintf(stderr, "smdiag: unable to open offscreen pixmap\n");
		exit(1);
	}
        xmul = WIDTH / (2*(samplesperbit > 0 ? samplesperbit : 1));
        XSelectInput(display, window, KeyPressMask | StructureNotifyMask
		     | ExposureMask) ;
        XAllocNamedColor(display, DefaultColormap(display, 0), "red",
                         &color, &dummy);
        gr_values.foreground = col_trace;
        gr_values.line_width = 1;
        gr_values.line_style = LineSolid;
        gr_context = XCreateGC(display, window, GCForeground | GCLineWidth |
                               GCLineStyle, &gr_values);
        XStoreName(display, window, "diagnostics");
        /*
         * Do not allow the window to be resized
         */
        memset(&sizehints, 0, sizeof(sizehints));
        sizehints.min_width = sizehints.max_width = WIDTH;
        sizehints.min_height = sizehints.max_height = HEIGHT;
        sizehints.flags = PMinSize | PMaxSize;
        XSetWMNormalHints(display, window, &sizehints);
        XMapWindow(display, window);
        XSynchronize(display, 1);
        return 0;
}

#undef WIDTH
#undef HEIGHT

/* ---------------------------------------------------------------------- */

static void closewindow(void)
{
        if (!display)
                return;
        XDestroyWindow(display, window);
        XCloseDisplay(display);
        display = NULL;
}

/* ---------------------------------------------------------------------- */

#define XCOORD(x) ((x) * xm)
#define YCOORD(y) ((SHRT_MAX - (int)(y)) * winattrs.height / USHRT_MAX)

static void drawdata(short *data, int len, int replace, int xm)
{
	int cnt;
        GC gc;
        XGCValues gcv;
        XWindowAttributes winattrs;	

        if (!display || !pixmap)
                return;
        XGetWindowAttributes(display, window, &winattrs);
	gcv.line_width = 1;
        gcv.line_style = LineSolid;
        gc = XCreateGC(display, pixmap, GCLineWidth | GCLineStyle, &gcv);
        XSetState(display, gc, col_background, col_background, GXcopy, 
		  AllPlanes);
	if (replace)
		XFillRectangle(display, pixmap, gc, 0, 0, 
			       winattrs.width, winattrs.height);
	else
		XCopyArea(display, window, pixmap, gr_context, 0, 0, 
			  winattrs.width, winattrs.height, 0, 0);
        XSetForeground(display, gc, col_trace);
	for (cnt = 0; cnt < len-1; cnt++)
		XDrawLine(display, pixmap, gc, XCOORD(cnt), YCOORD(data[cnt]),
			  XCOORD(cnt+1), YCOORD(data[cnt+1]));
        XSetForeground(display, gc, col_zeroline);
	XDrawLine(display, pixmap, gc, 0, YCOORD(0), winattrs.width,
		  YCOORD(0));
	XCopyArea(display, pixmap, window, gr_context, 0, 0, winattrs.width,
                  winattrs.height, 0, 0);
        XFreeGC(display, gc);
	XSync(display, 0);
}

#undef XCOORD
#undef YCOORD

/* ---------------------------------------------------------------------- */

#define XCOORD(x) ((SHRT_MAX - (int)(x)) * winattrs.width / USHRT_MAX)
#define YCOORD(y) ((SHRT_MAX - (int)(y)) * winattrs.height / USHRT_MAX)

static void drawconstell(short *data, int len)
{
	int cnt;
        GC gc;
        XGCValues gcv;
        XWindowAttributes winattrs;	

        if (!display || !pixmap)
                return;
        XGetWindowAttributes(display, window, &winattrs);
	gcv.line_width = 1;
        gcv.line_style = LineSolid;
        gc = XCreateGC(display, pixmap, GCLineWidth | GCLineStyle, &gcv);
        XSetState(display, gc, col_background, col_background, GXcopy, 
		  AllPlanes);
	XCopyArea(display, window, pixmap, gr_context, 0, 0, 
		  winattrs.width, winattrs.height, 0, 0);
        XSetForeground(display, gc, col_trace);
	for (cnt = 0; cnt < len-1; cnt += 2)
		XDrawPoint(display, pixmap, gc, 
			   XCOORD(data[cnt]), YCOORD(data[cnt+1]));
        XSetForeground(display, gc, col_zeroline);
	XDrawLine(display, pixmap, gc, 0, YCOORD(0), winattrs.width, YCOORD(0));
	XDrawLine(display, pixmap, gc, XCOORD(0), 0, XCOORD(0), winattrs.height);
	XCopyArea(display, pixmap, window, gr_context, 0, 0, winattrs.width,
                  winattrs.height, 0, 0);
        XFreeGC(display, gc);
	XSync(display, 0);
}

#undef XCOORD
#undef YCOORD

/* ---------------------------------------------------------------------- */

static void clearwindow(void)
{
        XWindowAttributes winattrs;	
	GC gc;
        XGCValues gcv;

        if (!display || !pixmap)
                return;
        XGetWindowAttributes(display, window, &winattrs);
	gcv.line_width = 1;
        gcv.line_style = LineSolid;
        gc = XCreateGC(display, pixmap, GCLineWidth | GCLineStyle, &gcv);
        XSetState(display, gc, col_background, col_background, GXcopy, 
		  AllPlanes);
	XFillRectangle(display, pixmap, gc, 0, 0, 
		       winattrs.width, winattrs.height);
        XSetForeground(display, gc, col_zeroline);
        XClearArea(display, window, 0, 0, 0, 0, False);
	XCopyArea(display, pixmap, window, gr_context, 0, 0, winattrs.width,
                  winattrs.height, 0, 0);
        XFreeGC(display, gc);	
}

/* ---------------------------------------------------------------------- */

static Bool predicate(Display *display, XEvent *event, char *arg)
{
        return True;
}

/* ---------------------------------------------------------------------- */

static char *getkey(void)
{
        XWindowAttributes winattrs;	
        XEvent evt;
        static char kbuf[32];
        int i;

        if (!display)
                return NULL;
	XSync(display, 0);
        while (XCheckIfEvent(display, &evt, predicate, NULL)) {
		switch (evt.type) {
		case KeyPress:
			i = XLookupString((XKeyEvent *)&evt, kbuf, sizeof(kbuf)-1, 
					  NULL, NULL);
			if (!i)
				return NULL;
			kbuf[i] = 0;
			return kbuf;
		case DestroyNotify:
			XCloseDisplay(display);
			display = NULL;
			return NULL;
		case Expose:
			XGetWindowAttributes(display, window, &winattrs);
			XCopyArea(display, pixmap, window, gr_context, 0, 0, 
				  winattrs.width, winattrs.height, 0, 0);
			break;
		default:
			break;
		}
        }
        return NULL;
}

/* ---------------------------------------------------------------------- */

static void printmode(unsigned int mode, unsigned int trigger)
{
	printf("Source: %s%s\n", (mode == SM_DIAGMODE_DEMOD) ?
	       "demodulator (eye diagram)" : "input (oscilloscope)",
	       (trigger & SM_DIAGFLAG_DCDGATE) ? " gated with DCD" : "");
}

/* ---------------------------------------------------------------------- */

static const char *usage_str = 
"[-d display] [-i smif] [-c] [-e]\n"
"  -d: display host\n"
"  -i: specify the name of the baycom kernel driver interface\n"
"  -c: toggle carrier trigger\n"
"  -e: eye diagram mode\n\n"
"  -p: constellation plot\n\n";

int main(int argc, char *argv[])
{
	char *disp = NULL;
	unsigned int mode = HDRVC_DIAGMODE_INPUT;
	unsigned int trigger = 0;
	unsigned int ifflags;
	short data[256];
	char *cp;
	int ret;
	unsigned int samplesperbit;

	progname = *argv;
	printf("%s: Version 0.2; (C) 1996-1997 by Thomas Sailer HB9JNX/AE4WA\n", *argv);
	hdrvc_args(&argc, argv, "sm0");
	while ((ret = getopt(argc, argv, "d:ecp")) != -1) {
		switch (ret) {
		case 'd':
			disp = optarg;
			break;
		case 'e':
			mode = HDRVC_DIAGMODE_DEMOD;
			trigger = HDRVC_DIAGFLAG_DCDGATE;
			break;
		case 'c':
			trigger ^= HDRVC_DIAGFLAG_DCDGATE;
			break;
		case 'p':
			mode = HDRVC_DIAGMODE_CONSTELLATION;
			break;
		default:
			printf("usage: %s %s", *argv, usage_str);
			exit(-1);
		}
	}
	hdrvc_init();
	ifflags = hdrvc_get_ifflags();
	if (!(ifflags & IFF_UP)) {
		fprintf(stderr, "interface %s down\n", hdrvc_ifname());
		exit(1);
	}
	if (!(ifflags & IFF_RUNNING)) {
		fprintf(stderr, "interface %s not running\n", hdrvc_ifname());
		exit(1);
	}
	printmode(mode, trigger);
	for (;;) {
		if ((ret = hdrvc_diag2(mode, trigger, data, sizeof(data) / sizeof(short),
				       &samplesperbit)) < 0) {
			perror("hdrvc_diag2");
			exit(1);
		}
		if (ret > 0) {
			if (!display) {
				openwindow(disp, mode == HDRVC_DIAGMODE_CONSTELLATION,
					   samplesperbit);
				clearwindow();
			}
			if (mode == SM_DIAGMODE_CONSTELLATION)
				drawconstell(data, ret);
			else
				drawdata(data, ret, mode == HDRVC_DIAGMODE_INPUT,
					 mode == HDRVC_DIAGMODE_INPUT ? 5:xmul);
		} else
			usleep(100000L);
		if (display) {
			if ((cp = getkey())) {
				for (; *cp; cp++) {
					printf("char pressed: '%c'\n", *cp);
					switch (*cp) {
					case 'c':
					case 'C':
						clearwindow();
						printmode(mode, trigger);
						break;
					case 'q':
					case 'Q':
						closewindow();
						break;
					case 'i':
					case 'I':
						if (mode == HDRVC_DIAGMODE_CONSTELLATION)
							break;
						mode = HDRVC_DIAGMODE_INPUT;
						clearwindow();
						printmode(mode, trigger);
						break;
					case 'e':
					case 'E':
						if (mode == HDRVC_DIAGMODE_CONSTELLATION)
							break;
						mode = HDRVC_DIAGMODE_DEMOD;
						clearwindow();
						printmode(mode, trigger);
						break;
					case 'd':
					case 'D':
						trigger ^= HDRVC_DIAGFLAG_DCDGATE;
						clearwindow();
						printmode(mode, trigger);
						break;
					}
				}
			}
			if (!display)
				exit(0);
		}
	}
}

/* ---------------------------------------------------------------------- */
