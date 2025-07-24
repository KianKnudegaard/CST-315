#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#include "VMmanager.h"

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_JOBS 64

#define MAX_PID_MAP 64

typedef struct {
    pid_t shell_pid;     // From fork()
    int vm_pid;          // From create_process()
} PIDMap;

PIDMap pid_map[MAX_PID_MAP];
int pid_map_count = 0;


typedef struct {
    pid_t pid;
    char command[MAX_LINE];
    int active;
} Job;

Job jobs[MAX_JOBS];
int job_count = 0;

void add_job(pid_t pid, char *input) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].command, input, MAX_LINE - 1);
        jobs[job_count].command[MAX_LINE - 1] = '\0';
        jobs[job_count].active = 1;
        job_count++;
    } else {
        fprintf(stderr, "Too many background jobs\n");
    }
}

void parse_input(char *input, char **args) {
    char *token;
    int i = 0;
    token = strtok(input, " \t\r\n");
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    args[i] = NULL;
}

void handle_exit_signal(int sig) {
    printf("\n[Shell exiting via signal]\n");
    exit(0);
}

// Executes a single command with redirection support
void execute_single_command(char **args, int background) {
    int in_redirect = -1, out_redirect = -1;
    char *input_file = NULL;
    char *output_file = NULL;
    int vm_pid;

    // Scan for redirection operators
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            input_file = args[i + 1];
            in_redirect = i;
        }
        if (strcmp(args[i], ">") == 0) {
            output_file = args[i + 1];
            out_redirect = i;
        }
    }

    // Remove redirection parts from args
    if (in_redirect != -1) args[in_redirect] = NULL;
    if (out_redirect != -1) args[out_redirect] = NULL;

    pid_t pid = fork();

    if (pid == 0) {
        // Handle input redirection
        if (input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                perror("Failed to open input file");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Handle output redirection
        if (output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Failed to open output file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (pid == 0) {

            if (execvp(args[0], args) == -1) {
                perror("Command failed");
                exit(1);
            }
        }
        else if (pid > 0) {
            // In the parent shell, save the mapping
            pid_map[pid_map_count++] = (PIDMap){pid, vm_pid};
        }
    } else if (pid > 0) {
        if (!background) {
            waitpid(pid, NULL, 0);
        } else {
            printf("Process running in background with PID %d\n", pid);
            add_job(pid, args[0]);
        }
    } else {
        perror("Fork failed");
    }
}

// Handles piping by splitting commands at '|'
void execute_pipeline(char *line, int background) {
    char *commands[10];
    int num_cmds = 0;

    commands[num_cmds++] = strtok(line, "|");
    while ((commands[num_cmds] = strtok(NULL, "|")) != NULL && num_cmds < 9) {
        num_cmds++;
    }

    int in_fd = 0;
    pid_t pids[10];

    for (int i = 0; i < num_cmds; i++) {
        int pipefd[2];
        if (i < num_cmds - 1) {
            pipe(pipefd); // Create a pipe only if it's not the last command
        }

        char *args[MAX_ARGS];
        char *cmd = commands[i];
        while (*cmd == ' ') cmd++;
        cmd[strcspn(cmd, "\n")] = 0;
        parse_input(cmd, args);

        pid_t pid = fork();
        if (pid == 0) {
            // CHILD
            if (in_fd != 0) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (i < num_cmds - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
            }
            execvp(args[0], args);
            perror("execvp failed");
            exit(1);
        } else if (pid > 0) {
            // PARENT
            pids[i] = pid;
            if (in_fd != 0) close(in_fd);
            if (i < num_cmds - 1) {
                close(pipefd[1]);
                in_fd = pipefd[0];
            }
        } else {
            perror("fork failed");
            exit(1);
        }
    }

    if (!background) {
        for (int i = 0; i < num_cmds; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }
}


int main(int argc, char *argv[]) {
    FILE *input_source = stdin;
    int batch_mode = 0;

    if (argc == 2) {
        input_source = fopen(argv[1], "r");
        if (!input_source) {
            perror("Failed to open batch file");
            exit(1);
        }
        batch_mode = 1;
    }

    char input[MAX_LINE];
    signal(SIGUSR1, handle_exit_signal);

    while (1) {
        for (int j = 0; j < job_count; j++) {
            if (jobs[j].active) {
                int status;
                pid_t result = waitpid(jobs[j].pid, &status, WNOHANG);
                if (result > 0) {
                    jobs[j].active = 0;
                    printf("[Completed] PID %d - %s\n", jobs[j].pid, jobs[j].command);

                    for (int k = 0; k < pid_map_count; k++) {
                        if (pid_map[k].shell_pid == jobs[j].pid) {
                            free_process(pid_map[k].vm_pid);
                            break;
                        }
                    }
                }
            }
        }

        if (!batch_mode) {
            printf("my_shell> ");
            fflush(stdout);
        }

        if (fgets(input, MAX_LINE, input_source) == NULL) {
            if (batch_mode) {
                input_source = stdin;
                batch_mode = 0;
                continue;
            } else {
                printf("\n");
                break;
            }
        }

        if (strchr(input, '\n') == NULL) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }

        char *commands[MAX_ARGS];
        int command_count = 0;

        char *cmd = strtok(input, ";");
        while (cmd != NULL && command_count < MAX_ARGS) {
            commands[command_count++] = strdup(cmd);
            cmd = strtok(NULL, ";");
        }

        for (int i = 0; i < command_count; i++) {
            char *line = commands[i];
            while (*line == ' ') line++;
            line[strcspn(line, "\n")] = 0;

            // Handle pipeline first
            if (strchr(line, '|') != NULL) {
                execute_pipeline(line, 0); // background not handled in this pipeline version
                continue;
            }

            // Then parse input
            char *args[MAX_ARGS];
            parse_input(line, args);
            if (args[0] == NULL) continue;

            // Check built-in commands
            if (strcmp(args[0], "cd") == 0) {
                if (args[1] == NULL) {
                    fprintf(stderr, "cd: missing argument\n");
                } else {
                    if (chdir(args[1]) != 0) {
                        perror("cd failed");
                    }
                }
                continue;
            }

            if (strcmp(args[0], "exit") == 0) {
                exit(0);
            }

            if (strcmp(args[0], "jobs") == 0) {
                for (int j = 0; j < job_count; j++) {
                    if (jobs[j].active) {
                        printf("[%d] PID %d - %s\n", j + 1, jobs[j].pid, jobs[j].command);
                    }
                }
                continue;
            }

            if (strcmp(args[0], "end") == 0) {
                for (int j = 0; j < job_count; j++) {
                    if (jobs[j].active) {
                        kill(jobs[j].pid, SIGTERM);
                        jobs[j].active = 0;
                    }
                }
                printf("All background jobs terminated.\n");
                continue;
            }
            else if (strcmp(args[0], "memaccess") == 0) {
                if (args[1] && args[2] && args[3]) {
                    pid_t spid = atoi(args[1]);
                    char mode = args[2][0];
                    int vaddr = atoi(args[3]);

                    // Lookup vm_pid
                    for (int i = 0; i < pid_map_count; i++) {
                        if (pid_map[i].shell_pid == spid) {
                            int vm_pid = pid_map[i].vm_pid;
                            access_memory(&processes[vm_pid - 1], vm_pid, vaddr, mode);
                            break;
                        }
                    }
                } else {
                    printf("Usage: memaccess <shell_pid> <r/w> <virtual_address>\n");
                }
                continue;
            }


            // Background job detection
            int j = 0;
            while (args[j] != NULL) j++;
            int background = 0;
            if (j > 0 && strcmp(args[j - 1], "&") == 0) {
                background = 1;
                args[j - 1] = NULL;
            }

            execute_single_command(args, background);
        }

    }

    return 0;
}
