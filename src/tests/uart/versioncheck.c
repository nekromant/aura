#include <aura/aura.h>

int main() {
	slog_init(NULL, 18);
	
	printf("libaura version %s (numeric %u)\n", 
	       aura_get_version(), aura_get_version_code());

	return 0;
}


