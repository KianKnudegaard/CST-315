#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include "VMmanager.h"
#include "advancedScheduler.h"
#include "fileSystem.h"

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

void add_job(pid_t pid, const char *input) {
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
    int i = 0;
    char *token = strtok(input, " \t\r\n");
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

void create_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("Error creating file");
    } else {
        printf("File '%s' created successfully.\n", filename);
        fclose(fp);
    }
}

void delete_file(const char *filename) {
    if (remove(filename) == 0) {
        printf("File '%s' deleted successfully.\n", filename);
    } else {
        perror("Error deleting file");
    }
}

// Executes a single command with redirection support
void execute_single_command(char **args, int background) {
    int in_redirect = -1, out_redirect = -1;
    char *input_file = NULL;
    char *output_file = NULL;

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

        if (execvp(args[0], args) == -1) {
            perror("Command failed");
            exit(1);
        }
    }
    else if (pid > 0) {
        // register the command with the scheduler (before wait!)
        PCB *vm_proc = sched_create_process(2, 1);
        enqueue(&ready_queue, vm_proc);
        if (pid_map_count < MAX_PID_MAP) {
            pid_map[pid_map_count].shell_pid = pid;
            pid_map[pid_map_count].vm_pid = vm_proc->pid;
            pid_map_count++;
        }

        if (!background) {
            waitpid(pid, NULL, 0);
        }
        else {
            printf("Process running in background with PID %d\n", pid);
            add_job(pid, args[0]);
        }
    }

    else {
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
        } if (pid > 0) {
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

    start_scheduler_threads();

    init_file_system();

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

            if (strchr(line, '|') != NULL) {
                execute_pipeline(line, 0); // background not handled in this pipeline version
                continue;
            }

            char *args[MAX_ARGS];
            parse_input(line, args);
            if (args[0] == NULL) continue;

            if (strcmp(args[0], "exit") == 0) {
                free_node(fs_root);
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
            if (strcmp(args[0], "memaccess") == 0) {
                if (args[1] && args[2] && args[3]) {
                    pid_t spid = atoi(args[1]);
                    char mode = args[2][0];
                    int vaddr = atoi(args[3]);

                    for (int k = 0; k < pid_map_count; k++) {
                        if (pid_map[k].shell_pid == spid) {
                            int vm_pid = pid_map[k].vm_pid;
                            access_memory(&processes[vm_pid - 1], vm_pid, vaddr, mode);
                            break;
                        }
                    }
                } else { printf("Usage: memaccess <shell_pid> <r/w> <virtual_address>\n"); }
                continue;
            }

            if (strcmp(args[0], "create") == 0) {
                if (args[1] == NULL) { printf("Usage: create <file_name>\n"); }
                else { create_file(args[1]); }
                continue;
            }

            if (strcmp(args[0], "delete") == 0) {
                if (args[1] == NULL) { printf("Usage: delete <file_name>\n"); }
                else { delete_file(args[1]); }
                continue;
            }

            if (strcmp(args[0], "mkdir") == 0) {
                fs_mkdir(args[1]);
                continue;
            }

            if (strcmp(args[0], "rmdir") == 0) {
                int force = args[2] && strcmp(args[2], "-f") == 0;
                fs_rmdir(args[1], force);
                continue;
            }

            if (strcmp(args[0], "createf") == 0) {
                if (!args[1] || !args[2]) { printf("Usage: createf <file_name> <size>\n"); }
                else { fs_create_file(args[1], atoi(args[2])); }
                continue;
            }

            if (strcmp(args[0], "deletef") == 0) {
                if (!args[1]) { printf("Usage: deletef <file_name>\n"); }
                else { fs_delete_file(args[1]); }
                continue;
            }

            if (strcmp(args[0], "move") == 0) {
                if (!args[1] || !args[2]) { printf("Usage: move <source> <destination_dir>\n"); }
                else { fs_move(args[1], args[2]); }
                continue;
            }

            if (strcmp(args[0], "copy") == 0) {
                if (!args[1] || !args[2]) { printf("Usage: copy <source> <new_name>\n"); }
                else { fs_copy(args[1], args[2]); }
                continue;
            }

            if (strcmp(args[0], "copydir") == 0) {
                if (!args[1] || !args[2]) { printf("Usage: copydir <source_directory> <new_name>\n"); }
                else { fs_copy_dir(args[1], args[2]); }
                continue;
            }

            if (strcmp(args[0], "search") == 0) {
                if (!args[1]) { printf("Usage: search <file_or_dir_name>\n"); }
                else {
                    FSNode *result = fs_search(fs_root, args[1]);
                    if (result) {
                        printf("Found: %s (%s)\n", result->name,
                               result->type == DIR_NODE ? "Directory" : "File");
                    } else { printf("Not found.\n"); }
                }
                continue;
            }

            if (strcmp(args[0], "tree") == 0) {
                fs_tree();
                continue;
            }

            if (strcmp(args[0], "info") == 0 && args[2] && strcmp(args[2], "-d") == 0) {
                fs_info(args[1], 1);
                continue;
            }

            if (strcmp(args[0], "info") == 0) {
                if (!args[1]) { printf("Usage: info <name>\n"); }
                else { fs_info(args[1], 0); }
                continue;
            }

            if (strcmp(args[0], "pwd") == 0) {
                fs_pwd();
                continue;
            }

            if (strcmp(args[0], "cd") == 0) {
                if (!args[1]) { fprintf(stderr, "cd: missing argument\n"); }
                else { fs_cd(args[1]); }
                continue;
            }

            if (strcmp(args[0], "rename") == 0) {
                if (!args[1] || !args[2]) {fprintf(stderr, "rename: missing argument\n");}
                else { fs_rename(args[1], args[2]); }
            }

            if (strcmp(args[0], "editfile") == 0) {
                if (!args[1] || !args[2]) { fprintf(stderr, "editfile: missing argument\n"); }
                else { fs_edit_file(args[1], args[2]); }
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
