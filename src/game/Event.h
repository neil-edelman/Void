struct Event;

struct Event *Event(const int t_ms, void (*consumer)(int));
void Event_(struct Event **event_ptr);
void EventDispatch(const int t_ms);
