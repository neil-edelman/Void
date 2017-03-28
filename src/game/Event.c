/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt.

 Events have a specific time to call a function. Events uses a pool which is
 drawn upon and stuck in the linked-list.

 @title		Event
 @author	Neil
 @version	3.3; 2016-01
 @since		3.2; 2015-11 */

#include <stdio.h>  /* snprintf */
#include <stdarg.h>
#include <string.h> /* memcpy */
#include "../Print.h"
#include "../system/Timer.h"
#include "../general/Random.h"
#include "../general/Orcish.h"
#include "Event.h"

/* Q15.11: va_ macros don't like function-pointers */
typedef void (*Runnable)(void);
typedef void (*Consumer)(void *);
typedef void (*Biconsumer)(void *, void *);

static struct Event {
	struct Event *prev, *next;
	char         label[16];
	unsigned     t_ms;
	struct Event **notify;
	enum FnType  fn_type;
	union {
		struct {
			Runnable run;
		} runnable;
		struct {
			Consumer accept;
			void *t;
		} consumer;
		struct {
			Biconsumer accept;
			void *t;
			void *u;
		} biconsumer;
	} fn;
} events[2048], *first_event, *last_event, *iterator;
static const unsigned events_capacity = sizeof events / sizeof(struct Event);
static unsigned       events_size;

static struct Event *iterate(void);
char *decode_fn_type(const enum FnType fn_type);

/* public */

/** Constructor.
 @return	An object or a null pointer, if the object couldn't be created. */
int Event(struct Event **const event_ptr, const int delay_ms, const int sigma_ms, enum FnType fn_type, ...) {
	va_list args;
	struct Event *event, *prev, *next;
	int real_delay_ms;
	unsigned i;

	if(events_size >= events_capacity) {
		Warn("Event: couldn't be created; reached maximum of %u/%u.\n", events_size, events_capacity);
		return 0;
	}

	real_delay_ms = delay_ms;
	if(sigma_ms > 0) {
		/* FIXME: Truncated Guassian */
		real_delay_ms += RandomUniformInt(sigma_ms);
		if(real_delay_ms < 0) real_delay_ms = 0;
	}

	if(event_ptr && *event_ptr) {
		Warn("Event: would override %s on creation of %s %u!\n", EventToString(*event_ptr), decode_fn_type(fn_type), events_size);
		EventList();
		return 0;
	}

	event = &events[events_size++];

	event->prev    = event->next = 0;
	Orcish(event->label, sizeof event->label);
	event->t_ms    = TimerGetGameTime() + real_delay_ms;
	event->notify  = event_ptr;
	if(event_ptr) *event_ptr = event;
	event->fn_type = fn_type;
	/* do something different based on what the type is */
	va_start(args, fn_type);
	switch(fn_type) {
		case FN_RUNNABLE:
			event->fn.runnable.run = va_arg(args, Runnable);
			break;
		case FN_CONSUMER:
			event->fn.consumer.accept = va_arg(args, Consumer);
			event->fn.consumer.t      = va_arg(args, void *);
			break;
		case FN_BICONSUMER:
			event->fn.biconsumer.accept = va_arg(args, Biconsumer);
			event->fn.biconsumer.t      = va_arg(args, void *);
			event->fn.biconsumer.u      = va_arg(args, void *);
			break;
	}
	va_end(args);

	/* O(n), technically; however, new values have a higher likelihood of being
	 at the end. FIXME: gets screwed up 29.3 days in!!!! use TimerCompare() */
	/* FIXME: singly-linked!!! more events are happening soon? */
	for(prev = last_event, next = 0, i = 0;
		prev && prev->t_ms > event->t_ms;
		next = prev, prev = prev->prev, i++);
	if(prev) {
		event->next = prev->next;
		prev->next  = event;
	} else {
		event->next = first_event;
		first_event = event;
	}
	if(next) {
		event->prev = next->prev;
		next->prev  = event;
	} else {
		event->prev = last_event;
		last_event  = event;
	}

	Pedantic("Event: new %s at %dms inefficiency %u (size %u.)\n", EventToString(event), event->t_ms, i, events_size);

	return -1;
}

/** Destructor.
 @param event_ptr	A reference to the object that is to be deleted. */
void Event_(struct Event **event_ptr) {
	struct Event *event, *replace;
	unsigned idx;
	unsigned characters;
	char buffer[128];

	if(!event_ptr || !(event = *event_ptr)) return;
	idx = (unsigned)(event - events);
	if(idx >= events_size) {
		Warn("~Event: %s, %u not in range %u.\n", EventToString(event), idx + 1, events_size);
		EventList();
		return;
	}

	/* store the string for debug info */
	characters = snprintf(buffer, sizeof buffer, "%s", EventToString(event));

	/* update notify for delete */
	if(event->notify) {
		Pedantic("~Event: %s notify %s to 0.\n", EventToString(event), EventToString(*event->notify));
		*event->notify = 0;
	}

	/* take it out of the queue */
	if(event->prev) event->prev->next = event->next;
	else            first_event       = event->next;
	if(event->next) event->next->prev = event->prev;
	else            last_event        = event->prev;

	/* replace it with the last one */
	if(idx < --events_size) {

		replace = &events[events_size];
		memcpy(event, replace, sizeof(struct Event));

		/* insert it into the new position */
		if(event->prev) event->prev->next = event;
		else            first_event       = event;
		if(event->next) event->next->prev = event;
		else            last_event        = event;

		/* update notify */
		if(event->notify) *event->notify = event;

		if(characters < sizeof buffer) snprintf(buffer + characters, sizeof buffer - characters, "; replaced by %s", EventToString(event));
	}
	/* FIXME: this is not the same event */
	Pedantic("~Event: erase, %s size %u.\n", buffer, events_size);

	*event_ptr = event = 0;
}

/** Flush the Events without Dispatch according to predicate. */
void EventRemoveIf(int (*const predicate)(struct Event *const)) {
	struct Event *e;

	Debug("Event::removeIf: clearing Events.\n");
	while((e = iterate())) {
		Pedantic("Event::removeIf: consdering %s.\n", EventToString(e));
		if(!predicate || predicate(e)) Event_(&e);
	}
}

/** This updates the notify field when the thing you're notifying changes
 addresses. Takes a pointer. */
void EventSetNotify(struct Event **const e_ptr) {
	struct Event *e;

	if(!e_ptr || !(e = *e_ptr)) return;
	/* this is supposed to happen -- Sprites are moving around thus this pointer
	 has to update
	 if(e->notify) Warn("Event::setNotify: %s overriding a previous notification, %s!\n", EventToString(e), EventToString(*e->notify));*/
	e->notify = e_ptr;
}

/** Accessor. */
void (*EventGetConsumerAccept(const struct Event *const e))(void *) {
	if(!e || e->fn_type != FN_CONSUMER) return 0;
	return e->fn.consumer.accept;
}

/** Dispach all events up to the present. */
void EventDispatch(void) {
	struct Event *e;

	while((e = first_event) && TimerIsTime(e->t_ms)) {
		const enum FnType fn_type = e->fn_type;
		void (*runnable)(void) = 0;
		void (*consumer)(void *) = 0;
		void (*biconsumer)(void *, void *) = 0;
		void *t = 0, *u = 0;

		/* make a copy */
		switch(fn_type) {
			case FN_RUNNABLE:
				runnable = e->fn.runnable.run;
				break;
			case FN_CONSUMER:
				consumer = e->fn.consumer.accept;
				t = e->fn.consumer.t;
				break;
			case FN_BICONSUMER:
				biconsumer = e->fn.biconsumer.accept;
				t = e->fn.biconsumer.t;
				u = e->fn.biconsumer.u;
				break;
		}
		/* delete */
		Pedantic("\nEvent: %s triggered. First deleting; then %u events left.\n", EventToString(e), events_size - 1);
		Event_(&e);
		switch(fn_type) {
			case FN_RUNNABLE:	runnable();			break;
			case FN_CONSUMER:	consumer(t);		break;
			case FN_BICONSUMER:	biconsumer(t, u);	break;
		}
	}
}

/** Sometimes you just want to change your mind. */
void EventReplaceArguments(struct Event *const event, ...) {
	va_list args;

	if(!event) return;
	Pedantic("Event::replaceArguments: %s.\n", EventToString(event));

	va_start(args, event);
	switch(event->fn_type) {
		case FN_RUNNABLE:
			Warn("Event::replaceArguments: %s has no arguments.\n", EventToString(event));
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

/** Has room for four events at a time.
 @param e: Event turned into a string. */
char *EventToString(const struct Event *const e) {
	static int b;
	static char buffer[4][64];
	int last_b;

	if(!e) {
		snprintf(buffer[b], sizeof buffer[b], "%s", "null event");
	} else {
		/*snprintf(buffer[b], sizeof buffer[b], "%s%s[#%u %ums](%s,%s)", \
		 decode_fn_type(e->fn_type), e->label, (int)(e - events) + 1, e->t_ms, \
		 e->fn_type == FN_CONSUMER && e->fn.consumer.accept \
		 == (void (*)(void *))&ship_recharge ? "rcrg" : "not rcrg", \
		 e->fn_type == FN_CONSUMER && e->fn.consumer.accept == \
		 (void (*)(void *))&ship_recharge ? \
		 SpriteToString(e->fn.consumer.t) : "no");*/
		snprintf(buffer[b], sizeof buffer[b], "%s%s[#%u %ums]",
			decode_fn_type(e->fn_type), e->label, (int)(e - events) + 1,
			e->t_ms);
	};
	last_b = b;
	b = (b + 1) & 3;

	return buffer[last_b];
}

/** Prints the event list. */
void EventList(void) {
	struct Event *e;
	/*int i;*/
	Info("Events %ums {\n", TimerGetGameTime());
	for(e = first_event; e; e = e->next) {
		Info("\t%s\n", EventToString(e));
	}
	/*Debug("} or {\n");
	 for(i = 0; i < events_size; i++) {
	 Debug("\t%s\n", EventToString(events + i));
	 }*/
	Debug("}\n");
}

static struct Event *iterate(void) {
	if(!iterator) {
		iterator = first_event;
	} else {
		iterator = iterator->next;
	}
	return iterator;
}

char *decode_fn_type(const enum FnType fn_type) {
	switch(fn_type) {
		case FN_RUNNABLE:	return "<Runnable>";
		case FN_CONSUMER:	return "<Consumer>";
		case FN_BICONSUMER:	return "<Biconsumer>";
		default:			return "<not an event type>";
	}
}
