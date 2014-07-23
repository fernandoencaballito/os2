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
} point;

typedef struct{
    Task_t *phil[NF];   // filósofos
    bool in_use[NF];    // tenedores usados
    bool hungry[NF];    // filósofos hambrientos
} GlobalVars;

static int
prev(int n){
    return n == 0 ? NF-1 : n-1;
}

static int
next(int n){
    return n == NF-1 ? 0 : n+1;
}

#define left_fork(i)			(i)
#define right_fork(i)			next(i)
#define left_phil(i)			prev(i)
#define right_phil(i)			next(i)

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
    int left, right;
    GlobalVars *gv = (GlobalVars *)mt_curr_task->data;

    Receive(NULL, &i, &len);
    left = left_phil(i);
    right = right_phil(i);

    while ( true )
    {
        think(i);
        become_hungry(i);

        Atomic();
        if ( forks_avail(i) )
            take_forks(i);
        else
        {
            gv->hungry[i] = true;
            Pause();
            gv->hungry[i] = false;
        }
        Unatomic();

        eat(i);

        Atomic();
        leave_forks(i);
        if ( gv->hungry[left] && forks_avail(left) ){
            take_forks(left);
            Ready(gv->phil[left]);
        }
        if ( gv->hungry[right] && forks_avail(right) ){
            take_forks(right);
            Ready(gv->phil[right]);
        }
        Unatomic();
    }
}

int 
atomic_phil_main(int argc, char *argv[]){
    int i;
    unsigned c;
    bool cursor;
    GlobalVars *gv = Malloc(sizeof(GlobalVars));

    // Inicializar display
    mt_cons_clear();
    cursor = mt_cons_cursor(false);
    cprintk(LIGHTGREEN, BLACK, "Oprima cualquier tecla para salir");

    // Inicializar estructuras de datos
    for ( i = 0 ; i < NF ; i++ ){
        gv->hungry[i] = false;
        gv->in_use[i] = false;
        mprint(fork_position(i), "free", COLOR_FREE);
    }
    SetData(mt_curr_task, gv);

    // Inicializar el generador de números aleatorios
    srand(mt_ticks);

    // Crear y arrancar procesos
    for ( i = 0 ; i < NF ; i++ ){
        gv->phil[i] = CreateTask(philosopher, 0, NULL, NULL, DEFAULT_PRIO);
        SetData(gv->phil[i], gv);
        Ready(gv->phil[i]);
        Send(gv->phil[i], &i, sizeof i);
    }

    // Esperar una tecla para salir
    while ( !mt_kbd_getch(&c) )
        ;

    // Liquidar procesos
    for ( i = 0 ; i < NF ; i++ )
        DeleteTask(gv->phil[i]);

    // Reponer pantalla
    mt_cons_clear();
    mt_cons_cursor(cursor);

    return 0;
}
