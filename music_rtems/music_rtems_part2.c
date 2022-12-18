/**********************************************************
 *  INCLUDES
 *********************************************************/
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <limits.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <rtems.h>
#include <rtems/termiostypes.h>
#include <rtems/shell.h>
#include <rtems/untar.h>
#include <bsp.h>


/**********************************************************
 *  CONSTANTS
 *********************************************************/
#define SLAVE_ADDR 0x8
#define NSEC_PER_SEC 1000000000UL

#define DEV_NAME "/dev/com1"
#define AUDIOFILE_NAME "/let_it_be.raw"

#define PERIOD_TASK_PLAYBACK_SEC                0           /* Period of Playback Task */
#define PERIOD_TASK_PLAYBACK_NSEC               64000000    /* Period of Playback Task */
#define PERIOD_TASK_READ_COMMANDS_SEC           1           /* Period of Read Commands Task */
#define PERIOD_TASK_READ_COMMANDS_NSEC          0           /* Period of Read Commands Task */
#define PERIOD_TASK_SHOW_PLAYBACK_STATE_SEC     2           /* Period of Show Playback State Task */
#define PERIOD_TASK_SHOW_PLAYBACK_STATE_NSEC    500000000   /* Period of Show Playback State Task */
#define SEND_SIZE                               256         /* BYTES */

// audiofile tarball stuff
#define TARFILE_START _binary_tarfile_start
#define TARFILE_SIZE _binary_tarfile_size

typedef enum{paused, playing} state_t;

typedef struct {
    unsigned char buf[SEND_SIZE];
    int fd_file;
    int fd_serie;
} th_args_t;


/**********************************************************
 *  GLOBALS
 *********************************************************/
// audiofile tarball stuff
extern int _binary_tarfile_start;
extern int _binary_tarfile_size;
// playback state: governs whether the audiofile is read or not
state_t playback_state = playing;
pthread_mutex_t mutex_playback_state;


void diffTime(struct timespec end, struct timespec start, struct timespec *diff)
{
    if (end.tv_nsec < start.tv_nsec) {
        diff->tv_nsec = NSEC_PER_SEC - start.tv_nsec + end.tv_nsec;
        diff->tv_sec = end.tv_sec - (start.tv_sec+1);
    } else {
        diff->tv_nsec = end.tv_nsec - start.tv_nsec;
        diff->tv_sec = end.tv_sec - start.tv_sec;
    }
}


void addTime(struct timespec end, struct timespec start, struct timespec *add)
{
    unsigned long aux;
    aux = start.tv_nsec + end.tv_nsec;
    add->tv_sec = start.tv_sec + end.tv_sec +
                  (aux / NSEC_PER_SEC);
    add->tv_nsec = aux % NSEC_PER_SEC;
}


int compTime(struct timespec t1, struct timespec t2)
{
    if (t1.tv_sec == t2.tv_sec) {
        if (t1.tv_nsec == t2.tv_nsec) {
            return (0);
        } else if (t1.tv_nsec > t2.tv_nsec) {
            return (1);
        } else if (t1.tv_sec < t2.tv_sec) {
            return (-1);
        }
    } else if (t1.tv_sec > t2.tv_sec) {
        return (1);
    } else if (t1.tv_sec < t2.tv_sec) {
        return (-1);
    }
    return (0);
}


//compute time: 4.6ms
void* task_playback(void *args)
{
    struct timespec start, end, diff, cycle;
    th_args_t *th_args = (th_args_t*) args;
    state_t local_playback_state;

    cycle.tv_sec = PERIOD_TASK_PLAYBACK_SEC;
    cycle.tv_nsec = PERIOD_TASK_PLAYBACK_NSEC;

    clock_gettime(CLOCK_REALTIME, &start);
    while (1) {
        // read global playback_state
        pthread_mutex_lock(&mutex_playback_state);
        local_playback_state = playback_state;
        pthread_mutex_unlock(&mutex_playback_state);

        // depending on playback state
        switch (local_playback_state) {
            case playing:
                // read from music file
                if ( read(th_args->fd_file, th_args->buf, SEND_SIZE) < 0 ) {
                    perror("read: error reading file\n");
                    exit(-1);
                }
                break;
            case paused:
                // fill buffer with 0's
                memset(th_args->buf, 0, SEND_SIZE);
                break;
        }
        
        // write on the serial/I2C port
        if ( write(th_args->fd_serie, th_args->buf, SEND_SIZE) < 0 ) {
            perror("write: error writing serial\n");
            exit(-1);
        }

        clock_gettime(CLOCK_REALTIME, &end);
        diffTime(end, start, &diff);

        if (compTime(cycle, diff) <= 0) {
            fprintf(stderr, "[CRASH PLAYBACK] lasted longer than the cycle\n");
            exit(-1);
        }

        diffTime(cycle, diff, &diff);
        nanosleep(&diff, NULL);
        addTime(start, cycle, &start);
    }
    return NULL;
}


//compute time: 0.0723ms
void* task_read_commands(void *args)
{
    struct timespec start, end, diff, cycle;
    char keyboard_input = '1';

    cycle.tv_sec = PERIOD_TASK_READ_COMMANDS_SEC;
    cycle.tv_nsec = PERIOD_TASK_READ_COMMANDS_NSEC;

    clock_gettime(CLOCK_REALTIME, &start);
    while (1) {
        switch (keyboard_input) {
            case '0':
                pthread_mutex_lock(&mutex_playback_state);
                playback_state = 0;
                pthread_mutex_unlock(&mutex_playback_state);
                break;
            case '1':
                pthread_mutex_lock(&mutex_playback_state);
                playback_state = 1;
                pthread_mutex_unlock(&mutex_playback_state);
                break;
        }
        
        while ( scanf("%c", &keyboard_input) <= 0 );
        clock_gettime(CLOCK_REALTIME, &end);

        // diff = (end - start) % cycle
        diffTime(end, start, &diff);
        while( compTime(diff, cycle) > 0 )
            diffTime(diff, cycle, &diff);

        // this really really should never happen
        if (compTime(cycle, diff) <= 0) {
            fprintf(stderr, "[CRASH READ_COMMANDS] lasted longer than the cycle\n");
            exit(-1);
        }

        diffTime(cycle, diff, &diff);
        nanosleep(&diff, NULL);
        addTime(start, cycle, &start);
    }
    return NULL;
}


//compute time: 2.46ms
void* task_show_playback_state(void *args)
{
    struct timespec start, end, diff, cycle;
    state_t local_playback_state;

    cycle.tv_sec = PERIOD_TASK_SHOW_PLAYBACK_STATE_SEC;
    cycle.tv_nsec = PERIOD_TASK_SHOW_PLAYBACK_STATE_NSEC;

    clock_gettime(CLOCK_REALTIME, &start);
    while (1) {
        // read global playback_state
        pthread_mutex_lock(&mutex_playback_state);
        local_playback_state = playback_state;
        pthread_mutex_unlock(&mutex_playback_state);

        switch (local_playback_state) {
            case playing:
                printf("Reproduction resumed\n"); fflush(stdout);
                break;
            case paused:
                printf("Reproduction paused\n"); fflush(stdout);
                break;
        }

        clock_gettime(CLOCK_REALTIME, &end);
        diffTime(end, start, &diff);

        if (compTime(cycle, diff) <= 0) {
            fprintf(stderr, "[CRASH SHOW_PLAYBACK_STATE] lasted longer than the cycle\n");
            exit(-1);
        }

        diffTime(cycle, diff, &diff);
        nanosleep(&diff, NULL);
        addTime(start, cycle, &start);
    }
    return NULL;
}


rtems_task Init (rtems_task_argument ignored)
{
    th_args_t th_args;

    printf("Populating Root file system from TAR file.\n");
    Untar_FromMemory((unsigned char *) (&TARFILE_START), (unsigned long) &TARFILE_SIZE);

    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4, 100, "/dev/foobar", false, true, NULL);

    /* Open serial port */
    printf("open serial device %s \n", DEV_NAME);
    th_args.fd_serie = open(DEV_NAME, O_RDWR);
    if (th_args.fd_serie < 0) {
        printf("open: error opening serial %s\n", DEV_NAME);
        exit(-1);
    }

    struct termios portSettings;
    speed_t speed = B115200;

    tcgetattr(th_args.fd_serie, &portSettings);
    cfsetispeed(&portSettings, speed);
    cfsetospeed(&portSettings, speed);
    cfmakeraw(&portSettings);
    tcsetattr(th_args.fd_serie, TCSANOW, &portSettings);

    /* Open music file */
    printf("open file %s begin\n", AUDIOFILE_NAME);
    th_args.fd_file = open(AUDIOFILE_NAME, O_RDWR);
    if (th_args.fd_file < 0) {
        perror("open: error opening file \n");
        exit(-1);
    }

    if ( pthread_mutex_init(&mutex_playback_state, NULL) < 0) {
        perror("pthread_mutex_init: error initializing mutex\n");
        exit(-1);
    }

    pthread_t task1, task2, task3;
    pthread_attr_t th_attr_task1, th_attr_task2, th_attr_task3;
    struct sched_param th_params_task1, th_params_task2, th_params_task3;
    th_params_task1.sched_priority = 3;
    th_params_task2.sched_priority = 2;
    th_params_task3.sched_priority = 1;

    pthread_attr_init(&th_attr_task1);
    pthread_attr_init(&th_attr_task2);
    pthread_attr_init(&th_attr_task3);

    pthread_attr_setscope(&th_attr_task1, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&th_attr_task2, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&th_attr_task3, PTHREAD_SCOPE_SYSTEM);

    pthread_attr_setinheritsched(&th_attr_task1, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&th_attr_task2, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&th_attr_task3, PTHREAD_EXPLICIT_SCHED);

    pthread_attr_setschedpolicy(&th_attr_task1, SCHED_FIFO);
    pthread_attr_setschedpolicy(&th_attr_task2, SCHED_FIFO);
    pthread_attr_setschedpolicy(&th_attr_task3, SCHED_FIFO);

    pthread_attr_setschedparam(&th_attr_task1, &th_params_task1);
    pthread_attr_setschedparam(&th_attr_task2, &th_params_task2);
    pthread_attr_setschedparam(&th_attr_task3, &th_params_task3);

    pthread_create(&task1, &th_attr_task1, task_playback, (void*) &th_args);
    pthread_create(&task2, &th_attr_task2, task_read_commands, NULL);
    pthread_create(&task3, &th_attr_task3, task_show_playback_state, NULL);

    pthread_join(task1, NULL);
    pthread_join(task2, NULL);
    pthread_join(task3, NULL);
    exit(0);

} /* End of Init() */

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 20
#define CONFIGURE_UNIFIED_WORK_AREAS
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_INIT
#include <rtems/confdefs.h>
