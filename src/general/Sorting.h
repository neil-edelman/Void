int bubble(void **base_ptr, int (*compare)(const void *, const void *), void **(*address_next)(void *const));
int isort(void **base_ptr, int (*compare)(const void *, const void *), void **(*address_prev)(void *const), void **(*address_next)(void *const));
void inotify(void **base_ptr, void *element, int (*compare)(const void *, const void *), void **(*address_prev)(void *const), void **(*address_next)(void *const));
