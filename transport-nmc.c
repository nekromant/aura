#include <aura/aura.h>
#include <aura/private.h>
#include <easynmc.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

#define MAGIC 0xbeefbabe
#define AURA_MAGIC_HDR     0xdeadf00d
#define AURA_OBJECT_EVENT  0xdeadc0de
#define AURA_OBJECT_METHOD 0xdeadbeaf
#define AURA_STRLEN 16 

struct nmc_aura_object {
	uint32_t type;
	uint32_t id;
	uint32_t name[AURA_STRLEN];
	uint32_t argfmt[AURA_STRLEN];
	uint32_t retfmt[AURA_STRLEN];
	uint32_t ptr;
};


struct nmc_aura_header {
	uint32_t magic;
	uint32_t strlen;
};

struct nmc_aura_syncbuffer { 
	uint32_t owner;
	uint32_t id;
	uint32_t inbound_buffer_ptr;
	uint32_t outbound_buffer_ptr;
};

struct nmc_export_table {
	struct nmc_aura_header hdr; 
	struct nmc_aura_object objs[];
};

enum { 
	NMC_EVENT_STDIO=1<<0,
	NMC_EVENT_TRANSPORT=1<<1
};

struct nmc_private {
	struct easynmc_handle *h; 
	uint32_t eaddr;
	uint32_t esz;
	uint32_t ep;
	struct aura_node *node; 
	int is_online;
	uint32_t flags;
	struct aura_buffer *writing;
	struct aura_buffer *reading;
	int inbufsize;
	struct nmc_aura_syncbuffer *sbuf; 
};

char *nmc_fetch_str(void *nmstr)
{
	uint32_t *n = nmstr; 
	int len=0;
	while (n[len++]);;
	uint8_t *tmp = malloc(len);
	char *ret = (char *) tmp; 
	if (!tmp)
		return NULL;
	do { 
		*tmp++ = (uint8_t) (*n++ & 0xff); 
	} while (*n);
	*tmp = 0x0;
	return ret;
}

static int handle_aura_rpc_section(struct easynmc_handle *h, char* name, FILE *rfd, GElf_Shdr shdr)
{
	struct nmc_private *pv = easynmc_userdata_get(h);

	if (strcmp(name, ".aura_rpc_exports") == 0) { 
		uint32_t addr = shdr.sh_addr << 2;
		uint32_t size   = shdr.sh_size;

		slog(4, SLOG_DEBUG, "transport-nmc: Found RPC export section: %s at offset %u len %u", 
		     name, addr, size);

		if (!size) { 
			slog(4, SLOG_ERROR, "transport-nmc: RPC export section is empty!", 
			     name, pv->eaddr, pv->esz);
			return 1;
		}
		pv->eaddr = addr;
		pv->esz = size;
		return 1;
	}

	if (strcmp(name, ".aura_rpc_syncbuf") == 0) { 
		uint32_t addr = shdr.sh_addr << 2;
		uint32_t size   = shdr.sh_size;

		slog(4, SLOG_DEBUG, "transport-nmc: Found sync buffer: %s at offset %u len %u", 
		     name, addr, size);
		if (!size) { 
			slog(4, SLOG_ERROR, "transport-nmc: RPC export section is empty!", 
			     name, pv->eaddr, pv->esz);
			return 1;
		}

		pv->sbuf = (struct nmc_aura_syncbuffer*) &pv->h->imem[addr];
	}

	return 0;
}

static int parse_and_register_exports(struct nmc_private *pv)
{
	struct nmc_aura_object *cur;
	struct nmc_export_table *tbl = (struct nmc_export_table *) &pv->h->imem[pv->eaddr];
	struct aura_export_table *etbl;
	struct aura_node *node = pv->node;
	int count=0;
	int i;
	int max_buf_sz = 0;
 
	if (tbl->hdr.magic != AURA_MAGIC_HDR) {
		slog (0, SLOG_ERROR, "transport-nmc: Bad rpc magic");
		return -EIO; 
	}
	
	if (tbl->hdr.strlen != AURA_STRLEN) {
		slog (0, SLOG_ERROR, "transport-nmc: strlen mismatch: %d vs %d", 
		      tbl->hdr.strlen, AURA_STRLEN);
		return -EIO; 
	}

	slog(4, SLOG_DEBUG, "transport-nmc: parsing objects");

	cur = tbl->objs;
	while (cur->type != 0) { 
		if ((cur->type != AURA_OBJECT_METHOD) && 
		    (cur->type != AURA_OBJECT_EVENT))
			break;
		count++;
		cur++;
	}

	etbl = aura_etable_create(node, count);
	if (!etbl)
		BUG(node, "Failed to create etable");

	cur = tbl->objs;
	while (cur->type != 0) { 
		char *type = NULL; 
		if (cur->type == AURA_OBJECT_METHOD)
			type = "method";
		if (cur->type == AURA_OBJECT_EVENT)
			type = "event";
		if (!type) { 
			slog(0, SLOG_WARN, "transport-nmc: Bad data in object section, load stopped");
			break;
		}

		char *name = nmc_fetch_str(cur->name);
		char *arg  = nmc_fetch_str(cur->argfmt);
		char *ret  = nmc_fetch_str(cur->retfmt);

		aura_etable_add(etbl, name, arg, ret);

		slog(4, SLOG_DEBUG, "transport-nmc: %s name %s (%s : %s)", 
		     type, name, arg, ret);

		free(name);
		free(arg);
		free(ret);
		cur++;
	}

	for (i=0; i<etbl->next; i++) { 
		struct aura_object *o = &etbl->objects[i];
		if (o->retlen > max_buf_sz)
			max_buf_sz = o->retlen;
	}

	pv->inbufsize = max_buf_sz;

	aura_etable_activate(etbl);
	
	return 0;
}

static struct easynmc_section_filter rpc_filter = {
	.name = "aura_rpc_exports",
	.handle_section = handle_aura_rpc_section
};

static void nonblock(int fd, int state)
{
	struct termios ttystate;

	//get the terminal state
	tcgetattr(fd, &ttystate);
	if (state==1)
	{
		//turn off canonical mode
		ttystate.c_lflag &= ~ICANON;
		ttystate.c_lflag &= ~ECHO;
		ttystate.c_lflag = 0;
		ttystate.c_cc[VTIME] = 0; /* inter-character timer unused */
		ttystate.c_cc[VMIN] = 0; /* We're non-blocking */
		
	}
	else if (state==0)
	{
		//turn on canonical mode
		ttystate.c_lflag |= ICANON | ECHO;
	}
	//set the terminal attributes.
	tcsetattr(fd, TCSANOW, &ttystate);

}

static int nmc_open(struct aura_node *node, const char *filepath)
{
	int ret = -ENOMEM;
	struct easynmc_handle *h = easynmc_open(EASYNMC_CORE_ANY);
	if (!h)
		return -EIO;

	struct nmc_private *pv = calloc(1, sizeof(*pv));
	if (!pv) { 
		ret = -ENOMEM;
		goto errclose;
	}

	pv->node = node;
	pv->h = h;

	easynmc_userdata_set(h, pv);

	easynmc_register_section_filter(h, &rpc_filter);
	
	ret = easynmc_load_abs(h, filepath, &pv->ep, ABSLOAD_FLAG_DEFAULT);
	if (!pv->eaddr) { 
		slog(0, SLOG_ERROR, "transport-nmc: abs file doesn't have a valid aura_rpc_exports section");
		ret = -EIO;
		goto errfreemem;
	}
	
	ret = parse_and_register_exports(pv);
	if (ret != 0) 
		goto errfreemem;

	aura_set_userdata(node, pv);

	nonblock(h->iofd, 1);

	aura_add_pollfds(node, h->iofd,  (EPOLLIN | EPOLLET));
	aura_add_pollfds(node, h->memfd, (EPOLLPRI | EPOLLET));

	easynmc_start_app(h, pv->ep);

	aura_set_status(node, AURA_STATUS_ONLINE);
	return 0;

errfreemem:
	free(pv);
errclose:
	easynmc_close(h);
	return ret;
}

static void nmc_close(struct aura_node *node)
{
	slog(0, SLOG_INFO, "Closing dummy transport");
}


static void nmc_loop(struct aura_node *node, const struct aura_pollfds *fd)
{
	struct aura_object *o;
	struct nmc_private *pv = aura_get_userdata(node);
	struct easynmc_handle *h = pv->h;

	/* Handle state changes */
	if (!pv->is_online && (easynmc_core_state(h) == EASYNMC_CORE_RUNNING)) { 
		aura_set_status(node, AURA_STATUS_ONLINE);
		pv->is_online++;
	};

	if (pv->is_online && (easynmc_core_state(h) != EASYNMC_CORE_RUNNING)) {
		aura_set_status(node, AURA_STATUS_OFFLINE);
		pv->is_online = 0;
	};

	if (fd && (fd->fd == pv->h->iofd)) { 
		while (1) { 
			char tmp[128];
			int count; 
			slog(4, SLOG_DEBUG, "transport-nmc: We can read stdout!");
			count = read(fd->fd, tmp, 128);  
			if (count <= 0)
				break; /* Fuckup? Try later! */
			fwrite(tmp, 128, 1, stdout);
			if (count < 128) /* No more data */
				break;
		}
	}
	
	if (pv->sbuf && pv->sbuf->owner == 0) { 
		slog(4, SLOG_DEBUG, "transport-nmc: We can issue a call!");
		
	}

		/*
		if (outbound_pending(pv)) {
			struct aura_buffer *b = pv->writing; 
			ssize_t written = write(h->iofd, 
					       &b->data[b->pos],
					       b->size - b->pos);

			slog(4, SLOG_DEBUG, "%d bytes written", written);

			if (written >= 0) { 
				b->pos += written;
			} else if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) { 
				pv->flags &= ~NMC_CAN_WRITE;
			} else {
				BUG(node, "transport-nmc: Something nasty happened while writing");
			}

			if (b->pos == b->size) { 
				aura_buffer_release(node, b);
				pv->writing = NULL;
			}
		}

		if (inbound_pending(pv)) {
			struct aura_buffer *b = pv->reading;
			int ret, toread;
			struct aura_object *o;  
			struct nmc_aura_msg_hdr *msg = (struct nmc_aura_msg_hdr *) pv->reading->data;
			if (b->pos < sizeof(struct nmc_aura_msg_hdr))
				toread = sizeof(struct nmc_aura_msg_hdr) - b->pos;
			else {
				o = aura_etable_find_id(node->tbl, msg->objid);
				if (!o)
					BUG(node, "Invalid object id in message from NMC");
				toread = o->retlen + sizeof(struct nmc_aura_msg_hdr) - b->pos;
			}
			ret = read(h->iofd, &b->data[b->pos], toread);
			aura_hexdump("buf", b->data, b->size);

			if (ret >= 0) { 
				b->pos += ret;
			} else if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) { 
				pv->flags &= ~NMC_CAN_READ;
			} else {
				BUG(node, "transport-nmc: Something nasty happened while reading");
			}

			slog(4, SLOG_DEBUG, "%d/%d bytes read pos %d", ret, toread, b->pos);

			if (b->pos == toread) {
				b->userdata = o;
				aura_queue_buffer(&node->inbound_buffers, b);
				aura_eventloop_interrupt(aura_eventloop_get_data(node));
				pv->reading = NULL;
			}
		}
		*/
}

struct aura_buffer *ion_buffer_request(struct aura_node *node, int size)
{

}

void ion_buffer_release(struct aura_node *node, struct aura_buffer *buf)
{
	
}

static struct aura_transport nmc = { 
	.name = "nmc",
	.open = nmc_open,
	.close = nmc_close,
	.loop  = nmc_loop,
	.buffer_request = ion_buffer_request,
	.buffer_release = ion_buffer_release
};

AURA_TRANSPORT(nmc);

