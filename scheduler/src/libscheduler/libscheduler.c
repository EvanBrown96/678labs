/** @file libscheduler.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libscheduler.h"
#include "../libpriqueue/libpriqueue.h"


/**
  Stores information making up a job to be scheduled including any statistics.

  You may need to define some global variables or a struct to store your job queue elements.
*/
typedef struct _job_t
{

  int job_id;
  int priority;

  int arrival_time;
  int start_time;
  int latest_update_time;
  int running_time;
  int remaining_time;

} job_t;

/**
  creates a new job

  @param job_id id of the new job
  @param priority priority of the new job
  @param arrival_time time the job was given to the scheduler
  @param running_time time the job takes to complete
  @return the new job
 */
job_t* new_job(int job_id, int priority, int arrival_time, int running_time){
  job_t* new_j = malloc(sizeof(job_t));
  new_j->job_id = job_id;
  new_j->priority = priority;
  new_j->arrival_time = arrival_time;
  new_j->start_time = -1;
  new_j->latest_update_time = -1;
  new_j->running_time = running_time;
  new_j->remaining_time = running_time;

  return new_j;
}

/**
  globals
*/

scheme_t g_scheme;         // scheme being used by the scheduler
priqueue_t g_job_queue;    // queue for storing idle jobs
priqueue_t g_idle_cores;   // queue for storing idle cores
int g_total_cores;         // count of total cores available
job_t** g_running_jobs;     // array of jobs currently running
int* g_cores_list;         // array of core indices

int total_jobs;            // count of total jobs completed
int total_waiting_time;    // total time jobs have spent waiting
int total_turnaround_time; // total time from arrival to completion for all jobs
int total_response_time;   // total time from arrival to first run for all jobs

/**
  comparison function for core ids

  used to sort idle_cores priqueue

  @param core1: first core to compare
  @param core2: second core to compare
  @return < 0 if first core should be first; > 0 if second core should be first
 */
int core_compare(const void* core1, const void* core2){
  return *((int*) core1) - *((int*) core2);
}


/**
  comparison function for fcfs scheme

  sorts based on arrival time

  @param j1: first job to compare
  @param j2: second job to compare
  @return < 0 if first job should be first; > 0 if second job should be first
 */
int job_compare_fcfs(const void* j1, const void* j2){
  job_t* job1 = (job_t*)j1;
  job_t* job2 = (job_t*)j2;

  return (job1->arrival_time - job2->arrival_time);
}

/**
  comparison function for sjf scheme

  sorts based on remaining time
  tiebreaker based on arrival time

  @param j1: first job to compare
  @param j2: second job to compare
  @return < 0 if first job should be first; > 0 if second job should be first
 */
int job_compare_sjf(const void* j1, const void* j2){
  job_t* job1 = (job_t*)j1;
  job_t* job2 = (job_t*)j2;

  if(job1->remaining_time == job2->remaining_time){
    return (job1->arrival_time - job2->arrival_time);
  }
  return (job1->remaining_time - job2->remaining_time);
}

/**
  comparison function for priority scheme

  sorts based on priority
  tiebreaker based on arrival time

  @param j1: first job to compare
  @param j2: second job to compare
  @return < 0 if first job should be first; > 0 if second job should be first
 */
int job_compare_pri(const void* j1, const void* j2){
  job_t* job1 = (job_t*)j1;
  job_t* job2 = (job_t*)j2;

  if(job1->priority == job2->priority){
    return (job1->arrival_time - job2->arrival_time);
  }
  return (job1->priority - job2->priority);
}

/**
  Initalizes the scheduler.

  Assumptions:
    - You may assume this will be the first scheduler function called.
    - You may assume this function will be called once once.
    - You may assume that cores is a positive, non-zero number.
    - You may assume that scheme is a valid scheduling scheme.

  @param cores the number of cores that is available by the scheduler. These cores will be known as core(id=0), core(id=1), ..., core(id=cores-1).
  @param scheme  the scheduling scheme that should be used. This value will be one of the six enum values of scheme_t
*/
void scheduler_start_up(int cores, scheme_t scheme)
{
  // set constant globals
  g_total_cores = cores;
  g_scheme = scheme;

  // setup jobs queue and list
  switch(scheme){
    case(FCFS):
    case(RR):
      priqueue_init(&g_job_queue, job_compare_fcfs);
      break;
    case(PRI):
    case(PPRI):
      priqueue_init(&g_job_queue, job_compare_pri);
      break;
    default:
      priqueue_init(&g_job_queue, job_compare_sjf);
  }
  g_running_jobs = malloc(cores * sizeof(job_t*));

  // setup idle cores queue
  priqueue_init(&g_idle_cores, core_compare);
  g_cores_list = malloc(cores * sizeof(int));
  for(int i = 0; i < cores; i++){
    g_cores_list[i] = i;
    priqueue_offer(&g_idle_cores, &g_cores_list[i]);
  }

  // initialize timing data to 0
  total_jobs = 0;
  total_waiting_time = 0;
  total_turnaround_time = 0;
  total_response_time = 0;
}


void schedule_job(job_t* job, int core, int time){
  // set the job start time if job hasn't been run before
  if(job->start_time == -1){
    job->start_time = time;
  }

  // update job latest start time
  job->latest_update_time = time;

  g_running_jobs[core] = job;
}

void update_remaining_time(job_t* job, int time){
  job->remaining_time -= (time - job->latest_update_time);
  job->latest_update_time = time;
}

void unschedule_job(int core, int time){
  job_t* job = g_running_jobs[core];
  g_running_jobs[core] = NULL;

  // test if the job was first scheduled in this same time period
  // if so, reset start time, since the job didn't actually get to run
  if(job->start_time == time) job->start_time = -1;

  update_remaining_time(job, time);
  priqueue_offer(&g_job_queue, job);
}

/**
  Called when a new job arrives.

  If multiple cores are idle, the job should be assigned to the core with the
  lowest id.
  If the job arriving should be scheduled to run during the next
  time cycle, return the zero-based index of the core the job should be
  scheduled on. If another job is already running on the core specified,
  this will preempt the currently running job.
  Assumptions:
    - You may assume that every job wil have a unique arrival time.

  @param job_number a globally unique identification number of the job arriving.
  @param time the current time of the simulator.
  @param running_time the total number of time units this job will run before it will be finished.
  @param priority the priority of the job. (The lower the value, the higher the priority.)
  @return index of core job should be scheduled on
  @return -1 if no scheduling changes should be made.

 */
int scheduler_new_job(int job_number, int time, int running_time, int priority)
{

  // create new job
  job_t* this_job = new_job(job_number, priority, time, running_time);

  // if there are idle cores, job should start immediately
	if(!priqueue_is_empty(&g_idle_cores)){

    // get first idle core
    int core = *(int*)priqueue_poll(&g_idle_cores);
    // add job to running jobs list
    schedule_job(this_job, core, time);

    return core;

  }

  // if there are not idle cores...
  switch(g_scheme){
    case(PSJF):
    case(PPRI): {
      int least_job_core = 0;
      update_remaining_time(g_running_jobs[0], time);
      // find currently running job with least importance
      for(int i = 1; i < g_total_cores; i++){
        update_remaining_time(g_running_jobs[i], time);
        if(g_job_queue.comparer(g_running_jobs[i], g_running_jobs[least_job_core]) > 0){
          least_job_core = i;
        }
      }

      // test if we should do preemption
      if(g_job_queue.comparer(this_job, g_running_jobs[least_job_core]) < 0){
        // unschedule job currently running on the core
        unschedule_job(least_job_core, time);
        // schedule new job
        schedule_job(this_job, least_job_core, time);
        return least_job_core;
      }
    }
    default: {
      // non-preemptive schedulers (and round robin) should just be added to waiting queue
      // preemptive schedulers that don't do a preemption will fall through to this case also
      priqueue_offer(&g_job_queue, this_job);
      return -1;
    }
  }

}

/**
  Called when a job has completed execution.

  The core_id, job_number and time parameters are provided for convenience. You may be able to calculate the values with your own data structure.
  If any job should be scheduled to run on the core free'd up by the
  finished job, return the job_number of the job that should be scheduled to
  run on core core_id.

  @param core_id the zero-based index of the core where the job was located.
  @param job_number a globally unique identification number of the job.
  @param time the current time of the simulator.
  @return job_number of the job that should be scheduled to run on core core_id
  @return -1 if core should remain idle.
 */
int scheduler_job_finished(int core_id, int job_number, int time)
{
  // get job that finished
	job_t* finished_job = g_running_jobs[core_id];
  g_running_jobs[core_id] = NULL;
  // increment finished job count
  total_jobs++;

  // update timing information
  int turnaround_time = time - finished_job->arrival_time;
  total_turnaround_time += turnaround_time;
  total_waiting_time += (turnaround_time - finished_job->running_time);
  total_response_time += (finished_job->start_time - finished_job->arrival_time);
  printf("FINISHED JOB %d: %d, %d", finished_job->job_id, finished_job->arrival_time, finished_job->start_time);

  free(finished_job);

  // if there aren't any jobs waiting
  if(priqueue_is_empty(&g_job_queue)){
    priqueue_offer(&g_idle_cores, &g_cores_list[core_id]);
    return -1;
  }

  // otherwise, schedule next job on this core
  job_t* next_job = priqueue_poll(&g_job_queue);
  schedule_job(next_job, core_id, time);

  return next_job->job_id;

}


/**
  When the scheme is set to RR, called when the quantum timer has expired
  on a core.

  If any job should be scheduled to run on the core free'd up by
  the quantum expiration, return the job_number of the job that should be
  scheduled to run on core core_id.

  @param core_id the zero-based index of the core where the quantum has expired.
  @param time the current time of the simulator.
  @return job_number of the job that should be scheduled on core cord_id
  @return -1 if core should remain idle
 */
int scheduler_quantum_expired(int core_id, int time)
{
	return -1;
}


/**
  Returns the average waiting time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average waiting time of all jobs scheduled.
 */
float scheduler_average_waiting_time()
{
	return ((float) total_waiting_time) / total_jobs;
}


/**
  Returns the average turnaround time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average turnaround time of all jobs scheduled.
 */
float scheduler_average_turnaround_time()
{
	return ((float) total_turnaround_time) / total_jobs;
}


/**
  Returns the average response time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average response time of all jobs scheduled.
 */
float scheduler_average_response_time()
{
	return ((float) total_response_time) / total_jobs;
}


/**
  Free any memory associated with your scheduler.

  Assumptions:
    - This function will be the last function called in your library.
*/
void scheduler_clean_up()
{
  priqueue_destroy(&g_idle_cores);
  priqueue_destroy(&g_job_queue);
  free(g_running_jobs);
  free(g_cores_list);
}


/**
  This function may print out any debugging information you choose. This
  function will be called by the simulator after every call the simulator
  makes to your scheduler.
  In our provided output, we have implemented this function to list the jobs in the order they are to be scheduled. Furthermore, we have also listed the current state of the job (either running on a given core or idle). For example, if we have a non-preemptive algorithm and job(id=4) has began running, job(id=2) arrives with a higher priority, and job(id=1) arrives with a lower priority, the output in our sample output will be:

    2(-1) 4(0) 1(-1)

  This function is not required and will not be graded. You may leave it
  blank if you do not find it useful.
 */
void scheduler_show_queue()
{

}
