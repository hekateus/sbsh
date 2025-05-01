    // $$$ Basic functionality for input mode done $$$
    // $$$ Batch mode done $$$
    // $$$ Redirection done $$$
    
    // TODO: Implement parallel commands

#include <fcntl.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>


ssize_t read_line(FILE *file_ptr, char **lineptr, size_t *size) {
    if (file_ptr == stdin) {
        printf("sbsh> ");
    }
    return getline(lineptr, size, file_ptr);
}

int main(int argc, char *argv[]) {
    
    // Initialize err msg, delims, path, buffer (lineptr), buffer size, input file pointer
    const char error_message[30] = "An error has occurred\n";
    const char *DELIMITER = " ";
    const char *PATH_DELIM = ":";
    char *shell_path = strdup("/bin");
    char *lineptr = NULL;
    size_t size = 0;
    FILE *file_ptr;

    // Too many arguments
    if (argc > 2) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    // Batch mode
    else if (argc == 2) {
        file_ptr = fopen(argv[1], "r");
        if (file_ptr == NULL) {
            free(shell_path);
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    }
    // Interactive mode
    else {
        file_ptr = stdin;
        
    }
    // Main loop
    while (true) {
        // Read user input
        size_t count = 0;
        size_t array_size = 5;
        char **args = (char **) malloc(array_size * sizeof(char*));
        bool memory_freed = false; // Flag to track early freeing
        char *path_copy = NULL;
        if (args == NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        // Exit gracefully if EOF reached (user pressed ctrl-D)
        ssize_t num_read = read_line(file_ptr, &lineptr, &size);;
        if (num_read == -1) {
            if (feof(file_ptr)) {
                free(lineptr);
                free(args);
                free(shell_path);
                if (file_ptr != stdin) {
                    fclose(file_ptr);
                }
                exit(0);
            } else {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
        }
        // Check for new line char
        // Strip newlines
        if (num_read > 0 && lineptr[num_read - 1] == '\n') {
            lineptr[num_read - 1] = '\0';
        }
        // Populate array with tokens derived from separating user-
        // input by spaces as the delimiter
        char *p = lineptr;
        char *start = NULL;
        int token_len = 0;
        while (*p != '\0') {
            while (*p == ' ') p++;
            if (*p == '\0') break;
            start = p;
            token_len = 0;
            while (*p != '\0' && *p != ' ' && *p != '>') {
                p++;
                token_len++;
            }
            if (token_len > 0) {
                if (count == array_size) {
                    array_size *= 2;
                    args = realloc(args, array_size * sizeof(char*));
                    if (args == NULL) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        exit(1);
                    }
                }
                args[count] = strndup(start, token_len);
                count++;
            }
            if (*p == '>') {
                if (count == array_size) {
                    array_size *= 2;
                    args = realloc(args, array_size * sizeof(char*));
                    if (args == NULL) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        exit(1);
                    }
                }
                args[count] = strdup(">");
                count++;
                p++;
            }
        }
        if (count < array_size) {
            args[count] = NULL;
        } else {
            array_size++;
            args = realloc(args, array_size * sizeof(char*));
            if (args == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
            args[count] = NULL;
        }

        // If user presses enter without typing anything
        if (count == 0) { 
            memory_freed = true;
        } else if (count == 1 && (strcmp(args[0], "exit") == 0)) { // Check for builtins commands
            free(shell_path);
            memory_freed = true;
            if (file_ptr != stdin) {
                fclose(file_ptr);
            }
            exit(0);
        } else if (strcmp(args[0], "cd") == 0) {
            if (count != 2) { // cd takes only one argument for now
                write(STDERR_FILENO, error_message, strlen(error_message));
            } else {
                const char *dir = args[1];
                if (dir == NULL || chdir(dir) < 0) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                }
            }
            memory_freed = true;
        } else if (strcmp(args[0], "path") == 0) {
            free(shell_path);
            if (count - 1 == 0) {
                shell_path = strdup("");
                if (shell_path == NULL) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
            }  else {
                int path_size = count - 1; // Initialize as count - 2 + 1 for colons and null terminator (equivalent used)
                for (int i = 1; i < count; i++) {
                    path_size += strlen(args[i]); // Sum of length of args
                }
                char *new_path = malloc(path_size);
                if (new_path == NULL) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
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
            memory_freed = true;
        } else {
            int redir_index = -1;
            int ch_count = 0;
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], ">") == 0) {
                    if (redir_index != -1) {
                        write(STDERR_FILENO, error_message, strlen(error_message));
                        exit(0);
                    }
                        redir_index = i;
                }
                    ch_count++;
            }
            if (redir_index != -1 && redir_index == ch_count - 1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                continue;
            }
            char *cmd_args[ch_count];
            char *file = NULL;
            int i;
            if (redir_index != -1) {
                if (args[redir_index + 1] == NULL || args[redir_index + 2] != NULL) {
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    continue;
                }
                for (i = 0; i < redir_index; i++) {
                    cmd_args[i] = args[i];
                }
                cmd_args[redir_index] = NULL;
                file = args[redir_index + 1];
            } else {
                for (i = 0; args[i] != NULL; i++) {
                    cmd_args[i] = args[i];
                }
                cmd_args[i] = NULL;
            }

            // Split path string copy by colons
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
                        if (file != NULL) {
                            int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                            if (fd < 0) {
                                write(STDERR_FILENO, error_message, strlen(error_message));
                                exit(1);
                            }
                            dup2(fd, 1);
                            dup2(fd, 2);
                            close(fd);
                        }
                        if (execv(full_path, cmd_args) < 0) {
                            write(STDERR_FILENO, error_message, strlen(error_message));
                            exit(1);
                        }
                    } else {      // Parent process
                        waitpid(cpid, NULL, 0);
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
        // If memory has not been freed by now, free it
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
    }
    free(lineptr);
    free(shell_path);
    if (file_ptr != stdin) {
        fclose(file_ptr);
    }
    return 0;
}
