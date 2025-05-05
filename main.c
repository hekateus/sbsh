// A simple shell
#include "helpers.h"

int main(int argc, char *argv[]) {
    char *lineptr = NULL;
    size_t size = 0;
    FILE *file_ptr;
    // Path to search for external commands
    char *shell_path = strdup("/bin"); // strdup allocates memory that must be freed
    if (shell_path == NULL) {
        fprintf(stderr, "strdup failed\n");
        exit(1);
    }
    // Too many arguments
    if (argc > 2) {
        fprintf(stderr, "only accepts 0 or 1 arguments\n");
        free(shell_path); 
        exit(1);
    }
    // Batch mode
    if (argc == 2) {
        file_ptr = fopen(argv[1], "r");
        if (file_ptr == NULL) {
            free(shell_path);
            fprintf(stderr, "fopen failed\n");
            exit(1);
        }
    } else {
        file_ptr = stdin;
    }
    Command *commands = NULL;
    size_t num_commands = 0;
    size_t cmd_array_size = 0;
    // Main loop
    while (true) {
        // Read user input
        ssize_t num_read = read_line(file_ptr, &lineptr, &size);
        if (num_read == -1) {
            if (feof(file_ptr)) {
                cleanup(&lineptr, &commands, num_commands, &shell_path);
                if (file_ptr != stdin) {
                    fclose(file_ptr);
                    file_ptr = NULL;
                }
                exit(0);
            } else {
                free(shell_path);
                fprintf(stderr, "read_line failed\n");
                exit(1);
            }
        }
        // Check for new line char
        // Strip newlines
        if (num_read > 0) {
            while (num_read > 0 && (lineptr[num_read - 1] == '\n' || lineptr[num_read - 1] == '\r')) {
                lineptr[--num_read] = '\0';
            }
        }
        num_commands = 0;
        cmd_array_size = 0;
        commands = tokenize_input(lineptr, &num_commands, &cmd_array_size, &shell_path);
        if (commands == NULL || num_commands == 0) {
            cleanup_loop(&lineptr, &commands, num_commands);
            continue;
        }
        pid_t *pids = malloc(num_commands * sizeof(pid_t));
        size_t pid_count = 0;
        if (pids == NULL) {
            fprintf(stderr, "malloc failed\n");
            cleanup_loop(&lineptr, &commands, num_commands);
            continue;
        }
        for (size_t i = 0; i < num_commands; i++) {
            pid_t pid = execute_command(commands[i].tokens, commands[i].token_count, &shell_path, file_ptr);
            if (pid == -2) {
                cleanup(&lineptr, &commands, num_commands, &shell_path);
                free(pids);
                if (file_ptr != stdin) {
                    fclose(file_ptr);
                    file_ptr = NULL;
                }
                exit(0);
            } else if (pid >= 0) {
                pids[pid_count++] = pid;
            }
        }
        for (size_t i = 0; i < pid_count; i++) {
            waitpid(pids[i], NULL, 0);
        }
        free(pids);
        cleanup_loop(&lineptr, &commands, num_commands);
    }
    return 0;
}
