#include "../general/OrthoMath.h"

struct AutoImage;

typedef void (*DrawOutput)(const struct Ortho3f *const x,
	const struct AutoImage *const tex, const struct AutoImage *const nor);

int Draw(void);
void Draw_(void);
#ifdef SDL /* <-- awkward */
void DrawDisplay(void);
void DrawResize(const int w, const int h);
#endif /* awkward --> */
void DrawSetCamera(const struct Vec2f *const x);
void DrawGetScreen(struct Rectangle4f *const rect);
void DrawSetBackground(const char *const key);
void DrawSetShield(const char *const key);
void DrawDisplayLambert(const struct Ortho3f *const x,
	const struct AutoImage *const tex, const struct AutoImage *const nor);
void DrawDisplayFar(const struct Ortho3f *const x,
	const struct AutoImage *const tex, const struct AutoImage *const nor);
void DrawDisplayInfo(const struct Vec2f *const x,
	const struct AutoImage *const tex);
