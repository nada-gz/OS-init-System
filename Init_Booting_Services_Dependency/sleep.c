/*#include "kernel/types.h"
#include "user/user.h"

int main()
{
  int buf[2];
  
  while(1){
    int time = read(0,buf,sizeof(buf));
    
    sleep(time);
    break;
  
  }
  exit(0);
}*/
/*
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  char buf[16];
  int n = read(0, buf, sizeof(buf)-1);
  if(n > 0){
    buf[n] = 0;
    int ticks = atoi(buf);
    sleep(ticks);
  }
  exit(0);
}
*/
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if(argc < 2){
    fprintf(2, "Usage: sleep <ticks>\n");
    exit(1);
  }
  int ticks = atoi(argv[1]);
  sleep(ticks);
  exit(0);
}
