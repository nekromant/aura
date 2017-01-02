#include <aura/aura.h>

static LIST_HEAD(transports);

#define required(_rq)                                                   \
	if (!tr->_rq) {                                                 \
		slog(0, SLOG_WARN,                                              \
		     "Transport %s missing required field aura_transport.%s; Disabled", \
		     tr->name, #_rq                                             \
		     );                                                      \
		return;                                                         \
	}


void aura_transport_register(struct aura_transport *tr)
{
	/* Check if we have all that is required */
	required(name);
	required(open);
	required(close);
	required(handle_event);

	if ((tr->buffer_get) || (tr->buffer_put)) {
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
	if (strcmp(pos->name, name) == 0) {
		pos->usage++;
		return pos;
	}
	return NULL;
}

void aura_transport_release(const struct aura_transport *tr)
{
	((struct aura_transport *)tr)->usage--;
}

void aura_transport_dump_usage()
{
	struct aura_transport *pos;

	slog(0, SLOG_INFO, "--- Registered transports ---");
	list_for_each_entry(pos, &transports, registry)
	slog(0, SLOG_INFO, "%s (%d instances in use)", pos->name, pos->usage);
}
