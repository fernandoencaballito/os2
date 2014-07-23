#include "mtask.h"
#include "kernel.h"
#include "apps.h"

#define TABSIZE 8

#define CRT_ADDR 0x3D4
#define CRT_DATA 0x3D5
#define CRT_CURSOR_START 0x0A
#define CRT_CURSOR_END 0x0B
#define CRT_CURSOR_HIGH 0x0E
#define CRT_CURSOR_LOW 0x0F

#define DEFATTR ((BLACK << 12) | (LIGHTGRAY << 8))
#define BS 0x08
#define TTYS_NUM 4

typedef unsigned short row[NUMCOLS];
row *vidmem = (row *) VIDMEM;
static bool raw;
static Tty *tty[TTYS_NUM];
static Tty *focus;

static void 
print_tabs(void){
    char *s = "CONSOLA1 | CONSOLA2 | CONSOLA3 | CONSOLA4 ";
    unsigned short *p1 = VIDMEM;

    while ( *s )
        *p1++ = (*s++ & 0xFF) | DEFATTR;

    vidmem = &(vidmem[1]); //se cambia el puntero a memoria 
}

static void
setcursor(Tty * ttyp){
    if (ttyp->cursor_on){
        unsigned off = (ttyp->cur_y+1) * NUMCOLS + ttyp->cur_x;
        if(focus == ttyp){
            outb(CRT_ADDR, CRT_CURSOR_HIGH);
            outb(CRT_DATA, off >> 8);
            outb(CRT_ADDR, CRT_CURSOR_LOW);
            outb(CRT_DATA, off);
        }
    }
}

static void
scroll(void){
    Tty *ttyp = mt_curr_task->ttyp;
    int j;
	turnOffMouse(); // se apaga el mouse para no alterar las lineas que se desplazan

    for (j = 1; j < NUMROWS; j++){
        memcpy(&(ttyp->buf)[j - 1], &(ttyp->buf)[j], sizeof(row));
    }
    for (j = 0; j < NUMCOLS; j++){
        ttyp->buf[NUMROWS - 1][j] = DEFATTR;
    }
    ttyp->scrolls++;
    if(focus == ttyp){
        for (j = 1; j < NUMROWS; j++)
            memcpy(&vidmem[j - 1], &vidmem[j], sizeof(row));
        for (j = 0; j < NUMROWS; j++)
            vidmem[NUMROWS - 1][j] = DEFATTR;
    }
	turnOnMouse();
}

static void
put(unsigned char ch){
    Tty *ttyp = mt_curr_task->ttyp;
    unsigned short c = (ch & 0xFF) | (ttyp->cur_attr);

    (ttyp->buf)[ttyp->cur_y][ttyp->cur_x] = c;
    if(focus == ttyp){
        vidmem[ttyp->cur_y][ttyp->cur_x] = c;
    }
    ttyp->cur_x++;
    if (ttyp->cur_x >= NUMCOLS){
        ttyp->cur_x = 0;
        if (ttyp->cur_y == NUMROWS - 1)
            scroll();
        else
            ttyp->cur_y++;
    }
    setcursor(ttyp);
}

/* Interfaz pública */

void
mt_reload_cons(){
    unsigned short *p1 = &vidmem[0][0];
    unsigned short *p2 = &vidmem[NUMROWS][0];
    unsigned short *p3 = &(focus->buf)[0][0];

    while (p1 < p2)
        *p1++ = *p3++;
    //mt_cons_gotoxy(0, 0);
    setcursor(focus);
}

void
mt_cons_clear(void){
    Tty *ttyp = mt_curr_task->ttyp;
    unsigned short *p1 = &vidmem[0][0];
    unsigned short *p2 = &vidmem[NUMROWS][0];
    unsigned short *p3 = &(ttyp->buf)[0][0];

    while (p1 < p2){
        *p1++ = DEFATTR;
        *p3++ = DEFATTR;
    }
    mt_cons_gotoxy(0, 0);
}

void
mt_cons_clreol(void){
    Tty *ttyp = mt_curr_task->ttyp;
    unsigned short *p1 = &vidmem[ttyp->cur_y][ttyp->cur_x];
    unsigned short *p2 = &vidmem[ttyp->cur_y + 1][0];
    unsigned short *p3 = &(ttyp->buf)[ttyp->cur_y][ttyp->cur_x];

    while (p1 < p2){
        *p1++ = DEFATTR;
        *p3++ = DEFATTR;
    }
}

void
mt_cons_clreom(void){
    Tty *ttyp = mt_curr_task->ttyp; 
    unsigned short *p1 = &vidmem[ttyp->cur_y][ttyp->cur_x];
    unsigned short *p2 = &vidmem[NUMROWS][0];
    unsigned short *p3 = &(ttyp->buf)[ttyp->cur_y][ttyp->cur_x];

    while (p1 < p2){
        *p1++ = DEFATTR;
        *p3++ = DEFATTR;
    }
}

unsigned
mt_cons_nrows(void){
    return NUMROWS;
}

unsigned
mt_cons_ncols(void){
    return NUMCOLS;
}

unsigned
mt_cons_nscrolls(void){
    return mt_curr_task->ttyp->scrolls;
}

void
mt_cons_getxy(unsigned *x, unsigned *y){
    Tty *ttyp = mt_curr_task->ttyp;
    *x = ttyp->cur_x;
    *y = ttyp->cur_y;
}

void
mt_cons_gotoxy(unsigned x, unsigned y){
    Tty *ttyp = mt_curr_task->ttyp;
    if (y < NUMROWS && x < NUMCOLS){
        ttyp->cur_x = x;
        ttyp->cur_y = y;
        setcursor(ttyp);
    }
}

void
mt_cons_setattr(unsigned fg, unsigned bg){
    Tty *ttyp = mt_curr_task->ttyp;
    ttyp->cur_attr = ((fg & 0xF) << 8) | ((bg & 0xF) << 12);
}

void
mt_cons_getattr(unsigned *fg, unsigned *bg){
    Tty *ttyp = mt_curr_task->ttyp;
    *fg = (ttyp->cur_attr >> 8) & 0xF;
    *bg = (ttyp->cur_attr >> 12) & 0xF;
}

bool
mt_cons_cursor(bool on){
    Tty *ttyp = mt_curr_task->ttyp;
    bool prev = ttyp->cursor_on;
    unsigned start = on ? 14 : 1, end = on ? 15 : 0;

    outb(CRT_ADDR, CRT_CURSOR_START);
    outb(CRT_DATA, start);
    outb(CRT_ADDR, CRT_CURSOR_END);
    outb(CRT_DATA, end);
    ttyp->cursor_on = on;
    setcursor(ttyp);
    return prev;
}

void
mt_cons_putc(char ch){
    if (raw){
        put(ch);
        return;
    }

    switch (ch){
        case '\t':
            mt_cons_tab();
            break;
        case '\r':
            mt_cons_cr();
            break;
        case '\n':
            mt_cons_nl();
            break;
        case BS:
            mt_cons_bs();
            break;
        default:
            put(ch);
            break;
    }
}

void
mt_cons_puts(const char *str){
    while (*str)
        mt_cons_putc(*str++);
}

void
mt_cons_cr(void){
    Tty *ttyp = mt_curr_task->ttyp;
    ttyp->cur_x = 0;
    setcursor(ttyp);
}

void
mt_cons_nl(void){
    Tty *ttyp = mt_curr_task->ttyp;
    if (ttyp->cur_y == NUMROWS - 1)
        scroll();
    else
        ttyp->cur_y++;
    setcursor(ttyp);
}

void
mt_cons_tab(void){
    unsigned nspace = TABSIZE - (mt_curr_task->ttyp->cur_x % TABSIZE);
    while (nspace--)
        put(' ');
}

void
mt_cons_bs(void){
    Tty *ttyp = mt_curr_task->ttyp;
    if (ttyp->cur_x) ttyp->cur_x--;
    else if (ttyp->cur_y){
        ttyp->cur_y--;
        ttyp->cur_x = NUMCOLS - 1;
    }
    setcursor(ttyp);
}

bool
mt_cons_raw(bool on){
    bool prev = raw;
    raw = on;
    return prev;
}

void
tty_run(void *arg){
    char *s[] = { "shell", NULL };
    shell_main(1, s);
}

void 
initialize_tty(Tty *ttyp){
    int row, col;
    for(row = 0; row < NUMROWS; row++){
        for(col = 0; col < NUMCOLS; col++)
            (ttyp->buf)[row][col] = DEFATTR;
    }
    ttyp->key_mq = mt_new_kbd_queue();
    ttyp->cur_attr = DEFATTR;
    ttyp->cur_x = 0;
    ttyp->cur_y = 0;
    ttyp->scrolls = 0;
    ttyp->cursor_on = true;
}

void
switch_focus(int tty_num){
    Atomic();
    focus = tty[tty_num];
    set_key_mq(focus->key_mq);
    mt_reload_cons();
    Unatomic();
}

void 
mt_setup_ttys(void){
    Atomic();
    int i;
    for(i = 0; i < TTYS_NUM; i++){
        tty[i] = Malloc(sizeof(Tty));
        initialize_tty(tty[i]);
        Task_t *t = CreateTask(tty_run, 0, NULL, "", DEFAULT_PRIO);
        t->ttyp = tty[i];
        Ready(t);
    }
    print_tabs();
    switch_focus(0);
    Unatomic();
}
