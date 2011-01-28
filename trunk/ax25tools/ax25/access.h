#ifndef ACCESS_H
#define ACCESS_H

#define PASSSIZE        80      /* for md5 passwords, at least 32 characters */
#define MINPWLEN_SYS	20
#define MINPWLEN_MD5	8

#define	PWFILE		"bcpasswd"

/* PWxxx: set bits */
#define PW_CLEARTEXT     1
#define PW_SYS           2
#define PW_MD5           4
#define PW_UNIX		 8

void ask_pw_sys(char *prompt, char *pass_want, char *pw);
void ask_pw_md5(char *prompt, char *pass_want, char *pw);
char *read_pwd (struct passwd *pw, int *pwtype);
#endif

