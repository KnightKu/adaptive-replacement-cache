#include "arc.h"
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/slab.h>

/* This is the object we're managing. It has a name (sha1)
* and some data. This data will be loaded when ARC instruct
* us to do so. */
struct object {
	unsigned char sha1[20];
	struct arc_object entry;

	void *data;
};

unsigned char objname(struct arc_object *entry)
{
	struct object *obj = container_of(entry, struct object, entry);

	return obj->sha1[0];
}

/**
* Here are the operations implemented
*/

static unsigned long __op_hash(const void *key)
{
	const unsigned char *sha1 = key;
	return sha1[0];
}

static int __op_compare(struct arc_object *e, const void *key)
{
	struct object *obj = container_of(e, struct object, entry);

	return memcmp(obj->sha1, key, 20);
}

static struct arc_object *__op_create(const void *key)
{
	struct object *obj = kmalloc(sizeof(struct object), GFP_KERNEL);

	if (!obj)
		return NULL;

	memcpy(obj->sha1, key, 20);
	obj->data = NULL;

	arc_object_init(&obj->entry, prandom_u32() % 100);
	return &obj->entry;
}

static int __op_fetch(struct arc_object *e)
{
	struct object *obj = container_of(e, struct object, entry);

	obj->data = kzalloc(200, GFP_KERNEL);

	return 0;
}

static void __op_evict(struct arc_object *e)
{
	struct object *obj = container_of(e, struct object, entry);

	kfree(obj->data);
}

static void __op_destroy(struct arc_object *e)
{
	struct object *obj = container_of(e, struct object, entry);

	kfree(obj);
}

static struct arc_ops ops = {
	.hash		= __op_hash,
	.cmp		= __op_compare,
	.create		= __op_create,
	.fetch		= __op_fetch,
	.evict		= __op_evict,
	.destroy	= __op_destroy
};

static void stats(struct arc *s)
{
	int i = 0;
	struct arc_object *arc_obj;
	struct object *obj;

	list_for_each_entry(arc_obj, &s->mrug.head, head) {
		WARN_ON(arc_obj->state == &s->mrug);
		obj = container_of(arc_obj, struct object, entry);

		printk(KERN_ERR"[%02x]", obj->sha1[0]);
	}

	printk(KERN_ERR" + ");

	list_for_each_entry(arc_obj, &s->mru.head, head) {
		WARN_ON(arc_obj->state == &s->mrug);
		obj = container_of(arc_obj, struct object, entry);

		printk(KERN_ERR"[%02x]", obj->sha1[0]);

		if (i++ == s->p)
			printk(KERN_ERR" # ");
	}

	printk(KERN_ERR" + ");

	list_for_each_entry(arc_obj, &s->mfu.head, head) {
		WARN_ON(arc_obj->state == &s->mrug);
		obj = container_of(arc_obj, struct object, entry);

		printk(KERN_ERR"[%02x]", obj->sha1[0]);

		if (i++ == s->p)
			printk(KERN_ERR" # ");
	}
	if (i == s->p)
		printk(KERN_ERR" # ");
	printk(KERN_ERR" + ");

	list_for_each_entry(arc_obj, &s->mfug.head, head) {
		WARN_ON(arc_obj->state == &s->mrug);
		obj = container_of(arc_obj, struct object, entry);

		printk(KERN_ERR"[%02x]", obj->sha1[0]);
	}
	printk(KERN_ERR"\n");
}

#define MAXOBJ 16

static void __exit arc_exit(void) {}

static int __init arc_init(void)
{
	int i;
	unsigned char sha1[MAXOBJ][20];
	struct arc *s = arc_create(&ops, 300);


	for (i = 0; i < MAXOBJ; ++i) {
		memset(sha1[i], 0, 20);
		sha1[i][0] = i;
	}

	for (i = 0; i < 4 * MAXOBJ; ++i) {
		unsigned char *cur = sha1[prandom_u32() & (MAXOBJ - 1)];

		printk(KERN_ERR"get %02x: ", cur[0]);
		WARN_ON(arc_lookup(s, cur));

		stats(s);
	}

	for (i = 0; i < MAXOBJ; ++i) {
		unsigned char *cur = sha1[prandom_u32() & (MAXOBJ / 4 - 1)];

		printk(KERN_ERR"get %02x: ", cur[0]);
		WARN_ON(arc_lookup(s, cur));
		stats(s);
	}

	for (i = 0; i < 4 * MAXOBJ; ++i) {
		unsigned char *cur = sha1[prandom_u32() & (MAXOBJ - 1)];

		printk(KERN_ERR"get %02x: ", cur[0]);
		WARN_ON(arc_lookup(s, cur));
		stats(s);
	}

	return 0;
}

module_init(arc_init);
module_exit(arc_exit);


MODULE_AUTHOR("Gu Zheng <cengku@gmail.com>");
MODULE_DESCRIPTION("adaptive-replacement-cache demo");
MODULE_LICENSE("GPL");
