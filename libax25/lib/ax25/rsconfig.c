#define _LINUX_STRING_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include <config.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <netax25/ax25.h>
#include <netax25/axlib.h>
#include <netrose/rose.h>
#include <netax25/rsconfig.h>

#include "pathnames.h"

typedef struct _rsport
{
	struct _rsport *Next;
	char *Name;
	char *Addr;
	char *Device;
	char *Description;
} RS_Port;

static RS_Port *rs_ports       = NULL;
static RS_Port *rs_port_tail   = NULL;

static RS_Port *rs_port_ptr(char *name)
{
	RS_Port *p = rs_ports;

	if (name == NULL)
		return p;

	while (p != NULL) {
		if (p->Name != NULL) {
			if (strcasecmp(name, p->Name) == 0)
				return p;
		}
		p = p->Next;
	}

	return NULL;
}

char *rs_config_get_next(char *name)
{
	RS_Port *p;
	
	if (rs_ports == NULL)
		return NULL;
		
	if (name == NULL)
		return rs_ports->Name;
		
	if ((p = rs_port_ptr(name)) == NULL)
		return NULL;
		
	p = p->Next;

	if (p == NULL)
		return NULL;
		
	return p->Name;
}

char *rs_config_get_name(char *device)
{
	RS_Port *p = rs_ports;

	while (p != NULL) {
		if (p->Device != NULL) {
			if (strcmp(device, p->Device) == 0)
				return p->Name;
		}
		p = p->Next;
	}

	return NULL;
}

char *rs_config_get_addr(char *name)
{
	RS_Port *p = rs_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Addr;
}

char *rs_config_get_dev(char *name)
{
	RS_Port *p = rs_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Device;
}

char *rs_config_get_port(rose_address *address)
{
	RS_Port *p = rs_ports;
	rose_address addr;

	while (p != NULL) {
		if (p->Addr != NULL) {
			rose_aton(p->Addr, addr.rose_addr);
	
			if (rose_cmp(address, &addr) == 0)
				return p->Name;
		}
		p = p->Next;
	}

	return NULL;
}

char *rs_config_get_desc(char *name)
{
	RS_Port *p = rs_port_ptr(name);

	if (p == NULL)
		return NULL;

	return p->Description;
}

int rs_config_get_paclen(char *name)
{
	return 128;
}

static int rs_config_init_port(int fd, int lineno, char *line, const char **ifcalls, const char **ifdevs)
{
	RS_Port *p;
	char *name, *addr, *desc;
	const char *dev = NULL;
	int found = 0;
	
	name   = strtok(line, " \t");
	addr   = strtok(NULL, " \t");
	desc   = strtok(NULL, "");

	if (name == NULL || addr == NULL || desc == NULL) {
		fprintf(stderr, "rsconfig: unable to parse line %d of config file\n", lineno);
		return FALSE;
	}

	for (p = rs_ports; p != NULL; p = p->Next) {
		if (p->Name != NULL && strcasecmp(name, p->Name) == 0) {
			fprintf(stderr, "rsconfig: duplicate port name %s in line %d of config file\n", name, lineno);
			return FALSE;
		}
		if (p->Addr != NULL && strcasecmp(addr, p->Addr) == 0) {
			fprintf(stderr, "rsconfig: duplicate address %s in line %d of config file\n", addr, lineno);
			return FALSE;
		}
	}

	found = 0;
	for (;ifcalls && *ifcalls; ++ifcalls, ++ifdevs) {
	  if (strcmp(addr,*ifcalls) == 0) {
	    found = 1;
	    dev = *ifdevs;
	    break;
	  }
	}

	if (!found) {
#if 0 /* None of your business to complain about some port being down... */
		fprintf(stderr, "rsconfig: port %s not active\n", name);
#endif
		return FALSE;
	}

	if ((p = (RS_Port *)malloc(sizeof(RS_Port))) == NULL) {
		fprintf(stderr, "rsconfig: out of memory!\n");
		return FALSE;
	}

	p->Name        = strdup(name);
	p->Addr        = strdup(addr);
	p->Device      = strdup(dev);
	p->Description = strdup(desc);

	if (rs_ports == NULL)
		rs_ports = p;
	else
		rs_port_tail->Next = p;

	rs_port_tail = p;

	p->Next = NULL;
	
	return TRUE;
}

int rs_config_load_ports(void)
{
	FILE *fp = NULL;
	char buffer[256], *s;
	int fd, lineno = 1, n = 0, i;
	const char **calllist = NULL;
	const char **devlist  = NULL;
	const char **pp;
	int callcount = 0;
	struct ifreq ifr;

	/* Reliable listing of all network ports on Linux
	   is only available via reading  /proc/net/dev ...  */


	if ((fd = socket(PF_FILE, SOCK_DGRAM, 0)) < 0) {
	  fprintf(stderr, "rsconfig: unable to open socket (%s)\n", strerror(errno));
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
	      fprintf(stderr, "rsconfig: SIOCGIFHWADDR: %s\n", strerror(errno));
	      return FALSE;
	    }

	    if (ifr.ifr_hwaddr.sa_family != ARPHRD_ROSE)
	      continue;

	    /* store found interface callsigns */
	    /* rose_ntoa() returns pointer to static buffer */
	    s = rose_ntoa((void*)ifr.ifr_hwaddr.sa_data);

	    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
	      fprintf(stderr, "rsconfig: SIOCGIFFLAGS: %s\n", strerror(errno));
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
              }
            }
	  }
	  fclose(fp);
	  fp = NULL;
	}


	if ((fp = fopen(CONF_RSPORTS_FILE, "r")) == NULL) {
	  fprintf(stderr, "rsconfig: unable to open axports file %s (%s)\n", CONF_RSPORTS_FILE, strerror(errno));
	  goto cleanup;
	}

	while (fp && fgets(buffer, 255, fp)) {
	  if ((s = strchr(buffer, '\n')))
	    *s = '\0';

	  if (strlen(buffer) > 0 && *buffer != '#')
	    if (rs_config_init_port(fd, lineno, buffer, calllist, devlist))
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

	if (rs_ports == NULL)
		return 0;

	return n;
}
