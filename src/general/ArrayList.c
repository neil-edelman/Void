/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include "ArrayList.h"

/** ArrayList like in Java, but dangerous and unsafe because C.
 <p>
 Incomplete and untested!
 @author Neil
 @version 1
 @since 2015 */

struct ArrayList {
	int  capacity[2]; /* Fibonacci */
	int  size;
	int  is_sorted;
	int  width;
	/*int  (*compare)(const void *, const void *);*/
	void **array;
};

static void grow(int *a, int *b);

static const int fibonacci6 = 8;
static const int fibonacci7 = 13;

/* public */

/** "Constructs an empty list with the specified initial capacity."
 @param width	The size of the objects that go in the list.
 @return		An object or a null pointer, if the object couldn't be created. */
struct ArrayList *ArrayList(const int width/*int (*compare)(const void *, const void *)*/) {
	struct ArrayList *al;

	if(width <= 0) return 0;
	if(!(al = malloc(sizeof(struct ArrayList)))) {
		perror("ArrayList constructor");
		return 0;
	}
	al->capacity[0] = fibonacci6;
	al->capacity[1] = fibonacci7;
	al->size        = 0;
	al->is_sorted   = -1;
	al->array       = 0;
	al->width       = width;
	/*al->compare     = compare;*/
	fprintf(stderr, "ArrayList: new, capacity %d, #%p.\n", al->capacity[0], (void *)al);
	if(!(al->array = malloc(al->capacity[0] * sizeof(void *)))) {
		perror("ArrayList array constructor");
		ArrayList_(&al);
		return 0;
	}

	return al;
}

/** Destructor.
 @param al_ptr	A reference to the object that is to be deleted. */
void ArrayList_(struct ArrayList **al_ptr) {
	struct ArrayList *al;

	if(!al_ptr || !(al = *al_ptr)) return;
	fprintf(stderr, "~ArrayList: erase, #%p.\n", (void *)al);
	if(al->array) free(al->array);
	free(al);
	*al_ptr = al = 0;
}

/** "Appends the specified element to the end of this list."
 <p>
 Fixme: untested.
 @param al	The ArrayList.
 @param e	"element to be appended to this list"
 @return	True if the element could be added. */
int ArrayListAdd(struct ArrayList *al, void *e) {
	if(!al) return 0;
	if(!ArrayListEnsureCapacity(al, al->size + 1)) return 0;
	al->array[al->size++] = e;
	al->is_sorted = 0;
	return -1;
}

/** "Inserts the specified element at the specified position in this list."
 <p>
 Fixme: untested.
 @param al		The ArrayList.
 @param index	
 @param e		"element to be appended to this list"
 @return		True if the element could be added. */
int ArrayListIndexAdd(struct ArrayList *al, const int index, void *const e) {
	int i;

	if(!al || index < 0 || index > al->size) return 0;
	if(!ArrayListEnsureCapacity(al, al->size + 1)) return 0;
	for(i = al->size++; i > index; i--) al->array[i + 1] = al->array[i];
	al->array[index] = e;
	al->is_sorted = 0;
	return -1;
}

/** Concatenates another ArrayList onto this one.
 <p>
 Fixme: untested.
 @param al	The ArrayList.
 @param c	Another ArrayList.
 @return	Success. */
int ArrayListAddAll(struct ArrayList *al, const struct ArrayList *c) {
	int i, j;

	if(!al) return 0;
	if(!c) return -1;

	if(!ArrayListEnsureCapacity(al, al->size + c->size)) return 0;
	for(i = al->size, j = 0; j < c->size; i++, j++) {
		al->array[i] = c->array[j];
		al->size++;
	}

	return -1;
}

/** "Inserts all of the elements in the specified collection into this list,
 starting at the specified position. Shifts the element currently at that
 position (if any) and any subsequent elements to the right."
 <p>
 Fixme: untested.
 @param al		The ArrayList.
 @param index	"index at which to insert the first element"
 @param c		"element to be appended to this list"
 @return		Success. */
int ArrayListIndexAddAll(struct ArrayList *al, const int index, const struct ArrayList *c) {
	int i, j;

	if(!al || index < 0 || index > al->size) return 0;
	if(!c) return -1;

	if(!ArrayListEnsureCapacity(al, al->size + c->size)) return 0;
	for(i = al->size - 1, j = al->size - 1 + c->size - 1; i >= index; i--, j--) {
		al->array[j] = al->array[i];
	}
	for(j = 0; j < c->size; i++, j++) {
		al->array[i] = c->array[j];
		al->size++;
	}

	return -1;
}

/** "Removes all of the elements from this list." If you don't keep track of
 them somewhere else, do ArrayListForEach(al, &free) before.
 @param al	The ArrayList. */
void ArrayListClear(struct ArrayList *al) {
	if(!al) return;

	al->size = 0;
}

/** ArrayList::contains? no. */

/** "Increases the capacity of this ArrayList instance, if necessary, to ensure
 that it can hold at least the number of elements specified by the minimum
 capacity argument."
 @param min_capacity
 @return				True if the capacity increase was vaible. */
int ArrayListEnsureCapacity(struct ArrayList *al, const int min_capacity) {
	void **array;
	int c0, c1;

	if(!al) return 0;

	/* we're good */
	if(al->capacity[0] >= min_capacity) return -1;

	/* re-alloc */
	c0 = al->capacity[0];
	c1 = al->capacity[1];
	while(al->capacity[0] < min_capacity) {
		grow(&c0, &c1);
		if(c1 < 0) {
			fprintf(stderr, "ArrayList: %d too large for index, %d, #%p.\n", min_capacity, c0, (void *)al);
			return 0;
		}
	}
	if(!(array = realloc(al->array, c0 * sizeof(void *)))) {
		fprintf(stderr, "ArrayList: failed allocating space for capacity %d, #%p.\n", c0, (void *)al);
		return 0;
	}
	al->capacity[0] = c0;
	al->capacity[1] = c1;

	return -1;
}

/** "Performs the given action for each element."
 @param al		The ArrayList.
 @param action	"The action to be performed for each element." */
void ArrayListForEach(const struct ArrayList *al, void (* const action)(void *)) {
	int i;

	if(!al || !action) return;

	for(i = 0; i < al->size; i++) (*action)(al->array[i]);
}

/** "Returns the element at the specified position in this list."
 @param al		The ArrayList.
 @param index	"index of the element to return"
 @return		"the element at the specified position in this list" or 0 */
void *ArrayListGet(const struct ArrayList *al, const int index) {

	if(!al || index < 0 || index >= al->size) return 0;

	return al->array[index];
}

/** indexOf, lastIndexOf, boring */

/** "Returns true if this list contains no elements."
 @param al	The ArrayList.
 @return	"true if this list contains no elements" */
int ArrayListIsEmpty(const struct ArrayList *al) {
	if(!al) return 0;
	return al->size == 0 ? -1 : 0;
}

/** "Removes the element at the specified position in this list. Shifts any
 subsequent elements to the left (subtracts one from their indices)."
 <p>
 Fixme: untested.
 @param al		The ArrayList.
 @param index	"the index of the element to be removed"
 @return		The element removed. */
void *ArrayListRemove(struct ArrayList *al, const int index) {
	void *object;
	int i;

	if(!al || index < 0 || index > al->size - 1) return 0;
	object = al->array[index];
	for(i = index; i < al->size - 1; i++) al->array[i] = al->array[i + 1];
	al->size--;
	return object;
}

/** "Removes the first occurrence of the specified element from this list, if
 it is present. If the list does not contain the element, it is unchanged."
 <p>
 Fixme: untested.
 <p>
 Fixme: boring; don't use.
 @param al	The ArrayList.
 @param o	The object.
 @return	True if the element was removed. */
int ArrayListObjectRemove(struct ArrayList *al, const void *o) {
	int i;

	if(!al) return 0;
	for(i = 0; i < al->size; i++) {
		if(al->array[i] == o) break;
	}
	if(i == al->size) return 0;
	for( ; i < al->size - 1; i++) al->array[i] = al->array[i + 1];
	al->size--;
	return -1;
}

/** "Removes all of the elements of this collection that satisfy the given
 predicate."
 <p>
 Fixme: untested.
 @param al		The ArrayList.
 @param filter	The predicate.
 @param remove	Done to the removed elements (eg, free?)
 @return		The number of elements were removed. */
int ArrayListRemoveIf(struct ArrayList *al, int (* const filter)(void *), void  (* const remove)(void *)) {
	void *r;
	int ret = 0;
	int i;

	if(!al) return 0;

	for(i = 0; i < al->size; i++) {
		if((*filter)(al->array[i])) {
			r = ArrayListRemove(al, i);
			(*remove)(r);
			i--;
			ret++;
		}
	}
	return ret;
}

/** void removeRange(int fromIndex, int toIndex),
 void replaceAll(UnaryOperator<E> operator),
 int retainAll(Collection<?> c), boring */

/** "Replaces the element at the specified position in this list with the
 specified element."
 @param al		The ArrayList.
 @param index	"index of the element to replace"
 @param element	"element to be stored at the specified position"
 @return		"the element previously at the specified position" */
void *ArrayListSet(struct ArrayList *al, const int index, void *const element) {
	void *prev;

	if(!al || index < 0 || index >= al->size) return 0;

	prev = al->array[index];
	al->array[index] = element;

	return prev;
}

/** "Returns the number of elements in this list."
 @param al	The ArrayList.
 @return	"the number of elements in this list" */
int ArrayListSize(const struct ArrayList *al) {
	if(!al) return 0;
	return al->size;
}

/** "Sorts this list according to the order induced by the specified
 Comparator."
 @param al		The ArrayList.
 @param c	"c - the Comparator used to compare list elements." */
void ArrayListSort(const struct ArrayList *al, int (* const c)(const void *, const void *)) {
	if(!al) return;
	qsort(al->array, al->size, al->width, c);
}

/** "Trims the capacity of this ArrayList instance to be the list's current
 size. An application can use this operation to minimize the storage of an
 ArrayList instance."
 @param al	The ArrayList. */
void ArrayListTrimToSize(struct ArrayList *al) {
	void **array;
	int c0, c1;

	if(!al) return;

	/* we're good */
	if(al->capacity[0] >= al->size) return;

	/* re-alloc */
	c0 = al->size;
	c1 = c0 * 2;
	if(!(array = realloc(al->array, c0 * sizeof(void *)))) {
		fprintf(stderr, "ArrayList: failed shrinking space for capacity %d, #%p.\n", c0, (void *)al);
		return;
	}
	al->capacity[0] = c0;
	al->capacity[1] = c1;
}

/* private */

/** Fibonacci growing thing. */
static void grow(int *a, int *b) {
	*a ^= *b;
	*b ^= *a;
	*a ^= *b;
	*b += *a;
}
