#include "Delay.h"

void delay_delete();

DelayPool_(&removes);
DelayPool_(&delays);

|| !(delays = DelayPool())
|| !(removes = DelayPool())
static void Delay_migrate(const struct Migrate *const migrate);
static struct DelayPool *delays, *removes;

/* These are the types of delays. */

/** Called from \see{Delay_transfer}.
 @implements <Sprite>Action */
static void lazy_update_fg_bin(struct Sprite *const this) {
	unsigned to_bin;
	/**/char a[12];
	assert(this);
	/**/Sprite_to_string(this, &a);
	Vec2f_to_fg_bin(&this->x_5, &to_bin); /* fixme: again. */
	printf("%s: Bin%u -> Bin%u.\n", a, this->bin, to_bin);
	SpriteListRemove(bins + this->bin, this);
	SpriteListPush(bins + to_bin, (struct SpriteListNode *)this);
	this->bin = to_bin;
}
static void Delay_transfer(struct Sprite *const sprite) {
	struct Delay *d;
	if(!(d = DelayPoolNew(delays))) { fprintf(stderr, "Delay error: %s.\n",
											  DelayPoolGetError(delays)); return; }
	d->action = &lazy_update_fg_bin;
	d->sprite = sprite;
}

/** Called from \see{Delay_delete}.
 @implements <Sprite>Action */
static void lazy_delete(struct Sprite *const this) {
	assert(this);
	this->vt->delete(this);
}
static void Delay_delete(struct Sprite *const sprite) {
	struct Delay *d;
	if(!(d = DelayPoolNew(removes))) { fprintf(stderr, "Delay error: %s.\n",
											   DelayPoolGetError(removes)); return; }
	d->action = &lazy_delete;
	d->sprite = sprite;
}

/** This applies the delay. */
static void Delay_evaluate(struct Delay *const this) {
	assert(this && this->action);
	this->action(this->sprite);
}

/** Called by \see{Delay_migrate}.
 @implements <Transfer, Migrate>BiAction */
static void Delay_migrate_sprite(struct Delay *const this,
								 void *const migrate_void) {
	const struct Migrate *const migrate = migrate_void;
	assert(this && migrate);
	SpriteMigrate(migrate, &this->sprite);
}
/** The {sprite} field in {Delay} needs migrating. Called from {Sprite_migrate}.
 @implements Migrate */
static void Delay_migrate(const struct Migrate *const migrate) {
	DelayPoolBiForEach(delays, &Delay_migrate_sprite, (void *const)migrate);
	DelayPoolBiForEach(removes, &Delay_migrate_sprite, (void *const)migrate);
}
