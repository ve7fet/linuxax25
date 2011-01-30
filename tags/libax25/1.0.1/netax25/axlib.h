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
 * Callsign conversion functions, converts callsigns into network bit
 * shifted format and vica versa.
 */
 
#ifndef _AXLIB_H
#define	_AXLIB_H

#include <netax25/ax25.h>
#include <netrose/rose.h>

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
 * Libax25 version.
 */
extern char libax25_version[];

/*
 * The special "null" address, used as the default callsign in routing and
 * in other places.
 */
extern ax25_address null_ax25_address;

/*
 * This function converts an ASCII representation of a callsign into network
 * format. It returns -1 on error, 0 otherwise.
 */
extern int ax25_aton_entry(const char *, char *);

/*
 * This converts a string with optional digipeaters into a structure understood
 * by the kernel code.
 *
 * The string should be in the format:
 *
 * callsign [[V | VIA] callsign ...]
 *
 * On error a -1 is returned, otherwise the length of the structure is returned.
 */
extern int ax25_aton(const char *, struct full_sockaddr_ax25 *);

/*
 * Similar to convert_call above except the callsign(s) are not held in a
 * string but in a NULL terminated array of pointers to the strings.
 * On error a -1 is returned, otherwise the length of the structure is returned.
 */
extern int ax25_aton_arglist(const char ** , struct full_sockaddr_ax25 *);

/*
 * This function converts an ASCII representation of a Rose address into
 * network format. It returns -1 on error, 0 otherwise. The address must be
 * ten numbers long.
 */
extern int rose_aton(const char *, char *);

/*
 * This function returns the textual representation of a callsign in
 * network format. The data returned is in a statically allocated area, and
 * subsequent calls will destroy previous callsigns returned.
 */
extern char *ax25_ntoa(const ax25_address *);

/*
 * This function returns the textual representation of a Rose address in
 * network format. The data returned is in a statically allocated area, and
 * subsequent calls will destroy previous callsigns returned.
 */
extern char *rose_ntoa(const rose_address *);

/*
 * Compares two AX.25 callsigns in network format. Returns a 0 if they are
 * identical, 1 if they differ, or 2 if only the SSIDs differ.
 */
extern int ax25_cmp(const ax25_address *, const ax25_address *);

/*
 * Compares two Rose addresses in network format. Returns a 0 if they are
 * identical, 1 if they differ.
 */
extern int rose_cmp(const rose_address *, const rose_address *);

/*
 * Validates an AX.25 callsign, returns TRUE if it is valid, or FALSE if it
 * is not. The callsign should be AX.25 shifted format.
 */
extern int ax25_validate(const char *);

/*
 * Converts the giver string to upper case. It returns a pointer to the
 * original string.
 */
extern char *strupr(char *);

/*
 * Converts the giver string to lower case. It returns a pointer to the
 * original string.
 */
extern char *strlwr(char *);


#ifdef __cplusplus
}
#endif

#endif
