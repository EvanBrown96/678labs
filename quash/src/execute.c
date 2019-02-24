/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */

#include "execute.h"

#include "debug.h"

#include <stdio.h>

#include "quash.h"

#include "deque.h"

// Remove this and all expansion calls to it
/**
 * @brief Note calls to any function that requires implementation
 */
#define IMPLEMENT_ME()                                                  \
  char debug_str[256];  \
  sprintf(debug_str, "IMPLEMENT ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__); \
  PRINT_DEBUG(debug_str);

/***************************************************************************
 * Create Types
 ***************************************************************************/

IMPLEMENT_DEQUE_STRUCT(PIDDeque, pid_t);
IMPLEMENT_DEQUE(PIDDeque, pid_t);

typedef struct Job{
  int job_id;
  char* cmd;
  PIDDeque pid_list;
} Job;

IMPLEMENT_DEQUE_STRUCT(JobDeque, Job);
IMPLEMENT_DEQUE(JobDeque, Job);

static Job create_job(int job_id, PIDDeque pids){
  return (Job){
    job_id,
    get_command_string(),
    pids
  };
}

/***************************************************************************
 * Declare Global Variables
 ***************************************************************************/

// declare job holders
PIDDeque current_job;

JobDeque bg_jobs;

// declare pipes
int pipes[2][2];
int cur_pipe = 0;
int old_pipe = 1;

bool globals_created = false;

/***************************************************************************
 * Interface Functions
 ***************************************************************************/

int get_next_job_number(){
  if(is_empty_JobDeque(&bg_jobs)){
    return 1;
  }
  Job j = peek_back_JobDeque(&bg_jobs);
  return j.job_id+1;
}

// Return a string containing the current working directory.
char* get_current_directory(bool* should_free) {

  char* current_dir = malloc(256*sizeof(char));
  getcwd(current_dir, 256);

  // Change this to true if necessary
  *should_free = true;

  return current_dir;
}

// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) {
  // TODO: Lookup environment variables. This is required for parser to be able
  // to interpret variables from the command line and display the prompt
  // correctly
  // HINT: This should be pretty simple

  char* var_val = getenv(env_var);

  return var_val;
}

// Check the status of background jobs
void check_jobs_bg_status() {
  // TODO: Check on the statuses of all processes belonging to all background
  // jobs. This function should remove jobs from the jobs queue once all
  // processes belonging to a job have completed.
  IMPLEMENT_ME();

  // TODO: Once jobs are implemented, uncomment and fill the following line
  // print_job_bg_complete(job_id, pid, cmd);
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
  // while(args[0] != NULL){
  //   printf("%s ", args[0]);
  //   args++;
  // }
  //
  // printf("\n");

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

  // TODO: Remove warning silencers
  (void) signal; // Silence unused variable warning
  (void) job_id; // Silence unused variable warning

  // TODO: Kill all processes associated with a background job
  IMPLEMENT_ME();
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
  // TODO: Print background jobs
  IMPLEMENT_ME();

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

  // TODO: Remove warning silencers
  (void) p_in;  // Silence unused variable warning
  (void) p_out; // Silence unused variable warning
  (void) r_in;  // Silence unused variable warning
  (void) r_out; // Silence unused variable warning
  (void) r_app; // Silence unused variable warning

  // TODO: Setup pipes, redirects, and new process
  IMPLEMENT_ME();

  // create a new pipe
  pipe(pipes[cur_pipe]);

  pid_t pid = fork();
  push_back_PIDDeque(&current_job, pid);

  if(pid == 0){

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
  if (holders == NULL)
    return;

  PRINT_DEBUG("entered run_script\n");
  fflush(stdout);

  if(!globals_created){
    PRINT_DEBUG("creating globals\n");
    current_job = new_PIDDeque(5);
    bg_jobs = new_JobDeque(1);

    globals_created = true;
  }

  check_jobs_bg_status();

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
    int status;
    PRINT_DEBUG("waiting for processes...\n");
    while(!is_empty_PIDDeque(&current_job)){
      pid_t pid = pop_front_PIDDeque(&current_job);
      PRINT_DEBUG("%d\n", pid);
      waitpid(pid, &status, 0);
    }
    fflush(stdout);
  }
  else {

    // create job instance with latest created pids
    Job j = create_job(get_next_job_number(), current_job);
    // clear current job deque
    empty_PIDDeque(&current_job);

    // add new job to the background jobs list
    push_back_JobDeque(&bg_jobs, j);

    // print job start information
    print_job_bg_start(j.job_id, peek_front_PIDDeque(&(j.pid_list)), j.cmd);
  }
}
