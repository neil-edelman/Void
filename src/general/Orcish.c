/* 2014 Neil Edelman; Orcish words are gathered off the Internet, SMAUG1.8,
 made up myself, etc. They originate or are inspired by JRR Tolkien's Orcish;
 this file has had many incarnations. */

#include <stdlib.h> /* rand */
#include <stdio.h>	/* snprintf */
#include <ctype.h>	/* toupper */

/** Tolkienised random words are useful:
 {@link http://en.wikipedia.org/wiki/Languages_constructed_by_J._R._R._Tolkien}.
 
 @author	Neil
 @version	3.3, 2016-02
 @since		3.3, 2016-02 */

static const char *sylables[] = {
	"ub", "ul", "uk", "um", "uu", "oo", "ee", "uuk", "uru",
	"ick", "gn", "ch", "ar", "eth", "ith", "ath", "uth", "yth",
	"ur", "uk", "ug", "sna", "or", "ko", "uks", "ug", "lur", "sha", "grat",
	"mau", "eom", "lug", "uru", "mur", "ash", "goth", "sha", "cir", "un",
	"mor", "ann", "sna", "gor", "dru", "az", "azan", "nul", "biz", "balc",
	"balc", "tuo", "gon", "dol", "bol", "dor", "luth", "bolg", "beo",
	"vak", "bat", "buy", "kham", "kzam", "lg", "bo", "thi",
	"ia", "es", "en", "ion",
	"mok", "muk", "tuk", "gol", "fim", "ette", "moor", "goth", "gri",
	"shn", "nak", "ash", "bag", "ronk", "ask", "mal", "ome", "hi",
	"sek", "aah", "ove", "arg", "ohk", "to", "lag", "muzg", "ash", "mit",
	"rad", "sha", "saru", "ufth", "warg", "sin", "dar", "ann", "mor", "dab",
	"val", "dur", "dug", "bar",
	"ash", "krul", "gakh", "kraa", "rut", "udu", "ski", "kri", "gal",
	"nash", "naz", "hai", "mau", "sha", "akh", "dum", "olog", "lab", "lat"
};
	
static const char *suffixes[] = {
	"at", "ob", "agh", "uk", "uuk", "um", "uurz", "hai", "ishi", "ub",
	"ull", "ug", "an", "hai", "gae", "-hai", "luk", "tz", "hur", "dush",
	"ks", "mog", "grat", "gash", "th", "on", "gul", "gae", "gun",
	"dan", "og", "ar", "meg", "or", "lin", "dog", "ath", "ien", "rn", "bul",
	"bag", "ungol", "mog", "nakh", "gorg", "-dug", "duf", "ril", "bug",
	"snaga", "naz", "gul", "ak", "kil", "ku", "on", "ritz", "bad", "nya",
	"durbat", "durb", "kish", "olog", "-atul", "burz", "puga", "shar",
	"snar", "hai", "ishi", "uruk", "durb", "krimp", "krimpat", "zum",
	"gimb", "-gimb", "glob", "-glob", "sharku", "sha", "-izub", "-izish",
	"izg", "-izg", "ishi", "ghash", "thrakat", "thrak", "golug", "mokum",
	"ufum", "bubhosh", "gimbat", "shai", "khalok", "kurta", "ness", "funda"
};

/**
 @param name		Get a random word in psudo-Orcish.
 @param name_size	sizeof(name), ideally 16; smaller is truncated, larger not
					used. */
void Orcish(char *const name, const size_t name_size) {
	const int
		a = rand() / (RAND_MAX + 1.0) * (sizeof sylables / sizeof(char *)),
		b = rand() / (RAND_MAX + 1.0) * (sizeof sylables / sizeof(char *)),
		c = rand() / (RAND_MAX + 1.0) * (sizeof suffixes / sizeof(char *));
	/* pedantically small strings */
	if(name_size <= 0) {
		return;
	} else if(name_size == 1) {
		name[0] = '\0';
		return;
	}
	/*printf("{a,b,c} = {%d,%d,%d}\n", a, b, c);*/
	snprintf(name, name_size, "%s%s%s", sylables[a], sylables[b], suffixes[c]);
	name[0] = toupper(name[0]);
}
