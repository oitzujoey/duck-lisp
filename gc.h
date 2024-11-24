/*
MIT License

Copyright (c) 2021, 2022, 2023 Joseph Herguth

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

#ifndef DUCKVM_H
#define DUCKVM_H

#include "DuckLib/core.h"
#include "DuckLib/array.h"

typedef struct duckVM_gclist_s {
	struct duckVM_object_s *objects;
	struct duckVM_object_s **freeObjects;
	dl_bool_t *objectInUse;
	dl_size_t objects_length;
	dl_size_t freeObjects_length;
	dl_array_strategy_t strategy;
	struct duckVM_s *duckVM;
} duckVM_gclist_t;

typedef struct duckVM_s {
	dl_array_t errors;  /* Runtime errors. */
	dl_array_t stack;  /* dl_array_t:duckVM_object_t For data. */
	/* Addressed by symbol ID. */
	dl_array_t globals;  /* duckVM_object_t * */
	dl_array_t globals_map;  /* dl_ptrdiff_t */
	duckVM_gclist_t gclist;
} duckVM_t;


typedef struct duckVM_object_s * duckVM_list_t;

/* Should never appear on the stack */
typedef struct {
	struct duckVM_object_s *car;
	struct duckVM_object_s *cdr;
} duckVM_cons_t;

typedef struct {
	void *data;
	dl_error_t (*destructor)(duckVM_gclist_t *, struct duckVM_object_s *);
	dl_error_t (*marker)(duckVM_gclist_t *, dl_array_t *, struct duckVM_object_s *);
} duckVM_user_t;


typedef enum {
  duckVM_object_type_none,

  /* These types are user visible types. */
  duckVM_object_type_bool,
  duckVM_object_type_integer,
  duckVM_object_type_float,
  /* This always points to a cons or is nil. */
  duckVM_object_type_list,
  /* User-defined type */
  duckVM_object_type_user,

  /* These types should never appear on the stack. */
  duckVM_object_type_cons,
} duckVM_object_type_t;

typedef struct duckVM_object_s {
	union {
		dl_bool_t boolean;
		dl_ptrdiff_t integer;
		double floatingPoint;
		duckVM_list_t list;
		duckVM_cons_t cons;
		duckVM_user_t user;
	} value;
	duckVM_object_type_t type;
	dl_bool_t inUse;
} duckVM_object_t;

typedef dl_error_t (*duckVM_gclist_destructor_t)(duckVM_gclist_t *, duckVM_object_t *);


/* VM management */

/* Initialize the VM. */
dl_error_t duckVM_init(duckVM_t *duckVM, dl_size_t maxObjects);
/* Destroy the VM. This will free up any external resources that the VM is currently using. */
void duckVM_quit(duckVM_t *duckVM);

#endif /* DUCKVM_H */
