/**
 * @file   veil_utils.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2011 Marc Munro
 *     License:      BSD
 * 
 * \endcode
 * @brief  
 * Miscelaneous functions for veil
 * 
 */


#include "postgres.h"
#include "utils/memutils.h"
#include "veil_funcs.h"
#include "veil_datatypes.h"

/** 
 * Dynamically allocate memory using palloc in TopMemoryContext.
 * 
 * @param size The size of the chunk of memory being requested.
 * 
 * @return Pointer to the newly allocated chunk of memory
 */
void *
vl_malloc(size_t size)
{
	void *result;
    MemoryContext oldcontext = MemoryContextSwitchTo(TopMemoryContext);
	result = palloc(size);
	(void) MemoryContextSwitchTo(oldcontext);
	return result;
}

/** 
 * Return a static string describing an ObjType object.
 * 
 * @param obj The ObjType for which we want a description.
 * 
 * @return Pointer to a static string describing obj.
 */
char *
vl_ObjTypeName(ObjType obj)
{
    static char *names[] = {
		"Undefined", "ShmemCtl", "Int4", 
		"Range", "Bitmap", "BitmapArray", 
		"BitmapHash", "BitmapRef", "Int4Array"
	};

	if ((obj < OBJ_UNDEFINED) ||
		(obj > OBJ_INT4_ARRAY)) 
	{
		return "Unknown";
	}
	else {
		return names[obj];
	}
}

