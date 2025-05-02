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

// Initialize delims, path, buffer (lineptr), buffer size, input file pointer
const char error_message[30] = "An error has occurred\n";
const char *PATH_DELIM = ":";
char *lineptr = NULL;
size_t size = 0;
FILE *file_ptr;
bool memory_freed = false; // Flag to track early freeing
char *path_copy = NULL;

// Gets user input
ssize_t read_line(FILE *file_ptr, char **lineptr, size_t *size) {
    if (file_ptr == stdin) {
        // Interactive mode prompt
        printf("sbsh> ");
    }
    return getline(lineptr, size, file_ptr);
}

// Dynamically resizes array
int resize_args(char ***args, size_t *array_size, size_t count) {
    if (*array_size == 0) {
        *array_size = 1;
    }
    size_t new_size = *array_size * 2;
    char **new_args = realloc(*args, new_size * sizeof(char*));
    if (new_args == NULL) {
        for (size_t i = 0; i < count; i++) {
            free((*args)[i]);
        }
        free(*args);
        *args = NULL;
        write(STDERR_FILENO, error_message, strlen(error_message));
        return -1;
    }
    *args = new_args;
    *array_size = new_size;
    return 0;
}

// Populate array with tokens derived from separating user-
// input by spaces or redirection indicator (>) as the delimiter
char **tokenize_input(char *lineptr, size_t *count, size_t *array_size) {
    if (*array_size == 0) {
        *array_size = 5;
    }
    char **args = malloc(*array_size * sizeof(char*));
    if (args == NULL) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        return NULL;
    }
    char *p = lineptr;
    char *start = NULL;
    int token_len = 0;
    *count = 0;
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
            if (*count == *array_size) {
                if (resize_args(&args, array_size, *count) != 0) {
                    return NULL;
                }
            }
            args[*count] = strndup(start, token_len);
            if (args[*count] == NULL) {
                for (size_t i = 0; i < *count; i++) {
                    free(args[i]);
                }
                free(args);
                write(STDERR_FILENO, error_message, strlen(error_message));
                return NULL;
            }
            (*count)++;
        }
        if (*p == '>') {
            if (*count == *array_size) {
                if (resize_args(&args, array_size, *count) != 0) {
                    return NULL;
                }
            }
            args[*count] = strdup(">");
            if (args[*count] == NULL) {
                for (size_t i = 0; i < *count; i++) {
                    free(args[i]);
                }
                free(args);
                write(STDERR_FILENO, error_message, strlen(error_message));
                return NULL;
            }
            (*count)++;
            p++;
        }
    }
    if (*count >= *array_size) {
        if (resize_args(&args, array_size, *count) != 0) {
            return NULL;
        }
    }
    args[*count] = NULL;
    return args;
}

// Free memory
void cleanup(char **lineptr, char ***args, size_t count, char **path_copy) {
    free(*lineptr);
    *lineptr = NULL;
    if (*args != NULL) {
        for (size_t i = 0; i < count; i++) {
            free((*args)[i]);
            (*args)[i] = NULL;
        }
        free(*args);
        *args = NULL;
    }
    free(*path_copy);
    *path_copy = NULL;
}

// Parse command and execute
bool execute_command(char **args, size_t count, char **shell_path, FILE *file_ptr) {
    if (count == 0) {   // If user presses enter without typing anything
        memory_freed = true;
        return false;
    }
    if (count == 1 && strcmp(args[0], "exit") == 0) { // Check for builtins commands
        free(*shell_path);
        *shell_path = NULL;
        memory_freed = true;
        if (file_ptr != stdin) {
            fclose(file_ptr);
            file_ptr = NULL;
        }
        return true;
    }
    if (strcmp(args[0], "cd") == 0) {
        if (count != 2) { // cd takes only one argument for now
            write(STDERR_FILENO, error_message, strlen(error_message));
        } else {
            const char *dir = args[1];
            if (dir == NULL || chdir(dir) < 0) {
                write(STDERR_FILENO, error_message, strlen(error_message));
            }
        }
        memory_freed = true;
        return false;
    }
    if (strcmp(args[0], "path") == 0) {   // User wants to change shell path
        free(*shell_path); // free initial path
        if (count == 1) {
            *shell_path = strdup("");
            if (*shell_path == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
        }  else {
            size_t path_size = count - 1; // Initialize as count - 2 + 1 for colons and null terminator (equivalent used)
            for (size_t i = 1; i < count; i++) {
                path_size += strlen(args[i]); // Sum of length of args
            }
            char *new_path = malloc(path_size);
            if (new_path == NULL) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                exit(1);
            }
            char *path_ptr = new_path;
            for (int i = 1; i < count; i++) {
                snprintf(path_ptr, path_size - (path_ptr - new_path), "%s", args[i]);
                path_ptr += strlen(args[i]);
                if (i < count - 1) {
                    *path_ptr++ = ':';
                }
            }
            // Null terminate the path string
            *path_ptr = '\0';
            *shell_path = new_path;
        }
        memory_freed = true;
        return false;
    }
    // Check for redirection
    int redir_index = -1;
    int ch_count = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (redir_index != -1) {
                write(STDERR_FILENO, error_message, strlen(error_message));
                memory_freed = true;
                return false;
            }
            redir_index = i;
        }
        ch_count++;
    }
    char *cmd_args[ch_count + 1];
    char *file = NULL;
    if (redir_index != -1) {
        if (redir_index == ch_count - 1 || args[redir_index + 1] == NULL || args[redir_index + 2] != NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            memory_freed = true;
            return false;
        }
        for (int i = 0; i < redir_index; i++) {
            cmd_args[i] = args[i];
        }
        cmd_args[redir_index] = NULL;
        file = args[redir_index + 1];
    } else {
        for (int i = 0; i < ch_count; i++) {
            cmd_args[i] = args[i];
        }
        cmd_args[ch_count] = NULL;
    }

    if (!*shell_path || **shell_path == '\0') {
        write(STDERR_FILENO, error_message, strlen(error_message));
        memory_freed = true;
        return false;
    }

    // Split path string copy by colons
    path_copy = strdup(*shell_path);
    if (path_copy == NULL) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        memory_freed = true;
        return false;
    }
    bool path_found = false;
    char *dir = strtok(path_copy, PATH_DELIM);
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
        dir = strtok(path_copy, PATH_DELIM);
    }
    free(path_copy);
    path_copy = NULL;
    if (!path_found) {
        write(STDERR_FILENO, error_message, strlen(error_message));
    }
    return false;
}


int main(int argc, char *argv[]) {
    char *shell_path = strdup("/bin");
    if (shell_path == NULL) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    // Too many arguments
    if (argc > 2) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }
    // Batch mode
    if (argc == 2) {
        file_ptr = fopen(argv[1], "r");
        if (file_ptr == NULL) {
            free(shell_path);
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
    } else {
        file_ptr = stdin;
    }

    char **args = NULL;
    size_t count = 0;
    size_t array_size = 0;
    // Main loop
    while (true) {
        memory_freed = false;
        // Read user input
        ssize_t num_read = read_line(file_ptr, &lineptr, &size);;
        if (num_read == -1) {
            if (feof(file_ptr)) {
                cleanup(&lineptr, &args, count, &path_copy);
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
        count = 0;
        array_size = 5;
        args = tokenize_input(lineptr, &count, &array_size);
        if (args == NULL) {
            cleanup(&lineptr, &args, count, &path_copy);
            continue;
        }
        if (execute_command(args, count, &shell_path, file_ptr)) {
            cleanup(&lineptr, &args, count, &path_copy);
            exit(0);
        }
        cleanup(&lineptr, &args, count, &path_copy);
    }
    return 0;
}
