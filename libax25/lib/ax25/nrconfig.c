#define _LINUX_STRING_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <config.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <netax25/ax25.h>
#include <netrose/rose.h>

#include "pathnames.h"
#include <netax25/axlib.h>
#include <netax25/nrconfig.h>

typedef struct _nrport
{
	struct _nrport *Next;
	char *Name;
	char *Call;
	char *Alias;
	char *Device;
	int  Paclen;
	char *Description;
} NR_Port;

static NR_Port *nr_ports       = NULL;
static NR_Port *nr_port_tail   = NULL;

static int is_same_call(char *call1, char *call2)
{
        if (!call1 || !call2)
                return 0;
        for (; *call1 && *call2; call1++, call2++) {
                if (*call1 == '-' || *call2 == '-')
                        break;
                if (tolower(*call1 & 0xff) != tolower(*call2 & 0xff))
                        return 0;
        }
        if (!*call1 && !*call2)
                return 1;
        if (!*call1 && !strcmp(call2, "-0"))
                return 1;
        if (!*call2 && !strcmp(call1, "-0"))
                return 1;
        return (!strcmp(call1, call2) ? 1 : 0);
}

static NR_Port *nr_port_ptr(char *name)
{
	NR_Port *p = nr_ports;

	if (name == NULL)
		return p;

	while (p != NULL) {
		if (p->Name != NULL && strcasecmp(name, p->Name) == 0)
			return p;
		p = p->Next;
	}

	return NULL;
}

char *nr_config_get_next(char *name)
{
	NR_Port *p;
	
	if (nr_ports == NULL)
		return NULL;
		
	if (name == NULL)
		return nr_ports->Name;
		
	if ((p = nr_port_ptr(name)) == NULL)
		return NULL;
		
	p = p->Next;

	if (p == NULL)
		return NULL;
		
	return p->Name;
}

char *nr_config_get_name(char *device)
{
	NR_Port *p = nr_ports;

	while (p != NULL) {
		if (p->Device != NULL && strcmp(device, p->Device) == 0)
			return p->Name;

		p = p->Next;
	}

	return NULL;
}

char *nr_config_get_addr(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Call;
}

char *nr_config_get_dev(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Device;
}

char *nr_config_get_port(ax25_address *callsign)
{
	NR_Port *p = nr_ports;
	ax25_address addr;

	while (p != NULL) {
		if (p->Call != NULL) {
			ax25_aton_entry(p->Call, (char *)&addr);
	
			if (ax25_cmp(callsign, &addr) == 0)
				return p->Name;

		}
		p = p->Next;
	}

	return NULL;
}

char *nr_config_get_alias(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Alias;
}

int nr_config_get_paclen(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return 0;

	return p->Paclen;
}

char *nr_config_get_desc(char *name)
{
	NR_Port *p = nr_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Description;
}

static int nr_config_init_port(int fd, int lineno, char *line, const char **ifcalls, const char **ifdevs)
{
	NR_Port *p;
	char *name, *call, *alias, *paclen, *desc;
	const char *dev = NULL;
	int found = 0;
	char call_beautified[10];
	
	name   = strtok(line, " \t");
	call   = strtok(NULL, " \t");
	alias  = strtok(NULL, " \t");
	paclen = strtok(NULL, " \t");
	desc   = strtok(NULL, "");

	if (name == NULL || call == NULL || alias == NULL || paclen == NULL ||
	    desc == NULL) {
		fprintf(stderr, "nrconfig: unable to parse line %d of config file\n", lineno);
		return FALSE;
	}

	for (p = nr_ports; p != NULL; p = p->Next) {
		if (p->Name != NULL && strcasecmp(name, p->Name) == 0) {
			fprintf(stderr, "nrconfig: duplicate port name %s in line %d of config file\n", name, lineno);
			return FALSE;
		}
		if (p->Call != NULL && is_same_call(call, p->Call)) {
			fprintf(stderr, "nrconfig: duplicate callsign %s in line %d of config file\n", call, lineno);
			return FALSE;
		}
		if (p->Alias != NULL && strcasecmp(alias, p->Alias) == 0) {
			fprintf(stderr, "nrconfig: duplicate alias %s in line %d of config file\n", alias, lineno);
			return FALSE;
		}
	}

	if (atoi(paclen) <= 0) {
		fprintf(stderr, "nrconfig: invalid packet size %s in line %d of config file\n", paclen, lineno);
		return FALSE;
	}

	strupr(call);
	strupr(alias);

	strncpy(call_beautified, call, sizeof(call_beautified)-1);
        call_beautified[sizeof(call_beautified)-1] = 0;
	if (strchr(call_beautified, '-') == NULL && strlen(call_beautified) < 7)
		strcat(call_beautified, "-0");

	found = 0;
	for (;ifcalls && *ifcalls; ++ifcalls, ++ifdevs) {
	  if (strcmp(call_beautified, *ifcalls) == 0) {
	    found = 1;
	    dev = *ifdevs;
	    break;
	  }
	}

	if (!found) {
#if 0 /* None of your business to complain about some port being down... */
		fprintf(stderr, "nrconfig: port %s not active\n", name);
#endif
		return FALSE;
	}

	if ((p = (NR_Port *)malloc(sizeof(NR_Port))) == NULL) {
		fprintf(stderr, "nrconfig: out of memory!\n");
		return FALSE;
	}

	p->Name        = strdup(name);
	p->Call        = strdup(call);
	p->Alias       = strdup(alias);
	p->Device      = strdup(dev);
	p->Paclen      = atoi(paclen);
	p->Description = strdup(desc);

	if (nr_ports == NULL)
		nr_ports = p;
	else
		nr_port_tail->Next = p;

	nr_port_tail = p;

	p->Next = NULL;
	
	return TRUE;
}

int nr_config_load_ports(void)
{
	FILE *fp = NULL;
	char buffer[256], *s;
	int fd = -1, lineno = 1, n = 0, i;
	const char **calllist = NULL;
	const char **devlist  = NULL;
	const char **pp;
	int callcount = 0;
	struct ifreq ifr;

	/* Reliable listing of all network ports on Linux
	   is only available via reading  /proc/net/dev ...  */


	if ((fd = socket(PF_FILE, SOCK_DGRAM, 0)) < 0) {
	  fprintf(stderr, "nrconfig: unable to open socket (%s)\n", strerror(errno));
	  goto cleanup;
	}

	if ((fp = fopen("/proc/net/dev", "r"))) {
	  /* Two header lines.. */
	  s = fgets(buffer, sizeof(buffer), fp);
	  s = fgets(buffer, sizeof(buffer), fp);
	  /* .. then network interface names */
	  while (!feof(fp)) {
	    if (!fgets(buffer, sizeof(buffer), fp))
	      break;
	    s = strchr(buffer, ':');
	    if (s) *s = 0;
	    s = buffer;
	    while (isspace(*s & 0xff)) ++s;

	    memset(&ifr, 0, sizeof(ifr));
	    strncpy(ifr.ifr_name, s, IFNAMSIZ-1);        
	    ifr.ifr_name[IFNAMSIZ-1] = 0;

	    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
	      fprintf(stderr, "nrconfig: SIOCGIFHWADDR: %s\n", strerror(errno));
	      return FALSE;
	    }

	    if (ifr.ifr_hwaddr.sa_family != ARPHRD_NETROM)
	      continue;

	    /* store found interface callsigns */
	    /* ax25_ntoa() returns pointer to static buffer */
	    s = ax25_ntoa((void*)ifr.ifr_hwaddr.sa_data);

	    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
	      fprintf(stderr, "nrconfig: SIOCGIFFLAGS: %s\n", strerror(errno));
	      return FALSE;
	    }

	    if (!(ifr.ifr_flags & IFF_UP))
	      continue;


            if ((pp = realloc(calllist, sizeof(char *) * (callcount+2))) == 0)
              break;
            calllist = pp;
            if ((pp = realloc(devlist,  sizeof(char *) * (callcount+2))) == 0)
              break;
            devlist  = pp;
            if ((calllist[callcount] = strdup(s)) != NULL) {
              if ((devlist[callcount] = strdup(ifr.ifr_name)) != NULL) {
                ++callcount;
                calllist[callcount] = NULL;
                devlist [callcount] = NULL;
              } else {
                free((void*)calllist[callcount]);
                calllist[callcount] = NULL;
		devlist[callcount] = NULL;
              }
            }
	  }
	  fclose(fp);
	  fp = NULL;
	}


	if ((fp = fopen(CONF_NRPORTS_FILE, "r")) == NULL) {
	  fprintf(stderr, "nrconfig: unable to open nrports file %s (%s)\n", CONF_NRPORTS_FILE, strerror(errno));
	  goto cleanup;
	}

	while (fp && fgets(buffer, 255, fp)) {
	  if ((s = strchr(buffer, '\n')))
	    *s = '\0';

	  if (strlen(buffer) > 0 && *buffer != '#')
	    if (nr_config_init_port(fd, lineno, buffer, calllist, devlist))
	      n++;

	  lineno++;
	}

 cleanup:;
	if (fd >= 0) close(fd);
	if (fp) fclose(fp);

	for(i = 0; calllist && calllist[i]; ++i) {
	  free((void*)calllist[i]);
	  if (devlist[i] != NULL)
	    free((void*)devlist[i]);
	}
	if (calllist) free(calllist);
	if (devlist) free(devlist);

	if (nr_ports == NULL)
		return 0;

	return n;
}
