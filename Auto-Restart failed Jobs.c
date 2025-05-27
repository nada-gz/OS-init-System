void run_with_restart(char argv) {
    int pid;
    while (1) {
        if ((pid = fork()) == 0) {
            exec(argv[0], argv);  Run the service
            exit(1);  In case exec fails
        } else {
            wait(0);  Wait for the child to exit, then restart
        }
    }
}
// Background service with restart
if (bg) {
    if ((pid = fork()) == 0) {
        run_with_restart(argv); // Run in restart loop
    } else {
        bg_pids[bg_count++] = pid; // Track PID
    }
}
