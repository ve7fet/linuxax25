#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <config.h>

#include <netax25/ax25.h>		/* Needed by nrconfig.h */
#include <netax25/nrconfig.h>
#include <netax25/procutils.h>

#include "pathnames.h"

#define min(a,b) ((a) < (b) ? (a) : (b))

static char *space    = " \t\n\r";

/*
 * Version of atoi() that returns zero if s == NULL.
 */
static int safe_atoi(const char *s)
{
	return s ? atoi(s) : 0;
}

/*
 * Version of atox() that returns zero if s == NULL.
 */
static unsigned long safe_atox(const char *s)
{
	return s ? strtoul(s, NULL, 16) : 0;
}

/*
 * Version of strncpy() that returns NULL if either src or dest is NULL
 * and also makes sure destination string is always terminated.
 */
static char *safe_strncpy(char *dest, char *src, int n)
{
	if (!dest || !src)
		return NULL;
	dest[n] = 0;
	return strncpy(dest, src, n);
}

/*
 * Our version of strtok(). This one does not touch the original string,
 * reports the identity of the delimitting character and does not
 * retain any state info. 
 */
static char *token(char **ptr, const char *delim)
{
	static char buf[256];
	char *start;
	int len;

	if (!ptr || !*ptr || !delim)
		return NULL;

	start = *ptr + strspn(*ptr, delim);
	len   = strcspn(start, delim);
	*ptr  = start + len;

	if (len == 0)
		return NULL;

	len = min(len, sizeof(buf) - 1);
	memcpy(buf, start, len);
	buf[len] = 0;

	return buf;
}

static char *strip_zero_ssid(char *call)
{
	char *cp;

	if ((cp = strstr(call, "-0")) != NULL)
		*cp = 0;
	return call;
}

struct proc_ax25 *read_proc_ax25(void)
{
	FILE *fp;
	char buffer[256], *cp;
	struct proc_ax25 *p;
	struct proc_ax25 *list = NULL;

	errno = 0;

	if ((fp = fopen(PROC_AX25_FILE, "r")) == NULL)
		return NULL;

	while (fgets(buffer, 256, fp) != NULL) {
		if ((p = calloc(1, sizeof(struct proc_ax25))) == NULL)
			break;

		cp = buffer;

		p->magic = safe_atox(token(&cp, space));

		safe_strncpy(p->dev,       token(&cp, space), 13);
		safe_strncpy(p->src_addr,  token(&cp, space), 9);

		safe_strncpy(p->dest_addr, token(&cp, ", \t\r\n"), 9);

		p->ndigi = 0;
		while (*cp == ',' && p->ndigi < 9) {
			safe_strncpy(p->digi_addr[p->ndigi],
				     token(&cp, ", \t\r\n"), 10);
			p->ndigi++;
		}

		p->st = safe_atoi(token(&cp, space));

		p->vs = safe_atoi(token(&cp, space));
		p->vr = safe_atoi(token(&cp, space));
		p->va = safe_atoi(token(&cp, space));

		p->t1timer = safe_atox(token(&cp, space));
		p->t1      = safe_atox(token(&cp, space));

		p->t2timer = safe_atox(token(&cp, space));
		p->t2      = safe_atox(token(&cp, space));

		p->t3timer = safe_atox(token(&cp, space));
		p->t3      = safe_atox(token(&cp, space));

		p->idletimer = safe_atox(token(&cp, space));
		p->idle      = safe_atox(token(&cp, space));

		p->n2count = safe_atoi(token(&cp, space));
		p->n2      = safe_atoi(token(&cp, space));

		p->rtt    = safe_atox(token(&cp, space));

		p->window = safe_atoi(token(&cp, space));

		p->paclen = safe_atoi(token(&cp, space));

		p->sndq   = safe_atoi(token(&cp, space));
		p->rcvq   = safe_atoi(token(&cp, space));

		p->inode  = safe_atox(token(&cp, space));

		p->next = list;
		list = p;
	}
	fclose(fp);
	return list;
}

void free_proc_ax25(struct proc_ax25 *ap)
{
	struct proc_ax25 *p;

	while (ap != NULL) {
		p = ap->next;
		free(ap);
		ap = p;
	}
}

struct proc_ax25_route *read_proc_ax25_route(void)
{
	FILE *fp;
	char buffer[256], *cp;
	struct proc_ax25_route *new, *tmp, *p;
	struct proc_ax25_route *list = NULL;
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_AX25_ROUTE_FILE, "r")) == NULL)
		return NULL;

	while (fgets(buffer, 256, fp) != NULL) {
		if (!i++) continue;
		if ((new = calloc(1, sizeof(struct proc_ax25_route))) == NULL)
			break;

		cp = buffer;

		safe_strncpy(new->call, token(&cp, space), 9);
		safe_strncpy(new->dev,  token(&cp, space),   13);

		new->cnt  = safe_atoi(token(&cp, space));
		new->t    = safe_atoi(token(&cp, space));

		if (list == NULL || new->t > list->t) {
			tmp = list;
			list = new;
			new->next = tmp;
		} else {
			for (p = list; p->next != NULL; p = p->next)
				if (new->t > p->next->t)
					break;
			tmp = p->next;
			p->next = new;
			new->next = tmp;
		}
	}
	fclose(fp);
	return list;
}

void free_proc_ax25_route(struct proc_ax25_route *rp)
{
	struct proc_ax25_route *p;

	while (rp != NULL) {
		p = rp->next;
		free(rp);
		rp = p;
	}
}

struct proc_nr *read_proc_nr(void)
{
	FILE *fp;
	char buffer[256], *cp;
	struct proc_nr *p;
	struct proc_nr *list = NULL;
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_NR_FILE, "r")) == NULL)
		return NULL;
	while (fgets(buffer, 256, fp) != NULL) {
		if (!i++) continue;
		if ((p = calloc(1, sizeof(struct proc_nr))) == NULL)
			break;

		cp = buffer;

		safe_strncpy(p->user_addr,  token(&cp, space), 9);
		safe_strncpy(p->dest_node,  token(&cp, space), 9);
		safe_strncpy(p->src_node,   token(&cp, space), 9);
		safe_strncpy(p->dev,        token(&cp, space), 13);
		safe_strncpy(p->my_circuit, token(&cp, space), 5);
		safe_strncpy(p->ur_circuit, token(&cp, space), 5);

		p->st = safe_atoi(token(&cp, space));
		p->vs = safe_atoi(token(&cp, space));
		p->vr = safe_atoi(token(&cp, space));
		p->va = safe_atoi(token(&cp, space));

		p->t1timer   = safe_atox(token(&cp, "/"));
		p->t1        = safe_atox(token(&cp, "/ \t\r\n"));
		p->t2timer   = safe_atox(token(&cp, "/"));
		p->t2        = safe_atox(token(&cp, "/ \t\r\n"));
		p->t4timer   = safe_atox(token(&cp, "/"));
		p->t4        = safe_atox(token(&cp, "/ \t\r\n"));
		p->idletimer = safe_atox(token(&cp, "/"));
		p->idle      = safe_atox(token(&cp, "/ \t\r\n"));
		p->n2count   = safe_atoi(token(&cp, "/"));
		p->n2        = safe_atoi(token(&cp, "/ \t\r\n"));

		p->window = safe_atoi(token(&cp, space));
		p->sndq   = safe_atoi(token(&cp, space));
		p->rcvq   = safe_atoi(token(&cp, space));
		p->inode  = safe_atox(token(&cp, space));

		p->next = list;
		list = p;
	}
	fclose(fp);
	return list;
}

void free_proc_nr(struct proc_nr *np)
{
	struct proc_nr *p;

	while (np != NULL) {
		p = np->next;
		free(np);
		np = p;
	}
}

struct proc_nr_neigh *read_proc_nr_neigh(void)
{
	FILE *fp;
	char buffer[256], *cp;
	struct proc_nr_neigh *p;
	struct proc_nr_neigh *list = NULL;
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_NR_NEIGH_FILE, "r")) == NULL)
		return NULL;

	while (fgets(buffer, 256, fp) != NULL) {
		if (!i++) continue;
		if ((p = calloc(1, sizeof(struct proc_nr_neigh))) == NULL)
			break;

		cp = buffer;

		p->addr = safe_atoi(token(&cp, space));

		safe_strncpy(p->call, token(&cp, space), 9);
		safe_strncpy(p->dev,  token(&cp, space), 13);

		p->qual = safe_atoi(token(&cp, space));
		p->lock = safe_atoi(token(&cp, space));
		p->cnt  = safe_atoi(token(&cp, space));

		p->next = list;
		list = p;
	}
	fclose(fp);
	return list;
}

void free_proc_nr_neigh(struct proc_nr_neigh *np)
{
	struct proc_nr_neigh *p;

	while (np != NULL) {
		p = np->next;
		free(np);
		np = p;
	}
}

struct proc_nr_nodes *read_proc_nr_nodes(void)
{
	FILE *fp;
	char buffer[256], *cp;
	struct proc_nr_nodes *new, *tmp, *p;
	struct proc_nr_nodes *list = NULL;
	char *name;
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_NR_NODES_FILE, "r")) == NULL)
		return NULL;

	while (fgets(buffer, 256, fp) != NULL) {
		if (!i++) continue;
		if ((new = calloc(1, sizeof(struct proc_nr_nodes))) == NULL)
			break;

		cp = buffer;

		safe_strncpy(new->call,  token(&cp, space), 9);
		strip_zero_ssid(new->call);

		safe_strncpy(new->alias, token(&cp, space), 6);

		new->w     = safe_atoi(token(&cp, space));
		new->n     = safe_atoi(token(&cp, space));
		new->qual1 = safe_atoi(token(&cp, space));
		new->obs1  = safe_atoi(token(&cp, space));
		new->addr1 = safe_atoi(token(&cp, space));
		if (new->n > 1) {
			new->qual2 = safe_atoi(token(&cp, space));
			new->obs2  = safe_atoi(token(&cp, space));
			new->addr2 = safe_atoi(token(&cp, space));
		}
		if (new->n > 2) {
			new->qual3 = safe_atoi(token(&cp, space));
			new->obs3  = safe_atoi(token(&cp, space));
			new->addr3 = safe_atoi(token(&cp, space));
		}
		if (list == NULL || strcmp(new->alias, list->alias) < 0) {
			tmp = list;
			list = new;
			new->next = tmp;
		} else {
			for (p = list; p->next != NULL; p = p->next)
				if (strcmp(new->alias, p->next->alias) < 0)
					break;
			tmp = p->next;
			p->next = new;
			new->next = tmp;
		}
	}
	/*
	 * Now load the local nodes.
	 */
	name = NULL;
	while ((name = nr_config_get_next(name)) != NULL) {
		if ((new = calloc(1, sizeof(struct proc_nr_nodes))) == NULL)
			break;
		if ((cp = nr_config_get_addr(name)) == NULL)
			break;
		strip_zero_ssid(cp);
		safe_strncpy(new->call, cp, 9);
		if ((cp = nr_config_get_alias(name)) == NULL)
			break;
		safe_strncpy(new->alias, cp, 6);
		new->w = 1;             /* local node w */
		new->qual1 = 255;       /* obviously, local node quality should be set to maximum value */
		new->obs1 = 6;          /* persist */ 

		/*
		 * n == 0 indicates a local node.
		 */
		new->n = 0;
		if (list == NULL || strcmp(new->alias, list->alias) < 0) {
			tmp = list;
			list = new;
			new->next = tmp;
		} else {
			for (p = list; p->next != NULL; p = p->next)
				if (strcmp(new->alias, p->next->alias) < 0)
					break;
			tmp = p->next;
			p->next = new;
			new->next = tmp;
		}
	}
	fclose(fp);
	return list;
}

void free_proc_nr_nodes(struct proc_nr_nodes *np)
{
	struct proc_nr_nodes *p;

	while (np != NULL) {
		p = np->next;
		free(np);
		np = p;
	}
}

struct proc_rs *read_proc_rs(void)
{
	FILE *fp;
	char buffer[256];
	struct proc_rs *p;
	struct proc_rs *list = NULL;
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_RS_FILE, "r")) == NULL)
		return NULL;
	while (fgets(buffer, 256, fp) != NULL) 
	{
		if (!i++) continue;
		if ((p = calloc(1, sizeof(struct proc_rs))) == NULL)
			break;
		safe_strncpy(p->dest_addr, strtok(buffer, " \t\n\r"), 10);
		safe_strncpy(p->dest_call, strtok(NULL, " \t\n\r"), 9);
		safe_strncpy(p->src_addr, strtok(NULL, " \t\n\r"), 10);
		safe_strncpy(p->src_call, strtok(NULL, " \t\n\r"), 9);
		safe_strncpy(p->dev,  strtok(NULL, " \t\n\r"), 13);
		p->lci   = safe_atox(strtok(NULL, " \t\n\r"));
		p->neigh = safe_atoi(strtok(NULL, " \t\n\r"));
		p->st    = safe_atoi(strtok(NULL, " \t\n\r"));
		p->vs    = safe_atoi(strtok(NULL, " \t\n\r"));
		p->vr    = safe_atoi(strtok(NULL, " \t\n\r"));
		p->va    = safe_atoi(strtok(NULL, " \t\n\r"));
		p->t     = safe_atoi(strtok(NULL, " \t\n\r"));
		p->t1    = safe_atoi(strtok(NULL, " \t\n\r"));
		p->t2    = safe_atoi(strtok(NULL, " \t\n\r"));
		p->t3    = safe_atoi(strtok(NULL, " \t\n\r"));
		p->hb    = safe_atoi(strtok(NULL, " \t\n\r"));
		p->sndq  = safe_atoi(strtok(NULL, " \t\n\r"));
		p->rcvq  = safe_atoi(strtok(NULL, " \t\n\r"));
		p->next  = list;
		list = p;
	}
	fclose(fp);
	return list;
}

void free_proc_rs(struct proc_rs *ap)
{
	struct proc_rs *p;

	while (ap != NULL) {
		p = ap->next;
		free(ap);
		ap = p;
	}
}

struct proc_rs_neigh *read_proc_rs_neigh(void)
{
	FILE *fp;
	char buffer[256];
	struct proc_rs_neigh *p;
	struct proc_rs_neigh *list = NULL;
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_RS_NEIGH_FILE, "r")) == NULL)
		return NULL;
	while (fgets(buffer, 256, fp) != NULL) 
	{
		if (!i++) continue;
		if ((p = calloc(1, sizeof(struct proc_rs_neigh))) == NULL)
			break;
		p->addr = safe_atoi(strtok(buffer, " \t\n\r"));
		safe_strncpy(p->call, strtok(NULL, " \t\n\r"), 9);
		safe_strncpy(p->dev,  strtok(NULL, " \t\n\r"), 13);
		p->count = safe_atoi(strtok(NULL, " \t\n\r"));
		p->use = safe_atoi(strtok(NULL, " \t\n\r"));
		safe_strncpy(p->mode,  strtok(NULL, " \t\n\r"), 3);
		safe_strncpy(p->restart,  strtok(NULL, " \t\n\r"), 3);
		p->t0 = safe_atoi(strtok(NULL, " \t\n\r"));
		p->tf = safe_atoi(strtok(NULL, " \t\n\r"));
		p->next = list;
		list = p;
	}
	fclose(fp);
	return list;
}

void free_proc_rs_neigh(struct proc_rs_neigh *np)
{
	struct proc_rs_neigh *p;

	while (np != NULL) {
		p = np->next;
		free(np);
		np = p;
	}
}

struct proc_rs_nodes *read_proc_rs_nodes(void)
{
	FILE *fp;
	char buffer[256];
	struct proc_rs_nodes *p;
	struct proc_rs_nodes *list = NULL;
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_RS_NODES_FILE, "r")) == NULL)
		return NULL;
	while (fgets(buffer, 256, fp) != NULL) 
	{
		if (!i++) continue;
		if ((p = calloc(1, sizeof(struct proc_rs_nodes))) == NULL)
			break;
		safe_strncpy(p->address, strtok(buffer, " \t\n\r"), 10);
		p->mask   = safe_atoi(strtok(NULL, " \t\n\r"));
		p->n      = safe_atoi(strtok(NULL, " \t\n\r"));
		p->neigh1 = safe_atoi(strtok(NULL, " \t\n\r"));
		p->neigh2 = safe_atoi(strtok(NULL, " \t\n\r"));
		p->neigh3 = safe_atoi(strtok(NULL, " \t\n\r"));
		p->next = list;
		list = p;
	}
	fclose(fp);
	return list;
}

void free_proc_rs_nodes(struct proc_rs_nodes *np)
{
	struct proc_rs_nodes *p;

	while (np != NULL) {
		p = np->next;
		free(np);
		np = p;
	}
}

struct proc_rs_route *read_proc_rs_routes(void)
{
	FILE *fp;
	char buffer[256];
	struct proc_rs_route *p;
	struct proc_rs_route *list = NULL;
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_RS_ROUTES_FILE, "r")) == NULL)
		return NULL;
	while (fgets(buffer, 256, fp) != NULL) 
	{
		if (!i++) continue;
		if ((p = calloc(1, sizeof(struct proc_rs_route))) == NULL)
			break;
		p->lci1 = safe_atox(strtok(buffer, " \t\n\r"));
		safe_strncpy(p->address1, strtok(NULL, " \t\n\r"), 10);
		safe_strncpy(p->call1, strtok(NULL, " \t\n\r"), 9);
		p->neigh1 = safe_atoi(strtok(NULL, " \t\n\r"));
		p->lci2   = safe_atox(strtok(NULL, " \t\n\r"));
		safe_strncpy(p->address2, strtok(NULL, " \t\n\r"), 10);
		safe_strncpy(p->call2, strtok(NULL, " \t\n\r"), 9);
		p->neigh2 = safe_atoi(strtok(NULL, " \t\n\r"));
		p->next = list;
		list = p;
	}
	fclose(fp);
	return list;
}

void free_proc_rs_routes(struct proc_rs_route *np)
{
	struct proc_rs_route *p;

	while (np != NULL) {
		p = np->next;
		free(np);
		np = p;
	}
}

char *get_call(int uid)
{
	FILE *fp;
	char buf[256];
	static char call[10];
	int i = 0;

	errno = 0;
	if ((fp = fopen(PROC_AX25_CALLS_FILE, "r")) == NULL)
		return NULL;
	while (fgets(buf, 256, fp) != NULL) {
		if (!i++) continue;
		if (safe_atoi(strtok(buf, " \t\r\n")) == uid) {
			fclose(fp);
			safe_strncpy(call, strtok(NULL, " \t\r\n"), 9);
			return call;
		}
	}
	fclose(fp);
	return NULL;
}

struct proc_ax25 *find_link(const char *src, const char *dest, const char *dev)
{
	static struct proc_ax25 a;
	struct proc_ax25 *p, *list;

	list = read_proc_ax25();
	for (p = list; p != NULL; p = p->next) {
		if (!strcmp(src, p->src_addr) &&
		    !strcmp(dest, p->dest_addr) &&
		    !strcmp(dev, p->dev)) {
			a = *p;
			a.next = NULL;
			free_proc_ax25(list);
			return &a;
		}
	}
	free_proc_ax25(list);
	return NULL;
}

struct proc_nr_neigh *find_neigh(int addr, struct proc_nr_neigh *neighs)
{
	static struct proc_nr_neigh n;
	struct proc_nr_neigh *p, *list;

	list = neighs ? neighs : read_proc_nr_neigh();
	for (p = list; p != NULL; p = p->next) {
		if (addr == p->addr) {
			n = *p;
			n.next = NULL;
			p = &n;
			break;
		}
	}
	if (!neighs)
		free_proc_nr_neigh(list);
	return p;
}

struct proc_nr_nodes *find_node(char *addr, struct proc_nr_nodes *nodes)
{
	static struct proc_nr_nodes n;
	struct proc_nr_nodes *p, *list;

	strip_zero_ssid(addr);
	list = nodes ? nodes : read_proc_nr_nodes();
	for (p = list; p != NULL; p = p->next) {
		if (!strcasecmp(addr, p->call) || !strcasecmp(addr, p->alias)) {
			n = *p;
			n.next = NULL;
			p = &n;
			break;
		}
	}
	if (!nodes)
		free_proc_nr_nodes(list);
        return p;
}
