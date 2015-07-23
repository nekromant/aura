/* This include file is for doxygen groups only */

/** @defgroup node Opening and closing nodes
 *
 * You start working with aura by opening a node using aura_open() or aura_vopen().
 * struct aura_node represents a remote device connected via some transport.
 * The transport layer is irrelevant here. Opening a node does NOT mean that the node is
 * immediately ready to accept calls - opening only starts the process of connecting
 * to a node which may take a little time depending on the transport. Even more, a node
 * can go offline and online multiple times and normally this doesn't cause an application
 * to terminate (although, you can always do so).
 *
 * When you are done working with the node, you just call aura_close() and all the memory is
 * freed. Simple, huh? Process to the next section to learn how to make calls on the opened node.
 */


 /** @defgroup workflow Working with nodes
 *
 * A node 'exports' a table of 'events' and 'methods' that it provides. This is all done
 * in the main event loop. Once aura receives and compiles the table of available objects
 * the node changes the state to indicate that it is ready to accept calls and deliver
 * events. If the node goes offline for some reason aura will read the export table again
 * once it goes online.
 *
 * A 'method' represents a simple remote function. It accepts several arguments and
 * returns several results e.g. it looks just like a function call in lua:
 *
 * a, b, c = function(arg1, arg2)
 *
 * The method can be called either synchronously using (See aura_call() and aura_call_raw()
 * or asynchronously using aura_start_call() or aura_start_call_raw().
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
 * However in real life synchronous API is rarely enough for anything serious, so we need
 * to be asynchronous instead.
 *
 *
 * Events represent something that happens on the remote side and can be only handled in
 * asynchronous fashion. Events can also deliver a set of arguments for the user to handle.
 *
 * TODO: EVENT API DESCRIPTION HERE
 *
 * Objects (methods and events alike) are stored internally enumerated from 0 to n. You can either
 * do calls and set callbacks using object names (they are searched using a hash table, that's
 * pretty fast) or via their ids. Beware, though: If the node represents a hardware device, it
 * goes offline for a firmware upgrade and when it comes back to life it has a different export
 * table and the id for the same method may change. You get the idea, so don't shoot yourself in
 * the knee!
 *
 * TODO: Right now if the node goes offline all the callbacks to events are reset and all pending
 * method calls are cancelled with AURA_CALL_TRANSPORT_FAIL status. Further versions are planned
 * to try and preserve existing callbacks and warn if any object signature changes.
 *
 * Before you can do synchronous calls on the node you have to create an eventloop using
 * aura_eventloop_create(). Right now it is not created automatically for added flexibility.
 * After it is done you can do synchronous calls or enter event handling loop using aura_handle_events()
 * and aura_handle_events_timeout()
 *
 * TODO: We really should create an eventloop internally for only one node in lazy fashion, if
 * the user only wants synchronous calls. More newbie-friendly.
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

