#define M_ITEM 0x01
#define M_P_DWN 0x02
#define M_END 0x03

typedef struct
{
	char* st_ptr;
	char key;
	int entr_type;
	void* arg;
} menuitem;

struct wint_s
{
	WINDOW* ptr;
	int fline;
	int lline;
	struct wint_s* next;
};
typedef struct wint_s wint;

WINDOW* winopen(wint*, int, int, int, int, int);
void winclose(wint*);
void menu_write_line(WINDOW*, int, int,int, char*);
int p_dwn_menu(wint*, menuitem*, int, int);
void menu_write_item(WINDOW*, int,int, const char*);
int top_menu(wint*, menuitem*, int);
