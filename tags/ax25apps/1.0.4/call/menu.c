/*
 * menu (c)1995 Alexander Tietzel (DG6XA) 
 * little Menu-System for use with ncurses
 * date        activity                             autor
 * 22.07.1995  wininfo->wint (vector->single chain) Alexander Tietzel (DG6XA)
 * 25.07.1995  some minor changes                   Alexander Tietzel (DG6XA)
 */

#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include "menu.h"

typedef struct {
	char *st_ptr;
	int xpos;
	char key;
} topmenuitem;

WINDOW *winopen(wint * wtab, int nlines, int ncols, int begin_y,
		int begin_x, int border)
{

	while (wtab->next != NULL)
		wtab = wtab->next;

	wtab->next = (wint *) malloc(sizeof(wint));
	wtab = wtab->next;

	wtab->next = NULL;

	wtab->ptr = newwin(nlines, ncols, begin_y, begin_x);

	if (wtab->ptr == NULL)
		return NULL;

	wtab->fline = begin_y;
	wtab->lline = begin_y + nlines;

	if (border)
		wborder(wtab->ptr, ACS_VLINE, ACS_VLINE, ACS_HLINE,
			ACS_HLINE, ACS_ULCORNER, ACS_URCORNER,
			ACS_LLCORNER, ACS_LRCORNER);

	return wtab->ptr;
}

void winclose(wint * wtab)
{
	wint *awin;
	wint *lwin;
	int awin_lines;

	if (wtab->next == NULL)
		return;

	lwin = wtab;
	while (lwin->next->next != NULL)
		lwin = lwin->next;
	awin = lwin->next;

	awin_lines = awin->lline - awin->fline;

	while (wtab->next != NULL) {
		if (wtab->fline >= 0 && wtab->lline >= 0 &&
		    awin->lline >= wtab->fline &&
		    awin->fline <= wtab->lline) {
			if (wtab->fline <= awin->fline) {
				if (wtab->lline < awin->lline) {
					wtouchln(wtab->ptr,
						 awin->fline - wtab->fline,
						 awin_lines -
						 (awin->lline -
						  wtab->lline), 1);
				} else {
					wtouchln(wtab->ptr,
						 awin->fline - wtab->fline,
						 awin_lines, 1);
				}
			} else {
				wtouchln(wtab->ptr, 0,
					 awin_lines - wtab->fline +
					 awin->fline, 1);
			}

			wnoutrefresh(wtab->ptr);
		}

		wtab = wtab->next;
	}

	doupdate();
	lwin->next = 0;
	free(awin);
}

void menu_write_line(WINDOW * win, int ypos, int menu_breite, int reverse,
		     char st[])
{
	int cnt;
	int high = FALSE;

	if (reverse)
		wattron(win, A_REVERSE);

	wmove(win, ypos + 1, 1);
	for (cnt = 0; st[cnt] != 0; cnt++) {
		if (st[cnt] == '~') {
			if (!reverse) {
				wattron(win, A_BOLD);
				high = TRUE;
			}
		} else {
			waddch(win, st[cnt]);
			if (high == TRUE) {
				high = FALSE;
				wattroff(win, A_BOLD);
			}
		}
	}

	for (cnt = strlen(st); cnt < menu_breite; cnt++)
		waddch(win, ' ');

	if (reverse)
		wattroff(win, A_REVERSE);
}

int p_dwn_menu(wint * wtab, menuitem * menustr, int starty, int startx)
{
	int str_max_length = 0;
	int cnt = 0;
	int ypos, oldypos;
	int lines = 0;
	int c;
	WINDOW *menuwin;

	for (lines = 0; (*(menustr[lines].st_ptr) != 0); lines++) {
		if (strlen(menustr[lines].st_ptr) > str_max_length)
			str_max_length = strlen(menustr[lines].st_ptr);
	}

	menuwin =
	    winopen(wtab, lines + 2, str_max_length + 1, starty, startx,
		    TRUE);

	wrefresh(menuwin);

	menu_write_line(menuwin, 0, str_max_length, TRUE,
			menustr[0].st_ptr);
	for (ypos = 1; ypos < lines; ypos++)
		menu_write_line(menuwin, ypos, str_max_length, FALSE,
				menustr[ypos].st_ptr);

	wrefresh(menuwin);
	ypos = 0;

	do {
		while ((c = getch()) == ERR);
		oldypos = ypos;
		switch (c) {
		case KEY_DOWN:
			if (++ypos >= lines)
				ypos = 0;
			break;
		case KEY_UP:
			if (ypos == 0)
				ypos = lines - 1;
			else
				ypos--;
			break;
		default:
			if ((char) c >= 'a' && (char) c <= 'z')
				c -= 'a' - 'A';

			for (cnt = 0;
			     menustr[cnt].key != (char) c && cnt < lines;
			     cnt++);
			if (menustr[cnt].key == (char) c)
				ypos = cnt;
			break;
		}

		if (ypos != oldypos) {
			menu_write_line(menuwin, ypos, str_max_length,
					TRUE, menustr[ypos].st_ptr);
			menu_write_line(menuwin, oldypos, str_max_length,
					FALSE, menustr[oldypos].st_ptr);

			wrefresh(menuwin);
		}
	} while (c != KEY_ENTER && c != '\r' && c != '\n' && c != KEY_RIGHT
		 && c != KEY_LEFT && c != 0x1b);

	delwin(menuwin);

	winclose(wtab);

	if (c == 0x1b)
		return 0;


	if (c == KEY_RIGHT || c == KEY_LEFT)
		return c;
	else
		return ypos + 1;
}

void menu_write_item(WINDOW * win, int xpos, int reverse, const char st[])
{
	int cnt;
	int high = FALSE;

	if (reverse)
		wattron(win, A_REVERSE);

	wmove(win, 1, xpos + 1);

	for (cnt = 0; st[cnt] != 0; cnt++) {
		if (st[cnt] == '~') {
			if (!reverse) {
				wattron(win, A_BOLD);
				high = TRUE;
			}
		} else {
			waddch(win, st[cnt]);
			if (high) {
				high = FALSE;
				wattroff(win, A_BOLD);
			}
		}
	}

	if (reverse)
		wattroff(win, A_REVERSE);
}


int top_menu(wint * wtab, menuitem menustr[], int ystart)
{
	int str_max_length = 0;
	int str_length = 0;
	int cnt;
	int xpos, oldxpos;
	int ypos = 0;
	int items = 0;
	int c;
	WINDOW *menuwin;
	int items_xpos[12];

	curs_set(0);		/*cursor visibility off */

	for (items = 0; *(menustr[items].st_ptr) != 0; items++) {
		if (items == 0)
			items_xpos[0] = 1;
		else
			items_xpos[items] =
			    items_xpos[items - 1] + str_length;

		if (strlen(menustr[items].st_ptr) > str_max_length)
			str_max_length = strlen(menustr[items].st_ptr);

		str_length = strlen(menustr[items].st_ptr) + 1;
	}

	menuwin = winopen(wtab, 3, 80, ystart, 0, TRUE);

	wrefresh(menuwin);

	menu_write_item(menuwin, 1, TRUE, menustr[0].st_ptr);

	for (xpos = 1; xpos < items; xpos++)
		menu_write_item(menuwin, items_xpos[xpos], FALSE,
				menustr[xpos].st_ptr);

	wrefresh(menuwin);
	xpos = 0;

	do {
		while ((c = getch()) == ERR);

		oldxpos = xpos;

		switch (c) {
		case KEY_RIGHT:
			if (++xpos >= items)
				xpos = 0;
			break;

		case KEY_LEFT:
			if (xpos == 0)
				xpos = items - 1;
			else
				xpos--;
			break;

		case KEY_DOWN:
		case KEY_ENTER:
		case '\r':
		case '\n':
			ypos = 0;
			do {
				switch (ypos) {
				case KEY_RIGHT:
					if (++xpos >= items)
						xpos = 0;
					menu_write_item(menuwin,
							items_xpos[xpos],
							TRUE,
							menustr[xpos].
							st_ptr);
					menu_write_item(menuwin,
							items_xpos
							[oldxpos], FALSE,
							menustr[oldxpos].
							st_ptr);
					wrefresh(menuwin);
					oldxpos = xpos;
					break;

				case KEY_LEFT:
					if (xpos == 0)
						xpos = items - 1;
					else
						xpos--;
					menu_write_item(menuwin,
							items_xpos[xpos],
							TRUE,
							menustr[xpos].
							st_ptr);
					menu_write_item(menuwin,
							items_xpos
							[oldxpos], FALSE,
							menustr[oldxpos].
							st_ptr);
					wrefresh(menuwin);
					oldxpos = xpos;
					break;

				}

				ypos =
				    p_dwn_menu(wtab,
					       (menuitem *) menustr[xpos].
					       arg, ystart + 2,
					       items_xpos[xpos] + 1);
				touchwin(menuwin);
				wrefresh(menuwin);
			} while (ypos == KEY_RIGHT || ypos == KEY_LEFT);
			break;

		default:
			if ((char) c >= 'a' && (char) c <= 'z')
				c -= 'a' - 'A';
			for (cnt = 0;
			     menustr[cnt].key != (char) c && cnt <= items;
			     cnt++);
			if (menustr[cnt].key == (char) c)
				xpos = cnt;
		}

		if (xpos != oldxpos) {
			menu_write_item(menuwin, items_xpos[xpos], TRUE,
					menustr[xpos].st_ptr);
			menu_write_item(menuwin, items_xpos[oldxpos],
					FALSE, menustr[oldxpos].st_ptr);
			wrefresh(menuwin);
		}

	} while (ypos == 0 && c != 0x1b);

	delwin(menuwin);
	curs_set(1);
	winclose(wtab);

	if (c == 0x1b)
		return 0;

	return (ypos & 0x0F) | ((xpos & 0x07) << 4);
}
