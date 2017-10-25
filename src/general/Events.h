typedef void (*Runnable)(void);
typedef void (*IntConsumer)(const int);
struct Sprite;
typedef void (*SpriteConsumer)(struct Sprite *const);
struct Events;

void Events_(struct Events **const pthis);
struct Events *Events(void);
void EventsUpdate(struct Events *const this);
int EventsRunnable(struct Events *const events, const unsigned ms_future,
	const Runnable run);
