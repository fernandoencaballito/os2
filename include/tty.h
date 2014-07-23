#ifndef TTY_H
#define TTY_H

#define NUMROWS 24 // Sin incluir barra superior
#define NUMCOLS 80

typedef struct{
    MsgQueue_t * key_mq;
    unsigned short buf[NUMROWS][NUMCOLS];
    unsigned cur_x, cur_y, cur_attr;
    unsigned scrolls;
    bool cursor_on;
} Tty;

#endif
