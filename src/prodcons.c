#include "kernel.h"

#define FULL			0xDB
#define EMPTY			0xB0
#define BUF_SIZE		40

#define TPROD			200
#define TMON			40

#define MSG_COL			21
#define MSG_LIN			17
#define MSG_FMT			"%s"

#define BUF_COL			21
#define BUF_LIN			10
#define BUF_FMT			"%s"

#define TIME_COL		21
#define TIME_LIN		8
#define TIME_FMT		"Segundos:   %u"

#define PRODSTAT_COL	21
#define PRODSTAT_LIN	12
#define PRODSTAT_FMT	"Productor:  %s"

#define CONSSTAT_COL	21
#define CONSSTAT_LIN	13
#define CONSSTAT_FMT	"Consumidor: %s"

#define MAIN_FG			LIGHTCYAN
#define BUF_FG			YELLOW
#define CLK_FG			LIGHTGREEN
#define MON_FG			LIGHTRED

#define forever while(true)

typedef struct{
    unsigned seconds;
    bool end_consumer;
    char buffer[BUF_SIZE+1];
    char *end;
    char *head;
    char *tail;
    Semaphore_t *buf_used, *buf_free;
    Task_t *prod, *cons, *clk, *mon;
} GlobalVars;

/* funciones de entrada-salida */

static int 
mprint(int fg, int x, int y, char *format, ...){
    int n;
    va_list args;

    Atomic();
    mt_cons_gotoxy(x, y);
    mt_cons_setattr(fg, BLACK);
    va_start(args, format);
    n = vprintk(format, args);
    va_end(args);
    mt_cons_clreol();
    Unatomic();
    return n;
}

static void
put_buffer(void){
    GlobalVars *gv = (GlobalVars *)mt_curr_task->data;
    *(gv->tail)++ = FULL;
    if ( gv->tail == gv->end )
        gv->tail = gv->buffer;
    mprint(BUF_FG, BUF_COL, BUF_LIN, BUF_FMT, gv->buffer);
}

static void
get_buffer(void){
    GlobalVars *gv = (GlobalVars *)mt_curr_task->data;
    *(gv->head)++ = EMPTY;
    if ( gv->head == gv->end )
        gv->head = gv->buffer;
    mprint(BUF_FG, BUF_COL, BUF_LIN, BUF_FMT, gv->buffer);
}

/* funciones auxiliares */

static const char *
task_status(unsigned status){
    static const char *states[] =
    {
        "TaskSuspended",
        "TaskReady", 
        "TaskCurrent", 
        "TaskDelaying", 
        "TaskWaiting", 
        "TaskSending", 
        "TaskReceiving", 
        "TaskTerminated" 
    };
    static unsigned nstates = sizeof states / sizeof(char *);

    return status >= nstates ? "???" : states[status];
}

/* procesos */

static void
clock(void *arg){
    forever{
        mprint(CLK_FG, TIME_COL, TIME_LIN, TIME_FMT, ((GlobalVars *)mt_curr_task->data)->seconds);
        Delay(1000);
        ((GlobalVars *)mt_curr_task->data)->seconds++;
    }
}

static void
producer(void *arg){
    forever{
        WaitSem(((GlobalVars *)mt_curr_task->data)->buf_free);
        put_buffer();
        SignalSem(((GlobalVars *)mt_curr_task->data)->buf_used);
        Delay(TPROD);
    }
}

static void
consumer(void *arg){
    unsigned c;

    forever{
        while ( !mt_kbd_getch(&c) )
            ;
        if ( c == 'S' || c == 's' )
            break;
        WaitSem(((GlobalVars *)mt_curr_task->data)->buf_used);
        get_buffer();
        SignalSem(((GlobalVars *)mt_curr_task->data)->buf_free);
    }

    ((GlobalVars *)mt_curr_task->data)->end_consumer = true;
}

static void
monitor(void *args){
    forever{
        mprint(MON_FG, PRODSTAT_COL, PRODSTAT_LIN, PRODSTAT_FMT,
                task_status(((GlobalVars *)mt_curr_task->data)->prod->state));
        mprint(MON_FG, CONSSTAT_COL, CONSSTAT_LIN, CONSSTAT_FMT,
                task_status(((GlobalVars *)mt_curr_task->data)->cons->state));
        Delay(TMON);
    }
}

int
prodcons_main(int argc, char **argv){
    GlobalVars *gv = Malloc(sizeof(GlobalVars));
    gv->end_consumer = false;
    gv->end = gv->buffer + BUF_SIZE;
    gv->head = gv->buffer;
    gv->tail = gv->buffer;
    memset(gv->buffer, EMPTY, BUF_SIZE);

    bool cursor = mt_cons_cursor(false);
    mt_cons_clear();

    gv->buf_free = CreateSem("Fee space", BUF_SIZE);
    gv->buf_used = CreateSem("Used space", 0);
    gv->prod = CreateTask(producer, 0, NULL, "Producer", DEFAULT_PRIO);
    gv->cons = CreateTask(consumer, 0, NULL, "Consumer", DEFAULT_PRIO);
    gv->clk = CreateTask(clock, 0, NULL, "Clock", DEFAULT_PRIO);
    gv->mon = CreateTask(monitor, 0, NULL, "Monitor", DEFAULT_PRIO + 1);

    SetData(gv->prod, gv);
    SetData(gv->cons, gv);
    SetData(gv->clk, gv);
    SetData(gv->mon, gv);
    SetData(mt_curr_task, gv);

    Ready(gv->prod);
    Ready(gv->cons);
    Ready(gv->clk);
    Ready(gv->mon);

    mprint(MAIN_FG, MSG_COL, MSG_LIN, MSG_FMT, "Oprima S para salir\n");
    mprint(MAIN_FG, MSG_COL, MSG_LIN+1, MSG_FMT, "Cualquier otra tecla para activar el consumidor");

    while ( !gv->end_consumer )
        Yield();

    DeleteTask(gv->prod);
    DeleteTask(gv->clk);
    DeleteTask(gv->mon);

    DeleteSem(gv->buf_free);
    DeleteSem(gv->buf_used);

    mt_cons_cursor(cursor);
    mt_cons_clear();

    return 0;
}
