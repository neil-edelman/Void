struct Pilot;

struct Pilot *Pilot();
void Pilot_(struct Pilot **pilotptr);
int32_t PilotGetCash(const struct Pilot *pilot);
int32_t PilotGiveCash(struct Pilot *pilot, int32_t cash);
