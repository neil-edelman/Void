/** 2016 Neil Edelman, distributed under the terms of the MIT License;
 see readme.txt, or \url{ https://opensource.org/licenses/MIT }.

 {<T>Set} is a dynamic array that stores unordered {<T>}, which must be set
 using {SET_TYPE}. The {<T>Set} is initially contiguous, but removing an
 element is done lazily through a queue internal to the set; as such, indices
 will remain the same thoughout the lifetime of the data. Resising incurs
 amortised cost, done though a Fibonacci sequence. {<T>Set} is not synchonised.
 The preprocessor macros are all undefined at the end of the file for
 convenience when including multiple set types in the same file.

 @param SET_NAME
 This literally becomes {<T>}. As it's used in function names, this should
 comply with naming rules; required.

 @param SET_TYPE
 The type associated with {<T>}. Has to be a valid type, accessible to the
 compiler at the time of inclusion; required.

 @param SET_TO_STRING
 Optional print function implementing {<T>ToString}; makes available
 \see{<T>SetToString}.

 @param SET_DEBUG
 Prints information to {stdout}.

 @param SET_TEST
 Unit testing framework using {<T>SetTest}, included in a separate header,
 {../test/SetTest.h}. Must be defined equal to a (random) filler, satisfying
 {<T>Action}. If {NDEBUG} is not defined, turns on {assert} private function
 integrity testing. Requires {SET_TO_STRING}.

 @title		Set.h
 @std		C89/90
 @author	Neil
 @version	1.4; 2017-07 made migrate simpler
 @since		1.3; 2017-05 split {List} from {Set}; much simpler
			1.2; 2017-01 almost-redundant functions simplified
			1.1; 2016-11 multi-index
			1.0; 2016-08 permute */



#include <stddef.h>	/* ptrdiff_t */
#include <stdlib.h>	/* malloc free qsort */
#include <assert.h>	/* assert */
#include <string.h>	/* memcpy (memmove strerror strcpy memcmp in SetTest.h) */
#ifdef SET_TO_STRING /* <-- print */
#include <stdio.h>	/* snprintf */
#endif /* print --> */
#include <errno.h>	/* errno */
#ifdef SET_DEBUG
#include <stdarg.h>	/* for print */
#endif /* calls --> */



/* unused macro */
#ifdef UNUSED
#undef UNUSED
#endif
#define UNUSED(a) ((void)(a))



/* check defines */
#ifndef SET_NAME
#error Set generic SET_NAME undefined.
#endif
#ifndef SET_TYPE
#error Set generic SET_TYPE undefined.
#endif
#if defined(SET_DEBUG) && !defined(SET_TO_STRING)
#error Set: SET_DEBUG requires SET_TO_STRING.
#endif
#if !defined(SET_TEST) && !defined(NDEBUG)
#define SET_NDEBUG
#define NDEBUG
#endif



/* After this block, the preprocessor replaces T with SET_TYPE, T_(X) with
 SET_NAMEX, PRIVATE_T_(X) with SET_U_NAME_X, and T_NAME with the string
 version. http://stackoverflow.com/questions/16522341/pseudo-generics-in-c */
#ifdef CAT
#undef CAT
#endif
#ifdef CAT_
#undef CAT_
#endif
#ifdef PCAT
#undef PCAT
#endif
#ifdef PCAT_
#undef PCAT_
#endif
#ifdef T
#undef T
#endif
#ifdef T_
#undef T_
#endif
#ifdef PRIVATE_T_
#undef PRIVATE_T_
#endif
#ifdef T_NAME
#undef T_NAME
#endif
#ifdef QUOTE
#undef QUOTE
#endif
#ifdef QUOTE_
#undef QUOTE_
#endif
#define CAT_(x, y) x ## y
#define CAT(x, y) CAT_(x, y)
#define PCAT_(x, y) x ## _ ## y
#define PCAT(x, y) PCAT_(x, y)
#define QUOTE_(name) #name
#define QUOTE(name) QUOTE_(name)
#define T_(thing) CAT(SET_NAME, thing)
#define PRIVATE_T_(thing) PCAT(set, PCAT(SET_NAME, thing))
#define T_NAME QUOTE(SET_NAME)

/* Troubles with this line? check to ensure that SET_TYPE is a valid type,
 whose definition is placed above {#include "Set.h"}. */
typedef SET_TYPE PRIVATE_T_(Type);
#define T PRIVATE_T_(Type)





/* constants across multiple includes in the same translation unit */
#ifndef SET_H /* <-- SET_H */
#define SET_H

static const size_t set_fibonacci6 = 8;
static const size_t set_fibonacci7 = 13;
static const size_t set_not_part   = (size_t)-1;
/** Used as a null pointer with indices; {Set} will not allow the size to be
 this big. */
static const size_t set_null       = (size_t)-2;

/* designated initializers are C99; this is safe because C has rules for enum
 default initialisers */
enum SetError {
	SET_NO_ERROR,
	SET_ERRNO,
	SET_PARAMETER,
	SET_OUT_OF_BOUNDS,
	SET_OVERFLOW
};
static const char *const set_error_explination[] = {
	/*[SET_NO_ERROR]      =*/ "no error",
	/*[SET_ERRNO]         =*/ 0, /* <- get errno */
	/*[SET_PARAMETER]     =*/ "parameter out-of-range",
	/*[SET_OUT_OF_BOUNDS] =*/ "out-of-bounds",
	/*[SET_OVERFLOW]      =*/ "overflow"
};

/* global for constructor allocation errors */
static enum SetError set_global_error;
static int           set_global_errno_copy;

#ifndef MIGRATE /* <-- migrate */
#define MIGRATE
/** Contains information about a {realloc}. */
struct Migrate;
struct Migrate {
	const void *begin, *end; /* old pointers */
	ptrdiff_t delta;
};
#endif /* migrate --> */

/** Calls on {realloc} with \see{<T>SetSetMigrate}. */
typedef void (*Migrate)(const struct Migrate *const info);

#endif /* SET_H --> */



/** Operates by side-effects only. */
typedef void (*T_(Action))(T *const element);

/** Returns (non-zero) true or (zero) false. */
typedef int  (*T_(Predicate))(T *const element);

#ifdef SET_TO_STRING /* <-- string */

/** Responsible for turning {<T>} (the first argument) into a 12 {char}
 null-terminated output string (the second.) */
typedef void (*T_(ToString))(const T *, char (*const)[12]);

/* Check that {SET_TO_STRING} is a function implementing {<T>ToString}. */
static const T_(ToString) PRIVATE_T_(to_string) = (SET_TO_STRING);

#endif /* string --> */



/* Set element. */
struct PRIVATE_T_(Element) {
	T data; /* has to be the first element for convenince */
	size_t prev, next; /* removed queue */
};

/** The set. To instantiate, see \see{<T>Set}. */
struct T_(Set);
struct T_(Set) {
	struct PRIVATE_T_(Element) *array;
	size_t capacity[2]; /* Fibonacci, [0] is the capacity, [1] is next */
	size_t size; /* including removed */
	size_t head, tail; /* removed queue */
	enum SetError error; /* errors defined by enum SetError */
	int errno_copy; /* copy of errno when when error == E_ERRNO */
	Migrate migrate; /* called when change memory addresses if present */
};



/** Debug messages from set functions; turn on using {SET_DEBUG}. */
static void PRIVATE_T_(debug)(struct T_(Set) *const this,
	const char *const fn, const char *const fmt, ...) {
#ifdef SET_DEBUG
	/* \url{ http://c-faq.com/varargs/vprintf.html } */
	va_list argp;
	fprintf(stderr, "Set<" T_NAME ">#%p.%s: ", (void *)this, fn);
	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);
#else
	UNUSED(this); UNUSED(fn); UNUSED(fmt);
#endif
}

/** Ensures capacity.
 @return Success.
 @throws SET_OVERFLOW, SET_ERRNO */
static int PRIVATE_T_(reserve)(struct T_(Set) *const this,
	const size_t min_capacity) {
	size_t c0, c1;
	struct PRIVATE_T_(Element) *array;
	const size_t max_size = (set_null - 1) / sizeof *array;
	assert(this);
	assert(this->size <= this->capacity[0]);
	assert(this->capacity[0] <= this->capacity[1]);
	assert(this->capacity[1] < set_null);
	assert(set_null < set_not_part);
	if(this->capacity[0] >= min_capacity) return 1;
	if(max_size < min_capacity) return this->error = SET_OVERFLOW, 0; 
	c0 = this->capacity[0];
	c1 = this->capacity[1];
	while(c0 < min_capacity) {
		c0 ^= c1, c1 ^= c0, c0 ^= c1, c1 += c0;
		if(c1 <= c0 || c1 > max_size) c1 = max_size;
	}
	if(!(array = realloc(this->array, c0 * sizeof *this->array)))
		return this->error = SET_ERRNO, this->errno_copy = errno, 0;
	PRIVATE_T_(debug)(this, "reserve", "array#%p[%lu] -> #%p[%lu].\n",
		(void *)this->array, (unsigned long)this->capacity[0], (void *)array,
		(unsigned long)c0);
	/* Migrate function, if it is defined and it changed memory locations. */
	if(this->migrate && this->array != array) {
		struct Migrate migrate;
		migrate.begin = this->array;
		migrate.end   = (const char *)this->array + this->size * sizeof *array;
		migrate.delta = (const char *)array - (const char *)this->array;
		this->migrate(&migrate);
	}
	this->array = array;
	this->capacity[0] = c0;
	this->capacity[1] = c1;
	return 1;
}

/** We are very lazy and we just enqueue the removed for later elements.
 @param idx: Must be a valid index. */
static void PRIVATE_T_(enqueue_removed)(struct T_(Set) *const this,
	const size_t e) {
	struct PRIVATE_T_(Element) *elem;
	assert(this);
	assert(e < this->size);
	elem = this->array + e;
	/* cannot be part of the removed set already */
	assert(elem->prev == set_not_part);
	assert(elem->next == set_not_part);
	if((elem->prev = this->tail) == set_null) {
		assert(this->head == set_null);
		this->head = this->tail = e;
	} else {
		struct PRIVATE_T_(Element) *const last = this->array + this->tail;
		assert(last->next == set_null);
		last->next = this->tail = e;
	}
	elem->next = set_null;
}

/** Dequeues a removed element, or if the queue is empty, returns null. */
static struct PRIVATE_T_(Element) *PRIVATE_T_(dequeue_removed)(
	struct T_(Set) *const this) {
	struct PRIVATE_T_(Element) *elem;
	size_t e;
	assert(this);
	assert((this->head == set_null) == (this->tail == set_null));
	if((e = this->head) == set_null) return 0;
	elem = this->array + e;
	assert(elem->prev == set_null);
	assert(elem->next != set_not_part);
	if((this->head = elem->next) == set_null) {
		this->head = this->tail = set_null;
	} else {
		struct PRIVATE_T_(Element) *const next = this->array + elem->next;
		assert(elem->next < this->size);
		next->prev = set_null;
	}
	elem->prev = elem->next = set_not_part;
	return elem;
}

/** Gets rid of the removed elements at the tail of the list. Each remove has
 potentially one delete in the worst case, it just gets differed a bit. */
static void PRIVATE_T_(trim_removed)(struct T_(Set) *const this) {
	struct PRIVATE_T_(Element) *elem, *prev, *next;
	size_t e;
	assert(this);
	while(this->size
		&& (elem = this->array + (e = this->size - 1))->prev != set_not_part) {
		if(elem->prev == set_null) {
			assert(this->head == e), this->head = elem->next;
		} else {
			assert(elem->prev < this->size), prev = this->array + elem->prev;
			prev->next = elem->next;
		}
		if(elem->next == set_null) {
			assert(this->tail == e), this->tail = elem->prev;
		} else {
			assert(elem->next < this->size), next = this->array + elem->next;
			next->prev = elem->prev;
		}
		this->size--;
	}
}

/** Destructor for Set.
 @param thisp: A reference to the object that is to be deleted; it will be set
 to null. If it is already null or it points to null, doesn't do anything.
 @order \Theta(1)
 @allow */
static void T_(Set_)(struct T_(Set) **const thisp) {
	struct T_(Set) *this;
	if(!thisp || !(this = *thisp)) return;
	PRIVATE_T_(debug)(this, "Delete", "erasing.\n");
	free(this->array);
	free(this);
	*thisp = 0;
}

/** Constructs an empty Set with capacity Fibonacci6, which is 8.
 @return A new Set.
 @throws AE_ERRNO: Use {SetError(0)} to get the error.
 @order \Theta(1)
 @allow */
static struct T_(Set) *T_(Set)(void) {
	struct T_(Set) *this;
	if(!(this = malloc(sizeof(struct T_(Set))))) {
		set_global_error = SET_ERRNO;
		set_global_errno_copy = errno;
		return 0;
	}
	this->array        = 0;
	this->capacity[0]  = set_fibonacci6;
	this->capacity[1]  = set_fibonacci7;
	this->size         = 0;
	this->head = this->tail = set_null;
	this->error        = SET_NO_ERROR;
	this->errno_copy   = 0;
	this->migrate      = 0;
	if(!(this->array = malloc(this->capacity[0] * sizeof *this->array))) {
		T_(Set_)(&this);
		set_global_error = SET_ERRNO;
		set_global_errno_copy = errno;
		return 0;
	}
	PRIVATE_T_(debug)(this, "New", "capacity %d.\n", this->capacity[0]);
	return this;
}

/** See what's the error if something goes wrong. Resets the error.
 @return The last error string.
 @order \Theta(1)
 @allow */
static const char *T_(SetGetError)(struct T_(Set) *const this) {
	const char *str;
	enum SetError *perr;
	int *perrno;
	perr   = this ? &this->error      : &set_global_error;
	perrno = this ? &this->errno_copy : &set_global_errno_copy;
	if(!(str = set_error_explination[*perr])) str = strerror(*perrno);
	*perr = 0;
	return str;
}

/** @return	Is the set empty?
 @param this: If {this} is null, returns true.
 @order \Theta(1)
 @allow */
static size_t T_(SetIsEmpty)(const struct T_(Set) *const this) {
	if(!this) return 1;
	return this->size ? 0 : 1;
}

/** Is {idx} a valid index for {this}.
 @order \Theta(1)
 @allow */
static int T_(SetIsElement)(struct T_(Set) *const this, const size_t idx) {
	struct PRIVATE_T_(Element) *elem;
	if(!this) return 0;
	if(idx >= this->size
		|| (elem = this->array + idx, elem->prev != set_not_part))
		return 0;
	return 1;
}

/** Sets a callback when the position of the set changes internally.
 @param migrate: Set to null to turn off.
 @order \Theta(1)
 @allow */
static void T_(SetSetMigrate)(struct T_(Set) *const this,
	const Migrate migrate) {
	if(!this) return;
	this->migrate = migrate;
}

/** Increases the capacity of this Set to ensure that it can hold at least the
 number of elements specified by the {min_capacity}.
 @param this: If {this} is null, returns false.
 @return True if the capacity increase was viable; otherwise the set is not
 touched and the error condition is set.
 @throws SET_ERRNO, SET_OVERFLOW
 @order \Omega(1), O({capacity})
 @allow */
static int T_(SetReserve)(struct T_(Set) *const this,
	const size_t min_capacity) {
	if(!this) return 0;
	if(!PRIVATE_T_(reserve)(this, min_capacity)) return 0;
	PRIVATE_T_(debug)(this, "Reserve", "set set size to %u to contain %u.\n",
		this->capacity[0], min_capacity);
	return 1;
}

/** Gets an uninitialised new element.
 @param this: If {this} is null, returns null.
 @return If failed, returns a null pointer and the error condition will be set.
 @throws SET_OVERFLOW, SET_ERRNO
 @order amortised O(1)
 @allow */
static T *T_(SetNew)(struct T_(Set) *const this) {
	struct PRIVATE_T_(Element) *elem;
	if(!this) return 0;
	if(!(elem = PRIVATE_T_(dequeue_removed)(this))) {
		if(!PRIVATE_T_(reserve)(this, this->size + 1)) return 0;
		elem = this->array + this->size++;
		elem->prev = elem->next = set_not_part;
	}
	PRIVATE_T_(debug)(this, "New", "added.\n");
	return &elem->data;
}

/** Removes an element associated with {data} from {this}.
 @param this: If {this} is null, returns false.
 @return Success.
 @throws SET_OUT_OF_BOUNDS
 @order amortised O(1)
 @allow */
static int T_(SetRemove)(struct T_(Set) *const this, T *const data) {
	struct PRIVATE_T_(Element) *elem;
	size_t e;
	if(!this || !data) return 0;
	elem = (struct PRIVATE_T_(Element) *)(void *)data;
	if(elem < this->array
		|| this->array + this->size <= elem
		|| elem->prev != set_not_part)
		return this->error = SET_OUT_OF_BOUNDS, 0;
	e = elem - this->array;
	PRIVATE_T_(enqueue_removed)(this, e);
	if(e >= this->size - 1) PRIVATE_T_(trim_removed)(this);
	PRIVATE_T_(debug)(this, "Remove", "removing %lu.\n", (unsigned long)e);
	return 1;
}

/** Gets an existing element by index.
 @param this: If {this} is null, returns null.
 @param idx: Index.
 @return If failed, returns a null pointer and the error condition will be set.
 @throws SET_OVERFLOW, SET_ERRNO
 @order \Theta(1)
 @allow */
static T *T_(SetGetElement)(struct T_(Set) *const this, const size_t idx) {
	struct PRIVATE_T_(Element) *elem;
	if(!this) return 0;
	if(idx >= this->size
		|| (elem = this->array + idx, elem->prev != set_not_part))
		{ this->error = SET_OUT_OF_BOUNDS; return 0; }
	return &elem->data;
}

/** Gets an index out of an existing element. Calling on an {element} that is
 not in {this} will give garbage.
 @order \Theta(1)
 @allow */
static size_t T_(SetGetIndex)(struct T_(Set) *const this,
	const T *const element) {
	return (const struct PRIVATE_T_(Element) *)(void *)element - this->array;
}

/** General iterator is a {size_t}.
 @param iterator_ptr: Set it to zero; this gets incremented. To delete in-place,
 call {<T>SetGetNext} first, then call \see{<T>SetRemove} with the first.
 @return The next {T} or null if there are none left.
 @allow */
static T *T_(SetGetNext)(struct T_(Set) *const this,
	size_t *const iterator_ptr) {
	struct PRIVATE_T_(Element) *elem = 0;
	if(!iterator_ptr || !this) return 0;
	for( ; ; ) {
		if(*iterator_ptr >= this->size) { *iterator_ptr = 0; return 0; }
		elem = this->array + (*iterator_ptr)++;
		if(elem->prev != set_not_part) break;
	}
	return &elem->data;
}

/** Reverse iterator.
 @param iterator_ptr: Set it to zero initially; this gets decremented.
 @return The previous {T} or null if there are no more.
 @allow */
static T *T_(SetGetPrevious)(struct T_(Set) *const this,
	size_t *const iterator_ptr) {
	struct PRIVATE_T_(Element) *elem = 0;
	if(!iterator_ptr || !this || !this->size) return 0;
	for( ; ; ) {
		/* this will not go into an infinite loop because all size > 0 have at
		 least one element */
		if(!*iterator_ptr || *iterator_ptr >= this->size)
			*iterator_ptr = this->size;
			elem = this->array + --(*iterator_ptr);
			if(elem->prev != set_not_part) break;
	}
	return &elem->data;
}

/** Removes all data from {this}.
 @order \Theta(1)
 @allow */
static void T_(SetClear)(struct T_(Set) *const this) {
	if(!this) return;
	this->size = 0;
	this->head = this->tail = set_null;
	PRIVATE_T_(debug)(this, "Clear", "cleared.\n");
}

/** For each element.
 @order \Theta({this}.n) \times O({action}) where
 |{this}| <= {this}.n <= |{this}+{this}.deleted|.
 @allow */
static void T_(SetForEach)(struct T_(Set) *const this, const T_(Action) action){
	size_t i;
	if(!this || !action) return;
	for(i = 0; i < this->size; i++) {
		if(this->array[i].prev != set_not_part) continue;
		action(&this->array[i].data);
	}
}

/** @return A {<T>} in the {Set} that causes the {predicate} to return false,
 or null if the {predicate} is true for every case. If {this} or {predicate} is
 null, returns null.
 @order ~ O({this}.n) \times O({predicate}) where
 |{this}| <= {this}.n <= |{this}+{this}.deleted|.
 @fixme Untested.
 @allow */
static T *T_(SetShortCircuit)(struct T_(Set) *const this,
	const T_(Predicate) predicate) {
	struct PRIVATE_T_(Element) *elem;
	size_t i;
	if(!this || !predicate) return 0;
	for(i = 0; i < this->size; i++) {
		elem = this->array + i;
		if(elem->prev != set_not_part) continue;
		if(!predicate(&elem->data)) return &elem->data;
	}
	return 0;
}

#ifdef SET_TO_STRING /* <-- print */

#ifndef SET_PRINT_THINGS /* <-- once inside translation unit */
#define SET_PRINT_THINGS

static const char *const set_cat_start     = "[ ";
static const char *const set_cat_end       = " ]";
static const char *const set_cat_alter_end = "...]";
static const char *const set_cat_sep       = ", ";
static const char *const set_cat_star      = "*";
static const char *const set_cat_null      = "null";

struct Set_SuperCat {
	char *print, *cursor;
	size_t left;
	int is_truncated;
};
static void set_super_cat_init(struct Set_SuperCat *const cat,
	char *const print, const size_t print_size) {
	cat->print = cat->cursor = print;
	cat->left = print_size;
	cat->is_truncated = 0;
	print[0] = '\0';
}
static void set_super_cat(struct Set_SuperCat *const cat,
	const char *const append) {
	size_t lu_took; int took;
	if(cat->is_truncated) return;
	took = sprintf(cat->cursor, "%.*s", (int)cat->left, append);
	if(took < 0)  { cat->is_truncated = -1; return; } /*implementation defined*/
	if(took == 0) { return; }
	if((lu_took = (size_t)took) >= cat->left)
		cat->is_truncated = -1, lu_took = cat->left - 1;
	cat->cursor += lu_took, cat->left -= lu_took;
}
#endif /* once --> */

/** Can print 4 things at once before it overwrites. One must set
 {SET_TO_STRING} to a function implementing {<T>ToString} to get this
 functionality.
 @return Prints {this} in a static buffer.
 @order \Theta(1); it has a 255 character limit; every element takes some of it.
 @allow */
static const char *T_(SetToString)(const struct T_(Set) *const this) {
	static char buffer[4][256];
	static unsigned buffer_i;
	struct Set_SuperCat cat;
	int is_first = 1;
	char scratch[12];
	size_t i;
	assert(strlen(set_cat_alter_end) >= strlen(set_cat_end));
	assert(sizeof buffer > strlen(set_cat_alter_end));
	set_super_cat_init(&cat, buffer[buffer_i],
		sizeof *buffer / sizeof **buffer - strlen(set_cat_alter_end));
	buffer_i++, buffer_i &= 3;
	if(!this) {
		set_super_cat(&cat, set_cat_null);
		return cat.print;
	}
	set_super_cat(&cat, set_cat_start);
	for(i = 0; i < this->size; i++) {
		if(this->array[i].prev != set_not_part) continue;
		if(!is_first) set_super_cat(&cat, set_cat_sep); else is_first = 0;
		PRIVATE_T_(to_string)(&this->array[i].data, &scratch),
		scratch[sizeof scratch - 1] = '\0';
		set_super_cat(&cat, scratch);
		if(cat.is_truncated) break;
	}
	sprintf(cat.cursor, "%s",
		cat.is_truncated ? set_cat_alter_end : set_cat_end);
	return cat.print; /* static buffer */
}

#endif /* print --> */

#ifdef SET_TEST /* <-- test */
#include "../test/TestSet.h" /* need this file if one is going to run tests */
#endif /* test --> */

/* prototype */
static void PRIVATE_T_(unused_coda)(void);
/** This silences unused function warnings from the pre-processor, but allows
 optimisation, (hopefully.)
 \url{ http://stackoverflow.com/questions/43841780/silencing-unused-static-function-warnings-for-a-section-of-code } */
static void PRIVATE_T_(unused_set)(void) {
	T_(Set_)(0);
	T_(Set)();
	T_(SetGetError)(0);
	T_(SetIsEmpty)(0);
	T_(SetIsElement)(0, (size_t)0);
	T_(SetSetMigrate)(0, 0);
	T_(SetReserve)(0, (size_t)0);
	T_(SetNew)(0);
	T_(SetRemove)(0, 0);
	T_(SetGetElement)(0, (size_t)0);
	T_(SetGetIndex)(0, 0);
	T_(SetGetPrevious)(0, 0);
	T_(SetGetNext)(0, 0);
	T_(SetClear)(0);
	T_(SetForEach)(0, 0);
	T_(SetShortCircuit)(0, 0);
#ifdef SET_TO_STRING
	T_(SetToString)(0);
#endif
	PRIVATE_T_(unused_coda)();
}
/** {clang}'s pre-processor is not fooled if you have one function. */
static void PRIVATE_T_(unused_coda)(void) { PRIVATE_T_(unused_set)(); }



/* un-define all macros */
#undef SET_NAME
#undef SET_TYPE
#undef CAT
#undef CAT_
#undef PCAT
#undef PCAT_
#undef T
#undef T_
#undef PRIVATE_T_
#undef T_NAME
#undef QUOTE
#undef QUOTE_
#ifdef SET_TO_STRING
#undef SET_TO_STRING
#endif
#ifdef SET_DEBUG
#undef SET_DEBUG
#endif
#ifdef SET_NDEBUG
#undef SET_NDEBUG
#undef NDEBUG
#endif
#ifdef SET_TEST
#undef SET_TEST
#endif
