/*****************************************************************************/

/*
 *	xfsmdiag_main.h  -- kernel soundcard radio modem driver diagnostics utility.
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

#ifndef _XFSMDIAG_MAIN_H
#define _XFSMDIAG_MAIN_H

/* ---------------------------------------------------------------------- */

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/x.H>

/* ---------------------------------------------------------------------- */

class scope : public Fl_Box {
	Pixmap pixmap;
	bool pixmalloc;
	int X, Y, W, H;
	unsigned long col_zeroline;
	unsigned long col_background;
	unsigned long col_trace;
	int drawmode;

protected:
	void draw();
	void resize(int, int, int, int);

public:
	scope(int, int, int, int, const char * = 0);
	~scope();

	void drawdata(short data[], int len, int xm);
	void mode(int dmode);
	int mode(void);
	void clear(void);
};

/* ---------------------------------------------------------------------- */
#endif /* _XFSMDIAG_MAIN_H */
