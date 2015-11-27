struct Event;

int Event(const int delay_ms, void (*runnable)(void));
void Event_(struct Event **event_ptr);
void EventDispatch(const int t_ms);
