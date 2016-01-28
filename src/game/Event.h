enum FnType { FN_RUNNABLE, FN_CONSUMER, FN_BICONSUMER };

struct Event;

int Event(const char z, const int delay_ms, const int sigma_ms, enum FnType fn_type, ...);
void Event_(struct Event **event_ptr);
void EventClear(void);
/*void EventSetNotify(struct Event **const e_ptr);*/
void EventDispatch(void);
void EventReplaceArguments(struct Event *const e, ...);
