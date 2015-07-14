enum Behaviour { B_NONE, B_HUMAN, B_STUPID };

struct Ship;

struct Ship *Ship(struct Ship **notify, const int texture, const int size, const enum Behaviour behaviour);
void Ship_(struct Ship **ship_ptr);
void ShipSetOrientation(struct Ship *ship, const float x, const float y, const float theta);
int ShipGetOrientation(const struct Ship *ship, float *x_ptr, float *y_ptr, float *theta_ptr);
void ShipSetVelocities(struct Ship *ship, const int turning, const int acceleration, const float dt_s);
float ShipGetOmega(const struct Ship *ship);
void ShipGetVelocity(const struct Ship *ship, float *vx_ptr, float *vy_ptr);
float ShipGetMass(const struct Ship *ship);
struct Sprite *ShipGetSprite(const struct Ship *ship);
int ShipGetId(const struct Ship *s);
void ShipUpdate(const float dt_s);
void ShipShoot(struct Ship *ship, const int colour);
