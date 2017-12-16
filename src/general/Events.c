/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Use when you want to call a function, but not right away.

 @title		Event
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites.
			2016-01
			2015-11 */

#include <stdio.h> /* perror fprintf stderr */
#include <assert.h>
#include "../system/Timer.h"
#include "Events.h"



/*************** Declare types. ****************/

/** This is one polymorphic {Event}. */
struct Event;
struct EventVt;
struct Events;

struct Event { const struct EventVt *vt; };
#define LIST_NAME Event
#define LIST_TYPE struct Event
#include "../templates/List.h"

/** {Runnable} is an {Event}. */
struct Runnable {
	struct EventListNode event;
	Runnable run;
};
#define POOL_NAME Runnable
#define POOL_TYPE struct Runnable
#define POOL_MIGRATE struct Events
#include "../templates/Pool.h"

/** {IntConsumer} is an {Event}. */
struct IntConsumer {
	struct EventListNode event;
	IntConsumer accept;
	int param;
};
#define POOL_NAME IntConsumer
#define POOL_TYPE struct IntConsumer
#define POOL_MIGRATE struct Events
#include "../templates/Pool.h"

/** {SpriteConsumer} is an {Event}. */
struct SpriteConsumer {
	struct EventListNode event;
	SpriteConsumer accept;
	struct Sprite *param;
};
#define POOL_NAME SpriteConsumer
#define POOL_TYPE struct SpriteConsumer
#define POOL_MIGRATE struct Events
#include "../templates/Pool.h"

/** Events are fit to various bins depending on the length of the delay. Longer
 delays will be more granular and less-resolution. */
static struct Events {
	unsigned update; /* time */
	struct EventList immediate;
	struct EventList approx1s[8];
	struct EventList approx8s[8];
	struct EventList approx64s[8];
	struct RunnablePool *runnables;
	struct IntConsumerPool *int_consumers;
	struct SpriteConsumerPool *sprite_consumers;
} *events;

static const size_t approx1s_size = sizeof((struct Events *)0)->approx1s
	/ sizeof *((struct Events *)0)->approx1s;
static const size_t approx8s_size = sizeof((struct Events *)0)->approx8s
	/ sizeof *((struct Events *)0)->approx8s;
static const size_t approx64s_size = sizeof((struct Events *)0)->approx64s
	/ sizeof *((struct Events *)0)->approx64s;


/******************* Define virtual functions. ********************/

typedef void (*EventsAction)(struct Event *const);

struct EventVt { EventsAction call; };

/** This is only called from {run_event_list} as the event list must be purged
 because the backing in the {Pool}s has been removed; important!
 @implements <Event>Action */
static void event_call(struct Event *const this) {
	assert(events && this);
	this->vt->call(this);
}
/** @implements <Runnable>Action */
static void runnable_call(struct Runnable *const this) {
	this->run();
	RunnablePoolRemove(events->runnables, this);
}
/** @implements <IntConsumer>Action */
static void int_consumer_call(struct IntConsumer *const this) {
	this->accept(this->param);
	IntConsumerPoolRemove(events->int_consumers, this);
}
/** @implements <SpriteConsumer>Action */
static void sprite_consumer_call(struct SpriteConsumer *const this) {
	this->accept(this->param);
	SpriteConsumerPoolRemove(events->sprite_consumers, this);
}

static const struct EventVt
	runnable_vt        = { (EventsAction)&runnable_call },
	int_consumer_vt    = { (EventsAction)&int_consumer_call },
	sprite_consumer_vt = { (EventsAction)&sprite_consumer_call };



/******************** Type functions. ******************/

/** Used in {Events_}, {Events}, and {EventsClear}. */
static void clear_event_lists(void) {
	unsigned i;
	assert(events);
	EventListClear(&events->immediate);
	for(i = 0; i < approx1s_size; i++)
		EventListClear(events->approx1s + i);
	for(i = 0; i < approx8s_size; i++)
		EventListClear(events->approx8s + i);
	for(i = 0; i < approx64s_size; i++)
		EventListClear(events->approx64s + i);
}

/** Used in {EventsRemoveIf}. */
/*static void clear_event_lists_predicate(const EventsPredicate predicate) {
	unsigned i;
	assert(events);
	EventListTakeIf(0, &events->immediate, predicate);
	for(i = 0; i < sizeof events->approx1s / sizeof *events->approx1s; i++)
		EventListTakeIf(0, events->approx1s + i, predicate);
	for(i = 0; i < sizeof events->approx8s / sizeof *events->approx8s; i++)
		EventListTakeIf(0, events->approx8s + i, predicate);
	for(i = 0; i < sizeof events->approx64s / sizeof *events->approx64s; i++)
		EventListTakeIf(0, events->approx64s + i, predicate);
} <- fixme: leaves Pool!! */

/** @implements <Events>Migrate */
static void events_migrate(struct Events *const e, const struct Migrate *const migrate) {
	unsigned i;
	assert(e && e == events && migrate);
	EventListMigrate(&e->immediate, migrate);
	for(i = 0; i < approx1s_size; i++)
		EventListMigrate(e->approx1s + i, migrate);
	for(i = 0; i < approx8s_size; i++)
		EventListMigrate(e->approx8s + i, migrate);
	for(i = 0; i < approx64s_size; i++)
		EventListMigrate(e->approx64s + i, migrate);
}

/** Destructor. */
void Events_(void) {
	if(!events) return;
	clear_event_lists();
	SpriteConsumerPool_(&events->sprite_consumers);
	IntConsumerPool_(&events->int_consumers);
	RunnablePool_(&events->runnables);
	free(events), events = 0;
}

/** @return True if the Events pool has been set up. */
int Events(void) {
	const char *e = 0;
	if(events) return 1;
	if(!(events = malloc(sizeof *events)))
		{ perror("Events"); Events_(); return 0; }
	events->update = TimerGetGameTime();
	clear_event_lists();
	events->runnables = 0;
	events->int_consumers = 0;
	events->sprite_consumers = 0;
	do {
		if(!(events->runnables = RunnablePool(&events_migrate, events)))
			{ e = RunnablePoolGetError(events->runnables); break; }
		if(!(events->int_consumers = IntConsumerPool(&events_migrate, events)))
			{ e = IntConsumerPoolGetError(events->int_consumers); break; }
		if(!(events->sprite_consumers=SpriteConsumerPool(&events_migrate,
			events)))
			{ e = SpriteConsumerPoolGetError(events->sprite_consumers); break; }
	} while(0); if(e) {
		fprintf(stderr, "Events: %s.\n", e); Events_(); return 0;
	}
	return 1;
}



/*************** Sub-type constructors. ******************/

/** Fits into a bin. */
static struct EventList *fit_future(struct Events *const this,
	const unsigned ms_future) {
	const unsigned ms_now = TimerGetGameTime(), ms = ms_now + ms_future;
	unsigned select;
	struct EventList *list;
	assert(this);
	if(ms_future < 0x200) {
		list = &this->immediate;
		select = 0;
	} else if(ms_future < 0x2200) {
		list = this->approx1s;
		select = ((ms >> 10) & 7) + ((ms & (0x400 - 1)) > 0x200) ? 1 : 0;
	} else if(ms_future < 0x11000) {
		list = this->approx8s;
		select = ((ms >> 13) & 7) + ((ms & (0x2000 - 1)) > 0x1000) ? 1 : 0;
	} else if(ms_future < 0x88000) {
		list = this->approx64s;
		select = ((ms >> 16) & 7) + ((ms & (0x10000 - 1)) > 0x8000) ? 1 : 0;
	} else { /* Maximum delay is, \${1.024s * 64 * 8 * (min/60s) <= 8.74min} */
		list = this->approx64s;
		select = ((ms_now >> 16) + 7) & 7;
	}
	assert(select < 8);
	return list + select;
}

/** Abstract {Event} constructor. */
static void event_filler(struct Event *const this,
	const unsigned ms_future, const struct EventVt *const vt) {
	assert(events && this && vt);
	this->vt = vt;
	EventListPush(fit_future(events, ms_future), this);
}
/** Creates a new {Runnable}. */
int EventsRunnable(const unsigned ms_future, const Runnable run) {
	struct Runnable *this;
	if(!events || !run) return 0;
	if(!(this = RunnablePoolNew(events->runnables)))
		{ fprintf(stderr, "EventsRunnable: %s.\n",
		RunnablePoolGetError(events->runnables)); return 0; }
	this->run = run;
	event_filler(&this->event.data, ms_future, &runnable_vt);
	return 1;
}
/** Creates a new {SpriteConsumer}. */
int EventsSpriteConsumer(const unsigned ms_future,
	const SpriteConsumer accept, struct Sprite *const param) {
	struct SpriteConsumer *this;
	if(!events || !accept) return 0;
	if(!(this = SpriteConsumerPoolNew(events->sprite_consumers)))
		{ fprintf(stderr, "EventsSpriteConsumer: %s.\n",
		SpriteConsumerPoolGetError(events->sprite_consumers)); return 0; }
	this->accept = accept;
	this->param  = param;
	event_filler(&this->event.data, ms_future, &sprite_consumer_vt);
	return 1;
}



/** Clears all events. */
void EventsClear(void) {
	if(!events) return;
	clear_event_lists();
	RunnablePoolClear(events->runnables);
	IntConsumerPoolClear(events->int_consumers);
	SpriteConsumerPoolClear(events->sprite_consumers);
}

/*void EventsRemoveIf(const EventsPredicate predicate) {
	if(!events) return;
	clear_event_lists_predicate(predicate);
}*/

static void run_event_list(struct EventList *const list) {
	assert(events && list);
	EventListForEach(list, &event_call);
	/* {event_call} strips away their backing, so this call is important! */
	EventListClear(list);
}

/** Fire off {Events} that have happened. */
void EventsUpdate(void) {
	struct EventList *chosen;
	struct { unsigned cur, end; int done; } t1s, t8s, t64s;
	const unsigned now = TimerGetGameTime();
	enum { T64S, T8S, T1S } granularity = T64S;
	if(!events) return;
	/* Always run events that are immediate asap. */
	run_event_list(&events->immediate);
	/* Applies the timer with a granularity of whatever; all are [0, 7].
	 fixme: Meh, could be better. */
	t64s.cur = (events->update >> 16) & 7;
	t64s.end = (now >> 16) & 7;
	t8s.cur  = (events->update >> 13) & 7;
	t8s.end  = (now >> 13) & 7;
	t1s.cur  = (events->update >> 10) & 7;
	t1s.end  = (now >> 10) & 7;
	do {
		switch(granularity) {
			case T64S: t64s.done = (t64s.cur == t64s.end);
			case T8S:  t8s.done  = t64s.done && (t8s.cur == t8s.end);
			case T1S:  t1s.done  = t8s.done && (t1s.cur == t1s.end);
		}
		if(t1s.cur < 8) {
			chosen = events->approx1s + t1s.cur++;
			granularity = T1S;
		} else {
			t1s.cur = 0;
			if(t8s.cur != 8) {
				chosen = events->approx8s + t8s.cur++;
				granularity = T8S;
			} else {
				t8s.cur = 0;
				chosen = events->approx64s + t64s.cur++;
				t64s.cur &= 7;
				granularity = T64S;
			}
		}
		run_event_list(chosen);
	} while(!t1s.done);
}
