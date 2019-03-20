#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

const int NUM_CMD = 5;

// declare commands
const char* ls[] = { "ls", "-l", NULL };
const char* awk[] = { "awk", "{print $1}", NULL };
const char* sort[] = { "sort", NULL };
const char* uniq[] = { "uniq", NULL };
const char* sort2[] = { "sort", "--reverse", NULL};

const char** cmd[] = {ls, awk, sort, uniq, sort2, NULL};

int main(){

  // declare pipes
  int p[2][2];
  pipe(p[1]);

  int other_pipe = 1;
  int cur_pipe = 0;

  // declare pids
  int pids[NUM_CMD-1];
  int status;

  int i = 0;
  while(cmd[i] != NULL){

    // create new pipe
    //p[cur_pipe][0] = 0;
    //p[cur_pipe][1] = 0;
    pipe(p[cur_pipe]);
    printf("creating pipe %d\n", cur_pipe);

    // print the process info
    int j = 0;
    while(cmd[i][j] != NULL){
      printf("%s ", cmd[i][j]);
      j++;
    }
    printf("\n");

    printf("cur_pipe: [%d, %d]\n", p[cur_pipe][0], p[cur_pipe][1]);
    printf("other_pipe: [%d, %d]\n", p[other_pipe][0], p[other_pipe][1]);

    if(i != 0){
      printf("in: %d\n", p[other_pipe][0]);
    }

    if(i != NUM_CMD-1){
      printf("out: %d\n", p[cur_pipe][1]);
    }

    // fork the process
    int cur_pid = fork();
    pids[i] = cur_pid;

    if(cur_pid == 0){

      close(p[cur_pipe][0]);
      close(p[other_pipe][1]);

      if(i != 0){
        dup2(p[other_pipe][0], STDIN_FILENO);
      }
      close(p[other_pipe][0]);

      if(i != NUM_CMD-1){
        dup2(p[cur_pipe][1], STDOUT_FILENO);
      }
      close(p[cur_pipe][1]);

      //sleep(10);

      execvp(cmd[i][0], cmd[i]);

      exit(-1);

    }

    close(p[other_pipe][0]);
    close(p[other_pipe][1]);

    other_pipe = cur_pipe;
    //cur_pipe++;
    cur_pipe = (cur_pipe+1)%2;
    i++;
  }

  close(p[other_pipe][0]);
  close(p[other_pipe][1]);

  for(int i = 0; i < NUM_CMD; i++){
    waitpid(pids[i], &status, 0);
  }

  return 0;
}
