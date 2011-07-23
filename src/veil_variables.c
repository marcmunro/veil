/**
 * @file   veil_variables.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2011 Marc Munro
 *     License:      BSD
 * \endcode
 * @brief  
 * Functions for dealing with Veil variables.
 *
 * Variables may be either session or shared, and are used to retain
 * state between function calls.  Shared variables are available to all
 * suitably privileged sessions within a database.  Session variables
 * hold values that are private to a single session.
 * 
 */

#include "postgres.h"
#include "veil_datatypes.h"
#include "utils/hsearch.h"
#include "storage/shmem.h"

#include "veil_funcs.h"

/**
 * Baselines the number of session variables that can be created in each
 * context.
 */
#define SESSION_HASH_ELEMS    32

/**
 * This identifies the hash table for all session variables.  The shared
 * variable hash tables are managed in veil_shmem.c.
 */

static HTAB *session_hash = NULL;


/** 
 * Create, or attach to, a hash for session variables.
 * 
 */
static HTAB *
create_session_hash()
{
	HASHCTL hashctl;

	/* TODO: Think about creating a specific memory context for this. */

	hashctl.keysize = HASH_KEYLEN;
	hashctl.entrysize = sizeof(VarEntry);

	return hash_create("VEIL_SESSION", 
					   SESSION_HASH_ELEMS, &hashctl, HASH_ELEM);
}

/** 
 * Define a new, or attach to an existing, shared variable.  Raise an
 * ERROR if the variable already exists as a session variable or if we
 * cannot create the variable due to resource limitations (out of
 * memory, or out of space in the shared hash).
 * 
 * @param name The name of the variable.
 * 
 * @return Pointer to the shared variable.  If the variable is newly
 * created by this call then it will be unitialised (ie it will have a
 * NULL obj reference).
 */
VarEntry *
vl_lookup_shared_variable(char *name)
{
	VarEntry *var;
	HTAB     *shared_hash = vl_get_shared_hash();
	bool      found;

	if (!session_hash) {
		session_hash = create_session_hash();
	}

	var = (VarEntry *) hash_search(session_hash, (void *) name,
								   HASH_FIND, &found);
	if (found) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("attempt to redefine session variable %s", name),
				 errdetail("You are trying to create shared variable %s "
						   "but it already exists as a session variable.",
						   name)));
	}

	var = (VarEntry *) hash_search(shared_hash, (void *) name,
								   HASH_ENTER, &found);

	if (!var) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Out of memory for shared variables")));
	}

	if (!found) {
		/* Shared variable did not already exist so we must initialise
		 * it. */

		var->obj = NULL;
		var->shared = true;
	}

	return var;
}

/** 
 * Lookup a variable by name, creating it as as a session variable if it
 * does not already exist.
 * 
 * @param name The name of the variable
 * 
 * @return Pointer to the shared or session variable.  If the variable
 * is newly created by this call then it will be unitialised (ie it will
 * have a NULL obj reference).
 */
VarEntry *
vl_lookup_variable(char *name)
{
	VarEntry *var;
	HTAB     *shared_hash = vl_get_shared_hash();
	bool found;

	if (!session_hash) {
		session_hash = create_session_hash();
	}

	var = (VarEntry *)hash_search(session_hash, (void *) name,
								  HASH_FIND, &found);
	if (!var) {
		/* See whether this is a shared variable. */
		var = (VarEntry *)hash_search(shared_hash, (void *) name,
									  HASH_FIND, NULL);
	}


	if (!var) {
		/* Create new session variable */
		var = (VarEntry *) hash_search(session_hash, (void *) name,
									   HASH_ENTER, &found);
		if (!var) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Out of memory for shared variables")));
		}
		var->obj = NULL;
		var->shared = false;
	}
	return var;
}

/** 
 * Return the next variable from a scan of the hash of variables.  Note
 * that this function is not re-entrant.
 * 
 * @param prev The last variable retrieved by a scan, or NULL if
 * starting a new scan.
 * 
 * @return The next variable encountered in the scan.  NULL if we have
 * finished.
 */
veil_variable_t *
vl_next_variable(veil_variable_t *prev)
{
	static bool doing_shared;
	static HTAB *hash;
	static HASH_SEQ_STATUS status;
	static veil_variable_t result;
	VarEntry *var;

	if (!session_hash) {
		session_hash = create_session_hash();
	}

	if (!prev) {
		doing_shared = true;
		/* Initialise a scan of the shared hash. */
		hash = vl_get_shared_hash();

		hash_seq_init(&status, hash);
	}

	var = hash_seq_search(&status);

	if (!var) {
		/* No more entries from that hash. */
		if (doing_shared) {
			/* Switch to, and get var from, the session hash. */
			doing_shared = false;
			hash = session_hash;
			hash_seq_init(&status, hash);
			var = hash_seq_search(&status);
		}
	}

	if (var) {
		/* Yay, we have an entry. */
		result.name = var->key;
		result.shared = var->shared;
		if (var->obj) {
			result.type = vl_ObjTypeName(var->obj->type);
		}
		else {
			result.type = vl_ObjTypeName(OBJ_UNDEFINED);;
		}
		return &result;
	}
	else {
		/* Thats all.  There are no more entries */
		return NULL;
	}
}

/** 
 * Reset all Int4 entries in an ::Int4Array (to zero).
 * 
 * @param array The array to be reset.
 */
void
vl_ClearInt4Array(Int4Array *array)
{
	int elems = 1 + array->arraymax - array->arrayzero;
	int i;
	for (i = 0; i < elems; i++) {
		array->array[i] = 0;
	}
}

/** 
 * Return a newly initialised (zeroed) ::Int4Array.  It may already
 * exist in which case it will be re-used if possible.  It may
 * be created in either session or shared memory depending on the value
 * of shared.
 * 
 * @param current Pointer to an existing Int4Array if one exists.
 * @param shared Whether to create the variable in shared or session
 * memory.
 * @param min Index of the first entry in the array.
 * @param max Index of the last entry in the array.
 */
Int4Array *
vl_NewInt4Array(Int4Array *current, bool shared,
				int32 min, int32 max)
{
	Int4Array *result = NULL;
	int        elems = 1 + max - min;

    if (current) {
		int cur_elems = 1 + current->arraymax - current->arrayzero;
		if (elems <= cur_elems) {
			vl_ClearInt4Array(current);
			result = current;
		}
		else {
			if (!shared) {
				/* Note that we can't free shared memory - no api to do
				 * so. */
				pfree(current);
			}
		}
	}
	if (!result) {
		if (shared) {
			result = vl_shmalloc(sizeof(Int4Array) + (sizeof(int32) * elems));
		}
		else {
			result = vl_malloc(sizeof(Int4Array) + (sizeof(int32) * elems));
		}
	}
	result->type = OBJ_INT4_ARRAY;
	result->arrayzero = min;
	result->arraymax = max;

	return result;
}

/** 
 * Set an entry within an ::Int4Array.  If idx is outside of the
 * acceptable range, raise an error.
 * 
 * @param array The ::Int4Array within which the entry is to be set. 
 * @param idx The index of the entry to be set.
 * @param value The value to which the entry will be set.
 */
void
vl_Int4ArraySet(Int4Array *array, int32 idx, int32 value)
{
	if ((idx < array->arrayzero) ||
		(idx > array->arraymax))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("Int4ArraySet range error"),
				 errdetail("Index (%d) not in range %d..%d.  ", idx,
						   array->arrayzero, array->arraymax)));
	}
	array->array[idx - array->arrayzero] = value;
}

/** 
 * Get an entry from an ::Int4Array.  If idx is outside of the
 * acceptable range, raise an error.
 * 
 * @param array The ::Int4Array within from the entry is to be read. 
 * @param idx The index of the entry to be retrieved.

 * @return The value of the entry
 */
int32
vl_Int4ArrayGet(Int4Array *array, int32 idx)
{
	if ((idx < array->arrayzero) ||
		(idx > array->arraymax))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Int4ArrayGet range error"),
				 errdetail("Index (%d) not in range %d..%d.  ", idx,
						   array->arrayzero, array->arraymax)));
	}
	return array->array[idx - array->arrayzero];
}
 
