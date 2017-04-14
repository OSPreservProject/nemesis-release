#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ecs.h>
#include "buttons_gui.h"
#include <NashMod.ih>
#include <IDCMacros.h>
typedef struct {
    buttons_gui_st buttons_data;
    Nash_clp nash;
} launcher_st;

int main(int argc, char *argv[]) {
    launcher_st *st;
    
    st = calloc(sizeof(*st),1);

    buttons_init(st, &(st->buttons_data));
    
    st->nash = NashMod$New(NAME_FIND("modules>NashMod", NashMod_clp));
#define NASH(_str) Nash$ExecuteString(st->nash, _str);
    NASH("source profile.nash");
    NASH("inherit -out -err");

    buttons_handle_events_thread(&(st->buttons_data));
    return 0;
}
    
void test_invoke(void *data, WS_Event *ev) {
    launcher_st *st = (launcher_st*)data;
    int button;
    int safety_off;

    safety_off = 0;
    if (SET_IN(ev->d.u.Mouse.buttons, WS_Button_Middle)) safety_off = 1;
    button = ev->d.u.Mouse.x / 32;
    printf("button press %d\n", button);
    switch (button) {
    case 0: /* WS nash */
	NASH("!run modules>WSnash env>nash");
	break;
    case 1: /* qosbars */
	NASH("qosbars");
	break;
    case 2: /* reboot */
	if (safety_off) NASH("reboot");
	break;
    case 3:  /* chail load */
	if (safety_off) NASH("b");
	break;
    case 4: /* carnage */
	NASH("carnage");
	break;
    case 5: /* timidity */
	NASH("timidity");
	break;
    case 6: /* rvideo/scamper */
	NASH("rvideo");
	break;
    case 7:
	NASH("amp bond.mp3");
	break;
    case 8:
	NASH("really");
	NASH("aumixer");
	break;
    case 9:
	NASH("source doom1");
	break;
    case 10:
	NASH("tone");
	break;
    case 11:
	NASH("duck");
	break;
    case 12:
	NASH("scamper");
	break;
    case 13:
	NASH("java");
	break;
    case 14: /* toy story; ie measure xjd */
	NASH("xjd");
	break;
    case 15: /* qosmeasure */
	NASH("qosmeasure");
	break;
    case 16: /* xmarkov */
	NASH("xmarkov");
	break;
    case 28:
	NASH("ide");
	break;
    case 29:
	NASH("really");
	NASH("lksound");
	break;
    case 30:
	NASH("really");
	NASH("ncr");
	break;
    case 31:
	NASH("really");
	NASH("oppo");
	NASH("nicstar");
	break;
    default:
	break;
    }	
}
