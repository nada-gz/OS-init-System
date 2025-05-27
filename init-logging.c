// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h" 
#include "kernel/fs.h"   
#include "user/user.h"   
#include "kernel/fcntl.h" 
   

char *argv[] = { "sh", 0 };

int log_fd = -1;

void initlog(char *msg) {
    if (log_fd < 0) {
        log_fd = open("init.log", O_CREATE | O_WRONLY | O_APPEND);
        if (log_fd < 0) {
            printf("init: cannot open log file\n");
            return;
        }
    }
    char buf[512];
    int n;
    
    safestrcpy(buf, "[init] ", sizeof("[init] ")); 
    safestrcpy(buf + strlen(buf), msg, sizeof(buf) - strlen(buf));
    safestrcpy(buf + strlen(buf), "\n", sizeof(buf) - strlen(buf));

    
    if (n > 0) { 
        if (write(log_fd, buf, n) != n) { 
            printf("init: log write error during message: %s\n", msg);
        }
    }
}


void init_logging_setup() {

    log_fd = open("init.log", O_CREATE | O_WRONLY | O_APPEND);

    if (log_fd < 0) {
        printf("init: failed to initialize logging system\n");
    } else {

        initlog("Logging system initialized.");
    }
}



int
main(void)
{
  int pid, wpid;
  
  init_logging_setup();

  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    printf("init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      
      close(1); 
      close(2); 

      int service_log_fd = open("init.log", O_CREATE | O_WRONLY | O_APPEND);

      if (service_log_fd < 0) {
        exit(1);
      }

      if (dup(service_log_fd) != 1) {
        exit(1); 
      }

      if (dup(service_log_fd) != 2) {
        exit(1); 
      }

      close(service_log_fd);
      
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *) 0);
      if(wpid == pid){
        // the shell exited; restart it.
        break;
      } else if(wpid < 0){
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // it was a parentless process; do nothing.
      }
    }
  }
}






