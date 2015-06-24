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


void aura_add_pollfds(struct aura_node *node, int fd, short events) 
{
	struct aura_pollfds *ap;

	if (!node->fds) { 
		/* Start with 8 fds. Unlikely more will be needed */
		node->fds    = calloc(8, sizeof(*node->fds));
		node->numfds = 8;
		node->nextfd = 0;
	}

	if (node->nextfd >= node->numfds) {
		int count = node->numfds * 2;
		node->fds    = realloc(node->fds, count * sizeof(*node->fds));
		node->numfds = count; 
	}

	if (!node->fds) { 
		slog(0, SLOG_FATAL, "Memory allocation problem");
		aura_panic(node);
	}

	ap = &node->fds[node->nextfd++];
	ap->fd = fd; 
	ap->events = events;
	ap->node = node;

	aura_eventsys_fd_action(ap, AURA_FD_ADDED);

	if (node->fd_changed_cb)
		node->fd_changed_cb(ap, AURA_FD_ADDED, node->fd_changed_arg);
}

void aura_del_pollfds(struct aura_node *node, int fd) 
{
	int i;
	for (i=0; i < node->nextfd; i++) {
		struct aura_pollfds *fds = &node->fds[i];
		if (fds->fd == fd) 
			break;
	}
	if (i == node->nextfd) {
		slog(0, SLOG_FATAL, "Attempt to delete invalid descriptor from node");
		aura_panic(node); 
	}

	/* Notify the event system */
	aura_eventsys_fd_action(&node->fds[i], AURA_FD_REMOVED);
	/* Fire the callback */ 
	if (node->fd_changed_cb)
		node->fd_changed_cb(&node->fds[i], AURA_FD_REMOVED, 
				    node->fd_changed_arg);
	
	memmove(&node->fds[i], &node->fds[i+1], 
		sizeof(struct aura_pollfds) * (node->nextfd - i - 1));
	node->nextfd--;
	bzero(&node->fds[node->nextfd], sizeof(struct aura_pollfds));
}

