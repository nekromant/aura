#include <aura/aura.h>

static LIST_HEAD(transports);

#define required(_rq)							\
	if (!tr->_rq) {							\
	slog(0, SLOG_WARN,						\
	     "Transport %s missing required field aura_transport.%s; Disabled",	\
	     tr->name, #_rq						\
		);							\
	return;								\
	}


void aura_transport_register(struct aura_transport *tr)
{
	/* Check if we have all that is required */
	required(name);
	required(open);
	required(close);
	required(loop);

	if ((tr->buffer_get) || (tr->buffer_put))
	{
		required(buffer_get);
		required(buffer_put)
	}

	/* Warn against erroneous config */
	if (tr->buffer_overhead < tr->buffer_offset) {
		slog(0, SLOG_WARN,
		     "Transport has buffer_overhead (%d) < buffer_offset (%d). It will crash. Disabled",
		     tr->buffer_overhead, tr->buffer_offset);
		return;
	}

	/* Add it! */
	list_add_tail(&tr->registry, &transports);
}

/** 
 * Find a transport identified by it's name
 * 
 * @param name 
 * 
 * @return struct aura_transport or NULL
 */
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

/**
 * Get a set of descriptors to poll events for this node.
 * The memory for this struct is managed by the library and it should NOT
 * be freed (Unless it's a crash you want)
 *
 * @param node
 * @param fds
 * @return
 */
int aura_get_pollfds(struct aura_node *node, const struct aura_pollfds **fds)
{
	*fds = node->fds;
	return node->nextfd;
}

/** 
 * Add a file descriptor to be polled by the event system. 
 * Events are a bitmask from poll.h 
 * 
 * @param node 
 * @param fd 
 * @param events 
 */
void aura_add_pollfds(struct aura_node *node, int fd, uint32_t events)
{
	struct aura_pollfds *ap;

	if (!node->fds) {
		/* Start with 8 fds. Unlikely more will be needed */
		node->fds    = calloc(8, sizeof(*node->fds));
		node->numfds = 8;
		node->nextfd = 0;
		slog(4, SLOG_DEBUG, "node: %d descriptor slots available", node->numfds);
	}

	if (node->nextfd >= node->numfds) {
		int count = node->numfds * 2;
		node->fds    = realloc(node->fds, count * sizeof(*node->fds));
		node->numfds = count;
		slog(4, SLOG_DEBUG, "node: Resized. %d descriptor slots available", node->numfds);
	}

	if (!node->fds) {
		slog(0, SLOG_FATAL, "Memory allocation problem");
		aura_panic(node);
	}

	ap = &node->fds[node->nextfd++];
	ap->fd = fd;
	ap->events = events;
	ap->node = node;

	if (node->fd_changed_cb)
		node->fd_changed_cb(ap, AURA_FD_ADDED, node->fd_changed_arg);
}

/** 
 * Remove a descriptor from the list of the descriptors to be polled.
 * 
 * @param node 
 * @param fd 
 */
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

	/* Fire the callback */
	if (node->fd_changed_cb)
		node->fd_changed_cb(&node->fds[i], AURA_FD_REMOVED,
				    node->fd_changed_arg);

	memmove(&node->fds[i], &node->fds[i+1],
		sizeof(struct aura_pollfds) * (node->nextfd - i - 1));
	node->nextfd--;
	bzero(&node->fds[node->nextfd], sizeof(struct aura_pollfds));
}

