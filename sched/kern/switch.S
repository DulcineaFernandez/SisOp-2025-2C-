/*
 * Your code here for context_switch
 *
 * It should restore the register values in the Trapframe with the 'iret' instruction.
 * This exits the kernel and starts executing some environment's code.
 *
 * This function does not return.
 */

/*Llamada en env_run context_switch(&e->env_tf);
//recibe la structura de env_tf (Trapframe)
 el cual ya tiene un orden en sus elementos (puesto por la catedra)
que nos facilita todo a nosotros 
Para poder usar la funcion: popal y pushal y eleminar todo más rápido.
*/

.globl context_switch;
context_switch:
	mov 4(%esp), %esp
	popa
	pop %es
	pop %ds
	add $8, %esp
	iret

spin:
	jmp spin
