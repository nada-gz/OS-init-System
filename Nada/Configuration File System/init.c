#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  // Run commands from init.conf before starting shell
  int fd = open("init.conf", O_RDONLY);
  if (fd >= 0) {
    char buf[512];
    int n = read(fd, buf, sizeof(buf)-1);
    if (n > 0) {
      buf[n] = '\0';
      char *line = buf;
      char *next;

      while (line && *line) {
        next = strchr(line, '\n');
        if (next) {
          *next = 0;
        }

        if (strlen(line) > 0) {
          pid = fork();
          if (pid < 0) {
            printf("init: fork failed\n");
            exit(1);
          }
          if (pid == 0) {
            char *argv_cmd[8];
            int i = 0;
            char *token = line;

            while (*token && i < 7) {
                while (*token == ' ')
                    token++;
                if (*token == 0)
                    break;
                argv_cmd[i++] = token;
                while (*token && *token != ' ')
                    token++;
                if (*token) {
                    *token = 0;
                    token++;
                }
            }
            argv_cmd[i] = 0;

            printf("init: exec %s\n", argv_cmd[0]);
            exec(argv_cmd[0], argv_cmd);
            printf("init: exec %s failed\n", argv_cmd[0]);
            exit(1);
          }
          else {
            wait((int *) 0);
          }
        }

        if (next) {
          line = next + 1;
        } else {
          break;
        }
      }
    }
    close(fd);

    while ((wpid = wait(0)) > 0)
      ;
    }

  for(;;) {
    printf("init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }
    for(;;){
        wpid = wait((int *) 0);
        if(wpid == pid){
            break;
        } else if(wpid < 0){
            printf("init: wait returned an error\n");
            exit(1);
        }
    }
  }
}
