#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "plcm_ioctl.h"

#define VERSION "1.0"
#define LINE_LENGTH 16
#define NUM_PAGES 7

#define CYCLE_TIME_SECONDS 5
#define REFRESH_TIME_SECONDS 10

void ShowMessage (int devfd, char *str1 , char *str2)
{
    printf("DISPLAY LINE 1: \"%s\"\n", str1);
    printf("DISPLAY LINE 2: \"%s\"\n", str2);
    ioctl(devfd, PLCM_IOCTL_SET_LINE, 1);
    write(devfd, str1, strlen(str1));
    ioctl(devfd, PLCM_IOCTL_SET_LINE, 2);
    write(devfd, str2, strlen(str2));
}

void getoutput(char* cmd, char* output, int outputlen)
{
    FILE *fp;
    int i;
    int status;

    /* Open the command for reading. */
    fp = popen(cmd, "r");
    if (fp == NULL) {
        fprintf( stderr,"Failed to run command\n" );
        return;
    }

    /* Read the output a line at a time - output it. */
    if (fgets(output, outputlen+1, fp) != NULL) {
        /* end string if control characters found */
        for ( i=0 ; i < outputlen ; i++ ) {
            if (iscntrl(output[i]))
                output[i] = 0;
        }
    }
    
    /* close */
    pclose(fp);

    return;
}

void showpage(int devfd, int page)
{
    char cmd1[255];
    char cmd2[255];
    char line1[LINE_LENGTH+1];
    char line2[LINE_LENGTH+1];

    bzero(cmd1,255);
    bzero(cmd2,255);
    bzero(line1,LINE_LENGTH+1);
    bzero(line2,LINE_LENGTH+1);

    snprintf(cmd1,255,"/usr/share/untangle-lcd/bin/ut-lcd-page %i %i",page,1);
    snprintf(cmd2,255,"/usr/share/untangle-lcd/bin/ut-lcd-page %i %i",page,2);

    getoutput(cmd1, line1, LINE_LENGTH);
    getoutput(cmd2, line2, LINE_LENGTH);
    ShowMessage(devfd, line1, line2);

    return;
}

int main(int argc, char* argv[]) {

    int i = 0;
    unsigned char Keypad_Value = 0, Pre_Value = 0, Counter = 0;
    unsigned char detect_press;
    unsigned char detect_dir;
    int devfd;
    unsigned char line1[16];
    unsigned char line2[16];

    int current_page = 0;
    time_t next_cycle_time = 0; /* next time that the page should be changed */
    time_t next_refresh_time = 0; /* next time that the page should be refreshed */
    int auto_cycling = 1;

    fprintf(stderr,"Untangle LANNERE LCD-server " VERSION"\n"); 

    devfd = open("/dev/plcm_drv", O_RDWR);
    if(devfd == -1)
    {
        printf("Can't open /dev/plcm_drv\n");
        return -1;
    }

    showpage(devfd, current_page);

    Pre_Value = ioctl(devfd, PLCM_IOCTL_GET_KEYPAD, 0);
    ioctl(devfd, PLCM_IOCTL_SET_LINE, 1);
    while (1) {            

        usleep(100000); //sleep .1 seconds

        /**
         * If auto-cycle is on and the next cycle time isn't computed
         * compute it.
         * Also set the next_refresh time if necessary
         */
        if (auto_cycling && next_cycle_time == 0) {
            next_cycle_time = time(NULL) + CYCLE_TIME_SECONDS;
        }
        if (next_refresh_time == 0) {
            next_refresh_time = time(NULL) + REFRESH_TIME_SECONDS;
        }

        /**
         * Read from the LCD
         */

        Keypad_Value = ioctl(devfd, PLCM_IOCTL_GET_KEYPAD, 0);

        if(Pre_Value != Keypad_Value)
        {
            detect_press=(Keypad_Value & 0x40);
            detect_dir=(Keypad_Value & 0x28);
            if (detect_press == 0x00) {
                switch(detect_dir){
                    case 0x00:
                    case 0x20:
                        current_page = ((current_page + 1) % NUM_PAGES);
                        showpage(devfd, current_page);
                        next_refresh_time = 0; next_cycle_time = 0; /* reset counters */
                        break;
    
                    case 0x08:
                    case 0x28:
                        current_page = ((current_page - 1) % NUM_PAGES);
                        if (current_page < 0) current_page = NUM_PAGES-1;
                        showpage(devfd, current_page);
                        next_refresh_time = 0; next_cycle_time = 0; /* reset counters */
                        break;
                    default:
                        /* otherwise just check the refresh time and continue */
                        if (time(NULL) >= next_refresh_time) {
                            //printf("REFRESH\n");
                            showpage(devfd, current_page);
                            next_refresh_time = 0;
                        }
                }
            }

            /*recalculate the timer after pushed button*/
            if (auto_cycling && next_cycle_time == 0) {
                next_cycle_time = time(NULL) + CYCLE_TIME_SECONDS;
            }
            if (next_refresh_time == 0) {
                next_refresh_time = time(NULL) + REFRESH_TIME_SECONDS;
            }

            Pre_Value = Keypad_Value;
        }
        usleep(100000); // 100 msec

        /**
         * Check auto-cycling settings,
         * If its on and the cycle time has passed, cycle the page
         */
        if (auto_cycling && (time(NULL) >= next_cycle_time)) {
            //printf("Auto-cycling current page...\n"); fflush(stdout);
            current_page = ((current_page + 1) % NUM_PAGES);
            showpage(devfd, current_page);
            next_cycle_time = 0; /* reset next cycle time */
        }
 
    }

    close(devfd);
    return 0;
}
