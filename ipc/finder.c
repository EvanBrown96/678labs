#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <sys/wait.h>

#define BSIZE 256

#define BASH_EXEC  "/bin/bash"
#define FIND_EXEC  "/usr/bin/find"
#define XARGS_EXEC "/usr/bin/xargs"
#define GREP_EXEC  "/bin/grep"
#define SORT_EXEC  "/usr/bin/sort"
#define HEAD_EXEC  "/usr/bin/head"

int main(int argc, char* argv[]){

  if (argc != 4) {
    printf("usage: finder DIR STR NUM_FILES\n");
    exit(0);
  }

  char* search_dir = argv[1];
  char* search_str = argv[2];
  char* num_files  = argv[3];

  int status;
  pid_t pid_1, pid_2, pid_3, pid_4;

  int p1[2], p2[2], p3[2];

  // create pipes
  pipe(p1);
  pipe(p2);
  pipe(p3);

  // first process
  pid_1 = fork();
  if(pid_1 == 0){

      // close all read ends
      close(p1[0]);
      close(p2[0]);
      close(p3[0]);
      // close write ends of pipes 2 and 3
      close(p2[1]);
      close(p3[1]);
      // set standard output to the write end of p1 and close p1, since it's not needed anymore
      dup2(p1[1], STDOUT_FILENO);
      close(p1[1]);

      char cmdbuf[BSIZE];
      bzero(cmdbuf, BSIZE);
      sprintf(cmdbuf, "%s %s -name \'*\'.[ch]", FIND_EXEC, search_dir);

      char* args[] = {BASH_EXEC, "-c", cmdbuf, (char*) NULL};
      if(execv(BASH_EXEC, args) < 0){
        fprintf(stderr, "Error executing process 1 (ERRNO %d)", errno);
        return EXIT_FAILURE;
      }

      exit(0);

  }

  // second process
  pid_2 = fork();
  if(pid_2 == 0){

    // close unnecessary read ends
    close(p2[0]);
    close(p3[0]);
    // close unnecessary write ends
    close(p1[1]);
    close(p3[1]);
    // set standard input to the read end of pipe 1
    dup2(p1[0], STDIN_FILENO);
    close(p1[0]);
    // set standard output to the write end of pipe 2
    dup2(p2[1], STDOUT_FILENO);
    close(p2[1]);

    char* args[] = {XARGS_EXEC, GREP_EXEC, "-c", search_str, (char*) NULL};
    if(execv(XARGS_EXEC, args) < 0){
      fprintf(stderr, "Error executing process 2 (ERRNO %d)", errno);
      return EXIT_FAILURE;
    }

    exit(0);

  }

  // second process
  pid_3 = fork();
  if(pid_3 == 0){

    // close unnecessary read ends
    close(p1[0]);
    close(p3[0]);
    // close unnecessary write ends
    close(p1[1]);
    close(p2[1]);
    // set standard input to the read end of pipe 2
    dup2(p2[0], STDIN_FILENO);
    close(p2[0]);
    // set standard output to the write end of pipe 3
    dup2(p3[1], STDOUT_FILENO);
    close(p3[1]);

    char* args[] = {SORT_EXEC, "-t", ":", "+1.0", "-2.0", "--numeric", "--reverse", (char*) NULL};
    if(execv(SORT_EXEC, args) < 0){
      fprintf(stderr, "Error executing process 3 (ERRNO %d)", errno);
      return EXIT_FAILURE;
    }

    exit(0);

  }

  // second process
  pid_4 = fork();
  if(pid_4 == 0){

    // close unnecessary read ends
    close(p1[0]);
    close(p2[0]);
    // close unnecessary write ends
    close(p1[1]);
    close(p2[1]);
    close(p3[1]);
    // set standard input to the read end of pipe 3
    dup2(p3[0], STDIN_FILENO);
    close(p3[0]);

    char cmdbuf[BSIZE];
    bzero(cmdbuf, BSIZE);
    sprintf(cmdbuf, "%s --lines=%s", HEAD_EXEC, num_files);

    char* args[] = {BASH_EXEC, "-c", cmdbuf, (char*) NULL};
    if(execv(BASH_EXEC, args) < 0){
      fprintf(stderr, "Error executing process 3 (ERRNO %d)", errno);
      return EXIT_FAILURE;
    }

    exit(0);

  }

  // close all pipe ends in main process
  close(p1[0]);
  close(p1[1]);
  close(p2[0]);
  close(p2[1]);
  close(p3[0]);
  close(p3[1]);

  // verify processes completed successfully
  if((waitpid(pid_1, &status, 0)) == -1){
    fprintf(stderr, "Process 1 encountered an error. ERROR%d", errno);
    return EXIT_FAILURE;
  }

  if((waitpid(pid_2, &status, 0)) == -1){
    fprintf(stderr, "Process 1 encountered an error. ERROR%d", errno);
    return EXIT_FAILURE;
  }

  if((waitpid(pid_3, &status, 0)) == -1){
    fprintf(stderr, "Process 1 encountered an error. ERROR%d", errno);
    return EXIT_FAILURE;
  }

  if((waitpid(pid_4, &status, 0)) == -1){
    fprintf(stderr, "Process 1 encountered an error. ERROR%d", errno);
    return EXIT_FAILURE;
  }

  return 0;

}
