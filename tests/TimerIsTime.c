#include <stdlib.h>
#include <limits.h>
#include <stdio.h>  /* fprintf */

static unsigned game_time;

/** @return		If the game time is greater or equal t. */
int TimerIsTime(const unsigned t) {
	const int p1 = (t <= game_time);
	const int p2 = ((game_time ^ INT_MIN) < t);
	const int p3 = (game_time <= INT_MAX);
	
	/* this is literally the worst case for optimising */
	return (p1 && p2) || ((p2 || p1) && p3);
}

int main(void) {
	printf("game_time %u\n", game_time);
	printf("0 -> %d (true?)\n", TimerIsTime(0));
	printf("1 -> %d (false?)\n", TimerIsTime(1));
	printf("-1 -> %d (true?)\n", TimerIsTime(-1));
	printf("intmax -> %d (false?)\n", TimerIsTime(INT_MAX));
	printf("intmin -> %d (false?)\n", TimerIsTime(INT_MIN));
	printf("intmin+1 -> %d (true?)\n", TimerIsTime(INT_MIN + 1));

	game_time = INT_MAX;
	printf("game_time %u\n", game_time);
	printf("0 -> %d (true?)\n", TimerIsTime(0));
	printf("214...7 -> %d (true?)\n", TimerIsTime(2147483647));
	printf("-1 -> %d (false?)\n", TimerIsTime(-1));
	printf("intmax -> %d (true?)\n", TimerIsTime(INT_MAX));
	printf("intmin -> %d (false?)\n", TimerIsTime(INT_MIN));
	printf("intmin+1 -> %d (false?)\n", TimerIsTime(INT_MIN + 1));

	game_time = INT_MIN;
	printf("game_time %u\n", game_time);
	printf("0 -> %d (false?)\n", TimerIsTime(0));
	printf("214...7 -> %d (true?)\n", TimerIsTime(2147483647));
	printf("1 -> %d (true?)\n", TimerIsTime(1));
	printf("intmax -> %d (true?)\n", TimerIsTime(INT_MAX));
	printf("intmin -> %d (true?)\n", TimerIsTime(INT_MIN));
	printf("intmin+1 -> %d (false?)\n", TimerIsTime(INT_MIN + 1));

	game_time = -1;
	printf("game_time %u\n", game_time);
	printf("0 -> %d (false?)\n", TimerIsTime(0));
	printf("-1 -> %d (true?)\n", TimerIsTime(-1));
	printf("intmax -> %d (false?)\n", TimerIsTime(INT_MAX));
	printf("intmin -> %d (true?)\n", TimerIsTime(INT_MIN));

	return EXIT_SUCCESS;
}
