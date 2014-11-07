extern int ttyfd;
/*
 * this is a port of wampes ethertap whith my extension for bpqether
 *
 * (c) 20020630 Thomas Osterried  dl9sau
 * License: GPL
 */

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>

#ifdef  linux

#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif
#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(2,4,0)-1
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#define TRY_TUNTAP 1
#include <net/if.h>
#include <linux/if_tun.h>
#else
#include <linux/if.h>
#endif

#endif /* linux */

#include "ax25ipd.h"

#define ETAP_MTU_MIN    256
#define ETAP_MTU        1024
#define ETAP_MTU_MAX    1500
#ifndef	MAX_FRAME
#define MAX_FRAME       2048
#endif

#define ETHERTAP_HEADER_LEN_TUN		18
#define ETHERTAP_HEADER_LEN_ETHERTAP	16
#define	ETHERTAP_HEADER_LEN_MAX	ETHERTAP_HEADER_LEN_TUN

struct ethertap_packet {
  char ethernet_header[18];
  char data[MAX_FRAME];
};

unsigned char hwaddr_remote[6];

static int ethertap_header_len;

/*---------------------------------------------------------------------------*/

int send_bpq(unsigned char *buf, int l)
{
  struct ethertap_packet ethertap_packet;
  void *addr = &ethertap_packet;
  int offset = ETHERTAP_HEADER_LEN_MAX - ethertap_header_len;

  static const unsigned char ethernet_header[18] = {
    0x00, 0x00, 0x00, 0x02,                     /* ??? ??? ETH_P_AX25 (16bit) */ 
    0xfe, 0xfd, 0x00, 0x00, 0x00, 0x00,         /* Destination address (kernel et
hertap module) */
    0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00,         /* Source address (WAMPES etherta
p module) */
    0x08, 0xff                                  /* Protocol (bpqether) */
  };

  if (l <= 0)
    return -1;

  if (l > (sizeof(ethertap_packet.data) - 2))
    l = sizeof(ethertap_packet.data);
  memcpy(ethertap_packet.data + 2, buf, l);
  memcpy(ethertap_packet.ethernet_header, (const char *) ethernet_header, sizeof(ethernet_header));
  memcpy(ethertap_packet.ethernet_header + 4, hwaddr_remote, 6);
  memcpy(ethertap_packet.ethernet_header + 4 + 6 + 2, hwaddr_remote +2, 6 -2);
  ethertap_packet.data[0] = (l + 5) % 256;
  ethertap_packet.data[1] = (l + 5) / 256;
  l += 2;

  /*send_tty(addr + offset, l + sizeof(ethertap_packet.ethernet_header) - offset);*/
  write(ttyfd, addr + offset, l + sizeof(ethertap_packet.ethernet_header) - offset);
  return l;
}

/*---------------------------------------------------------------------------*/

int receive_bpq(unsigned char *buf, int l)
{
  if ((l -= ethertap_header_len) <= 0 ||
      (buf[ethertap_header_len-2] & 0xff) != 0x08 ||
      (buf[ethertap_header_len-1] & 0xff) != 0xff) {
    /* not a bpqether packet.
     * drop silently - ethernet.ax25 is not the only protocol spoken
     * on ethernet ;)
     */
    return -1;
  }
  l -= 2;
  if (l <= 0 && buf[ethertap_header_len] + buf[ethertap_header_len] * 256 - 5 != l) {
    /* length error in bpqether packet  */
    return 0;
  }

  from_kiss(buf + ethertap_header_len + 2, l);
  return l;
}

/*---------------------------------------------------------------------------*/
/* TUN/TAP support for linux. ethertap is obsolete */

#ifdef  TRY_TUNTAP
static int tun_alloc(char *dev)
{
  struct ifreq ifr;
  int fd, err;

  if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
     return -1;

  memset(&ifr, 0, sizeof(ifr));

  /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
   *        IFF_TAP   - TAP device
   *
   *        IFF_NO_PI - Do not provide packet information
   */
  ifr.ifr_flags = IFF_TAP;
  if (*dev) {
     strncpy(ifr.ifr_name, dev, IFNAMSIZ);
     ifr.ifr_name[IFNAMSIZ-1] = 0;
  }

  if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
     close(fd);
     return err;
  }
  strcpy(dev, ifr.ifr_name);

  /* persist mode */
#if 0
  if (ioctl(fd, TUNSETPERSIST, 1) < 0)
     perror("TUNSETPERSIST");
#endif

  /* don't checksum */
#if 0
  if (ioctl(fd, TUNSETNOCSUM, 1) < 0)
      perror("TUNSETNOCSUM");
#endif

  return fd;
}              
#endif

/*---------------------------------------------------------------------------*/

int open_ethertap(char *ifname)
{

  int fd;
  char devname[PATH_MAX];
  struct ifreq ifr;
  struct stat statbuf;
  int tuntap = 0;
  int mtu = 0;
  int skfd;
  
  strcpy(devname, "/dev/");
  strncpy(devname + 5, ifname, sizeof(devname) - 5 -1);
  devname[sizeof(devname) -1] = 0;

  /* tuntap == 0: original ethertap. works on a real character device  */
  if (!stat(devname, &statbuf)) {
    fd = open(devname, O_RDWR);
    if (fd < 0) {
      LOGL2("%s: %s\n", devname, strerror(errno));
      return -1;
    }
#ifdef  TRY_TUNTAP
  } else {
    strcpy(devname, ifname);
    if ((fd = tun_alloc(devname)) < 0) {
      LOGL2("%s: %s\n", devname, strerror(errno));
      return -1;
    }
    ifname = devname;
    tuntap = 1;
#endif
  }
  ethertap_header_len = (tuntap ? ETHERTAP_HEADER_LEN_TUN : ETHERTAP_HEADER_LEN_ETHERTAP);

  if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    close(fd);
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0;
  if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
    perror("SIOCGIFFLAGS");

  ifr.ifr_flags |= (IFF_UP | IFF_NOARP);
  if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0)
    perror("SIOCSIFFLAGS");

  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0;
  if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0) {
    perror("SIOCGIFHWADDR");
    hwaddr_remote[0] = 0xfe;
    hwaddr_remote[1] = 0xfd;
    hwaddr_remote[2] = 0x0;
    hwaddr_remote[3] = 0x0;
    hwaddr_remote[4] = 0x0;
    hwaddr_remote[5] = 0x0;
  } else
    memcpy(hwaddr_remote, ifr.ifr_hwaddr.sa_data, 6);

  if (ttyspeed > ETAP_MTU_MIN && ttyspeed != 9600) {
    mtu = ttyspeed;
  } else {
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ-1] = 0;
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(skfd, SIOCGIFMTU, &ifr) < 0)
      perror("SIOCGIFMTU");
    else
      mtu = ifr.ifr_mtu;
  }
  if (mtu < ETAP_MTU_MIN || mtu > ETAP_MTU_MAX)
    mtu = ETAP_MTU;
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0;
  ifr.ifr_mtu = mtu;
  if (ioctl(skfd, SIOCSIFMTU, &ifr) < 0)
    perror("SIOSGIFMTU");

  close(skfd);

  return fd;

}

/*---------------------------------------------------------------------------*/

int set_bpq_dev_call_and_up(char *ifname)
{
  FILE * fp;
  char buf[1024];
  char bpq_name[IFNAMSIZ];
  char dev_name[IFNAMSIZ];
  struct ifreq ifr;
  int drop;
  int skfd;
  int err = -1;

  /*
   * 1. find correspondent bpqether device in /proc (i.e. bpq0 for tap0)
   * 2. set the ax25 call of bpq0 (in this example)
   * 3. ifup bpq0
   */

  if (!(fp = fopen("/proc/net/bpqether", "r"))) {
    perror("/proc/net/bpqether");
    return -1;
  }

  /*
   * look up bpqether devce in /proc:
   *   dev   ether      destination        accept from
   *   bpq0  tap0       FF:FF:FF:FF:FF:FF  *
   */

  drop = 1;
  *bpq_name = *dev_name = 0;
  while (fgets(buf, sizeof(buf), fp)) {
    if (drop) {
      /* header line  */
      drop = 0;
      continue;
    }
    sscanf(buf, "%s %s ", bpq_name, dev_name);
    if (!strcmp(dev_name, ifname))
      break;
    *bpq_name = *dev_name = 0;
  }
  fclose(fp);
  if (!*bpq_name) {
    LOGL2("Did not found bpqether device for %s in /proc/net/bpqether: %s", ifr.ifr_name, strerror(errno));
    return -1;
  }

  LOGL1("found bpq device %s for %s\n", bpq_name, dev_name);

  if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket()");
    return -1;
  }
  memset(&ifr, 0, sizeof(ifr));
  memcpy(ifr.ifr_name, bpq_name, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0;

  ifr.ifr_hwaddr.sa_family = PF_AX25;
  memcpy(ifr.ifr_hwaddr.sa_data, mycallsign, 7);

  if (ioctl(skfd, SIOCSIFHWADDR, &ifr) < 0) {
    perror("SIOCSIFHWADDR");
    close(skfd);
    return -1;
  }
  strncpy(ifr.ifr_name, bpq_name, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0;
  ifr.ifr_mtu = 256;
  if (ioctl(skfd, SIOCSIFMTU, &ifr) < 0)
    perror("SIOSGIFMTU");

  strncpy(ifr.ifr_name, bpq_name, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0;
  if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0) {
    perror("SIOCGIFFLAGS");
    ifr.ifr_flags = 0;
  }

  ifr.ifr_flags |= (IFF_UP);
  if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0) {
    perror("SIOCSIFFLAGS");
    err = -1;
  }

  close(skfd);

  return err;
}
