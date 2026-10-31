/* Review fixture: link by the ABI name, without haven/string.h macros. */
#include <stddef.h>

extern void *memcpy(void *dst, const void *src, size_t n);

int main(void)
{
	unsigned char src[96], dst[96];
	for (size_t s = 0; s < 8; ++s) {
		for (size_t d = 0; d < 8; ++d) {
			for (size_t n = 0; n <= 64; ++n) {
				for (size_t i = 0; i < sizeof(src); ++i) {
					src[i] = (unsigned char)(i * 7 + 3);
					dst[i] = 0xa5;
				}
				if (memcpy(dst + d, src + s, n) != dst + d)
					return 1;
				for (size_t i = 0; i < sizeof(dst); ++i) {
					unsigned char expected =
						(i >= d && i < d + n) ?
							src[s + i - d] : 0xa5;
					if (dst[i] != expected)
						return 2;
				}
				/* GCC's runtime contract includes exact self-copy. */
				if (memcpy(src + s, src + s, n) != src + s)
					return 3;
				for (size_t i = 0; i < sizeof(src); ++i)
					if (src[i] != (unsigned char)(i * 7 + 3))
						return 4;
			}
		}
	}
	return 0;
}
