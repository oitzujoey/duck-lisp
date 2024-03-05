
#ifndef DUCKLIB_STRING_H
#define DUCKLIB_STRING_H

#include "core.h"
#include "array.h"

dl_bool_t DECLSPEC dl_string_isDigit(const dl_uint8_t character);
dl_bool_t DECLSPEC dl_string_isHexadecimalDigit(const dl_uint8_t character);
dl_bool_t DECLSPEC dl_string_isAlpha(const dl_uint8_t character);
dl_bool_t DECLSPEC dl_string_isSpace(const dl_uint8_t character);

dl_error_t DECLSPEC dl_string_toBool(dl_bool_t *result, const dl_uint8_t *string, const dl_size_t string_length);
dl_error_t dl_string_fromBool(dl_array_t *result, dl_bool_t boolean);
dl_error_t dl_string_fromUint8(dl_array_t *result, dl_uint8_t integer);
dl_error_t DECLSPEC dl_string_toPtrdiff(dl_ptrdiff_t *result, const dl_uint8_t *string, const dl_size_t string_length);
dl_error_t dl_string_fromPtrdiff(dl_array_t *result, dl_ptrdiff_t ptrdiff);
dl_error_t dl_string_fromSize(dl_array_t *result, dl_size_t sz);
dl_error_t DECLSPEC dl_string_toDouble(double *result, const dl_uint8_t *string, const dl_size_t string_length);

void DECLSPEC dl_string_compare(dl_bool_t *result,
                                const dl_uint8_t *str1,
                                const dl_size_t str1_length,
                                const dl_uint8_t *str2,
                                const dl_size_t str2_length);
void DECLSPEC dl_string_compare_partial(dl_bool_t *result,
                                        const dl_uint8_t *str1,
                                        const dl_uint8_t *str2,
                                        const dl_size_t length);

#define dl_string_toLower(c) (((c >= 'A') && (c <= 'Z')) ? (c - 'A' + 'a') : c)
#define dl_string_toUpper(c) (((c >= 'a') && (c <= 'z')) ? (c - 'a' + 'A') : c)

#endif /* DUCKLIB_STRING_H */
