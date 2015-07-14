#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <time.h>
#include "../general/Sorting.h"

struct Widget {
	int garbage;
	float key;
	struct Widget *prev, *next;
} widgets[10];
static const int widgets_size = sizeof(widgets) / sizeof(struct Widget);

int compare(const struct Widget *a, const struct Widget *b) {
	/*if(a->key - b->key > 0) printf("Elements %d and %d are out-of-order.\n", a->key, b->key);
	return a->key - b->key;*/
	return a->key > b->key;
}

struct Widget **address_prev(struct Widget *const a) {
	return &a->prev;
}

struct Widget **address_next(struct Widget *const a) {
	return &a->next;
}

void print(struct Widget *const *head_ptr) {
	struct Widget *node = (head_ptr) ? *head_ptr : 0;
	int i = 0;

	printf("{ ");
	while(node) {
		printf("%u)%4.1f ", (int)(node - widgets), node->key);
		node = node->next;
		if(++i > widgets_size + 8) {
			printf("...");
			break;
		}
	}
	printf("}\n");
}

int main(void) {
	struct Widget *head = &widgets[0];
	int i, swaps;

	srand(clock());
	rand(); /* weird */
	for(i = 0; i < widgets_size; i++) {
		widgets[i].key = 100.0f * rand() / RAND_MAX;
		widgets[i].prev = (i > 0) ? &widgets[i - 1] : 0;
		widgets[i].next = (i < widgets_size - 1) ? &widgets[i + 1] : 0;
	}
	print(&head);
	/*do {
		swaps = isort(&head, &compare, &address_prev, &address_next);
		print(&head);
	} while(swaps);*/
	/* "(void *) is a generic pointer" said no one ever :[ */
	swaps = isort((void **)&head, (int (*)(const void *, const void *))&compare, (void **(*)(void *const))&address_prev, (void **(*)(void *const))&address_next);
	print(&head);
	printf("Difficutly: %u.\n", swaps);
	printf("Changing 5: from %3.1f.\n", widgets[5].key);
	widgets[5].key = 100.0f * rand() / RAND_MAX;
	printf("Changing 5: to %3.1f.\n", widgets[5].key);
	print(&head);
	printf("Notify:\n");
	inotify((void **)&head, (void *)&widgets[5], (int (*)(const void *, const void *))&compare, (void **(*)(void *const))&address_prev, (void **(*)(void *const))&address_next);
	print(&head);

	return EXIT_SUCCESS;
}
