#include <aura/aura.h>
#include <unistd.h>
int main() {
        slog_init(NULL, 88);
        struct aura_node *n = aura_open("simpleusb", "../simpleusbconfigs/susb-test.conf");

        if (!n) {
                printf("err\n");
                return -1;
        }
        aura_close(n);
        return 0;
}
