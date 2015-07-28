/* This include file is for doxygen groups only */

/** @defgroup node The basics
 *
 * This documentation describes only the C API and aura's internal stuff. If you
 * are looking for documentation on lua bindings - it's not here. See luadoc.
 *
 * TODO: Describe that we need to include <aura/aura.h>, pkg-config and the rest
 * of the stuff
 *
 * You start working with aura by opening a node using aura_open() or aura_vopen().
 * struct aura_node represents a remote device connected via some transport.
 * The transport layer is irrelevant here. Opening a node does NOT mean that the node is
 * immediately ready to accept calls - opening only starts the process of connecting
 * to a node which may take a little time depending on the transport. Even more, a node
 * can go offline and online multiple times and normally this doesn't cause an application
 * to terminate (although, you can always do so).
 *
 * You can associate your own data with a node using aura_set_userdata() and retrieve the pointer
 * with aura_get_userdata()
 *
 * When you are done working with the node, you just call aura_close() and all the memory is
 * freed. Simple, huh? Get over to to the next section to learn how to do something useful with a node
 * apart from opening and closing.
 *
 */

 /** @defgroup sync The synchronous API
 *
 * One of the core things of aura is the export table, or etable for short.
 * A node 'exports' a table of 'events' and 'methods' that it provides. This is all done
 * by the transport in the main event loop. Once aura receives and compiles the table of
 * available objects the node changes the state to indicate that it is ready to accept
 * calls and deliver incoming events. If the node goes offline for some reason aura
 * will read the export table again once it goes online.
 *
 * A 'method' represents a simple remote function. It accepts several arguments and
 * returns several results e.g. it looks just like a function call in lua:
 *
 * a, b, c = function(arg1, arg2)
 *
 * The method can be called either synchronously using (See aura_call() and aura_call_raw()
 * or asynchronously using aura_start_call() or aura_start_call_raw(). This section covers
 * only synchronous API.
 *
 * Let's start with the simplest synchronous example for calling remote functions.
 * \code{.c}
 *
 *  int ret;
 *  struct aura_buffer *retbuf;
 *  // Different transports have different options!
 *  struct aura_node *node = aura_open("dummy", 1, 2, 3);
 *  if (!node) {
 *     fprintf("err: failed to open node\n");
 *     return -1;
 *  }
 *
 *  aura_wait_status(node, AURA_STATUS_ONLINE);
 *
 *  ret = aura_call(node, "echo16", &retbuf, 128);
 *  if (0 == ret) {
 *     ret = aura_buffer_get_u16(retbuf);
 *     printf("Call succeeded - we got %d from device\n", ret);
 *     aura_buffer_release(retbuf);
 *  }
 *
 * aura_close(node);
 * \endcode
 *
 * That's it? Thought it would be harder?
 *
 * Events, on the contrary represent something that happened on the remote side. E.g. a timer
 * expired, or a user pressed a button. Events can deliver arbitrary payload. Just like returning
 * function arguments. Events are queued by the receiving side and should be read by via
 * aura_read_event() and aura_read_event_timeout(). These function block until an actual event arrives.
 * You can use aura_get_pending_events() to find out how many events are pending.
 *
 *
 * Objects (methods and events alike) are stored internally in a struct aura_object, enumerated from
 * 0 to n. You can either do calls and set callbacks using object names (they are searched using a
 * hash table, that's pretty fast) or via their ids. Beware, though: If the node represents a hardware
 * device, it goes offline for a firmware upgrade and when it comes back to life it has a different export
 * table and the id for the same method may change. Calling by name allows you to avoid conflicts.
 * You get the idea, so don't shoot yourself in the knee!
 *
 * For something advanced usage - have a look at the async API that is way more powerful.
 */


 /** @defgroup async The asynchronous API
 *
 * If you are reading this, you must be familiar with the concept of events and methods.
 * If not have a look at the previous section that describes synchronous API. It has a nice
 * overview of how things work.
 *
 * In this section we'll be dealing with asynchronous API. In real life your target device may
 * work at speeds of several Mhz and take ages (compared to the host PC) to execute a method call.
 * That's not cool, because we can do many other things while the remote side gets the job done.
 *
 * With asynchronous API you can start a method call, and get the results in the supplied callback.
 * Events also get delivered into their respective callbacks (if any).
 *
 * If the node goes offline and later becomes live once more with a different export table, aura will
 * try to preserve all of the event callbacks, even if their id changes. The callback will only get
 * unregistered if event signature changes (e.g. the number or type of the returned values changes)
 *
 * All of the callbacks are delivered from the eventloop. aura comes with it's own easy to use event loop
 * implementation that is covered in the next section. The general workflow is:
 *
 * - Open one or several nodes using aura_open() or aura_vopen()
 * - Register all of your callbacks
 * - Create an eventloop using aura_eventloop_create()
 * - Run the event processing loop with aura_handle_events() for as long as you want
 *
 *
 * If you wish to integrate aura with your own event loop, see aura_get_pollfds() to get a list of
 * descriptors to poll and aura_fd_changed_cb() to get notified whenever transport expects a descriptor
 * to be added/removed.
 *
 */

/** @defgroup loop Event loop functions
 *  This group functions related to aura event loop.
 *  By default aura comes with a simple and easy to use event loop that can aggregate and dispatch events for
 *  several nodes.
 *  To use it, you should first create an event loop with aura_eventloop_create() from one or more open nodes. Once done,
 *  You can call aura_handle_events() and aura_handle_events_timeouts()
 *
 *  Rule of thumb for multi-threading: No more than one thread per node.
 *
 */

/** @defgroup retparse Parsing return values
 *  Events and methods deliver data in a struct aura_buffer
 *  that should be used as opaque type. The functions documented in this section
 *  should be used to parse the buffer obtained from a device. These functions take care to
 *  swap endianness if required.
 *
 *  Aura buffers have internal pointer that is advanced by the number of bytes
 *  required when each argument is read out. Attempting to cross the buffer boundary will result
 *  in aura_panic() on the relevant node.
 */

/** @defgroup trapi Transport plugins API
 *  This chapter describes the transport API. You will have to use this API
 *  to create your own transports.
 *
 *  To create a transport module you have to do the following:
 *  - Create a .c file for you transport and pick a name. transport-dummy.c is a good boilerplate
 *  - Add your .c file into the Makefile to actually be able to build it
 *  - Define a struct aura_transport in you file and fill in all the required fields
 *  - Call AURA_TRANSPORT(name) macro to register it in the global registry
 *  - Bindings may require some additional love to see your transport.
 *  See bindings-lua.c for a hint of what's going on.
 *
 *  The simplest example is below:
 *
 *  \code{.c}
 *  #include <aura/aura.h>
 *  #include <aura/private.h>
 *
 *   (your code here)
 *
 *  static struct aura_transport dummy = {
 *	.name = "dummy",
 *	.open = dummy_open,
 *	.close = dummy_close,
 *	.loop  = dummy_loop,
 *	.buffer_overhead = 16,
 *	.buffer_offset = 8
 * };
 * AURA_TRANSPORT(dummy);
 *  \endcode
 */

/** @defgroup bufapi Buffer management
 * All arguments, event data and returned values are stored in buffers, represented by struct aura_buffer
 * The actual data is stored in NODE endianness (e.g. the same byte order that is on the remote side)
 * Transport plugins can override buffer management, if required.
 *
 * If you are just using aura for your application - treat aura_buffer as an opaque type.
 * You won't really need this API in most cases.
 *
 * If you are developing a transport plugin - you may need some of these calls, but
 * most likely you'll need to manipululate buffer->data[] directly.
 *
 * Try to adjust your transport's buffer_overhead and buffer_offset parameters so that
 * you will not need any temporary buffers.
 */

/** @defgroup misc Misc functions and routines
 *  This group contains misc utility functions
 */

