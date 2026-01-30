#include "exec.h"
#include <fcntl.h>
#include <string.h>

// sets "key" with the key part of "arg"
// and null-terminates it
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  key = "KEY"
// "PATH=/usr/bin"
// KEY : PATH
static void
get_environ_key(char *arg, char *key)
{
	int i;
	for (i = 0; arg[i] != '='; i++)
		key[i] = arg[i];

	key[i] = END_STRING;
}

// sets "value" with the value part of "arg"
// and null-terminates it
// "idx" should be the index in "arg" where "=" char
// resides
//
// Example:
//  - KEY=value
//  arg = ['K', 'E', 'Y', '=', 'v', 'a', 'l', 'u', 'e', '\0']
//  value = "value"
// "PATH=/usr/bin"
// VALUE /usr/bin"
static void
get_environ_value(char *arg, char *value, int idx)
{
	size_t i, j;
	for (i = (idx + 1), j = 0; i < strlen(arg); i++, j++)
		value[j] = arg[i];

	value[j] = END_STRING;
}

// sets the environment variables received
// in the command line
//
// Hints:
// - use 'block_contains()' to
// 	get the index where the '=' is
// - 'get_environ_*()' can be useful here

static void
set_environ_vars(char **eargv, int eargc)
{
	char key[BUFLEN];
	char value[BUFLEN];

	for (int i = 0; i < eargc; i++) {
		int indice = block_contains(eargv[i], '=');
		if (indice != -1) {
			get_environ_key(eargv[i], key);
			get_environ_value(eargv[i], value, indice);
			setenv(key, value, 1);
		}
	}
}

// opens the file in which the stdin/stdout/stderr
// flow will be redirected, and returns
// the file descriptor
//
// Find out what permissions it needs.
// Does it have to be closed after the execve(2) call?
//
// Hints:
// - if O_CREAT is used, add S_IWUSR and S_IRUSR
// 	to make it a readable normal file
static int
open_redir_fd(char *file, int flags)
{
	int fd;

	if (file == NULL) {
		return -1;
	}

	if (strcmp(file, "&1") == 0) {
		return 1;
	}

	fd = open(file, flags, 0644);

	if (fd == -1) {
		fprintf_debug(stderr, "Error opening file: %s\n", file);
	}

	return fd;
}

// executes a command - does not return
//
// Hint:
// - check how the 'cmd' structs are defined
// 	in types.h
// - casting could be a good option

// ESTA ES LA FUNCIÓN PRINCIPAL
// ACÁ NOS FIJAMOS QUE COMANDO SE MANDO
// SEGUN EL COMANDO HACEMOS LA APRTE QUE NOS PIDE
void
exec_cmd(struct cmd *cmd)
{
	// To be used in the different cases
	// Punteros que representan cada tipo de comadno
	struct execcmd *e;
	struct backcmd *b;
	struct execcmd *r;
	struct pipecmd *p;

	switch (cmd->type) {
	case EXEC:
		e = (struct execcmd *) cmd;
		set_environ_vars(e->eargv, e->eargc);
		if (execvp(e->argv[0], e->argv) < 0) {
			fprintf(stderr, "exec failed : %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
		break;

	case BACK: {
		b = (struct backcmd *) cmd;
		exec_cmd(b->c);
		break;
	}
	case REDIR: {
		// PARA REDIRECCIONES. PARTE DOS
		// VAMOS A USAR OPEN_REDIR_FD() DUP2()
		//  changes the input/output/stderr flow
		//
		//  To check if a redirection has to be performed
		//  verify if file name's length (in the execcmd struct)
		//  is greater than zero
		//

		struct execcmd *e = (struct execcmd *) cmd;
		pid_t pid = fork();

		if (pid == 0) {
			if (strlen(e->in_file) > 0) {
				int fd = open_redir_fd(e->in_file, O_RDONLY);
				if (fd == -1) {
					fprintf_debug(
					        stderr, "Error opening input file: %s\n", e->in_file);
					_exit(EXIT_FAILURE);
				}
				if (dup2(fd, 0) == -1) {
					fprintf_debug(
					        stderr,
					        "Error in dup2 for input\n");
					close(fd);
					_exit(EXIT_FAILURE);
				}
				close(fd);
			}

			if (strlen(e->out_file) > 0) {
				int fd = open_redir_fd(e->out_file,
				                       O_WRONLY | O_CREAT |
				                               O_TRUNC);
				if (fd == -1) {
					fprintf_debug(
					        stderr, "Error opening output file: %s\n", e->out_file);
					_exit(EXIT_FAILURE);
				}
				if (dup2(fd, 1) == -1) {
					fprintf_debug(
					        stderr,
					        "Error in dup2 for output\n");
					close(fd);
					_exit(EXIT_FAILURE);
				}
				close(fd);
			}

			if (strlen(e->err_file) > 0) {
				if (strcmp(e->err_file, "&1") == 0) {
					if (dup2(1, 2) == -1) {
						fprintf_debug(
						        stderr, "Error in dup2 for 2>&1\n");
						_exit(EXIT_FAILURE);
					}
				} else {
					int fd = open_redir_fd(e->err_file,
					                       O_WRONLY | O_CREAT |
					                               O_TRUNC);
					if (fd == -1) {
						fprintf_debug(
						        stderr,
						        "Error opening error "
						        "file: %s\n",
						        e->err_file);
						_exit(EXIT_FAILURE);
					}
					if (dup2(fd, 2) == -1) {
						fprintf_debug(
						        stderr, "Error in dup2 for error\n");
						close(fd);
						_exit(EXIT_FAILURE);
					}
					close(fd);
				}
			}

			if (execvp(e->argv[0], e->argv) < 0) {
				fprintf_debug(stderr,
				              "exec failed : %s\n",
				              strerror(errno));
				_exit(EXIT_FAILURE);
			}

		} else if (pid < 0) {
			fprintf_debug(stderr, "fork failed\n");
			_exit(EXIT_FAILURE);

		} else {
			wait(NULL);
		}

		break;
	}

	case PIPE: {
		// PARA LOS CASOS DE TUBERIAS
		// pipes two commands

		p = (struct pipecmd *) cmd;

		int pipefd[2];

		if (pipe(pipefd) == -1) {
			fprintf_debug(stderr, "pipe failed\n");
			_exit(-1);
		}

		pid_t pid1 = fork();

		if (pid1 == 0) {
			setpgid(0, 0);
			close(pipefd[0]);
			dup2(pipefd[1], 1);
			close(pipefd[1]);

			exec_cmd(p->leftcmd);
			_exit(EXIT_FAILURE);
		}

		pid_t pid2 = fork();

		if (pid2 == 0) {
			setpgid(0, 0);
			close(pipefd[1]);
			dup2(pipefd[0], 0);
			close(pipefd[0]);
			exec_cmd(p->rightcmd);
			_exit(EXIT_FAILURE);
		}

		close(pipefd[0]);
		close(pipefd[1]);

		wait(pid1);
		wait(pid2);

		free_command(parsed_pipe);

		_exit(EXIT_SUCCESS);
	}
	}
}
