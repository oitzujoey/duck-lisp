/*
MIT License

Copyright (c) 2023 Joseph Herguth

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "duckLisp.h"

char *duckLisp_disassemble(dl_memoryAllocation_t *memoryAllocation,
                           const dl_uint8_t *bytecode,
                           const dl_size_t length) {
	dl_error_t e = dl_error_ok;

	dl_array_t disassembly;
	/**/ dl_array_init(&disassembly, memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_uint8_t opcode;
	dl_ptrdiff_t arg = 0;
	dl_uint8_t tempChar;
	dl_size_t tempSize;
	for (dl_ptrdiff_t i = 0; (dl_size_t) i < length; i++) {
		if (arg == 0) opcode = bytecode[i];

		switch (opcode) {
		case duckLisp_instruction_nop:
			e = dl_array_pushElements(&disassembly, DL_STR("nop\n"));
			if (e) return dl_null;
			arg = 0;
			continue;

		case duckLisp_instruction_pushString8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-string.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '"';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			default:
				if (tempSize > 0) {
					if (bytecode[i] == '\n') {
						e = dl_array_pushElements(&disassembly, DL_STR("\\n"));
						if (e) return dl_null;
					}
					else {
						e = dl_array_pushElement(&disassembly, (dl_uint8_t *) &bytecode[i]);
						if (e) return dl_null;
					}
					--tempSize;
					if (!tempSize) {
						tempChar = '"';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = '\n';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						arg = 0;
						continue;
					}
					break;
				}
				--i;
				tempChar = '"';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			}
			break;

		case duckLisp_instruction_pushSymbol8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-symbol.8      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '"';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			default:
				if (tempSize > 0) {

					if (bytecode[i] == '\n') {
						e = dl_array_pushElements(&disassembly, DL_STR("\\n"));
						if (e) return dl_null;
					}
					else {
						e = dl_array_pushElement(&disassembly, (dl_uint8_t *) &bytecode[i]);
						if (e) return dl_null;
					}
					--tempSize;
					if (!tempSize) {
						tempChar = '"';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = '\n';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushSymbol16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-symbol.16     "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '"';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			default:
				if (tempSize > 0) {

					if (bytecode[i] == '\n') {
						e = dl_array_pushElements(&disassembly, DL_STR("\\n"));
						if (e) return dl_null;
					}
					else {
						e = dl_array_pushElement(&disassembly, (dl_uint8_t *) &bytecode[i]);
						if (e) return dl_null;
					}
					--tempSize;
					if (!tempSize) {
						tempChar = '"';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = '\n';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushSymbol32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-symbol.32     "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '"';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			default:
				if (tempSize > 0) {

					if (bytecode[i] == '\n') {
						e = dl_array_pushElements(&disassembly, DL_STR("\\n"));
						if (e) return dl_null;
					}
					else {
						e = dl_array_pushElement(&disassembly, (dl_uint8_t *) &bytecode[i]);
						if (e) return dl_null;
					}
					--tempSize;
					if (!tempSize) {
						tempChar = '"';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = '\n';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushBooleanFalse:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-boolean-false"));
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushBooleanTrue:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-boolean-true"));
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushInteger8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-integer.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushInteger16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-integer.16 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushInteger32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-integer.32 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushDoubleFloat:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-double-float  "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushIndex8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-index.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushUpvalue8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-upvalue.8	"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushUpvalue16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-upvalue.16 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pushUpvalue32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-upvalue.32 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushVaClosure8:
			/* Fall through */
		case duckLisp_instruction_pushClosure8:
			switch (arg) {
			case 0:
				if (opcode == duckLisp_instruction_pushClosure8) {
					e = dl_array_pushElements(&disassembly, DL_STR("push-closure.8     "));
				}
				else {
					e = dl_array_pushElements(&disassembly, DL_STR("push-va-closure.8  "));
				}
				if (e) return dl_null;
				break;
			case 1:
				// Function address
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 2:
				// Arity
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 3;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 2;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 5:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 1;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) tempChar = '\n';
				else tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) {
					arg = 0;
					continue;
				}
				break;
			default:
				// Upvalues
				if (tempSize-- > 0) {
					DL_DOTIMES(m, 4) {
						tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						if (m != (4 - 1)) i++;
					}
					if (tempSize == 0) tempChar = '\n';
					else tempChar = ' ';
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					if (tempSize == 0) {
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushVaClosure16:
			/* Fall through */
		case duckLisp_instruction_pushClosure16:
			switch (arg) {
			case 0:
				if (opcode == duckLisp_instruction_pushClosure16) {
					e = dl_array_pushElements(&disassembly, DL_STR("push-closure.16    "));
				}
				else {
					e = dl_array_pushElements(&disassembly, DL_STR("push-va-closure.16 "));
				}
				if (e) return dl_null;
				break;
			case 1:
				// Function address
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				// Arity
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 4:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 3;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 5:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 2;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 1;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) tempChar = '\n';
				else tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) {
					arg = 0;
					continue;
				}
				break;
			default:
				// Upvalues
				if (tempSize-- > 0) {
					DL_DOTIMES(m, 4) {
						tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						if (m != (4 - 1)) i++;
					}
					if (tempSize == 0) tempChar = '\n';
					else tempChar = ' ';
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					if (tempSize == 0) {
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushVaClosure32:
			/* Fall through */
		case duckLisp_instruction_pushClosure32:
			switch (arg) {
			case 0:
				if (opcode == duckLisp_instruction_pushClosure32) {
					e = dl_array_pushElements(&disassembly, DL_STR("push-closure.32    "));
				}
				else {
					e = dl_array_pushElements(&disassembly, DL_STR("push-va-closure.32 "));
				}
				if (e) return dl_null;
				break;
			case 1:
				// Function address
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				// Arity
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 6:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 3;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 2;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 1;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 9:
				tempSize |= (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) tempChar = '\n';
				else tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) {
					arg = 0;
					continue;
				}
				break;
			default:
				// Upvalues
				if (tempSize-- > 0) {
					DL_DOTIMES(m, 4) {
						tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						if (m != (4 - 1)) i++;
					}
					if (tempSize == 0) tempChar = '\n';
					else tempChar = ' ';
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					if (tempSize == 0) {
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pushGlobal8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("push-global.8   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_setUpvalue8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-upvalue.8   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setUpvalue16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-upvalue.16  "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setUpvalue32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-upvalue.32  "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_setStatic8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-global.8    "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_releaseUpvalues8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("release-uvs.8         "));
				if (e) return dl_null;
				break;
			case 1:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) tempChar = '\n';
				else tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) {
					arg = 0;
					continue;
				}
				break;
			default:
				// Upvalues
				if (tempSize-- > 0) {
					const dl_size_t top = 1;
					DL_DOTIMES(m, top) {
						tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						if ((dl_size_t) m != (top - 1)) i++;
					}
					if (tempSize == 0) tempChar = '\n';
					else tempChar = ' ';
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					if (tempSize == 0) {
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_releaseUpvalues16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("release-uvs.16        "));
				if (e) return dl_null;
				break;
			case 1:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) {
					arg = 0;
					continue;
				}
				break;
			default:
				// Upvalues
				if (tempSize-- > 0) {
					const dl_size_t top = 2;
					DL_DOTIMES(m, top) {
						tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						if ((dl_size_t) m != (top - 1)) i++;
					}
					if (tempSize == 0) tempChar = '\n';
					else tempChar = ' ';
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					if (tempSize == 0) {
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_releaseUpvalues32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("release-uvs.32        "));
				if (e) return dl_null;
				break;
			case 1:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				// Upvalues length
				tempSize = (dl_size_t) bytecode[i] << 8 * 0;
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) tempChar = '\n';
				else tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				if (tempSize == 0) {
					arg = 0;
					continue;
				}
				break;
			default:
				// Upvalues
				if (tempSize-- > 0) {
					const dl_size_t top = 4;
					DL_DOTIMES(m, top) {
						tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						if ((dl_size_t) m != (top - 1)) i++;
					}
					if (tempSize == 0) tempChar = '\n';
					else tempChar = ' ';
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					if (tempSize == 0) {
						arg = 0;
						continue;
					}
					break;
				}
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_call8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("call.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_call16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("call.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_call32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("call.32         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_acall8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("acall.8         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_acall16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("acall.16        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_acall32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("acall.32        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_funcall8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("funcall.8       "));
				if (e) return dl_null;
				break;

			case 1:
				// Function index
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 2:
				// Arity
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;

			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_funcall16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("funcall.16      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				// Arity
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_funcall32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("funcall.32      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				// Arity
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_apply8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("apply.8         "));
				if (e) return dl_null;
				break;

			case 1:
				// Function index
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 2:
				// Arity
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;

			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_apply16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("apply.16        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_apply32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("apply.32        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_ccall8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("c-call.8        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_brnz8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("brnz.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_brnz16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("brnz.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_brnz32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("brnz.32         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_jump8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("jump.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_jump16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("jump.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_jump32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("jump.32         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_move8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("move.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_move16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("move.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_move32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("move.32         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_pop8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("pop.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pop16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("pop.16          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_pop32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("pop.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_not8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("not.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_not16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("not.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_not32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("not.32           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_add8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("add.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_add16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("add.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_add32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("add.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_mul8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("mul.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_mul16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("mul.16		   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_mul32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("mul.32			"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_div8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("div.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_div16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("div.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_div32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("div.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_sub8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("sub.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_sub16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("sub.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_sub32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("sub.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_equal8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("equal.8         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_equal16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("equal.16       "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_equal32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("equal.32        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_greater8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("greater.8       "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_greater16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("greater.16     "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_greater32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("greater.32      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_less8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("less.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_less16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("less.16			   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_less32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("less.32				"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_cons8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cons.8          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_cons16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cons.16			   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_cons32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cons.32				"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_vector8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("vector.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			default:
				if (tempSize > 0) {
					tempChar = ' ';
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
					e = dl_array_pushElement(&disassembly, &tempChar);
					if (e) return dl_null;
					--tempSize;
					if (!tempSize) {
						tempChar = '\n';
						e = dl_array_pushElement(&disassembly, &tempChar);
						if (e) return dl_null;
						arg = 0;
						continue;
					}
					break;
				}
				--i;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			}
			break;

		case duckLisp_instruction_makeVector8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-vector.8      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_makeVector16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-vector.16     "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_makeVector32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-vector.32     "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_getVecElt8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("get-vector-element.8  "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_getVecElt16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("get-vector-element.16 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_getVecElt32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("get-vector-element.32 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_setVecElt8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-vector-element.8  "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setVecElt16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-vector-element.16 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setVecElt32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-vector-element.32 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 9:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 10:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 11:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 12:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_car8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("car.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_car16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("car.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_car32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("car.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_cdr8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cdr.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_cdr16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cdr.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_cdr32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("cdr.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_setCar8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-car.8       "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setCar16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-car.16      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setCar32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-car.32      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_setCdr8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-cdr.8       "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setCdr16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-cdr.16      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setCdr32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-cdr.32      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_nullp8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("null?.8         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_nullp16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("null?.16       "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_nullp32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("null?.32        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_typeof8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("type-of.8             "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_typeof16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("type-of.16           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_typeof32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("type-of.32            "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_makeType:
			e = dl_array_pushElements(&disassembly, DL_STR("make-type\n"));
			if (e) return dl_null;
			arg = 0;
			continue;

		case duckLisp_instruction_makeInstance8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-instance.8    "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_makeInstance16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-instance.16   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_makeInstance32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-instance.32   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 9:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 10:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 11:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 12:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_compositeValue8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("composite-value.8     "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_compositeValue16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("composite-value.16    "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_compositeValue32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("composite-value.32    "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_compositeFunction8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("composite-function.8  "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_compositeFunction16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("composite-function.16 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_compositeFunction32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("composite-function.32 "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_setCompositeValue8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-composite-value.8       "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setCompositeValue16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-composite-value.16      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setCompositeValue32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-composite-value.32      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_setCompositeFunction8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-composite-function.8    "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setCompositeFunction16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-composite-function.16   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_setCompositeFunction32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("set-composite-function.32   "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_length8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("length.8              "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_length16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("length.16            "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_length32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("length.32             "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_symbolString8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("symbol-string.8       "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_symbolString16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("symbol-string.16     "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_symbolString32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("symbol-string.32      "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_symbolId8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("symbol-id.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_symbolId16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("symbol-id.16         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_symbolId32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("symbol-id.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_makeString8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-string.8         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_makeString16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-string.16       "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_makeString32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("make-string.32        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_concatenate8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("concatenate.8         "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_concatenate16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("concatenate.16        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_concatenate32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("concatenate.32        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_substring8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("substring.8           "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_substring16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("substring.16          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_substring32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("substring.32          "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 5:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 6:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 7:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 8:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = ' ';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;

			case 9:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 10:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 11:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 12:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_nil:
			e = dl_array_pushElements(&disassembly, DL_STR("nil\n"));
			if (e) return dl_null;
			arg = 0;
			continue;

		case duckLisp_instruction_yield:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("yield"));
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_exit:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("exit"));
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		case duckLisp_instruction_return0:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("return.0"));
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_return8:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("return.8        "));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_return16:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("return.16		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;
		case duckLisp_instruction_return32:
			switch (arg) {
			case 0:
				e = dl_array_pushElements(&disassembly, DL_STR("return.32		"));
				if (e) return dl_null;
				break;
			case 1:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 2:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 3:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				break;
			case 4:
				tempSize = bytecode[i];
				tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				tempChar = '\n';
				e = dl_array_pushElement(&disassembly, &tempChar);
				if (e) return dl_null;
				arg = 0;
				continue;
			default:
				e = dl_array_pushElements(&disassembly, DL_STR("Invalid arg number.\n"));
				if (e) return dl_null;
			}
			break;

		default:
			e = dl_array_pushElements(&disassembly, DL_STR("Illegal opcode '"));
			if (e) return dl_null;
			tempChar = dl_nybbleToHexChar((bytecode[i] >> 4) & 0xF);
			e = dl_array_pushElement(&disassembly, &tempChar);
			if (e) return dl_null;
			tempChar = dl_nybbleToHexChar(bytecode[i] & 0xF);
			e = dl_array_pushElement(&disassembly, &tempChar);
			if (e) return dl_null;
			tempChar = '\'';
			e = dl_array_pushElement(&disassembly, &tempChar);
			if (e) return dl_null;
			tempChar = '\n';
			e = dl_array_pushElement(&disassembly, &tempChar);
			if (e) return dl_null;
		}
		arg++;
	}

	/* Push a return. */
	e = dl_array_pushElements(&disassembly, "\0", 1);
	if (e) return dl_null;

	/* No more editing, so shrink array. */
	e = dl_realloc(memoryAllocation, &disassembly.elements, disassembly.elements_length * disassembly.element_size);
	if (e) {
		return dl_null;
	}

	return ((char *) disassembly.elements);
}
