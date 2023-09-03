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
#include "DuckLib/sort.h"

dl_error_t duckLisp_instructionObject_quit(duckLisp_t *duckLisp, duckLisp_instructionObject_t *instruction) {
	dl_error_t e = dl_error_ok;

	(void) duckLisp;

	DL_DOTIMES(j, instruction->args.elements_length) {
		duckLisp_instructionArgClass_t arg = DL_ARRAY_GETADDRESS(instruction->args,
		                                                         duckLisp_instructionArgClass_t,
		                                                         j);
		if ((arg.type == duckLisp_instructionArgClass_type_string) && arg.value.string.value) {
			e = DL_FREE(duckLisp->memoryAllocation, &arg.value.string.value);
			if (e) goto cleanup;
		}
	}
	e = dl_array_quit(&instruction->args);
	if (e) goto cleanup;

 cleanup:
	return e;
}

/* This is only to be used after the bytecode has been fully assembled. */
typedef struct {
	/* If this is an array index to a linked list element (the bytecode linked list), incrementing the link address will
	   not necessarily increment this variable. This is important to keep in mind since the list elements are part of an
	   array. This is the *array* index, not the *list* index. */
	dl_ptrdiff_t source;  /* Points to the array (not list) index. */
	dl_ptrdiff_t target;  /* Points to the array (not list) index. */
	dl_uint8_t size;  /* Can hold values 1-4. */
	dl_bool_t forward;  /* True if a forward reference. Otherwise this is a backward reference. */
	dl_bool_t absolute;  /* Indicates an absolute address, which is always 32 bits. i.e. Do not mung. */
} jumpLink_t;

typedef struct {
	jumpLink_t *links;
	dl_size_t links_length;
} linkArray_t;

typedef enum {
	jumpLinkPointers_type_address,
	jumpLinkPointers_type_target,
} jumpLinkPointer_type_t;

typedef struct {
	dl_ptrdiff_t index;
	jumpLinkPointer_type_t type;
} jumpLinkPointer_t;

int jumpLink_less(const void *l, const void *r, const void *context) {
	/* Array of links. */
	const linkArray_t *linkArray = context;
	/* Pointers to links. */
	const jumpLinkPointer_t *left_pointer = l;
	const jumpLinkPointer_t *right_pointer = r;
	/* Addresses we are comparing. */
	/* See those `2 * `s and ` + 1`s? We call that a hack. If we have
	   (label l1) (goto l2) (nop) (goto l1) (label l2)
	   then the source address assigned to (goto l1) is the same as the target address
	   assigned to (label l2). This *should* be fine, but Hoare Quicksort messes with
	   the order when indexing the links. To force an explicit order, we append an
	   extra bit that is set to make the comparison think that labels are larger than
	   the goto which has the same address.
	*/
	const jumpLink_t *links = linkArray->links;
	const dl_ptrdiff_t left = ((left_pointer->type == jumpLinkPointers_type_target)
	                           ? (2 * links[left_pointer->index].target + 1)
	                           : (2 * links[left_pointer->index].source));
	const dl_ptrdiff_t right = ((right_pointer->type == jumpLinkPointers_type_target)
	                            ? (2 * links[right_pointer->index].target + 1)
	                            : (2 * links[right_pointer->index].source));
	return left - right;
}


/* This function has three major parts.
   1. Remove redundant instructions.
   2. Create a mapping of jumps and branches to target labels.
   3. Assemble to a preliminary bytecode. Jumps and branches do not have an address at this point. Jumps are a single
      byte long at this point. The opcode used by jumps is "jump32".
   4. Attempt to shrink the size of the jump and branch instructions. Use "jump16" or "jump8" if possible. Likely the
      very definition of "premature optimization", but I like it.
   5. Insert relative jump and branch targets according to the size of the opcode. */
dl_error_t duckLisp_assemble(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *bytecode,
                             dl_array_t *assembly) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t tempPtrdiff = -1;

	typedef struct {
		dl_ptrdiff_t source;
		dl_bool_t absolute;
	} duckLisp_label_source_t;

	typedef struct {
		dl_ptrdiff_t target;
		dl_array_t sources; /* dl_array_t:duckLisp_label_source_t */
	} duckLisp_label_t;

	typedef struct {
		dl_uint8_t byte;
		dl_ptrdiff_t next;
		dl_ptrdiff_t prev;
	} byteLink_t;

	linkArray_t linkArray = {0};
	dl_array_t bytecodeList;  /* byteLink_t */
	/**/ dl_array_init(&bytecodeList, duckLisp->memoryAllocation, sizeof(byteLink_t), dl_array_strategy_double);
	dl_array_t currentArgs;  /* unsigned char */
	/**/ dl_array_init(&currentArgs, duckLisp->memoryAllocation, sizeof(unsigned char), dl_array_strategy_double);

	byteLink_t tempByteLink;

	dl_array_t labels;  /* duckLisp_label_t */
	/* No error */ dl_array_init(&labels,
	                             duckLisp->memoryAllocation,
	                             sizeof(duckLisp_label_t),
	                             dl_array_strategy_double);

#ifdef USE_DATALOGGING
	duckLisp->datalog.total_instructions_generated += assembly->elements_length;
#endif /* USE_DATALOGGING */

#ifndef NO_OPTIMIZE_PUSHPOPS
	/* Push-pop peephole optimization */
	/* A push followed by an immediate pop is redundant. There is no reason for that sequence to exist. Delete the two
	   instructions. */
	/* One worry here is that in the final bytecode, it is impossible to tell if the second of any two sequential
	   instructions is the target of a branch.

	   add8 1 2 3
	   pop8 1     <-- target of a branch.

	   Fortunately this is not a problem when working with high-level assembly. It actually looks like this:

	   add 1 2 3
	   label 12
	   pop 1

	   So as long as the label is in between the two instructions, this algorithm will work fine. */

	DL_DOTIMES(i, assembly->elements_length) {
		duckLisp_instructionObject_t instruction = DL_ARRAY_GETADDRESS(*assembly, duckLisp_instructionObject_t, i);
		duckLisp_instructionClass_t class = instruction.instructionClass;
		/* The following instruction classes could also be deleted at some point. */
		/* duckLisp_instructionClass_not, */
		/* duckLisp_instructionClass_mul, */
		/* duckLisp_instructionClass_div, */
		/* duckLisp_instructionClass_add, */
		/* duckLisp_instructionClass_sub, */
		/* duckLisp_instructionClass_equal, */
		/* duckLisp_instructionClass_less, */
		/* duckLisp_instructionClass_greater, */
		/* duckLisp_instructionClass_cons, */
		/* duckLisp_instructionClass_vector, */
		/* duckLisp_instructionClass_makeVector, */
		/* duckLisp_instructionClass_getVecElt, */
		/* duckLisp_instructionClass_car, */
		/* duckLisp_instructionClass_cdr, */
		/* duckLisp_instructionClass_nullp, */
		/* duckLisp_instructionClass_typeof, */
		/* duckLisp_instructionClass_makeType, */
		/* duckLisp_instructionClass_makeInstance, */
		/* duckLisp_instructionClass_compositeValue, */
		/* duckLisp_instructionClass_compositeFunction, */
		/* Another optimization would be to run this block multiple times over the entire assembly. */
		if (((dl_size_t) i < assembly->elements_length - 1)
		    && ((class == duckLisp_instructionClass_nil)
		        || (class == duckLisp_instructionClass_makeType)
		        || (class == duckLisp_instructionClass_pushString)
		        || (class == duckLisp_instructionClass_pushBoolean)
		        || (class == duckLisp_instructionClass_pushInteger)
		        || (class == duckLisp_instructionClass_pushDoubleFloat)
		        || (class == duckLisp_instructionClass_pushIndex)
		        || (class == duckLisp_instructionClass_pushSymbol)
		        || (class == duckLisp_instructionClass_pushUpvalue)
		        || (class == duckLisp_instructionClass_pushClosure)
		        || (class == duckLisp_instructionClass_pushVaClosure)
		        || (class == duckLisp_instructionClass_pushGlobal))) {
			duckLisp_instructionObject_t nextInstruction = DL_ARRAY_GETADDRESS(*assembly,
			                                                                   duckLisp_instructionObject_t,
			                                                                   i + 1);
			duckLisp_instructionClass_t nextClass = nextInstruction.instructionClass;
			if (nextClass == duckLisp_instructionClass_pop) {
				/* Preprocessor conditional for debug */
#if 0
				switch (class) {
				case duckLisp_instructionClass_nil:
					puts("NIL");
					break;
				case duckLisp_instructionClass_makeType:
					puts("MAKE TYPE");
					break;
				case duckLisp_instructionClass_pushString:
					puts("PUSH STRING");
					break;
				case duckLisp_instructionClass_pushBoolean:
					puts("PUSH BOOLEAN");
					break;
				case duckLisp_instructionClass_pushInteger:
					puts("PUSH INTEGER");
					break;
				case duckLisp_instructionClass_pushDoubleFloat:
					puts("PUSH INTEGER");
					break;
				case duckLisp_instructionClass_pushIndex:
					puts("PUSH INDEX");
					break;
				case duckLisp_instructionClass_pushSymbol:
					puts("PUSH SYMBOL");
					break;
				case duckLisp_instructionClass_pushUpvalue:
					puts("PUSH UPVALUE");
					break;
				case duckLisp_instructionClass_pushClosure:
					puts("PUSH CLOSURE");
					break;
				case duckLisp_instructionClass_pushVaClosure:
					puts("PUSH VA CLOSURE");
					break;
				case duckLisp_instructionClass_pushGlobal:
					puts("PUSH GLOBAL");
					break;
				default:;
				}
#endif
				instruction.instructionClass = duckLisp_instructionClass_internalNop;
				DL_ARRAY_GETADDRESS(*assembly, duckLisp_instructionObject_t, i) = instruction;
				duckLisp_instructionArgClass_t pops = DL_ARRAY_GETADDRESS(nextInstruction.args,
				                                                          duckLisp_instructionArgClass_t,
				                                                          0);
				--pops.value.integer;
				if (pops.value.integer == 0) {
					e = duckLisp_instructionObject_quit(duckLisp, &instruction);
					if (e) goto cleanup;
					e = duckLisp_instructionObject_quit(duckLisp, &nextInstruction);
					if (e) goto cleanup;
					nextInstruction.instructionClass = duckLisp_instructionClass_internalNop;
					DL_ARRAY_GETADDRESS(*assembly, duckLisp_instructionObject_t, i) = instruction;
					DL_ARRAY_GETADDRESS(*assembly, duckLisp_instructionObject_t, i + 1) = nextInstruction;

#ifdef USE_DATALOGGING
	duckLisp->datalog.pushpop_instructions_removed += 2;
#endif /* USE_DATALOGGING */
				}
				else {
					DL_ARRAY_GETADDRESS(nextInstruction.args, duckLisp_instructionArgClass_t, 0) = pops;
				}
			}
		}
	}
#endif /* NO_OPTIMIZE_PUSHPOPS */

	/* Create label links. */
	/* The links have one pointer to the target instruction, which is always a label instruction.
	   The links have a bunch of other pointers to the branch instructions for that label. These are always jump or
	   branch instructions. */

	DL_DOTIMES(i, compileState->currentCompileState->label_number) {
		duckLisp_label_t label;
		/**/ dl_array_init(&label.sources,
		                   duckLisp->memoryAllocation,
		                   sizeof(duckLisp_label_source_t),
		                   dl_array_strategy_double);
		label.target = -1;
		e = dl_array_pushElement(&labels, &label);
		if (e) goto cleanup;
	}

	/* Assemble high-level assembly to jump target-less bytecode. */

	byteLink_t currentInstruction;
	currentInstruction.prev = -1;
	for (dl_ptrdiff_t j = 0; (dl_size_t) j < assembly->elements_length; j++) {
		duckLisp_instructionObject_t instruction = DL_ARRAY_GETADDRESS(*assembly, duckLisp_instructionObject_t, j);
		/* This is OK because there is no chance of reallocating the args array. */
		duckLisp_instructionArgClass_t *args = dl_null;
		if (instruction.args.elements_length != 0) {
			args = &DL_ARRAY_GETADDRESS(instruction.args, duckLisp_instructionArgClass_t, 0);
		}
		dl_size_t byte_length;

		e = dl_array_clear(&currentArgs);
		if (e) goto cleanup;

		switch (instruction.instructionClass) {
		case duckLisp_instructionClass_internalNop: {
			/* Does not reach bytecode. */
			continue;
		}
		case duckLisp_instructionClass_nop: {
			/* Finish later. We probably don't need it. */
			currentInstruction.byte = duckLisp_instruction_nop;
			break;
		}
		case duckLisp_instructionClass_pushIndex: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_index:
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_pushIndex8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_pushIndex16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushIndex32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) e = eError;
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushBoolean: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_integer:
				currentInstruction.byte = duckLisp_instruction_pushBooleanFalse + (args[0].value.integer != 0);
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) e = eError;
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushInteger: {
			dl_bool_t sign = args[0].value.integer < 0;
			unsigned long long absolute = sign ? -args[0].value.integer : args[0].value.integer;

			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_integer:
				if (absolute < 0x80LU) {
					currentInstruction.byte = duckLisp_instruction_pushInteger8;
					byte_length = 1;
				}
				else if (absolute < 0x8000LU) {
					currentInstruction.byte = duckLisp_instruction_pushInteger16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushInteger32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) e = eError;
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushDoubleFloat: {
			if (args[0].type == duckLisp_instructionArgClass_type_doubleFloat) {
				currentInstruction.byte = duckLisp_instruction_pushDoubleFloat;
				byte_length = 8;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = (((*((dl_uint64_t *) &args[0].value.doubleFloat))
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) e = eError;
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushString: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_integer:
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_pushString8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_pushString16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushString32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			switch (args[1].type) {
			case duckLisp_instructionArgClass_type_string:
				e = dl_array_pushElements(&currentArgs, args[1].value.string.value, args[1].value.string.value_length);
				if (e) goto cleanup;
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushSymbol: {
			dl_bool_t sign;
			unsigned long long absolute[2];
			sign = args[0].value.integer < 0;
			absolute[0] = sign ? -args[0].value.integer : args[0].value.integer;
			sign = args[1].value.integer < 0;
			absolute[1] = sign ? -args[1].value.integer : args[1].value.integer;
			absolute[0] = dl_max(absolute[0], absolute[1]);

			if ((args[0].type == duckLisp_instructionArgClass_type_integer) &&
			    (args[1].type == duckLisp_instructionArgClass_type_integer) &&
			    (args[2].type == duckLisp_instructionArgClass_type_string)) {
				if (absolute[0] < 0x100LU) {
					currentInstruction.byte = duckLisp_instruction_pushSymbol8;
					byte_length = 1;
				}
				else if (absolute[0] < 0x10000LU) {
					currentInstruction.byte = duckLisp_instruction_pushSymbol16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushSymbol32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n + byte_length) = ((args[1].value.integer
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				e = dl_array_pushElements(&currentArgs, args[2].value.string.value, args[2].value.string.value_length);
				if (e) goto cleanup;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class[es]. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushUpvalue: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				/* I'm not checking if it's negative since I'm lazy and that should never happen. */
				if (args[0].value.index < 0x100LL) {
					currentInstruction.byte = duckLisp_instruction_pushUpvalue8;
					byte_length = 1;
				}
				else if (args[0].value.index < 0x10000LL) {
					currentInstruction.byte = duckLisp_instruction_pushUpvalue16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushUpvalue32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class[es]. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_pushGlobal: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_index:
				currentInstruction.byte = duckLisp_instruction_pushGlobal8;
				byte_length = 1;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_setUpvalue: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t offset = 0;
				byte_length = 1;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				offset += byte_length;
				/* I'm not checking if it's negative since I'm lazy and that should never happen. */
				if (args[0].value.index < 0x100LL) {
					currentInstruction.byte = duckLisp_instruction_setUpvalue8;
					byte_length = 1;
				}
				else if (args[0].value.index < 0x10000LL) {
					currentInstruction.byte = duckLisp_instruction_setUpvalue16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_setUpvalue32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, offset + n) = ((args[1].value.integer
					                                                             >> 8*(byte_length - n - 1))
					                                                            & 0xFFU);
				}
				offset += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class[es]. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_setStatic: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				currentInstruction.byte = duckLisp_instruction_setStatic8;
				dl_ptrdiff_t offset = 0;
				byte_length = 1;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				offset += byte_length;
				byte_length = 1;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, offset + n) = ((args[1].value.integer
					                                                             >> 8*(byte_length - n - 1))
					                                                            & 0xFFU);
				}
				offset += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class[es]. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_move: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_move8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_move16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_move32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}

		case duckLisp_instructionClass_pop: {
			if (args[0].type == duckLisp_instructionArgClass_type_integer) {
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_pop8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_pop16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pop32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_not: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_not8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_not16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_not32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_mul: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_mul8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_mul16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_mul32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_div: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_div8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_div16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_div32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_add: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_add8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_add16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_add32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_sub: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_sub8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_sub16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_sub32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_equal: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_equal8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_equal16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_equal32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_greater: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_greater8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_greater16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_greater32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_less: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_less8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_less16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_less32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_cons: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_cons8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_cons16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_cons32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_vector: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_vector8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_vector16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_vector32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(l, instruction.args.elements_length - 1) {
					DL_DOTIMES(n, byte_length) {
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[l + 1].value.index
						                                                            >> 8*(byte_length - n - 1))
						                                                           & 0xFFU);
					}
					index += byte_length;
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_makeVector: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_makeVector8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_makeVector16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_makeVector32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[1].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_getVecElt: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_getVecElt8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_getVecElt16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_getVecElt32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[1].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_setVecElt: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_setVecElt8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_setVecElt16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_setVecElt32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[1].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[2].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_car: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_car8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_car16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_car32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_cdr: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_cdr8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_cdr16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_cdr32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_setCar: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_setCar8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_setCar16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_setCar32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_setCdr: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_setCdr8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_setCdr16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_setCdr32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_nullp: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_nullp8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_nullp16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_nullp32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_typeof: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_typeof8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_typeof16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_typeof32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_makeType: {
			currentInstruction.byte = duckLisp_instruction_makeType;
			break;
		}
		case duckLisp_instructionClass_makeInstance: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)
				    && ((unsigned long) args[2].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_makeInstance8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)
				         && ((unsigned int) args[2].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_makeInstance16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_makeInstance32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[1].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[2].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_compositeValue: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_compositeValue8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_compositeValue16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_compositeValue32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_compositeFunction: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_compositeFunction8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_compositeFunction16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_compositeFunction32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_setCompositeValue: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_setCompositeValue8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_setCompositeValue16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_setCompositeValue32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_setCompositeFunction: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_setCompositeFunction8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_setCompositeFunction16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_setCompositeFunction32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, 2 * byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, byte_length + n) = ((args[1].value.index
					                                                                  >> 8*(byte_length - n - 1))
					                                                                 & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_length: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_length8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_length16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_length32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_symbolString: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_symbolString8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_symbolString16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_symbolString32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_symbolId: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_symbolId8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_symbolId16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_symbolId32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_makeString: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				if ((unsigned long) args[0].value.index < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_makeString8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.index < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_makeString16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_makeString32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_concatenate: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)) {
				dl_ptrdiff_t index = 0;
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_concatenate8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_concatenate16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_concatenate32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[1].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				e = dl_error_invalidValue;
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) e = eError;
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_substring: {
			if ((args[0].type == duckLisp_instructionArgClass_type_index)
			    && (args[1].type == duckLisp_instructionArgClass_type_index)
			    && (args[2].type == duckLisp_instructionArgClass_type_index)) {
				dl_ptrdiff_t index = 0;
				if (((unsigned long) args[0].value.index < 0x100UL)
				    && ((unsigned long) args[1].value.index < 0x100UL)) {
					currentInstruction.byte = duckLisp_instruction_substring8;
					byte_length = 1;
				}
				else if (((unsigned int) args[0].value.index < 0x10000UL)
				         && ((unsigned int) args[1].value.index < 0x10000UL)) {
					currentInstruction.byte = duckLisp_instruction_substring16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_substring32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length * instruction.args.elements_length);
				if (e) goto cleanup;
				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[0].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[1].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				DL_DOTIMES(n, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = ((args[2].value.index
					                                                            >> 8*(byte_length - n - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				e = dl_error_invalidValue;
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) e = eError;
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_nil: {
			currentInstruction.byte = duckLisp_instruction_nil;
			break;
		}
		case duckLisp_instructionClass_releaseUpvalues: {
			DL_DOTIMES(k, instruction.args.elements_length) {
				dl_size_t arg = args[k].value.integer;
				if (arg < 0x00000100UL) {
					currentInstruction.byte = duckLisp_instruction_releaseUpvalues8;
					byte_length = 1;
				}
				else if (arg < 0x00010000UL) {
					currentInstruction.byte = duckLisp_instruction_releaseUpvalues16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_releaseUpvalues32;
					byte_length = 4;
				}
			}
			dl_ptrdiff_t index = 0;
			// Number of upvalues
			e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
			if (e) goto cleanup;
			DL_DOTIMES (l, byte_length) {
				DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = ((instruction.args.elements_length
				                                                            >> 8*(byte_length - l - 1))
				                                                           & 0xFFU);
			}
			index += byte_length;
			// Upvalues
			e = dl_array_pushElements(&currentArgs, dl_null, instruction.args.elements_length * byte_length);
			if (e) {
				goto cleanup;
			}
			for (dl_ptrdiff_t k = 0; (dl_size_t) k < instruction.args.elements_length; k++) {
				DL_DOTIMES(l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = ((args[k].value.integer
					                                                            >> 8*(byte_length - l - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
			}
			break;
		}
		case duckLisp_instructionClass_ccall: {
			switch (args[0].type) {
			case duckLisp_instructionArgClass_type_integer:
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_ccall8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_ccall16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_ccall32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.integer
					                                                    >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			default:
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_funcall: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;

				// Function index
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_funcall8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_funcall16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_funcall32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				DL_DOTIMES (l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = ((args[0].value.index
					                                                            >> 8*(byte_length - l - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				// Arity
				byte_length = 1;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				DL_DOTIMES (l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = ((args[1].value.integer
					                                                            >> 8*(byte_length - l - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_apply: {
			if (args[0].type == duckLisp_instructionArgClass_type_index) {
				dl_ptrdiff_t index = 0;

				// Function index
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_apply8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_apply16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_apply32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				DL_DOTIMES (l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = ((args[0].value.index
					                                                            >> 8*(byte_length - l - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				// Arity
				byte_length = 1;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				DL_DOTIMES (l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = ((args[1].value.integer
					                                                            >> 8*(byte_length - l - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_acall: {
			if (args[0].type == duckLisp_instructionArgClass_type_integer) {
				if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_acall8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_acall16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_acall32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length + 1);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < 1; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n + 1) = args[1].value.integer & 0xFFU;
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
			/* Labels */
		case duckLisp_instructionClass_pseudo_label:
		case duckLisp_instructionClass_pushClosure:
		case duckLisp_instructionClass_pushVaClosure:
			/* Branches */
		case duckLisp_instructionClass_call:
		case duckLisp_instructionClass_jump:
		case duckLisp_instructionClass_brnz: {
			dl_ptrdiff_t index = 0;
			dl_ptrdiff_t label_index = -1;
			duckLisp_label_t label;
			duckLisp_label_source_t labelSource;
			label_index = args[0].value.integer;

			/* This should never fail due to the above initialization. */
			label = DL_ARRAY_GETADDRESS(labels, duckLisp_label_t, label_index);
			/* There should only be one label instruction. The rest should be branches. */
			tempPtrdiff = bytecodeList.elements_length;
			if (instruction.instructionClass == duckLisp_instructionClass_pseudo_label) {
				if (label.target == -1) {
					label.target = tempPtrdiff;
				}
				else {
					e = dl_error_invalidValue;
					eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Redefinition of label."));
					if (eError) e = eError;
					goto cleanup;
				}
			}
			else {
				tempPtrdiff++;	/* `++` for opcode. This is so the optimizer can be used with generic address links. */
				labelSource.source = tempPtrdiff;
				/* Optimize for size. */
				labelSource.absolute = dl_false;
				e = dl_array_pushElement(&label.sources, &labelSource);
				if (e) goto cleanup;
				linkArray.links_length++;
			}
			DL_ARRAY_GETADDRESS(labels, duckLisp_label_t, label_index) = label;

			if (instruction.instructionClass == duckLisp_instructionClass_pseudo_label) {
				continue;
			}

#ifndef NO_OPTIMIZE_JUMPS
			switch (instruction.instructionClass) {
			case duckLisp_instructionClass_pushVaClosure:
				currentInstruction.byte = duckLisp_instruction_pushVaClosure8;
				break;
			case duckLisp_instructionClass_pushClosure:
				currentInstruction.byte = duckLisp_instruction_pushClosure8;
				break;
			case duckLisp_instructionClass_call:
				currentInstruction.byte = duckLisp_instruction_call8;
				break;
			case duckLisp_instructionClass_jump:
				currentInstruction.byte = duckLisp_instruction_jump8;
				break;
			case duckLisp_instructionClass_brnz:
				currentInstruction.byte = duckLisp_instruction_brnz8;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}
#else
			switch (instruction.instructionClass) {
			case duckLisp_instructionClass_pushVaClosure:
				currentInstruction.byte = duckLisp_instruction_pushVaClosure32;
				break;
			case duckLisp_instructionClass_pushClosure:
				currentInstruction.byte = duckLisp_instruction_pushClosure32;
				break;
			case duckLisp_instructionClass_call:
				currentInstruction.byte = duckLisp_instruction_call32;
				break;
			case duckLisp_instructionClass_jump:
				currentInstruction.byte = duckLisp_instruction_jump32;
				break;
			case duckLisp_instructionClass_brnz:
				currentInstruction.byte = duckLisp_instruction_brnz32;
				break;
			default:
				e = dl_error_invalidValue;
				goto cleanup;
			}

			byte_length = 4;
			e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
			if (e) goto cleanup;
			index += byte_length;
#endif

			if ((instruction.instructionClass == duckLisp_instructionClass_brnz)
			    || (instruction.instructionClass == duckLisp_instructionClass_call)) {
				/* br?? also have a pop argument. Insert that. */
				byte_length = 1;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + n) = args[1].value.integer & 0xFFU;
				}
				index += byte_length;
			}
			else if ((instruction.instructionClass == duckLisp_instructionClass_pushClosure)
			         || (instruction.instructionClass == duckLisp_instructionClass_pushVaClosure)) {
				/* Arity */
				byte_length = 1;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				DL_DOTIMES (l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = ((args[1].value.integer
					                                                            >> 8*(byte_length - l - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				/* Number of upvalues */
				byte_length = 4;
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) goto cleanup;
				DL_DOTIMES (l, byte_length) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + l) = (((instruction.args.elements_length - 2)
					                                                            >> 8*(byte_length - l - 1))
					                                                           & 0xFFU);
				}
				index += byte_length;

				/* Upvalues */
				// I'm not going to check args here either.
				byte_length = 4;
				e = dl_array_pushElements(&currentArgs,
				                          dl_null,
				                          (instruction.args.elements_length - 2) * byte_length);
				if (e) goto cleanup;
				for (dl_ptrdiff_t l = 2; (dl_size_t) l < instruction.args.elements_length; l++) {
					DL_DOTIMES(m, byte_length) {
						DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, index + m) = ((args[l].value.integer
						                                                            >> 8*(byte_length - m - 1))
						                                                           & 0xFFU);
					}
					index += byte_length;
				}
			}
			break;
		}
		case duckLisp_instructionClass_return: {
			if (args[0].type == duckLisp_instructionArgClass_type_integer) {
				if (!args[0].value.integer) {
					currentInstruction.byte = duckLisp_instruction_return0;
					byte_length = 0;
				}
				else if ((unsigned long) args[0].value.integer < 0x100UL) {
					currentInstruction.byte = duckLisp_instruction_return8;
					byte_length = 1;
				}
				else if ((unsigned int) args[0].value.integer < 0x10000UL) {
					currentInstruction.byte = duckLisp_instruction_return16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_return32;
					byte_length = 4;
				}
				e = dl_array_pushElements(&currentArgs, dl_null, byte_length);
				if (e) {
					goto cleanup;
				}
				for (dl_ptrdiff_t n = 0; (dl_size_t) n < byte_length; n++) {
					DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, n) = ((args[0].value.index >> 8*(byte_length - n - 1))
					                                                   & 0xFFU);
				}
				break;
			}
			else {
				eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid argument class. Aborting."));
				if (eError) {
					e = eError;
				}
				goto cleanup;
			}
			break;
		}
		case duckLisp_instructionClass_halt: {
			currentInstruction.byte = duckLisp_instruction_halt;
			byte_length = 0;
			break;
		}
		default: {
			e = dl_error_invalidValue;
			eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("Invalid instruction class. Aborting."));
			if (eError) e = eError;
			goto cleanup;
		}
		}

		/* Write instruction to linked list of bytecode. */
		if (bytecodeList.elements_length > 0) {
			DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = bytecodeList.elements_length;
		}
		currentInstruction.prev = bytecodeList.elements_length - 1;
		e = dl_array_pushElement(&bytecodeList, &currentInstruction);
		if (e) {
			if (bytecodeList.elements_length > 0) DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = -1;
			goto cleanup;
		}
		for (dl_ptrdiff_t k = 0; (dl_size_t) k < currentArgs.elements_length; k++) {
			tempByteLink.byte = DL_ARRAY_GETADDRESS(currentArgs, dl_uint8_t, k);
			DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = bytecodeList.elements_length;
			tempByteLink.prev = bytecodeList.elements_length - 1;
			e = dl_array_pushElement(&bytecodeList, &tempByteLink);
			if (e) {
				if (bytecodeList.elements_length > 0) DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = -1;
				goto cleanup;
			}
		}
	}
	if (bytecodeList.elements_length > 0) {
		DL_ARRAY_GETTOPADDRESS(bytecodeList, byteLink_t).next = -1;
	}

	/* Resolve jumps here. */
	/* Each entry points to an entry in the linked list */

	if (linkArray.links_length) {
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &linkArray.links,
		              linkArray.links_length * sizeof(jumpLink_t));
		if (e) goto cleanup;

		{
			dl_ptrdiff_t index = 0;
			for (dl_ptrdiff_t i = 0; (dl_size_t) i < labels.elements_length; i++) {
				duckLisp_label_t label = DL_ARRAY_GETADDRESS(labels, duckLisp_label_t, i);
				for (dl_ptrdiff_t j = 0; (dl_size_t) j < label.sources.elements_length; j++) {
					linkArray.links[index].target = label.target;
					linkArray.links[index].source = DL_ARRAY_GETADDRESS(label.sources,
					                                                    duckLisp_label_source_t,
					                                                    j).source;
					linkArray.links[index].absolute = DL_ARRAY_GETADDRESS(label.sources,
					                                                      duckLisp_label_source_t,
					                                                      j).absolute;
					linkArray.links[index].size = 0;
					linkArray.links[index].forward = label.target > linkArray.links[index].source;
					index++;
				}
				e = dl_array_quit(&label.sources);
				if (e) goto cleanup;
			}
		}
	}
#ifndef NO_OPTIMIZE_JUMPS
	if (linkArray.links_length) {
		/* Address has been set.
		   Target has been set.*/

		/* Create a copy of the original linkArray. This gives us a one-to-one mapping of the new goto addresses to the
		   current goto addresses. */

		linkArray_t newLinkArray;
		newLinkArray.links_length = linkArray.links_length;
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &newLinkArray.links,
		              newLinkArray.links_length * sizeof(jumpLink_t));
		if (e) goto cleanup;

		/**/ dl_memcopy_noOverlap(newLinkArray.links, linkArray.links, linkArray.links_length * sizeof(jumpLink_t));

		/* Create array double the size as jumpLink. */

		jumpLinkPointer_t *jumpLinkPointers = dl_null;
		e = dl_malloc(duckLisp->memoryAllocation,
		              (void **) &jumpLinkPointers,
		              2 * linkArray.links_length * sizeof(jumpLinkPointer_t));
		if (e) goto cleanup;


		/* Fill array with each jumpLink index and index type. */
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) {
			jumpLinkPointers[i].index = i;
			jumpLinkPointers[i].type = jumpLinkPointers_type_address;
		}

		for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) {
			jumpLinkPointers[i + linkArray.links_length].index = i;
			jumpLinkPointers[i + linkArray.links_length].type = jumpLinkPointers_type_target;
		}

		/* I suspect a simple linked list would have been faster than all this junk. */

		/**/ quicksort_hoare(jumpLinkPointers,
		                     2 * linkArray.links_length,
		                     sizeof(jumpLinkPointer_t),
		                     0,
		                     2 * linkArray.links_length - 1,
		                     jumpLink_less,
		                     &linkArray);

		/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < 2 * linkArray.links_length; i++) { */
		/* 	printf("%lld %c		 ", jumpLinkPointers[i].index, jumpLinkPointers[i].type ? 't' : 's'); */
		/* } */
		/* putchar('\n'); */
		/* putchar('\n'); */

		/* Optimize addressing size. */

		dl_ptrdiff_t offset;
		do {
			/* printf("\n"); */
			offset = 0;
			for (dl_ptrdiff_t j = 0; (dl_size_t) j < 2 * newLinkArray.links_length; j++) {
				unsigned char newSize;
				dl_ptrdiff_t difference;
				dl_ptrdiff_t index = jumpLinkPointers[j].index;
				jumpLink_t link = newLinkArray.links[index];

				/* Make sure to check for duplicate links.
				   ^^^ Make sure to ignore that. They are not duplicates.
				   They need to point to the original links so that the originals can be updated.
				   This means I should have created a member that points to the original struct. */

				/*
				  Required structs:
				  goto-label struct. Has a single label and multiple gotos. Possibly superfluous. Done.
				  Original jump link struct. Saved so that the bytecode addresses can be updated. Done.
				  Malleable jump link struct. Scratchpad and final result of calculation. Done.
				  Link pointer struct. Sorted so that malleable links can be updated in order. Done.
				*/

				/* jumpLink[i].address += offset; */

				if (jumpLinkPointers[j].type == jumpLinkPointers_type_target) {
					link.target += offset;
					/* printf("t %lli	index l%lli		 offset %lli\n", link.target, index, offset); */
				}
				else {
					link.source += offset;

					/* Range calculation */
					difference = link.target - (link.source + link.size);

					/* Size calculation */
					if ((DL_INT8_MAX >= difference) && (difference >= DL_INT8_MIN)) {
						newSize = 1;  /* +1 for opcode. */
					}
					else if ((DL_INT16_MAX >= difference) && (difference >= DL_INT16_MIN)) {
						newSize = 2;
					}
					else {
						newSize = 4;
					}
					if (link.absolute) newSize = 4;

					/* printf("t %lli	index j%lli		 offset %lli  difference %lli  size %u	newSize %u\n", */
					/*        link.source, */
					/*        index, */
					/*        offset, */
					/*        difference, */
					/*        link.size, */
					/*        newSize) */;
					if (newSize > link.size) {
						offset += newSize - link.size;
						link.size = newSize;
					}
				}
				newLinkArray.links[index] = link;
			}
		} while (offset != 0);

		e = dl_free(duckLisp->memoryAllocation, (void *) &jumpLinkPointers);
		if (e) goto cleanup;
		/* putchar('\n'); */

		/* printf("old: "); */
		/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) { */
		/* 	printf("%s%lli⇒%lli ; ", */
		/* 	       linkArray.links[i].forward ? "f" : "", */
		/* 	       linkArray.links[i].source, */
		/* 	       linkArray.links[i].target); */
		/* } */
		/* printf("\n"); */

		/* printf("new: "); */
		/* for (dl_ptrdiff_t i = 0; (dl_size_t) i < newLinkArray.links_length; i++) { */
		/* 	printf("%s%lli⇒%lli ; ", */
		/* 	       newLinkArray.links[i].forward ? "f" : "", */
		/* 	       newLinkArray.links[i].source, */
		/* 	       newLinkArray.links[i].target); */
		/* } */
		/* printf("\n\n"); */


		/* Insert addresses into bytecode. */

		for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) {
			/* The bytecode list is a linked list, but there is no problem
			   addressing it as an array since the links were inserted
			   in order. Additional links will be placed on the end of
			   the array and will not affect the indices of the earlier links. */

			// ` - 1` because we want to insert the links *in place of* the target link.
			dl_ptrdiff_t base_address = linkArray.links[i].source - 1;
			dl_bool_t array_end = DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).next == -1;

			if (!newLinkArray.links[i].absolute) {
				if (newLinkArray.links[i].size == 1) {
				}
				else if (newLinkArray.links[i].size == 2) {
					// This is sketch.
					DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).byte += 1;
				}
				else {
					DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, base_address).byte += 2;
				}
			}

			dl_ptrdiff_t previous = base_address;
			for (dl_uint8_t j = 1; j <= newLinkArray.links[i].size; j++) {
				byteLink_t byteLink;
				if (newLinkArray.links[i].absolute) {
					byteLink.byte = (newLinkArray.links[i].target >> 8*(newLinkArray.links[i].size - j)) & 0xFFU;
				}
				else {
					byteLink.byte = (((newLinkArray.links[i].target
					                   - (newLinkArray.links[i].source + newLinkArray.links[i].size))
					                  >> 8*(newLinkArray.links[i].size - j))
					                 & 0xFFU);
				}
				byteLink.prev = previous;
				byteLink.next = linkArray.links[i].source;

				DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, previous).next = bytecodeList.elements_length;
				DL_ARRAY_GETADDRESS(bytecodeList,
				                    byteLink_t,
				                    linkArray.links[i].source).prev = bytecodeList.elements_length;

				e = dl_array_pushElement(&bytecodeList, &byteLink);
				if (e) goto cleanup;

				previous = bytecodeList.elements_length - 1;
			}

#ifdef USE_DATALOGGING
				duckLisp->datalog.jumpsize_bytes_removed += 4 - newLinkArray.links[i].size;
#endif /* USE_DATALOGGING */

			if (array_end) DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, bytecodeList.elements_length - 1).next = -1;
		}

		/* Clean up, clean up, everybody do your share… */
		e = dl_free(duckLisp->memoryAllocation, (void *) &newLinkArray.links);
		if (e) goto cleanup;

		e = dl_free(duckLisp->memoryAllocation, (void *) &linkArray.links);
		if (e) goto cleanup;
	} /* End address space optimization. */
#else
	if (linkArray.links_length) {
		const dl_uint8_t addressSize = 4;
		for (dl_ptrdiff_t i = 0; (dl_size_t) i < linkArray.links_length; i++) {
			const dl_ptrdiff_t source = linkArray.links[i].source;
			const dl_ptrdiff_t address = ((linkArray.links[i].absolute)
			                              ? linkArray.links[i].target
			                              : (linkArray.links[i].target - (source + addressSize)));
			for (dl_uint8_t j = 0; j < addressSize; j++) {
				const dl_uint8_t byte = (address >> 8*(addressSize - j - 1)) & 0xFFU;
				DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, source + j).byte = byte;
			}
		}
	}
#endif

	/* Adjust the opcodes for the address size and set address. */
	/* i.e. rewrite the whole instruction. */

	/* Convert bytecodeList to array. */
	if (bytecodeList.elements_length > 0) {
		tempByteLink.next = 0;
		while (tempByteLink.next != -1) {
			tempByteLink = DL_ARRAY_GETADDRESS(bytecodeList, byteLink_t, tempByteLink.next);
			e = dl_array_pushElement(bytecode, &tempByteLink.byte);
			if (e) goto cleanup;
		}
	}

 cleanup:

#ifdef USE_DATALOGGING
	duckLisp->datalog.total_bytes_generated += bytecode->elements_length;
#endif /* USE_DATALOGGING */

	eError = dl_array_quit(&currentArgs);
	if (eError) e = eError;

	eError = dl_array_quit(&bytecodeList);
	if (eError) e = eError;

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	eError = dl_array_quit(&labels);
	if (eError) e = eError;

	return e;
}
