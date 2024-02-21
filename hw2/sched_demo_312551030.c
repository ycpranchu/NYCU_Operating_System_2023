#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

typedef struct 
{
  int thread_id;
  int thread_num;
  int sched_policy;
  int sched_priority;
  float time_wait;
} thread_info_t;

pthread_barrier_t barrier;

void *thread_func(void *thread_arg)
{
    /* 1. Wait until all threads are ready */
  thread_info_t* arg = (thread_info_t *)thread_arg;
  int thread_id = arg->thread_id;
  float time_wait = arg->time_wait;
  
  pthread_barrier_wait(&barrier);

  /* 2. Do the task */
  for (int i = 0; i < 3; i++) {
    printf("Thread %d is running\n", thread_id);

    /* Busy for <time_wait> seconds */
    clock_t start = clock(), end = clock();
    double time_elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    while (((float)(clock() - start)) / CLOCKS_PER_SEC <= time_wait);
  }

  // /* 3. Exit the function  */
  pthread_exit(NULL);
}

int main(int argc, char **argv)
{
  thread_info_t *thread_info;
  int num_threads = 4;
  float time_wait = 0.5;
  const char delim[2] = ",";

  /* 1. Parse program arguments */
  int cmd_opt;
  while ((cmd_opt = getopt(argc, argv, "n:t:s:p:")) != EOF) 
  {
    switch (cmd_opt) 
    {
      case 'n': 
      {
        num_threads = atoi(optarg);
        thread_info = malloc(num_threads * sizeof(thread_info_t));
        break;
      }
      case 't': 
      {
        for (int i = 0; i < num_threads; i++) 
        {
          thread_info[i].thread_id = i;
          thread_info[i].time_wait = atof(optarg);
        }

        break;
      }
      case 's': 
      {
        int count = 0;
        char *policy = strtok(optarg, delim);

        for (int i = 0; i < num_threads; i++) 
        {
          if (strcmp(policy, "FIFO") == 0)
            thread_info[i].sched_policy = SCHED_FIFO;
            
          if (strcmp(policy, "NORMAL") == 0)
            thread_info[i].sched_policy = SCHED_OTHER;

          policy = strtok(NULL, delim);
        }

        break;
      }
      case 'p': 
      {
        int count = 0;
        char *priority = strtok(optarg, delim);

        for (int i = 0; i < num_threads; i++) 
        {
          if (thread_info[i].sched_policy == SCHED_OTHER) thread_info[i].sched_priority = -1;
          else thread_info[i].sched_priority = atoi(priority);

          priority = strtok(NULL, delim);
        }

        break;
      }
    }
  }

  /* 2. Create <num_threads> worker threads */
  pthread_t threads[num_threads];
  pthread_attr_t attr[num_threads];
  struct sched_param schedp[num_threads];
  pthread_barrier_init(&barrier, NULL, num_threads + 1);

  for (int i = 0; i < num_threads; i++) 
  {
    pthread_attr_init(&attr[i]);
  }

  /* 3. Set CPU affinity */
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0, &cpuset);

  int retcode;

  for (int i = 0; i < num_threads; i++) 
  {
    retcode = pthread_attr_setaffinity_np(&attr[i], sizeof(cpuset), &cpuset);
    if (retcode != 0)
    {
      printf("ERROR: `pthread_attr_setaffinity_np()` failed. retcode = %i: %s.\n",  retcode, strerror(retcode));
      return 1;
    }
  }

  /* 4. Set the attributes to each thread */
  for (int i = 0; i < num_threads; i++) 
  {
    retcode = pthread_attr_setschedpolicy(&attr[i], thread_info[i].sched_policy);
    if (retcode != 0)
    {
      printf("ERROR: `pthread_attr_setschedpolicy()` failed. retcode = %i: %s.\n", retcode, strerror(retcode));
      return 1;
    }

    if (thread_info[i].sched_policy != SCHED_OTHER)
    {
      schedp[i].sched_priority = thread_info[i].sched_priority;
      retcode = pthread_attr_setschedparam(&attr[i], &schedp[i]);
      if (retcode != 0)
      {
        printf("ERROR: `pthread_attr_setschedparam()` failed. retcode = %i: %s.\n", retcode, strerror(retcode));
        return 1;
      }  
    }

    retcode = pthread_attr_setinheritsched(&attr[i], PTHREAD_EXPLICIT_SCHED);
    if (retcode != 0)
    {
      printf("ERROR: `pthread_attr_setinheritsched()` failed. retcode = %i: %s.\n", retcode, strerror(retcode));
      return 1;
    }  
  }

  /* 5. Start all threads at once */
  for (int i = 0; i < num_threads; i++) 
  {
    pthread_create(&threads[i], &attr[i], thread_func, (void *)&thread_info[i]);
  }

  pthread_barrier_wait(&barrier);
  pthread_barrier_destroy(&barrier);

  /* 6. Wait for all threads to finish  */
  for (long long i = 0; i < num_threads; ++i)
  {
      pthread_join(threads[i], NULL);
  }

  return 0;
}