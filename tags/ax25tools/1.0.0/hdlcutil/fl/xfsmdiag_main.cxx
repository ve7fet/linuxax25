/*****************************************************************************/

/*
 *	xfsmdiag.c  -- kernel soundcard radio modem driver diagnostics utility.
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
 *   0.1  14.12.1996  Started
 *   0.2  11.05.1997  introduced hdrvcomm.h
 *   0.3  05.01.2000  upgraded to fltk 1.0.7
 */

/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>
#include <net/if.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "hdrvcomm.h"
#include "xfsmdiag.h"
#include <FL/x.H>
#include <FL/fl_draw.H>

/* ---------------------------------------------------------------------- */

static char *progname;
static unsigned int drawflags = 0;

/* ---------------------------------------------------------------------- */

scope::scope(int x, int y, int w, int h, const char *l)
	: Fl_Box(x, y, w, h, l) 
{
	box(FL_DOWN_FRAME);
	X = x+3;
	Y = y+4;
	W = w-6;
	H = h-8;
	drawmode = HDRVC_DIAGMODE_OFF;
	pixmalloc = false;
	hide();
}

scope::~scope()
{
	if (pixmalloc) {
		XFreePixmap(fl_display, pixmap);
		pixmalloc = false;
	}
}

void scope::resize(int xx, int yy, int ww, int hh)
{
	if (pixmalloc && (ww != w() || hh != h())) {
		XFreePixmap(fl_display, pixmap);
		pixmalloc = false;
	}
	X = xx+3;
	Y = yy+4;
	W = ww-6;
	H = hh-8;
	Fl_Box::resize(xx, yy, ww, hh);
}

void scope::draw()
{
	GC gc;
        XGCValues gcv;

	/* cannot use draw_box(); as it clears the whole window -> flicker */
	/* from fl_boxtype.C, fl_down_frame */
	if (W > 0 && H > 0) {
		fl_color(FL_DARK1); fl_xyline(X-3, Y-4, X+W+1);
		fl_color(FL_DARK2); fl_yxline(X-3, Y+H+3, Y-3, X+W+1);
		fl_color(FL_DARK3); fl_yxline(X-2, Y+H+2, Y-2, X+W);
		fl_color(FL_LIGHT3); fl_xyline(X-1, Y+H+1, X+W+1, Y-2);
		fl_color(FL_LIGHT2); fl_xyline(X-2, Y+H+2, X+W+2, Y-4);
		fl_color(FL_LIGHT1); fl_xyline(X-3, Y+H+3, X+W+2);
	}
	draw_label();
	if (pixmalloc)
		XCopyArea(fl_display, pixmap, fl_window, fl_gc, 0, 0, W, H, X, Y);
	else {
		gcv.line_width = 1;
		gcv.line_style = LineSolid;
		gcv.fill_style = FillSolid;
		gc = XCreateGC(fl_display, pixmap, GCLineWidth | GCLineStyle | GCFillStyle, &gcv);
		XSetState(fl_display, gc, col_background, col_background, GXcopy, AllPlanes);
		XFillRectangle(fl_display, fl_window, gc, X, Y, W, H);
		XFreeGC(fl_display, gc);
	}
	XSync(fl_display, 0);
}

/* ---------------------------------------------------------------------- */

int scope::mode(void)
{
	mode(drawmode);
	return drawmode;
}

void scope::mode(int dmode)
{
	Fl_Window *wnd;

	if (dmode != drawmode) {
		if (pixmalloc) {
			XFreePixmap(fl_display, pixmap);
			pixmalloc = false;
		}
		drawmode = dmode;
		switch (drawmode) {
		case HDRVC_DIAGMODE_OFF:
		default:
			hide();
			break;

		case HDRVC_DIAGMODE_INPUT:
		case HDRVC_DIAGMODE_DEMOD:
			size(512+6, 256+8);
			show();
			break;

		case HDRVC_DIAGMODE_CONSTELLATION:
			size(512+6, 512+8);
			show();
			break;
		}
	}
	if (pixmalloc || !visible() || W < 2 || H < 2)
		return;
	if (!(wnd = window()))
		return;
	wnd->make_current();
	if (!(pixmap = XCreatePixmap(fl_display, fl_window, W, H, fl_visual->depth))) {
		fprintf(stderr, "unable to open offscreen pixmap\n");
		exit(1);
	}
	pixmalloc = true;
	col_zeroline = fl_xpixel(FL_RED);
	col_background = fl_xpixel(FL_WHITE);
	col_trace = fl_xpixel(FL_BLACK);
	clear();
}

/* ---------------------------------------------------------------------- */

void scope::clear(void)
{
	GC gc;
        XGCValues gcv;

	if (!pixmalloc)
		return;
	gcv.line_width = 1;
        gcv.line_style = LineSolid;
	gcv.fill_style = FillSolid;
        gc = XCreateGC(fl_display, pixmap, GCLineWidth | GCLineStyle | GCFillStyle, &gcv);
        XSetState(fl_display, gc, col_background, col_background, GXcopy, AllPlanes);
	XFillRectangle(fl_display, pixmap, gc, 0, 0, W, H);
        XFreeGC(fl_display, gc);
	redraw();
}

/* ---------------------------------------------------------------------- */

#define WIDTH 512

/* ---------------------------------------------------------------------- */

void scope::drawdata(short data[], int len, int xm)
{
	int cnt;
        GC gc;
        XGCValues gcv;

	mode();
	if (!pixmalloc || (drawmode != HDRVC_DIAGMODE_CONSTELLATION && 
			   drawmode != HDRVC_DIAGMODE_INPUT && 
			   drawmode != HDRVC_DIAGMODE_DEMOD))
		return;
	gcv.line_width = 1;
        gcv.line_style = LineSolid;
        gc = XCreateGC(fl_display, pixmap, GCLineWidth | GCLineStyle, &gcv);
	if (drawmode == HDRVC_DIAGMODE_CONSTELLATION) {
#define XCOORD(x) ((SHRT_MAX - (int)(x)) * W / USHRT_MAX)
#define YCOORD(y) ((SHRT_MAX - (int)(y)) * H / USHRT_MAX)
		XSetState(fl_display, gc, col_background, col_background, GXcopy, AllPlanes);
		XSetForeground(fl_display, gc, col_trace);
		for (cnt = 0; cnt < len-1; cnt += 2)
			XDrawPoint(fl_display, pixmap, gc, XCOORD(data[cnt]), YCOORD(data[cnt+1]));
		XSetForeground(fl_display, gc, col_zeroline);
		XDrawLine(fl_display, pixmap, gc, 0, YCOORD(0), W, YCOORD(0));
		XDrawLine(fl_display, pixmap, gc, XCOORD(0), 0, XCOORD(0), H);
#undef XCOORD
#undef YCOORD
	} else {
#define XCOORD(x) ((x) * xm)
#define YCOORD(y) ((SHRT_MAX - (int)(y)) * H / USHRT_MAX)
		XSetState(fl_display, gc, col_background, col_background, GXcopy, AllPlanes);
		if (drawmode == HDRVC_DIAGMODE_INPUT) {
			XFillRectangle(fl_display, pixmap, gc, 0, 0, W, H);
			xm = 5;
		}
		XSetForeground(fl_display, gc, col_trace);
		for (cnt = 0; cnt < len-1; cnt++)
			XDrawLine(fl_display, pixmap, gc, XCOORD(cnt), YCOORD(data[cnt]),
				  XCOORD(cnt+1), YCOORD(data[cnt+1]));
		XSetForeground(fl_display, gc, col_zeroline);
		XDrawLine(fl_display, pixmap, gc, 0, YCOORD(0), W, YCOORD(0));
#undef XCOORD
#undef YCOORD
	}
        XFreeGC(fl_display, gc);
	redraw();
}

/* ---------------------------------------------------------------------- */

void cb_cleargr(Fl_Button *, long)
{
	scdisp->clear();
}

/* ---------------------------------------------------------------------- */

void cb_mode(Fl_Check_Button *, long which)
{
	struct sm_diag_data diag;
	short data;

	diag.mode = HDRVC_DIAGMODE_OFF;
	diag.flags = 0;
	diag.datalen = 1;
	diag.data = &data;
	hdrvc_diag(&diag);
	switch (which) {
	case 256:
		drawflags ^= HDRVC_DIAGFLAG_DCDGATE;
		break;
		
	case 0:
		scdisp->mode(HDRVC_DIAGMODE_OFF);
		drawflags = 0;
		break;

	case 1:
		scdisp->mode(HDRVC_DIAGMODE_INPUT);
		drawflags = 0;
		break;

	case 2:
		scdisp->mode(HDRVC_DIAGMODE_DEMOD);
		drawflags = HDRVC_DIAGFLAG_DCDGATE;
		break;

	case 3:
		scdisp->mode(HDRVC_DIAGMODE_CONSTELLATION);
		drawflags = HDRVC_DIAGFLAG_DCDGATE;
		break;
	}
	sm_dcd->value(!!(drawflags & HDRVC_DIAGFLAG_DCDGATE));
}

/* ---------------------------------------------------------------------- */

void cb_quit(Fl_Button *, long)
{
	struct sm_diag_data diag;
	short data;
	
	diag.mode = HDRVC_DIAGMODE_OFF;
	diag.flags = 0;
	diag.datalen = 1;
	diag.data = &data;
	hdrvc_diag(&diag);
	exit(0);
}

/* ---------------------------------------------------------------------- */

void cb_timer(void *)
{
	struct hdrvc_channel_state cs;
	int ret;
	short data[256];
	unsigned int samplesperbit;

	Fl::add_timeout(0.2, cb_timer);
	/*
	 * display state
	 */
	ret = hdrvc_get_channel_state(&cs);
	if (ret < 0) {
		perror("hdrvc_get_channel_state");
	} else {
		st_ptt->value(cs.ptt);
		st_dcd->value(cs.dcd);
	}
	/*
	 * draw scope
	 */
	if ((ret = hdrvc_diag2(scdisp->mode(), drawflags, data, sizeof(data) / sizeof(short), 
			       &samplesperbit)) < 0) {
		perror("hdrvc_diag2");
		exit(1);
	}
	if (ret > 0)
		scdisp->drawdata(data, ret, WIDTH / (2*(samplesperbit > 0 ? samplesperbit : 1)));
}

/* ---------------------------------------------------------------------- */

static const char *usage_str = 
"[-i smif]\n"
"  -i: specify the name of the baycom kernel driver interface\n\n";

/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
        int c, i;
	int ret;
	unsigned int ifflags;
	char name[64];

	progname = *argv;
	printf("%s: Version 0.3; (C) 1996,1997,2000 by Thomas Sailer HB9JNX/AE4WA\n", *argv);

	hdrvc_args(&argc, argv, "sm0");
	for (i = 1; i < argc; ) {
		c = i;
		Fl::arg(argc, argv, c);
		if (c <= 0) {
			i++;
			continue;
		}
		memmove(&argv[i], &argv[i+c], (argc-i) * sizeof(char *));
		argc -= c;
	}
#if 0
	while ((ret = getopt(argc, argv, "")) != -1) {
		switch (ret) {
		default:
			printf("usage: %s %s", *argv, usage_str);
			exit(-1);
		}
	}
#endif
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
	create_the_forms();
	scdisp->hide();
	/*
	 * set driver and modem name
	 */
	ret = hdrvc_get_mode_name(name, sizeof(name));
	if (ret < 0) {
		perror("hdrvc_get_mode_name");
		modename->hide();
	} else 
		modename->value(name);
	ret = hdrvc_get_driver_name(name, sizeof(name));
	if (ret < 0) {
		perror("hdrvc_get_driver_name");
		drivername->hide();
	} else 
		drivername->value(name);
	Fl::add_timeout(0.1, cb_timer);
	scopewindow->show();
	Fl::run();
	cb_quit(quit, 0);
	exit(0);
}
