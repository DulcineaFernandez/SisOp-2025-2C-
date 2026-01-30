#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    envid_t me = sys_getenvid();
    int prio = sys_get_priority(me);

    // para diferenciar los tres procesos segun su ID
    // (esto es solo para el test)
    int tipo = me % 3;
    if (tipo == 0)
        sys_set_priority(me, 2);  // alta prioridad
    else if (tipo == 1)
        sys_set_priority(me, 5);  // media
    else
        sys_set_priority(me, 9);  // baja

    prio = sys_get_priority(me);
    cprintf("[Proceso %x] Inicia con prioridad %d\n", me, prio);

    for (int i = 0; i < 10; i++) {
        prio = sys_get_priority(me);
        for (int j = 0; j < 10000000; j++); // simulamos carga para que el test no sea tan "amable"
        if (i > 2 && prio == 9) {
            int r = sys_set_priority(me, 3);
            cprintf("[Proceso %x] Intentamos mejorar prioridad (devuelve %d)\n", me, r);
        }
        if (i == 8 && prio == 5) {
            int r = sys_set_priority(me, 8);
            cprintf("[Proceso %x] Intentamos empeorar prioridad (devuelve %d)\n", me, r);
        }
        cprintf("[Proceso %x] Iteracion %d (prio %d)\n", me, i, prio);
        sys_yield();
}

    cprintf("[Proceso %x] Finaliza con prioridad %d\n", me, prio);
}