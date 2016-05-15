#include <aura/aura.h>
#include <unistd.h>
int main() {
        slog_init(NULL, 88);
        struct aura_node *n = aura_open("usb", "1d50:6032;www.ncrmnt.org;aura-usb-test-rig;");

        if (!n) {
                printf("err\n");
                return -1;
        }
        aura_close(n);
        return 0;
}
