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
#include "../system/Timer.c"
#include "Events.h"



/*************** Declare types. ****************/

/** This is one polymorphic {Event}. */
struct Event;
struct EventVt;
struct EventList;
struct Event { struct EventVt *vt; };

#define LIST_NAME Event
#define LIST_TYPE struct Event
#include "../templates/List.h"

/** {Runnable} is an {Event}. */
typedef void (*Runnable)(void);

struct Runnable {
	struct EventListNode event;
	Runnable run;
};

#define POOL_NAME Runnable
#define POOL_TYPE struct Runnable
#include "../templates/Pool.h"

/** {IntConsumer} is an {Event}. */
typedef void (*IntConsumer)(const int);

struct IntConsumer {
	struct EventListNode event;
	IntConsumer accept;
	int param;
};

#define POOL_NAME IntConsumer
#define POOL_TYPE struct IntConsumer
#include "../templates/Pool.h"

struct Sprite;
/** {SpriteConsumer} is an {Event}. */
typedef void (*SpriteConsumer)(struct Sprite *const);

struct SpriteConsumer {
	struct EventListNode event;
	SpriteConsumer accept;
	struct Sprite *const param;
};

#define POOL_NAME SpriteConsumer
#define POOL_TYPE struct SpriteConsumer
#include "../templates/Pool.h"

/** Events are fit to various bins depending on the length of the delay. Longer
 delays will be more granular and less-resolution. Maximum delay is,
 \${1.024s * 64 * 8 * (min/60s) <= 8.74min} */
struct Events;
struct Events {
	unsigned update; /* time */
	struct EventList immediate;
	struct EventList approx1s[7]; /* the {1s} falls though to {8s}, etc. */
	struct EventList approx8s[7];
	struct EventList approx64s[8];
	struct RunnablePool *runnables;
	struct IntConsumerPool *int_consumers;
	struct SpriteConsumerPool *sprite_consumers;
};



/******************* Define virtual functions. ********************/

typedef void (*EventsAction)(struct Events *const, struct Event *const);

struct EventVt { EventsAction call; };

/** This is only called from {run_event_list} as the event list must be purged
 because the backing in the {Pool}s has been removed; important!
 @implements <Event, [Events]>BiAction */
static void event_call(struct Event *const this, struct Events *const events) {
	assert(events && this);
	this->vt->call(events, this);
}

static void runnable_call(struct Events *const events,
	struct Runnable *const this) {
	this->run();
	RunnablePoolRemove(events->runnables, this);
}

static void int_consumer_call(struct Events *const events,
	struct IntConsumer *const this) {
	this->accept(this->param);
	IntConsumerPoolRemove(events->int_consumers, this);
}

static void sprite_consumer_call(struct Events *const events,
	struct SpriteConsumer *const this) {
	this->accept(this->param);
	SpriteConsumerPoolRemove(events->sprite_consumers, this);
}

static const struct EventVt
	runnable_vt        = { (EventsAction)&runnable_call },
	int_consumer_vt    = { (EventsAction)&int_consumer_call },
	sprite_consumer_vt = { (EventsAction)&sprite_consumer_call };



/******************** Type functions. ******************/

/** Used in {Events_} and {Events}. */
static void clear_events(struct Events *const this) {
	unsigned i;
	assert(this);
	EventListClear(&this->immediate);
	for(i = 0; i < sizeof this->approx1s / sizeof(struct EventList *); i++)
		EventListClear(this->approx1s + i);
	for(i = 0; i < sizeof this->approx8s / sizeof(struct EventList *); i++)
		EventListClear(this->approx8s + i);
	for(i = 0; i < sizeof this->approx64s / sizeof(struct EventList *); i++)
		EventListClear(this->approx64s + i);
}

/** Destructor. */
void Events_(struct Events **const pthis) {
	struct Events *this;
	if(!pthis || (this = *pthis)) return;
	clear_events(this);
	SpriteConsumerPool_(&this->sprite_consumers);
	IntConsumerPool_(&this->int_consumers);
	RunnablePool_(&this->runnables);
	free(this);
	*pthis = this = 0;
}

/** Events pool constructor. */
struct Events *Events(void) {
	struct Events *this;
	const char *e = 0;
	if(!(this = malloc(sizeof *this)))
		{ perror("Events"); Events_(&this); return 0; }
	clear_events(this);
	this->runnables = 0;
	this->int_consumers = 0;
	this->sprite_consumers = 0;
	do {
		if(!(this->runnables = RunnablePool(&EventListMigrate, this)))
			{ e = RunnablePoolGetError(this->runnables); break; }
		if(!(this->int_consumers = IntConsumerPool(&EventListMigrate, this)))
			{ e = IntConsumerPoolGetError(this->int_consumers); break; }
		if(!(this->sprite_consumers=SpriteConsumerPool(&EventListMigrate,this)))
			{ e = SpriteConsumerPoolGetError(this->sprite_consumers); break;}
	} while(0); if(e) {
		fprintf(stderr, "Events: %s.\n", e);
		Events_(&this);
	}
	return this;
}

static void run_event_list(struct Events *const events,
	struct EventList *const list) {
	assert(events && list);
	EventListBiForEach(list, (EventBiAction)&event_call, events);
	/* {event_call} strips away their backing, so this call is important! */
	EventListClear(list);
}

/** Fire off {Events} that have happened. */
void EventsUpdate(struct Events *const this) {
	struct EventList *chosen;
	struct { unsigned cur, end; int done; } t1s, t8s, t64s;
	const unsigned now = TimerGetGameTime();
	enum { T64S, T8S, T1S } granularity = T64S;
	if(!this) return;
	/* Always run events that are immediate asap. */
	run_event_list(this, &this->immediate);
	/* Applies the timer with a granularity of whatever; all are [0, 7].
	 fixme: Meh, could be better. */
	t64s.cur = (this->update & (7 << 16)) >> 16;
	t64s.end = (now & (7 << 16)) >> 16;
	t8s.cur  = (this->update & (7 << 13)) >> 13;
	t8s.end  = (now & (7 << 13)) >> 13;
	t1s.cur  = (this->update & (7 << 10)) >> 10;
	t1s.end  = (now & (7 << 10)) >> 10;
	t1s.done = t8s.done && (t1s.cur == t1s.end);
	do {
		switch(granularity) {
			case T64S: t64s.done = (t64s.cur == t64s.end);
			case T8S:  t8s.done  = t64s.done && (t8s.cur == t8s.end);
			case T1S:  t1s.done  = t8s.done && (t1s.cur == t1s.end);
		}
		if(t1s.cur != 7) {
			chosen = this->approx1s + t1s.cur++;
			granularity = T1S;
		} else {
			t1s.cur = 0;
			if(t8s.cur != 7) {
				chosen = this->approx8s + t8s.cur++;
				granularity = T8S;
			} else {
				t8s.cur = 0;
				chosen = this->approx64s + t64s.cur++;
				t64s.cur &= 7;
				granularity = T64S;
			}
		}
		run_event_list(this, chosen);
	} while(!t1s.done);
}

static struct EventList *fit_future(struct Events *const this,
	const unsigned ms_future) {
	struct EventList *list = 0;
	const unsigned ms = TimerGetGameTime() + ms_future;
	unsigned select;
	enum { T64S, T8S, T1S, IMM } granularity;
	assert(this);
	if(ms_future < 0x200) {
		granularity = IMM;
		select = 0;
	} else if(ms_future < 0x2200) {
		granularity = T1S;
		select = (ms >> 10) & 7 + (ms & 1023 > 512) ? 1 : 0;
	} else if(ms_future < 0x11000) {
		granularity = T8S;
		select = (ms >> 13) & 7 + (ms & 8191 > 4096) ? 1 : 0;
	} else if(ms_future < 0x88000) {
		granularity = T64S;
		select = (ms >> 16) & 7 + (ms & 65535 > 32768) ? 1 : 0;
	} else {
		granularity = T64S;
		select = ((TimerGetGameTime() >> 16) + 7) & 7;
	}
	return list;
}

int EventsRunnable(struct Events *const this, const unsigned ms_future,
	const Runnable run) {
	struct EventList *list;
	if(!this || !run) return 0;
	list = fit_future(this, ms_future);
}










#if 0
/* These are the types of delays. */

/** Called from \see{Event_transfer}.
 @implements <Sprite>Action */
static void lazy_update_fg_bin(struct Sprite *const this) {
	unsigned to_bin;
	/**/char a[12];
	assert(this);
	/**/Sprite_to_string(this, &a);
	Vec2f_to_fg_bin(&this->x_5, &to_bin); /* fixme: again. */
	printf("%s: Bin%u -> Bin%u.\n", a, this->bin, to_bin);
	SpriteListRemove(bins + this->bin, this);
	SpriteListPush(bins + to_bin, (struct SpriteListNode *)this);
	this->bin = to_bin;
}
static void Event_transfer(struct Sprite *const sprite) {
	struct Event *d;
	if(!(d = EventPoolNew(delays))) { fprintf(stderr, "Event error: %s.\n",
		EventPoolGetError(delays)); return; }
	d->action = &lazy_update_fg_bin;
	d->sprite = sprite;
}

/** Called from \see{Event_delete}.
 @implements <Sprite>Action */
static void lazy_delete(struct Sprite *const this) {
	assert(this);
	this->vt->delete(this);
}
static void Event_delete(struct Sprite *const sprite) {
	struct Event *d;
	if(!(d = EventPoolNew(removes))) { fprintf(stderr, "Event error: %s.\n",
		EventPoolGetError(removes)); return; }
	d->action = &lazy_delete;
	d->sprite = sprite;
}

/** This applies the delay. */
static void Event_evaluate(struct Event *const this) {
	assert(this && this->action);
	this->action(this->sprite);
}

/** Called by \see{Event_migrate}.
 @implements <Transfer, Migrate>BiAction */
static void Event_migrate_sprite(struct Event *const this,
								 void *const migrate_void) {
	const struct Migrate *const migrate = migrate_void;
	assert(this && migrate);
	SpriteMigrate(migrate, &this->sprite);
}
#endif
