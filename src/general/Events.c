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
struct Event {
	struct EventVt *vt;
	struct EventList *in_list;
};

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
 delays will be more granular and less-resolution. Maximum delay is
 ~10 minutes; \${1.024s * 64 * 8 * (min/60s) <= 8.74min}. */
struct Events;
struct Events {
	unsigned update; /* time */
	struct EventList now;
	struct EventList approx1s[7];
	struct EventList approx8s[7];
	struct EventList approx64s[7];
	struct RunnablePool *runnables;
	struct IntConsumerPool *int_consumers;
	struct SpriteConsumerPool *sprite_consumers;
};



/******************* Define virtual functions. ********************/

typedef void (*EventsAction)(struct Events *const, struct Event *const);

struct EventVt { EventsAction call; };

static void event_call(struct Events *const events, struct Event *const this) {
	assert(events && this);
	this->vt->call(events, this);
}

static void runnable_call(struct Events *const events,
	struct Runnable *const this) {
	assert(events && this);
	this->run();
	EventListRemove(this->event.data.in_list, &this->event.data);
	RunnablePoolRemove(events->runnables, this);
}

static void int_consumer_call(struct Events *const events,
	struct IntConsumer *const this) {
	assert(events && this);
	this->accept(this->param);
	EventListRemove(this->event.data.in_list, &this->event.data);
	IntConsumerPoolRemove(events->int_consumers, this);
}

static void sprite_consumer_call(struct Events *const events,
	struct SpriteConsumer *const this) {
	assert(events && this);
	this->accept(this->param);
	EventListRemove(this->event.data.in_list, &this->event.data);
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
	EventListClear(&this->now);
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

static void run_event_list(struct Events *const this,
	struct EventList *const list) {
	assert(this && list);
	
}

/** Fire off {Events} that have happened. */
void EventsUpdate(struct Events *const this) {
	const unsigned now = TimerGetGameTime(), time;
	if(!this) return;
	run_event_list(this, &this->now);
	for(time = ; i < 7; i++) {
		run_events();
	}
}


void delay_delete();

EventPool_(&removes);
EventPool_(&delays);

|| !(delays = EventPool())
|| !(removes = EventPool())
static void Event_migrate(const struct Migrate *const migrate);
static struct EventPool *delays, *removes;

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
