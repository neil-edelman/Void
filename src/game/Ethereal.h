struct Image;
struct Gate;
struct Sprite;
struct Ethereal;

struct Ethereal *Ethereal(const struct Image *image);
struct Ethereal *EtherealGate(const struct Gate *gate);
void Ethereal_(struct Ethereal **e_ptr);
void EtherealClear(void);
void (*EtherealGetCallback(struct Ethereal *e))(struct Ethereal *, struct Sprite *);
struct Sprite *EtherealGetSprite(const struct Ethereal *e);
int EtherealGetId(const struct Ethereal *e);
int EtherealIsDestroyed(const struct Ethereal *e);
