#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>



int prepare(void)
{
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

int execute_command(char** command_arglist, int* input_fd, int* output_fd, char* output_file, char* input_file)
{
	int pid;

	pid = fork();
	if (pid == 0) {
		// child process
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
		execvp(command_arglist[0], command_arglist);
		perror("execvp failed");
		exit(1);

	} else if (pid < 0) {
		perror("fork failed");
		exit(1);
	}
	return pid;
}


int check_background_execution(int count, char** arglist)
{
	if (count > 0 && strcmp(arglist[count - 1], "&") == 0) {
		arglist[count - 1] = NULL; // remove the '&' from the argument list
		return 1; // indicates that the command should be run in the background
	}
	return 0; // indicates that the command should be run in the foreground
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

        if (i == pipe_count && count >= 3) { // last command check input/output redirection
            if (strcmp(arglist[count - 2], ">") == 0) {
                output_file = arglist[count - 1];
                command_arglist[count - 2] = NULL;
                command_arglist[count - 1] = NULL;
            } else if (strcmp(arglist[count - 2], "<") == 0) {
                input_file = arglist[count - 1];
                command_arglist[count - 2] = NULL;
                command_arglist[count - 1] = NULL;
            }
        }

		pids[i] = execute_command(command_arglist, &prev_input_fd, &next_output_fd, output_file, input_file);

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
			waitpid(pids[i], NULL, 0);
		}
	}

	free(pids);
	free(pipe_indices);

	return 1;
}