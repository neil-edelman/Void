enum Behaviour { B_NONE, B_HUMAN, B_STUPID };

struct Ship;
struct ShipClass; /* from Lores */

struct Ship *Ship(struct Ship **notify, const struct ShipClass *ship_class, const enum Behaviour behaviour);
void Ship_(struct Ship **ship_ptr);
void ShipSetOrientation(struct Ship *ship, const float x, const float y, const float theta);
int ShipGetOrientation(const struct Ship *ship, float *const x_ptr, float *const y_ptr, float *const theta_ptr);
void ShipSetVelocities(struct Ship *ship, const int turning, const int acceleration, const float dt_s);
float ShipGetOmega(const struct Ship *ship);
void ShipGetVelocity(const struct Ship *ship, float *vx_ptr, float *vy_ptr);
float ShipGetMass(const struct Ship *ship);
float ShipGetBounding(const struct Ship *ship);
struct Sprite *ShipGetSprite(const struct Ship *ship);
int ShipGetHit(const struct Ship *const ship);
int ShipGetMaxHit(const struct Ship *const ship);
int ShipIsDestroyed(const struct Ship *s);
float ShipGetHorizon(const struct Ship *s);
void ShipSetHorizon(struct Ship *s, const float h);
int ShipGetId(const struct Ship *s);
void ShipUpdate(const float dt_s);
void ShipShoot(struct Ship *ship, const int colour);
void ShipHit(struct Ship *ship, const int damage);
