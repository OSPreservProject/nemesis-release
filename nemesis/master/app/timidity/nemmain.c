#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FSDir.ih>
#include <time.h>
#include <IDCMacros.h>
#include "config.h"
#include "output.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "controls.h"
#include <exit.h>
#include <bstring.h>

int read_config_file(char *name);
void init_tables(void);

#undef TIMDIR
#define TIMDIR "timidity-data" /* XXX */


char *strtok(char *s, const char *delim) {
    static char *strtokptr = 0;
    char *r;
    int i;
    if (s) { 
	strtokptr = s;
    }
    do {
	r=strsep(&strtokptr, delim);
	if (r) {
	    i = strspn(r, delim);
	    r+=i;
	}
    } while ((!r || !*r) && strtokptr && *strtokptr );
    return r;
}

char * readfile(char *buf, int bufsize, FILE *fp) {
    char ch;
    char *bufptr;
    ch = 0;
    bufptr = buf;
#undef EOF
    while (bufsize>1 && ch != '\n') {
	if (Rd$EOF(fp->rd)) return 0;
	ch = Rd$GetC(fp->rd);
	if (ch != '\n') {
	    *(buf++) = ch;
	    bufsize--;
	}
    }
    *(buf) = 0;
    return bufptr;
}
#define LOTS
#ifdef LOTS
char * playinglist[] = { 
"midi/impossible.mid", "midi/magne2.mid", "midi/Crazy.mid", "midi/mars.mid", "midi/Desire.mid", "midi/Summer_of_69.mid", "midi/Mysterious_ways.mid","midi/Final_countdown.mid","muppets.mid", "midi/THUNDERB.MID","midi/bludanub.mid"  };

#define NUMFILES 11
#else
char *playinglist[] = { "midi/Mission_impossible.mid","midi/bludanub.mid" };
#define NUMFILES 2

#endif

extern FILE *infp, *outfp;
int main(int argc, char *argv[])
{
    extern PlayMode     nemesis_play_mode;
    extern ControlMode  dumb_control_mode;
    FSDir_clp           pwd_dir;
     
    pwd_dir = NAME_FIND("fs>pwd", FSDir_clp);
    if(FSDir$Lookup(pwd_dir, TIMDIR, True) != FSTypes_RC_OK)
    {
	printf("%s: failed to lookup \"%s\"\n", argv[0], TIMDIR);
	exit(EXIT_FAILURE);
    }    
    if(FSDir$CWD(pwd_dir) != FSTypes_RC_OK)
    {
	printf("%s: failed to CWD to \"%s\".\n", argv[0], TIMDIR);	
	exit(EXIT_FAILURE);
    }

    printf("PWD is %s\n", TIMDIR);
    infp = stdout;
    outfp = stderr;

    nemesis_play_mode.rate = DEFAULT_RATE;

    printf("Reading config file\n");

    read_config_file("timidity.cfg");

    init_tables();


      if (!control_ratio)
	{
	  control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
	  if(control_ratio<1)
		 control_ratio=1;
	  else if (control_ratio > MAX_CONTROL_RATIO)
		 control_ratio=MAX_CONTROL_RATIO;
	}
    
    nemesis_play_mode.open_output();


     {
	int i;
	int got;
	char *array[64];
	FILE *t;
	got = 0;
	for (i=1; i<argc; i++) {
	    printf("arg %d is [%s]\n", i, argv[i]);
	    if ((t = fopen(argv[1],"r"))) {
		array[got] = argv[1];
		got ++;
		fclose(t);
		printf("Found at .\n");
	    } else {
		char altstr[128];
		sprintf(altstr,"midi/%s", argv[i]);
		if ((t = fopen(altstr, "r"))) {
		    array[got] = strdup(altstr);
		    got++;
		    fclose(t);
		    printf("Found at midi\n");
		}
	    }
	}
	if (got > 0) {
	    dumb_control_mode.pass_playing_list( got, array);
	} else {
	    dumb_control_mode.pass_playing_list( NUMFILES, playinglist);
	}
     }
     exit(EXIT_SUCCESS);
}
