#ifndef AURA_ETABLE_H
#define AURA_ETABLE_H


struct aura_export_table {
	int			size;
	int			next;
	struct aura_node *	owner;
	struct hsearch_data	index;
	struct aura_object	objects[];
};

struct aura_export_table *aura_etable_create(struct aura_node *owner, int n);
void aura_etable_add(struct aura_export_table *tbl, const char *name, const char *argfmt, const char *retfmt);
void aura_etable_activate(struct aura_export_table *tbl);

struct aura_object *aura_etable_find(struct aura_export_table *tbl, const char *name);
struct aura_object *aura_etable_find_id(struct aura_export_table *tbl, int id);


#endif /* end of include guard: AURA_ETABLE_H */
