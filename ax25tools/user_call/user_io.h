extern void err(char *message);

extern int user_write(int fd, const void *buf, size_t count);
extern int user_read(int fd, void *buf, size_t count);
extern int select_loop(int s);

extern void init_compress(void);
extern void end_compress(void);

extern int compression;
extern int paclen_in;
extern int paclen_out;
