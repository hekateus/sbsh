## Super basic shell

Only runs on linux for now.

To compile: gcc -Wall main.c helpers.c text_colors.c -o ./sbsh

Work in progress.

### Built-ins
 - cd - changes directory (usage: cd [directory], e.g. 'cd bin')
 - path - updates path with directories listed by user (usage: path [directory] [[directory]... ], e.g. 'path /bin /sbin')
 - exit
