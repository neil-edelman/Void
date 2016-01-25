enum FnType { FN_RUNNABLE, FN_CONSUMER, FN_BICONSUMER };

struct Event;

struct Event *Event(const int delay_ms, const int sigma_ms, enum FnType type, ...);
void Event_(struct Event **event_ptr);
void EventClear(void);
void EventDispatch(void);
void EventReplaceArguments(struct Event *const e, ...);
