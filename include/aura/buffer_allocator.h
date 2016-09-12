#ifndef AURA_BUF_ALLOC_H
#define AURA_BUF_ALLOC_H

struct aura_buffer_allocator {
    const char *name;
    void *(*create)(struct aura_node *node);
    struct aura_buffer *(*request)(struct aura_node *node, void *data, int size);
    void (*release)(struct aura_node *node, void *data, struct aura_buffer *buf);
    void (*destroy)(struct aura_node *node, void *data);
};

static inline void *aura_node_allocatordata_get(struct aura_node *node)
{
    return node->allocator_data;
}

static inline void aura_node_allocatordata_set(struct aura_node *node, void *data)
{
    node->allocator_data = data;
}



#endif
