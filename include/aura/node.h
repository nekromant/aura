#ifndef AURA_NODE_H
#define AURA_NODE_H

struct aura_node;

const char *aura_node_call_strerror(int errcode);

void aura_bufferpool_preheat(struct aura_node *nd, int size, int count);

struct aura_node *aura_open(const char *name, const char *opts);
void aura_close(struct aura_node *dev);
int aura_queue_call(struct aura_node *node, int id, void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg), void *arg, struct aura_buffer *buf);

int aura_start_call_raw(struct aura_node *dev, int id, void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg), void *arg, ...);

int aura_start_call(struct aura_node *dev, const char *name, void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg), void *arg, ...);

int aura_call_raw(struct aura_node *dev, int id, struct aura_buffer **ret, ...);

int aura_call(struct aura_node *dev, const char *name, struct aura_buffer **ret, ...);

int aura_set_event_callback_raw(struct aura_node *node, int id, void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg), void *arg);

int aura_set_event_callback(struct aura_node *node, const char *event, void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg), void *arg);

void aura_enable_sync_events(struct aura_node *node, int count);
int aura_get_pending_events(struct aura_node *node);
int aura_get_next_event(struct aura_node *node, const struct aura_object **obj, struct aura_buffer **retbuf);

void aura_etable_changed_cb(struct aura_node *node, void (*cb)(struct aura_node *node,
							       struct aura_export_table *old,
							       struct aura_export_table *newtbl,
							       void *arg), void *arg);

void aura_status_changed_cb(struct aura_node *node, void (*cb)(struct aura_node *node, int newstatus, void *arg), void *arg);

void aura_fd_changed_cb(struct aura_node *node, void (*cb)(const struct aura_pollfds *fd, enum aura_fd_action act, void *arg), void *arg);

void aura_unhandled_evt_cb(struct aura_node *node, void (*cb)(struct aura_node *node,
							      struct aura_buffer *buf,
							      void *arg), void *arg);

void aura_object_migration_failed_cb(struct aura_node *node, void (*cb)(struct aura_node *node,
									struct aura_object *failed,
									void *arg), void *arg);

const struct aura_object *aura_get_current_object(struct aura_node *node);


struct aura_pollfds *aura_add_pollfds(struct aura_node *node, int fd, uint32_t events);
void aura_del_pollfds(struct aura_node *node, int fd);
int aura_get_pollfds(struct aura_node *node, const struct aura_pollfds **fds);

int aura_wait_status(struct aura_node *node, int status);
int aura_wait_status_timeout(struct aura_node *node, int status, struct timeval *timeout);

int aura_get_status(struct aura_node *node);
void *aura_get_userdata(struct aura_node *node);
void  aura_set_userdata(struct aura_node *node, void *udata);

struct aura_eventloop *aura_node_eventloop_get(struct aura_node *node);
void aura_node_eventloop_set(struct aura_node *node, struct aura_eventloop *loop);
void  aura_node_eventloopdata_set(struct aura_node *node, void *udata);
void *aura_node_eventloopdata_get(struct aura_node *node);
void *aura_get_transportdata(struct aura_node *node);
void  aura_set_transportdata(struct aura_node *node, void *udata);


#endif /* end of include guard: AURA_NODE_H */
