/* LIBAX25 - Library for AX.25 programs
 * Copyright (C) 1997-1999 Jonathan Naylor, Tomi Manninen, Jean-Paul Roubelat
 * and Alan Cox.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Generic serial port handling functions.
 */
 
#ifndef _TTYUTILS_H
#define	_TTYUTILS_H

#ifndef	TRUE
#define	TRUE	1
#endif

#ifndef	FALSE
#define	FALSE	0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Put a given file descriptor into raw mode, if the hwflag is set to TRUE
 * then hardware handshaking is enabled. Returns TRUE if successful.
 */
extern int tty_raw(int fd, int hwflag);

/*
 * Set the speed of the given file descriptor. Returns TRUE is it was
 * successful.
 */
extern int tty_speed(int fd, int speed);

/*
 * Determines whether a given tty is already open by another process. Returns
 * TRUE if is already locked, or FALSE if it is free.
 */
extern int tty_is_locked(char *tty);

/*
 * Creates a lock file for the given tty. It writes the process ID to the
 * file so take care if doing a fork. Returns TRUE if everything was OK.
 */
extern int tty_lock(char *tty);

/*
 * Removes the lock file for a given tty. Returns TRUE if successful.
 */
extern int tty_unlock(char *tty);

#ifdef __cplusplus
}
#endif

#endif
