struct Wmd;

struct Wmd *Wmd(struct Sprite *const from, const int colour);
void Wmd_(struct Wmd **wmd_ptr);
void WmdClear(void);
int WmdGetExpires(const struct Wmd *wmd);
float WmdGetImpact(const struct Wmd *wmd);
int WmdGetDamage(const struct Wmd *wmd);
void WmdForceExpire(struct Wmd *wmd);
int WmdIsDestroyed(const struct Wmd *wmd);
void WmdUpdateLight(struct Wmd *wmd);
int WmdGetId(const struct Wmd *wmd);
