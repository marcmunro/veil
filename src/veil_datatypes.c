/**
 * @file   veil_datatypes.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2011 Marc Munro
 *     License:      BSD
 * 
 * \endcode
 * @brief  
 * Code for non-bitmap datatypes.
 * 
 */

#include "postgres.h"
#include "veil_datatypes.h"
#include "veil_funcs.h"


/** 
 * Create a new session or shared ::Range object.
 * 
 * @param shared Whether the object is to be created in shared (true) or
 * session (false) memory.
 * 
 * @return Pointer to newly created object.
 */
Range *
vl_NewRange(bool shared)
{
    Range *range;

	if (shared) {
		range = vl_shmalloc(sizeof(Range));
	}
	else {
		range = vl_malloc(sizeof(Range));
	}

	range->type = OBJ_RANGE;
	return range;
}

/** 
 * Create a new session or shared ::Int4Var object.
 * 
 * @param shared Whether the object is to be created in shared (true) or
 * session (false) memory.
 * 
 * @return Pointer to newly created object.
 */
Int4Var *
vl_NewInt4(bool shared)
{
	Int4Var *i4v;

	if (shared) {
		i4v = vl_shmalloc(sizeof(Int4Var));
	}
	else {
		i4v = vl_malloc(sizeof(Int4Var));
	}
	i4v->type = OBJ_INT4;
	i4v->isnull = true;
	return i4v;
}

