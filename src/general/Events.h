typedef void (*Runnable)(void);
typedef void (*IntConsumer)(const int);
struct Sprite;
typedef void (*SpriteConsumer)(struct Sprite *const);
struct Events;

void Events_(void);
int Events(void);
void EventsClear(void);
void EventsUpdate(void);
int EventsRunnable(const unsigned ms_future, const Runnable run);
int EventsSpriteConsumer(const unsigned ms_future,
	const SpriteConsumer consumer, const struct Sprite *const param);
