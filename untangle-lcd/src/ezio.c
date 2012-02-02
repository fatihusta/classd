#include <stdio.h>
#include <stdlib.h>

#include "serial.h"
#include "command.h"
#include "icon.h"
#include "string.h"

#define VERSION "1.0"
#define LINE_LENGTH 16
#define NUM_PAGES 2

//com port. 0=COM1 or ttyS0
#define COMPORT 1 
#define BAUD 2400
#define NULSTR "                                       "

void ShowMessage (char *str1 , char *str2)
{
    int a,b;
    printf("LINE1: \"%s\"\n", str1);
    printf("LINE2: \"%s\"\n", str2);
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
        printf("Failed to run command\n" );
        return;
    }

    /* Read the output a line at a time - output it. */
    fgets(output, outputlen-1, fp);

    /* end string if non-alpha-numberic characters found */
    for ( i=0 ; i < outputlen ; i++ ) {
        if (!isalnum(output[i]))
            output[i] = 0;
    }
    
    /* if its a short string center it */
    if (strlen(output)<outputlen) {
        int spacing_len = (outputlen - strlen(output)) / 2;
        {
            printf("spacing_len: %i outputlen: %i\n",spacing_len, outputlen);
            char space[spacing_len+1];
            char orig[outputlen];
            bzero(space,spacing_len+1);
            memset(space,' ',spacing_len);
            strcpy(orig, output);
            
            printf("short string: \"%s\"\n", output);
            printf("centered string: \"%s%s%s\"\n", space, output, space);
            snprintf(output, outputlen, "%s%s%s", space, orig, space);
            printf("output: \"%s\"\n",output);
        }
    }
        
    /* close */
    pclose(fp);

    return;
}

void showpage(int page)
{
    char line1[LINE_LENGTH+1];
    char line2[LINE_LENGTH+1];
    bzero(&line1,LINE_LENGTH+1);
    bzero(&line2,LINE_LENGTH+1);
    printf("Showing page: %i\n",page);
    
    Cls();
    switch (page) {
    case 0:
        strncpy(line1,"*** Untangle ***",LINE_LENGTH);
        getoutput("hostname -s", line2, LINE_LENGTH);
        ShowMessage(line1, line2);
        break;
    case 1:
        strncpy(line1,"***   Load   ***",LINE_LENGTH);
        getoutput("cat /proc/loadavg", line2, LINE_LENGTH);
        ShowMessage(line1, line2);
        break;
    }

    return;
}

int main(int argc, char* argv[]) {

    fprintf(stderr,"Untangle EZIO LCD-server " VERSION"\n"); 
    Serial_Init(COMPORT, BAUD);  /* Initialize RS-232 environment */ 
    Init(); 			/* Initialize EZIO */
    Cls();			/* Clear screen */
    init_all_icon();		/* Initialize all icon */

    unsigned char line1[16];
    unsigned char line2[16];

    int current_page = 0;
    showpage(current_page);
   
    while (1) {			
        int res;
        unsigned char buf[255];
     
        ReadKey(); /* sub-routine to send "read key" command */
        res = read(fd,buf,255); /* read response from EZIO */

        switch(buf[1]) {
	    case 0xbe: 	/* Up Botton was received */
            current_page = ((current_page + 1) % NUM_PAGES);
            showpage(current_page);
            break;
	 
        case 0xbd:	/* Down Botton was received */
            current_page = ((current_page - 1) % NUM_PAGES);
            showpage(current_page);
            break;
	 
        case 0xbb: /* Enter Botton was received */
            current_page = 1;
            showpage(current_page);
            break;
	     
        case 0xb7: /* Escape Botton was received */
            current_page = 1;
            showpage(current_page);
            break;
        } 
 
    }

    printf("Done.\n\n");
    Uninit_Serial();
    return 0;
 
}
