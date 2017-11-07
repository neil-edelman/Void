struct Input;

void Input_(struct Input **const pthis);
struct Input *Input(const unsigned (*const keys)[5]);
void InputPoll(struct Input *const this);
