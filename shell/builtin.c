#include "builtin.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// returns true if the 'exit' call
// should be performed
//
// (It must not be called from here)
int
exit_shell(char *cmd)
{
	if (strcmp(cmd, "exit") == 0) {
		return 1;
	}

	return 0;
}

// returns true if "chdir" was performed
//  this means that if 'cmd' contains:
// 	1. $ cd directory (change to 'directory')
// 	2. $ cd (change to $HOME)
//  it has to be executed and then return true
//
//  Remember to update the 'prompt' with the
//  	new directory.
//
// Examples:
//  1. cmd = ['c','d', ' ', '/', 'b', 'i', 'n', '\0']
//  2. cmd = ['c','d', '\0']
int
cd(char *cmd)
{
	char *dir;

	if (strcmp(cmd, "cd") == 0) {
		dir = getenv("HOME");
	}

	else if (strncmp(cmd, "cd ", 3) == 0) {
		dir = cmd + 3;
		while (*dir == ' ')
			dir++;
	}

	else {
		return 0;
	}

	if (chdir(dir) < 0) {
		fprintf(stderr, "cannot cd to %s : %s\n", dir, strerror(errno));
	}

	else if (getcwd(prompt, BUFLEN) == NULL) {
		perror("ERROR_GETCWD");
	}

	return 1;
}

// returns true if 'pwd' was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
int
pwd(char *cmd)
{
	if (strcmp(cmd, "pwd") != 0) {
		return 0;
	}

	char *dir_actual = getcwd(NULL, 0);

	if (dir_actual == NULL) {
		perror("No se pudo obtener el directorio actual");
		return 0;
	}

	printf("%s\n", dir_actual);

	free(dir_actual);

	return 1;
}

// returns true if `history` was invoked
// in the command line
//
// (It has to be executed here and then
// 	return true)
// ES UN CHALLENGE
int
history(char *cmd)
{
	// Your code here

	return 0;
}
