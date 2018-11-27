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

#include <assert.h>
#include "../system/Timer.h"
#include "Events.h"



struct EventVt;

/** Abstract {Event}. */
struct Event { const struct EventVt *vt; };
#define LIST_NAME Event
#define LIST_TYPE struct Event
#include "../templates/List.h"

/** {Runnable} extends {Event}. */
struct Runnable {
	struct EventLink base;
	Runnable run;
};
static void runnable_migrate(struct Runnable *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	EventLinkMigrate(&this->base.data, migrate);
}
#define POOL_NAME Runnable
#define POOL_TYPE struct Runnable
#define POOL_MIGRATE_EACH &runnable_migrate
#include "../templates/Pool.h"

/** {IntConsumer} extends {Event}. */
struct IntConsumer {
	struct EventLink base;
	IntConsumer accept;
	int param;
};
static void int_consumer_migrate(struct IntConsumer *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	EventLinkMigrate(&this->base.data, migrate);
}
#define POOL_NAME IntConsumer
#define POOL_TYPE struct IntConsumer
#define POOL_MIGRATE_EACH &int_consumer_migrate
#include "../templates/Pool.h"

/** {ItemConsumer} extends {Event}. */
struct ItemConsumer {
	struct EventLink base;
	ItemConsumer accept;
	struct Item *param;
};
static void item_consumer_migrate(struct ItemConsumer *const this,
	const struct Migrate *const migrate) {
	assert(this && migrate);
	EventLinkMigrate(&this->base.data, migrate);
}
#define POOL_NAME ItemConsumer
#define POOL_TYPE struct ItemConsumer
#define POOL_MIGRATE_EACH &item_consumer_migrate
#include "../templates/Pool.h"

/** Events are fit to various bins depending on the length of the delay. Longer
 delays will be more granular and less-resolution. */
static struct Events {
	unsigned update; /* time */
	struct EventList immediate;
	struct EventList approx1s[8];
	struct EventList approx8s[8];
	struct EventList approx64s[8];
	struct RunnablePool runnables;
	struct IntConsumerPool int_consumers;
	struct ItemConsumerPool item_consumers;
} events;

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
	assert(this);
	this->vt->call(this);
}
/** @implements <Runnable>Action */
static void runnable_call(struct Runnable *const this) {
	assert(this);
	this->run();
	RunnablePoolRemove(&events.runnables, this);
}
/** @implements <IntConsumer>Action */
static void int_consumer_call(struct IntConsumer *const this) {
	assert(this);
	this->accept(this->param);
	IntConsumerPoolRemove(&events.int_consumers, this);
}
/** @implements <ItemConsumer>Action */
static void item_consumer_call(struct ItemConsumer *const this) {
	this->accept(this->param);
	ItemConsumerPoolRemove(&events.item_consumers, this);
}

static const struct EventVt
	runnable_vt      = { (EventsAction)&runnable_call },
	int_consumer_vt  = { (EventsAction)&int_consumer_call },
	item_consumer_vt = { (EventsAction)&item_consumer_call };



/******************** Type functions. ******************/

/** Used in {Events_}, {Events}, and {EventsClear}. */
static void clear_event_lists(void) {
	unsigned i;
	EventListClear(&events.immediate);
	for(i = 0; i < approx1s_size; i++)
		EventListClear(events.approx1s + i);
	for(i = 0; i < approx8s_size; i++)
		EventListClear(events.approx8s + i);
	for(i = 0; i < approx64s_size; i++)
		EventListClear(events.approx64s + i);
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

/** Clears events and all heap memory. */
void EventsReset(void) {
	clear_event_lists();
	ItemConsumerPool_(&events.item_consumers);
	IntConsumerPool_(&events.int_consumers);
	RunnablePool_(&events.runnables);
}



/*************** Sub-type constructors. ******************/

/** Fits into a bin. */
static struct EventList *fit_future(const unsigned ms_future) {
	const unsigned ms_now = TimerGetGameTime(), ms = ms_now + ms_future;
	unsigned select;
	struct EventList *list;
	if(ms_future < 0x200) {
		list = &events.immediate;
		select = 0;
	} else if(ms_future < 0x2200) {
		list = events.approx1s;
		select = ((ms >> 10) & 7) + ((ms & (0x400 - 1)) > 0x200) ? 1 : 0;
	} else if(ms_future < 0x11000) {
		list = events.approx8s;
		select = ((ms >> 13) & 7) + ((ms & (0x2000 - 1)) > 0x1000) ? 1 : 0;
	} else if(ms_future < 0x88000) {
		list = events.approx64s;
		select = ((ms >> 16) & 7) + ((ms & (0x10000 - 1)) > 0x8000) ? 1 : 0;
	} else { /* Maximum delay is, \${1.024s * 64 * 8 * (min/60s) <= 8.74min} */
		list = events.approx64s;
		select = ((ms_now >> 16) + 7) & 7;
	}
	assert(select < 8);
	return list + select;
}

/** Abstract {Event} constructor. */
static void event_filler(struct Event *const this,
	const unsigned ms_future, const struct EventVt *const vt) {
	assert(this && vt);
	this->vt = vt;
	EventListPush(fit_future(ms_future), this);
}
/** Creates a new {Runnable}.
 @return Success. */
int EventsRunnable(const unsigned ms_future, const Runnable run) {
	struct Runnable *e;
	if(!run) return 0;
	if(!(e = RunnablePoolNew(&events.runnables))) return 0;
	e->run = run;
	event_filler(&e->base.data, ms_future, &runnable_vt);
	return 1;
}
/** Creates a new {SpriteConsumer}.
 @return Success. */
int EventsItemConsumer(const unsigned ms_future,
	const ItemConsumer accept, struct Item *const param) {
	struct ItemConsumer *this;
	if(!accept) return 0;
	if(!(this = ItemConsumerPoolNew(&events.item_consumers))) return 0;
	this->accept = accept;
	this->param  = param;
	event_filler(&this->base.data, ms_future, &item_consumer_vt);
	return 1;
}



/** Clears all events but doesn't reset the memory. */
void EventsClear(void) {
	clear_event_lists();
	RunnablePoolClear(&events.runnables);
	IntConsumerPoolClear(&events.int_consumers);
	ItemConsumerPoolClear(&events.item_consumers);
}

/*void EventsRemoveIf(const EventsPredicate predicate) {
	if(!events) return;
	clear_event_lists_predicate(predicate);
}*/

static void run_event_list(struct EventList *const list) {
	assert(list);
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
	/* Always run events that are immediate asap. */
	run_event_list(&events.immediate);
	/* Applies the timer with a granularity of whatever; all are [0, 7].
	 fixme: Meh, could be better. */
	t64s.cur = (events.update >> 16) & 7;
	t64s.end = (now >> 16) & 7;
	t8s.cur  = (events.update >> 13) & 7;
	t8s.end  = (now >> 13) & 7;
	t1s.cur  = (events.update >> 10) & 7;
	t1s.end  = (now >> 10) & 7;
	do {
		switch(granularity) {
			case T64S: t64s.done = (t64s.cur == t64s.end);
			case T8S:  t8s.done  = t64s.done && (t8s.cur == t8s.end);
			case T1S:  t1s.done  = t8s.done && (t1s.cur == t1s.end);
		}
		if(t1s.cur < 8) {
			chosen = events.approx1s + t1s.cur++;
			granularity = T1S;
		} else {
			t1s.cur = 0;
			if(t8s.cur != 8) {
				chosen = events.approx8s + t8s.cur++;
				granularity = T8S;
			} else {
				t8s.cur = 0;
				chosen = events.approx64s + t64s.cur++;
				t64s.cur &= 7;
				granularity = T64S;
			}
		}
		run_event_list(chosen);
	} while(!t1s.done);
}
