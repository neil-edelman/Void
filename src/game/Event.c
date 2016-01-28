/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdarg.h>
#include <string.h> /* memcpy */
#include "../Print.h"
#include "../system/Timer.h"
#include "../general/Random.h"
#include "Event.h"

/* Events have a specific time to call a function. Events uses a pool which is
 drawn upon and stuck in the linked-list.

 @author	Neil
 @version	3.3; 2016-01
 @since		3.2; 2015-11 */

static struct Event {
	struct Event *prev, *next;
	unsigned     t_ms;
	/*struct Event **notify; useless! */
	enum FnType  fn_type;
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
	char z;
} events[2048], *first_event, *last_event;
static const int events_capacity = sizeof events / sizeof(struct Event);
static int       events_size;

void print_all_events(void);

/* public */

/** Constructor.
 @return	An object or a null pointer, if the object couldn't be created. */
int Event(const char z, const int delay_ms, const int sigma_ms, enum FnType fn_type, ...) {
	va_list args;
	struct Event *event, *prev, *next;
	int real_delay_ms;
	unsigned i;

	if(events_size >= events_capacity) {
		Warn("Event: couldn't be created; reached maximum of %u.\n", events_capacity);
		return 0;
	}

	real_delay_ms = delay_ms;
	if(sigma_ms > 0) {
		/* FIXME: Truncated Guassian */
		real_delay_ms += RandomUniformInt(sigma_ms);
		if(real_delay_ms < 0) real_delay_ms = 0;
	}

	event = &events[events_size++];

	event->prev    = event->next = 0;
	event->t_ms    = TimerGetGameTime() + real_delay_ms;
	/*event->notify  = 0;*/
	event->z       = z;
	event->fn_type = fn_type;
	/* do something different based on what the type is */
	va_start(args, fn_type);
	switch(fn_type) {
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

	/* O(n), technically; however, new values have a higher likelihood of being
	 at the end. FIXME: gets screwed up 29.3 days in!!!! use TimerCompare() */
	for(prev = last_event, next = 0, i = 0;
		prev && prev->t_ms > event->t_ms;
		next = prev, prev = prev->prev, i++);
	if(prev) {
		event->next = prev->next;
		prev->next  = event;
	} else {
		event->next = first_event;
		first_event  = event;
	}
	if(next) {
		event->prev = next->prev;
		next->prev  = event;
	} else {
		event->prev = last_event;
		last_event  = event;
	}

	Debug("Event: new at %dms inefficiency %u %c (size %u).\n", event->t_ms, i, event->z, events_size);

	return -1/*event*/;
}

/** Destructor.
 @param event_ptr	A reference to the object that is to be deleted. */
void Event_(struct Event **event_ptr) {
	struct Event *event, *replace;
	int index;

	if(!event_ptr || !(event = *event_ptr)) return;
	index = event - events;
	if(index < 0 || index >= events_size) {
		Warn("~Event: %u not in range %u (%c).\n", index + 1, events_size, event->z);
		print_all_events();
		return;
	}

	/* update notify */
	/*if(event->notify) *event->notify = 0;*/

	/* take it out of the queue */
	if(event->prev) event->prev->next = event->next;
	else            first_event       = event->next;
	if(event->next) event->next->prev = event->prev;
	else            last_event        = event->prev;

	/* replace it with the last one */
	if(index < --events_size) {

		replace = &events[events_size];
		memcpy(event, replace, sizeof(struct Event));

		/* insert it into the new position */
		if(event->prev) event->prev->next = event;
		else            first_event       = event;
		if(event->next) event->next->prev = event;
		else            last_event        = event;

		/* update notify */
		/*if(event->notify) *event->notify = event;*/
	}
	Debug("~Event: erase, %c size %u.\n", event->z, events_size);

	*event_ptr = event = 0;
}

/** Flush the Events without Dispatch. */
void EventClear(void) {
	Debug("Event::clear: clearing Events.\n");
	first_event = last_event = 0;
}

/*void EventSetNotify(struct Event **const e_ptr) {
	struct Event *e;

	if(!e_ptr || !(e = *e_ptr)) return;
	if(e->notify) Warn("Event::setNotify: #%p overriding previous notification #%p.\n", (void *)e_ptr, (void *)e->notify);
	Pedantic("Event::setNotify: Event %c set #%p.\n", e->z, (void *)e_ptr);
	e->notify = e_ptr;
}*/

/** Dispach all events up to the present. */
void EventDispatch(void) {
	struct Event *e;

	while((e = first_event) && TimerIsTime(e->t_ms)) {
		/* switch off notification so that the event can recurse without
		 overriding the notify field; it's useless once it's been triggered */
		/*e->notify = 0;*/
		Pedantic("Event: event %c triggered at %ums.\n", e->z, e->t_ms);
		switch(e->fn_type) {
			case FN_RUNNABLE:    e->fn.runnable.run(); break;
			case FN_CONSUMER:    e->fn.consumer.accept(e->fn.consumer.t); break;
			case FN_BICONSUMER:
				e->fn.biconsumer.accept(e->fn.biconsumer.t, e->fn.biconsumer.u);
				break;
		}
		Event_(&e);
	}
}

/** Sometimes you just want to change your mind. */
void EventReplaceArguments(struct Event *const event, ...) {
	va_list args;

	if(!event) return;

	va_start(args, event);
	switch(event->fn_type) {
		case FN_RUNNABLE:
			break;
		case FN_CONSUMER:
			event->fn.consumer.t   = va_arg(args, void *);
			break;
		case FN_BICONSUMER:
			event->fn.biconsumer.t = va_arg(args, void *);
			event->fn.biconsumer.u = va_arg(args, void *);
			break;
	}
	va_end(args);
}

void print_all_events(void) {
	struct Event *e;
	Debug("%d {\n", TimerGetGameTime());
	for(e = first_event; e; e = e->next) {
		Debug("Event %c at %ums.\n", e->z, e->t_ms);
	}
	Debug("}\n");
}
