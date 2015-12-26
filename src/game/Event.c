/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include "../Print.h"
#include "../system/Timer.h"
#include "Event.h"

/* Events have a specific time to call a function.

 @author	Neil
 @version	1; 2015-11
 @since		1; 2015-11 */

struct Event {
	struct Event *next;
	int          t_ms;
	void         (*runnable)(void);
} *next_event;

void insert(struct Event *e);

/* public */

/** Constructor. (FIXME: have a pool of Events and draw from there.)
 @return	An object or a null pointer, if the object couldn't be created. */
int Event(const int delay_ms, void (*runnable)(void)) {
	struct Event *event, *last, *next;

	if(!runnable) {
		fprintf(stderr, "Event: runnable not specified.\n");
		return 0;
	}
	if(!(event = malloc(sizeof(struct Event)))) {
		perror("Event constructor");
		Event_(&event);
		return 0;
	}
	event->next     = 0;
	event->t_ms     = TimerLastTime() + delay_ms;
	event->runnable = runnable;
	Pedantic("Event: new at %dms, #%p.\n", event->t_ms, (void *)event);

	/* FIXME: O(n) :[ */
	for(last = 0, next = next_event;
		next && next->t_ms < event->t_ms;
		last = next, next = next->next);
	if(last) {
		event->next = last->next;
		last->next  = event;
	} else {
		next_event  = event;
	}

	return -1;
}

/** Destructor.
 @param event_ptr	A reference to the object that is to be deleted. */
void Event_(struct Event **event_ptr) {
	struct Event *event;

	if(!event_ptr || !(event = *event_ptr)) return;
	Pedantic("~Event: erase, #%p.\n", (void *)event);
	free(event);
	*event_ptr = event = 0;
}

/** FIXME: Aught memory alloc */
void EventDispatch(const int t_ms) {
	struct Event *e;
	while((e = next_event) && e->t_ms <= t_ms) {
		Pedantic("Event: event triggered at %dms.\n", e->t_ms);
		e->runnable();
		next_event = e->next;
		Event_(&e);
	}
}

/** fixme: eventclear */
