#include <stdio.h>
#include <stdlib.h>

#include "serial.h"
#include "command.h"
#include "icon.h"
#include "string.h"

#define VERSION "1.0"
#define LINE_LENGTH 16
#define NUM_PAGES 8

//com port. 0=COM1 or ttyS0
#define COMPORT 1 
#define BAUD 2400
#define NULSTR "                                       "

#define CYCLE_TIME_SECONDS 5
#define REFRESH_TIME_SECONDS 10

void ShowMessage (char *str1 , char *str2)
{
    int a,b;
    Cls();
    //printf("DISPLAY LINE 1: \"%s\"\n", str1);
    //printf("DISPLAY LINE 2: \"%s\"\n", str2);
    a = strlen(str1);
    b = 40 - a;
    write(fd,str1,a);
    write(fd,NULSTR,b);
    write(fd,str2,strlen(str2));
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
    fgets(output, outputlen+1, fp);

    /* end string if control characters found */
    for ( i=0 ; i < outputlen ; i++ ) {
        if (iscntrl(output[i]))
            output[i] = 0;
    }
    
    /* close */
    pclose(fp);

    return;
}

void showpage(int page)
{
    char cmd1[255];
    char cmd2[255];
    char line1[LINE_LENGTH+1];
    char line2[LINE_LENGTH+1];

    bzero(cmd1,255);
    bzero(cmd2,255);
    bzero(line1,LINE_LENGTH+1);
    bzero(line2,LINE_LENGTH+1);

    snprintf(cmd1,255,"/usr/share/untangle-portwell-lcd/bin/ut-lcd-page %i %i",page,1);
    snprintf(cmd2,255,"/usr/share/untangle-portwell-lcd/bin/ut-lcd-page %i %i",page,2);

    getoutput(cmd1, line1, LINE_LENGTH);
    getoutput(cmd2, line2, LINE_LENGTH);
    ShowMessage(line1, line2);

    return;
}

int main(int argc, char* argv[]) {

    fprintf(stderr,"Untangle EZIO LCD-server " VERSION"\n"); 
    Serial_Init(COMPORT, BAUD);  /* Initialize RS-232 environment */ 
    Init(); 			/* Initialize EZIO */
    Cls();			    /* Clear screen */
    init_all_icon();	/* Initialize all icon */

    unsigned char line1[16];
    unsigned char line2[16];

    int current_page = 0;
    int result;
    time_t next_cycle_time = 0; /* next time that the page should be changed */
    time_t next_refresh_time = 0; /* next time that the page should be refreshed */
    int auto_cycling = 1;
    unsigned char buf[255];

    showpage(current_page);

#if 0
    // the API doesn't appear to support NONBLOCK
    if (fcntl(fd,F_SETFL,O_NONBLOCK) < 0) {
        fprintf(stderr,"Failed to set NONBLOCK: %s\n",strerror(errno));
    }
#endif
    
    while (1) {			

        usleep(100000); //sleep .1 seconds
        bzero(buf,255);
        
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
        ReadKey(); 
        if ((result = read(fd,buf,255)) <= 0) /* read response from EZIO */ {
            if (result == 0) {
                fprintf(stderr, "EOF from LCD device. Exiting...");
                exit(1);
            }
            if (result < 0 && errno != EAGAIN) {
                fprintf(stderr, "Read error from LCD device: \"%s\" Exiting...", strerror(errno));
                exit(1);
            }

        }

        /**
         * Check auto-cycling settings,
         * If its on and the cycle time has passed, cycle the page
         */
        if (auto_cycling && (time(NULL) >= next_cycle_time)) {
            //printf("Auto-cycling current page...\n"); fflush(stdout);
            current_page = ((current_page + 1) % NUM_PAGES);
            showpage(current_page);
            next_cycle_time = 0; /* reset next cycle time */
        }

        /**
         * Process input from user
         */
        switch(buf[1]) {
	    case 0xbe: 	/* Up Botton was received */
            //printf("KEY: UP\n");
            current_page = ((current_page + 1) % NUM_PAGES);
            showpage(current_page);
            next_refresh_time = 0; next_cycle_time = 0; /* reset counters */
            break;
	 
        case 0xbd:	/* Down Botton was received */
            //printf("KEY: DOWN\n");
            current_page = ((current_page - 1) % NUM_PAGES);
            if (current_page < 0) current_page = NUM_PAGES-1;
            showpage(current_page);
            next_refresh_time = 0; next_cycle_time = 0; /* reset counters */
            break;
	 
        case 0xbb: /* Enter Botton was received */
            //printf("KEY: ENTER\n");
            auto_cycling = !auto_cycling; //toggle auto-cycle
            if (auto_cycling) {
                ShowMessage("** Auto-Cycle **","      ON        ");
                sleep(3);
            } else {
                ShowMessage("** Auto-Cycle **","      OFF       ");
                sleep(3);
            }
            showpage(current_page);
            next_refresh_time = 0; next_cycle_time = 0; /* reset counters */
            break;
	     
        case 0xb7: /* Escape Botton was received */
            //printf("KEY: ESC\n");
            current_page = 0;
            showpage(current_page);
            next_refresh_time = 0; next_cycle_time = 0; /* reset counters */
            break;
        default:
            /* otherwise just check the refresh time and continue */
            if (time(NULL) >= next_refresh_time) {
                //printf("REFRESH\n");
                showpage(current_page);
                next_refresh_time = 0;
            }
        } 
 
    }

    Uninit_Serial();
    return 0;
 
}
