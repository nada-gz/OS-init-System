printf("init: launching fallback shell\n");
// Fallback shell loop
while (1) {
  int pid = fork();
  if (pid == 0) {
    char *sh_argv[] = {"sh", 0};
    exec("sh", sh_argv);
    printf("init: exec sh failed\n");
    exit(1);
  }

  int status;
  int wpid = wait(&status);
  if (wpid == pid) {
    printf("init: fallback shell (pid %d) exited with status %d\n", wpid, status);
  }
}
