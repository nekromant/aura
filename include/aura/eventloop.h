#ifndef AURA_EVTLOOP_H
#define AURA_EVTLOOP_H

#include <aura/list.h>


struct aura_pollfds;
struct aura_node;
struct aura_eventloop {
        int keep_running;
        int poll_timeout;
        struct list_head nodelist;
        void *eventsysdata;
        const struct aura_eventloop_module *module;
};

struct aura_eventloop_module {
        const char *name;
        int usage;
        struct list_head linkage;
        int  (*create)(struct aura_eventloop *loop);
        void (*destroy)(struct aura_eventloop *loop);
        void (*fd_action)(struct aura_eventloop *loop,
                          const struct aura_pollfds *ap,
                          int action);
        void (*dispatch)(struct aura_eventloop *loop, int flags);
        void (*loopbreak)(struct aura_eventloop *loop, struct timeval *tv);
        void (*periodic)(struct aura_eventloop *loop, struct aura_node *node, struct timeval *tv);
        void (*node_added)(struct aura_eventloop *loop, struct aura_node *node);
        void (*node_removed)(struct aura_eventloop *loop, struct aura_node *node);
};

#define AURA_EVENTLOOP_MODULE(s)                                            \
        static void __attribute__((constructor (101))) do_mreg_ ## s(void) { \
                aura_eventloop_module_register(&s);                       \
        }

void aura_eventloop_module_register(struct aura_eventloop_module *m);
const struct aura_eventloop_module *aura_eventloop_module_get();
int aura_eventloop_module_select(const char *name);

static inline void *aura_eventloop_moduledata_get(struct aura_eventloop *loop)
{
        return loop->eventsysdata;
}

static inline void aura_eventloop_moduledata_set(struct aura_eventloop *loop,
                                                 void *data)
{
        loop->eventsysdata = data;
}



#endif
