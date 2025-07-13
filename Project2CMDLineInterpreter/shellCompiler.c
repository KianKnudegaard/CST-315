#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_JOBS 64

typedef struct {
    pid_t pid;
    char command[MAX_LINE];
    int active;
} Job;

Job jobs[MAX_JOBS]; // global job list
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

void execute_command(char **args, int background) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "Command not found: %s\n", args[0]);
        }
        exit(1);
    } else {
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
        } else {
            printf("Process running in background with PID %d\n", pid);
            add_job(pid, args[0]); // or pass input string for full command
        }
    }

}


void parse_input(char *input, char **args) {
    char *token;
    int i = 0;

    // Tokenize the input string
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
    char *args[MAX_ARGS];

    signal(SIGUSR1, handle_exit_signal);  // Example: kill -SIGUSR1 <pid> to exit

    while (1) {

        // Check for any completed background jobs
        for (int j = 0; j < job_count; j++) {
            if (jobs[j].active) {
                int status;
                pid_t result = waitpid(jobs[j].pid, &status, WNOHANG);
                if (result > 0) {
                    jobs[j].active = 0;
                    printf("[Completed] PID %d - %s\n", jobs[j].pid, jobs[j].command);
                }
            }
        }

        if (!batch_mode) {
            printf("my_shell> ");
            fflush(stdout);
        }

        if (fgets(input, MAX_LINE, input_source) == NULL) {
            if (batch_mode) {
                // Batch mode finished — switch to interactive mode
                input_source = stdin;
                batch_mode = 0;
                continue;
            } else {
                printf("\n"); // clean exit
                break;
            }
        }


        if (strchr(input, '\n') == NULL) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF); // flush remainder
        }

        char *commands[MAX_ARGS];
        int command_count = 0;

        // Split input into commands by semicolons
        char *cmd = strtok(input, ";");
        while (cmd != NULL && command_count < MAX_ARGS) {
            commands[command_count++] = strdup(cmd);
            cmd = strtok(NULL, ";");
        }

        // For each command, parse args and execute
        pid_t child_pids[MAX_ARGS];
        int child_index = 0;

        for (int i = 0; i < command_count; i++) {
            // Clean leading/trailing whitespace
            char *line = commands[i];
            while (*line == ' ') line++;
            line[strcspn(line, "\n")] = 0;

            if (batch_mode) {
                printf("Batch>> %s", line);
                fflush(stdout);
            }

            parse_input(line, args);

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

            if (args[0] == NULL) continue;

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

            // Background check
            int background = 0;
            int j = 0;
            while (args[j] != NULL) j++;
            if (j > 0 && strcmp(args[j - 1], "&") == 0) {
                background = 1;
                args[j - 1] = NULL;
            }

            pid_t pid = fork();
            if (pid == 0) {
                if (execvp(args[0], args) == -1) {
                    fprintf(stderr, "Command not found: %s\n", args[0]);
                }
                exit(1);
            } else {
                if (!background) {
                    child_pids[child_index++] = pid;
                } else {
                    printf("Process running in background with PID %d\n", pid);
                    add_job(pid, line);
                }
            }
        }

        // Wait for foreground child processes
        for (int i = 0; i < child_index; i++) {
            waitpid(child_pids[i], NULL, 0);
        }

    }

    return 0;
}