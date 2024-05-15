/* Implementação de um Shell*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

void rotate_pipes(int fd[2][2], int* current_fd, int* previous_fd) {
	//Alterna as variáveis entre 0 e 1
	*previous_fd = *current_fd;
	*current_fd += 1;
	*current_fd %= 2;

	if (pipe(fd[*current_fd]) < 0) {
		perror("pipe()");
		exit(-1);
	}
}

void execute_command(char **argv, int command_index) {
	char *cmd, **cmd_argv;
	cmd = argv[command_index];
	cmd_argv = &argv[command_index];
	if (execvp(cmd, cmd_argv) < 0) {
		perror("execvp()");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	if (argc == 1) {
		printf("Uso: %s <cmd> <p1> <p2> ... <pn>\n", argv[0]);
		return 0;
	}

	int fd[2][2];

	// command_index aponta para a posição do argv onde está o próximo comando a ser executado
	// input_from_pipe é uma flag que diz se há ou não entrada esperando no pipe
	int i, command_index = 1, current_fd = 0, previous_fd = 0, input_from_pipe = 0, output_to_pipe = 0;
	for (i = 1; i <= argc; i++) {
		char* operator = argv[i];
		if (!operator || strcmp(operator, "|") == 0) {
			argv[i] = NULL;
			if (!output_to_pipe) {
				rotate_pipes(fd, &current_fd, &previous_fd);
			}

			pid_t p_id;
			p_id = fork();
			if (p_id < 0) {
				perror("fork()");
				exit(EXIT_FAILURE);
			}

			if (p_id == 0) {
				// Filho

				if (operator || output_to_pipe) {
					// O operador é um pipe, conecte-o no stdout

					dup2(fd[current_fd][1], STDOUT_FILENO);
				}
				if (input_from_pipe) {
					// Se houver entrada esperando no pipe, conecte-o no stdin

					dup2(fd[previous_fd][0], STDIN_FILENO);
					close(fd[previous_fd][0]);
				}
				close(fd[current_fd][1]);
				execute_command(argv, command_index);
			}
			else {
				// Pai

				close(fd[current_fd][1]);
				if (input_from_pipe) {
					close(fd[previous_fd][0]);
				}
				if (operator) {
					// O operador é um pipe

					input_from_pipe = 1;
				}
				else {
					// O operador é null

					input_from_pipe = 0;
				}
				output_to_pipe = 0;
				//printf("Pai (%d) esperando filho (%d) terminar. Operador: %s\n", (int)getpid(), p_id, operator);
				waitpid(p_id, NULL, 0);
				//printf("Filho acabou.\n");
				command_index = i + 1;
			}
		}
		else if (strcmp(operator, "<") == 0) {
			argv[i] = NULL;
			rotate_pipes(fd, &current_fd, &previous_fd);

			pid_t p_id;
			p_id = fork();
			if (p_id < 0) {
				perror("fork()");
				exit(EXIT_FAILURE);
			}

			if (p_id == 0) {
				//Filho
				char* path = argv[i+1];
				FILE* arquivo;

				arquivo = fopen(path, "r");
				char msg;
				while ((msg = fgetc(arquivo)) != EOF) {
					write(fd[current_fd][1], &msg, sizeof(char));
				}
				close(fd[current_fd][0]);
				close(fd[current_fd][1]);
				fclose(arquivo);
				return 0;
				
			}
			else {
				// Pai
				input_from_pipe = 1;
				//printf("Pai (%d) esperando filho (%d) terminar. Operador: %s\n", (int)getpid(), p_id, operator);
				waitpid(p_id, NULL, 0);
				//printf("Filho acabou.\n");
				close(fd[current_fd][1]);
			}
		}
		else if (strcmp(operator, ">") == 0 || strcmp(operator, ">>") == 0) {
			argv[i] = NULL;
			rotate_pipes(fd, &current_fd, &previous_fd);

			pid_t p_id;
			p_id = fork();
			if (p_id < 0) {
				perror("fork()");
				exit(EXIT_FAILURE);
			}

			if (p_id == 0) {
				// Filho
				char* path = argv[i+1];
				FILE* arquivo;

				if (strcmp(operator, ">") == 0) {
					arquivo = fopen(path, "w");
				}
				if (strcmp(operator, ">>") == 0) {
					arquivo = fopen(path, "a");
				}

				#define PIPE_SIZE 4096

				char msg[PIPE_SIZE];
				int nr;
				bzero(msg, PIPE_SIZE);
				nr = read(fd[current_fd][0], msg, PIPE_SIZE);
				if (nr < 0) {
					perror("read()");
					return -1;
				}
				fputs(msg, arquivo);
				close(fd[current_fd][0]);
				close(fd[current_fd][1]);
				fclose(arquivo);
				return 0;
			}
			else {
				// Pai
				output_to_pipe = 1;
				//printf("Pai (%d) executando filho (%d). Operador: %s\n", (int)getpid(), p_id, operator);
			}
		}
		else if (strcmp(operator, "&&") == 0 || strcmp(operator, "||") == 0) {
			argv[i] = NULL;
			if (!output_to_pipe) {
				rotate_pipes(fd, &current_fd, &previous_fd);
			}

			int status;
			int is_and = 0;
			if (strcmp(operator, "&&") == 0) {
				is_and = 1;
			}

			pid_t p_id = fork();
			if (p_id < 0) {
				perror("fork()");
				exit(EXIT_FAILURE);
			}

			if (p_id == 0) {
				// Filho
				if (input_from_pipe) {
					dup2(fd[previous_fd][0], STDIN_FILENO);
					close(fd[previous_fd][0]);
				}
				if (output_to_pipe) {
					dup2(fd[current_fd][1], STDOUT_FILENO);
				}
				execute_command(argv, command_index);
			}
			else {
				// Pai
				close(fd[current_fd][1]);
				if (input_from_pipe) {
					close(fd[previous_fd][0]);
				}
				//printf("Pai (%d) esperando filho (%d) terminar. Operador: %s\n", (int)getpid(), p_id, operator);
				if (waitpid(p_id, &status, 0)<0){
					perror("waitpid");
					exit(EXIT_FAILURE);
				}
				//printf("Filho acabou.\n");
				
				if (!WIFEXITED(status)) {
					perror("waitpid");
					exit(EXIT_FAILURE);
				}
				if (is_and && WEXITSTATUS(status)) {
					return 0;
				}
				else if (!is_and && !WEXITSTATUS(status)) {
					return 0;
				}
				output_to_pipe = 0;
				command_index = i+1;
			}
		}
		else if (strcmp(operator, "&") == 0) {
			argv[i] = NULL;
			if (!output_to_pipe) {
				rotate_pipes(fd, &current_fd, &previous_fd);
			}

			pid_t p_id = fork();
			if (p_id < 0) {
				perror("fork()");
				exit(EXIT_FAILURE);
			}

			if (p_id == 0) {
				// Filho
				if (input_from_pipe) {
					dup2(fd[previous_fd][0], STDIN_FILENO);
					close(fd[previous_fd][0]);
				}
				if (output_to_pipe) {
					dup2(fd[current_fd][1], STDOUT_FILENO);
				}
				execute_command(argv, command_index);
			}
			else {
				// Pai
				close(fd[current_fd][1]);
				if (input_from_pipe) {
					close(fd[previous_fd][0]);
				}
				//printf("Pai (%d): criando filho (%d) para executar em background. Operador: %s\n", (int)getpid(), p_id, operator);
				command_index = i+1;
			}
		}
	}
	return 0;
}
