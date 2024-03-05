
#ifndef DUCKLIB_CORE_H
#define DUCKLIB_CORE_H

#ifndef DECLSPEC
#  if defined(_WIN32)
#    if defined(EXPORTING_DUCKLIB)
#      define DECLSPEC __declspec(dllexport)
#    else
#      define DECLSPEC __declspec(dllimport)
#    endif
#  else /* non windows */
#    define DECLSPEC
#  endif
#endif

typedef unsigned char dl_bool_t;
#define dl_false ((dl_bool_t) 0)
#define dl_true ((dl_bool_t) 1)

typedef unsigned long dl_size_t;
typedef long dl_ptrdiff_t;

typedef unsigned char dl_uint8_t;
typedef unsigned short dl_uint16_t;
typedef unsigned int dl_uint32_t;
typedef unsigned long long dl_uint64_t;

typedef char dl_int8_t;
typedef short dl_int16_t;
typedef int dl_int32_t;

#define DL_UINT8_MAX    255U
#define DL_UINT16_MAX 65535U

#define DL_INT8_MAX     127
#define DL_INT16_MAX  32767

#define DL_INT8_MIN    -128
#define DL_INT16_MIN -32768


/* Word alignment @FIXME */
#define DL_ALIGNMENT 8


#define dl_null ((void *) 0)

typedef enum {
	dl_error_ok = 0,
	dl_error_invalidValue,
	dl_error_bufferUnderflow,
	dl_error_bufferOverflow,
	dl_error_nullPointer,
	dl_error_danglingPointer,
	dl_error_outOfMemory,
	dl_error_shouldntHappen,
	dl_error_cantHappen
} dl_error_t;

extern const dl_uint8_t *dl_errorString[];

#define dl_max(a,b) ((a > b) ? (a) : (b))
#define dl_min(a, b) ((a < b) ? (a) : (b))

#define TIF(condition, t, f) ((condition) ? (t) : (f))

dl_error_t DECLSPEC dl_memcopy(void *destination, const void *source, dl_size_t size);
void DECLSPEC dl_memcopy_noOverlap(void *destination, const void *source, const dl_size_t size);

void DECLSPEC dl_memclear(void *destination, dl_size_t size);

void DECLSPEC dl_strlen(dl_size_t *length, const dl_uint8_t *string);
#define DL_STR(DL_STR_string) ((dl_uint8_t *) DL_STR_string), (sizeof(DL_STR_string) - 1)

dl_uint8_t DECLSPEC dl_nybbleToHexChar(dl_uint8_t i);

#define DL_DOTIMES(I, TOP) for (dl_ptrdiff_t I = 0; (dl_size_t) I < (dl_size_t) (TOP); I++)

#endif /* DUCKLIB_CORE */
