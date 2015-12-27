/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdarg.h>
#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* perror FIXME */
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
	enum FnType  type;
	union {
		struct {
			void (*run)(void);
		} runnable;
		struct {
			void (*accept)(char *);
			char *t;
		} consumer;
		struct {
			void (*accept)(char *, const int);
			char *t;
			int  u;
		} biconsumer;
	} fn;
} *next_event;

void insert(struct Event *e);

/* public */

/** Constructor. (FIXME: have a pool of Events and draw from there.)
 @return	An object or a null pointer, if the object couldn't be created. */
int Event(const int delay_ms, enum FnType type, ...) {
	va_list args;
	struct Event *event, *last, *next;

	if(!(event = malloc(sizeof(struct Event)))) {
		perror("Event constructor");
		Event_(&event);
		return 0;
	}
	event->next            = 0;
	event->t_ms            = TimerLastTime() + delay_ms;
	event->type            = type;
	/* do something different based on what the type is */
	va_start(args, type);
	switch(type) {
		case FN_RUNNABLE:
			event->fn.runnable.run = va_arg(args, void (*)(void));
			break;
		case FN_CONSUMER:
			event->fn.consumer.accept = va_arg(args, void (*)(char *));
			event->fn.consumer.t      = va_arg(args, char *);
		case FN_BICONSUMER:
			event->fn.biconsumer.accept = va_arg(args, void (*)(char *, const int));
			event->fn.biconsumer.t      = va_arg(args, char *);
			event->fn.biconsumer.u      = va_arg(args, const int);
	}
	va_end(args);
	Pedantic("Event: new at %dms, #%p.\n", event->t_ms, (void *)event);

	/* FIXME: O(n) :[? it depends, if an event has an time expected value near
	 the beginning of the event queue, then this is actually good (probably the
	 inverse, have pointer to last; I envesion that this will be quite full) */
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

/** Dispach all events up to t_ms.
 FIXME: Aught memory alloc */
void EventDispatch(const int t_ms) {
	struct Event *e;
	while((e = next_event) && e->t_ms <= t_ms) {
		Pedantic("Event: event triggered at %dms.\n", e->t_ms);
		switch(e->type) {
			case FN_RUNNABLE:    e->fn.runnable.run(); break;
			case FN_CONSUMER:    e->fn.consumer.accept(e->fn.consumer.t); break;
			case FN_BICONSUMER:
				e->fn.biconsumer.accept(e->fn.biconsumer.t, e->fn.biconsumer.u);
				break;
		}
		next_event = e->next;
		Event_(&e);
	}
}

/** fixme: eventclear */
