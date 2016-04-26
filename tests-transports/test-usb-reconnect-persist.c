#include <aura/aura.h>
#include <unistd.h>


int do_test_once(struct aura_node *n)
{
	int ret;

	aura_wait_status(n, AURA_STATUS_ONLINE);
	struct aura_buffer *retbuf;

	ret = aura_call(n, "turnTheLedOn", &retbuf, 0x100, 0x100);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (0 != ret)
		return 1;

	aura_buffer_release(retbuf);
	aura_wait_status(n, AURA_STATUS_OFFLINE);
	return 0;
}

int main()
{
	int ret = 0;

	slog_init(NULL, 88);

	struct aura_node *n = aura_open("usb", "1d50:6032;www.ncrmnt.org;aura-usb-test-rig;");
	if (!n)
		return -1;

	ret += do_test_once(n);
	ret += do_test_once(n);
	ret += do_test_once(n);

	aura_close(n);
	return ret;
}
