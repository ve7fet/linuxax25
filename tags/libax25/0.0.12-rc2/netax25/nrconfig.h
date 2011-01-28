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
 * This file contains the definitions of the entry points into the NET/ROM
 * configuration functions.
 */

#ifndef	_NRCONFIG_H
#define	_NRCONFIG_H

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
 * This function must be called before using any of the other functions in
 * this part of the library. It returns the number of active ports, or 0
 * on failure.
 */
extern int nr_config_load_ports(void);

/*
 * This function allows the enumeration of all the active configured ports.
 * Passing NULL as the argument returns the first port name in the list,
 * subsequent calls to this function should be made with the last port name
 * returned. A NULL return indicates either an error, or the end of the list.
 */
extern char *nr_config_get_next(char *);

/*
 * This function maps the device name onto the port name (as used in the axports
 * file. On error a NULL is returned.
 */
extern char *nr_config_get_name(char *);

/*
 * This function maps the port name onto the callsign of the port. On error a
 * NULL is returned.
 */
extern char *nr_config_get_addr(char *);

/*
 * This function maps the port name onto the device name of the port. On error a
 * NULL is returned.
 */
extern char *nr_config_get_dev(char *);

/*
 * This function maps the callsign in AX.25 shifted format onto the port name.
 * On error, NULL is returned.
 */
extern char *nr_config_get_port(ax25_address *);

/*
 * This function takes the port name and returns the alias of the port. On
 * error NULL is returned.
 */
extern char *nr_config_get_alias(char *);

/*
 * This function takes the port name and returns the maximum packet length.
 * On error a 0 is returned.
 */
extern int nr_config_get_paclen(char *);

/*
 * This function takes the port name and returns the description of the port.
 * On error a NULL is returned.
 */
extern char *nr_config_get_desc(char *);

#ifdef __cplusplus
}
#endif

#endif
