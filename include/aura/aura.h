#ifndef AURA_H
#define AURA_H

#include <search.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <aura/slog.h>
#include <search.h>
#include <errno.h>
#include <stdbool.h>

#include "list.h"
#include "format.h"
#include "endian.h"


/**  Node status */
enum aura_node_status {
	AURA_STATUS_OFFLINE,    //!< AURA_STATUS_OFFLINE
	AURA_STATUS_ONLINE,     //!< AURA_STATUS_ONLINE
};

#define AURA_EVTLOOP_ONCE     (1 << 1)
#define AURA_EVTLOOP_NONBLOCK (1 << 2)
#define AURA_EVTLOOP_NO_GC    (1 << 3)

/** Remote method call status */
enum aura_call_status {
	AURA_CALL_COMPLETED,            //!< AURA_CALL_COMPLETED
	AURA_CALL_TIMEOUT,              //!< AURA_CALL_TIMEOUT
	AURA_CALL_TRANSPORT_FAIL,       //!< AURA_CALL_TRANSPORT_FAIL
};

/** File descriptor action */
enum aura_fd_action {
	AURA_FD_ADDED,  //!< Descriptor added
	AURA_FD_REMOVED //!< Descriptor removed
};

struct aura_pollfds {
	int			magic;
	struct aura_node *	node;
	int			fd;
	uint32_t		events;
	void *			eventsysdata; /* Private eventsystem data */
	struct list_head qentry;
};

struct aura_object;
struct aura_node {
	const struct aura_transport *	tr;
	struct aura_export_table *	tbl;
	void *				transport_data;
	void *				user_data;

	void *				allocator_data;

	enum aura_node_status		status;
	struct list_head		outbound_buffers;
	struct list_head		inbound_buffers;

	/* A simple, memory efficient buffer pool */
	struct list_head		buffer_pool;
	int				num_buffers_in_pool;
	int				gc_threshold;
	/* Synchronos calls put their stuff here */
	bool				sync_call_running;
	bool				need_endian_swap;
	bool				is_opening;
	bool				start_event_sent;
	bool				waiting_for_status;
	struct aura_buffer *		sync_ret_buf;
	int				sync_call_result;

	/* Synchronous event storage */
	struct list_head		event_buffers;
	int				sync_event_max;
	int				sync_event_count;

	/* General callbacks */
	void *				status_changed_arg;
	void				(*status_changed_cb)(struct aura_node *node, int newstatus, void *arg);

	void *				etable_changed_arg;
	void				(*etable_changed_cb)(struct aura_node *node, struct aura_export_table *old, struct aura_export_table *newtbl, void *arg);

	void *				object_migration_failed_arg;
	void				(*object_migration_failed_cb)(struct aura_node *node, struct aura_object *failed, void *arg);

	void				(*unhandled_evt_cb)(struct aura_node *node, struct aura_buffer *ret, void *arg);
	void *				unhandled_evt_arg;
	void *				fd_changed_arg;
	void				(*fd_changed_cb)(const struct aura_pollfds *fd, enum aura_fd_action act, void *arg);


	/* Event system and polling */
	struct list_head		fd_list;     /* list of descriptors owned by this node */
	int					fd_count;         /* Total count of descriptors this node owns */

	struct aura_eventloop *		loop;           /* eventloop structure */
	int				evtloop_is_autocreated;
	void *				eventloop_data; /* eventloop module private data */
	struct list_head		eventloop_node_list;
	struct list_head		timer_list;     /* List of timers associated with the node */
	const struct aura_object *	current_object;
};


struct aura_object {
	int	id;
	char *	name;
	char *	arg_fmt;
	char *	ret_fmt;

	int	valid;
	char *	arg_pprinted;
	char *	ret_pprinted;
	int	num_args;
	int	num_rets;

	int	arglen;
	int	retlen;

	/* Store callbacks here.
	 * TODO: Can there be several calls of the same method pending? */
	int	pending;
	void	(*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg);
	void *	arg;
};

struct aura_eventloop;

enum node_event {
	NODE_EVENT_STARTED,
	NODE_EVENT_HAVE_OUTBOUND,
	NODE_EVENT_DESCRIPTOR
};

#define object_is_event(o)  (o->arg_fmt == NULL)
#define object_is_method(o) (o->arg_fmt != NULL)


#define __BIT(n) (1 << n)

/**
 * \addtogroup trapi
 * @{
 */

/** Represents an aura transport module */
struct aura_transport {
	/** \brief Required
	 *
	 * String name identifying the transport e.g. "usb"
	 */
	const char *	name;
	/** \brief Optional
	 *
	 * Flags. TODO: Is it still needed?
	 */
	uint32_t	flags;

	/**
	 * \brief Optional.
	 *
	 * Additional bytes to allocate for each buffer.
	 *
	 *  If your transport layer requires any additional bytes to encapsulate the
	 *  actual serialized message - just specify how many. This is node to avoid
	 *  unneeded copying while formatting the message for transmission in the transport
	 *  layer.
	 */
	int buffer_overhead;

	/** \brief Optional.
	 *
	 * Offset in the buffer at which core puts serialized data.
	 *
	 * NOTE: Your buffer_overhead should more or equal buffer_offset bytes. Otherwise
	 * core will complain and refuse to register your transport.
	 *
	 */
	int buffer_offset;

	/** \brief Required.
	 *
	 *  Open function.
	 *
	 *  Open function should perform sanity checking on supplied (in ap) arguments
	 *  and allocate internal data structures and set required periodic timeout in
	 *  the respective node. See transport-dummy.c for a boilerplate.
	 *
	 *  Avoid doing any blocking stuff in open(), do in in loop instead in non-blocking fashion.
	 *  @param node current node
	 *  @param opts string containing transport-specific options
	 *  @return 0 if everything went fine, anything other will be considered an error.
	 */
	int (*open)(struct aura_node *node, const char *opts);
	/**
	 * \brief Required.
	 *
	 * Close function.
	 *
	 * The reverse of open. Free any allocated memory, etc.
	 * This function may block if required. But avoid it if you can.
	 *
	 * @param node current node
	 */
	void (*close)(struct aura_node *node);
	/**
	 * \brief Required
	 *
	 * The workhorse of your transport plugin.
	 *
	 * This function should check the node->outbound_buffers queue for any new messages to
	 * deliver and place any incoming messages into node->inbound_buffers queue.
	 *
	 * This function is called by the core when:
	 * - Descriptors associated with this node report being ready for I/O (fd will be set to
	 *   the descriptor that reports being ready for I/O)
	 * - Node's periodic timeout expires (fd will be NULL)
	 * - A new message has been put into outbound queue that has previously been empty
	 *   (e.g. Wake up! We've got work to do) (fd will NULL)
	 *
	 * @param node current node
	 * @param fd pointer to struct aura_pollfds that generated an event.
	 */
	void (*handle_event)(struct aura_node *node, enum node_event, const struct aura_pollfds *fd);

	/**
	 * \brief Optional.
	 *
	 * Your transport may implement passing aura_buffers as arguments.
	 * This may be extremely useful for DSP applications, where you also
	 * implement your own buffer_request and buffer_release to take care of
	 * allocating memory on the DSP side. This function should serialize buffer buf
	 * into buffer dst. Buffer pointer/handle must be cast to uint64_t.
	 *
	 * @param buf
	 */
	void (*buffer_put)(struct aura_buffer *dst, struct aura_buffer *buf);
	/**
	 * \brief Optional
	 *
	 * Your transport may implement passing aura_buffers as arguments.
	 * This may be extremely useful for DSP applications, where you also
	 * implement your own buffer_request and buffer_release to take care of
	 * allocating memory on the DSP side.
	 * This function should deserialize and return aura_buffer from buffer buf
	 * @param buf
	 * @param ptr
	 */
	struct aura_buffer *		(*buffer_get)(struct aura_buffer *buf);
	/**
	 * \brief Optional
	 *
	 * If your transport needs a custom memory allocator, specify it here.
	 */
	struct aura_buffer_allocator *	allocator;

	/** \brief Private.
	 *
	 * Transport usage count */
	int				usage;

	/** \brief Private.
	 *
	 * List entry for global transport list */
	struct list_head		registry;
};

/**
 * @}
 */


/**
 * \addtogroup bufapi
 * @{
 */

/** Represents a buffer (surprize!) that is used for serialization/deserialization
 *
 *  * The data is stored in NODE endianness.
 *  * The buffer size includes a transport-specific overhead if any
 *  * The buffer has an internal data pointer
 *  * The buffer has an associated object
 *  * The buffer can be allocated using a transport-specific memory allocator
 */
struct aura_buffer {
	/** Magic value, used for additional sanity-checking */
	uint32_t		magic;
	/** Total allocated space available in this buffer */
	int			size;
	/** Pointer to the next element to read/write */
	int			pos;
	/** The useful payload size. Functions that put data in the buffer increment this */
	int payload_size;
	/** object assosiated with this buffer */
	struct aura_object *	object;
	/** The node that owns the buffer */
	struct aura_node *	owner;
	/** list_entry. Used to link buffers in queue keep in buffer pool */
	struct list_head	qentry;
	/** The actual data in this buffer */
	char *			data;
};

/**
 * @}
 */


struct aura_buffer *aura_serialize(struct aura_node *node, const char *fmt, int size, va_list ap);
int  aura_fmt_len(struct aura_node *node, const char *fmt);
char *aura_fmt_pretty_print(const char *fmt, int *valid, int *num_args);


const struct aura_transport *aura_transport_lookup(const char *name);
void aura_set_status(struct aura_node *node, int status);


#define AURA_TRANSPORT(s)                                          \
	void __attribute__((constructor(101))) do_reg_ ## s(void) { \
		aura_transport_register(&s);                       \
	}

enum aura_endianness aura_get_host_endianness();
void aura_hexdump(char *desc, void *addr, int len);
const char *aura_get_version();
unsigned int aura_get_version_code();

void aura_set_node_endian(struct aura_node *node, enum aura_endianness en);


#define AURA_DECLARE_CACHED_ID(idvar, node, name) \
	static int idvar = -1; \
	if (idvar == -1) { \
		struct aura_object *tmp = aura_etable_find(node->tbl, name); \
		if (!tmp) \
			BUG(node, "Static lobject lookup failed!"); \
		idvar = tmp->id; \
	}

void aura_etable_destroy(struct aura_export_table *tbl);

#include <aura/node.h>
#include <aura/buffer.h>
#include <aura/eventloop-funcs.h>
#include <aura/etable.h>

void __attribute__((noreturn)) aura_panic(struct aura_node *node);
int __attribute__((noreturn))  BUG(struct aura_node *node, const char *msg, ...);

void aura_print_stacktrace();

#define min_t(type, x, y) ({                                    \
		type __min1 = (x);                      \
		type __min2 = (y);                      \
		__min1 < __min2 ? __min1 : __min2; })


#define max_t(type, x, y) ({                                    \
		type __max1 = (x);                      \
		type __max2 = (y);                      \
		__max1 > __max2 ? __max1 : __max2; })


#endif
