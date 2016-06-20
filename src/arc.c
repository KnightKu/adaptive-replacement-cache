#include "arc.h"
#include <linux/slab.h>

#ifndef MAX
#define MAX(a, b) ( (a) > (b) ? (a) : (b) )
#endif

#ifndef MIN
#define MIN(a, b) ( (a) < (b) ? (a) : (b) )
#endif

static void arc_hash_insert(struct arc *cache, const void *key, struct arc_object *obj)
{
	unsigned long hashval = cache->ops->hash(key) % HASH_SIZE(cache->hash);

	hash_add(cache->hash, &obj->hash, hashval);
}

static struct arc_object *arc_hash_lookup(struct arc *cache, const void *key)
{
	struct arc_object *obj;
	int bulk;

	hash_for_each(cache->hash, bulk, obj, hash)
		if (cache->ops->cmp(obj, key) == 0)
			return obj;

	return NULL;
}

/* Initialize a new object with this function. */
void arc_object_init(struct arc_object *obj, unsigned long size)
{
	obj->state = NULL;
	obj->size = size;

	INIT_LIST_HEAD(&obj->head);
	INIT_HLIST_NODE(&obj->hash);
}
EXPORT_SYMBOL(arc_object_init);

/* Forward-declaration needed in arc_move(). */
static void arc_balance(struct arc *cache, unsigned long size);

/* Move the object to the given state. If the state transition requires,
* fetch, evict or destroy the object. */
static struct arc_object *arc_move(struct arc *cache,
				   struct arc_object *obj,
				   struct arc_state *state)
{
	if (obj->state) {
		obj->state->size -= obj->size;
		list_del(&obj->head);
	}

	if (state == NULL) {
		/* The object is being removed from the cache,
		 * destroy it. */
		hash_del(&obj->hash);
		cache->ops->destroy(obj);
		return NULL;
	} else {
		if (state == &cache->mrug || state == &cache->mfug) {
			/* The object is being moved to one of the ghost lists,
			 * evict the object from the cache.
			 * */
			cache->ops->evict(obj);
		} else if (obj->state != &cache->mru &&
			   obj->state != &cache->mfu) {
			 /* The object is being moved from one of the ghost
			 * lists into the MRU or MFU list, fetch the object
			 * into the cache. */
			arc_balance(cache, obj->size);
			if (cache->ops->fetch(obj)) {
				/* If the fetch fails, put the object back
				 * to the list it was in before. */
				obj->state->size += obj->size;
				list_add(&obj->head, &obj->state->head);
				return NULL;
			}
		}
		list_add(&obj->head, &state->head);
		obj->state = state;
		obj->state->size += obj->size;
	}

	return obj;
}

/* Return the LRU element from the given state. */
static struct arc_object *arc_state_lru(struct arc_state *state)
{
	struct list_head *head = state->head.prev;

	return list_entry(head, struct arc_object, head);
}

/* Balance the lists so that we can fit an object with the given size into
 * the cache. */
static void arc_balance(struct arc *cache, unsigned long size)
{
	/* First move objects from MRU/MFU to their respective ghost lists. */
	while (cache->mru.size + cache->mfu.size + size > cache->c) {
		if (cache->mru.size > cache->p) {
			struct arc_object *obj = arc_state_lru(&cache->mru);

			arc_move(cache, obj, &cache->mrug);
		} else if (cache->mfu.size > 0) {
			struct arc_object *obj = arc_state_lru(&cache->mfu);

			arc_move(cache, obj, &cache->mfug);
		} else {
            		break;
		}
	}

	/* Then start removing objects from the ghost lists. */
	while (cache->mrug.size + cache->mfug.size > cache->c) {
		if (cache->mfug.size > cache->p) {
			struct arc_object *obj = arc_state_lru(&cache->mfug);

			arc_move(cache, obj, NULL);
		} else if (cache->mrug.size > 0) {
			struct arc_object *obj = arc_state_lru(&cache->mrug);

			arc_move(cache, obj, NULL);
		} else {
			break;
		}
	}
}


/* Create a new cache. */
struct arc *arc_create(struct arc_ops *ops, unsigned long c)
{
	struct arc *cache = kzalloc(sizeof(struct arc), GFP_KERNEL);

	if (!cache)
		return NULL;

	cache->ops = ops;

	hash_init(cache->hash);

	cache->c = c;
	cache->p = c >> 1;

	INIT_LIST_HEAD(&cache->mrug.head);
	INIT_LIST_HEAD(&cache->mru.head);
	INIT_LIST_HEAD(&cache->mfu.head);
	INIT_LIST_HEAD(&cache->mfug.head);

	return cache;
}
EXPORT_SYMBOL(arc_create);

/* Destroy the given cache. Free all objects which remain in the cache. */
void arc_destroy(struct arc *cache)
{
	struct arc_object *obj;
    
	list_for_each_entry(obj, &cache->mru.head, head)
		arc_move(cache, obj, NULL);
	list_for_each_entry(obj, &cache->mrug.head, head)
		arc_move(cache, obj, NULL);
	list_for_each_entry(obj, &cache->mfu.head, head)
		arc_move(cache, obj, NULL);
	list_for_each_entry(obj, &cache->mfug.head, head)
		arc_move(cache, obj, NULL);

	kfree(cache);
}
EXPORT_SYMBOL(arc_destroy);

/* Lookup an object with the given key. */
struct arc_object *arc_lookup(struct arc *cache, const void *key)
{
	struct arc_object *obj = arc_hash_lookup(cache, key);

        if (!obj) {
		obj = cache->ops->create(key);
		if (!obj)
			return NULL;

		/* New objects are always moved to the MRU list. */
		arc_hash_insert(cache, key, obj);
		return arc_move(cache, obj, &cache->mru);
	}

	if (obj->state == &cache->mru || obj->state == &cache->mfu) {
		/* Object is already in the cache, move it to the head of the
		 * MFU list. */
		return arc_move(cache, obj, &cache->mfu);
	} else if (obj->state == &cache->mrug) {
		cache->p = MIN(cache->c,
			   cache->p + MAX(cache->mfug.size / cache->mrug.size,
			   1));
		return arc_move(cache, obj, &cache->mfu);
	} else if (obj->state == &cache->mfug) {
		cache->p = MAX(0, cache->p - MAX(cache->mrug.size / cache->mfug.size, 1));
		return arc_move(cache, obj, &cache->mfu);
	} else {
		printk(KERN_ERR"Invalid arc list.\n");
		return NULL;
	}
}
EXPORT_SYMBOL(arc_lookup);
