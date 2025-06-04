
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXLINE 128
#define MAXARGS 8
#define MAX_BG 64
#define CONSOLE 1

void split(char *line, char **argv, int *bg) {
  *bg = 0;
  while (*line) {
    while (*line == ' ') *line++ = 0;
    if (*line) *argv++ = line;
    while (*line && *line != ' ') {
      if (*line == '&') {
        *bg = 1;
        *line = 0;
      }
      line++;
    }
  }
  *argv = 0;
}

void run_with_restart(char **argv) {
  int pid, wpid, status;

  while (1) {
    pid = fork();
    if (pid < 0) {
      printf("init: fork failed for %s\n", argv[0]);
      return;
    }
    if (pid == 0) {
      exec(argv[0], argv);
      printf("init: exec %s failed\n", argv[0]);
      exit(1);
    }

    wpid = wait(&status);
    printf("init: background service %s (pid %d) exited with status %d, restarting...\n", argv[0], wpid, status);
  }
}

int remove_pid(int *bg_pids, int *bg_count, int pid) {
  for (int i = 0; i < *bg_count; i++) {
    if (bg_pids[i] == pid) {
      // Shift remaining pids left
      for (int j = i; j < *bg_count - 1; j++) {
        bg_pids[j] = bg_pids[j + 1];
      }
      (*bg_count)--;
      return 1;  // removed
    }
  }
  return 0;  // not found
}

int main(void) {
  int fd;

  if (open("console", O_RDWR) < 0) {
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);
  dup(0);

  fd = open("init.conf", O_RDONLY);
  if (fd < 0) {
    printf("init: could not open init.conf\n");
    exit(1);
  }

  char buf[MAXLINE];
  char *argv[MAXARGS];
  int i = 0, n;

  int fg_count = 0;
  int bg_pids[MAX_BG];
  int bg_count = 0;

  // Read commands line by line
  while ((n = read(fd, &buf[i], 1)) == 1) {
    if (buf[i] == '\n' || i == MAXLINE - 1) {
      buf[i] = '\0';

      if (buf[0] != '\0') {
        int bg = 0;
        split(buf, argv, &bg);

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
        } else if (argv[0]) {
          if (bg) {
            int pid = fork();
            if (pid < 0) {
              printf("init: fork failed for %s\n", argv[0]);
            } else if (pid == 0) {
              run_with_restart(argv);
              exit(0);  // never reached
            } else {
              if (bg_count < MAX_BG) {
                bg_pids[bg_count++] = pid;
              }
              printf("init: started background service %s with restart (pid %d)\n", argv[0], pid);
              
            }
          } else {
            int pid = fork();
            if (pid < 0) {
              printf("init: fork failed for %s\n", argv[0]);
            } else if (pid == 0) {
              exec(argv[0], argv);
              printf("init: exec %s failed\n", argv[0]);
              exit(1);
            } else {
              printf("init: started foreground service %s (pid %d)\n", argv[0], pid);
              fg_count++;
            }
          }
        }
      }
      i = 0;
    } else {
      i++;
    }
  }

  close(fd);

  // Wait for all foreground children to finish
  int fg_remaining = fg_count;
  while (fg_remaining > 0) {
    int status;
    int wpid = wait(&status);
    if (wpid > 0) {
      printf("init: foreground process %d exited with status %d\n", wpid, status);
      fg_remaining--;
    }
  }
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
    } else if (wpid > 0) {
    
    }
  }

  return 0;
}
