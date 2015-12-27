enum FnType { FN_RUNNABLE, FN_CONSUMER, FN_BICONSURMER };

struct Event;

int Event(const int delay_ms, enum FnType type, ...);
void Event_(struct Event **event_ptr);
void EventDispatch(const int t_ms);
