/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free bsearch qsort */
#include <stdio.h>  /* fprintf */
#include <string.h> /* strcmp */
#include "Map.h"

/** Map kind of like Java, but dangerous and unsafe because C. This Map is
 kind of like a TreeMap, but Entries are unsorted until sorting. Memory
 management of the data is left entirely out of the Map.
 <p>
 There is no way to have dynamically allocated keys be returned for freeing.
 If you do require, you will have to keep track of them yourself. Also, if you
 change the keys with respect to {@code compare}, the Map may become invalid.
 @author Neil
 @version 1
 @since 2015 */

struct Entry {
	void *key;
	void *value;
};

struct Map {
	char         *name;
	int          capacity[2]; /* Fibonacci */
	int          size;
	int          iterate;
	int          is_sorted;
	int          (*compare)(const void *, const void *);
	void         (*destructor)(void *, void *);
	struct Entry *entries;
};

static const int fibonacci6 = 8;
static const int fibonacci7 = 13;

/* global variable: compare_keys; don't know how to make this otherwise; no way
 it's thread-safe! */
int (*compare_keys)(const void *, const void *);

/* private prototypes */
static void grow(int *a, int *b);
int compare_entries(const struct Entry *a, const struct Entry *b);

/* public */

/** "Constructs an empty list with the specified initial capacity."
 @param name		A name to use in debugging, can be null.
 @param compare		A function used for comparing keys, such as {@code strcmp}.
 @param destructor	Destructor for the elements, if you are not going to keep
					track of them (eg, {@code free}.)
 @return			An object or a null pointer, if the object couldn't be
					created. */
struct Map *Map(char *const name, int (*const compare)(const void *, const void *)) {
	struct Map *m;

	if(!compare) return 0;

	if(!(m = malloc(sizeof(struct Map)))) {
		perror("Map constructor");
		return 0;
	}
	m->name        = name ? name : "Untitled";
	m->capacity[0] = fibonacci6;
	m->capacity[1] = fibonacci7;
	m->size        = 0;
	m->iterate     = 0;
	m->is_sorted   = -1;
	m->compare     = compare;
	m->destructor  = 0;
	m->entries     = 0;
	fprintf(stderr, "Map: new \"%s,\" capacity %d, #%p.\n", name, m->capacity[0], (void *)m);
	if(!(m->entries = malloc(m->capacity[0] * sizeof(struct Entry)))) {
		perror("Map entries constructor");
		Map_(&m);
		return 0;
	}

	return m;
}

/** Destructor. Make sure you appropreately free the pointers first;
 {@see MapSetDestructor} may be useful.
 @param m_ptr	A reference to the object that is to be deleted. */
void Map_(struct Map **m_ptr) {
	struct Map *m;

	if(!m_ptr || !(m = *m_ptr)) return;
	if(m->entries) {
		MapClear(m);
		free(m->entries);
	}
	fprintf(stderr, "~Map: erase \"%s,\" #%p.\n", m->name, (void *)m);
	free(m);
	*m_ptr = m = 0;
}

/** Associates a Map with a destuctor. This is kind of important if the values
 you are storing are dynamically allocated and there is not some other data
 structure that's holding them, as well.
 @param m			The map.
 @param destructor	The destructor; it will be called key, value. */
void MapSetDestructor(struct Map *m, void (*const destructor)(void *, void *)) {
	if(!m) return;
	m->destructor = destructor;
}

/** "Increases the capacity of this Map instance, if necessary, to ensure
 that it can hold at least the number of elements specified by the minimum
 capacity argument."
 @param m	The map.
 @param min_capacity
 @return				True if the capacity increase was vaible. */
int MapEnsureCapacity(struct Map *m, const int min_capacity) {
	struct Entry *entries;
	int c0, c1;

	if(!m) return 0;

	/* we're good */
	if(m->capacity[0] >= min_capacity) return -1;

	/* re-alloc */
	c0 = m->capacity[0];
	c1 = m->capacity[1];
	while(m->capacity[0] < min_capacity) {
		grow(&c0, &c1);
		if(c1 < 0) {
			fprintf(stderr, "Map: \"%s\" %d too large for index, %d, #%p.\n", m->name, min_capacity, c0, (void *)m);
			return 0;
		}
	}
	if(!(entries = realloc(m->entries, c0 * sizeof(struct Entry)))) {
		fprintf(stderr, "Map: \"%s\" failed allocating space for capacity %d, #%p.\n", m->name, c0, (void *)m);
		return 0;
	}
	m->entries     = entries;
	m->capacity[0] = c0;
	m->capacity[1] = c1;

	return -1;
}

/** "Trims the capacity of this Map instance to be the list's current
 size. An application can use this operation to minimize the storage of an
 Map instance."
 @param m	The Map. */
void MapTrimToSize(struct Map *m) {
	struct Entry *entries;
	int c0, c1;
	
	if(!m) return;
	
	/* we're good */
	if(m->capacity[0] >= m->size) return;
	
	/* re-mloc */
	c0 = m->size;
	c1 = c0 * 2;
	if(!(entries = realloc(m->entries, c0 * sizeof(void *)))) {
		fprintf(stderr, "Map: \"%s\" failed shrinking space for capacity %d, #%p.\n", m->name, c0, (void *)m);
		return;
	}
	m->capacity[0] = c0;
	m->capacity[1] = c1;
}

/** "Sorts this list according to the order induced by the specified
 Comparator."
 @param m	The Map. */
void MapSort(struct Map *m) {
	if(!m) return;
	/*fprintf(stderr, "Map: \"%s\" sorting %d elements.\n", m->name, m->size);*/
	compare_keys = m->compare;
	qsort(m->entries, m->size, sizeof(struct Entry), (int (*)(const void *, const void *))compare_entries);
	m->is_sorted = -1;
}

/** Iterator
 @param m			The map.
 @param key_ptr		If set, receives address of key on success.
 @param value_ptr	If set, receives address of value on success.
 @return			Success. Failure means no keys left. */
int MapIterate(struct Map *m, const void **const key_ptr, void **value_ptr) {
	if(!m) return 0;
	if(m->iterate >= m->size) {
		m->iterate = 0;
		return 0;
	}
	if(key_ptr)   *key_ptr   = m->entries[m->iterate].key;
	if(value_ptr) *value_ptr = m->entries[m->iterate].value;
	m->iterate++;
	return -1;
}

/*******************************************************************************
I've tried to duplicate as much as possible from the Java SE 8 Map interface.
 ******************************************************************************/

/** "Returns the number of key-value mappings in this map." If the map contains
 more than Integer.MAX_VALUE elements, you are screwed.
 @param m	The Map.
 @return	"the number of key-value mappings in this map" */
int MapSize(const struct Map *m) {
	if(!m) return 0;
	return m->size;
}

/** "Returns true if this map contains no key-value mappings."
 @param m	The Map.
 @return	"true if this map contains no key-value mappings" */
int MapIsEmpty(const struct Map *m) {
	if(!m) return 0;
	return m->size == 0 ? -1 : 0;
}

/** "Returns true if this map contains a mapping for the specified key. More
 formally, returns true if and only if this map contains a mapping for a key k
 such that (key==null ? k==null : key.equals(k)). (There can be at most one
 such mapping.)"
 @param m	The Map.
 @param key	"key whose presence in this map is to be tested"
 @return	"true if this map contains a mapping for the specified key" */
int MapContainsKey(struct Map *m, const void *key) {
	struct Entry target, *entry;

	if(!m) return 0;
	if(!m->is_sorted) MapSort(m);

	compare_keys = m->compare;
	target.key   = (void *)key;
	entry = bsearch(&target, m->entries, m->size, sizeof(struct Entry), (int (*)(const void *, const void *))compare_entries);

	return entry == 0 ? 0 : -1;
}

/** boolean containsValue(Object value) -- boring */

/** "Returns the value to which the specified key is mapped, or null if this
 map contains no mapping for the key."
 @param m		The Map.
 @param key		"the key whose associated value is to be returned"
 @return		"the value to which the specified key is mapped, or null if this map contains no mapping for the key" */
void *MapGet(struct Map *m, const void *key) {
	struct Entry target, *entry;

	if(!m) return 0;
	if(!m->is_sorted) MapSort(m);

	/* set the global compare_keys to the Map-specific */
	compare_keys = m->compare;
	target.key   = (void *)key;
	entry = bsearch(&target, m->entries, m->size, sizeof(struct Entry), (int (*)(const void *, const void *))compare_entries);

	return entry ? entry->value : 0;
}

/** "Associates the specified value with the specified key in this map
 (optional operation). If the map previously contained a mapping for the key,
 the old value is replaced by the specified value. (A map m is said to contain
 a mapping for a key k if and only if m.containsKey(k) would return true.)"
 @param m		The Map.
 @param key		"key with which the specified value is to be associated"
 @param value	"value to be associated with the specified key"
 @return		"the previous value associated with key, or null if there was
				no mapping for key. (A null return can also indicate that the
				map previously associated null with key, if the implementation
				supports null values.)" (We do, kind of; there is no way to
				test if the return value is an error or a null element.) */
int MapPut(struct Map *m, const void *key, const void *value) {
	struct Entry target, *old_e, *entry;

	if(!m) return 0;
	if(!m->is_sorted) MapSort(m);

	compare_keys = m->compare;
	target.key   = (void *)key;
	old_e = bsearch(&target, m->entries, m->size, sizeof(struct Entry), (int (*)(const void *, const void *))compare_entries);
	if(!old_e) {
		if(!MapEnsureCapacity(m, m->size + 1)) return 0;
		entry = &m->entries[m->size++];
		/*fprintf(stderr, "Map::put: \"%s\" new element; map size %d, #%p.\n", m->name, m->size, (void *)entry);*/
	} else {
		entry = old_e;
		/*fprintf(stderr, "Map::put: \"%s\" replacing element #%p.\n", m->name, (void *)old_e);*/
		if(m->destructor) (*m->destructor)(old_e->key, old_e->value);
	}
	entry->key   = (void *)key;
	entry->value = (void *)value;

	m->is_sorted = 0;

	return -1;
}

/** "Removes the mapping for a key from this map if it is present (optional
 operation). More formally, if this map contains a mapping from key k to value
 v such that (key==null ? k==null : key.equals(k)), that mapping is removed.
 (The map can contain at most one such mapping.)
 <p>
 "If this map permits null values, then a return value of null does not
 necessarily indicate that the map contained no mapping for the key; it's also
 possible that the map explicitly mapped the key to null."
 <p>
 If there's a destructor, the value will be pointing at garbage.
 @param m		The Map.
 @param key		"key whose mapping is to be removed from the map"
 @return		"The previous value associated with key, or null if there was
				no mapping for key." */
void *MapRemove(struct Map *m, const void *key) {
	int i;
	struct Entry target, *entry;
	void *value;

	if(!m) return 0;
	if(!m->is_sorted) MapSort(m);

	compare_keys = m->compare;
	target.key   = (void *)key;
	entry = bsearch(&target, m->entries, m->size, sizeof(struct Entry), (int (*)(const void *, const void *))compare_entries);
	if(!entry) return 0;
	value = entry->value;
	if(m->destructor) (*m->destructor)(entry->key, entry->value);
	/* fixme: inefficient */
	for(i = entry - m->entries; i < m->size - 1; i++) m->entries[i] = m->entries[i + 1];
	m->size--;

	return value;
}

/** "Copies all of the mappings from the specified map to this map (optional
 operation). The effect of this call is equivalent to that of calling put(k, v)
 on this map once for each mapping from key k to value v in the specified map.
 The behavior of this operation is undefined if the specified map is modified
 while the operation is in progress."
 @param m		The Map.
 @param cat
 @return		""
 tricky */
/*void MapPutAll(struct Map *m, const struct Map cat) {
	v i;
	
	if(!m) return 0;
	
	return i;
} */

/** "Removes all of the mappings from this map (optional operation). The map
 will be empty after this call returns."
 @param m	The Map. */
void MapClear(struct Map *m) {
	if(!m) return;

	if(m->destructor) {
		int i;
		for(i = 0; i < m->size; i++) {
			(*m->destructor)(m->entries[i].key, m->entries[i].value);
		}
	}
	m->size = 0;
}

/* Set<K> keySet(); nope */
/* Collection<V> values(); nope */
/* Set<Map.Entry<K,V>> entrySet(); uhhhh, umm, no, I want to not expose Entry */
/* boolean equals(Object o); I couldn't see the use . . . and then defining = */
/* int hashCode(); nope */

/** "Returns the value to which the specified key is mapped, or defaultValue if
 this map contains no mapping for the key."
 @param m				The Map.
 @param key				"the key whose associated value is to be returned"
 @param defaultValue	"the default mapping of the key"
 @return				"the value to which the specified key is mapped, or
						defaultValue if this map contains no mapping for the
						key" */
void *MapGetOrDefault(struct Map *m, const void *key, const void *defaultValue) {
	struct Entry target, *entry;

	if(!m) return (void *)defaultValue;
	if(!m->is_sorted) MapSort(m);

	compare_keys = m->compare;
	target.key   = (void *)key;
	entry = bsearch(&target, m->entries, m->size, sizeof(struct Entry), (int (*)(const void *, const void *))compare_entries);

	return entry ? entry->value : (void *)defaultValue;

}

/** "Performs the given action for each entry in this map until all entries
 have been processed." The order is the order internally.
 @param m		The Map.
 @param action	"The action to be performed for each entry" */
void MapForEach(const struct Map *m, void (*const action)(const void *, void *)) {
	int i;

	if(!m || !action) return;

	for(i = 0; i < m->size; i++) (*action)(m->entries[i].key, m->entries[i].value);
}

/** Short-circuits is-each. (Just made this up, not in Java 8.)
 <p>
 Fixme: untested!
 @param m			The Map.
 @param predicate	A boolean function.
 @return			The result. */
int MapIsEach(const struct Map *m, int (*const predicate)(void *const, void *)) {
	int i;

	if(!m || !predicate) return 0;

	for(i = 0; i < m->size; i++) {
		if(!(*predicate)(m->entries[i].key, m->entries[i].value)) return 0;
	}

	return -1;
}

/** "Replaces each entry's value with the result of invoking the given function
 on that entry until all entries have been processed."
 <p>
 You will have to do freeing yourself!
 <p>
 Fixme: untested!
 @param m			The Map.
 @param predicate	The function specifying whether to replace; can be null.
 @param replace		The function performing the replacement. */
void MapReplaceAll(struct Map *m, int (*const predicate)(void *const, void *), void *(*const replace)(void *const, void *)) {
	int i;

	if(!m || !replace) return;

	for(i = 0; i < m->size; i++) {
		if(predicate && !(*predicate)(m->entries[i].key, m->entries[i].value)) continue;
		m->entries[i].value = (*replace)(m->entries[i].key, m->entries[i].value);
	}
}

/** "Removes ml of the elements of this collection that satisfy the given
 predicate." (Just made this up, not in Java 8.)
 <p>
 Fixme: untested!
 @param m		The Map.
 @param filter	The predicate.
 @param remove	Done to the removed elementsl; can be null.
 @return		The number of elements were removed. */
void MapRemoveIf(struct Map *m, int (* const filter)(void *const, void *)) {
	int i, j;
	int ret = 0;
	
	if(!m) return;
	
	for(i = 0; i < m->size; i++) {
		if((*filter)(m->entries[i].key, m->entries[i].value)) {
			if(m->destructor) (*m->destructor)(m->entries[i].key, m->entries[i].value);
			for(j = i; j < m->size - 1; j++) m->entries[j] = m->entries[j + 1];
			m->size--;
			i--;
			ret++;
		}
	}
}

/** default V putIfAbsent(K key, V value); use if(!get()) put(), fixme */
/** default boolean remove(Object key, Object value); not very useful? fixme */
/** "Replaces the entry for the specified key only if currently mapped to the
 specified value."
 @param m		The Map.
 @param key		"key with which the specified value is associated"
 @param old		"value expected to be associated with the specified key"
 @return		"value to be associated with the specified key"
 Man that's just lazy */
/*v MapReplace(struct Map *m, ) {
	v i;
	
	if(!m) return 0;
	
	return i;
}*/
/** default V replace(K key, V value); use if(containsKey()) put(); fixme */
/** default V computeIfAbsent(K key, Function<? super K,? extends V> mappingFunction); use if(!containsKey()) put(); fixme */
/** default V computeIfPresent(K key,
 BiFunction<? super K,? super V,? extends V> remappingFunction); use if(get()) put(); fixme */
/** default V compute(K key,
 BiFunction<? super K,? super V,? extends V> remappingFunction); I don't even know what that does. */
/** default V merge(K key, V value, BiFunction<? super V,? super V,? extends V> remappingFunction); fixme */

#if 0
/** Prints the map to stderr.
 @param m		The Map. */
void MapPrint(struct Map *m) {
	int i;

	if(!m) {
		fprintf(stderr, "Map::print: null.\n");
		return;
	}

	fprintf(stderr, "Map::print: Map %s:\n", m->name);
	for(i = 0; i < m->size; i++) {
		fprintf(stderr, "Map::print: #%p -> #%p\n", m->entries[i].key, m->entries[i].value);
	}
}

/** Accessor.
 @param entry	Entry.
 @return		key */
void *EntryKey(const struct Entry *entry) {
	if(!entry) return 0;
	return entry->key;
}

/** Provided for you to sort strings. */
int EntryStrcmp(const struct Entry *a, const struct Entry *b) {
	EntryPrint(a);
	EntryPrint(b);
	return strcmp(a->key, b->key);
}

void EntryPrint(const struct Entry *e) {
	if(!e) {
		fprintf(stderr, "Entry: (Null)\n");
		return;
	}
	fprintf(stderr, "Entry: (#%p -> #%p)\n", e->key, e->value);
}
#endif

/* private */

/** Fibonacci growing thing. */
static void grow(int *a, int *b) {
	*a ^= *b;
	*b ^= *a;
	*a ^= *b;
	*b += *a;
}

/** Compare the keys of the Entry */
int compare_entries(const struct Entry *a, const struct Entry *b) {
	/*fprintf(stderr, "<%s,%s>\n", a->key, b->key);*/
	return compare_keys(a->key, b->key);
}
