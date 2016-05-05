#include <aura/aura.h>
#include <aura/private.h>
#include <signal.h>

#ifndef AURA_DISABLE_BACKTRACE
#include <execinfo.h>
#endif

#define TRACE_LEN 36

int __attribute__((noreturn)) BUG(struct aura_node *node, const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	slogv(0, SLOG_FATAL, msg, ap);
	va_end(ap);
	aura_panic(node);
}


/**  \addtogroup misc
 *  @{
 */

/**
 * Call this function to immediately cause an emergency exit.
 *
 * This function will take care to print a stack trace and dump all
 * the interesting things to log. This function never returns.
 *
 * @param node - Node that caused panic. Can be NULL.
 */
void __attribute__((noreturn)) aura_panic(struct aura_node *node)
{
	void *array[TRACE_LEN];
	size_t size;
	char **strings;
	size_t i;

	aura_transport_dump_usage();

#ifndef AURA_DISABLE_BACKTRACE
	size = backtrace(array, TRACE_LEN);
	strings = backtrace_symbols(array, size);

	slog(0, SLOG_DEBUG, "--- Dumping aura stack (%d entries) ---", size);
	for (i = 0; i < size; i++)
		slog(0, SLOG_DEBUG, "%s", strings[i]);
#else
	slog(0, SLOG_DEBUG, "Stackdump disabled. Perhaps your C library sucks");
#endif

	/* TODO: Dump interesting stuff from node var */

	free(strings);
	exit(128);
}

/**
 * @}
 */
static void handler(int sig, siginfo_t *si, void *unused)
{
	slog(0, SLOG_FATAL, "AURA got a segmentation fault, this is bad");
	aura_panic(NULL);
}


void __attribute__((constructor(101))) reg_seg_handler()
{
	struct sigaction sa;

	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler;
	sigaction(SIGSEGV, &sa, NULL);
}
