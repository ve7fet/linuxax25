#ifndef FALSE
#define	FALSE	0
#endif
#ifndef TRUE
#define	TRUE	1 
#endif
extern int fd;
extern int interrupted;
extern int paclen;

/* In call.c */
extern void convert_crlf(char *, int);
extern void convert_lfcr(char *, int);

/* In yapp.c */
extern void cmd_yapp(char *, int);

/* In dostime.c */
extern void date_unix2dos(time_t, unsigned short*, unsigned short*);
extern int yapp2unix(char *);
extern void unix2yapp( int, char *);
