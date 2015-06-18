#include <aura/aura.h>
#include <search.h>

struct aura_export_table *aura_etable_create(struct aura_node *owner, int n)
{
	/* man hsearch: recommends at least 25% more to avoid collisions */
	int nel = n + (n / 4);
	dbg("etable: Creating etable for %d elements, %d hash entries", n, nel);
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

void aura_etable_activate(struct aura_export_table *tbl)
{
	if (aura_get_status(tbl->owner) == AURA_STATUS_ONLINE) { 
		slog(0, SLOG_FATAL, "Internal BUG: Attemped to change export table when transport is online");
		aura_panic(tbl->owner);
	}
	
	if (tbl->owner->tbl)
		aura_etable_destroy(tbl->owner->tbl);
	tbl->owner->tbl = tbl;
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
