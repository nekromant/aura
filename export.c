#include <aura/aura.h>
#include <search.h>

struct aura_export_table *aura_etable_create(struct aura_node *owner, int n)
{
	/* man hsearch: recommends at least 25% more to avoid collisions */
	int nel = n + (n / 4);
	slog(4, SLOG_DEBUG, "etable: Creating etable for %d elements, %d hash entries", n, nel);
	struct aura_export_table *tbl = calloc(1,
					       sizeof(struct aura_export_table) +
					       n * sizeof(struct aura_object));
	if (!tbl)
		return NULL;

	hcreate_r(nel, &tbl->index);
	tbl->owner = owner;
	tbl->next  = 0;
	tbl->size  = n;

	return tbl;
}


void aura_etable_add(struct aura_export_table *tbl,
		    const char *name,
		    const char *argfmt,
		    const char *retfmt)
{
	int ret;
	ENTRY e, *ep;
	int arg_valid = 1;
	int ret_valid = 1;
	struct aura_object *target;

	if (tbl->next >= tbl->size)
		BUG(tbl->owner, "Internal BUG: Insufficient export table storage");

	target = aura_etable_find(tbl, name);
	if (target != NULL)
		BUG(tbl->owner, "Internal BUG: Duplicate export table entry: %s", name);

	target = &tbl->objects[tbl->next];
	target->id      = tbl->next++;
	if (!name) {
		BUG(tbl->owner, "Internal BUG: object name can't be nil");
	} else {
		target->name    = strdup(name);
	}

	if (argfmt)
		target->arg_fmt = strdup(argfmt);
	if (retfmt)
		target->ret_fmt = strdup(retfmt);

	if ((argfmt && !target->arg_fmt) ||
	    (retfmt && !target->ret_fmt))
		BUG(tbl->owner, "Internal allocation error");

	if (argfmt)
		target->arg_pprinted = aura_fmt_pretty_print(argfmt, &arg_valid, &target->num_args);

	if (retfmt)
		target->ret_pprinted = aura_fmt_pretty_print(retfmt, &ret_valid, &target->num_rets);

	target->valid = arg_valid && ret_valid;
	if (!target->valid) {
		slog(0, SLOG_WARN, "Object %d (%s) has corrupt export table",
		     target->id, target->name);
	}
	/* Calculate the sizes required */
	target->arglen = aura_fmt_len(tbl->owner, argfmt);
	target->retlen = aura_fmt_len(tbl->owner, retfmt);

	/* Add this shit to index */
	e.key  = target->name;
	e.data = (char *) target;
	ret = hsearch_r(e, ENTER, &ep, &tbl->index);
	if (!ret)
		BUG(tbl->owner, "Internal BUG: Error adding entry to hash table");

}

struct aura_object *aura_etable_find(struct aura_export_table *tbl,
				     const char *name)
{
	int ret;
	ENTRY e, *ep;
	struct aura_object *target = NULL;

	if (!tbl)
		return NULL;

	e.key  = (char *) name;
	e.data = NULL;
	ret = hsearch_r(e, FIND, &ep, &tbl->index);
	if (ret)
		target = (struct aura_object *) ep->data;
	return target;
}

struct aura_object *aura_etable_find_id(struct aura_export_table *tbl,
					int id)
{
	if (id >= tbl->next)
		return NULL;
	return &tbl->objects[id];
}

#define format_matches(one, two) \
		((!one && !two) || \
		(one && two && strcmp(one, two)))

static int object_is_equal(struct aura_object *one, struct aura_object *two)
{
	/* Objects are considered equal if: */

	/* Names match */
	if (strcmp(one->name, two->name) != 0)
		return 0; /* Nope */

	/* Argument formats match */
	if (!format_matches(one->arg_fmt, two->arg_fmt))
		return 0;

	/* Ret formats match */
	if (!format_matches(one->ret_fmt, two->ret_fmt))
		return 0;

	return 1;
}

static int migrate_object(struct aura_object *src, struct aura_object *dst)
{
	if (!src || !dst)
		return 0;

	if (object_is_equal(src, dst)) {
		dst->calldonecb = src->calldonecb;
		dst->arg = src->arg;
		slog(4, SLOG_DEBUG, "etable: Successful migration of obj %d->%d (%s)",src->id, dst->id, dst->name);
		return 1;
	}
	return 0;
}
static void etable_migrate(struct aura_export_table *old, struct aura_export_table *new)
{
	int i;
	struct aura_node *node;
	/* Sanity checking */
	if (!old || !new) /* Nothing to migrate */
		return;
	if (old->owner != new->owner)
		BUG(old->owner, "BUG during export table migration: etable owners are not equal!");

	node = old->owner;

	/* Migration is a complex process. We need to scan tables to see if there are any
	 * differences (e.g. If the node was down for a firmware update) and move all the callbacks and
	 * their arguments to the new table before nuking the old one.
	 *
	 * If return format or arguments formats have changed, we
	 * will NOT migrate anything. This is better than keeping in a potentially broken state.
	 *
	 * The algo is somewhat smart:
	 * We first try one-to-one mapping with the same id, since it's more likely to be the case 99% of the time.
	 * If that fails - we do a hash-search of the name in the new table, and try to migrate our callbacks there
	 *
	 */
	for(i=0; i < old->next; i++) {
		struct aura_object *src = &old->objects[i];
		struct aura_object *dst = &new->objects[i];

		/* One-to-one mapping */
		if (migrate_object(src, dst))
			continue;

		/* Try hash-search */
		dst = aura_etable_find(new, src->name);
		if (migrate_object(src, dst))
			continue;

		if (src->calldonecb) {
			/* Migration failed, object had a callback set.
			   We need to notify the application about a potential problem
			 */
			if (node->object_migration_failed_cb)
				node->object_migration_failed_cb(node, src,
								 node->object_migration_failed_arg);
			else {
				slog(1, SLOG_WARN,
				     "Migration of callbacks for object %s failed\n", src->name);
				slog(1, SLOG_WARN,
				     "Set aura_object_migration_failed_cb() callback to disable this warning\n");
			}
		}
	}
}

void aura_etable_activate(struct aura_export_table *tbl)
{
	struct aura_node *node = tbl->owner;

	if (aura_get_status(node) == AURA_STATUS_ONLINE) {
		slog(0, SLOG_FATAL, "Internal BUG: Attemped to change export table when transport is online");
		aura_panic(node);
	}

	if (node->is_opening)
		BUG(node, "Transport BUG: Do not call aura_etable_activate in open()");

	if (node->etable_changed_cb)
		node->etable_changed_cb(node, node->tbl, tbl, node->etable_changed_arg);

	if (node->tbl) {
		etable_migrate(node->tbl, tbl);
		aura_etable_destroy(node->tbl);
	}
	node->tbl = tbl;


}

void aura_etable_destroy(struct aura_export_table *tbl)
{
	/* Iterate over the table and free all the strings */
	int i;

	for (i=0; i < tbl->next; i++) {
		struct aura_object *tmp;
		tmp=&tbl->objects[i];
		free(tmp->name);
		if (tmp->arg_fmt)
			free(tmp->arg_fmt);
		if (tmp->ret_fmt)
			free(tmp->ret_fmt);
		if (tmp->arg_pprinted)
			free(tmp->arg_pprinted);
		if (tmp->ret_pprinted)
			free(tmp->ret_pprinted);
	}
	/* Get rid of the table itself */
	hdestroy_r(&tbl->index);
	free(tbl);
}
