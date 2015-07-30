struct Debris;

struct Debris *Debris(const int texture, const int size, const float mass);
void Debris_(struct Debris **deb_ptr);
void DebrisSetOrientation(struct Debris *deb, const float x, const float y, const float theta, const float vx, const float vy, const float omega);
void DebrisGetOrientation(struct Debris *deb, float *x_ptr, float *y_ptr, float *theta_ptr);
void DebrisGetVelocity(const struct Debris *deb, float *vx_ptr, float *vy_ptr);
void DebrisSetVelocity(struct Debris *deb, const float vx, const float vy);
float DebrisGetMass(const struct Debris *d);
int DebrisGetId(const struct Debris *d);
struct Sprite *DebrisGetSprite(const struct Debris *deb);
void DebrisHit(struct Debris *deb, const int hit);
int DebrisIsDestroyed(const struct Debris *d);
void DebrisEnforce(struct Debris *deb);
void DebrisExplode(struct Debris *deb);
