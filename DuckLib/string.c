
#include "string.h"

dl_bool_t dl_string_isDigit(const dl_uint8_t character) {
	return (character >= '0') && (character <= '9');
}

dl_bool_t dl_string_isHexadecimalDigit(const dl_uint8_t character) {
	return (((character >= '0') && (character <= '9'))
	        || ((character >= 'a') && (character <= 'f'))
	        || ((character >= 'A') && (character <= 'F')));
}

dl_bool_t dl_string_isAlpha(const dl_uint8_t character) {
	return ((character >= 'a') && (character <= 'z')) || ((character >= 'A') && (character <= 'Z'));
}

dl_bool_t dl_string_isSpace(const dl_uint8_t character) {
	return character <= ' ';
}


dl_error_t dl_string_toBool(dl_bool_t *result, const dl_uint8_t *string, const dl_size_t string_length) {
	dl_error_t e = dl_error_ok;

	dl_string_compare(result, string, string_length, DL_STR("true"));
	if (!*result) {
		dl_string_compare(result, string, string_length, DL_STR("false"));
		if (!*result) {
			*result = dl_false;
			e = dl_error_invalidValue;
			goto cleanup;
		}
		else {
			*result = dl_false;
		}
	}
	else {
		*result = dl_true;
	}

 cleanup:
	return e;
}

dl_error_t dl_string_fromBool(dl_array_t *result, dl_bool_t boolean) {
	dl_error_t e = dl_error_ok;

	if (boolean) {
		e = dl_array_pushElements(result, DL_STR("true"));
		if (e) goto cleanup;
	}
	else {
		e = dl_array_pushElements(result, DL_STR("false"));
		if (e) goto cleanup;
	}

 cleanup: return e;
}

dl_error_t dl_string_fromUint8(dl_array_t *result, dl_uint8_t integer) {
	dl_error_t e = dl_error_ok;

	dl_array_t reversedResult;
	(void) dl_array_init(&reversedResult, sizeof(dl_uint8_t), dl_array_strategy_double);

	if (integer == 0) {
		dl_uint8_t tempChar = '0';
		e = dl_array_pushElement(&reversedResult, &tempChar);
		if (e) goto cleanup;
	}
	else {
		while (integer > 0) {
			dl_uint8_t tempChar = '0' + (integer % 10);
			integer /= 10;
			e = dl_array_pushElement(&reversedResult, &tempChar);
			if (e) goto cleanup;
		}
	}

	dl_size_t length = reversedResult.elements_length;
	DL_DOTIMES(i, length) {
		e = dl_array_pushElement(result, &((char *) reversedResult.elements)[length - i - 1]);
		if (e) goto cleanup;
	}

 cleanup:
	(void) dl_array_quit(&reversedResult);
	return e;
}

dl_error_t dl_string_toPtrdiff(dl_ptrdiff_t *result, const dl_uint8_t *string, const dl_size_t string_length) {
	dl_error_t e = dl_error_ok;

	dl_ptrdiff_t index = 0;
	dl_bool_t tempBool;
	dl_bool_t negative = dl_false;
	dl_bool_t hexadecimal = dl_false;

	*result = 0;

	if ((dl_size_t) index >= string_length) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	if (string[index] == '-') {
		index++;

		if ((dl_size_t) index >= string_length) {
			e = dl_error_invalidValue;
			goto cleanup;
		}

		negative = dl_true;
	}

	if (!dl_string_isDigit(string[index])) {
		e = dl_error_invalidValue;
		goto cleanup;
	}

	/* Hexadecimal */
	if ((dl_size_t) index < string_length) {
		if ((string[index] == '0')
		    && ((dl_size_t) index + 1 < string_length)
		    && ((string[index + 1] == 'x')
		        || (string[index + 1] == 'X'))) {
			hexadecimal = dl_true;
			index += 2;
		}
	}

	while ((dl_size_t) index < string_length) {
		if (hexadecimal) {
			tempBool = dl_string_isHexadecimalDigit(string[index]);
		}
		else {
			tempBool = dl_string_isDigit(string[index]);
		}
		if (!tempBool) {
			e = dl_error_invalidValue;
			goto cleanup;
		}

		if (hexadecimal) {
			*result = (*result * 16
			           + (('0' <= string[index]) && (string[index] <= '9')
			              ? string[index] - '0'
			              : ('a' <= string[index]) && (string[index] <= 'f')
			              ? string[index] - 'a' + 10
			              : (string[index] - 'A' + 10)));
		}
		else {
			*result = *result * 10 + (string[index] - '0');
		}

		index++;
	}

	if (negative) {
		*result = -*result;
	}

 cleanup:
	return e;
}

dl_error_t dl_string_fromPtrdiff(dl_array_t *result, dl_ptrdiff_t ptrdiff) {
	dl_error_t e = dl_error_ok;

	dl_array_t reversedResult;
	(void) dl_array_init(&reversedResult, sizeof(dl_uint8_t), dl_array_strategy_double);

	if (ptrdiff == 0) {
		dl_uint8_t tempChar = '0';
		e = dl_array_pushElement(&reversedResult, &tempChar);
		if (e) goto cleanup;
	}
	else if (ptrdiff < 0) {
		dl_uint8_t tempChar = '-';
		e = dl_array_pushElement(result, &tempChar);
		if (e) goto cleanup;
		while (ptrdiff < 0) {
			dl_uint8_t tempChar = '0' - (ptrdiff % 10);
			ptrdiff /= 10;
			e = dl_array_pushElement(&reversedResult, &tempChar);
			if (e) goto cleanup;
		}
	}
	else {
		while (ptrdiff > 0) {
			dl_uint8_t tempChar = '0' + (ptrdiff % 10);
			ptrdiff /= 10;
			e = dl_array_pushElement(&reversedResult, &tempChar);
			if (e) goto cleanup;
		}
	}

	dl_size_t length = reversedResult.elements_length;
	DL_DOTIMES(i, length) {
		e = dl_array_pushElement(result, &((char *) reversedResult.elements)[length - i - 1]);
		if (e) goto cleanup;
	}

 cleanup:
	(void) dl_array_quit(&reversedResult);
	return e;
}

dl_error_t dl_string_fromSize(dl_array_t *result, dl_size_t sz) {
	dl_error_t e = dl_error_ok;

	dl_array_t reversedResult;
	(void) dl_array_init(&reversedResult, sizeof(dl_uint8_t), dl_array_strategy_double);

	if (sz == 0) {
		dl_uint8_t tempChar = '0';
		e = dl_array_pushElement(&reversedResult, &tempChar);
		if (e) goto cleanup;
	}
	else {
		while (sz > 0) {
			dl_uint8_t tempChar = '0' + (sz % 10);
			sz /= 10;
			e = dl_array_pushElement(&reversedResult, &tempChar);
			if (e) goto cleanup;
		}
	}

	dl_size_t length = reversedResult.elements_length;
	DL_DOTIMES(i, length) {
		e = dl_array_pushElement(result, &((char *) reversedResult.elements)[length - i - 1]);
		if (e) goto cleanup;
	}

 cleanup:
	(void) dl_array_quit(&reversedResult);
	return e;
}

dl_error_t dl_string_toDouble(double *result, const dl_uint8_t *string, const dl_size_t string_length) {
	dl_error_t e = dl_error_ok;

	dl_ptrdiff_t index = 0;
	dl_bool_t tempBool;
	dl_bool_t negative = dl_false;
	dl_ptrdiff_t power = 0;
	dl_bool_t power_negative = dl_false;

	*result = 0.0;

	if (string[index] == '-') {
		index++;

		if ((dl_size_t) index >= string_length) {
			e = dl_error_invalidValue;
			goto cleanup;
		}

		negative = dl_true;
	}

	/* Try .1 */
	if (string[index] == '.') {
		index++;

		if ((dl_size_t) index >= string_length) {
			e = dl_error_invalidValue;
			goto cleanup;
		}

		power = 10;

		if (!dl_string_isDigit(string[index])) {
			e = dl_error_invalidValue;
			goto cleanup;
		}

		*result += (double) (string[index] - '0') / (double) power;

		index++;

		while (((dl_size_t) index < string_length) && (dl_string_toLower(string[index]) != 'e')) {
			power = 10 * power;

			tempBool = dl_string_isDigit(string[index]);
			if (!tempBool) {
				e = dl_error_invalidValue;
				goto cleanup;
			}

			*result += (double) (string[index] - '0') / (double) power;
			index++;
		}
	}
	// Try 1.2, 1., and 1
	else {
		if (!dl_string_isDigit(string[index])) {
			e = dl_error_invalidValue;
			goto cleanup;
		}

		*result = string[index] - '0';

		index++;

		while (((dl_size_t) index < string_length)
		       && (dl_string_toLower(string[index]) != 'e')
		       && (string[index] != '.')) {
			if (!dl_string_isDigit(string[index])) {
				e = dl_error_invalidValue;
				goto cleanup;
			}

			*result = *result * 10.0 + (double) (string[index] - '0');

			index++;
		}

		if (string[index] == '.') {
			index++;

			if ((dl_size_t) index >= string_length) {
				// eError = duckLisp_error_push(duckLisp, DL_STR("Expected a digit after decimal point."), index);
				// e = eError ? eError : dl_error_bufferOverflow;
				// This is expected. 1. 234.e61  435. for example.
				goto cleanup;
			}

			power = 1;
		}

		while (((dl_size_t) index < string_length) && (dl_string_toLower(string[index]) != 'e')) {
			power = 10 * power;
			if (!dl_string_isDigit(string[index])) {
				e = dl_error_invalidValue;
				goto cleanup;
			}

			*result += (double) (string[index] - '0') / (double) power;

			index++;
		}
	}

	// …e3
	if (dl_string_toLower(string[index]) == 'e') {
		index++;

		if ((dl_size_t) index >= string_length) {
			e = dl_error_invalidValue;
			goto cleanup;
		}

		if (string[index] == '-') {
			index++;

			if ((dl_size_t) index >= string_length) {
				e = dl_error_invalidValue;
				goto cleanup;
			}

			power_negative = dl_true;
		}

		if (!dl_string_isDigit(string[index])) {
			e = dl_error_invalidValue;
			goto cleanup;
		}

		power = string[index] - '0';

		index++;

		while ((dl_size_t) index < string_length) {
			if (!dl_string_isDigit(string[index])) {
				e = dl_error_invalidValue;
				goto cleanup;
			}

			power = power * 10 + string[index] - '0';

			index++;
		}

		if (power_negative) {
			if (power == 0) {
				e = dl_error_invalidValue;
				goto cleanup;
			}

			for (dl_ptrdiff_t i = 0; i < power; i++) {
				*result /= 10.0;
			}
		}
		else {
			for (dl_ptrdiff_t i = 0; i < power; i++) {
				*result *= 10.0;
			}
		}
	}

	if ((dl_size_t) index != string_length) {
		e = dl_error_cantHappen;
		goto cleanup;
	}

	if (negative) {
		*result = -*result;
	}

	cleanup:

	return e;
}


void dl_string_compare(dl_bool_t *result,
                       const dl_uint8_t *str1,
                       const dl_size_t str1_length,
                       const dl_uint8_t *str2,
                       const dl_size_t str2_length) {
	*result = dl_true;
	if (str1_length != str2_length) {
		*result = dl_false;
	}
	else for (dl_ptrdiff_t i = 0; (dl_size_t) i < str1_length; i++) {
		if (str1[i] != str2[i]) {
			*result = dl_false;
			break;
		}
	}
}

void dl_string_compare_partial(dl_bool_t *result, const dl_uint8_t *str1, const dl_uint8_t *str2, const dl_size_t length) {
	*result = dl_true;
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < length; i++) {
		if (str1[i] != str2[i]) {
			*result = dl_false;
			break;
		}
	}
}
