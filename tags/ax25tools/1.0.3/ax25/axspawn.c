/*
 * axspawn.c - run a program from ax25d.
 *
 * Copyright (c) 1996 Joerg Reuter DL1BKE (jreuter@poboxes.com)
 *
 * This program is a hack.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *                
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * It might even kill your cat... ;-) 
 *
 * Status: alpha (still...)
 *
 * usage: change the "default" lines in your /usr/local/etc/ax25d.conf:
 *
 * 	default * * * * * 1 root /usr/local/sbin/axspawn axspawn
 *
 * a line like this would wait for an incoming info frame first.
 *
 * 	default * * * * * 1 root /usr/local/sbin/axspawn axspawn --wait
 *
 * The program will check if the peer is an AX.25 socket, the
 * callsign is a valid amateur radio callsign, strip the SSID, 
 * check if UID/GID are valid, allow a password-less login if the
 * password-entry in /etc/passwd is "+" or empty; in every other case
 * login will prompt for a password.
 *
 * Still on my TODO list: a TheNet compatible or MD5 based 
 * authentication scheme... Won't help you if you changed the "+"-entry
 * in /etc/passwd to a valid passord and login with telnet, though. 
 * A better solution could be a small program called from .profile.
 *
 * Axspawn can create user accounts automatically. You may specify
 * the user shell, first and maximum user id, group ID in the config
 * file and (unlike WAMPES) create a file "/usr/local/etc/ax25.profile" 
 * which will be copied to ~/.profile.
 *
 * This is an example for the config file:
 *
 * # this is /usr/local/etc/axspawn.conf
 * #
 * # allow automatic creation of user accounts
 * create    yes
 *
 * # allow empty password field (so user may login via telnet, or pop3 their
 * # mail) [default no]
 * create_empty_password	no
 *
 * # pwcheck method: password or call or group [default: password]
 * # "password" means, that passwords with '+' force a login without
 * #   prompting for a password (old behaviour; backward compatibility).
 * # "call" means, that ham calls via ax25/netrom/rose/.. should be able
 * #   to login without password, even if it's set (for e.g. to secure
 * #   from abuse of inet connections)
 * # "group" means, that if the gid of the user matches the configured
 * #  default user_gid, then the login is granted without password.
 * #pwcheck call
 * #pwcheck group
 * #pwcheck password
 *
 * # create with system utility useradd(8)? [default no]
 * create_with_useradd	no
 *
 * # guest user if above is 'no' or everything else fails. Disable with "no"
 * guest     ax25
 *
 * # group id or name for autoaccount
 * group     ax25
 *
 * # first user id to use
 * first_uid 400
 *
 * # maximum user id
 * max_uid   2000
 *
 * # where to add the home directory for the new user
 * home      /home/hams
 *
 * # secure homedirectories (g-rwx)
 * #secure_home yes
 *
 * # user's shell
 * shell     /bin/bash
 *
 * # bind user id to callsign for outgoing connects.
 * associate yes
 *
 * SECURITY:
 *
 * Umm... auto accounting is a security problem by definition. Unlike
 * WAMPES, which creates an empty password field, Axspawn adds an
 * "impossible" ('+') password to /etc/passwd. Login gets called with
 * the "-f" option, thus new users have the chance to login without
 * a password. (I guess this won't work with the shadow password system).
 * News: there are good reasons to use empty password fields. For e.g.
 *   at our mailbox-system db0tud where we have no inet access, so
 *   every user is a ham, not a hacker. We need the empty password
 *   so new may use ax25 for automatical account generation, and then
 *   could login via telnet or poll their mail with pop3.
 *   But i acknowledge, that this package will mostly used by hams who
 *   use their computer for both, inet and ampr. So the new option
 *   create_empty_password is not the default, but a feature
 *   needed because we won't patch this code after a new release.
 *   the useradd method and compression implementation is the the work
 *   of thomas <dg1rtf>, the finetune is by me. GPL
 *   the compression code initialy was for tnt by dl4ybg (GPL).
 *     - dl9sau for db0tud team.
 *
 * The "associate" option has to be used with great care. If a user
 * logs on it removes any existing callsign from the translation table
 * for this userid and replaces it with the callsign and SSID of the
 * user. This will happen with multiple connects (same callsign,
 * different SSIDs), too. Unless you want your users to be able
 * to call out from your machine disable "associate".
 *
 * Of course Axspawn does callsign checking: Only letters and numbers
 * are allowed, the callsign must be longer than 4 characters and
 * shorter than 6 characters (without SSID). There must be at least
 * one digit, and max. two digits within the call. The SSID must
 * be within the range of 0 and 15. Please drop me a note if you
 * know a valid Amateur Radio (sic!) callsign that does not fit this 
 * pattern _and_ can be represented correctly in AX.25.
 *
 * It uses the forkpty from libbsd.a (found after analyzing logind)
 * which has no prototype in any of my .h files.
 *
 */

/* removed -h <protocol> from login command as it was causing hostname lookups
   with new login/libc - Terry, vk2ktj. */


/*#define QUEUE_DELAY 400		/ * 400 usec */
#define QUEUE_DELAY 100000		/* 10 msec */
#define USERPROFILE ".profile"
#define PASSWDFILE  "/etc/passwd"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <utmp.h>
#include <paths.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/wait.h>

#include <config.h>

#include <sys/socket.h>

#include <netax25/ax25.h>
#include <netrose/rose.h>

#include <netax25/axlib.h>

#include "../pathnames.h"
#include "axspawn.h"
#include "access.h"

#define MAXLEN strlen("DB0PRA-15")
#define MINLEN strlen("KA9Q")

#define AX_PACLEN	256
#define	NETROM_PACLEN	236
#define	ROSE_PACLEN	128

#define IS_DIGIT(x)  ( (x >= '0') && (x <= '9') )
#define IS_LETTER(x) ( (x >= 'A') && (x <= 'Z') )

#define MSG_NOCALL	"Sorry, you are not allowed to connect.\n"
#define MSG_CANNOTFORK	"Sorry, system is overloaded.\n"
#define MSG_NOPTY	"Sorry, all channels in use.\n"
#define MSG_NOTINDBF	"Sorry, you are not in my database.\n"

#define write_ax25_static_line(s) { \
  char *msg = strdup(s); \
  if (msg) { \
    write_ax25(msg, strlen(msg), 1); \
    free(msg); \
  } \
}

#define EXITDELAY	10

#define	USERADD_CONF	"/etc/default/useradd"

struct huffencodtab {
	unsigned short code;
	unsigned short len;
};

struct huffdecodtab {
	unsigned short node1;
	unsigned short node2;
};



/* huffman encoding table */
static struct huffencodtab huffencodtab[] = {
{0xab2c,15},{0xaa84,15},{0x9fc4,15},{0xab3c,15},
{0xab1c,15},{0xaafc,15},{0xaaec,15},{0xaad4,15},
{0xaab4,15},{0xf340,10},{0xaaa4,15},{0x7d64,15},
{0xaadc,15},{0xf400, 7},{0xaa94,15},{0x9ff4,15},
{0x9fd4,15},{0x7d74,15},{0xab44,15},{0xab34,15},
{0xab24,15},{0xab14,15},{0xab04,15},{0xaaf4,15},
{0xaae4,15},{0xab60,14},{0xab0c,15},{0xaacc,15},
{0xaabc,15},{0xaaac,15},{0xaa9c,15},{0xaa8c,15},
{0xc000, 3},{0x3a80, 9},{0xabc0,10},{0x0060,11},
{0x7d40,12},{0xab5c,14},{0x0000,12},{0xab58,14},
{0x7c00, 9},{0x3c80, 9},{0x7d00,11},{0x0010,12},
{0x1200, 7},{0x7a00, 7},{0xb800, 6},{0x3200, 7},
{0x2200, 7},{0xf600, 8},{0x3d00, 8},{0x9e00, 9},
{0xbd80, 9},{0x7c80, 9},{0x0080, 9},{0xaa00, 9},
{0xbd00, 9},{0x9f00, 9},{0x0300, 8},{0xab78,13},
{0xab68,13},{0x3c00, 9},{0x3000, 9},{0x0020,11},
{0x7d50,12},{0x3800, 7},{0x7800, 7},{0x9c00, 7},
{0xfe00, 7},{0x2400, 6},{0xbc00, 8},{0x0200, 8},
{0x0100, 8},{0xf100, 8},{0x0040,11},{0x3100, 8},
{0xf200, 8},{0x3400, 7},{0x1c00, 7},{0x1e00, 7},
{0xbe00, 7},{0xaba0,11},{0x3e00, 7},{0x1400, 6},
{0x3600, 7},{0xf380, 9},{0xf080, 9},{0x2000, 8},
{0xfc00, 8},{0x9f80,10},{0x9e80, 9},{0xab90,12},
{0x3b80, 9},{0xab80,12},{0xab54,14},{0x3a50,13},
{0xab50,14},{0xa000, 5},{0x1800, 6},{0x9800, 6},
{0x7000, 5},{0x4000, 3},{0x0400, 6},{0xac00, 6},
{0xf800, 6},{0x6000, 4},{0x3a00,10},{0xfd00, 8},
{0x2800, 5},{0xb000, 6},{0x8000, 4},{0xb400, 6},
{0x1000, 7},{0x7d20,12},{0xe000, 5},{0x9000, 5},
{0xe800, 5},{0x0800, 5},{0xf700, 8},{0xa800, 7},
{0x7d80, 9},{0xf300,10},{0x7e00, 7},{0xab48,14},
{0x3a48,13},{0xab4c,14},{0x3a60,12},{0x9ffc,15},
{0x9fec,15},{0x2100, 8},{0x9fdc,15},{0x9fcc,15},
{0xf000, 9},{0x7d7c,15},{0x7d6c,15},{0x3a40,14},
{0xab40,15},{0xab38,15},{0xab30,15},{0xab28,15},
{0xab20,15},{0xab18,15},{0xab70,13},{0xab10,15},
{0xab08,15},{0xab00,15},{0xaaf8,15},{0xaaf0,15},
{0x3b00, 9},{0xaae8,15},{0xaae0,15},{0xaad8,15},
{0xaad0,15},{0xab64,14},{0x7d30,12},{0xaac8,15},
{0xaac0,15},{0xaab8,15},{0xaab0,15},{0xaaa8,15},
{0xaaa0,15},{0xaa98,15},{0xaa90,15},{0xaa88,15},
{0xaa80,15},{0x9ff8,15},{0x9ff0,15},{0x9fe8,15},
{0x9fe0,15},{0x9fd8,15},{0x9fd0,15},{0x9fc8,15},
{0x9fc0,15},{0x7d78,15},{0x7d70,15},{0x3a58,13},
{0x7d68,15},{0x7d60,15},{0xab46,15},{0xab42,15},
{0xab3e,15},{0xab3a,15},{0xab36,15},{0xab32,15},
{0xab2e,15},{0xab2a,15},{0xab26,15},{0xab22,15},
{0xab1e,15},{0xab1a,15},{0xab16,15},{0xab12,15},
{0xab0e,15},{0xab0a,15},{0xab06,15},{0xab02,15},
{0xaafe,15},{0xaafa,15},{0xaaf6,15},{0xaaf2,15},
{0xaaee,15},{0xaaea,15},{0xaae6,15},{0xaae2,15},
{0xaade,15},{0xaada,15},{0xaad6,15},{0xaad2,15},
{0xaace,15},{0xaaca,15},{0xaac6,15},{0xaac2,15},
{0xaabe,15},{0xaaba,15},{0xaab6,15},{0xaab2,15},
{0xaaae,15},{0xaaaa,15},{0xaaa6,15},{0xaaa2,15},
{0xaa9e,15},{0x3a70,12},{0xaa9a,15},{0xaa96,15},
{0xaa92,15},{0x3080, 9},{0xaa8e,15},{0xaa8a,15},
{0xaa86,15},{0xaa82,15},{0x9ffe,15},{0x9ffa,15},
{0x9ff6,15},{0x9ff2,15},{0x9fee,15},{0x9fea,15},
{0x9fe6,15},{0x9fe2,15},{0x9fde,15},{0x9fda,15},
{0x9fd6,15},{0x9fd2,15},{0x9fce,15},{0x9fca,15},
{0x9fc6,15},{0x9fc2,15},{0x7d7e,15},{0x7d7a,15},
{0x7d76,15},{0x7d72,15},{0x7d6e,15},{0x7d6a,15},
{0x7d66,15},{0x7d62,15},{0x3a46,15},{0x3a44,15}
};

/* huffman decoding table */
static struct huffdecodtab huffdecodtab[] = {
{ 79,  1},{  2, 66},{ 24,  3},{  4,208},
{292,  5},{  6,298},{317,  7},{ 16,  8},
{  9,173},{ 10,116},{ 41, 11},{ 12, 37},
{125, 13},{357, 14},{ 15,438},{  0,  0},
{229, 17},{ 18, 46},{ 19, 61},{ 20, 99},
{ 21,161},{404, 22},{ 23,483},{  1,  0},
{306, 25},{313, 26},{294, 27},{245, 28},
{221, 29},{231, 30},{277, 31},{ 32,103},
{ 33,108},{ 34,339},{421, 35},{ 36,500},
{  2,  0},{122, 38},{353, 39},{ 40,434},
{  3,  0},{131, 42},{128, 43},{361, 44},
{ 45,442},{  4,  0},{ 56, 47},{ 52, 48},
{135, 49},{370, 50},{ 51,450},{  5,  0},
{138, 53},{375, 54},{ 55,454},{  6,  0},
{148, 57},{ 58, 94},{381, 59},{ 60,460},
{  7,  0},{ 75, 62},{ 63,152},{392, 64},
{ 65,469},{  8,  0},{164, 67},{311, 68},
{ 69,246},{ 70, 97},{253, 71},{257, 72},
{ 73,270},{319, 74},{  9,  0},{ 76,155},
{396, 77},{ 78,473},{ 10,  0},{165, 80},
{296, 81},{300, 82},{295, 83},{206, 84},
{ 85,320},{193, 86},{ 87,318},{199, 88},
{184, 89},{ 90,112},{ 91,346},{430, 92},
{ 93,508},{ 11,  0},{379, 95},{ 96,458},
{ 12,  0},{ 98,218},{ 13,  0},{100,158},
{400,101},{102,478},{ 14,  0},{331,104},
{105,328},{408,106},{107,487},{ 15,  0},
{109,336},{417,110},{111,496},{ 16,  0},
{113,343},{425,114},{115,504},{ 17,  0},
{117,141},{118,186},{119,321},{351,120},
{121,432},{ 18,  0},{355,123},{124,436},
{ 19,  0},{359,126},{127,440},{ 20,  0},
{364,129},{130,444},{ 21,  0},{132,145},
{368,133},{134,448},{ 22,  0},{372,136},
{137,452},{ 23,  0},{377,139},{140,456},
{ 24,  0},{142,234},{143,236},{144,383},
{ 25,  0},{366,146},{147,446},{ 26,  0},
{387,149},{385,150},{151,462},{ 27,  0},
{390,153},{154,467},{ 28,  0},{394,156},
{157,471},{ 29,  0},{398,159},{160,475},
{ 30,  0},{402,162},{163,481},{ 31,  0},
{ 32,  0},{175,166},{214,167},{211,168},
{169,195},{243,170},{171,281},{286,172},
{ 33,  0},{265,174},{ 34,  0},{176,202},
{177,315},{178,297},{179,232},{180,252},
{181,228},{189,182},{255,183},{ 35,  0},
{185,242},{ 36,  0},{284,187},{192,188},
{ 37,  0},{190,241},{191,201},{ 38,  0},
{ 39,  0},{194,227},{ 40,  0},{196,267},
{197,220},{237,198},{ 41,  0},{200,309},
{ 42,  0},{ 43,  0},{203,260},{204,268},
{308,205},{ 44,  0},{244,207},{ 45,  0},
{304,209},{210,223},{ 46,  0},{212,258},
{238,213},{ 47,  0},{215,303},{216,249},
{273,217},{ 48,  0},{219,316},{ 49,  0},
{ 50,  0},{222,278},{ 51,  0},{224,264},
{250,225},{230,226},{ 52,  0},{ 53,  0},
{ 54,  0},{ 55,  0},{ 56,  0},{ 57,  0},
{251,233},{ 58,  0},{363,235},{ 59,  0},
{ 60,  0},{ 61,  0},{239,256},{240,480},
{ 62,  0},{ 63,  0},{ 64,  0},{ 65,  0},
{ 66,  0},{ 67,  0},{299,247},{275,248},
{ 68,  0},{ 69,  0},{ 70,  0},{ 71,  0},
{ 72,  0},{271,254},{ 73,  0},{ 74,  0},
{ 75,  0},{ 76,  0},{259,269},{ 77,  0},
{293,261},{262,263},{ 78,  0},{ 79,  0},
{ 80,  0},{279,266},{ 81,  0},{ 82,  0},
{ 83,  0},{ 84,  0},{ 85,  0},{342,272},
{ 86,  0},{274,335},{ 87,  0},{276,302},
{ 88,  0},{ 89,  0},{ 90,  0},{283,280},
{ 91,  0},{374,282},{ 92,  0},{ 93,  0},
{291,285},{ 94,  0},{301,287},{288,326},
{323,289},{290,427},{ 95,  0},{ 96,  0},
{ 97,  0},{ 98,  0},{ 99,  0},{100,  0},
{101,  0},{102,  0},{103,  0},{104,  0},
{105,  0},{106,  0},{107,  0},{108,  0},
{305,307},{109,  0},{110,  0},{111,  0},
{112,  0},{310,384},{113,  0},{312,314},
{114,  0},{115,  0},{116,  0},{117,  0},
{118,  0},{119,  0},{120,  0},{121,  0},
{122,  0},{322,325},{123,  0},{349,324},
{124,  0},{125,  0},{327,476},{126,  0},
{406,329},{330,485},{127,  0},{412,332},
{410,333},{334,489},{128,  0},{129,  0},
{415,337},{338,494},{130,  0},{419,340},
{341,498},{131,  0},{132,  0},{423,344},
{345,502},{133,  0},{428,347},{348,506},
{134,  0},{350,510},{135,  0},{352,433},
{136,  0},{354,435},{137,  0},{356,437},
{138,  0},{358,439},{139,  0},{360,441},
{140,  0},{362,443},{141,  0},{142,  0},
{365,445},{143,  0},{367,447},{144,  0},
{369,449},{145,  0},{371,451},{146,  0},
{373,453},{147,  0},{148,  0},{376,455},
{149,  0},{378,457},{150,  0},{380,459},
{151,  0},{382,461},{152,  0},{153,  0},
{154,  0},{386,463},{155,  0},{388,464},
{389,466},{156,  0},{391,468},{157,  0},
{393,470},{158,  0},{395,472},{159,  0},
{397,474},{160,  0},{399,477},{161,  0},
{401,479},{162,  0},{403,482},{163,  0},
{405,484},{164,  0},{407,486},{165,  0},
{409,488},{166,  0},{411,490},{167,  0},
{413,491},{414,493},{168,  0},{416,495},
{169,  0},{418,497},{170,  0},{420,499},
{171,  0},{422,501},{172,  0},{424,503},
{173,  0},{426,505},{174,  0},{175,  0},
{429,507},{176,  0},{431,509},{177,  0},
{178,  0},{179,  0},{180,  0},{181,  0},
{182,  0},{183,  0},{184,  0},{185,  0},
{186,  0},{187,  0},{188,  0},{189,  0},
{190,  0},{191,  0},{192,  0},{193,  0},
{194,  0},{195,  0},{196,  0},{197,  0},
{198,  0},{199,  0},{200,  0},{201,  0},
{202,  0},{203,  0},{204,  0},{205,  0},
{206,  0},{207,  0},{208,  0},{209,  0},
{  0,465},{210,  0},{211,  0},{212,  0},
{213,  0},{214,  0},{215,  0},{216,  0},
{217,  0},{218,  0},{219,  0},{220,  0},
{221,  0},{222,  0},{223,  0},{224,  0},
{225,  0},{226,  0},{227,  0},{228,  0},
{229,  0},{230,  0},{231,  0},{232,  0},
{233,  0},{234,  0},{235,  0},{  0,492},
{236,  0},{237,  0},{238,  0},{239,  0},
{240,  0},{241,  0},{242,  0},{243,  0},
{244,  0},{245,  0},{246,  0},{247,  0},
{248,  0},{249,  0},{250,  0},{251,  0},
{252,  0},{253,  0},{512,511},{254,  0},
{255,  0}
};

static unsigned char mask8tab[8] = {
  0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01
};

static unsigned short mask16tab[16] = {
  0x8000,0x4000,0x2000,0x1000,0x0800,0x0400,0x0200,0x0100,
  0x0080,0x0040,0x0020,0x0010,0x0008,0x0004,0x0002,0x0001
};


char   policy_add_user = 1;
char   policy_add_empty_password = 0;
char   policy_add_prog_useradd = 0;
char   policy_guest = 1;
char   policy_associate = 0;
char   pwcheck = 1;
char   secure_home = 0;

gid_t  user_gid =  400;
char   *user_shell = "/bin/bash";
int    user_shell_configured = 0;
char   *start_home = "/home/hams";
char   *guest = "guest";
int    start_uid = 400;
int    end_uid   = 65535;
int    paclen    = ROSE_PACLEN;		/* Its the shortest ie safest */

int    huffman = 0;
int    bin = 0;
int    fdmaster;

struct write_queue {
	struct write_queue *next;
	char *data;
	unsigned int  len;
};

struct write_queue *wqueue_head = NULL;
struct write_queue *wqueue_tail = NULL;
long wqueue_length = 0L;


int encstathuf(char *src, int srclen, char *dest, int *destlen);
int decstathuf(char *src, char *dest, int srclen, int *destlen);

/*---------------------------------------------------------------------------*/

/* Use private function because some platforms are broken, eg 386BSD */

int Xtolower(int c)
{
  return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
}

/*---------------------------------------------------------------------------*/

/* Use private function because some platforms are broken, eg 386BSD */

int Xtoupper(int c)
{
  return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
}

/*---------------------------------------------------------------------------*/

int Strcasecmp(const char *s1, const char *s2)
{
  while (Xtolower(*s1) == Xtolower(*s2)) {
    if (!*s1) return 0;
    s1++;
    s2++;
  }
  return Xtolower(*s1) - Xtolower(*s2);
}

/*---------------------------------------------------------------------------*/

int Strncasecmp(const char *s1, const char *s2, int n)
{
  while (--n >= 0 && Xtolower(*s1) == Xtolower(*s2)) {
    if (!*s1) return 0;
    s1++;
    s2++;
  }
  return n < 0 ? 0 : Xtolower(*s1) - Xtolower(*s2);
}

/* This one is in /usr/lib/libbsd.a, but not in bsd.h and fellows... weird. */
/* (found in logind.c)							    */

pid_t forkpty(int *, char *, void *, struct winsize *);

/* The buffer in src (first byte length-1) is decoded into dest
   (first byte length-1). If decoding is not successful, non-0 
   is returned
*/

int encstathuf(char *src, int srclen, char *dest, int *destlen)
{
	char *srcptr;
	char *destptr;
	int wrklen;
	int bit16;
	int bit8;
	unsigned short huffcode;
	int hufflen;
	
	if ((src == NULL) || (dest == NULL)) {
		syslog(LOG_NOTICE, "Huffman encode: src or dest NULL!");
		return(1);
	}
	if (srclen > 255) {
		syslog(LOG_NOTICE, "Huffman encode: srclen > 255: %d", srclen);
		return(1);
	}
	srcptr = src;
	destptr = &dest[1];
	*destlen = 0;
	wrklen = 0;
	bit8 = 0;
	*destptr = 0;
	huffcode = huffencodtab[(int)*srcptr].code;
	hufflen = huffencodtab[(int)*srcptr].len;
	bit16 = 0;
	for (;;) {
		if (huffcode & mask16tab[bit16])
			*destptr |= mask8tab[bit8];
		bit8++;
		if (bit8 > 7) {
			destptr++;
			(*destlen)++;
			if ((*destlen) >= srclen) {
				/* coding uneffective, use copy */
				memcpy(&dest[1],src,srclen);
				dest[0] = 255;
				*destlen = srclen + 1;
				return(0);
			}
			bit8 = 0;
			*destptr = 0;
		}
		bit16++;
		if (bit16 == hufflen) {
			srcptr++;
			wrklen++;
			if (wrklen == srclen) break;
			huffcode = huffencodtab[(int)*srcptr].code;
			hufflen = huffencodtab[(int)*srcptr].len;
			bit16 = 0;
		}
	}
	if (bit8 != 0) (*destlen)++;
	(*destlen)++;
	dest[0] = (char)(srclen-1);
	return(0);
}

/* The buffer in src (first byte length-1) is decoded into dest
	 (first byte length-1). If decoding is not successful, non-0 
   is returned
*/


int decstathuf(char *src, char *dest, int srclen, int *destlen)
{
	unsigned char *srcptr;
	unsigned char *destptr;
	int wrklen;
	unsigned char bitmask;
	unsigned short decod;
	unsigned short newnode;

	if ((src == NULL) || (dest == NULL)) return(1);

	srcptr = src;
	destptr = dest;
	*destlen = (int)((*srcptr)+1);
	srcptr++;
	if (--srclen == 0) {
		return(1);
	}
	if (*destlen == 0) {
		return(1);
	}

	if (*destlen == 256) {
		/* no compression, only copy */
		memcpy(dest,src+1,srclen);
		*destlen = (unsigned char)(srclen);
		return(0);
	}
	wrklen = 0;
	decod = 0;
	bitmask = 0x80;
	for (;;) {
		while (bitmask > 0) {
			if ((*srcptr) & bitmask) {
				newnode = huffdecodtab[decod].node2;
				if (newnode == 0) {
					*destptr = (char)huffdecodtab[decod].node1;
					destptr++;
					wrklen++;
					if (wrklen >= *destlen) break; /* decoding finished */
					decod = 0;
				}
				else decod = newnode;
			}
			else {
				newnode = huffdecodtab[decod].node1;
				if (huffdecodtab[decod].node2 == 0) {
					*destptr = (char)huffdecodtab[decod].node1;
					destptr++;
					wrklen++;
					if (wrklen >= *destlen) break; /* decoding finished */
					decod = 0;
				}
				else decod = newnode;
			}
			if (decod) bitmask = bitmask >> 1;
		}
		if (wrklen >= *destlen) break;
		bitmask = 0x80;
		srcptr++;
		if (srclen == 0) return(1);
		srclen--;
	}
	return(0);
}


int _write_ax25(const char *s, int len)
{
	int i;
	if (!len)
		return 0;

	i = write(1, s, len);
	fflush(stdout);
	return (i > 0 ? i : 0);	/* on error, -1 is returned  */
}

int read_ax25(unsigned char *s, int size)
{
	int len;
	int k;
	char buffer[255];
	char decomp[260];
	int declen;
	struct termios termios;

	if ((len = read(0, s, size)) < 0)
		return len;

	if (huffman) {
		if (!decstathuf(s, decomp, len, &declen))  {
			 *(decomp+declen) = 0;
			strcpy(s, decomp);
			len = declen;
		}
	}
	if (bin) {
		return(len);
	}
	

	for (k = 0; k < len; k++)
		if (s[k] == '\r') s[k] = '\n';
		

	if (!huffman && !Strncasecmp(s, "//COMP ON\n", 10)) {
		sprintf(buffer,"\r//COMP 1\r");
		write_ax25(buffer, strlen(buffer), 1);
		sleep(1);
		memset((char *) &termios, 0, sizeof(termios));
		termios.c_iflag = ICRNL | IXOFF | IGNBRK;
		termios.c_oflag = 0;
		termios.c_cflag = CS8 | CREAD | CLOCAL;
		termios.c_lflag = 0;
		termios.c_cc[VMIN] = 0;
		termios.c_cc[VTIME] = 0;
		tcsetattr(0, TCSANOW, &termios);

		huffman = 1;
		strcpy(s,"\n");
		return 1;
	}

	if (huffman && !Strncasecmp(s, "//COMP OFF\n", 11)) {
		sprintf(buffer,"\r//COMP 0\r"); fflush(stdout);
		write_ax25(buffer, strlen(buffer), 1);
		sleep(1);
		huffman = 0;
		memset((char *) &termios, 0, sizeof(termios));
		termios.c_iflag = ICRNL | IXOFF;
		termios.c_oflag = OPOST | ONLCR;
		termios.c_cflag = CS8 | CREAD | CLOCAL;
		termios.c_lflag = ISIG | ICANON;
		termios.c_cc[VINTR]  = 127;
		termios.c_cc[VQUIT]  =	28;
		termios.c_cc[VERASE] =   8;
		termios.c_cc[VKILL]  =	24;
		termios.c_cc[VEOF]   =   4;
		cfsetispeed(&termios, B19200);
		cfsetospeed(&termios, B19200);
		tcsetattr(0, TCSANOW, &termios);

		strcpy(s,"\n");
		return 1;
	}

	return len;
}

/*
 *  We need to buffer the data from the pipe since bash does
 *  a fflush() on every output line. We don't want it, it's
 *  PACKET radio, isn't it?
 */

void kick_wqueue(int dummy)
{
	char *s, *p;
	struct write_queue *w_buf, *new_head;
	char *q, *r;
	int i;
	int len, curr_len;
	struct itimerval itv;
	int bufsize = (huffman ? 256-1 : paclen);
	char decoded[260];
	int  declen;
	
	signal(SIGALRM, SIG_IGN);
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = QUEUE_DELAY;
	for (;;) {
		if (wqueue_length == 0) {
			return;
		}

		/* recompute waitqueue  */
		if (wqueue_head->len < bufsize && wqueue_head->next) {
			int s_len;
			if (!(s = malloc(bufsize))) {
				break;
			}
			if (!(new_head = malloc(sizeof(struct write_queue)))) {
				free(s);
				break;
			}
			p = s;
			s_len = 0;
	
			while ((w_buf = wqueue_head)) {
				curr_len = (w_buf->len > bufsize-s_len ? bufsize-s_len : w_buf->len);
				memcpy(p, w_buf->data, curr_len);
				s_len += curr_len;
				p += curr_len;
				if (w_buf->len > curr_len) {
					/* rewrite current buffer, and break */
					w_buf->len -= curr_len;
		        		for (q = w_buf->data, r = w_buf->data+curr_len, i = 0; i < w_buf->len; i++)
						*q++ = *r++;
					break;
				}
				wqueue_head = w_buf->next;
				free(w_buf->data);
				free(w_buf);
			}
			new_head->data = s;
			new_head->len = s_len;
			new_head->next = wqueue_head;
			wqueue_head = new_head;
		}

		w_buf = wqueue_head;
		curr_len = w_buf->len > bufsize ? bufsize : w_buf->len;
		if (huffman && !encstathuf(w_buf->data,curr_len,decoded,&declen)) {
			*(decoded+declen) = 0;
			s = decoded;
			len = declen;
		} else {
			s = w_buf->data;
			len = curr_len;
		}
		if (!_write_ax25(s, len) && errno == EAGAIN) {
			/*
			 * socket busy?
			 * don't block, user may want interrupt the jam
			 */
			itv.it_value.tv_sec = 1;
			break;
		}
		wqueue_length -= curr_len;
		if (w_buf->len > curr_len) {
			/*
			 * there's still data to be written - copy restbuffer
			 * to left
			 */
			w_buf->len -= curr_len;
			for (q = w_buf->data, r = w_buf->data+curr_len, i = 0; i < w_buf->len; i++)
				*q++ = *r++;
		} else {
			wqueue_head = w_buf->next;
			free(w_buf->data);
			free(w_buf);
	
			if (!wqueue_head) {
				wqueue_tail = NULL;
				return;
			}
		}

		/*
		 * if only a few bytes are left, wait a few ms if
		 * new data is available in order to send "full packets"
		 */
		if (wqueue_length < paclen)
			break;	
	}

	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;

	setitimer(ITIMER_REAL, &itv, 0);
	signal(SIGALRM, kick_wqueue);
}

int write_ax25(char *s, int len, int kick)
{
	struct itimerval itv, oitv;
	struct write_queue * buf;
	struct termios termios;
	static struct termios save_termios;
	static struct termios save_termios_master;
	static int last_ended_with_CR = 0;

	int i = 0;
	int j = 0;
	char *p;
	
	if (!len)
		return 0;
	signal(SIGALRM, SIG_IGN);

	if (!bin && !strncmp(s, "//BIN ON\r", 9)) {
		tcgetattr(fdmaster, &save_termios_master);
		tcgetattr(0, &save_termios);
		memset((char *) &termios, 0, sizeof(termios));
		termios.c_iflag = IGNBRK | IGNPAR;
		termios.c_oflag = 0;
		termios.c_cflag = CBAUD | CS8 | CREAD | CLOCAL;
		termios.c_cflag = ~(CSTOPB|PARENB|PARODD|HUPCL);
		termios.c_lflag = 0;
		termios.c_cc[VMIN] = 1;
		termios.c_cc[VTIME] = 0;
		termios.c_cc[VSTART] = -1;
		termios.c_cc[VSTOP] = -1;
		tcsetattr(0, TCSANOW, &termios);
		tcsetattr(fdmaster, TCSANOW, &termios);
		*s= 0; len=0;
		bin = 1;
		kick_wqueue(0);
		return 0;
	 }

	if (bin && !strncmp(s, "//BIN OFF\r", 10)) {
		kick_wqueue(0);
		bin = 0;
		tcsetattr(fdmaster, TCSANOW, &save_termios_master);
		tcsetattr(0, TCSANOW, &save_termios);
		last_ended_with_CR = 0;
		return 0;
	 } 


	if (!bin) {
		p = s; i = 0; j = 0;
		if (last_ended_with_CR) {
			/*
			 * \r\n was splited. wrote already \r, now ommiting \n
			 */
			if (*s == '\n') {
				s++, p++;
				len--;
			}
			last_ended_with_CR = 0;
			if (!len) {
				if (wqueue_head)
					kick_wqueue(0);
				return 0;
			}
		}
		while (j < len) {
			if ((j + 1 < len) && *(p + j) == '\r' && *(p + j + 1) == '\n') {
		  		*(p + i) = '\r';
				j++;
			}
			else
				*(p + i) = *(p + j);
			i++; j++;										
		}
		len = i;
		if (len && s[len-1] == '\r')
			last_ended_with_CR = 1;
		*(p+len) = 0;
	}

	buf = (struct write_queue *) malloc(sizeof(struct write_queue));
	if (buf == NULL)
		return 0;

	buf->data = (char *) malloc(len);
	if (buf->data == NULL) {
		free(buf);
		return 0;
	}

	memcpy(buf->data, s, len);
	buf->len = len;
	buf->next = NULL;	
	
	if (wqueue_head == NULL)
	{
		wqueue_head = buf;
		wqueue_tail = buf;
		wqueue_length = len;
	} else {
		wqueue_tail->next = buf;
		wqueue_tail = buf;
		wqueue_length += len;
	}
	
	if (wqueue_length > 7*paclen || kick || bin)
	{
		kick_wqueue(0);
	} else {
		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = 0;
		itv.it_value.tv_sec = 0;
		itv.it_value.tv_usec = QUEUE_DELAY;
		setitimer(ITIMER_REAL, &itv, &oitv);
		signal(SIGALRM, kick_wqueue);
	}
	return len;
}

int get_assoc(struct sockaddr_ax25 *sax25)
{
	FILE *fp;
	int  uid;
	char buf[81];
	
	fp = fopen(PROC_AX25_CALLS_FILE, "r");
	if (!fp) return -1;
	
	fgets(buf, sizeof(buf)-1, fp);
	
	while(!feof(fp))
	{
		if (fscanf(fp, "%d %s", &uid, buf) == 2)
			if (sax25->sax25_uid == uid)
			{
				ax25_aton_entry(buf, (char *) &sax25->sax25_call);
				fclose(fp);
				return 0;
			}
	}
	fclose(fp);
	
	return -1;
}


void cleanup(char *tty)
{
	struct utmp ut, *ut_line;
	struct timeval tv;
	FILE *fp;


	setutent();
	ut.ut_type = LOGIN_PROCESS;
	strncpy(ut.ut_id, tty + 3, sizeof(ut.ut_id));
	ut_line = getutid(&ut);

	if (ut_line != NULL) {
		ut_line->ut_type = DEAD_PROCESS;
		ut_line->ut_host[0] = '\0';
		ut_line->ut_user[0] = '\0';
		gettimeofday(&tv, NULL);
		ut_line->ut_tv.tv_sec = tv.tv_sec;
		ut_line->ut_tv.tv_usec = tv.tv_usec;
		if ((fp = fopen(_PATH_WTMP, "r+")) != NULL) {
			fseek(fp, 0L, SEEK_END);
			if (fwrite(ut_line, sizeof(ut), 1, fp) != 1)
				syslog(LOG_ERR, "Ooops, I think I've just barbecued your wtmp file\n");
			fclose(fp);
		}
	}

	endutent();
}


/* On debian wheezy, useradd (from passwd / shadow package) is buggy.
 * Short before end of main, it starts the external program nscd for
 * cleaning user / group cache. Their handler for waiting the forked
 * process waits for child's or -1 and errno EINTR with waitpid() - else
 * it loops again.
 * When useradd is started by axspawn, their waitpid() returns -1 errno ECHILD,
 * which leads their do-while-function go into an endless loop, eating
 * 100% CPU power.
 * If we mask SIGCHLD before execve(), useradd works as it should.
 */

void signal_handler_sigchild(int dummy)
{
	exit(0);
}
			
/* 
 * add a new user to /etc/passwd and do some init
 */

void new_user(char *newuser)
{
	struct passwd pw, *pwp;
	uid_t uid;
	char command[1024];
	FILE *fp;
	char username[80];
	char homedir[256], userdir[256];
	char buf[4096];
	char subdir[4];
	int cnt;
	unsigned char *q;
	char *p;
	struct stat fst;
	int fd_a, fd_b, fd_l;
	mode_t homedir_mode = S_IRUSR|S_IWUSR|S_IXUSR|S_IXOTH|(secure_home ? 0 : (S_IRGRP|S_IXGRP));
	
	/*
	 * build path for home directory
	 */

	strncpy(subdir, newuser, 3);
	subdir[3] = '\0';
	sprintf(username, "%s", newuser);
	sprintf(homedir, "%s/%s.../%s", start_home, subdir, newuser);
	strcpy(userdir, homedir);

	fd_l = open(LOCK_AXSPAWN_FILE, O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
	flock(fd_l, LOCK_EX);

retry:
	/*
	 * find first free UID
	 */

	for (uid = start_uid; uid < 65535; uid++)
	{
		pwp = getpwuid(uid);
		if (pwp == NULL)
			break;
	}

	if (uid >= 65535 || uid < start_uid)
		goto out;

	/*
	 * build directories for home
	 */
	 
	p = homedir;
		
	while (*p == '/') p++;

	chdir("/");

	while(p)
	{
		q = strchr(p, '/');
		if (q)
		{
			*q = '\0';
			q++;
			while (*q == '/') q++;
			if (*q == 0) q = NULL;
		}

		if (stat(p, &fst) < 0)
		{
			if (errno == ENOENT)
			{
				if (q == NULL && policy_add_prog_useradd) {
					/* Some useradd implementations
					 * fail if the directory for the
					 * new user already existss. That's
					 * why we stop here now, just before
					 * mkdir of ....../username/
					 * We had to use this function,
					 * because we decided to make
					 * directories in the form
					 * /home/hams/dl9.../ - and if
					 * for e.g. dl9.../ does not exist,
					 * then useradd will refuse to make
					 * the needed / missing subdirs
					 * for /home/hams/dl9.../dl9sau/
					 */
					goto end_mkdirs;
				}
				mkdir(p, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

				if (q == NULL)
				{
					chown(p, uid, user_gid);
					chmod(p, homedir_mode);
				} 
			}
			else
				goto out;
		}
		
		if (chdir(p) < 0)
			goto out;
		p = q;
	}
end_mkdirs:

	/*
	 * add the user now
	 */

	if (policy_add_prog_useradd) {

		pid_t pid = -1;
		struct stat statbuf;
		if (!user_shell_configured && stat(USERADD_CONF, &statbuf) == -1) {
			 /* some programs need a shell explicitely specified
			  * in /etc/passwd, although this field is not required
			  * (and useradd does not set a shell when not
			  * specified).
			  * useradd has a config file. On debian for e.g.,
			  * there is no /etc/default/useradd. Thus we
			  * explecitely give the shell option to useradd, when
			  * no useradd config file is present.
			  */
			  user_shell_configured = 1;
		}

		pid = fork();
		if (pid == -1)
			goto out;
		else if (pid == 0) {
			int chargc = 0;
			char *chargv[20];
			char *envp[] = { NULL };
			char s_uid[32];
			char s_gid[32];

			sprintf(s_uid, "%d", uid);
			sprintf(s_gid, "%d", user_gid);

			chargv[chargc++] = "/usr/sbin/useradd";
			chargv[chargc++] = "-p";
			chargv[chargc++] = ((policy_add_empty_password) ? "" : "+");
			chargv[chargc++] = "-c";
			chargv[chargc++] = username;
			chargv[chargc++] = "-d";
			chargv[chargc++] = userdir;
			chargv[chargc++] = "-u";
			chargv[chargc++] = s_uid;
			chargv[chargc++] = "-g";
			chargv[chargc++] = s_gid;
			chargv[chargc++] = "-m";
			chargv[chargc++] = newuser;
			if (user_shell_configured) {
		  		chargv[chargc++] = "-s";
		  		chargv[chargc++] = user_shell;
			}
			chargv[chargc]   = NULL;
			
			/* signal SIGCHLD: see description above */
			signal(SIGCHLD, signal_handler_sigchild);
                	execve(chargv[0], chargv, envp);
		} else {
			int status;
			pid_t wpid = -1;
			wpid = waitpid(-1, &status, 0);
			if (wpid == -1)
				goto out;
			if (!WIFEXITED(status))
				goto out;
		}

	} else {

		fp = fopen(PASSWDFILE, "a+");
		if (fp == NULL)
			goto out;
	
		pw.pw_name   = newuser;
		pw.pw_passwd = ((policy_add_empty_password) ? "" : "+");
		pw.pw_uid    = uid;
		pw.pw_gid    = user_gid;
		pw.pw_gecos  = username;
		pw.pw_dir    = userdir;
		pw.pw_shell  = user_shell;
	
		if (getpwuid(uid) != NULL) goto retry;	/* oops?! */

		if (putpwent(&pw, fp) < 0)
			goto out;
	
		fclose(fp);

	}

	/*
	 * copy ax25.profile
	 */

	fd_a = open(CONF_AXSPAWN_PROF_FILE, O_RDONLY);
	
	if (fd_a > 0)
	{
		int first = 1;
		fd_b = open(USERPROFILE, O_CREAT|O_WRONLY|O_APPEND, S_IRUSR|S_IWUSR);

		if (fd_b < 0)
			goto out;
		
		/* just 2b sure */
		if (lseek(fd_b, 0L, SEEK_END) > 0L)
			write(fd_b, "\n", 1);
		while ( (cnt = read(fd_a, &buf, sizeof(buf))) > 0 ) {
			if (first) {
				/* fix: profiles never start with "#!/bin/sh",
				 * but previous ax25.profile did. We fix this
				 * now. We append this profile, because .profile
				 * may came through /etc/skel. And because
				 * its there, it's intended to be used. Then
				 * if ax25.profile is present, it's regarded
				 * as an optimization.
				 * -> mark #!... as ##
				 */
                        	if (buf[0] == '#' && buf[1] == '!')
					buf[1] = '#';
				first = 0;
			}
			write(fd_b, &buf, cnt);
		}
		close(fd_b);
		close(fd_a);
		chown(USERPROFILE, uid, user_gid);
	}
out:
	flock(fd_l, LOCK_UN);
}

void read_config(void)
{
	FILE *fp = fopen(CONF_AXSPAWN_FILE, "r");
	char buf[512];
	char cmd[40], param[80];
	char *p;
	
	if (fp == NULL)
		return;
		
	while (!feof(fp))
	{
		fgets(buf, sizeof(buf), fp);
		if ((p = strchr(buf, '#')) || (p = strchr(buf, '\n')))
			*p='\0';
		
		if (buf[0] != '\0')
		{
			sscanf(buf, "%s %s", cmd, param);

			if (!strncmp(cmd, "create", 6) && !cmd[6])
			{
				policy_add_user = (param[0] == 'y');
			} else
			if (!strncmp(cmd, "create_empty_password", 21))
				policy_add_empty_password = (param[0] == 'y');
			else
			if (!strncmp(cmd, "create_with_useradd", 19))
				policy_add_prog_useradd = (param[0] == 'y');
			else
			if (!strncmp(cmd, "pwcheck", 7)) {
				if (!strncmp(param, "pass", 4))
					pwcheck = 1;
				else if (!strncmp(param, "call", 4))
					pwcheck = 2;
				else if (!strncmp(param, "group", 5))
					pwcheck = 3;
			} else
			if (!strncmp(cmd, "guest", 5))
			{
				if (!strcmp(param, "no"))
				{
					policy_guest = 0;
				} else {
					policy_guest = 1;
					guest = (char *) malloc(strlen(param)+1);
					strcpy(guest, param);
				}
			} else
			if (!strncmp(cmd, "group", 5))
			{
				user_gid = strtol(param, &p, 0);
				if (*p != '\0')
				{
					struct group * gp = getgrnam(param);
					if (gp != NULL)
						user_gid = gp->gr_gid;
					else
						user_gid = 400;
					endgrent();
				}
			} else
			if (!strncmp(cmd, "first", 5))
			{
				start_uid = strtol(param, &p, 0);
				if (*p != '\0')
					start_uid = 400;
			} else
			if (!strncmp(cmd, "max", 3))
			{
				end_uid = strtol(param, &p, 0);
				if (*p != '\0')
					end_uid = 0;
			} else
			if (!strncmp(cmd, "home", 4))
			{
				start_home = (char *) malloc(strlen(param)+1);
				strcpy(start_home, param);
			} else
			if (!strncmp(cmd, "secure_home", 11)) {
				secure_home = (param[0] == 'y');
			} else	
			if (!strncmp(cmd, "assoc", 5))
			{
				if (!strcmp(param, "yes"))
					policy_associate = 1;
				else 
					policy_associate = 0;
			} else
			if (!strncmp(cmd, "shell", 5))
			{
				user_shell = (char *) malloc(strlen(param)+1);
				strcpy(user_shell, param);
				user_shell_configured = 1;
			} else
			{
				printf("error in config: ->%s %s<-\n", cmd, param);
			}
		}
	}
	
	fclose(fp);
}

char ptyslave[20];
int child_pid;

void signal_handler(int dummy)
{
	kill(child_pid, SIGHUP);
	cleanup(ptyslave+5);
	exit(1);
}

int main(int argc, char **argv)
{
	char call[20], user[20], as_user[20];
	char buf[2048];
	int  k, cnt, digits, letters, invalid, ssid, ssidcnt;
	socklen_t addrlen;
	struct timeval tv;
	pid_t pid = -1;
	char *p;
	fd_set fds_read, fds_err;
	struct passwd *pw;
	int  chargc = 0;
	char *chargv[20];
	int envc = 0;
	char *envp[20];
	struct utmp ut_line;
	struct winsize win = { 0, 0, 0, 0};
	struct sockaddr_ax25 sax25;
	union {
		struct full_sockaddr_ax25 fsax25;
		struct sockaddr_rose      rose;
	} sockaddr;
	char *protocol = "";
	char is_guest = 0;
	char wait_for_tcp = 0;
	char changeuser = 0;
	char user_changed = 0;
	char rootlogin = 0;
	char dumb_embedded_system = 0;
	int pwtype = 0;
	int pwtype_orig = 0;
	char prompt[20];
	char *pwd = 0;

	*prompt = 0;

	digits = letters = invalid = ssid = ssidcnt = 0;

	for (k = 1; k < argc; k++){
		if (!strcmp(argv[k], "-w") || !strcmp(argv[k], "--wait"))
			wait_for_tcp = 1;
		if (!strcmp(argv[k], "-c") || !strcmp(argv[k], "--changeuser"))
			changeuser = 1;
		if (!strcmp(argv[k], "-r") || !strcmp(argv[k], "--rootlogin"))
			rootlogin = 1;
		if (!strcmp(argv[k], "-e") || !strcmp(argv[k], "--embedded"))
			dumb_embedded_system = 1;
		if ((!strcmp(argv[k], "-p") || !strcmp(argv[k], "--pwprompt")) && k < argc-1 ) {
			strncpy(prompt, argv[k+1], sizeof(prompt));
			prompt[sizeof(prompt)-1] = '\0';
			k++;
		}
		if (!strcmp(argv[k], "--only-md5"))
			pwtype = PW_MD5;
	}
	read_config();

	if (!pwtype)
		pwtype = (PW_CLEARTEXT | PW_SYS | PW_MD5 | PW_UNIX);
	pwtype_orig = pwtype;
	if (!*prompt) {
		if (gethostname(buf, sizeof(buf)) < 0) {
			strcpy(prompt, "Check");
		} else {
			if ((p = strchr(buf, '.')))
				*p = 0;
			strncpy(prompt, buf, sizeof(prompt));
			prompt[sizeof(prompt)-1] = 0;
		}
	}
	strupr(prompt);
	
	openlog("axspawn", LOG_PID, LOG_DAEMON);

	if (getuid() != 0) {
		printf("permission denied\n");
		syslog(LOG_NOTICE, "user %d tried to run axspawn\n", getuid());
		return 1;
	}

	addrlen = sizeof(struct full_sockaddr_ax25);
	k = getpeername(0, (struct sockaddr *) &sockaddr, &addrlen);
	
	if (k < 0) {
		syslog(LOG_NOTICE, "getpeername: %m\n");
		return 1;
	}

	switch (sockaddr.fsax25.fsa_ax25.sax25_family) {
		case AF_AX25:
			strcpy(call, ax25_ntoa(&sockaddr.fsax25.fsa_ax25.sax25_call));
			protocol = "AX.25";
			paclen   = AX_PACLEN;
			break;

		case AF_NETROM:
			strcpy(call, ax25_ntoa(&sockaddr.fsax25.fsa_ax25.sax25_call));
			protocol = "NET/ROM";
			paclen   = NETROM_PACLEN;
			break;

		case AF_ROSE:
			strcpy(call, ax25_ntoa(&sockaddr.rose.srose_call));
			protocol = "Rose";
			paclen   = ROSE_PACLEN;
			break;

		default:
			syslog(LOG_NOTICE, "peer is not an AX.25, NET/ROM or Rose socket\n");
			return 1;
	}

	for (k = 0; k < strlen(call); k++)
	{
		if (ssidcnt)
		{
			if (!IS_DIGIT(call[k]))
				invalid++;
			else
			{
				if (ssidcnt > 2)
					invalid++;
				else if (ssidcnt == 1)
					ssid = (int) (call[k] - '0');
				else
				{
					ssid *= 10;
					ssid += (int) (call[k] - '0');
					
					if (ssid > 15) invalid++;
				}
				ssidcnt++;
			}
		} else
		if (IS_DIGIT(call[k]))
		{
			digits++;
			if (k > 3) invalid++;
		} else
		if (IS_LETTER(call[k]))
			letters++;
		else
		if (call[k] == '-')
		{
			if (k < MINLEN)
				invalid++;
			else
				ssidcnt++;
		}
		else
			invalid++;
	}
		
	if ( invalid || (k < MINLEN) || (digits > 2) || (digits < 1) )
	{
		write_ax25_static_line(MSG_NOCALL);
		syslog(LOG_NOTICE, "%s is not an Amateur Radio callsign\n", call);
		sleep(EXITDELAY);
		return 1;
	}
	
	if (wait_for_tcp) {
		/* incoming TCP/IP connection? */
		if (read_ax25(buf, sizeof(buf)) < 0)
			exit(0);
	}

	strcpy(user, call);
	strlwr(user);
	p = strchr(user, '-');
	if (p) *p = '\0';

	*as_user = 0;
	if (changeuser) {
		char *p_buf;
		sprintf(buf, "Login (%s): ", user);
		write_ax25(buf, strlen(buf), 1);
		if ((cnt = read_ax25(buf, sizeof(buf)-1)) < 0)
				exit(1);
		buf[cnt] = 0;

		/* skip leading blanks */
		for (p_buf = buf; *p_buf && *p_buf != '\n' && !isalnum(*p_buf & 0xff); p_buf++) ;
		/* skip trailing junk, blanks, \n, .. */
		for (p = p_buf; *p && isalnum(*p & 0xff); p++) ;
		*p = 0;

		if (*p_buf) {
			strncpy(as_user, p_buf, sizeof(as_user));
			as_user[sizeof(as_user)-1] = 0;
			user_changed = 1;
		}
	}
	
	if (!*as_user)
		strcpy(as_user, user);
	pw = getpwnam(as_user);
	endpwent();

	if (pw == NULL)
	{
		if (user_changed) {
			syslog(LOG_NOTICE, "%s (callsign: %s) not found in /etc/passwd\n", as_user, call);
			sleep(EXITDELAY);
			return 1;
		}

		if (policy_add_user) 
		{
			new_user(as_user);
			pw = getpwnam(as_user);
			endpwent();
			if (pw == NULL) {
				syslog(LOG_NOTICE, "%s (callsign: %s) not found in /etc/passwd, even aver new_user()\n", as_user, call);
				sleep(EXITDELAY);
				return 1;
			}
		}
		
		if (pw == NULL && policy_guest)
		{
			strcpy(as_user,guest);
			pw = getpwnam(guest);
			endpwent();
			is_guest = 1;
		}
	}
	if (!pw) {
		write_ax25_static_line(MSG_NOTINDBF);
		syslog(LOG_NOTICE, "%s (callsign: %s) not found in /etc/passwd\n", as_user, call);
		sleep(EXITDELAY);
		return 1;
	}
	
	if (!rootlogin && (pw->pw_uid == 0 || pw->pw_gid == 0))
	{
		write_ax25_static_line(MSG_NOCALL);
		syslog(LOG_NOTICE, "root login of %s (callsign: %s) denied\n", as_user, call);
		sleep(EXITDELAY);
		return 1;
	}
	
again:
	if (!(pwd = read_pwd(pw, &pwtype))) {
		if ((!pwtype || pwtype != PW_CLEARTEXT) && (pwtype != PW_UNIX)) {
			sleep (EXITDELAY);
			return 1;
		}
	}
	if (pwtype == PW_UNIX) {
		pwtype = PW_CLEARTEXT;
		pwcheck = 1;
	}

	if (pwtype != PW_CLEARTEXT) {
		char pass_want[PASSSIZE+1];
		if (pwtype == PW_MD5)
			ask_pw_md5(prompt, pass_want, pwd);
		else
                	ask_pw_sys(prompt, pass_want, pwd);

		cnt = read_ax25(buf, sizeof(buf)-1);
		if (cnt <= 0) {
			sprintf(buf,"no response\r");
			write_ax25(buf, strlen(buf),1);
			sleep (EXITDELAY);
			return -11;
		}
		buf[cnt] = 0;
		if ((p = strchr(buf, '\n')))
			*p = 0;
		if ((pwtype & PW_MD5) && !strcmp(buf, "sys") && (pwtype_orig & PW_SYS)) {
			pwtype = (pwtype_orig & ~PW_MD5);
			if (pwd)
				free(pwd);
			pwd = 0;
			goto again;
		}
		if (!strstr(buf, pass_want)) {
			sprintf(buf,"authentication failed\r");
			write_ax25(buf, strlen(buf), 1);
			sleep (EXITDELAY);
			return -11;
		}
		if (pwd)
			free(pwd);
		pwd = 0;
	} else {
		if (pw->pw_uid == 0 || pw->pw_gid == 0) {
			sprintf(buf, "Sorry, root logins are only allowed with md5- or baycom-password!\r");
			write_ax25(buf, strlen(buf), 1);
			syslog(LOG_NOTICE, "root login of %s (callsign: %s) denied (only with md5- or baycom-Login)!\n", user, call);
			sleep(EXITDELAY);
			return 1;
		}
	}
	if (pwd)
		free(pwd);
	pwd = 0;

	/*
	 * associate UID with callsign (or vice versa?)
	 */

	if (policy_associate)
	{
		int fds = socket(AF_AX25, SOCK_SEQPACKET, 0);
		if (fds != -1)
		{
			sax25.sax25_uid = pw->pw_uid;
			if (get_assoc(&sax25) != -1)
				ioctl(fds, SIOCAX25DELUID, &sax25);
			switch (sockaddr.fsax25.fsa_ax25.sax25_family) {
				case AF_AX25:
				case AF_NETROM:
					sax25.sax25_call = sockaddr.fsax25.fsa_ax25.sax25_call;
					break;
				case AF_ROSE:
					sax25.sax25_call = sockaddr.rose.srose_call;
                                        break;
			}
			ioctl(fds, SIOCAX25ADDUID, &sax25);
			close(fds);
		}
	}
	
	fcntl(1, F_SETFL, O_NONBLOCK);
	
	pid = forkpty(&fdmaster, ptyslave, NULL, &win);
	
	if (pid == 0)
	{
		struct termios termios;
		char *shell = "/bin/sh";

        	memset((char *) &termios, 0, sizeof(termios));
        	
        	ioctl(0, TIOCSCTTY, (char *) 0);

		termios.c_iflag = ICRNL | IXOFF;
            	termios.c_oflag = OPOST | ONLCR;
                termios.c_cflag = CS8 | CREAD | CLOCAL;
                termios.c_lflag = ISIG | ICANON;
                termios.c_cc[VINTR]  = /* 127 */ 0x03;
                termios.c_cc[VQUIT]  =  28;
                termios.c_cc[VERASE] =   8;
                termios.c_cc[VKILL]  =  24;
                termios.c_cc[VEOF]   =   4;
                cfsetispeed(&termios, B19200);
                cfsetospeed(&termios, B19200);
                tcsetattr(0, TCSANOW, &termios);

		setutent();
                ut_line.ut_type = LOGIN_PROCESS;
                ut_line.ut_pid  = getpid();
                strncpy(ut_line.ut_line, ptyslave + 5, sizeof(ut_line.ut_line));
                strncpy(ut_line.ut_id,   ptyslave + 8, sizeof(ut_line.ut_id));
                strncpy(ut_line.ut_user, "LOGIN",      sizeof(ut_line.ut_user));
                strncpy(ut_line.ut_host, protocol,     sizeof(ut_line.ut_host));
		gettimeofday(&tv, NULL);
		ut_line.ut_tv.tv_sec = tv.tv_sec;
		ut_line.ut_tv.tv_usec = tv.tv_usec;
                ut_line.ut_addr = 0;                
                pututline(&ut_line);
                endutent();

		/* become process group leader, if we not already are */
		if (getpid() != getsid(0)) {
			if (setsid() == -1)
				exit(1);
		}

                chargc = 0;
                envc = 0;

		if (dumb_embedded_system) {
			int ret = -1;
			char *p = 0;

			chown(ptyslave, pw->pw_uid, 0);
			chmod(ptyslave, 0622);

			ret = -1;
			if (pw->pw_dir && *(pw->pw_dir))
				p = pw->pw_dir;
		  		ret = chdir(p);
			if (ret != 0) {
				p = "/tmp";
		  		chdir(p);
			}

                	if ((envp[envc] = (char *) malloc(strlen(p)+6)))
                		sprintf(envp[envc++], "HOME=%s", p);
                	if ((envp[envc] = (char *) malloc(strlen(pw->pw_name)+6)))
                		sprintf(envp[envc++], "USER=%s", pw->pw_name);
                	if ((envp[envc] = (char *) malloc(strlen(pw->pw_name)+9)))
                		sprintf(envp[envc++], "LOGNAME=%s", pw->pw_name);

			if (pw->pw_shell && *(pw->pw_shell)) {
				shell = pw->pw_shell;
			} else {
				shell = "/bin/sh";
			}
			if ((p = strrchr(shell, '/'))) {
				if (p[1]) {
					if ((p = strdup(p)))
						*p = '-';
					
				} else p = 0;
			}
			if (!p)
				p = shell;
                	chargv[chargc++] = p;
               		if ((envp[envc] = (char *) malloc(strlen(shell)+7)))
               			sprintf(envp[envc++], "SHELL=%s", shell);

			if (pw->pw_uid == 0)
				p = "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin";
			else
				p = "/bin:/usr/bin:/usr/local/bin";
                	if ((envp[envc] = (char *) malloc(strlen(p)+6)))
                		sprintf(envp[envc++], "PATH=%s", p);

		} else {

                	chargv[chargc++] = "/bin/login";
                	chargv[chargc++] = "-p";
			/* there exist several conectps:
		 	* Historicaly, the special character '+' in the password
		 	* field indicated that users may login via ax25, netrom, rose,
		 	* etc.. - but not via other protocols like telnet.
		 	* This secures the digipeater from abuse by inet access of
		 	* non-hams.
		 	* On the other hand, this leads to the problem that telent
		 	* via HF, pop3, etc.. do not work.
		 	* The "pwcheck == 2 method means, that the password is used on
		 	* every login mechanism other than this axspawn program;
		 	* here we do not rely on the password - the ax25 call of
		 	* the ham is enough. We have already checked above, that
		 	* the call of the user is valid (and not root, httpd, etc..);
		 	* thus this method is safe here.
		 	* Another mechanism (pwcheck == 3) is to check if the gid
		 	* equals to user_gid preference. I prefer this way, because
		 	* this approach gives the chance to temporary lock a user
		 	* out (abuse, ..) by changing his gid in passwd to for e.g.
		 	* 65534 (nogroup).
		 	*/
			chargv[chargc++] = "-h";
			chargv[chargc++] = protocol;
                	if (pwtype != PW_CLEARTEXT /* PW_SYS or PW_MD5 are already authenticated */ 
				|| pwcheck == 2 || (pwcheck == 3 && (pw->pw_gid == user_gid || is_guest)) || !strcmp(pw->pw_passwd, "+"))
	        		chargv[chargc++] = "-f";
                	chargv[chargc++] = as_user;
		}
               	chargv[chargc]   = NULL;

                if ((envp[envc] = (char *) malloc(30)))
                	sprintf(envp[envc++], "AXCALL=%s", call);
                if ((envp[envc] = (char *) malloc(30)))
                	sprintf(envp[envc++], "CALL=%s", user);
                if ((envp[envc] = (char *) malloc(30)))
                	sprintf(envp[envc++], "PROTOCOL=%s", protocol);
		if ((envp[envc] = (char *) malloc(30)))
			sprintf(envp[envc++], "TERM=dumb"); /* SuSE bug (dump - tsts) */
		/* other useful defaults */
		if ((envp[envc] = (char *) malloc(30)))
			sprintf(envp[envc++], "EDITOR=/usr/bin/ex");
		if ((envp[envc] = (char *) malloc(30)))
			sprintf(envp[envc++], "LESS=-d -E -F");
		envp[envc] = NULL;

		if (dumb_embedded_system) {
			if (setgid(pw->pw_gid) == -1)
				exit(1);
			if (setuid(pw->pw_uid) == -1)
				exit(1);
                	execve(shell, chargv, envp);
			/* point of no return */
			exit(1);
		}

                execve(chargv[0], chargv, envp);
		/* point of no return */
		exit(1);
        }
        else if (pid > 0)
        {
        	child_pid = 0;
        	signal(SIGHUP, signal_handler);
        	signal(SIGTERM, signal_handler);
        	signal(SIGINT, signal_handler);
        	signal(SIGQUIT, signal_handler);
 
        	while(1)
        	{
       			FD_ZERO(&fds_read);
       			FD_ZERO(&fds_err);
        		FD_SET(0, &fds_read);
        		FD_SET(0, &fds_err);
        		if (wqueue_length <= paclen*7)
				FD_SET(fdmaster, &fds_read);
        		FD_SET(fdmaster, &fds_err);
        		
        		k = select(fdmaster+1, &fds_read, NULL, &fds_err, NULL);
  
        		if (k > 0)
        		{
        			if (FD_ISSET(0, &fds_err))
        			{
					if (huffman) {
							sprintf(buf,"\r//COMP 0\r");
							write_ax25(buf, strlen(buf), 1);
							sleep(EXITDELAY);
        				}
        				kill(pid, SIGHUP);
        				cleanup(ptyslave+5);
        				return 1;
        			}
        			
        			if (FD_ISSET(fdmaster, &fds_err))
        			{	
					/* give the last packet in the timer controlled sendqueue a chance.. */
					if (wqueue_length) {
						continue;
					}
					if (huffman) {
							sprintf(buf,"\r//COMP 0\r");
							write_ax25(buf, strlen(buf), 1);
							sleep(EXITDELAY);
        				}
        				cleanup(ptyslave+5);
        				return 1;
        			}

        			if (FD_ISSET(0, &fds_read))
        			{
        				cnt = read_ax25(buf, sizeof(buf));
        				if (cnt < 0)	/* Connection died */
        				{
        					kill(pid, SIGHUP);
        					cleanup(ptyslave+5);
        					return 1;
        				} else
        					write(fdmaster, buf, cnt);
        			}
        			
        			if (FD_ISSET(fdmaster, &fds_read))
        			{
        				cnt = read(fdmaster, buf, (huffman ? 254 : sizeof(buf)));
        				if (cnt < 0)
        				{
						/* give the last packet in the timer controlled sendqueue a chance.. */
						if (wqueue_length) {
							continue;
						}
						if (huffman) {
								sprintf(buf,"\r//COMP 0\r");
								write_ax25(buf, strlen(buf), 1);
								sleep(EXITDELAY);
        					}
	        				cleanup(ptyslave+5);
        					return 1;	/* Child died */
        				}
        				write_ax25(buf, cnt, 0);
        			}
        		} else 
        		if (k < 0 && errno != EINTR)
        		{
				if (huffman) {
						sprintf(buf,"\r//COMP 0\r");
						write_ax25(buf, strlen(buf), 1);
						sleep(EXITDELAY);
        			}

        			kill(pid, SIGHUP);	/* just in case... */
        			cleanup(ptyslave+5);
        			return 0;
        		}
        	}
        } 
        else
        {
		write_ax25_static_line(MSG_CANNOTFORK);
        	syslog(LOG_ERR, "cannot fork %m, closing connection to %s\n", call);
        	sleep(EXITDELAY);
        	return 1;
        }
        
        sleep(EXITDELAY);

	return 0;
}
