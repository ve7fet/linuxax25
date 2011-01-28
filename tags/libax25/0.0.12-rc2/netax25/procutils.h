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
 * Support routines to simplify the reading of the /proc/net/ax25* and
 * /proc/net/nr* files.
 */

#ifndef _PROCUTILS_H
#define	_PROCUTILS_H

#ifndef	TRUE
#define	TRUE	1
#endif

#ifndef	FALSE
#define	FALSE	0
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct proc_ax25_route {
	char			call[10];
	char			dev[14];
	int			cnt;
	long			t;

	struct proc_ax25_route	*next;
};

struct proc_ax25 {
	unsigned long		magic;
	char			dev[14];
	char			src_addr[10];
	char			dest_addr[10];
	char			digi_addr[8][11];
	int			ndigi;
	unsigned char		st;
	unsigned short		vs, vr, va;
	unsigned long		t1timer, t1, t2timer, t2, t3timer, t3;
	unsigned long		idletimer, idle;
	unsigned char		n2count, n2;
	unsigned long		rtt;
	unsigned char		window;
	unsigned short		paclen;
	unsigned short		sndq, rcvq;
	unsigned long		inode;

	struct proc_ax25	*next;
};

struct proc_nr {
	char			user_addr[10], dest_node[10], src_node[10];
	char			dev[14];
	char			my_circuit[6], ur_circuit[6];
	unsigned char		st;
	unsigned short		vs, vr, va;
	unsigned long		t1timer, t1, t2timer, t2, t4timer, t4;
	unsigned long		idletimer, idle;
	unsigned char		n2count, n2;
	unsigned char		window;
	unsigned short		sndq, rcvq;
	unsigned long		inode;

	struct proc_nr		*next;
};

struct proc_nr_neigh {
	int			addr;
	char			call[10];
	char			dev[14];
	int			qual;
	int			lock;
	int			cnt;

	struct proc_nr_neigh	*next;
};

struct proc_nr_nodes {
	char			call[10], alias[7];
	unsigned char		w, n;
	unsigned char		qual1, qual2, qual3;
	unsigned char		obs1, obs2, obs3;
	int			addr1, addr2, addr3;

	struct proc_nr_nodes	*next;
};

struct proc_rs {
	char			dest_addr[11], dest_call[10];
	char			src_addr[11], src_call[10];
	char			dev[14];
	unsigned short		lci;
	unsigned int		neigh;
	unsigned char		st;
	unsigned short		vs, vr, va;
	unsigned short		t, t1, t2, t3;
	unsigned short		hb;
	unsigned long		sndq, rcvq;

	struct proc_rs		*next;
};

struct proc_rs_route {
	unsigned short		lci1;
	char			address1[11], call1[10];
	unsigned int		neigh1;
	unsigned short		lci2;
	char			address2[11], call2[10];
	unsigned int		neigh2;

	struct proc_rs_route	*next;
};

struct proc_rs_neigh {
	int			addr;
	char			call[10];
	char			dev[14];
	unsigned int		count;
	unsigned int		use;
	char			mode[4];
	char			restart[4];
	unsigned short		t0, tf;

	struct proc_rs_neigh	*next;
};

struct proc_rs_nodes {
	char			address[11];
	unsigned char		mask;
	unsigned char		n;
	unsigned int		neigh1, neigh2, neigh3;

	struct proc_rs_nodes	*next;
};

extern struct proc_ax25 *read_proc_ax25(void);
extern void free_proc_ax25(struct proc_ax25 *ap);

extern struct proc_ax25_route *read_proc_ax25_route(void);
extern void free_proc_ax25_route(struct proc_ax25_route *rp);

extern struct proc_nr *read_proc_nr(void);
extern void free_proc_nr(struct proc_nr *);
 
extern struct proc_nr_neigh *read_proc_nr_neigh(void);
extern void free_proc_nr_neigh(struct proc_nr_neigh *np);

extern struct proc_nr_nodes *read_proc_nr_nodes(void);
extern void free_proc_nr_nodes(struct proc_nr_nodes *np);

extern struct proc_rs *read_proc_rs(void);
extern void free_proc_rs(struct proc_rs *);

extern struct proc_rs_neigh *read_proc_rs_neigh(void);
extern void free_proc_rs_neigh(struct proc_rs_neigh *);

extern struct proc_rs_nodes *read_proc_rs_nodes(void);
extern void free_proc_rs_nodes(struct proc_rs_nodes *);

extern struct proc_rs_route *read_proc_rs_routes(void);
extern void free_proc_rs_routes(struct proc_rs_route *);

extern char *get_call(int uid);

extern struct proc_ax25 *find_link(const char *src, const char *dest, const char *dev);
extern struct proc_nr_neigh *find_neigh(int addr, struct proc_nr_neigh *neigh);
extern struct proc_nr_nodes *find_node(char *addr, struct proc_nr_nodes *nodes);

#ifdef _cplusplus
}
#endif

#endif
