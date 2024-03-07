
#ifdef USE_STDLIB
#include <string.h>
#endif /* USE_STDLIB */
#include "core.h"

const dl_uint8_t *dl_errorString[] = {
#define X(S) (dl_uint8_t *) S
	X("dl_error_ok"),
	X("dl_error_invalidValue"),
	X("dl_error_bufferUnderflow"),
	X("dl_error_bufferOverflow"),
	X("dl_error_nullPointer"),
	X("dl_error_danglingPointer"),
	X("dl_error_outOfMemory"),
	X("dl_error_shouldntHappen"),
	X("dl_error_cantHappen")
};

#ifdef USE_STDLIB
void dl_memcopy(void *destination, const void *source, dl_size_t size) {
	(void) memmove(destination, source, size);
}
#else /* USE_STDLIB */
void dl_memcopy(void *destination, const void *source, dl_size_t size) {
	const dl_uint8_t *s;

	if (destination > source) {
		s = (dl_uint8_t*)source + size - 1;
		for (dl_uint8_t *d = (dl_uint8_t*)destination + size - 1; s >= (dl_uint8_t *) source; --d, --s) {
			*d = *s;
		}
	}
	else if (destination < source) {
		s = source;
		for (dl_uint8_t *d = destination; s < (dl_uint8_t *) source + size; d++, s++) {
			*d = *s;
		}
	}
}
#endif /* USE_STDLIB */

void dl_memcopy_noOverlap(void *destination, const void *source, const dl_size_t size) {
	if (size) {
		const dl_uint8_t *s;
		s = source;
		for (dl_uint8_t *d = destination; s < (dl_uint8_t *) source + size; d++, s++) {
			*d = *s;
		}
	}
}

#ifdef USE_STDLIB
void dl_memclear(void *destination, dl_size_t size) {
	(void) memset(destination, 0, size);
}
#else /* USE_STDLIB */
void dl_memclear(void *destination, dl_size_t size) {
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < size; i++) {
		((dl_uint8_t *) destination)[i] = 0;
	}
}
#endif /* USE_STDLIB */

void dl_strlen(dl_size_t *length, const dl_uint8_t *string) {
	dl_size_t i = 0;
	while (string[i] != '\0') {
		i++;
	}
	*length = i;
}

dl_uint8_t dl_nybbleToHexChar(dl_uint8_t i) {
	i &= 0xFU;
	if (i < 10) {
		return i + '0';
	}
	else {
		return i - 10 + 'A';
	}
}
