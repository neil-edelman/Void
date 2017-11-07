struct Poll;

void Poll_(struct Poll **const pthis);
struct Poll *Poll(const unsigned (*const pkeys)[5]);
void PollUpdate(struct Poll *const this);
