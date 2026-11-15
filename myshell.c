#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define True 1
#define False 0


void signal_handler(int sig) {
    if (sig == SIGCHLD) {
        pid_t res;
        while ((res = waitpid(-1, NULL, WNOHANG)) > 0);
        if (res == -1 && errno != ECHILD && errno != EINTR) {
            perror("waitpid failed in signal handler");
            exit(1);
        }
    }
}


int prepare(void)
{	
	struct sigaction new_action = {.sa_handler = signal_handler, .sa_flags = SA_RESTART};
	sigaction(SIGCHLD, &new_action, NULL);
	signal(SIGINT, SIG_IGN);  // in a shell you donwt quit on CTRL+C.
    return 0;
}

int finalize(void)
{
    return 0;
}


int split_pipeline(int count, char** arglist, int* pipe_count, int** pipe_indices)
{
	*pipe_count = 0;
	*pipe_indices = NULL;

	for (int i = 0; i < count; ++i) {
		if (strcmp(arglist[i], "|") == 0) {
			*pipe_count += 1;
			arglist[i] = NULL; // replace the pipe symbol with NULL to separate commands
			*pipe_indices = (int*) realloc(*pipe_indices, sizeof(int) * (*pipe_count));
			if (*pipe_indices == NULL) {
				perror("realloc failed");
                if (pipe_count != NULL) {
                    free(*pipe_indices);
                }
				return 0;
			}
			(*pipe_indices)[(*pipe_count) - 1] = i;
		}
	}

	return 1;
}

int execute_command(char** command_arglist, int* input_fd, int* output_fd, char* output_file, char* input_file, int is_background, int unused_read)
{
	int pid;

	pid = fork();
	if (pid == 0) {
		// child process
		if (unused_read != -1) {
			close(unused_read);
		}
        if (input_file != NULL) {
            *input_fd = open(input_file, O_RDONLY, 0600);
            if (*input_fd < 0) {
                perror("open input file failed");
                exit(1);
            }
        }
        if (output_file != NULL) {
            *output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (*output_fd < 0) {
                perror("open output file failed");
                exit(1);
            }
        }
		if (*input_fd != STDIN_FILENO) {
			dup2(*input_fd, STDIN_FILENO);
			close(*input_fd);
		}
		if (*output_fd != STDOUT_FILENO) {
			dup2(*output_fd, STDOUT_FILENO);
			close(*output_fd);
		}
		if (!is_background) { // we quit if it's not a background process.
			signal(SIGINT, SIG_DFL);
		}
		execvp(command_arglist[0], command_arglist);
		perror("execvp failed");
		exit(1);

	} else if (pid < 0) {
		perror("fork failed");
		return -1;
	}
	return pid;
}


int check_background_execution(int count, char** arglist)
{
	if (count > 0 && strcmp(arglist[count - 1], "&") == 0) {
		arglist[count - 1] = NULL; // remove the '&' from the argument list
		return True; 
	}
	return False;
}



int process_arglist(int count, char** arglist)
{	
	int* pids;
	int pipe_count;
	int* pipe_indices;
	char** command_arglist;
	int prev_input_fd, next_output_fd;
	int background_execution;
    char* output_file = NULL, *input_file = NULL;



	if (split_pipeline(count, arglist, &pipe_count, &pipe_indices) == 0) {
		return 0;
	}
	
	background_execution = check_background_execution(count, arglist);

	pids = (int*) malloc(sizeof(int) * (pipe_count + 1));
	if (pids == NULL) {
		perror("malloc failed");
		free(pipe_indices);
		return 0;
	}

	prev_input_fd = STDIN_FILENO;

	for (int i = 0;  i<pipe_count+1; i++){
		int pfds[2];
		int pid;
		next_output_fd = STDOUT_FILENO;

		if (i < pipe_count) {
			if (pipe(pfds) == -1) {
				perror("pipe failed");
				free(pids);
				free(pipe_indices);
				return 0;
			}
			next_output_fd = pfds[1];
		}
		command_arglist = (arglist + (i == 0 ? 0 : pipe_indices[i - 1] + 1));

        if (pipe_count == 0 && count >= 3) { // check input/output redirection can be done only if there is no pipe (by assumption))
            if (strcmp(arglist[count - 2], ">") == 0) {
                output_file = arglist[count - 1];
                arglist[count - 2] = NULL;
                arglist[count - 1] = NULL;
            } else if (strcmp(arglist[count - 2], "<") == 0) {
                input_file = arglist[count - 1];
                arglist[count - 2] = NULL;
                arglist[count - 1] = NULL;
            }
        }
		pid = execute_command(command_arglist, &prev_input_fd, &next_output_fd, output_file, input_file, background_execution, (i < pipe_count) ? pfds[0] : -1);
		if (pid < 0) {
			free(pids);
			free(pipe_indices);
			return 0;
		}
		pids[i] = pid;
		if (prev_input_fd != STDIN_FILENO) {
			close(prev_input_fd);
		}
		if (i < pipe_count) {
			close(pfds[1]); 
			prev_input_fd = pfds[0];
		}
	}

	if (background_execution == 0) {
		// if the last command is not run in the background, we wait for it to finish
		for (int i = 0; i < pipe_count + 1; i++) {
			if (waitpid(pids[i], NULL, 0) == -1&& errno != ECHILD && errno != EINTR) {
				perror("waitpid failed");
				free(pids);
				free(pipe_indices);
				return 0;
			}
		}
	}

	free(pids);
	free(pipe_indices);
	return 1;
}