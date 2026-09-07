#define _XOPEN_SOURCE 700
/* 
 * The code compiles correctly without defining _XOPEN_SOURCE. 
 * However, VS Code may show warnings related to barriers (pthread barriers).
 * Uncommenting this line might be necessary in some systems to enable POSIX 
 * features explicitly, but it also causes a warning for `usleep()`.
 *
 * Since the program works fine without this definition, we are keeping it 
 * commented to avoid unnecessary warnings.
 */
#include <pthread.h>  /* Needed for all pthread library calls */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   /* Needed for strcpy() */
#include <time.h>     /* Needed for time() to seed the RNG */
#include <unistd.h>   /* Needed for sleep() and usleep() */

/* Custom semaphore structure implementation */
typedef struct {
  int value;             /* Current value of the semaphore */
  pthread_mutex_t mtx;   /* Mutex for protecting the semaphore value */
  pthread_cond_t cv;     /* Condition variable for waiting threads */
} semaphore;

/* Global variables */
int n = 100;             /* Number of visitors (default value) */
int m = 10;              /* Number of boats (default value) */
int numOfVisitorLeft = 100; /* Counter for remaining visitors */
#define OneMin 100000    /* 1 minute = 100ms for simulation scaling */

/* Arrays for boat-visitor synchronization */
int *BA,  /* Boat available flag: 1 = available, 0 = not available */
    *BC,  /* Boat-visitor connection: stores visitor ID or -1 if no visitor */
    *BT;  /* Boating time: stores the ride time for each boat */

pthread_barrier_t *BB;    /* Barrier for boat-visitor synchronization (one per boat) */
pthread_mutex_t bmtx;     /* Mutex for protecting the shared arrays */
semaphore boat;           /* Semaphore for boats waiting for visitors */
semaphore rider;          /* Semaphore for visitors waiting for boats */
pthread_barrier_t EOS;    /* End of simulation barrier */

/* V operation for semaphore (signal) */
int signal(semaphore *s) {
  pthread_mutex_lock(&s->mtx);
  s->value++;  /* Increment semaphore value */
  pthread_cond_signal(&s->cv);  /* Wake up a waiting thread */
  pthread_mutex_unlock(&s->mtx);
  return 0;
}

/* P operation for semaphore (wait) */
int wait(semaphore *s) {
  pthread_mutex_lock(&s->mtx);
  while (s->value <= 0) {  /* Wait if semaphore value is <= 0 */
    pthread_cond_wait(&s->cv, &s->mtx);  /* Wait for signal */
  }
  s->value--;  /* Decrement semaphore value */
  pthread_mutex_unlock(&s->mtx);
  return 0;
}

/* Initialize semaphore */
int init(semaphore *s, int value) {
  s->value = value;
  pthread_mutex_init(&s->mtx, NULL);
  pthread_cond_init(&s->cv, NULL);
  return 0;
}

/* Destroy semaphore and free resources */
int pthread_sem_destroy(semaphore *s) {
  pthread_mutex_destroy(&s->mtx);
  pthread_cond_destroy(&s->cv);
  return 0;
}

/* Boat thread function */
void *boatThread(void *arg) {
  int id = *(int *)arg;
  free(arg);  /* Free the memory allocated for the thread ID */
  
  /* Initialize barrier for this boat */
  pthread_barrier_init(&BB[id], NULL, 2);
  printf("Boat \t\t%2d\t Ready\n", id);

  while (1) {
    /* Mark the boat as available */
    pthread_mutex_lock(&bmtx);
    BA[id] = 1;     /* Set availability flag */
    BC[id] = -1;    /* No visitor associated yet */
    pthread_mutex_unlock(&bmtx);
    
    signal(&rider);  /* Signal that a boat is available */
    wait(&boat);     /* Wait for a visitor to signal */
    
    /* Wait for the visitor to complete the handshake */
    pthread_barrier_wait(&BB[id]);
    
    /* Get the ride time and mark boat as not available */
    pthread_mutex_lock(&bmtx);
    BA[id] = 0;
    int rideTime = BT[id];
    pthread_mutex_unlock(&bmtx);
    
    /* Start the boat ride */
    printf("Boat \t\t%2d\t Start of ride for visitor \t%2d\n", BC[id], id);
    usleep(rideTime * OneMin);  /* Simulate the ride */
    printf("Boat \t\t%2d\t End of ride for visitor \t%2d (ride time = %2d)\n", BC[id], id, rideTime);

    /* Check if all visitors have left */
    pthread_mutex_lock(&bmtx);
    if (numOfVisitorLeft <= 0) {
      pthread_mutex_unlock(&bmtx);
      pthread_barrier_wait(&EOS);  /* Signal end of simulation */
      break;
    }
    pthread_mutex_unlock(&bmtx);
  }

  return NULL;
}

/* Visitor (rider) thread function */
void *riderThread(void *arg) {
  int id = *(int *)arg;
  free(arg);  /* Free the memory allocated for the thread ID */
  
  /* Generate random visit and ride times */
  int vtime = 30 + rand() % 91;  /* 30-120 minutes for sightseeing */
  int rtime = 15 + rand() % 46;  /* 15-60 minutes for boating */
  
  printf("Visitor \t%2d\t Starts sightseeing for \t%3d minutes\n", id, vtime);
  usleep(vtime * OneMin);  /* Simulate sightseeing */
  
  printf("Visitor \t%2d\t Ready to ride a boat (ride time = %2d)\n", id, rtime);
  
  signal(&boat);  /* Signal that a visitor is ready */
  wait(&rider);   /* Wait for a boat to signal availability */
  
  /* Search for an available boat */
  int boatId = -1;
  int found = 0;
  while (!found) {
    pthread_mutex_lock(&bmtx);
    for (int i = 0; i < m; i++) {
      if (BA[i] && BC[i] == -1) {  /* Check if boat is available and not assigned */
        boatId = i;
        BC[i] = id;     /* Assign this visitor to the boat */
        BT[i] = rtime;  /* Set the ride time */
        found = 1;
        printf("Visitor \t%2d\t Finds boat \t%2d\t\n", id, i);
        break;
      }
    }
    pthread_mutex_unlock(&bmtx);
    
    if (!found) {
      usleep(OneMin);  /* Short sleep to avoid busy waiting if no boat found */
    }
  }
  
  /* Wait for the boat to be ready */
  pthread_barrier_wait(&BB[boatId]);
  
  /* Update the count of visitors left */
  pthread_mutex_lock(&bmtx);
  numOfVisitorLeft--;
  pthread_mutex_unlock(&bmtx);
  
  printf("Visitor \t%2d\t Leaving\n", id);
  return NULL;
}

/* Main function */
int main(int argc, char *argv[]) {
  /* Parse command-line arguments */
  if (argc != 3) {
    printf("Usage: %s <number of boats> <number of visitors>\n", argv[0]);
    exit(1);
  }
  m = atoi(argv[1]);  /* Number of boats */
  n = atoi(argv[2]);  /* Number of visitors */
  numOfVisitorLeft = n;
  
  /* Validate input */
  if (m < 5 || m > 10 || n < 20 || n > 100) {
    printf("Invalid input: 5 <= m <= 10, 20 <= n <= 100\n");
    exit(1);
  }
  
  /* Initialize semaphores */
  init(&boat, 0);
  init(&rider, 0);

  /* Initialize mutex and barrier */
  pthread_barrier_init(&EOS, NULL, 2);
  pthread_mutex_init(&bmtx, NULL);

  /* Allocate memory for shared arrays */
  BA = (int *)malloc(m * sizeof(int));
  BC = (int *)malloc(m * sizeof(int));
  BT = (int *)malloc(m * sizeof(int));
  BB = (pthread_barrier_t *)malloc(m * sizeof(pthread_barrier_t));

  if (BA == NULL || BC == NULL || BT == NULL || BB == NULL) {
    printf("Memory allocation failed\n");
    exit(1);
  }

  /* Initialize shared arrays */
  for (int i = 0; i < m; i++) {
    BA[i] =  0;    /* Boats not available initially */
    BC[i] = -1;    /* No visitor assigned */
    BT[i] = -1;    /* No ride time set */
  }

  /* Seed random number generator */
  srand(time(NULL));

  /* Create threads */
  pthread_t *tb = (pthread_t *)malloc(m * sizeof(pthread_t)); /* Boat threads */
  pthread_t *tr = (pthread_t *)malloc(n * sizeof(pthread_t)); /* Rider threads */

  /* Create boat threads */
  for (int i = 0; i < m; i++) {
    int *id = (int *)malloc(sizeof(int));
    *id = i;
    pthread_create(&tb[i], NULL, boatThread, id);
  }

  /* Create visitor threads */
  for (int i = 0; i < n; i++) {
    int *id = (int *)malloc(sizeof(int));
    *id = i;
    pthread_create(&tr[i], NULL, riderThread, id);
  }

  /* Wait for simulation to end */
  pthread_barrier_wait(&EOS);

  printf("End of simulation\n");

  /* Clean up resources */
  pthread_barrier_destroy(&EOS);
  pthread_mutex_destroy(&bmtx);
  for (int i = 0; i < m; i++) {
    pthread_barrier_destroy(&BB[i]);
  }
  pthread_sem_destroy(&boat);
  pthread_sem_destroy(&rider);
  free(BA);
  free(BC);
  free(BT);
  free(BB);
  free(tb);
  free(tr);
  return 0;
}