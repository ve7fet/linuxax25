#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <config.h>

#include <netax25/ax25.h>		/* Needed by nrconfig.h */
#include <netax25/nrconfig.h>
#include <netax25/procutils.h>

#include "pathnames.h"
#include <ctype.h>

#define PROC_AX25_FILE "/proc/net/ax25"
#define LINE_MAX_LEN 2048
#define MAX_FIELDS 32
#define min(a,b) ((a) < (b) ? (a) : (b))

int has_header = 0; 

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

typedef struct proc_ax25 proc_ax25_t;


// --- Fonctions d'aide à l'extraction (omises pour la concision) ---
void extract_current_max(const char *field, unsigned long *current, unsigned long *max) {
    if (!field || !current || !max) return;
    char *slash = strchr(field, '/');
    if (slash) {
        *current = strtoul(field, NULL, 10); 
        *max = strtoul(slash + 1, NULL, 10);
    } else {
        *current = strtoul(field, NULL, 10);
        *max = 0; 
    }
}

proc_ax25_t *parse_ax25_line(char *line) {
    char *fields[MAX_FIELDS];
    char *saveptr;
    int field_count = 0;
    char *line_copy = strdup(line);
    if (!line_copy) { perror("strdup"); return NULL; }

    char *token = strtok_r(line_copy, " \t", &saveptr);
    while (token != NULL && field_count < MAX_FIELDS) {
        fields[field_count] = token;
        field_count++;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    
    int min_fields = has_header ? 21 : 24;
    if (field_count < min_fields) { free(line_copy); return NULL; }

    proc_ax25_t *new_conn = (proc_ax25_t *)malloc(sizeof(proc_ax25_t));
    if (!new_conn) { perror("malloc"); free(line_copy); return NULL; }
    memset(new_conn, 0, sizeof(proc_ax25_t));

    // Détermination des indices (Dynamique)
    int st_idx = has_header ? 6 : 4;
    int vs_idx = has_header ? 7 : 5;
    int vr_idx = has_header ? 8 : 6;
    int va_idx = has_header ? 9 : 7;
    int t1_idx = has_header ? 10 : 8;
    int t2_idx = has_header ? 11 : 9;
    int t3_idx = has_header ? 12 : 10;
    int idle_idx = has_header ? 13 : 11;
    int n2_idx = has_header ? 14 : 12;
    int rtt_idx = has_header ? 15 : 13;
    int win_idx = has_header ? 16 : 14;
    int paclen_idx = has_header ? 17 : 15;
    int send_q_idx = has_header ? 18 : 21;
    int recv_q_idx = has_header ? 19 : 22;
    int inode_idx = has_header ? 20 : 23; 
    
    // Extraction des données (inchangé)
    new_conn->magic = strtoul(fields[0], NULL, 16); 
    strncpy(new_conn->dev, fields[1], 13); new_conn->dev[13] = '\0';
    strncpy(new_conn->src_addr, fields[2], 9); new_conn->src_addr[9] = '\0';
    strncpy(new_conn->dest_addr, fields[3], 9); new_conn->dest_addr[9] = '\0';
    strncpy(new_conn->digi_addr[0], fields[4], 10); new_conn->digi_addr[0][10] = '\0';
    strncpy(new_conn->digi_addr[1], fields[5], 10); new_conn->digi_addr[1][10] = '\0';
    new_conn->ndigi = 0;
    if (strcmp(fields[4], "*") != 0) new_conn->ndigi = 1;
    if (strcmp(fields[5], "*") != 0) new_conn->ndigi = 2;
    new_conn->st = (unsigned char)atoi(fields[st_idx]);
    new_conn->vs = (unsigned short)atoi(fields[vs_idx]);
    new_conn->vr = (unsigned short)atoi(fields[vr_idx]);
    new_conn->va = (unsigned short)atoi(fields[va_idx]);
    if (has_header) {
        extract_current_max(fields[t1_idx], &new_conn->t1timer, &new_conn->t1);
        extract_current_max(fields[t2_idx], &new_conn->t2timer, &new_conn->t2);
        extract_current_max(fields[t3_idx], &new_conn->t3timer, &new_conn->t3);
        extract_current_max(fields[idle_idx], &new_conn->idletimer, &new_conn->idle);
        unsigned long n2_current, n2_max;
        extract_current_max(fields[n2_idx], &n2_current, &n2_max);
        new_conn->n2count = (unsigned char)n2_current; new_conn->n2 = (unsigned char)n2_max;
    } else {
        new_conn->t1timer = strtoul(fields[t1_idx], NULL, 10); new_conn->t1 = 0;
        new_conn->t2timer = strtoul(fields[t2_idx], NULL, 10); new_conn->t2 = 0;
        new_conn->t3timer = strtoul(fields[t3_idx], NULL, 10); new_conn->t3 = 0;
        new_conn->idletimer = strtoul(fields[idle_idx], NULL, 10); new_conn->idle = 0;
        new_conn->n2count = (unsigned char)atoi(fields[n2_idx]); new_conn->n2 = 0;
    }
    new_conn->rtt = strtoul(fields[rtt_idx], NULL, 10);
    new_conn->window = (unsigned char)atoi(fields[win_idx]);
    new_conn->paclen = (unsigned short)atoi(fields[paclen_idx]);
    new_conn->sndq = (unsigned short)atoi(fields[send_q_idx]);
    new_conn->rcvq = (unsigned short)atoi(fields[recv_q_idx]);
    new_conn->inode = strtoul(fields[inode_idx], NULL, 10);
    new_conn->next = NULL;
    free(line_copy);
    return new_conn;
}

proc_ax25_t *read_proc_ax25(void) {
    FILE *pipe_fp = NULL; char line[LINE_MAX_LEN]; int line_count = 0;
    proc_ax25_t *head = NULL; proc_ax25_t *current = NULL;
    char command[sizeof("/bin/cat ") + sizeof(PROC_AX25_FILE)];
    sprintf(command, "/bin/cat %s", PROC_AX25_FILE);

    if ((pipe_fp = popen(command, "r")) == NULL) { fprintf(stderr, "Error: Cannot open pipe for command '%s'.\n", command); return NULL; }
    if (fgets(line, sizeof(line), pipe_fp) != NULL) {
        char *p = line; while(isspace(*p)) p++; has_header = (!isdigit(*p)); line_count = 1;
        while (fgets(line, sizeof(line), pipe_fp) != NULL) line_count++;
    }
    pclose(pipe_fp); 
    if (line_count <= (has_header ? 1 : 0)) return NULL;

    if ((pipe_fp = popen(command, "r")) == NULL) { fprintf(stderr, "Error: Cannot reopen pipe for command '%s': %s\n", command, strerror(errno)); return NULL; }
    if (has_header) { if (fgets(line, sizeof(line), pipe_fp) == NULL) { pclose(pipe_fp); return NULL; } }

    while (fgets(line, sizeof(line), pipe_fp) != NULL) {
        line[strcspn(line, "\n")] = 0; if (strlen(line) == 0) continue;
        proc_ax25_t *new_conn = parse_ax25_line(line);
        if (new_conn) {
            if (head == NULL) { head = new_conn; current = new_conn; } 
            else { current->next = new_conn; current = new_conn; }
        }
    }
    pclose(pipe_fp);
    return head;
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
