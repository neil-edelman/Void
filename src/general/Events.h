typedef void (*Runnable)(void);
typedef void (*IntConsumer)(const int);
struct Item;
typedef void (*ItemConsumer)(struct Item *const);
struct Events;
struct Event;
typedef int (*EventsPredicate)(const struct Event *const);

void Events_(void);
int Events(void);
void EventsClear(void);
void EventsRemoveIf(const EventsPredicate predicate);
void EventsUpdate(void);
int EventsRunnable(const unsigned ms_future, const Runnable run);
int EventsItemConsumer(const unsigned ms_future,
	const ItemConsumer accept, struct Item *const param);
