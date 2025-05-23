#include "helpers.h"
#include "text_colors.h"

// Interactive mode prompt
// TODO: add a little bit of customization
void prompt() {
    cyan_txt();
    printf("sbsh::$ ");
    reset_txt();
}

// Frees memory for main loop (excluding shell_path)
void cleanup_loop(char **lineptr, Command **commands, size_t num_commands) {
    free(*lineptr);
    *lineptr = NULL;
    if (*commands != NULL) {
        for (size_t i = 0; i < num_commands; i++) {
            for (size_t j = 0; j < (*commands)[i].token_count; j++) {
                free((*commands)[i].tokens[j]);
            }
            free((*commands)[i].tokens);
        }
        free(*commands);
        *commands = NULL;
    }
}

// Frees all memory
void cleanup(char **lineptr, Command **commands, size_t num_commands, char **shell_path) {
    cleanup_loop(lineptr, commands, num_commands);
    free(*shell_path);
    *shell_path = NULL;
}

// Gets user input
ssize_t read_line(FILE *file_ptr, char **lineptr, size_t *size) {
    if (file_ptr == stdin) {
        prompt();
    }
    return getline(lineptr, size, file_ptr);
}

// Dynamically resizes array
int resize_args(char ***args, size_t *array_size, size_t count) {
    if (*array_size == 0) {
        *array_size = 5;
    }
    size_t new_size = *array_size * 2;
    char **new_args = realloc(*args, new_size * sizeof(char*));
    if (new_args == NULL) {
        for (size_t i = 0; i < count; i++) {
            free((*args)[i]);
        }
        free(*args);
        *args = NULL;
        return -1;
    }
    *args = new_args;
    *array_size = new_size;
    return 0;
}

// Resize array of command structs
int resize_cmd_arr(Command **cmd, size_t *cmd_array_size, size_t count) {
    if (*cmd_array_size == 0) {
        *cmd_array_size = 4;
    }
    size_t new_size = *cmd_array_size * 2;
    Command *new_commands = realloc(*cmd, new_size * sizeof(Command));
    if (new_commands == NULL) {
        for (size_t i = 0; i < count; i++) {
            for (size_t j = 0; j < (*cmd)[i].token_count; j++) {
                free((*cmd)[i].tokens[j]);
            }
            free((*cmd)[i].tokens);
        }
        free(*cmd);
        *cmd = NULL;
        return -1;
    }
    *cmd = new_commands;
    *cmd_array_size = new_size;
    return 0;
}

// Separate input into tokens
// derived from separating user-
// input by spaces or redirection indicator (>) as the delimiter
char **tokenize_command(char *cmd_str, size_t *token_count, size_t *token_array_size) {
    if (*token_array_size == 0) {
        *token_array_size = 5;
    }
    char **tokens = malloc(*token_array_size * sizeof(char *));
    if (tokens == NULL) {
        fprintf(stderr, "malloc failed\n");
        return NULL;
    }
    char *p = cmd_str;
    *token_count = 0;
    while (*p != '\0') {
        while (isspace(*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        char *start = p;
        size_t token_len = 0;

        if (*p == '>') {
            token_len = 1;
            p++;
        } else {
            while (*p != '\0' && !isspace(*p) && *p != '>') {
                p++;
                token_len++;
            }
        }
        if (token_len > 0) {
            if (*token_count >= *token_array_size) {
                if (resize_args(&tokens, token_array_size, *token_count) != 0) {
                    for (size_t i = 0; i < *token_count; i++) {
                        free(tokens[i]);
                    }
                    free(tokens);
                    fprintf(stderr, "realloc failed\n");
                    return NULL;
                }
            }
            tokens[*token_count] = strndup(start, token_len);
            if (tokens[*token_count] == NULL) {
                for (size_t i = 0; i < *token_count; i++) {
                    free(tokens[i]);
                }
                free(tokens);
                fprintf(stderr, "strndup failed\n");
                return NULL;
            }
            (*token_count)++;
        }
        if (*p == '>') {
            if (*token_count >= *token_array_size) {
                if (resize_args(&tokens, token_array_size, *token_count) != 0) {
                    for (size_t i = 0; i < *token_count; i++) {
                        free(tokens[i]);
                    }
                    free(tokens);
                    fprintf(stderr, "realloc failed\n");
                    return NULL;
                }
            }
            tokens[*token_count] = strdup(">");
            if (tokens[*token_count] == NULL) {
                for (size_t i = 0; i < *token_count; i++) {
                    free(tokens[i]);
                }
                free(tokens);
                fprintf(stderr, "strdup failed\n");
                return NULL;
            }
            (*token_count)++;
            p++;
        }
    }
    if (*token_count >= *token_array_size) {
        if (resize_args(&tokens, token_array_size, *token_count) != 0) {
            for (size_t i = 0; i < *token_count; i++) {
                free(tokens[i]);
            }
            free(tokens);
            fprintf(stderr, "realloc failed\n");
            return NULL;
        }
    }
    tokens[*token_count] = NULL;
    return tokens;
}

// Populate Command struct array with tokens
Command *tokenize_input(char *lineptr, size_t *num_commands, size_t *cmd_array_size, char **shell_path) {
    if (*cmd_array_size == 0) {
        *cmd_array_size = 4;
    }
    Command *commands = malloc(*cmd_array_size * sizeof(Command));
    if (commands == NULL) {
        fprintf(stderr, "malloc failed\n");
        return NULL;
    }
    char *p = lineptr;
    char *start = NULL;
    *num_commands = 0;
    while (*p != '\0') {
        while (isspace(*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != '&') {
            p++;
        }
        if (p > start) {
            char *end = p;
            while (end > start && isspace(*(end - 1))) {
                end--;
            }
            if (end == start) {
                if (*p == '&') p++;
                continue;
            }
            char *cmd_str = strndup(start, end - start);
            if (cmd_str == NULL) {
                cleanup_loop(&lineptr, &commands, *num_commands);
                fprintf(stderr, "strndup failed\n");
                return NULL;
            }
            size_t token_count = 0;
            size_t token_array_size = 0;
            char **tokens = tokenize_command(cmd_str, &token_count, &token_array_size);
            if (tokens == NULL) {
                cleanup_loop(&lineptr, &commands, *num_commands);
                return NULL;
            }
            if (token_count == 0) {
                free(cmd_str);
                for (size_t i = 0; i < token_count; i++) {
                    free(tokens[i]);
                }
                free(tokens);
                if (*p == '&') p++;
                continue;
            }
            if (*num_commands == *cmd_array_size) {
                if (resize_cmd_arr(&commands, cmd_array_size, *num_commands) != 0) {
                    cleanup_loop(&lineptr, &commands, *num_commands);
                    fprintf(stderr, "realloc failed\n");
                    return NULL;
                }
            }
            commands[*num_commands].tokens = tokens;
            commands[*num_commands].token_count = token_count;
            (*num_commands)++;
            free(cmd_str);
        }
        if (*p == '&') p++;
    }
    if (*num_commands < *cmd_array_size) {
        commands[*num_commands].tokens = NULL;
        commands[*num_commands].token_count = 0;
    } else {
        if (resize_cmd_arr(&commands, cmd_array_size, *num_commands) != 0) {
            cleanup_loop(&lineptr, &commands, *num_commands);
            fprintf(stderr, "realloc failed\n");
            return NULL;
        }
        commands[*num_commands].tokens = NULL;
        commands[*num_commands].token_count = 0;
    }
    return commands;
}

// Parse command and execute
pid_t execute_command(char **args, size_t count, char **shell_path, FILE *file_ptr) {
    if (count == 0) {   // If user presses enter without typing anything
        return -1;
    }
    if (strcmp(args[0], "exit") == 0) { // Check for builtin commands
        if (count != 1) {
            fprintf(stderr, "bad exit command\n");
            return -1;
        }
        return -2;
    }
    if (strcmp(args[0], "cd") == 0) {
        if (count != 2) { // cd takes only one argument for now
            fprintf(stderr,  "specify a directory for cd\n");
        } else {
            const char *dir = args[1];
            if (dir == NULL || chdir(dir) < 0) {
                fprintf(stderr, "directory does not exist\n");
            }
        }
        return -1;
    }
    if (strcmp(args[0], "path") == 0) {   // User wants to change shell path
        free(*shell_path); // free initial path
        *shell_path = NULL;
        if (count == 1) {
            *shell_path = strdup("");
            if (*shell_path == NULL) {
                fprintf(stderr, "strdup failed\n");
                return -1;
            }
        }  else {
            size_t path_size = count - 1; // Initialize as count - 2 + 1 for colons and null terminator (equivalent used)
            for (size_t i = 1; i < count; i++) {
                path_size += strlen(args[i]); // Sum of length of args
            }
            char *new_path = malloc(path_size);
            if (new_path == NULL) {
                fprintf(stderr, "malloc failed\n");
                return -1;
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
        return -1;
    }
    // Check for redirection
    int redir_index = -1;
    int ch_count = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            if (redir_index != -1) {
                fprintf(stderr, "redirection failed\n");
                return -1;
            }
            redir_index = i;
        }
        ch_count++;
    }
    char *cmd_args[ch_count + 1];
    char *file = NULL;
    if (redir_index != -1) {
        if (
            redir_index == ch_count - 1 
            || args[redir_index + 1] == NULL 
            || args[redir_index + 2] != NULL
            || strlen(args[redir_index + 1]) == 0
        ) {
            fprintf(stderr, "bad redirection\n");
            return -1;
        }
        for (int i = 0; i < redir_index; i++) {
            cmd_args[i] = args[i];
        }
        cmd_args[redir_index] = NULL;
        // file get set here
        file = args[redir_index + 1];
    } else {
        for (int i = 0; i < ch_count; i++) {
            cmd_args[i] = args[i];
        }
        cmd_args[ch_count] = NULL;
    }
    if (!*shell_path || **shell_path == '\0') {
        fprintf(stderr, "bad path\n");
        return -1;
    }
    // Split path string copy by colons
    char *path_copy = NULL;
    path_copy = strdup(*shell_path);
    if (path_copy == NULL) {
        fprintf(stderr, "strdup failed\n");
        return -1;
    }
    bool path_found = false;
    char *dir = strtok(path_copy, ":");
    while (dir != NULL) {
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, args[0]);
        // If command is found in path and is executable
        if (access(full_path, X_OK) == 0) {
            // Fork process
            pid_t cpid = fork();
            if (cpid < 0) {
                free(path_copy);
                path_copy = NULL;
                fprintf(stderr, "fork failed\n");
                return -1;
            } else if (cpid == 0 ) {     // Execute command in child process 
                if (file != NULL) {
                    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) {
                        fprintf(stderr, "failed to open file\n");
                        exit(1);
                    }
                    // Close stdout (1) and print output to fd (file)
                    // then close fd
                    dup2(fd, 1);
                    close(fd);
                }
                if (execv(full_path, cmd_args) < 0) {
                    fprintf(stderr, "execv failed\n");
                    exit(1);
                }
            } else {      // Parent process
                path_found = true;
                free(path_copy);
                path_copy = NULL;
                return cpid;
            }
        }
        dir = strtok(NULL, ":");
    }
    free(path_copy);
    path_copy = NULL;
    if (!path_found) {
        fprintf(stderr, "command does not exist\n");
    }
    return -1;
}
