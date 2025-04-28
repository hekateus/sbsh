#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    // $$$ Basic functionality for input mode done $$$
    
    // TODO: Implement batch mode
    // TODO: Implement redirection
    // TODO: Implement parallel commands

    if (argc == 1) {
        // Initialize prompt, path, lineptr, buffer size, error msg, delimiters
        char *prompt = "sbsh> ";
        char *shell_path = strdup("/bin");
        const char error_message[30] = "An error has occurred\n";
        const char *DELIMITER = " ";
        const char *PATH_DELIM = ":";
        char *lineptr = NULL;
        size_t size = 0;


        // Main loop
        while (true) {
            printf("%s", prompt);

            // Read user input
            ssize_t num_read = getline(&lineptr, &size, stdin);
            size_t count = 0;
            size_t array_size = 5;
            char **args = (char **) malloc(array_size * sizeof(char*));
            bool memory_freed = false; // Flag to track early freeing
            char *path_copy = NULL;
            if (args == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
            if (num_read == -1) {
                if (feof(stdin)) {
                    free(lineptr);
                    free(args);
                    free(shell_path);
                    exit(0);
                } else {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
            } else {
                // Check for new line char
                // Strip newlines
                if (num_read > 0 && lineptr[num_read - 1] == '\n') {
                    lineptr[num_read - 1] = '\0';
                }
                // Populate array with tokens derived from separating user-
                // input by spaces as the delimiter
                char *input = lineptr;
                char *token; 
                while ((token = strsep(&input, DELIMITER)) != NULL && *token != '\0') {
                    // Check if array needs to be resized
                    if (count == array_size) {
                        array_size *= 2;
                        args = realloc(args, array_size * sizeof(char*));
                        if (args == NULL) {
                            write(STDERR_FILENO, error_message, strlen(error_message));
                            exit(1);
                        }
                    }
                    // Parse user input into separate tokens
                    if (count < array_size) {
                        args[count] = strdup(token);
                        count++;
                    }
                }
                // Set indicator for end of args
                args[count] = NULL;
                // If user presses enter without typing anything
                if (count == 0) {
                    memory_freed = true;
                } else if (count == 1 && (strcmp(args[0], "exit") == 0)) { // Check for builtins commands
                    free(shell_path);
                    memory_freed = true;
                    exit(0);
                } else if (count >= 1 && strcmp(args[0], "cd") == 0) {
                    if (count > 2) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                    } else {
                        const char *dir = count == 1 ? getenv("HOME") : args[1];
                        if (dir == NULL || chdir(dir) < 0) {
                            write(STDERR_FILENO, error_message, strlen(error_message));
                        }
                    }
                    free(lineptr);
                    lineptr = NULL;
                    for (int i = 0; i < count; i++) {
                        free(args[i]);
                        args[i] = NULL;
                    }
                    free(args);
                         args = NULL;
                    count = 0;
                    memory_freed = true;
                } else if (strcmp(args[0], "path") == 0) {
                    free(shell_path);
                    if (count - 1 == 0) {
                        shell_path = strdup("");
                        if (shell_path == NULL) {
                            perror("strdup");
                            exit(1);
                        }
                    }  else {
                        int path_size = count - 1; // Initialize as count - 2 + 1 for colons and null terminator (equivalent used)
                        for (int i = 1; i < count; i++) {
                            path_size += strlen(args[i]); // Sum of length of args
                        }
                        char *new_path = malloc(path_size);
                        if (new_path == NULL) {
                            perror("malloc");
                            exit(1);
                        }
                        char *path_ptr = new_path;
                        for (int i = 1; i < count; i++) {
                            snprintf(path_ptr, new_path + path_size - path_ptr, "%s", args[i]);
                            path_ptr += strlen(args[i]);
                            if (i < count - 1) {
                                *path_ptr++ = ':';
                            }
                        }
                        // Null terminate the path string
                        *path_ptr = '\0';
                        shell_path = new_path;
                    }
                    free(lineptr);
                    lineptr = NULL;
                    for (int i = 0; i < count; i++) {
                        free(args[i]);
                        args[i] = NULL;
                    }
                    free(args);
                    count = 0;
                    memory_freed = true;
                } else {
                    path_copy = strdup(shell_path);
                    char *dir = strsep(&path_copy, PATH_DELIM);
                    bool path_found = false;
                    while (dir != NULL) {
                        char full_path[PATH_MAX];
                        snprintf(full_path, sizeof(full_path), "%s/%s", dir, args[0]);
                        // If command is found in path and is executable
                        if (access(full_path, X_OK) == 0) {
                            // Fork process
                            pid_t cpid = fork();
                            if (cpid < 0) {
                                write(STDERR_FILENO, error_message, strlen(error_message));
                            } else if (cpid == 0 ) {     // Child process 
                                if (execv(full_path, args) < 0) {
                                    write(STDERR_FILENO, error_message, strlen(error_message));
                                    exit(1);
                                }
                            } else {      // Parent process
                                int cpid_wait = waitpid(cpid, NULL, 0);
                                path_found = true;
                                break;
                            }
                        }
                        dir = strsep(&path_copy, PATH_DELIM);
                    }
                    if (!path_found) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                    }
                }
                if (!memory_freed) {
                    free(lineptr);
                    lineptr = NULL;
                    for (int i = 0; i < count; i++) {
                        free(args[i]);
                        args[i] = NULL;
                    }
                    free(args);
                    args = NULL;
                    free(path_copy);
                    path_copy = NULL;
                }
                count = 0;
            }
        }
    }
    return 0;
}
