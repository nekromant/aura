#include <aura/aura.h>

/**
 * Get host endianness
 *
 *
 * @return
 */
enum aura_endianness aura_get_host_endianness()
{
	union {
		uint32_t	i;
		char		c[4];
	} e = { 0x01000000 };

	if (e.c[0])
		return AURA_ENDIAN_BIG;
	else
		return AURA_ENDIAN_LITTLE;
}

/**
 * Get aura version string. The string should not be freed.
 *
 *
 * @return string containing version number, e.g. "0.1.2"
 */
const char *aura_get_version()
{
	return AURA_VERSION " " AURA_VERSION_GIT " " AURA_BUILD_TAG " built on " AURA_BUILD_DATE;
}

/**
 * Get a version number that can be used for comparison
 *
 * @return
 */
unsigned int aura_get_version_code()
{
	int a, b, c;

	sscanf(AURA_VERSION, "%d.%d.%d", &a, &b, &c);
	return a * 100000 + b * 1000 + c;
}


/**
 * Print a hexdump of a buffer
 *
 * @param desc
 * @param addr
 * @param len
 */
void aura_hexdump(char *desc, void *addr, int len)
{
	int i;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char *)addr;

	if (desc != NULL)
		printf("%s:\n", desc);

	for (i = 0; i < len; i++) {
		if ((i % 16) == 0) {
			if (i != 0)
				printf("  %s\n", buff);
			printf("  %04x ", i);
		}

		printf(" %02x", pc[i]);
		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
			buff[i % 16] = '.';
		else
			buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}

	while ((i % 16) != 0) {
		printf("   ");
		i++;
	}

	printf("  %s\n", buff);
}
