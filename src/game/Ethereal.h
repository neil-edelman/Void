struct Image;
struct Gate;
struct Ethereal;

struct Ethereal *Ethereal(const struct Image *image);
struct Ethereal *EtherealGate(const struct Gate *gate);
void Ethereal_(struct Ethereal **e_ptr);
int EtherealGetId(const struct Ethereal *e);
int EtherealIsDestroyed(const struct Ethereal *e);
