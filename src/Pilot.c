/** Copyright 2014 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt

 Player pilot.

 @author Neil
 @version 1
 @since 2014 */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <stdint.h> /* structs */
#include "Pilot.h"

struct Pilot {
	int32_t cash;
};

/* constants */
int32_t start_cash = 1000;
int32_t max_cash   = 2000000000;
int32_t min_cash   =-2000000000;

/* public */

/** constructor
 @return an object or a null pointer if the object couldn't be created */
struct Pilot *Pilot() {
	struct Pilot *pilot;

	if(!(pilot = malloc(sizeof(struct Pilot)))) {
		perror("Pilot constructor");
		Pilot_(&pilot);
		return 0;
	}
	pilot->cash = start_cash;
	fprintf(stderr, "Pilot: new, #%p.\n", (void *)pilot);

	return pilot;
}

/** destructor
 @param pilot_ptr a reference to the object that is to be deleted */
void Pilot_(struct Pilot **pilot_ptr) {
	struct Pilot *pilot;

	if(!pilot_ptr || !(pilot = *pilot_ptr)) return;
	fprintf(stderr, "~Pilot: erase, #%p.\n", (void *)pilot);
	free(pilot);
	*pilot_ptr = pilot = 0;
}

/** accessor
 @param  pilot
 @return cash */
int32_t PilotGetCash(const struct Pilot *pilot) {
	if(!pilot) return 0;
	return pilot->cash;
}

/** give or take away cash
 @param  pilot
 @param  cash, negative values mean take away
 @return the actual value of the cash given */
int32_t PilotGiveCash(struct Pilot *pilot, int32_t cash) {
	int32_t given;

	if(!pilot) return 0;
	if(cash >= 0) {
		if(pilot->cash >= max_cash - cash) {
			given = max_cash - pilot->cash;
			pilot->cash = max_cash;
			return given;
		}
	} else {
		if(pilot->cash <= min_cash - cash) {
			given = min_cash - pilot->cash;
			pilot->cash = min_cash;
			return given;
		}
	}
	pilot->cash += cash;
	return cash;
}
