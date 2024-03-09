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
#include "DuckLib/string.h"

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

#if 0
static dl_error_t jumpLink_prettyPrint(dl_array_t *string_array, jumpLink_t jumpLink) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(jumpLink_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_ptrdiff_t source = "));
	if (e) goto cleanup;
	e = dl_string_fromPtrdiff(string_array, jumpLink.source);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_ptrdiff_t target = "));
	if (e) goto cleanup;
	e = dl_string_fromPtrdiff(string_array, jumpLink.target);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_uint8_t size = "));
	if (e) goto cleanup;
	e = dl_string_fromUint8(string_array, jumpLink.size);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_bool_t forward = "));
	if (e) goto cleanup;
	e = dl_string_fromBool(string_array, jumpLink.forward);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_bool_t absolute = "));
	if (e) goto cleanup;
	e = dl_string_fromBool(string_array, jumpLink.absolute);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
#endif

typedef struct {
	jumpLink_t *links;
	dl_size_t links_length;
} linkArray_t;

#if 0
static dl_error_t linkArray_prettyPrint(dl_array_t *string_array, linkArray_t linkArray) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(linkArray_t) {jumpLink_t links["));
	if (e) goto cleanup;
	e = dl_string_fromSize(string_array, linkArray.links_length);
	if (e) goto cleanup;
	e = dl_array_pushElements(string_array, DL_STR("] = {"));
	if (e) goto cleanup;
	DL_DOTIMES(i, linkArray.links_length) {
		e = jumpLink_prettyPrint(string_array, linkArray.links[i]);
		if (e) goto cleanup;
		if ((dl_size_t) i != linkArray.links_length - 1) {
			e = dl_array_pushElements(string_array, DL_STR(", "));
			if (e) goto cleanup;
		}
	}
	e = dl_array_pushElements(string_array, DL_STR("}}"));
	if (e) goto cleanup;

 cleanup: return e;
}
#endif

typedef enum {
	jumpLinkPointers_type_address,
	jumpLinkPointers_type_target,
} jumpLinkPointer_type_t;

#if 0
static dl_error_t jumpLinkPointer_type_prettyPrint(dl_array_t *string_array,
                                                   jumpLinkPointer_type_t jumpLinkPointer_type) {
	switch (jumpLinkPointer_type) {
	case jumpLinkPointers_type_address:
		return dl_array_pushElements(string_array, DL_STR("jumpLinkPointers_type_address"));
		break;
	case jumpLinkPointers_type_target:
		return dl_array_pushElements(string_array, DL_STR("jumpLinkPointers_type_target"));
		break;
	default:
		return dl_array_pushElements(string_array, DL_STR("INVALID"));
	}
}
#endif

typedef struct {
	dl_ptrdiff_t index;
	jumpLinkPointer_type_t type;
} jumpLinkPointer_t;

#if 0
static dl_error_t jumpLinkPointer_prettyPrint(dl_array_t *string_array, jumpLinkPointer_t jumpLinkPointer) {
	dl_error_t e = dl_error_ok;

	e = dl_array_pushElements(string_array, DL_STR("(jumpLinkPointer_t) {"));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("dl_ptrdiff_t index = "));
	if (e) goto cleanup;
	e = dl_string_fromPtrdiff(string_array, jumpLinkPointer.index);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR(", "));
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("jumpLinkPointer_type_t type = "));
	if (e) goto cleanup;
	e = jumpLinkPointer_type_prettyPrint(string_array, jumpLinkPointer.type);
	if (e) goto cleanup;

	e = dl_array_pushElements(string_array, DL_STR("}"));
	if (e) goto cleanup;

 cleanup: return e;
}
#endif

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
		if ((dl_size_t) i < assembly->elements_length - 1) {
			if ((class == duckLisp_instructionClass_nil)
			    || (class == duckLisp_instructionClass_makeType)
			    || (class == duckLisp_instructionClass_pushString)
			    || (class == duckLisp_instructionClass_pushBoolean)
			    || (class == duckLisp_instructionClass_pushInteger)
			    || (class == duckLisp_instructionClass_pushDoubleFloat)
			    || (class == duckLisp_instructionClass_pushIndex)
			    || (class == duckLisp_instructionClass_pushSymbol)
			    || (class == duckLisp_instructionClass_pushCompressedSymbol)
			    || (class == duckLisp_instructionClass_pushUpvalue)
			    || (class == duckLisp_instructionClass_pushClosure)
			    || (class == duckLisp_instructionClass_pushVaClosure)
			    || (class == duckLisp_instructionClass_pushGlobal)) {
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
					case duckLisp_instructionClass_pushCompressedSymbol:
						puts("PUSH COMPRESSED SYMBOL");
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
			// Seems to make the disassembler go mad.
			else if (class == duckLisp_instructionClass_pop) {
				duckLisp_instructionObject_t nextInstruction = DL_ARRAY_GETADDRESS(*assembly,
				                                                                   duckLisp_instructionObject_t,
				                                                                   i + 1);
				duckLisp_instructionClass_t nextClass = nextInstruction.instructionClass;
				if (nextClass == duckLisp_instructionClass_pop) {
					/* Preprocessor conditional for debug */
#if 0
					puts("POP");
#endif
					int pops = ((DL_ARRAY_GETADDRESS(instruction.args,
					                                 duckLisp_instructionArgClass_t,
					                                 0)
					             .value.integer)
					            + (DL_ARRAY_GETADDRESS(nextInstruction.args,
					                                   duckLisp_instructionArgClass_t,
					                                   0)
					               .value.integer));
					instruction.instructionClass = duckLisp_instructionClass_internalNop;
					DL_ARRAY_GETADDRESS(*assembly, duckLisp_instructionObject_t, i) = instruction;
					DL_ARRAY_GETADDRESS(nextInstruction.args, duckLisp_instructionArgClass_t, 0).value.integer = pops;
					DL_ARRAY_GETADDRESS(*assembly, duckLisp_instructionObject_t, i + 1) = nextInstruction;
#ifdef USE_DATALOGGING
					duckLisp->datalog.pushpop_instructions_removed += 1;
#endif /* USE_DATALOGGING */
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
		case duckLisp_instructionClass_pushCompressedSymbol: {
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
					currentInstruction.byte = duckLisp_instruction_pushCompressedSymbol8;
					byte_length = 1;
				}
				else if (absolute[0] < 0x10000LU) {
					currentInstruction.byte = duckLisp_instruction_pushCompressedSymbol16;
					byte_length = 2;
				}
				else {
					currentInstruction.byte = duckLisp_instruction_pushCompressedSymbol32;
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
			byte_length = 1;
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

		{
			dl_ptrdiff_t offset;
			dl_size_t iteration = 0;
			do {
				/* printf("\n"); */
				offset = 0;
				iteration++;
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

						/* printf("t %zi	index j%zi		 offset %zi  difference %zi  size %u	newSize %u\n", */
						/*        link.source, */
						/*        index, */
						/*        offset, */
						/*        difference, */
						/*        link.size, */
						/*        newSize); */
						if (newSize != link.size) {
							offset += newSize - link.size;
							link.size = newSize;
						}
					}
					newLinkArray.links[index] = link;
				}
				/* I haven't checked if it terminates after I changed `newSize > link.size` to `newSize != link.size`,
				   thus the `iteration < 10`. */
			} while ((offset != 0) && (iteration < 10));
		}

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


dl_error_t duckLisp_disassemble(dl_array_t *string,
                                dl_memoryAllocation_t *memoryAllocation,
                                const dl_uint8_t *bytecode,
                                const dl_size_t length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	const dl_size_t maxElements = 256;
	dl_array_t disassembly;
	/**/ dl_array_init(&disassembly, memoryAllocation, sizeof(char), dl_array_strategy_double);

	const struct {
		const dl_uint8_t opcode;
		dl_uint8_t *format;
		const dl_size_t format_length;
	} templates[] = {
		{duckLisp_instruction_nop, DL_STR("nop")},
		{duckLisp_instruction_pushString8, DL_STR("string.8 1 s0")},
		{duckLisp_instruction_pushString16, DL_STR("string.16 2 s0")},
		{duckLisp_instruction_pushString32, DL_STR("string.32 4 s0")},
		{duckLisp_instruction_pushSymbol8, DL_STR("symbol.8 1 1 s1")},
		{duckLisp_instruction_pushSymbol16, DL_STR("symbol.16 2 2 s1")},
		{duckLisp_instruction_pushSymbol32, DL_STR("symbol.32 4 4 s1")},
		{duckLisp_instruction_pushCompressedSymbol8, DL_STR("compressed-symbol.8 1 1 s1")},
		{duckLisp_instruction_pushCompressedSymbol16, DL_STR("compressed-symbol.16 2 2 s1")},
		{duckLisp_instruction_pushCompressedSymbol32, DL_STR("compressed-symbol.32 4 4 s1")},
		{duckLisp_instruction_pushBooleanFalse, DL_STR("false")},
		{duckLisp_instruction_pushBooleanTrue, DL_STR("true")},
		{duckLisp_instruction_pushInteger8, DL_STR("integer.8 1")},
		{duckLisp_instruction_pushInteger16, DL_STR("integer.16 2")},
		{duckLisp_instruction_pushInteger32, DL_STR("integer.32 4")},
		{duckLisp_instruction_pushDoubleFloat, DL_STR("double-float f")},
		{duckLisp_instruction_pushIndex8, DL_STR("index.8 1")},
		{duckLisp_instruction_pushIndex16, DL_STR("index.16 2")},
		{duckLisp_instruction_pushIndex32, DL_STR("index.32 4")},
		{duckLisp_instruction_pushUpvalue8, DL_STR("upvalue.8 1")},
		{duckLisp_instruction_pushUpvalue16, DL_STR("upvalue.16 2")},
		{duckLisp_instruction_pushUpvalue32, DL_STR("upvalue.32 4")},
		{duckLisp_instruction_pushClosure8, DL_STR("closure.8 1 1 4 V2")},
		{duckLisp_instruction_pushClosure16, DL_STR("closure.16 2 1 4 V2")},
		{duckLisp_instruction_pushClosure32, DL_STR("closure.32 4 1 4 V2")},
		{duckLisp_instruction_pushVaClosure8, DL_STR("variadic-closure.8 1 1 4 V2")},
		{duckLisp_instruction_pushVaClosure16, DL_STR("variadic-closure.16 2 1 4 V2")},
		{duckLisp_instruction_pushVaClosure32, DL_STR("variadic-closure.32 4 1 4 V2")},
		{duckLisp_instruction_pushGlobal8, DL_STR("global.8 1")},
		{duckLisp_instruction_setUpvalue8, DL_STR("set-upvalue.8 1 1")},
		{duckLisp_instruction_setUpvalue16, DL_STR("set-upvalue.16 1 2")},
		{duckLisp_instruction_setUpvalue32, DL_STR("set-upvalue.32 1 4")},
		{duckLisp_instruction_setStatic8, DL_STR("set-global.8 1 1")},
		{duckLisp_instruction_funcall8, DL_STR("funcall.8 1 1")},
		{duckLisp_instruction_funcall16, DL_STR("funcall.16 2 1")},
		{duckLisp_instruction_funcall32, DL_STR("funcall.32 4 1")},
		{duckLisp_instruction_releaseUpvalues8, DL_STR("release-upvalues.8 1 v0")},
		{duckLisp_instruction_releaseUpvalues16, DL_STR("release-upvalues.16 2 v0")},
		{duckLisp_instruction_releaseUpvalues32, DL_STR("release-upvalues.32 4 v0")},
		{duckLisp_instruction_call8, DL_STR("obsolete: call.8 1 1")},
		{duckLisp_instruction_call16, DL_STR("obsolete: call.16 1 2")},
		{duckLisp_instruction_call32, DL_STR("obsolete: call.32 1 4")},
		{duckLisp_instruction_acall8, DL_STR("obsolete: acall.8 1 1")},
		{duckLisp_instruction_acall16, DL_STR("obsolete: acall.16 1 2")},
		{duckLisp_instruction_acall32, DL_STR("obsolete: acall.32 1 4")},
		{duckLisp_instruction_apply8, DL_STR("apply.8 1 1")},
		{duckLisp_instruction_apply16, DL_STR("apply.16 2 1")},
		{duckLisp_instruction_apply32, DL_STR("apply.32 4 1")},
		{duckLisp_instruction_ccall8, DL_STR("c-call.8 1")},
		{duckLisp_instruction_ccall16, DL_STR("c-call.16 2")},
		{duckLisp_instruction_ccall32, DL_STR("c-call.32 4")},
		{duckLisp_instruction_jump8, DL_STR("jump.8 1")},
		{duckLisp_instruction_jump16, DL_STR("jump.16 2")},
		{duckLisp_instruction_jump32, DL_STR("jump.32 4")},
		{duckLisp_instruction_brz8, DL_STR("brz.8 1 1")},
		{duckLisp_instruction_brz16, DL_STR("brz.16 2 1")},
		{duckLisp_instruction_brz32, DL_STR("brz.32 4 1")},
		{duckLisp_instruction_brnz8, DL_STR("brnz.8 1 1")},
		{duckLisp_instruction_brnz16, DL_STR("brnz.16 2 1")},
		{duckLisp_instruction_brnz32, DL_STR("brnz.32 4 1")},
		{duckLisp_instruction_move8, DL_STR("move.8 1 1")},
		{duckLisp_instruction_move16, DL_STR("move.16 2 2")},
		{duckLisp_instruction_move32, DL_STR("move.32 4 4")},
		{duckLisp_instruction_not8, DL_STR("not.8 1")},
		{duckLisp_instruction_not16, DL_STR("not.16 2")},
		{duckLisp_instruction_not32, DL_STR("not.32 4")},
		{duckLisp_instruction_mul8, DL_STR("mul.8 1 1")},
		{duckLisp_instruction_mul16, DL_STR("mul.16 2 2")},
		{duckLisp_instruction_mul32, DL_STR("mul.32 4 4")},
		{duckLisp_instruction_div8, DL_STR("div.8 1 1")},
		{duckLisp_instruction_div16, DL_STR("div.16 2 2")},
		{duckLisp_instruction_div32, DL_STR("div.32 4 4")},
		{duckLisp_instruction_add8, DL_STR("add.8 1 1")},
		{duckLisp_instruction_add16, DL_STR("add.16 2 2")},
		{duckLisp_instruction_add32, DL_STR("add.32 4 4")},
		{duckLisp_instruction_sub8, DL_STR("sub.8 1 1")},
		{duckLisp_instruction_sub16, DL_STR("sub.16 2 2")},
		{duckLisp_instruction_sub32, DL_STR("sub.32 4 4")},
		{duckLisp_instruction_equal8, DL_STR("equal.8 1 1")},
		{duckLisp_instruction_equal16, DL_STR("equal.16 2 2")},
		{duckLisp_instruction_equal32, DL_STR("equal.32 4 4")},
		{duckLisp_instruction_greater8, DL_STR("greater.8 1 1")},
		{duckLisp_instruction_greater16, DL_STR("greater.16 2 2")},
		{duckLisp_instruction_greater32, DL_STR("greater.32 4 4")},
		{duckLisp_instruction_less8, DL_STR("less.8 1 1")},
		{duckLisp_instruction_less16, DL_STR("less.16 2 2")},
		{duckLisp_instruction_less32, DL_STR("less.32 4 4")},
		{duckLisp_instruction_cons8, DL_STR("cons.8 1 1")},
		{duckLisp_instruction_cons16, DL_STR("cons.16 2 2")},
		{duckLisp_instruction_cons32, DL_STR("cons.32 4 4")},
		{duckLisp_instruction_vector8, DL_STR("vector.8 1 V0")},
		{duckLisp_instruction_vector16, DL_STR("vector.16 2 V0")},
		{duckLisp_instruction_vector32, DL_STR("vector.32 4 V0")},
		{duckLisp_instruction_makeVector8, DL_STR("makeVector.8 1 1")},
		{duckLisp_instruction_makeVector16, DL_STR("makeVector.16 2 2")},
		{duckLisp_instruction_makeVector32, DL_STR("makeVector.32 4 4")},
		{duckLisp_instruction_getVecElt8, DL_STR("getVecElt.8 1 1")},
		{duckLisp_instruction_getVecElt16, DL_STR("getVecElt.16 2 2")},
		{duckLisp_instruction_getVecElt32, DL_STR("getVecElt.32 4 4")},
		{duckLisp_instruction_setVecElt8, DL_STR("setVecElt.8 1 1 1")},
		{duckLisp_instruction_setVecElt16, DL_STR("setVecElt.16 2 2 2")},
		{duckLisp_instruction_setVecElt32, DL_STR("setVecElt.32 4 4 4")},
		{duckLisp_instruction_car8, DL_STR("car.8 1")},
		{duckLisp_instruction_car16, DL_STR("car.16 2")},
		{duckLisp_instruction_car32, DL_STR("car.32 4")},
		{duckLisp_instruction_cdr8, DL_STR("cdr.8 1")},
		{duckLisp_instruction_cdr16, DL_STR("cdr.16 2")},
		{duckLisp_instruction_cdr32, DL_STR("cdr.32 4")},
		{duckLisp_instruction_setCar8, DL_STR("setCar.8 1 1")},
		{duckLisp_instruction_setCar16, DL_STR("setCar.16 2 2")},
		{duckLisp_instruction_setCar32, DL_STR("setCar.32 4 4")},
		{duckLisp_instruction_setCdr8, DL_STR("setCdr.8 1 1")},
		{duckLisp_instruction_setCdr16, DL_STR("setCdr.16 2 2")},
		{duckLisp_instruction_setCdr32, DL_STR("setCdr.32 4 4")},
		{duckLisp_instruction_nullp8, DL_STR("null?.8 1")},
		{duckLisp_instruction_nullp16, DL_STR("null?.16 2")},
		{duckLisp_instruction_nullp32, DL_STR("null?.32 4")},
		{duckLisp_instruction_typeof8, DL_STR("typeof.8 1")},
		{duckLisp_instruction_typeof16, DL_STR("typeof.16 2")},
		{duckLisp_instruction_typeof32, DL_STR("typeof.32 4")},
		{duckLisp_instruction_makeType, DL_STR("makeType")},
		{duckLisp_instruction_makeInstance8, DL_STR("makeInstance.8 1 1 1")},
		{duckLisp_instruction_makeInstance16, DL_STR("makeInstance.16 2 2 2")},
		{duckLisp_instruction_makeInstance32, DL_STR("makeInstance.32 4 4 4")},
		{duckLisp_instruction_compositeValue8, DL_STR("compositeValue.8 1")},
		{duckLisp_instruction_compositeValue16, DL_STR("compositeValue.16 2")},
		{duckLisp_instruction_compositeValue32, DL_STR("compositeValue.32 4")},
		{duckLisp_instruction_compositeFunction8, DL_STR("compositeFunction.8 1")},
		{duckLisp_instruction_compositeFunction16, DL_STR("compositeFunction.16 2")},
		{duckLisp_instruction_compositeFunction32, DL_STR("compositeFunction.32 4")},
		{duckLisp_instruction_setCompositeValue8, DL_STR("setCompositeValue.8 1 1")},
		{duckLisp_instruction_setCompositeValue16, DL_STR("setCompositeValue.16 2 2")},
		{duckLisp_instruction_setCompositeValue32, DL_STR("setCompositeValue.32 4 4")},
		{duckLisp_instruction_setCompositeFunction8, DL_STR("setCompositeFunction.8 1 1")},
		{duckLisp_instruction_setCompositeFunction16, DL_STR("setCompositeFunction.16 2 2")},
		{duckLisp_instruction_setCompositeFunction32, DL_STR("setCompositeFunction.32 4 4")},
		{duckLisp_instruction_makeString8, DL_STR("makeString.8 1")},
		{duckLisp_instruction_makeString16, DL_STR("makeString.16 2")},
		{duckLisp_instruction_makeString32, DL_STR("makeString.32 4")},
		{duckLisp_instruction_concatenate8, DL_STR("concatenate.8 1 1")},
		{duckLisp_instruction_concatenate16, DL_STR("concatenate.16 2 2")},
		{duckLisp_instruction_concatenate32, DL_STR("concatenate.32 4 4")},
		{duckLisp_instruction_substring8, DL_STR("substring.8 1 1 1")},
		{duckLisp_instruction_substring16, DL_STR("substring.16 2 2 2")},
		{duckLisp_instruction_substring32, DL_STR("substring.32 4 4 4")},
		{duckLisp_instruction_length8, DL_STR("length.8 1")},
		{duckLisp_instruction_length16, DL_STR("length.16 2")},
		{duckLisp_instruction_length32, DL_STR("length.32 4")},
		{duckLisp_instruction_symbolString8, DL_STR("symbolString.8 1")},
		{duckLisp_instruction_symbolString16, DL_STR("symbolString.16 2")},
		{duckLisp_instruction_symbolString32, DL_STR("symbolString.32 4")},
		{duckLisp_instruction_symbolId8, DL_STR("symbolId.8 1")},
		{duckLisp_instruction_symbolId16, DL_STR("symbolId.16 2")},
		{duckLisp_instruction_symbolId32, DL_STR("symbolId.32 4")},
		{duckLisp_instruction_pop8, DL_STR("pop.8 1")},
		{duckLisp_instruction_pop16, DL_STR("pop.16 2")},
		{duckLisp_instruction_pop32, DL_STR("pop.32 4")},
		{duckLisp_instruction_return0, DL_STR("return.0")},
		{duckLisp_instruction_return8, DL_STR("return.8 1")},
		{duckLisp_instruction_return16, DL_STR("return.16 2")},
		{duckLisp_instruction_return32, DL_STR("return.32 4")},
		{duckLisp_instruction_halt, DL_STR("halt")},
		{duckLisp_instruction_nil, DL_STR("nil")},
	};
	dl_ptrdiff_t *template_array = dl_null;
	e = DL_MALLOC(memoryAllocation, &template_array, maxElements, dl_ptrdiff_t);
	if (e) goto cleanup;

	DL_DOTIMES(i, maxElements) {
		template_array[i] = -1;
	}

	DL_DOTIMES(i, sizeof(templates)/sizeof(*templates)) {
		template_array[templates[i].opcode] = i;
	}

	e = dl_array_pushElements(&disassembly, DL_STR("DISASSEMBLY START\n"));
	if (e) goto cleanup;

	DL_DOTIMES(bytecode_index, length) {
		dl_ptrdiff_t template_index = template_array[bytecode[bytecode_index]];
		if (template_index >= 0) {
			dl_uint8_t *format = templates[template_index].format;
			dl_size_t format_length = templates[template_index].format_length;

			/* Name */
			DL_DOTIMES(k, format_length) {
				char formatChar = format[k];
				if (dl_string_isSpace(formatChar)) {
					format = &format[k];
					format_length -= k;
					break;
				}
				e = dl_array_pushElement(&disassembly, &formatChar);
				if (e) goto cleanup;
				if ((dl_size_t) k == format_length - 1) {
					format = &format[k];
					format_length -= k;
					break;
				}
			}
			format++;
			--format_length;

			/* Args */
			dl_size_t args[5];  /* Didn't count. Just guessed. */
			int args_size[5];
			int args_index = 0;
			while (format_length > 0) {
				char formatChar = *format;
				switch (formatChar) {
				case '1': {
					e = dl_array_pushElement(&disassembly, " ");
					if (e) goto cleanup;
					bytecode_index++;
					dl_uint8_t code = bytecode[bytecode_index];
					char hexChar = dl_nybbleToHexChar((code >> 4) & 0x0F);
					e = dl_array_pushElement(&disassembly, &hexChar);
					if (e) goto cleanup;
					hexChar = dl_nybbleToHexChar(code & 0x0F);
					e = dl_array_pushElement(&disassembly, &hexChar);
					if (e) goto cleanup;
					args[args_index] = code;
					args_size[args_index] = 1;
					args_index++;
					format++;
					--format_length;
					break;
				}
				case '2': {
					e = dl_array_pushElement(&disassembly, " ");
					if (e) goto cleanup;
					args[args_index] = 0;
					DL_DOTIMES(m, 2) {
						bytecode_index++;
						dl_uint8_t code = bytecode[bytecode_index];
						char hexChar = dl_nybbleToHexChar((code >> 4) & 0x0F);
						e = dl_array_pushElement(&disassembly, &hexChar);
						if (e) goto cleanup;
						hexChar = dl_nybbleToHexChar(code & 0x0F);
						e = dl_array_pushElement(&disassembly, &hexChar);
						if (e) goto cleanup;
						args[args_index] <<= 8;
						args[args_index] |= code;
					}
					args_size[args_index] = 2;
					args_index++;
					format++;
					--format_length;
					break;
				}
				case '4': {
					e = dl_array_pushElement(&disassembly, " ");
					if (e) goto cleanup;
					args[args_index] = 0;
					DL_DOTIMES(m, 4) {
						bytecode_index++;
						dl_uint8_t code = bytecode[bytecode_index];
						char hexChar = dl_nybbleToHexChar((code >> 4) & 0x0F);
						e = dl_array_pushElement(&disassembly, &hexChar);
						if (e) goto cleanup;
						hexChar = dl_nybbleToHexChar(code & 0x0F);
						e = dl_array_pushElement(&disassembly, &hexChar);
						if (e) goto cleanup;
						args[args_index] <<= 8;
						args[args_index] |= code;
					}
					args_size[args_index] = 4;
					args_index++;
					format++;
					--format_length;
					break;
				}
				case 'v': {
					format++;
					--format_length;
					char index = *format - '0';
					const dl_size_t length = args[(dl_uint8_t) index];
					const int size = args_size[(dl_uint8_t) index];
					format++;
					--format_length;
					DL_DOTIMES(m, length) {
						e = dl_array_pushElement(&disassembly, " ");
						if (e) goto cleanup;
						DL_DOTIMES(n, size) {
							bytecode_index++;
							char hexChar = dl_nybbleToHexChar((bytecode[bytecode_index] >> 4) & 0x0F);
							e = dl_array_pushElement(&disassembly, &hexChar);
							if (e) goto cleanup;
							hexChar = dl_nybbleToHexChar(bytecode[bytecode_index] & 0x0F);
							e = dl_array_pushElement(&disassembly, &hexChar);
							if (e) goto cleanup;
						}
					}
					break;
				}
				case 'V': {
					format++;
					--format_length;
					char index = *format - '0';
					dl_size_t length = args[(dl_uint8_t) index];
					format++;
					--format_length;
					DL_DOTIMES(m, length) {
						e = dl_array_pushElement(&disassembly, " ");
						if (e) goto cleanup;
						DL_DOTIMES(n, 4) {
							bytecode_index++;
							char hexChar = dl_nybbleToHexChar((bytecode[bytecode_index] >> 4) & 0x0F);
							e = dl_array_pushElement(&disassembly, &hexChar);
							if (e) goto cleanup;
							hexChar = dl_nybbleToHexChar(bytecode[bytecode_index] & 0x0F);
							e = dl_array_pushElement(&disassembly, &hexChar);
							if (e) goto cleanup;
						}
					}
					break;
				}
				case 's': {
					e = dl_array_pushElement(&disassembly, " ");
					if (e) goto cleanup;
					format++;
					--format_length;
					char index = *format - '0';
					dl_size_t length = args[(dl_uint8_t) index];
					format++;
					--format_length;
					e = dl_array_pushElement(&disassembly, "\"");
					if (e) goto cleanup;
					DL_DOTIMES(m, length) {
						bytecode_index++;
						char stringChar = bytecode[bytecode_index];
						if (dl_string_isSpace(stringChar) && (stringChar != '\n') && (stringChar != '\r')) {
							stringChar = ' ';
						}
						e = dl_array_pushElement(&disassembly, &stringChar);
						if (e) goto cleanup;
					}
					e = dl_array_pushElement(&disassembly, "\"");
					if (e) goto cleanup;
					break;
				}
				case 'f': {
					format++;
					--format_length;
					bytecode_index += 8;
					/* Do float here. */
					break;
				}
				case ' ': {
					format++;
					--format_length;
					break;
				}
				default:
					e = dl_error_invalidValue;
					goto cleanup;
				}
			}

			e = dl_array_pushElement(&disassembly, "\n");
			if (e) goto cleanup;
		}
		else {
			e = dl_array_pushElements(&disassembly, DL_STR("Illegal opcode '"));
			if (e) goto cleanup;
			char hexChar = dl_nybbleToHexChar((bytecode[bytecode_index] >> 4) & 0xF);
			e = dl_array_pushElement(&disassembly, &hexChar);
			if (e) goto cleanup;
			hexChar = dl_nybbleToHexChar(bytecode[bytecode_index] & 0xF);
			e = dl_array_pushElement(&disassembly, &hexChar);
			if (e) goto cleanup;
			e = dl_array_pushElement(&disassembly, "'");
			if (e) goto cleanup;
			e = dl_array_pushElement(&disassembly, "\n");
			if (e) goto cleanup;
		}
	}

	e = dl_array_pushElements(&disassembly, DL_STR("DISASSEMBLY END\n"));
	if (e) goto cleanup;

	/* Push a return. */
	e = dl_array_pushElements(&disassembly, "\0", 1);
	if (e) goto cleanup;

	/* No more editing, so shrink array. */
	e = dl_realloc(memoryAllocation, &disassembly.elements, disassembly.elements_length * disassembly.element_size);
	if (e) goto cleanup;

	*string = disassembly;

 cleanup:
	eError = DL_FREE(memoryAllocation, &template_array);
	if (eError) e = eError;
	return e;
}
