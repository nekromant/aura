#include <aura/aura.h>

static LIST_HEAD(transports);

void aura_transport_register(struct aura_transport *tr)
{
	list_add_tail(&tr->registry, &transports);
	/* TODO: Transport sanity checking */
}

/* No fancy hash table, we won't have that many */
const struct aura_transport *aura_transport_lookup(const char *name)
{
	struct aura_transport *pos; 
	list_for_each_entry(pos, &transports, registry)
		if (strcmp(pos->name, name) == 0 ) { 
			pos->usage++;
			return pos;
		}
	return NULL;
}

void aura_transport_release(const struct aura_transport *tr)
{
	((struct aura_transport* ) tr)->usage--;
}

void aura_transport_dump_usage() 
{
	struct aura_transport *pos; 
	slog(0, SLOG_INFO, "--- Registered transports ---");
	list_for_each_entry(pos, &transports, registry)
		slog(0, SLOG_INFO, "%s (%d instances in use)", pos->name, pos->usage);
}
