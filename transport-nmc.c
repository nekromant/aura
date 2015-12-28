#include <aura/aura.h>
#include <aura/private.h>
#include <linux/easynmc.h>
#include <easynmc.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <ion/ion.h>

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

enum { 
	SYNCBUF_IDLE = 0,
	SYNCBUF_ARGOUT = 1,
	SYNCBUF_RETIN = 2
};

struct nmc_aura_syncbuffer { 
	uint32_t state;
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

struct ion_buffer_descriptor { 
	int map_fd;
	int share_fd;
	int size; 
	ion_user_handle_t hndl; 
	struct aura_buffer *mapped_buf;
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
	int ion_fd; 
	struct aura_buffer *current_in;
	struct aura_buffer *current_out;
};

static char *nmc_fetch_str(void *nmstr)
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
	pv->ion_fd = -1; /* Default to invalid */
	
	if (pv->ion_fd < 0)
		pv->ion_fd = ion_open();	
	if (pv->ion_fd < 0) { 
		slog(0, SLOG_ERROR, "Failed to init ion");
		goto errfreemem;
	}

	if ((sizeof(struct aura_buffer) % 4))
		BUG(node, "Internal BUG: aura_buffer header must be 4-byte aligned")

	pv->node = node;
	pv->h = h;

	easynmc_userdata_set(h, pv);

	easynmc_register_section_filter(h, &rpc_filter);	

	ret = easynmc_load_abs(h, filepath, &pv->ep, ABSLOAD_FLAG_DEFAULT);
	if ((ret != 0) || (!pv->eaddr)) { 
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

static void fetch_stdout(struct aura_node *node)
{
	struct nmc_private *pv = aura_get_userdata(node);
	while (1) { 
		char tmp[128];
		int count; 
		slog(4, SLOG_DEBUG, "transport-nmc: We can read stdout!");
		count = read(pv->h->iofd, tmp, 128);  
		if (count <= 0)
			break; /* Fuckup? Try later! */
		fwrite(tmp, count, 1, stdout);
		if (count < 128) /* No more data */
			break;
	}
		
}

static void nmc_close(struct aura_node *node)
{
	struct nmc_private *pv = aura_get_userdata(node);
	/* 
	   TODO: Uncomment when driver's fixed
	   fetch_stdout(node);
	*/

	aura_del_pollfds(node, pv->h->iofd);
	aura_del_pollfds(node, pv->h->memfd);
	easynmc_close(pv->h);
	if (pv->current_out)
		aura_buffer_release(pv->current_out);
	if (pv->current_in)
		aura_buffer_release(pv->current_in);
	free(pv);
}

static uint32_t aura_buffer_to_nmc(struct aura_buffer *buf)
{
	struct aura_node *node = buf->owner;
	struct nmc_private *pv = aura_get_userdata(node);
	struct ion_buffer_descriptor *dsc = buf->transportdata;
	uint32_t nmaddress;

	if (dsc->share_fd == -1) { /* Already shared? */
		int ret = ion_share(pv->ion_fd, dsc->hndl, &dsc->share_fd);
		if (ret) 
			BUG(node, "ion_share() failed");
	}
	
	nmaddress = easynmc_ion2nmc(pv->h, dsc->share_fd);
	if (!nmaddress)
		BUG(node, "Failed to obtain nm address handle");

	return nmaddress;
}

static inline void do_issue_next_call(struct aura_node *node)
{
	struct aura_buffer *in_buf, *out_buf;
	struct nmc_private *pv = aura_get_userdata(node);
	struct aura_object *o;

	int data_offset = (sizeof(struct aura_buffer) / 4);

	out_buf = aura_dequeue_buffer(&node->outbound_buffers); 
	if (!out_buf)
		return;

	o = out_buf->object;
	in_buf = aura_buffer_request(node, o->retlen);
	if (!in_buf)
		BUG(node, "Buffer allocation faield");

	in_buf->object = o; 
	pv->current_out = out_buf;
	pv->current_in  = in_buf;
	pv->sbuf->id = o->id;
	
	pv->sbuf->outbound_buffer_ptr = aura_buffer_to_nmc(out_buf) + data_offset;
	pv->sbuf->inbound_buffer_ptr  = aura_buffer_to_nmc(in_buf) + data_offset;
	pv->sbuf->state = SYNCBUF_ARGOUT;
}

static void nmc_loop(struct aura_node *node, const struct aura_pollfds *fd)
{
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
	
	if (fd && (fd->fd == pv->h->iofd))
		fetch_stdout(node);
	
	if (pv->sbuf) 
		switch (pv->sbuf->state) { 
		case SYNCBUF_IDLE:
			slog(4, SLOG_DEBUG, "transport-nmc: We can issue our call");
			do_issue_next_call(node);
			break;
		case SYNCBUF_ARGOUT:
			slog(4, SLOG_DEBUG, "transport-nmc: NMC is still working");
			break;
		case SYNCBUF_RETIN:
			aura_buffer_release(pv->current_out);
			aura_queue_buffer(&node->inbound_buffers, pv->current_in);
			pv->current_out = NULL;
			pv->current_in  = NULL;
			pv->sbuf->state = SYNCBUF_IDLE;
			slog(4, SLOG_DEBUG, "transport-nmc: We got something from nmc");
			break;
		default:
			BUG(node, "Unexpected syncbuf state");
			break;
		}
}


struct aura_buffer *ion_buffer_request(struct aura_node *node, int size)
{
	int ret;
	int map_fd;
	ion_user_handle_t hndl;
	struct aura_buffer *buf;
	struct nmc_private *pv = aura_get_userdata(node);
	struct ion_buffer_descriptor *dsc = malloc(sizeof(*dsc));

	if (!dsc)
		BUG(node, "malloc failed!");

	ret = ion_alloc(pv->ion_fd, size, 0x10, 0xf, 0,  &hndl);	
	if (ret) 
		BUG(node, "ION allocation failed");
	
	ret = ion_map(pv->ion_fd, hndl, size, (PROT_READ | PROT_WRITE), 
		      MAP_SHARED, 0, (void *) &buf, &map_fd);
	if (ret)
		BUG(node, "ION mmap failed");
	
	dsc->map_fd = map_fd; 
	dsc->hndl = hndl;
	dsc->mapped_buf = buf; 
	dsc->size = size;
	dsc->share_fd = -1;
	buf->transportdata = dsc;
	return buf;
}

static void ion_buffer_release(struct aura_node *node, struct aura_buffer *buf)
{
	struct nmc_private *pv = aura_get_userdata(node);
	struct ion_buffer_descriptor *dsc = buf->transportdata;
	int ret;

	ret = ion_free(pv->ion_fd, dsc->hndl);
	if (ret)
		BUG(node, "Shit happened when doing ion_free(): %d", ret);
	munmap(buf, dsc->size);
	close(dsc->map_fd);
	free(dsc);
}

/* 
 * TODO: 
 * This buffer passing mechanism is very hacky and will not work on 64 bit
 * A total refactor is required, perhaps altering the aura core
 */

void nmc_buffer_put(struct aura_buffer *dst, struct aura_buffer *buf)
{
	if (sizeof(void*) != 4)
		BUG(dst->owner, "You know why we are screwed here, jerk!");

	int data_offset = (sizeof(struct aura_buffer) / 4);
	uint64_t buf_addrs = aura_buffer_to_nmc(buf) + data_offset;
	buf_addrs |= (((uint64_t) (uint32_t) buf) << 32);
	aura_buffer_put_u64(dst, buf_addrs);
	slog(4, SLOG_DEBUG, "nmc: serialized buf 0x%x to 0x%llx ", buf, buf_addrs);
}

struct aura_buffer *nmc_buffer_get(struct aura_buffer *buf)
{
	uint64_t addrs = aura_buffer_get_u64(buf);
	struct aura_buffer *ret = (struct aura_buffer *) ((uint32_t) (addrs >> 32));
	slog(4, SLOG_DEBUG, "nmc: deserialized buf 0x%x from 0x%llx ", buf, addrs);
	return ret;
}

static struct aura_transport nmc = { 
	.name = "nmc",
	.open = nmc_open,
	.close = nmc_close,
	.loop  = nmc_loop,
	.buffer_offset = 0,
	.buffer_overhead =0,
	.buffer_request = ion_buffer_request,
	.buffer_release = ion_buffer_release,
	.buffer_get = nmc_buffer_get,
	.buffer_put = nmc_buffer_put,
};

AURA_TRANSPORT(nmc);
