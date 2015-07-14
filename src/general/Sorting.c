/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include "Sorting.h"

/* Sorting algoritms!

 @author Neil
 @version	3.2, 2015-06
 @since		3.2, 2015-06 */

/* public */

/** Iteration-separated linked-list bubble sort; for when we don't have to get
 the right answer right away. Works with a singly-linked list.
 @param base_ptr		Pointer to the first element or null.
 @param compare			Compares two elements; switches them if it returns
						positive.
 @param address_next	Given an item, returns the address of the next item.
						Must be valid.
 @returns				The number of out-of-place items. */
int bubble(void **base_ptr, int (*compare)(const void *, const void *), void **(*address_next)(void *const)) {
	void *b, *c, **a_next, **b_next, **c_next;
	int out_of_place = 0;

	if(!(a_next = base_ptr) || !(b = *a_next)) return 0;
	for( ; ; ) {
		if(!(c = *(b_next = address_next(b)))) break;
		if(compare(b, c) > 0) {
			/* they're in the wrong order -- swap b and c */
			c_next = address_next(c);
			*a_next = c;
			*b_next = *c_next;
			*c_next = b;
			out_of_place++;
			a_next = c_next;
			continue;
		}
		a_next = b_next;
		b      = c;
	}
	return out_of_place;
}

/** Insertion sort. Works with a doubly-linked list.
 @param base_ptr		Pointer to the first element or null.
 @param compare			Compares two elements; switches them if it returns
						positive.
 @param address_prev	Given an item, returns the address of the previous
						item. Must be valid.
 @param address_next	Given an item, returns the address of the next item.
						Must be valid.
 @returns				m + n in {@code O(n + m)}. */
int isort(void **base_ptr, int (*compare)(const void *, const void *), void **(*address_prev)(void *const), void **(*address_next)(void *const)) {
	void *b, *c, *d, **b_next, **c_prev, **c_next, **d_prev;
	void *lte_c, *gt_c, **lte_c_next, **gt_c_prev;
	int out_of_place = 0;

	/* if 0-element list */
	if(!base_ptr || !(b = *base_ptr)) return 0;

	for( ; ; ) {

		/* if no next element */
		if(!(c = *(b_next = address_next(b)))) break;

		/* they're in the wrong order */
		if(compare(b, c) > 0) {

			/* take node c out */
			c_prev = address_prev(c);
			c_next = address_next(c);
			if((d = *b_next = *c_next)) *(d_prev = address_prev(d)) = b;

			/* keep comparing it to previous in the sorted list until it fits */
			gt_c = b;
			while((lte_c = *(gt_c_prev = address_prev(gt_c)))) {
				if(compare(lte_c, c) <= 0) break;
				gt_c = lte_c;
				out_of_place++;
			}
			*c_next = gt_c;
			*c_prev = lte_c;
			*gt_c_prev = c;
			if(lte_c) {
				*(lte_c_next = address_next(lte_c)) = c;
			} else {
				*base_ptr = c;
			}
			out_of_place++;

			continue;
		}

		b = c;
	}

	return out_of_place;
}

/** One element has changed it's value. */
void inotify(void **base_ptr, void *element, int (*compare)(const void *, const void *), void **(*address_prev)(void *const), void **(*address_next)(void *const)) {
	void **prev, **next, *a, *c;

	if(!base_ptr || !*base_ptr || !element) return;

	/* the next and the previous */
	a = *(prev = address_prev(element));
	c = *(next = address_next(element));

	/* compare them to find out what direction it's moving */
	if(a        && compare(a, element) > 0) {
		void *gt_e, *lte_e, **lte_e_next, **gt_e_prev;

		/* moving down the list -- remove it from the list */
		if(a) *address_next(a) = c;
		if(c) *address_prev(c) = a;

		/* keep comparing it to previous in the sorted list until it fits */
		gt_e = a;
		while((lte_e = *(gt_e_prev = address_prev(gt_e)))) {
			if(compare(element, lte_e) > 0) break;
			gt_e = lte_e;
		}
		*prev = lte_e;
		*next = gt_e;
		*gt_e_prev                                    = element;
		if(lte_e) *(lte_e_next = address_next(lte_e)) = element;
		else      *base_ptr                           = element;
	} else if(c && compare(element, c) > 0) {
		void *lt_e, *gte_e, **lt_e_next, **gte_e_prev;

		/* moving up the list -- remove it from the list */
		if(a) *address_next(a) = c;
		else if(c) *base_ptr   = c;
		else return; /* one element list, notification does nothing */
		if(c) *address_prev(c) = a;

		/* keep comparing it to next in the sorted list until it fits */
		lt_e = c;
		while((gte_e = *(lt_e_next = address_next(lt_e)))) {
			if(compare(gte_e, element) > 0) break;
			lt_e = gte_e;
		}
		*prev = lt_e;
		*next = gte_e;
		if(gte_e) *(gte_e_prev = address_prev(gte_e)) = element;
		*lt_e_next                                    = element;
	}
}
