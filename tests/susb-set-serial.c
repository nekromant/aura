#include <aura/aura.h>
#include <unistd.h>
#define USB_STRING_DESCRIPTOR_HEADER(stringLength) ((2*(stringLength)+2) | (3<<8))


void write_serial(struct aura_node *node, const char *str)
{
	int ret;
	uint16_t *data = malloc(32); 
	int i = 0;
        struct aura_buffer *retbuf; 

	data[i++] = USB_STRING_DESCRIPTOR_HEADER(strlen(str));
	do { 
		data[i++] = *str++;
	} while (*str);

        ret = aura_call(node, "set_serial", &retbuf, 0, 0, data);
        printf("Serial number written with result %d, replug the device\n", ret);
	aura_buffer_release(node, retbuf); 	
	free(data);
}

int main(int argc, const char **argv) {

        slog_init(NULL, 88);

        struct aura_node *n = aura_open("simpleusb", "simpleusbconfigs/pw-ctl.conf");
        if (!n) { 
                printf("err\n");
                return -1;
        }

        aura_wait_status(n, AURA_STATUS_ONLINE);

	write_serial(n, argv[1]);

	

        aura_close(n);
        return 0;
}
