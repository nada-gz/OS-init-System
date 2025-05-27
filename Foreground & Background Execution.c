int split(char *line, char **argv, int *bg) {
    int argc = 0;
    *bg = 0;

    while (*line) {
        while (*line == ' ' || *line == '\t') line++;
        if (*line == '\0' || *line == '\n') break;
        argv[argc++] = line;
        while (*line && *line != ' ' && *line != '\t' && *line != '\n') line++;
        if (*line) *line++ = '\0';
    }

    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        *bg = 1;
        argv[--argc] = 0;
    } else {
        argv[argc] = 0;
    }

    return argc;
}

// Execution block
if (bg) {
    if ((pid = fork()) == 0) {
        exec(argv[0], argv); // Background process
    } else {
        bg_pids[bg_count++] = pid; // Track background PID
    }
} else {
    if ((pid = fork()) == 0) {
        exec(argv[0], argv); // Foreground process
    } else {
        wait(0); // Wait for foreground to finish
    }
}
