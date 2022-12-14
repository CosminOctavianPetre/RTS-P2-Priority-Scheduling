/**********************************************************
 *  INCLUDES
 *********************************************************/
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
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
#define AUDIOFILE_NAME "/let_it_be_1bit.raw"

#define PERIOD_TASK_PLAYBACK_SEC                0           /* Period of Playback Task */
#define PERIOD_TASK_PLAYBACK_NSEC               512000000   /* Period of Playback Task */
#define PERIOD_TASK_READ_COMMANDS_SEC           1           /* Period of Read Commands Task */
#define PERIOD_TASK_READ_COMMANDS_NSEC          0           /* Period of Read Commands Task */
#define PERIOD_TASK_SHOW_PLAYBACK_STATE_SEC     2           /* Period of Show Playback State Task */
#define PERIOD_TASK_SHOW_PLAYBACK_STATE_NSEC    500000000   /* Period of Show Playback State Task */
#define SEND_SIZE                               256         /* BYTES */

// audiofile tarball stuff
#define TARFILE_START _binary_tarfile_start
#define TARFILE_SIZE _binary_tarfile_size

typedef enum{paused, playing} state_t;


/**********************************************************
 *  GLOBALS
 *********************************************************/
// audiofile tarball stuff
extern int _binary_tarfile_start;
extern int _binary_tarfile_size;
// playback state: governs whether the audiofile is read or not
state_t playback_state = playing;


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


void task_playback(int fd_serie, int fd_file, unsigned char *buf)
{
    struct timespec start, end, diff, cycle;
    int ret;

    cycle.tv_sec = PERIOD_TASK_PLAYBACK_SEC;
    cycle.tv_nsec = PERIOD_TASK_PLAYBACK_NSEC;

    clock_gettime(CLOCK_REALTIME, &start);
    while (1) {
        // read from music file
        ret = read(fd_file, buf, SEND_SIZE);
        if (ret < 0) {
            printf("read: error reading file\n");
            exit(-1);
        }
        
        // write on the serial/I2C port
        ret = write(fd_serie, buf, SEND_SIZE);
        if (ret < 0) {
            printf("write: error writting serial\n");
            exit(-1);
        }

        // get end time, calculate lapso and sleep
        clock_gettime(CLOCK_REALTIME, &end);
        diffTime(end, start, &diff);
        if (0 >= compTime(cycle, diff)) {
            printf("ERROR: lasted long than the cycle\n");
            exit(-1);
        }
        diffTime(cycle, diff, &diff);
        nanosleep(&diff, NULL);
        addTime(start, cycle, &start);
    }
}


void task_read_commands()
{
    struct timespec start, end, diff, cycle;
    char keyboard_input = '1';

    cycle.tv_sec = PERIOD_TASK_READ_COMMANDS_SEC;
    cycle.tv_nsec = PERIOD_TASK_READ_COMMANDS_NSEC;
    clock_gettime(CLOCK_REALTIME, &start);

    while (1) {
        switch (keyboard_input) {
            case '0':
                playback_state = 0;
                break;
            case '1':
                playback_state = 1;
                break;
        }

        //TODO: remove temporary this stuff
        switch (playback_state) {
            case playing:
                printf("Reproduction resumed\n"); fflush(stdout);
                break;
            case paused:
                printf("Reproduction paused\n"); fflush(stdout);
                break;
        }
        
        while ( scanf("%c", &keyboard_input) <= 0 );
        clock_gettime(CLOCK_REALTIME, &end);

        diffTime(end, start, &diff);
        while( compTime(diff, cycle) > 0 )
            diffTime(diff, cycle, &diff);

        nanosleep(&diff, NULL);
        addTime(start, cycle, &start);
    }
}


void task_show_playback_state()
{
    struct timespec start, end, diff, cycle;

    cycle.tv_sec = PERIOD_TASK_SHOW_PLAYBACK_STATE_SEC;
    cycle.tv_nsec = PERIOD_TASK_SHOW_PLAYBACK_STATE_NSEC;
    clock_gettime(CLOCK_REALTIME, &start);

    while (1) {
        switch (playback_state) {
            case playing:
                printf("Reproduction resumed\n"); fflush(stdout);
                break;
            case paused:
                printf("Reproduction paused\n"); fflush(stdout);
                break;
        }

        clock_gettime(CLOCK_REALTIME, &end);
        diffTime(end, start, &diff);

        nanosleep(&diff, NULL);
        addTime(start, cycle, &start);
    }
}


rtems_task Init (rtems_task_argument ignored)
{
    struct timespec start, end, diff, cycle;
    unsigned char buf[SEND_SIZE];
    int fd_file = -1;
    int fd_serie = -1;
    int ret = 0;

    printf("Populating Root file system from TAR file.\n");
    Untar_FromMemory((unsigned char *) (&TARFILE_START), (unsigned long) &TARFILE_SIZE);

    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4, 100, "/dev/foobar", false, true, NULL);

    /* Open serial port */
    printf("open serial device %s \n", DEV_NAME);
    fd_serie = open (DEV_NAME, O_RDWR);
    if (fd_serie < 0) {
        printf("open: error opening serial %s\n", DEV_NAME);
        exit(-1);
    }

    struct termios portSettings;
    speed_t speed = B115200;

    tcgetattr(fd_serie, &portSettings);
    cfsetispeed(&portSettings, speed);
    cfsetospeed(&portSettings, speed);
    cfmakeraw(&portSettings);
    tcsetattr(fd_serie, TCSANOW, &portSettings);

    /* Open music file */
    printf("open file %s begin\n", AUDIOFILE_NAME);
    fd_file = open (AUDIOFILE_NAME, O_RDWR);
    if (fd_file < 0) {
        perror("open: error opening file \n");
        exit(-1);
    }

    task_read_commands();
    // task_playback(fd_serie, fd_file, buf);
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
