#include "defs.h"
#include "types.h"
#include "readline.h"
#include "runcmd.h"
#include "unistd.h"
#include "stdio.h"
#include "signal.h"
#include "sys/wait.h"
#include "stdlib.h"
#include "errno.h"

char prompt[PRMTLEN] = { 0 };
static void *alt_stack = NULL;
void sigchild_handler(int sig);
void altstack_init(void);
void signal_handler(void);
void int_to_str(int num, char *buf);
void print_bg(pid_t pid, const char *prompt);


// no manejamos numeros negativos en la funcion
void
int_to_str(pid_t num, char *buf)
{
	char buff_aux[32];  // usamos un buffer auxiliar para guardar los digitos al reves
	int pos = 0;

	if (num == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return;
	}

	while (num > 0 && pos < (int) sizeof(buff_aux) - 1) {
		buff_aux[pos++] = (num % 10) + '0';
		num /= 10;
	}

	// lo copiamos ahora en el orden correcto
	int i = 0;
	for (int j = pos - 1; j >= 0; j--) {
		buf[i++] = buff_aux[j];
	}
	buf[i] = '\0';
}

void
print_bg(pid_t pid, const char *prompt)
{
	char numbuf[32];
	int_to_str(pid,
	           numbuf);  // usamos la función para convertir el pid a string

	// mensaje base
	const char *mensaje = "==> terminado: PID=";

	// lo separamos del prompt actual
	ssize_t ignorar1 = write(STDOUT_FILENO, "\n", 1);
	(void) ignorar1;
	ssize_t ignorar2 = write(STDOUT_FILENO, mensaje, strlen(mensaje));
	(void) ignorar2;
	ssize_t ignorar3 = write(STDOUT_FILENO, numbuf, strlen(numbuf));
	(void) ignorar3;
	ssize_t ignorar4 = write(STDOUT_FILENO, "\n", 1);
	(void) ignorar4;

#ifndef SHELL_NO_INTERACTIVE
	// re imprimimos el prompt
	if (prompt != NULL) {
		ssize_t ignorar5 = write(STDOUT_FILENO, prompt, strlen(prompt));
		(void) ignorar5;
		ssize_t ignorar6 = write(STDOUT_FILENO, "\n$ ", 3);
		(void) ignorar6;
	}
#endif
}

void
sigchild_handler(int sig)
{  // le pongo el (void) al sig, porque tiene que estar el "sig" para que sea una firma validad para sigaction
	(void) sig;  // esta para corroborar que no llego otra cosa que no esperaba------
	int status;
	int pid;
	int copia_errno = errno;

	while ((pid = waitpid(0, &status, WNOHANG)) > 0) {
		print_bg(pid, prompt);
	}
	errno = copia_errno;
}

void
altstack_init()
{
	alt_stack = malloc(SIGSTKSZ);
	if (alt_stack == NULL) {
		perror("Error en Malloc");
		exit(-1);
	}

	stack_t ss;
	ss.ss_sp = alt_stack;
	ss.ss_size = SIGSTKSZ;
	ss.ss_flags = 0;

	if (sigaltstack(&ss, NULL) == -1) {
		perror("Error en sigaltstack");
		exit(-1);
	}
}

void
signal_handler()
{
	altstack_init();  // Usamos un stack alternativo para el handler
	// asi evitamos problemas si la señal llega en un contexto donde el stack principal esta saturado o incosistente

	struct sigaction sa;
	sa.sa_handler = sigchild_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_ONSTACK | SA_NOCLDSTOP;
	// Con SA_ONSTACK le decimos al handler que se ejecute en el stack alternativo
	// Con SA_RESTART reintenta syscalls interrumpidas
	// Con SA_NOCLDSTOP ignora señales cuando un hijo se pausa

	if (sigaction(SIGCHLD, &sa, NULL)) {
		perror("Error en sigaction");
		exit(-1);
	}
}

// runs a shell command
static void
run_shell()
{
	char *cmd;

	while ((cmd = read_line(prompt)) != NULL)
		if (run_cmd(cmd) == EXIT_SHELL)
			return;
}

// initializes the shell
// with the "HOME" directory
static void
init_shell()
{
	char buf[BUFLEN] = { 0 };
	char *home = getenv("HOME");

	if (chdir(home) < 0) {
		snprintf(buf, sizeof buf, "cannot cd to %s ", home);
		perror(buf);
	} else {
		snprintf(prompt, sizeof prompt, "(%s)", home);
	}

	signal_handler();
}


int
main(void)
{
	init_shell();

	run_shell();

	free(alt_stack);
	return 0;
}
