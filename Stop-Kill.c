// Support kill command: "kill PID"
if (argv[0] && strcmp(argv[0], "kill") == 0 && argv[1]) {
  int kpid = atoi(argv[1]);
  if (remove_pid(bg_pids, &bg_count, kpid)) {
    if (kill(kpid) < 0)
      printf("init: failed to kill pid %d\n", kpid);
    else
      printf("init: killed pid %d\n", kpid);
  } else {
    printf("init: pid %d not found in background jobs\n", kpid);
  }
}
