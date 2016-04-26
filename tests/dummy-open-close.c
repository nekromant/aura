#include <aura/aura.h>

int main() {
	slog_init(NULL, 18);

	struct aura_node *n = aura_open("dummy", NULL);
	if (!n)
		return -1;
		
	aura_close(n);

	return 0;
}
