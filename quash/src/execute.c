/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */

#include "execute.h"

#include <stdio.h>

#include "quash.h"

#include "deque.h"

#include <fcntl.h>

#include <sys/wait.h>

#include <signal.h>



/***************************************************************************
 * Create Types & Type Management Functions
 ***************************************************************************/

IMPLEMENT_DEQUE_STRUCT(PIDDeque, pid_t);
IMPLEMENT_DEQUE(PIDDeque, pid_t);

/**
 * @brief abstraction of a job, or a number of inter-related user commands
 * and the processes associated with them
 */
typedef struct Job{
  int job_id; /**< id of the job */
  char* cmd; /**< command input by the user associated with this job */
  PIDDeque pid_list; /**< list of process ids running under this job */
} Job;

IMPLEMENT_DEQUE_STRUCT(JobDeque, Job);
IMPLEMENT_DEQUE(JobDeque, Job);


/**
* @brief creates a duplicate of a PIDDeque
*
* the original passed-in PIDDeque is not modified
*
* @param pid_list the PIDDeque to create a duplicate of
*
* @return a new PIDDeque which is a duplicate of pid_list
*/
PIDDeque duplicate_PIDDeque(PIDDeque* pid_list){

 size_t len;
 pid_t* raw_list = as_array_PIDDeque(pid_list, &len);

 *pid_list = new_PIDDeque(len);
 PIDDeque ret = new_PIDDeque(len);

 for(int i = 0; i < len; i++){
   push_back_PIDDeque(pid_list, raw_list[i]);
   push_back_PIDDeque(&ret, raw_list[i]);
 }

 free(raw_list);

 return ret;

}



/**
* @brief creates a job with the given pid deque
*
* job will be given next available job number after those already in the background deque
* job will be assigned a command string based on latest command passed in
* the pids passed in are copied and will NOT be affected by their inclusion
*
* @param job_id the id number to assign to the job
*
* @param pids deque containing the pids running under this job
*
* @return the created job
*/
Job create_job(int job_id, PIDDeque* pids){
 return (Job){
   job_id,
   get_command_string(),
   duplicate_PIDDeque(pids)
 };
}



/**
 * @brief frees any memory associated with the passed-in job
 *
 * @param j the job to destroy
 */
void destroy_job(Job j){

 destroy_PIDDeque(&(j.pid_list));
 free(j.cmd);

}



/***************************************************************************
 * Declare Global Variables
 ***************************************************************************/

// declare job holders
PIDDeque current_job; /**< contains a list of pids associated with the job running in the foreground */

JobDeque bg_jobs; /**< contains all of the jobs executing in the background */

// declare pipes
int pipes[2][2]; /**< used for piping input and output between processes */
int cur_pipe = 0; /**< id of the next pipe to be used - pipes are alternated */
int old_pipe = 1; /**< id of the pipe that was last used (always = cur_pipe+1%2) */

int status; /**< used for passing into waitpid and storing returned status */

bool globals_created = false; /**< flag if global variables have been initialized in run_script yet */



/***************************************************************************
 * Declare Useful Global Functions
 ***************************************************************************/

 /**
  * @brief helper function for returning next available job number for a job
  *
  * @return the next available background job number
  */
 int get_next_job_number(){
   if(is_empty_JobDeque(&bg_jobs)){
     return 1;
   }
   Job j = peek_back_JobDeque(&bg_jobs);
   return j.job_id+1;
 }



/**
 * @brief destroys all globals declared in execute.c
 *
 * should be called upon exit of program
 */
void cleanup_globals(){

  destroy_PIDDeque(&current_job);
  destroy_JobDeque(&bg_jobs);

}



/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current working directory.
char* get_current_directory(bool* should_free) {

  char* current_dir = malloc(256*sizeof(char));
  getcwd(current_dir, 256);

  // we allocated the memory for current_dir, so it should be freed
  *should_free = true;

  return current_dir;
}



// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) {

  char* var_val = getenv(env_var);

  return var_val;
}



// Check the status of background jobs
void check_jobs_bg_status() {

  // get initial length of jobs list
  int jobs_start_length = length_JobDeque(&bg_jobs);

  for(int i = 0; i < jobs_start_length; i++){
    // get the next job and its pid information
    Job j = pop_front_JobDeque(&bg_jobs);

    PIDDeque pid_list = duplicate_PIDDeque(&j.pid_list);
    pid_t first_pid = peek_front_PIDDeque(&pid_list);

    // iterate through all of its pids
    bool done = true;
    while(!is_empty_PIDDeque(&pid_list)){
      pid_t pid = pop_front_PIDDeque(&pid_list);

      // waitpid will return 0 if the process is still running
      int x = waitpid(pid, &status, WNOHANG);
      if(x == 0){
        done = false;
        break;
      }
    }

    destroy_PIDDeque(&pid_list);

    if(done){
      // if the process is done, print a message and destroy its data
      print_job_bg_complete(j.job_id, first_pid, j.cmd);
      destroy_job(j);
    }
    else{
      // otherwise re-add popped job to the jobs queue
      push_back_JobDeque(&bg_jobs, j);
    }

  }
}



// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char* cmd) {
  printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
  fflush(stdout);
}



// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char* cmd) {

  printf("Background job started: ");
  print_job(job_id, pid, cmd);
}



// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char* cmd) {
  printf("Completed: \t");
  print_job(job_id, pid, cmd);
}



/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd) {
  // Execute a program with a list of arguments. The `args` array is a NULL
  // terminated (last string is always NULL) list of strings. The first element
  // in the array is the executable
  char* exec = cmd.args[0];
  char** args = cmd.args;

  execvp(exec, args);

  // this will only execute if the execvp call failed
  perror("ERROR: Failed to execute program");
}



// Print strings
void run_echo(EchoCommand cmd) {
  // Print an array of strings. The args array is a NULL terminated (last
  // string is always NULL) list of strings.
  char** str = cmd.args;

  while(str[0] != NULL){
    printf("%s ", str[0]);
    str++;
  }

  printf("\n");

  // Flush the buffer before returning
  fflush(stdout);
}



// Sets an environment variable
void run_export(ExportCommand cmd) {
  // Write an environment variable
  const char* env_var = cmd.env_var;
  const char* val = cmd.val;

  // set relevant enviroment variable - always overwrite (1 flag)
  setenv(env_var, val, 1);

}



// Changes the current working directory
void run_cd(CDCommand cmd) {
  // Get the directory name
  const char* dir = cmd.dir;

  // Check if the directory is valid
  if (dir == NULL) {
    perror("ERROR: Failed to resolve path");
    return;
  }

  // change directory
  chdir(dir);

  // get current PWD value
  const char* new_dir = lookup_env("PWD");

  // set environment variables
  setenv("OLD_PWD", new_dir, 1);
  setenv("PWD", dir, 1);
}



// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd) {
  int signal = cmd.sig;
  int job_id = cmd.job;
  bool killed = false;

  int jobs_start_length = length_JobDeque(&bg_jobs);
  // we must iterate through ALL the jobs since they are popped and readded
  // to the bg_jobs deque - otherwise they will end up out of order
  for(int i = 0; i < jobs_start_length; i++){
    Job j = pop_front_JobDeque(&bg_jobs);

    if(j.job_id == job_id){
      // notify that a job meeting the job id was found
      killed = true;

      PIDDeque pid_list = duplicate_PIDDeque(&(j.pid_list));

      // kill all the processes in the job
      while(!is_empty_PIDDeque(&pid_list)){
        kill(pop_front_PIDDeque(&pid_list), signal);
      }

      destroy_PIDDeque(&pid_list);

    }

    // add the job back to the pid queue - it should stay there until the next
    // command is run, where it will print a completion notice and be removed
    push_back_JobDeque(&bg_jobs, j);
  }

  // if no job was found with the given id, print an error
  if(!killed){
    fprintf(stderr, "ERR: job found with given id\n");
  }

}



// Prints the current working directory to stdout
void run_pwd() {

  bool n = true;
  char* cwd = get_current_directory(&n);
  printf("%s\n", cwd);

  free(cwd);

  // Flush the buffer before returning
  fflush(stdout);
}



// Prints all background jobs currently in the job list to stdout
void run_jobs() {

  int jobs_start_length = length_JobDeque(&bg_jobs);

  for(int i = 0; i < jobs_start_length; i++){
    Job j = pop_front_JobDeque(&bg_jobs);

    print_job(j.job_id, peek_front_PIDDeque(&(j.pid_list)), j.cmd);
    push_back_JobDeque(&bg_jobs, j);

  }

  // Flush the buffer before returning
  fflush(stdout);
}



/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case GENERIC:
    run_generic(cmd.generic);
    break;

  case ECHO:
    run_echo(cmd.echo);
    break;

  case PWD:
    run_pwd();
    break;

  case JOBS:
    run_jobs();
    break;

  case EXPORT:
  case CD:
  case KILL:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }

  exit(0);
}



/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case EXPORT:
    run_export(cmd.export);
    break;

  case CD:
    run_cd(cmd.cd);
    break;

  case KILL:
    run_kill(cmd.kill);
    break;

  case GENERIC:
  case ECHO:
  case PWD:
  case JOBS:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}



/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some way
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */
void create_process(CommandHolder holder) {
  // Read the flags field from the parser
  bool p_in  = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in  = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                               // is true

  // create a new pipe
  pipe(pipes[cur_pipe]);

  // fork the process
  pid_t pid = fork();
  // push the pid of the child process to the current job queue (occurs in parent & child, only matters in parent)
  push_back_PIDDeque(&current_job, pid);

  if(pid == 0){

    // setup pipes
    if(p_in){
      // redirect input to this process from the previous pipe
      dup2(pipes[old_pipe][0], STDIN_FILENO);
    }
    if(p_out){
      // redirect output of this process to the next pipe
      dup2(pipes[cur_pipe][1], STDOUT_FILENO);
    }

    // close pipes
    close(pipes[cur_pipe][0]);
    close(pipes[cur_pipe][1]);
    close(pipes[old_pipe][0]);
    close(pipes[old_pipe][1]);

    // setup redirects
    if(r_in){
      int in_fd = open(holder.redirect_in, O_RDONLY);
      dup2(in_fd, STDIN_FILENO);
      close(in_fd);
    }

    if(r_out){
      // use default file permissions when creating file of -rw-r--r--
      int out_fd;
      if(r_app){
        out_fd = open(holder.redirect_out, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      }
      else{
        out_fd = open(holder.redirect_out, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      }
      dup2(out_fd, STDOUT_FILENO);
      close(out_fd);
    }

    child_run_command(holder.cmd); // This should be done in the child branch of a fork
  }
  else{
    parent_run_command(holder.cmd); // This should be done in the parent branch of a fork
  }

  // close the old pipe, since it will not be used anymore
  close(pipes[old_pipe][0]);
  close(pipes[old_pipe][1]);

  // update which pipe index should be used
  old_pipe = cur_pipe;
  cur_pipe = (cur_pipe+1)%2;

}



// Run a list of commands
void run_script(CommandHolder* holders) {
  // return immediately if no script entered
  if (holders == NULL)
    return;

  if(!globals_created){
    // initialize deques if this is the first time into the run_script command
    current_job = new_PIDDeque(5);
    bg_jobs = new_destructable_JobDeque(1, destroy_job);

    // set queue cleanup function to run at exit
    atexit(cleanup_globals);

    globals_created = true;
  }

  // check if any jobs have completed since last command
  check_jobs_bg_status();

  // check if we should exit
  if (get_command_holder_type(holders[0]) == EXIT &&
      get_command_holder_type(holders[1]) == EOC) {
    end_main_loop();
    return;
  }

  CommandType type;

  // initialize old pipe (will not be used, but create_process expected one to exist for all processes)
  pipe(pipes[old_pipe]);

  // Run all commands in the `holder` array
  for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i)
    create_process(holders[i]);

  // close old pipe, for posterity
  close(pipes[old_pipe][0]);
  close(pipes[old_pipe][1]);

  if (!(holders[0].flags & BACKGROUND)) {
    // process is running in foreground
    // go through every pid in the current job
    while(!is_empty_PIDDeque(&current_job)){
      pid_t pid = pop_front_PIDDeque(&current_job);
      // wait until this pid exits
      waitpid(pid, &status, 0);
    }
    fflush(stdout);

  }
  else {

    // create job instance with current job pids
    Job j = create_job(get_next_job_number(), &current_job);

    // clear current job deque
    empty_PIDDeque(&current_job);

    // add new job to the background jobs list
    push_back_JobDeque(&bg_jobs, j);

    // print job start information
    print_job_bg_start(j.job_id, peek_front_PIDDeque(&(j.pid_list)), j.cmd);

  }
}
