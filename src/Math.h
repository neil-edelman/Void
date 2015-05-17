/* floating two-vector */
struct Vec2f { float x, y; };
/* int two-vector */
struct Vec2i { int x, y; };
/* floating colour triplet */
struct Colour3f { float r, g, b; };
/* floating colour triplet and alpha */
struct Colour4f { float r, g, b, a; };

/* VS maybe not/maybe defines this, so define our own and then some */
#undef M_SQRT1_2
#undef M_PI
#define M_SQRT1_2 (0.707106781186547524400844362104849039284835937688474036588339) /* 1/Sqrt[2] */
#define M_PI (3.141592653589793238462643383279502884197169399375105820974944) /* Pi */
#define M_2PI (6.283185307179586476925286766559005768394338798750211641949889) /* 2Pi */
#define M_1_2PI (0.159154943091895335768883763372514362034459645740456448747667) /* 1/(2Pi) */
#define M_180_PI (57.29577951308232087679815481410517033240547246656432154916024) /* 180 / pi */

#ifndef fminf /* c89 */
#define fminf(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef sqrtf /* c89 */
#define sqrtf(x) ((float)sqrt((x)))
#endif

#ifndef atan2f /* c89 */
#define atan2f(x, y) ((float)atan2((x), (y)))
#endif

/* this got redundant once Math.h was created
 C99 is not supported on legacy systems (like Visual Studio 2010?)
#ifdef C89
#define isnan(x) ((x) != (x))
#ifndef atan2f
#define atan2f(x, y) ((float)atan2((x), (y)))
#endif
#endif */

/*void Vec2fZero(struct Vec2f *a);
void Vec2fSet(struct Vec2f *a, float x, float y);*/
