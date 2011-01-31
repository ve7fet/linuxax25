#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <pwd.h>
#include <fcntl.h>

#include "access.h"
#include "md5.h"
#include "axspawn.h"

#define CONV_RAND_MAX 0x7fffffff

#define	SYSTEMPW 0
#define	USERPW	 1

long seed = 1L;

int conv_rand(void);
void conv_randomize(void);
int conv_random(int num, int base);
char *generate_rand_pw(int len);
void calc_md5_pw (const char *MD5prompt, const char *MD5pw, char *MD5result);
static void char_to_hex(char *c, char *h, int n);

/*--------------------------------------------------------------------------*/

int conv_rand(void)
{
	seed = (1103515245L * seed + 12345) & CONV_RAND_MAX;
	return ((int) (seed & 077777));
}

/*--------------------------------------------------------------------------*/

void conv_randomize(void)
{
	seed = (time(0) & CONV_RAND_MAX);
}

/*--------------------------------------------------------------------------*/

int conv_random(int num, int base)
{
	return (((long) (conv_rand() * time(0)) & CONV_RAND_MAX) % num + base);
}

/*--------------------------------------------------------------------------*/

static void char_to_hex(char *c, char *h, int n)
{

	int  i;
	static char  *hextable = "0123456789abcdef";

	for (i = 0; i < n; i++) {
		*h++ = hextable[(*c>>4)&0xf];
		*h++ = hextable[*c++ &0xf];
	}
	*h = '\0';
}

/*--------------------------------------------------------------------------*/

char *generate_rand_pw(int len)
{
	static char pass[PASSSIZE+1];
	int i, j;

	pass[0] = 0;

	if (seed == 1L)
		conv_randomize();

	if (len < 1)
		return pass;

	if (len > PASSSIZE)
		len = PASSSIZE;

        for (i = 0; i < len; i++) {
                j = conv_random(10+26*2, 0);
                if (j < 10) {
                        pass[i] = j + '0';
                        continue;
                }
                j -= 10;
                if (j < 26) {
                        pass[i] = j + 'A';
			continue;
                }
                j -= 26;
                pass[i] = j + 'a';
        }
        pass[len] = 0;

        return pass;
}

/*--------------------------------------------------------------------------*/

void ask_pw_sys(char *prompt, char *pass_want, char *pw)
{
	char buffer[2048];
	int five_digits[5];
	int pwlen;
	int i, j;

	pass_want[0]= 0;

	if (!pw || !*pw)
		return;

	pwlen = strlen(pw);

	if (seed == 1L)
		conv_randomize();

	for (i = 0; i < 5; i++) {
		int k;
again:
		j = conv_random(pwlen, 0);
		/* store generated request-numbers  */
		five_digits[i] = j+1; /* pos0 refers as 1  */
		/* same number again? */
		for (k = 0; k < i; k++) {
			if (five_digits[k] == five_digits[i])
				goto again;
		}
		/* store expected string in cp->passwd  */
		pass_want[i] = pw[j];
	}
	/* and terminate the string  */
	pass_want[i] = 0;

	sprintf(buffer, "\r%s>  %d %d %d %d %d\r", prompt, five_digits[0], five_digits[1], five_digits[2], five_digits[3], five_digits[4]);
	write_ax25(buffer, strlen(buffer), 1);
}

/*--------------------------------------------------------------------------*/

void ask_pw_md5(char *prompt, char *pass_want, char *pw)
{
#define	SALT_LEN	10
	char buffer[2048];
	char cipher[16];
	char key[256];
	char *challenge;

	pass_want[0]= 0;

	if (!pw || !*pw)
		return;

	if (seed == 1L)
		conv_randomize();

	strncpy(key, pw, sizeof(key));
	key[sizeof(key)-1] = 0;

	/* compute random salt  */
	challenge = generate_rand_pw(SALT_LEN);

	/* ask for proper response to this challenge:  */
	sprintf(buffer, "\r%s> [%s]\r", prompt, challenge);
	write_ax25(buffer, strlen(buffer), 1);
	/* compute md5 challenge  */
	calc_md5_pw(challenge, key, cipher);
	/* store expected answer  */
	char_to_hex(cipher, pass_want, 16);
}

/*--------------------------------------------------------------------------*/

void calc_md5_pw (const char *MD5prompt, const char *MD5pw, char *MD5result)
{
  MD5_CTX context;
  short i, n, len;
  char buffer[1024];

  strncpy(buffer, MD5prompt, 10);
  buffer[10] = 0;
  strcat(buffer, MD5pw);

  MD5Init(&context);

  len = strlen(buffer);
  for (i= 0; i < len; i += 16) {
    n = (len - i) > 16 ? 16 : (len - i);
    MD5Update(&context, buffer+i, n);
  }

  MD5Final(&context);

  MD5result[0] = '\0';
  for (i = 0; i < 16; i++) {
    MD5result[i] = context.digest[i];
  }
}

/*--------------------------------------------------------------------------*/

void write_example_passwd(char *pwfile, char pwlocation, struct passwd *pw) {
	FILE * f;
	int i;

	if ((i = open(pwfile, O_CREAT|O_WRONLY|O_TRUNC, (S_IRUSR | S_IWUSR | (pwlocation == SYSTEMPW ? (S_IRGRP /* | S_IWGRP */ ) : 0)))) == -1)
		return;
	fchown(i, (pwlocation == SYSTEMPW ? 0 : pw->pw_uid), (pwlocation == SYSTEMPW ? 0 : pw->pw_gid));
	close(i);
	if ( ! (f = fopen(pwfile, "w")) )
		return;
	fprintf(f, "# %s Password file for axspawn\n", (pwlocation == SYSTEMPW ? "System" : "User")); 
	if (pwlocation == SYSTEMPW) {
		fprintf(f, "# disable user self-administered passwords in $HOME/.%s\n", PWFILE);
		fprintf(f, "# with the line \"systempasswordonly\"\n");
		fprintf(f, "# systempasswordonly\n");
	}
	fprintf(f, "# Examples (sys and md5 passwords may differ):\n");
	fprintf(f, "# md5 standard (secure) - length: >= %d and <= %d characters\n", MINPWLEN_MD5, PASSSIZE); 
	fprintf(f, "# %smd5:%s\n", (pwlocation == SYSTEMPW ? "username:" : ""), generate_rand_pw(MINPWLEN_MD5));
	fprintf(f, "# sys/baycom standard (not very secure) - length: >= %d and <= %d characters\n", MINPWLEN_SYS, PASSSIZE);
	fprintf(f, "# %ssys:%s\n", (pwlocation == SYSTEMPW ? "username:" : ""), generate_rand_pw(MINPWLEN_SYS));
	fprintf(f, "# unix standard (plaintext): no password is read here. Your password is looked\n");
	fprintf(f, "#   up during login in the system password table /etc/passwd or /etc/shadow\n");
	fprintf(f, "# unix\n");
	fclose(f);
}

/*--------------------------------------------------------------------------*/

char *read_pwd (struct passwd *pw, int *pwtype)
{
	FILE * f = 0;
	struct stat statbuf;
	char pwfile[PATH_MAX + 1];
	int len;
	char pwlocation;
	char buf[2048];
	int only_systempw = 0;
	char *pass = 0;
	char *p_buf;
	char *p;


	for (pwlocation = 0; pwlocation < 2; pwlocation++) {

		if (pwlocation == SYSTEMPW) {
			sprintf(pwfile, "/%s/%s", AX25_SYSCONFDIR, PWFILE);
			if (stat(pwfile, &statbuf)) {
				write_example_passwd(pwfile, pwlocation, pw);
				continue;
			}
			if (!S_ISREG(statbuf.st_mode) || (statbuf.st_mode & (S_IROTH | S_IWOTH)))
				continue;
			if ( !(f = fopen(pwfile, "r")) )
				continue;
		} else {
			if (only_systempw)
				goto end;
			snprintf(pwfile, sizeof(pwfile), "%s/.%s", pw->pw_dir, PWFILE);
			pwfile[sizeof(pwfile)-1] = 0;

			if (stat(pwfile, &statbuf)) {
				sprintf(buf, "Notice: No .%s file found in your homedirectory (for more secure\r", PWFILE);
				write_ax25(buf, strlen(buf), 1);
				sprintf(buf, "        password authentication than plaintext). Generating example file,\r");
				write_ax25(buf, strlen(buf), 1);
				sprintf(buf, "        with unique passwords (which may be changed).\r");
				write_ax25(buf, strlen(buf), 1);
				sprintf(buf, "        Please edit ~/.%s, and enable your prefered authentication type;\r", PWFILE);
				write_ax25(buf, strlen(buf), 1);
				sprintf(buf, "        MD5 is recommended.\r");
				write_ax25(buf, strlen(buf), 1);
				write_example_passwd(pwfile, pwlocation, pw);
				goto end;
			}
			if (!S_ISREG(statbuf.st_mode)) {
				sprintf(buf, "Error: password file .%s should be a regular file. Skiping..\r", PWFILE);
				write_ax25(buf, strlen(buf), 1);
				goto end;
			}
			if (statbuf.st_uid != 0 && statbuf.st_uid != pw->pw_uid) {
				sprintf(buf, "Error: your password file .%s is not owned by you. Skiping..\r", PWFILE);
				write_ax25(buf, strlen(buf), 1);
				goto end;
			}
			if  ((statbuf.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))) {
				sprintf(buf, "WARNING: your password file .%s has wrong permissions.\r", PWFILE);
				write_ax25(buf, strlen(buf), 1);
				sprintf(buf, "         Please change it with\r");
				write_ax25(buf, strlen(buf), 1);
				sprintf(buf, "           chmod 600 .%s\r", PWFILE);
				write_ax25(buf, strlen(buf), 1);
				sprintf(buf, "         and don't forget to change your password stored in .%s\r", PWFILE);
				write_ax25(buf, strlen(buf), 1);
				sprintf(buf, "         because it may be compromised.\r");
				write_ax25(buf, strlen(buf), 1);
				/* go on.. if user takes no action, he always gets this message */
			}
			if ( !(f = fopen(pwfile, "r")) )
				goto end;
		}


		for (;;) {
			if (!fgets(buf, sizeof(buf), f)) {
				fclose(f);
				if (pwlocation == SYSTEMPW)
					break;
				/* perhaps this is too irritating for the user
				   when the message occurs always. Thus only
				   write the notice, if cleartext fallback
				   is disabled by administrative configuration.
				 */
				if  (!((*pwtype) & PW_CLEARTEXT)) {
					sprintf(buf, "Failed to find a suitable password in %s\r", pwfile);
					write_ax25(buf, strlen(buf), 1);
				}
				goto end;
			}
			if ((p = strchr(buf, '\n')))
				*p = 0;
			if (!*buf || isspace(*buf & 0xff))
				continue;
			if (*buf == '#')
				continue;
			if (pwlocation == SYSTEMPW) {
				if (!Strcasecmp(buf, "systempasswordonly")) {
					only_systempw = 1;
					continue;
				}
				if (!(p = strchr(buf, ':')))
					continue;
				*p++ = 0;
				if (strcmp(pw->pw_name, buf))
					continue;
				p_buf = p;
			} else {
				p_buf = buf;
			}

			if (!Strcasecmp(p_buf, "unix")) {
				pass = p_buf;
			} else {
				if (!(pass = strchr(p_buf, ':')))
					continue;
				*pass++ = 0;
			}

			while (*pass && isspace(*pass & 0xff))
				pass++;
			for (p = pass; *p && !isspace(*p & 0xff); p++) ;
			*p = 0;

        		if ( (*pwtype & PW_MD5) && !Strcasecmp(p_buf, "md5") ) {
				fclose(f);
				*pwtype = PW_MD5;
				goto found;
			} else if ( (*pwtype & PW_SYS) && (!Strcasecmp(p_buf, "sys") || !strcmp(p_buf, "baycom")) ) {
				fclose(f);
				*pwtype = PW_SYS;
				goto found;
			} else if ( (*pwtype & PW_UNIX) &&  (!Strcasecmp(p_buf, "unix") ) ) {
				fclose(f);
				*pwtype = PW_UNIX;
				return 0;
			}
		}
	}
found:
	
	if (!pass || !*pwtype)
		goto end;

	len = strlen(pass);

	if ((*pwtype == PW_SYS && len < MINPWLEN_SYS) || (*pwtype == PW_MD5 && len < MINPWLEN_MD5)) {
		sprintf(buf, "Password in password file too short\r");
		write_ax25(buf, strlen(buf), 1);
		goto end;
	}
	if (strlen(pass) > PASSSIZE)
		pass[PASSSIZE] = 0;

	return strdup(pass);

end:
	*pwtype = (((*pwtype) & PW_CLEARTEXT) ? PW_CLEARTEXT : 0);
	           /*         ^ may allow cleartext? - then cleartext, else deny */
	return 0;
}
