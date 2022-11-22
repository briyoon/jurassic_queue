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

typedef struct ParkData
{
    int *maxpercar;
    int *queue;
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
    FILE *log_file;
} ParkData;

void *people_producer_thread(void* pd_ptr)
{
    ParkData *pd = (ParkData*) pd_ptr;

    for (int time_step = 0; time_step < SIM_LENGTH; time_step++)
    {
        // run poisson
        int meanAvg = 0;
        if (time_step < MINUTE_LENGTH * 2) // 0-2 hours
        {
            meanAvg = 25;
        }
        else if (time_step < MINUTE_LENGTH * 5) // 2-5 hours
        {
            meanAvg = 45;
        }
        else if (time_step < MINUTE_LENGTH * 7) // 5-7 hours
        {
            meanAvg = 35;
        }
        else // 7-10 hours
        {
            meanAvg = 25;
        }
        int arrivals = poissonRandom(meanAvg);

        // accept / reject
        pthread_mutex_lock(pd->mutex);
        *pd->queue = (int) fmin(MAX_WAIT_PEOPLE, *pd->queue + arrivals);

        // printf("q: %d\n", *pd->queue);

        int rejected = (int) fmax(0, *pd->queue + arrivals - MAX_WAIT_PEOPLE);

        // write to log
        fprintf(
            pd->log_file,
            "%03d arrive %03d reject %03d wait-line %03d at %02d:%02d:%02d\n",
            time_step,
            arrivals,
            rejected,
            *pd->queue,
            (time_step / SECONDS) % SECONDS + START_TIME, // hour
            time_step % SECONDS, // minute
            (time_step * SECONDS) % SECONDS // second
        );
        fflush(pd->log_file);
        pthread_mutex_unlock(pd->mutex);

        // barrier wait (thread sync)
        printf("Producer barrier wait\n");
        fflush(stdout);
        // time_step++;
        printf("Time step: %d\n", time_step);

        pthread_barrier_wait(pd->barrier);

        usleep(MINUTE_LENGTH);
    }

    return NULL;
}

void *ride_consumer_thread(void* pd_ptr)
{
    ParkData *pd = (ParkData*) pd_ptr;

    for (int time_step = 0; time_step < SIM_LENGTH; time_step++)
    {
        // barrier wait for all threads (thread sync)
        printf("Consumer barrier wait\n");
        fflush(stdout);
        pthread_barrier_wait(pd->barrier);

        // load and consume passengers
        usleep(LOAD_TIME);
        pthread_mutex_lock(pd->mutex);
        *pd->queue = (int) fmax(0, *pd->queue - *pd->maxpercar);
        pthread_mutex_unlock(pd->mutex);
        usleep(RIDE_TIME);
    }

    return NULL;
}

int main(int argc, char **argv)
{
    // getopt arg parser
    int carnum = 4;
    int maxpercar = 7;
    int c = 0;
    opterr = 0;

    while ((c = getopt(argc, argv, "n:N:m:M:")) != -1)
    {
        switch (tolower(c))
        {
            case 'n':
                carnum = atoi(optarg);
                break;
            case 'm':
                maxpercar = atoi(optarg);
                break;
            case '?':
                if (optopt == 'c') {fprintf (stderr, "Option -%c requires an argument.\n", optopt);}
                else if (isprint (optopt)) {fprintf (stderr, "Unknown option `-%c'.\n", optopt);}
                else {fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);}
                return 1;
            default:
                abort();
        }
    }

    // Park data struct
    // We wanted to try this instead of using globals, passing a struct full of
    // pointers to our state. Mainly to practice pointers
    int queue = 0;
    // int time_step = 0;
    pthread_barrier_t barrier;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    FILE *log_file = NULL;
    log_file = fopen(OUTFILE, "w+");

    ParkData *pd = (ParkData*) malloc(sizeof(ParkData));
    pd->maxpercar = &maxpercar;
    pd->barrier = &barrier;
    pd->queue = &queue;
    // pd->time_step = &time_step;
    pd->mutex = &mutex;
    pd->log_file = log_file;

    // mutex inits
    pthread_mutex_init(&mutex, NULL);
    pthread_barrier_init(&barrier, NULL, carnum + 1);

    // create threads
    pthread_t tid[carnum + 1];

    // producer
    pthread_create(&tid[0], NULL, people_producer_thread, (void *) pd);
    printf("Producer thread created /w tid %lu\n", tid[0]);

    // consumers
    for (int i = 0; i < carnum; i++)
    {
        pthread_create(&tid[i + 1], NULL, ride_consumer_thread, (void *) pd);
        printf("Consumer thread created /w tid %lu\n", tid[i + 1]);
    }

    // join
    for (int i = 0; i < carnum + 1; i++)
    {
        pthread_join(tid[i], NULL);
    }

    // printf("threads done\n");
    // fflush(stdout);

    // // figure stuff
    // FILE *fig1 = NULL, *fig2 = NULL, *fig3 = NULL;

    // fig1 = fopen("fig1.txt", "w");
    // fig2 = fopen("fig2.txt", "w");
    // fig3 = fopen("fig3.txt", "w");

    // clean up
    printf("Clean up\n");
    fflush(stdout);
    fflush(log_file);
    fclose(log_file);
    pthread_mutex_destroy(&mutex);
    free(pd);

    return 0;
}