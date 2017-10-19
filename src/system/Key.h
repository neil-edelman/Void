#define KEY_MAX 255

/* We don't care what historical deliniation these had; map 'special' keys to
 just keys. \url{https://en.wikipedia.org/wiki/Keyboard_layout}
 101 key US, 104 key Windows, 102/105 international, 110/109/112 extended
 so there's lots of room for these */
enum Keys {
	k_unknown = 0,
	k_f1 = KEY_MAX - 21,
	k_f2,
	k_f3,
	k_f4,
	k_f5,
	k_f6,
	k_f7,
	k_f8,
	k_f9,
	k_f10,
	k_f11,
	k_f12,
	k_left,
	k_up,
	k_right,
	k_down,
	k_pgup,
	k_pgdn,
	k_home,
	k_end,
	k_ins,
	k_max
};

int Key(void);
void KeyRegister(const unsigned k, void (*const handler)(void));
int KeyTime(const int key);
int KeyPress(const int key);
