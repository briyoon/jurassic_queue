//CS481 PA06//
//Brian Mc Collum and Phillip Rieman//
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include "random437.h"

#define SECONDS 60
#define MINUTE_LENGTH 10000
#define LOAD_TIME (MINUTE_LENGTH / SECONDS * 7)
#define RIDE_TIME (MINUTE_LENGTH / SECONDS * 53)
#define MAX_WAIT_PEOPLE 800
#define START_TIME 9
#define SIM_LENGTH 600
#define OUTFILE "log.txt"

double totalArrive = 0.0;
double totalReject = 0.0;
double avgTime = 0.0;
int time_step = 0;

typedef struct ParkData {
  int *maxpercar;
  int *queue;
  pthread_mutex_t *mutex;
  pthread_barrier_t *barrier;
  FILE *log_file;
  FILE *arriveFig;
  FILE *waitFig;
  FILE *rejectFig;
} ParkData;

void *people_producer_thread(void* pd_ptr) {
  ParkData *pd = (ParkData*) pd_ptr;

  while(time_step < SIM_LENGTH) {
    // run poisson
    int meanAvg = 0;
    if((time_step < 120) || ((time_step >= 480) && (time_step < 600))) { // 0-2 hours or 7-10 hours
      meanAvg = 25;
    }
    else if((time_step >= 120) && (time_step < 240)) { // 2-5 hours
      meanAvg = 45;
    }
    else if((time_step >= 240) && (time_step < 480)) { // 5-7 hours
      meanAvg = 35;
    }

    int arrivals = poissonRandom(meanAvg);
    printf("time_step %d\n", time_step);

    fprintf(pd->arriveFig, "%d %d\n", time_step, arrivals);
    
    pthread_mutex_lock(pd->mutex);
    int rejected = (int) fmax(0, *pd->queue + arrivals - MAX_WAIT_PEOPLE);
    fprintf(pd->rejectFig, "%d %d\n", time_step, rejected);
	    
    *pd->queue = (int) fmin(MAX_WAIT_PEOPLE, *pd->queue + arrivals);

    totalArrive += arrivals;
    totalReject += rejected;

    double waitTime = *pd->queue / meanAvg;
    fprintf(pd->waitFig, "%d %f\n", time_step, waitTime); 
    avgTime += waitTime;

    // write to log
    fprintf(pd->log_file, "%03d arrive %03d reject %03d wait-line %03d at %02d:%02d:%02d\n",
	    time_step, arrivals, rejected, *pd->queue,
	    (time_step / SECONDS) % SECONDS + START_TIME, // hour
	    time_step % SECONDS, // minute
	    (time_step * SECONDS) % SECONDS // second
	    );
    time_step++;
    fflush(pd->log_file);
    fflush(pd->arriveFig);
    fflush(pd->waitFig);
    fflush(pd->rejectFig);
    pthread_mutex_unlock(pd->mutex);
    fflush(stdout);
    pthread_barrier_wait(pd->barrier);
    usleep(MINUTE_LENGTH);
  }
  printf("producer done\n");
  return NULL;
}

void *ride_consumer_thread(void* pd_ptr) {
  ParkData *pd = (ParkData*) pd_ptr;
  while(time_step < SIM_LENGTH) {
    printf("Consumer barrier wait\n");
    fflush(stdout);
    pthread_barrier_wait(pd->barrier);
    pthread_mutex_lock(pd->mutex);
    usleep(LOAD_TIME);
    *pd->queue = (int) fmax(0, *pd->queue - *pd->maxpercar);
    usleep(RIDE_TIME);
    pthread_mutex_unlock(pd->mutex);
  }
  printf("consumer done\n");
  return NULL;
}

int main(int argc, char **argv) {
  // getopt arg parser
  int carnum;
  int maxpercar;
  int c = 0;
  opterr = 0;

  while((c = getopt(argc, argv, "n:N:m:M:")) != -1) {
    switch(tolower(c)) {
    case 'n':
      carnum = atoi(optarg);
      break;
    case 'm':
      maxpercar = atoi(optarg);
      break;
    case '?':
      if(optopt == 'c') {
	fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      }
      else if(isprint(optopt)) {
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      }
      else {
	fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      }
      return 1;
    default:
      abort();
    }
  }

  // Park data struct
  // We wanted to try this instead of using globals, passing a struct full of
  // pointers to our state. Mainly to practice pointers
  int queue = 0;
  pthread_barrier_t barrier;
  pthread_mutex_t mutex;
  FILE *log_file = NULL;
  FILE *arriveFig = NULL;
  FILE *waitFig = NULL;
  FILE *rejectFig = NULL;
  log_file = fopen(OUTFILE, "w+");
  arriveFig = fopen("n6m7arrive.txt", "w+");
  waitFig = fopen("n6m7wait.txt", "w+");
  rejectFig = fopen("n6m7reject.txt", "w+");

  ParkData *pd = (ParkData*) malloc(sizeof(ParkData));

  pd->maxpercar = &maxpercar;
  pd->barrier = &barrier;
  pd->queue = &queue;
  pd->mutex = &mutex;
  pd->log_file = log_file;
  pd->arriveFig = arriveFig;
  pd->waitFig = waitFig;
  pd->rejectFig = rejectFig;

  // mutex inits
  pthread_mutex_init(&mutex, NULL);
  pthread_barrier_init(&barrier, NULL, carnum + 1);

  // create threads
  pthread_t tid[carnum + 1];

  // producer
  pthread_create(&tid[0], NULL, people_producer_thread, (void *) pd);
  printf("Producer thread created /w tid %lu\n", tid[0]);

  // consumers
  for(int i = 0; i < carnum; i++) {
    pthread_create(&tid[i + 1], NULL, ride_consumer_thread, (void *) pd);
    printf("Consumer thread created /w tid %lu\n", tid[i + 1]);
  }

  // join
  for(int i = 0; i < carnum + 1; i++) {
    pthread_join(tid[i], NULL);
  }

  // clean up
  printf("total arrival: %f, total rejected: %f, reject ratio: %f, avg wait time: %f\n",
	   totalArrive, totalReject, totalReject / totalArrive, avgTime / 600.0);
  printf("Clean up\n");
  fflush(stdout);
  fflush(log_file);
  fflush(arriveFig);
  fflush(waitFig);
  fflush(rejectFig);
  fclose(log_file);
  fclose(arriveFig);
  fclose(waitFig);
  fclose(rejectFig);
  pthread_mutex_destroy(&mutex);
  free(pd);

  return 0;
}
