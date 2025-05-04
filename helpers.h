#ifndef HELPERS_H
#define HELPERS_H

#include <ctype.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Struct for storing commands
typedef struct {
    char **tokens;
    size_t token_count;
} Command;

void cleanup_loop(char **lineptr, Command **commands, size_t num_commands);
void cleanup(char **lineptr, Command **commands, size_t num_commands, char **shell_path);
ssize_t read_line(FILE *file_ptr, char **lineptr, size_t *size);
int resize_args(char ***args, size_t *array_size, size_t count);
int resize_cmd_arr(Command **cmd, size_t *cmd_array_size, size_t count);
char **tokenize_command(char *cmd_str, size_t *token_count, size_t *token_array_size);
Command *tokenize_input(char *lineptr, size_t *num_commands, size_t *cmd_array_size, char **shell_path);
pid_t execute_command(char **args, size_t count, char **shell_path, FILE *file_ptr);


#endif
