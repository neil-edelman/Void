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
	unsigned     t_ms;
	enum FnType  type;
	union {
		struct {
			void (*run)(void);
		} runnable;
		struct {
			void (*accept)(void *);
			void *t;
		} consumer;
		struct {
			void (*accept)(void *, void *);
			void *t;
			void *u;
		} biconsumer;
	} fn;
} *next_event/*, events pool */;

/*void print_all_events(void);*/

/* public */

/** Constructor. (FIXME: have a pool of Events and draw from there; this will
 requre a second level of indirection.)
 fixme: this crashes on loop, more careful (TIMER!)
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
	event->t_ms            = TimerGetGameTime() + delay_ms;
	event->type            = type;
	/* do something different based on what the type is */
	va_start(args, type);
	switch(type) {
		case FN_RUNNABLE:
			event->fn.runnable.run = va_arg(args, void (*)(void));
			break;
		case FN_CONSUMER:
			event->fn.consumer.accept = va_arg(args, void (*)(void *));
			event->fn.consumer.t      = va_arg(args, void *);
			break;
		case FN_BICONSUMER:
			event->fn.biconsumer.accept = va_arg(args, void (*)(void *, void *));
			event->fn.biconsumer.t      = va_arg(args, void *);
			event->fn.biconsumer.u      = va_arg(args, void *);
			break;
	}
	va_end(args);
	Pedantic("Event: new at %dms, #%p.\n", event->t_ms, (void *)event);

	/* FIXME: O(n) :[? it depends, if an event has an time expected value near
	 the beginning of the event queue, then this is actually good (probably the
	 inverse, have pointer to last; I envision that this will be quite full)
	 probably good to bsearch */
	for(last = 0, next = next_event;
		next && next->t_ms < event->t_ms;
		last = next, next = next->next);
	if(last) {
		event->next = last->next;
		last->next  = event;
	} else {
		event->next = next_event;
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
	/* Void(9908,0x7fff70b6ecc0) malloc: *** error for object 0x100785350: pointer being freed was not allocated
	 *** set a breakpoint in malloc_error_break to debug */
	//free(event);
	*event_ptr = event = 0;
}

/** Flush the Events without Dispatch. */
void EventClear(void) {
	struct Event *erase;

	Debug("Event::clear: clearing Events.\n");
	while(next_event) {
		erase = next_event;
		next_event = next_event->next;
		Event_(&erase);
	}
}

/** Dispach all events up to the present.
 fixme: wrap-around, :[
 FIXME: Aught memory alloc */
void EventDispatch(void) {
	struct Event *e;
	const unsigned t_ms = TimerGetGameTime();
	while((e = next_event) && e->t_ms <= t_ms) {
		Pedantic("Event: event triggered at %ums.\n", e->t_ms);
		switch(e->type) {
			case FN_RUNNABLE:    e->fn.runnable.run(); break;
			case FN_CONSUMER:    e->fn.consumer.accept(e->fn.consumer.t); break;
			case FN_BICONSUMER:
				e->fn.biconsumer.accept(e->fn.biconsumer.t, e->fn.biconsumer.u);
				break;
		}
		next_event = e->next;
		Event_(&e);
		/*print_all_events();*/
	}
}

/*void print_all_events(void) {
	struct Event *e;
	Debug("{\n");
	for(e = next_event; e; e = e->next) {
		Debug("Event at %u ms.\n", e->t_ms);
	}
	Debug("}\n");
}*/
