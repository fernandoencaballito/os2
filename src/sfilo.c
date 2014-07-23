#include "kernel.h"

#define NF						5
#define MIN_THINK				2000
#define MAX_THINK				6000
#define MIN_EAT					2000
#define MAX_EAT					6000

#define COLOR_NORMAL			LIGHTGRAY
#define COLOR_THINKING			WHITE
#define COLOR_HUNGRY			YELLOW
#define COLOR_EATING			LIGHTRED
#define COLOR_FREE				WHITE
#define COLOR_USED				LIGHTRED

typedef struct{
	int x;
	int y;
}
point;

typedef struct{
    Monitor_t *mon;     // monitor de acceso a los tenedores
    bool in_use[NF];    // tenedores usados
    Condition_t *cond;  // una variable de condicion para todos
} GlobalVars;

static int
next(int n){
    return n == NF-1 ? 0 : n+1;
}

#define left_fork(i)			(i)
#define right_fork(i)			next(i)

// posiciones en pantalla de los filosofos y los tenedores
static point pos[2*NF] ={
    { 55, 11 },
    { 51,  6 },
    { 41,  2 },
    { 28,  2 },
    { 18,  6 },
    { 15, 11 },
    { 18, 17 },
    { 28, 21 },
    { 41, 21 },
    { 51, 17 }
};

#define phil_position(i)		(pos[2*(i)+1])
#define fork_position(i)		(pos[2*(i)])

static void
mprint(point where, char *msg, unsigned color){
    Atomic();
    mt_cons_gotoxy(where.x, where.y);
    cprintk(color, BLACK, "%-10.10s", msg);
    Unatomic();
}

static void 
random_delay(int d1, int d2){
    unsigned t;

    Atomic();
    t = d1 + rand() % (d2-d1);
    Unatomic();

    Delay(t);
}

static void
think(int i){
    mprint(phil_position(i), "THINKING", COLOR_THINKING);
    random_delay(MIN_THINK, MAX_THINK);
}

static void
become_hungry(int i){
    mprint(phil_position(i), "HUNGRY", COLOR_HUNGRY);
}

static void
eat(int i){
    mprint(phil_position(i), "EATING", COLOR_EATING);
    random_delay(MIN_EAT, MAX_EAT);
}

static bool
forks_avail(int i){
    return !((GlobalVars *)mt_curr_task->data)->in_use[left_fork(i)]
        && !((GlobalVars *)mt_curr_task->data)->in_use[right_fork(i)];
}

static void
take_forks(int i){
    ((GlobalVars *)mt_curr_task->data)->in_use[left_fork(i)] =
        ((GlobalVars *)mt_curr_task->data)->in_use[right_fork(i)] = true;
    mprint(fork_position(left_fork(i)), "used", COLOR_USED);
    mprint(fork_position(right_fork(i)), "used", COLOR_USED);
}

static void
leave_forks(int i){
    ((GlobalVars *)mt_curr_task->data)->in_use[left_fork(i)] =
        ((GlobalVars *)mt_curr_task->data)->in_use[right_fork(i)] = false;
    mprint(fork_position(left_fork(i)), "free", COLOR_FREE);
    mprint(fork_position(right_fork(i)), "free", COLOR_FREE);
}

static void
philosopher(void *arg){
    int i;
    unsigned len = sizeof i;
    GlobalVars *gv = (GlobalVars *)mt_curr_task->data;

    Receive(NULL, &i, &len);

    while ( true ){
        think(i);
        become_hungry(i);

        EnterMonitor(gv->mon);
        while ( !forks_avail(i) )
            WaitCondition(gv->cond);
        take_forks(i);
        LeaveMonitor(gv->mon);

        eat(i);

        EnterMonitor(gv->mon);
        leave_forks(i);
        BroadcastCondition(gv->cond);
        LeaveMonitor(gv->mon);
    }
}

int 
simple_phil_main(int argc, char *argv[]){
    int i;
    unsigned c;
    Task_t *task[NF];
    bool cursor;
    GlobalVars *gv = Malloc(sizeof(GlobalVars));

    // Inicializar display
    mt_cons_clear();
    cursor = mt_cons_cursor(false);
    cprintk(LIGHTGREEN, BLACK, "Oprima cualquier tecla para salir");

    // Inicializar estructuras de datos
    gv->mon = CreateMonitor(NULL);
    gv->cond = CreateCondition(NULL, gv->mon);
    for ( i = 0 ; i < NF ; i++ ){
        gv->in_use[i] = false;
        mprint(fork_position(i), "free", COLOR_FREE);
    }
    SetData(mt_curr_task, gv);

    // Inicializar el generador de n�meros aleatorios
    srand(mt_ticks);

    // Crear y arrancar procesos
    for ( i = 0 ; i < NF ; i++ ){
        task[i] = CreateTask(philosopher, 0, NULL, NULL, DEFAULT_PRIO);
        SetData(task[i], gv);
        Ready(task[i]);
        Send(task[i], &i, sizeof i);
    }

    // Esperar una tecla para salir
    while ( !mt_kbd_getch(&c) )
        ;

    // Liquidar procesos y destruir recursos
    for ( i = 0 ; i < NF ; i++ )
        DeleteTask(task[i]);
    DeleteCondition(gv->cond);
    DeleteMonitor(gv->mon);

    // Reponer pantalla
    mt_cons_clear();
    mt_cons_cursor(cursor);

    return 0;
}
