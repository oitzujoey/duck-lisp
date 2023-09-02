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

dl_error_t duckLisp_emit_nullaryOperator(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         duckLisp_instructionClass_t instructionClass) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = instructionClass;

	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	/**/ duckLisp_localsLength_increment(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_unaryOperator(duckLisp_t *duckLisp,
                                       duckLisp_compileState_t *compileState,
                                       dl_array_t *assembly,
                                       duckLisp_instructionClass_t instructionClass,
                                       duckLisp_instructionArgClass_t argument) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = instructionClass;

	/* Push arguments into instruction. */
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	/**/ duckLisp_localsLength_increment(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_binaryOperator(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_instructionClass_t instructionClass,
                                        duckLisp_instructionArgClass_t argument0,
                                        duckLisp_instructionArgClass_t argument1) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = instructionClass;

	// Push arguments into instruction.
	e = dl_array_pushElement(&instruction.args, &argument0);
	if (e) goto cleanup;

	e = dl_array_pushElement(&instruction.args, &argument1);
	if (e) goto cleanup;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	/**/ duckLisp_localsLength_increment(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_ternaryOperator(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        duckLisp_instructionClass_t instructionClass,
                                        duckLisp_instructionArgClass_t argument0,
                                        duckLisp_instructionArgClass_t argument1,
                                        duckLisp_instructionArgClass_t argument2) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = instructionClass;

	// Push arguments into instruction.
	e = dl_array_pushElement(&instruction.args, &argument0);
	if (e) goto cleanup;

	e = dl_array_pushElement(&instruction.args, &argument1);
	if (e) goto cleanup;

	e = dl_array_pushElement(&instruction.args, &argument2);
	if (e) goto cleanup;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	/**/ duckLisp_localsLength_increment(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_unaryStackOperator(duckLisp_t *duckLisp,
                                            duckLisp_compileState_t *compileState,
                                            dl_array_t *assembly,
                                            duckLisp_instructionClass_t instructionClass,
                                            dl_ptrdiff_t index) {
	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = duckLisp_localsLength_get(compileState) - index;
	return duckLisp_emit_unaryOperator(duckLisp,
	                                   compileState,
	                                   assembly,
	                                   instructionClass,
	                                   argument);
}

dl_error_t duckLisp_emit_binaryStackOperator(duckLisp_t *duckLisp,
                                             duckLisp_compileState_t *compileState,
                                             dl_array_t *assembly,
                                             duckLisp_instructionClass_t instructionClass,
                                             dl_ptrdiff_t index0,
                                             dl_ptrdiff_t index1) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = duckLisp_localsLength_get(compileState) - index0;
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = duckLisp_localsLength_get(compileState) - index1;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    instructionClass,
	                                    argument0,
	                                    argument1);
}

dl_error_t duckLisp_emit_ternaryStackOperator(duckLisp_t *duckLisp,
                                              duckLisp_compileState_t *compileState,
                                              dl_array_t *assembly,
                                              duckLisp_instructionClass_t instructionClass,
                                              dl_ptrdiff_t index0,
                                              dl_ptrdiff_t index1,
                                              dl_ptrdiff_t index2) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	duckLisp_instructionArgClass_t argument2 = {0};
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = duckLisp_localsLength_get(compileState) - index0;
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = duckLisp_localsLength_get(compileState) - index1;
	argument2.type = duckLisp_instructionArgClass_type_index;
	argument2.value.index = duckLisp_localsLength_get(compileState) - index2;
	return duckLisp_emit_ternaryOperator(duckLisp,
	                                     compileState,
	                                     assembly,
	                                     instructionClass,
	                                     argument0,
	                                     argument1,
	                                     argument2);
}


dl_error_t duckLisp_emit_nil(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly) {
	return duckLisp_emit_nullaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_nil);
}

dl_error_t duckLisp_emit_makeString(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_makeString,
	                                        source_index);
}

dl_error_t duckLisp_emit_concatenate(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     const dl_ptrdiff_t vec_index,
                                     const dl_ptrdiff_t index_index) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_concatenate,
	                                         vec_index,
	                                         index_index);
}

dl_error_t duckLisp_emit_substring(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t string_index,
                                   const dl_ptrdiff_t start_index_index,
                                   const dl_ptrdiff_t end_index_index) {
	return duckLisp_emit_ternaryStackOperator(duckLisp,
	                                          compileState,
	                                          assembly,
	                                          duckLisp_instructionClass_substring,
	                                          string_index,
	                                          start_index_index,
	                                          end_index_index);
}

dl_error_t duckLisp_emit_length(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_length,
	                                        source_index);
}

dl_error_t duckLisp_emit_symbolString(duckLisp_t *duckLisp,
                                      duckLisp_compileState_t *compileState,
                                      dl_array_t *assembly,
                                      const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_symbolString,
	                                        source_index);
}

dl_error_t duckLisp_emit_symbolId(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_symbolId,
	                                        source_index);
}

dl_error_t duckLisp_emit_typeof(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_typeof,
	                                        source_index);
}

dl_error_t duckLisp_emit_makeType(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly) {
	return duckLisp_emit_nullaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_makeType);
}

dl_error_t duckLisp_emit_makeInstance(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t type_index,
                                    const dl_ptrdiff_t value_index,
                                    const dl_ptrdiff_t function_index) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	duckLisp_instructionArgClass_t argument2 = {0};
	/* Type */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = duckLisp_localsLength_get(compileState) - type_index;
	/* Value slot */
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = duckLisp_localsLength_get(compileState) - value_index;
	/* Function slot */
	argument2.type = duckLisp_instructionArgClass_type_index;
	argument2.value.index = duckLisp_localsLength_get(compileState) - function_index;
	return duckLisp_emit_ternaryOperator(duckLisp,
	                                     compileState,
	                                     assembly,
	                                     duckLisp_instructionClass_makeInstance,
	                                     argument0,
	                                     argument1,
	                                     argument2);
}

dl_error_t duckLisp_emit_compositeValue(duckLisp_t *duckLisp,
                                        duckLisp_compileState_t *compileState,
                                        dl_array_t *assembly,
                                        const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_compositeValue,
	                                        source_index);
}

dl_error_t duckLisp_emit_compositeFunction(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_compositeFunction,
	                                        source_index);
}

dl_error_t duckLisp_emit_setCompositeValue(duckLisp_t *duckLisp,
                                           duckLisp_compileState_t *compileState,
                                           dl_array_t *assembly,
                                           const dl_ptrdiff_t destination_index,
                                           const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_setCompositeValue,
	                                      destination_index,
	                                      source_index);
	return e;
}

dl_error_t duckLisp_emit_setCompositeFunction(duckLisp_t *duckLisp,
                                              duckLisp_compileState_t *compileState,
                                              dl_array_t *assembly,
                                              const dl_ptrdiff_t destination_index,
                                              const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_setCompositeFunction,
	                                      destination_index,
	                                      source_index);
	// I should probably make this operator return something.
	return e;
}

dl_error_t duckLisp_emit_nullp(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_nullp,
	                                        source_index);
}

dl_error_t duckLisp_emit_setCar(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t destination_index,
                                const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_setCar,
	                                      source_index,
	                                      destination_index);
	return e;
}

dl_error_t duckLisp_emit_setCdr(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t destination_index,
                                const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;
	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_setCdr,
	                                      source_index,
	                                      destination_index);
	return e;
}

dl_error_t duckLisp_emit_car(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_car,
	                                        source_index);
}

dl_error_t duckLisp_emit_cdr(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_cdr,
	                                        source_index);
}

dl_error_t duckLisp_emit_cons(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_cons,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_vector(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t *indexes,
                                const dl_size_t indexes_length) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_vector;

	/* Push arguments into instruction. */
	/* Length */
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = indexes_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto l_cleanup;

	/* Indices */
	DL_DOTIMES(i, indexes_length) {
		argument.type = duckLisp_instructionArgClass_type_index;
		argument.value.index = duckLisp_localsLength_get(compileState) - indexes[i];
		e = dl_array_pushElement(&instruction.args, &argument);
		if (e) goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto l_cleanup;

	/**/ duckLisp_localsLength_increment(compileState);

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_makeVector(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t length_index,
                                    const dl_ptrdiff_t fill_index) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	/* Length */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = duckLisp_localsLength_get(compileState) - length_index;
	/* Fill */
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = duckLisp_localsLength_get(compileState) - fill_index;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    duckLisp_instructionClass_makeVector,
	                                    argument0,
	                                    argument1);
}

dl_error_t duckLisp_emit_getVecElt(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t vec_index,
                                   const dl_ptrdiff_t index_index) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_getVecElt,
	                                         vec_index,
	                                         index_index);
}

dl_error_t duckLisp_emit_setVecElt(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t vec_index,
                                   const dl_ptrdiff_t index_index,
                                   const dl_ptrdiff_t value_index) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	duckLisp_instructionArgClass_t argument2 = {0};
	/* Vector */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = duckLisp_localsLength_get(compileState) - vec_index;
	/* Index */
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = duckLisp_localsLength_get(compileState) - index_index;
	/* Value */
	argument2.type = duckLisp_instructionArgClass_type_index;
	argument2.value.index = duckLisp_localsLength_get(compileState) - value_index;
	return duckLisp_emit_ternaryOperator(duckLisp,
	                                     compileState,
	                                     assembly,
	                                     duckLisp_instructionClass_setVecElt,
	                                     argument0,
	                                     argument1,
	                                     argument2);
}

dl_error_t duckLisp_emit_return(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_size_t count) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_return;

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.index = count;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length -= count;

 cleanup: return e;
}

dl_error_t duckLisp_emit_exit(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_halt;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length -= 1;

 cleanup: return e;
}

dl_error_t duckLisp_emit_pop(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_size_t count) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	if (count == 0) goto l_cleanup;

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pop;

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.index = count;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) {
		goto l_cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto l_cleanup;
	}

	compileState->currentCompileState->locals_length -= count;

 l_cleanup:

	return e;
}

dl_error_t duckLisp_emit_greater(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 const dl_ptrdiff_t source_index1,
                                 const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                              compileState,
	                                              assembly,
	                                              duckLisp_instructionClass_greater,
	                                              source_index1,
	                                              source_index2);
}

dl_error_t duckLisp_emit_equal(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t source_index1,
                               const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                              compileState,
	                                              assembly,
	                                              duckLisp_instructionClass_equal,
	                                              source_index1,
	                                              source_index2);
}

dl_error_t duckLisp_emit_less(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t source_index1,
                              const dl_ptrdiff_t source_index2) {
        return duckLisp_emit_binaryStackOperator(duckLisp,
                                                      compileState,
                                                      assembly,
                                                      duckLisp_instructionClass_less,
                                                      source_index1,
                                                      source_index2);
}

dl_error_t duckLisp_emit_not(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t index) {
	return duckLisp_emit_unaryStackOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_not, index);
}

dl_error_t duckLisp_emit_multiply(duckLisp_t *duckLisp,
                                  duckLisp_compileState_t *compileState,
                                  dl_array_t *assembly,
                                  const dl_ptrdiff_t source_index1,
                                  const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_mul,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_divide(duckLisp_t *duckLisp,
                                duckLisp_compileState_t *compileState,
                                dl_array_t *assembly,
                                const dl_ptrdiff_t source_index1,
                                const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_div,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_add(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index1,
                             const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_add,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_sub(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             const dl_ptrdiff_t source_index1,
                             const dl_ptrdiff_t source_index2) {
	return duckLisp_emit_binaryStackOperator(duckLisp,
	                                         compileState,
	                                         assembly,
	                                         duckLisp_instructionClass_sub,
	                                         source_index1,
	                                         source_index2);
}

dl_error_t duckLisp_emit_nop(duckLisp_t *duckLisp, duckLisp_compileState_t *compileState, dl_array_t *assembly) {
	return duckLisp_emit_nullaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_nop);
	/**/ duckLisp_localsLength_decrement(compileState);
}

dl_error_t duckLisp_emit_setStatic(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t destination_static_index,
                                   const dl_ptrdiff_t source_stack_index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};

	if (destination_static_index == source_stack_index) goto cleanup;

	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = duckLisp_localsLength_get(compileState) - source_stack_index;
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = destination_static_index;
	e = duckLisp_emit_binaryOperator(duckLisp,
	                                 compileState,
	                                 assembly,
	                                 duckLisp_instructionClass_setStatic,
	                                 argument0,
	                                 argument1);
	duckLisp_localsLength_decrement(compileState);
 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushGlobal(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t global_key) {
	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = global_key;
	return duckLisp_emit_unaryOperator(duckLisp,
	                                   compileState,
	                                   assembly,
	                                   duckLisp_instructionClass_pushGlobal,
	                                   argument);
}

dl_error_t duckLisp_emit_move(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              const dl_ptrdiff_t destination_index,
                              const dl_ptrdiff_t source_index) {
	dl_error_t e = dl_error_ok;

	if (destination_index == source_index) goto cleanup;

	e = duckLisp_emit_binaryStackOperator(duckLisp,
	                                      compileState,
	                                      assembly,
	                                      duckLisp_instructionClass_move,
	                                      source_index,
	                                      destination_index);
	if (e) goto cleanup;
	/**/ duckLisp_localsLength_decrement(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushBoolean(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = integer;
	e = duckLisp_emit_unaryOperator(duckLisp,
	                                compileState,
	                                assembly,
	                                duckLisp_instructionClass_pushBoolean,
	                                argument);
	if (e) goto cleanup;

	if (stackIndex != dl_null) *stackIndex = duckLisp_localsLength_get(compileState) - 1;

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushInteger(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_ptrdiff_t integer) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = integer;
	e = duckLisp_emit_unaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_pushInteger, argument);
	if (e) goto cleanup;

	if (stackIndex != dl_null) *stackIndex = duckLisp_localsLength_get(compileState) - 1;

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushDoubleFloat(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         dl_ptrdiff_t *stackIndex,
                                         const double doubleFloat) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_doubleFloat;
	argument.value.doubleFloat = doubleFloat;
	e = duckLisp_emit_unaryOperator(duckLisp,
	                                compileState,
	                                assembly,
	                                duckLisp_instructionClass_pushDoubleFloat,
	                                argument);
	if (e) goto cleanup;

	if (stackIndex != dl_null) *stackIndex = duckLisp_localsLength_get(compileState) - 1;

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushString(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    dl_ptrdiff_t *stackIndex,
                                    char *string,
                                    dl_size_t string_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_pushString;

	/* Write string length. */

	if (string_length > DL_UINT16_MAX) {
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    DL_STR("String longer than DL_UINT_MAX. Truncating string to fit."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		string_length = DL_UINT16_MAX;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	if (string_length) {
		e = dl_malloc(duckLisp->memoryAllocation, (void **) &argument.value.string.value, string_length * sizeof(char));
		if (e) goto cleanup;
		/**/ dl_memcopy_noOverlap(argument.value.string.value, string, string_length);
	}
	else {
		argument.value.string.value = dl_null;
	}

	argument.type = duckLisp_instructionArgClass_type_string;
	argument.value.string.value_length = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	if (stackIndex != dl_null) *stackIndex = duckLisp_localsLength_get(compileState);
	duckLisp_localsLength_increment(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushSymbol(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    dl_ptrdiff_t *stackIndex,
                                    dl_size_t id,
                                    char *string,
                                    dl_size_t string_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_pushSymbol;

	// Write string length.

	if (string_length > DL_UINT16_MAX) {
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    DL_STR("String longer than DL_UINT_MAX. Truncating string to fit."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		string_length = DL_UINT16_MAX;
	}

	// Push arguments into instruction.
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = id;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	e = dl_malloc(duckLisp->memoryAllocation, (void **) &argument.value.string.value, string_length * sizeof(char));
	if (e) goto cleanup;
	/**/ dl_memcopy_noOverlap(argument.value.string.value, string, string_length);

	argument.type = duckLisp_instructionArgClass_type_string;
	argument.value.string.value_length = string_length;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp_localsLength_get(compileState);
	duckLisp_localsLength_increment(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_pushClosure(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     dl_ptrdiff_t *stackIndex,
                                     const dl_bool_t variadic,
                                     const dl_ptrdiff_t function_label_index,
                                     const dl_size_t arity,
                                     const dl_ptrdiff_t *captures,
                                     const dl_size_t captures_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = (variadic
	                                ? duckLisp_instructionClass_pushVaClosure
	                                : duckLisp_instructionClass_pushClosure);

	// Push arguments into instruction.

	// Function label
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = function_label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	// Arity
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = arity;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	// Captures
	argument.type = duckLisp_instructionArgClass_type_integer;
	DL_DOTIMES(i, captures_length) {
		argument.value.integer = (captures[i] >= 0
		                          ? ((dl_ptrdiff_t) duckLisp_localsLength_get(compileState) - captures[i])
		                          : captures[i]);
		e = dl_array_pushElement(&instruction.args, &argument);
		if (e) goto cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto cleanup;
	}

	if (stackIndex != dl_null) *stackIndex = duckLisp_localsLength_get(compileState);
	duckLisp_localsLength_increment(compileState);

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_releaseUpvalues(duckLisp_t *duckLisp,
                                         duckLisp_compileState_t *compileState,
                                         dl_array_t *assembly,
                                         const dl_ptrdiff_t *upvalues,
                                         const dl_size_t upvalues_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	// Write instruction.
	instruction.instructionClass = duckLisp_instructionClass_releaseUpvalues;

	// Push arguments into instruction.

	// Upvalues
	argument.type = duckLisp_instructionArgClass_type_integer;
	{
		dl_size_t num_objects = 0;
		DL_DOTIMES(i, upvalues_length) if (upvalues[i] >= 0) num_objects++;
		if (num_objects == 0) goto cleanup;
	}
	DL_DOTIMES(i, upvalues_length) {
		if (upvalues[i] < 0) continue;
		argument.value.integer = duckLisp_localsLength_get(compileState) - upvalues[i];
		e = dl_array_pushElement(&instruction.args, &argument);
		if (e) goto cleanup;
	}

	// Push instruction into list.
	e = dl_array_pushElement(assembly, &instruction);
	if (e) {
		goto cleanup;
	}

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_ccall(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               dl_ptrdiff_t callback_index) {
	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = callback_index;
	return duckLisp_emit_unaryOperator(duckLisp, compileState, assembly, duckLisp_instructionClass_ccall, argument);
}

dl_error_t duckLisp_emit_pushIndex(duckLisp_t *duckLisp,
                                   duckLisp_compileState_t *compileState,
                                   dl_array_t *assembly,
                                   const dl_ptrdiff_t index) {
	return duckLisp_emit_unaryStackOperator(duckLisp,
	                                        compileState,
	                                        assembly,
	                                        duckLisp_instructionClass_pushIndex,
	                                        index);
}

dl_error_t duckLisp_emit_pushUpvalue(duckLisp_t *duckLisp,
                                     duckLisp_compileState_t *compileState,
                                     dl_array_t *assembly,
                                     const dl_ptrdiff_t index) {
	duckLisp_instructionArgClass_t argument = {0};
	argument.type = duckLisp_instructionArgClass_type_index;
	argument.value.index = index;
	return duckLisp_emit_unaryOperator(duckLisp,
	                                   compileState,
	                                   assembly,
	                                   duckLisp_instructionClass_pushUpvalue,
	                                   argument);
}

dl_error_t duckLisp_emit_setUpvalue(duckLisp_t *duckLisp,
                                    duckLisp_compileState_t *compileState,
                                    dl_array_t *assembly,
                                    const dl_ptrdiff_t upvalueIndex,
                                    const dl_ptrdiff_t index) {
	dl_error_t e = dl_error_ok;

	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	/* Upvalue */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = upvalueIndex;
	/* Object */
	argument1.type = duckLisp_instructionArgClass_type_index;
	argument1.value.index = duckLisp_localsLength_get(compileState) - index;
	e = duckLisp_emit_binaryOperator(duckLisp,
	                                 compileState,
	                                 assembly,
	                                 duckLisp_instructionClass_setUpvalue,
	                                 argument0,
	                                 argument1);
	if (e) goto cleanup;
	duckLisp_localsLength_decrement(compileState);

 cleanup:
	return e;
}

dl_error_t duckLisp_emit_funcall(duckLisp_t *duckLisp,
                                 duckLisp_compileState_t *compileState,
                                 dl_array_t *assembly,
                                 dl_ptrdiff_t index,
                                 dl_uint8_t arity) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	/* Function index. */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = duckLisp_localsLength_get(compileState) - index;
	/* Arity */
	argument1.type = duckLisp_instructionArgClass_type_integer;
	argument1.value.integer = arity;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    duckLisp_instructionClass_funcall,
	                                    argument0,
	                                    argument1);
}

dl_error_t duckLisp_emit_apply(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               dl_ptrdiff_t index,
                               dl_uint8_t arity) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	/* Function index. */
	argument0.type = duckLisp_instructionArgClass_type_index;
	argument0.value.index = duckLisp_localsLength_get(compileState) - index;
	/* Arity */
	argument1.type = duckLisp_instructionArgClass_type_integer;
	argument1.value.integer = arity;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    duckLisp_instructionClass_apply,
	                                    argument0,
	                                    argument1);
}

dl_error_t duckLisp_emit_acall(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               const dl_ptrdiff_t function_index,
                               const dl_size_t count) {
	duckLisp_instructionArgClass_t argument0 = {0};
	duckLisp_instructionArgClass_t argument1 = {0};
	argument1.type = duckLisp_instructionArgClass_type_integer;
	argument1.value.integer = duckLisp_localsLength_get(compileState) - function_index;
	argument0.type = duckLisp_instructionArgClass_type_integer;
	argument0.value.integer = count;
	return duckLisp_emit_binaryOperator(duckLisp,
	                                    compileState,
	                                    assembly,
	                                    duckLisp_instructionClass_acall,
	                                    argument0,
	                                    argument1);
}

/* We do label scoping in the emitters because scope will have no meaning during assembly. */

dl_error_t duckLisp_emit_call(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              const dl_size_t label_length,
                              const dl_size_t count) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_call;

	/* `label_index` should never equal -1 after this function exits. */
	e = duckLisp_scope_getLabelFromName(compileState->currentCompileState, &label_index, label, label_length);
	if (e) goto cleanup;

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Call references undeclared label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = count;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_brz(duckLisp_t *duckLisp,
                             duckLisp_compileState_t *compileState,
                             dl_array_t *assembly,
                             char *label,
                             dl_size_t label_length,
                             int pops) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_brz;

	if (pops < 0) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("brz: Cannot pop a negative number of objects."));
		if (eError) e = eError;
		goto cleanup;
	}

	/* `label_index` should never equal -1 after this function exits. */
	e = duckLisp_scope_getLabelFromName(compileState->currentCompileState, &label_index, label, label_length);
	if (e) goto cleanup;

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("brz references undeclared label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = pops;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length -= pops;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_brnz(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              dl_size_t label_length,
                              int pops) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_brnz;

	if (pops < 0) {
		e = dl_error_invalidValue;
		eError = duckLisp_error_pushRuntime(duckLisp, DL_STR("brnz: Cannot pop a negative number of objects."));
		if (eError) e = eError;
		goto cleanup;
	}

	/* `label_index` should never equal -1 after this function exits. */
	e = duckLisp_scope_getLabelFromName(compileState->currentCompileState, &label_index, label, label_length);
	if (e) goto cleanup;

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("brnz references undeclared label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = pops;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

	compileState->currentCompileState->locals_length -= pops;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_jump(duckLisp_t *duckLisp,
                              duckLisp_compileState_t *compileState,
                              dl_array_t *assembly,
                              char *label,
                              dl_size_t label_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_jump;

	/* `label_index` should never equal -1 after this function exits. */
	e = duckLisp_scope_getLabelFromName(compileState->currentCompileState, &label_index, label, label_length);
	if (e) goto cleanup;

	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Goto references undeclared label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\"."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp, ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}

dl_error_t duckLisp_emit_label(duckLisp_t *duckLisp,
                               duckLisp_compileState_t *compileState,
                               dl_array_t *assembly,
                               char *label,
                               dl_size_t label_length) {
	dl_error_t e = dl_error_ok;
	dl_error_t eError = dl_error_ok;
	dl_array_t eString;
	/**/ dl_array_init(&eString, duckLisp->memoryAllocation, sizeof(char), dl_array_strategy_double);

	duckLisp_scope_t scope;
	dl_ptrdiff_t label_index = -1;
	duckLisp_instructionObject_t instruction = {0};
	duckLisp_instructionArgClass_t argument = {0};
	/**/ dl_array_init(&instruction.args,
	                   duckLisp->memoryAllocation,
	                   sizeof(duckLisp_instructionArgClass_t),
	                   dl_array_strategy_double);

	/* Write instruction. */
	instruction.instructionClass = duckLisp_instructionClass_pseudo_label;

	/* This is why we pushed the scope here. */
	e = duckLisp_scope_getTop(duckLisp, compileState->currentCompileState, &scope);
	if (e) goto cleanup;

	/* Make sure label is declared. */
	/**/ dl_trie_find(scope.labels_trie, &label_index, label, label_length);
	if (e) goto cleanup;
	if (label_index == -1) {
		e = dl_error_invalidValue;
		eError = dl_array_pushElements(&eString, DL_STR("Label \""));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, label, label_length);
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = dl_array_pushElements(&eString, DL_STR("\" is not a top-level expression in a closed scope."));
		if (eError) {
			e = eError;
			goto cleanup;
		}
		eError = duckLisp_error_pushRuntime(duckLisp,
		                                    ((char *) eString.elements),
		                                    eString.elements_length);
		if (eError) e = eError;
		goto cleanup;
	}

	/* Push arguments into instruction. */
	argument.type = duckLisp_instructionArgClass_type_integer;
	argument.value.integer = label_index;
	e = dl_array_pushElement(&instruction.args, &argument);
	if (e) goto cleanup;

	/* Push instruction into list. */
	e = dl_array_pushElement(assembly, &instruction);
	if (e) goto cleanup;

 cleanup:

	eError = dl_array_quit(&eString);
	if (eError) e = eError;

	return e;
}
