#ifndef __ARC_H__
#define __ARC_H__

#include <linux/list.h>
#include <linux/hashtable.h>

/**********************************************************************
 * The arc state represents one of the m{r,f}u{g,} lists
 */
struct arc_state {
	unsigned long size;
	struct list_head head;
};

/* This structure represents an object that is stored in the cache. Consider
 * this structure private, don't access the fields directly. When creating
 * a new object, use the arc_object_init() function to initialize it. */
struct arc_object {
	struct arc_state *state;
	struct list_head head;
	struct hlist_node hash;
	unsigned long size;
};

struct arc_ops {
	/* Hash function to generate hash code */
	unsigned long (*hash) (const void *key);

	/* Compare the object with the key. */
	int (*cmp) (struct arc_object *obj, const void *key);

	/* Create a new object. The size of the new object must be know at
	 * this time. Use the arc_object_init() function to initialize
	 * the arc_object structure.
	 * */
	struct arc_object *(*create) (const void *key);

	/* Fetch the data associated with the object. */
	int (*fetch) (struct arc_object *obj);

	/* This function is called when the cache is full and we need to evict
	 * objects from the cache. Free all data associated with the object. */
	void (*evict) (struct arc_object *obj);

	/* This function is called when the object is completely removed from
	 * the cache directory. Free the object data and the object itself. */
	void (*destroy) (struct arc_object *obj);
};

/* The actual cache. */
struct arc {
	struct arc_ops *ops;
	DECLARE_HASHTABLE(hash, 12);
	unsigned long c, p;
	struct arc_state mrug, mru, mfu, mfug;
};

/* Functions to create and destroy the cache. */
struct arc *arc_create(struct arc_ops *ops, unsigned long c);
void arc_destroy(struct arc *cache);

/* Initialize a new object. To be used from the alloc() op function. */
void arc_object_init(struct arc_object *obj, unsigned long size);

/* Lookup an object in the cache. The cache automatically allocates and
 * fetches the object if it does not exists yet. */
struct arc_object *arc_lookup(struct arc *cache, const void *key);

#endif /* __ARC_H__ */
