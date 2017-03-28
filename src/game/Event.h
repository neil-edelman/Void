enum FnType { FN_RUNNABLE, FN_CONSUMER, FN_BICONSUMER };

/** See \see{Event}. */
struct Event;

int Event(struct Event **const event_ptr, const int delay_ms, const int sigma_ms, enum FnType fn_type, ...);
void Event_(struct Event **event_ptr);
void EventRemoveIf(int (*const predicate)(struct Event *const));
void EventSetNotify(struct Event **const e_ptr);
void (*EventGetConsumerAccept(const struct Event *const e))(void *);
void EventDispatch(void);
void EventReplaceArguments(struct Event *const e, ...);
char *EventToString(const struct Event *const e);
void EventList(void);
