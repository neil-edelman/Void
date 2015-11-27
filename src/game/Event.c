/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include "Event.h"

/* Events have a specific time to call a function.

 @author	Neil
 @version	1; 2015-11
 @since		1; 2015-11 */

struct Event {
	struct Event *next;
	int          t_ms;
	void         (*consumer)(int);
} *next_event;

void insert(struct Event *e);

/* public */

/** Constructor. (FIXME: have a pool of Events and draw from there.)
 FIXME: have a relative thing
 @return	An object or a null pointer, if the object couldn't be created. */
struct Event *Event(const int t_ms/*fixme: delay_ms*/, void (*consumer)(int)) {
	struct Event *event, *last, *next;

	if(!consumer) {
		fprintf(stderr, "Event: runnable not specified.\n");
		return 0;
	}
	if(!(event = malloc(sizeof(struct Event)))) {
		perror("Event constructor");
		Event_(&event);
		return 0;
	}
	event->next     = 0;
	event->t_ms     = t_ms;
	event->consumer = consumer;
	fprintf(stderr, "Event: new at %dms, #%p.\n", t_ms, (void *)event);

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
	
	return event;
}

/** Destructor.
 @param event_ptr	A reference to the object that is to be deleted. */
void Event_(struct Event **event_ptr) {
	struct Event *event;

	if(!event_ptr || !(event = *event_ptr)) return;
	fprintf(stderr, "~Event: erase, #%p.\n", (void *)event);
	free(event);
	*event_ptr = event = 0;
}

/** FIXME: Aught memory alloc */
void EventDispatch(const int t_ms) {
	struct Event *e;
	while((e = next_event) && e->t_ms <= t_ms) {
		fprintf(stderr, "Event: event triggered at %dms.\n", e->t_ms);
		e->consumer(t_ms);
		next_event = e->next;
		Event_(&e);
	}
}

/** fixme: eventclear */
