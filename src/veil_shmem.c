/**
 * @file   veil_shmem.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2015 Marc Munro
 *     License:      BSD
 *
 * \endcode
 * @brief  
 * Functions for dealing with veil shared memory.
 *
 * This provides dynamic memory allocation, like malloc, from chunks of
 * shared memory allocated from the Postgres shared memory pool.  In
 * order to be able to reset and reload shared memory structures while
 * other backends continue to use the existing structures, a shared
 * memory reset creates a new context, or switches to an existing one
 * that is no longer in use.  No more than two separate contexts will be
 * created.
 *
 * Each context of veil shared memory is associated with a shared hash,
 * which is used to store veil's shared variables.  A specially named
 * variable, VEIL_SHMEMCTL appears only in context0 and contains a
 * reference to chunk0, and the ShmemCtl structure.  From this structure
 * we can identify the current context, the initial chunks for each
 * active context, and whether a context switch is in progress. 
 * 
 * A context switch takes place in 3 steps:
 * -  preparation, in which we determine if a context switch is allowed,
 *    initialise the new context and record the fact that we are in the
 *    process of switching.  All subsequent operations in the current
 *    backend will work in the new context, while other backends will
 *    continue to use the original context
 * -  initialisation of the new context, variables, etc.  This is done
 *    by the user-space function veil_init().
 * -  switchover, when all other processes gain access to the newly
 *    initialised context.  They may continue to use the previous
 *    context for the duration of their current transactions.
 *
 * To access shared variable "x" in a new session, the following steps
 * are taken:
 *  - We access the hash "VEIL_SHARED1_nnn" (where nnn is the oid of our
 *    database).  This gives us a reference to the ShmemCtl structure.
 *    We record hash0 and shared_meminfo on the way.
 *  - We access ShemCtl to identify the current hash and current
 *    context. 
 *  - We look up variable "x" in the current hash, and if we have to
 *    allocate space for it, allocate it from the current context.
 *
 * Note that We use a dynamicall allocated LWLock, VeilLWLock to protect
 * our shared control structures.
 * 
 */

#include "postgres.h"
#include "utils/hsearch.h"
#include "storage/pg_shmem.h"
#include "storage/shmem.h"
#include "storage/lwlock.h"
#include "storage/procarray.h"
#include "access/xact.h"
#include "access/transam.h"
#include "miscadmin.h"
#include "veil_version.h"
#include "veil_shmem.h"
#include "veil_funcs.h"

/**
 * shared_meminfo provides access to the ShmemCtl structure allocated in
 * context 0.
 */
static ShmemCtl *shared_meminfo = NULL;

/**
 * Whether the current backend is in the process of switching contexts.
 * If so, it will be setting up the non-current context in readiness for
 * making it available to all other backends.
 */
static bool      prepared_for_switch = false;

/**
 * The LWLock that Veil will use for managing concurrent access to
 * shared memory.  It is initialised in _PG_init() to a lock id that is
 * distinct from any that will be dynamically allocated.
 */
static LWLockId  VeilLWLock = 0;

/**
 * The LWLock to be used while initially setting up shared memory and 
 * allocating a veil database-specific LWLock.  Initialised in
 * _PG_Init()
 */
static LWLockId  InitialLWLock = 0;

/** 
 * Return the index of the other context from the one supplied.
 * 
 * @param x the context for which we want the other one.
 * 
 * @return the opposite context to that of x.
 */
#define OTHER_CONTEXT(x) 	(x ? 0: 1)

/** 
 * Veil's startup function.  This should be run when the Veil shared
 * library is loaded by postgres.
 * 
 */
void
_PG_init()
{
	int veil_dbs;

	/* See definitions of the following two variables, for comments. */
	fprintf(stderr, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
	VeilLWLock = AddinShmemInitLock;
	InitialLWLock = AddinShmemInitLock;
	
	/* Define GUCs for veil */
	veil_config_init(); 
	veil_dbs = veil_dbs_in_cluster();
	
	/* Request a Veil-specific shared memory context */
	RequestAddinShmemSpace(2 * veil_shmem_context_size() * veil_dbs);

	/* Request a LWLock for later use by all backends */
	RequestAddinLWLocks(veil_dbs);
}

/** 
 * Create/attach to the shared hash identified by hashname.  Return a
 * pointer to an HTAB that references the shared hash.  All locking is
 * handled by the caller.
 * 
 * @param hashname 
 * 
 * @return Pointer to HTAB referencing the shared hash.
 */
static HTAB *
create_shared_hash(const char *hashname)
{
	HASHCTL  hashctl;
	HTAB    *result;
	char    *db_hashname;
	int      hash_elems = veil_shared_hash_elems();

	/* Add the current database oid into the hashname so that it is
	 * distinct from the shared hash for other databases in the
	 * cluster. */
	db_hashname = (char *) vl_malloc(HASH_KEYLEN);
	(void) snprintf(db_hashname, HASH_KEYLEN - 1, "%s_%u", 
					hashname, MyDatabaseId);
	hashctl.keysize = HASH_KEYLEN;
	hashctl.entrysize = sizeof(VarEntry);

	result = ShmemInitHash(db_hashname, hash_elems,
						   hash_elems, &hashctl, HASH_ELEM);
	pfree(db_hashname);
	return result;
}

/** 
 * Return reference to the HTAB for the shared hash associated with
 * context 0.
 * 
 * @return Pointer to HTAB referencing shared hash for context 0.
 */
static HTAB *
get_hash0()
{
	static HTAB *hash0 = NULL;

    if (!hash0) {
		hash0 = create_shared_hash("VEIL_SHARED1");
	}
	return hash0;
}

/** 
 * Return reference to the HTAB for the shared hash associated with
 * context 1.
 * 
 * @return Pointer to HTAB referencing shared hash for context 1.
 */
static HTAB *
get_hash1()
{
	static HTAB *hash1 = NULL;

    if (!hash1) {
		hash1 = create_shared_hash("VEIL_SHARED2");
	}

	return hash1;
}


/** 
 * Allocate or attach to, a new chunk of shared memory for a named
 * memory context.
 * 
 * @param name The name
 * @param size The size of the shared memory chunk to be allocated.
 * @param p_found Pointer to boolean that will identify whether this
 * chunk has already been initialised.
 * 
 * @return Pointer to chunk of shared memory.
 */
static MemContext *
get_shmem_context(char   *name,
				  size_t  size,
				  bool   *p_found)
{
	int         i;
	MemContext *context;
	char       *uniqname  = (char *) vl_malloc(strlen(name) + 16);
	int         max_dbs = veil_dbs_in_cluster();

	for (i = 0; i < max_dbs; i++) {
		(void) sprintf(uniqname, "%s_%d", name, i);
		context = ShmemInitStruct(uniqname, size, p_found);;
		if (!context) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("veil: cannot allocate shared memory(1)")));
		}

		if (*p_found) {
			/* Already exists.  Check database id. */
			if (context->db_id == MyDatabaseId) {
				/* This context is the one for the current database, 
				 * nothing else to do. */
				return context;
			}
		}
		else {
			/* We Just allocated our first context */
			context->db_id = MyDatabaseId;
			context->next = sizeof(MemContext);
			context->limit = size;
			context->lwlock = VeilLWLock;
			return context;
		}
	}

	/* We reach this point if no existing contexts are allocated to our
	 * database.  Now we check those existing contexts to see whether
	 * they are still in use.  If not, we will redeploy them. */

	for (i = 0; i < max_dbs; i++) {
		(void) sprintf(uniqname, "%s_%d", name, i);
		context = ShmemInitStruct(uniqname, size, p_found);;

		if (!context) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("veil: cannot allocate shared memory(2)")));
		}

		if (*p_found) {
			/* Is this context for a still existant database? */
			if (!vl_db_exists(context->db_id)) {
				/* We can re-use this context. */
				context->db_id = MyDatabaseId;
				context->next = sizeof(MemContext);
				context->limit = size;

				*p_found = false;  /* Tell the caller that init is
									* required */
				return context;
			}
		}
		else {
			/* We didn't find an unused context, so now we have created 
			 * a new one. */

			context->db_id = MyDatabaseId;
			context->next = sizeof(MemContext);
			context->limit = size;
			return context;
		}
	}
	ereport(ERROR,
			(errcode(ERRCODE_INTERNAL_ERROR),
			 errmsg("veil: no more shared memory contexts allowed")));
	return NULL;
}

/* Forward ref, required by next function. */
static void shmalloc_init(void);

/** 
 * Return the id (index) of the current context for this session 
 * 
 * @return The current context id
 */
static int
get_cur_context_id()
{
	static bool initialised = false;
	int context;

	if (!initialised) {
		shmalloc_init();
		initialised = true;
	}
		
	context = shared_meminfo->current_context;
	if (prepared_for_switch) {
		context = OTHER_CONTEXT(context);
	}
	else {
		/* Check for the default context being for a later transaction
		 * than current and, if so, use the other one. */
		if (TransactionIdPrecedes(GetCurrentTransactionId(), 
								  shared_meminfo->xid[context]))
		{
			context = OTHER_CONTEXT(context);
		}
	}

    return context;
}

/** 
 * Return pointer to shared memory allocated for the current context.
 * 
 * @return The current context. 
 */
static MemContext *
get_cur_context()
{
    int context;
	context = get_cur_context_id();
    return shared_meminfo->context[context];
}

/** 
 * Dynamically allocate a piece of shared memory from the current
 * context, doing no locking.
 * 
 * @param context The context in which we are operating
 * @param size The size of the requested piece of memory.
 * 
 * @return Pointer to dynamically allocated memory.
 */
static void *
do_vl_shmalloc(MemContext *context,
			   size_t size)
{
	void *result = NULL;
	size_t amount = (size_t) MAXALIGN(size);

	if ((amount + context->next) <= context->limit) {
		result = (void *) ((char *) context + context->next);
		context->next += amount;
	}
	else {
		ereport(ERROR,
				(ERROR,
				 (errcode(ERRCODE_INTERNAL_ERROR),
				  errmsg("veil: out of shared memory"))));
	}
	return result;
}

/** 
 * Dynamically allocate a piece of shared memory from the current context. 
 * 
 * @param size The size of the requested piece of memory.
 * 
 * @return Pointer to dynamically allocated memory.
 */
void *
vl_shmalloc(size_t size)
{
	MemContext *context;
	void       *result;

	context = get_cur_context();

	LWLockAcquire(VeilLWLock, LW_EXCLUSIVE);
	result = do_vl_shmalloc(context, size);
	LWLockRelease(VeilLWLock);

	return result;
}

/** 
 * Free a piece of shared memory within the current context.  Currently
 * this does nothing as implementation of freeing of shared memory has
 * been deferred.
 * 
 * @param mem Pointer to the memory to be freed.
 * 
 */
void
vl_free(void *mem)
{
	return;
}


/** 
 * Attach to, creating and initialising as necessary, the shared memory
 * control structure.  Record this for the session in shared_meminfo.
 */
static void
shmalloc_init(void)
{
	VeilLWLock = AddinShmemInitLock;
	InitialLWLock = AddinShmemInitLock;

	if (!shared_meminfo) {
		VarEntry   *var;
		MemContext *context0;
		MemContext *context1;
		bool        found = false;
		HTAB       *hash0;
		size_t      size;

		size = veil_shmem_context_size();

		LWLockAcquire(InitialLWLock, LW_EXCLUSIVE);
		context0 = get_shmem_context("VEIL_SHMEM0", size, &found);

		if (found && context0->memctl) {
			shared_meminfo = context0->memctl;
			VeilLWLock = shared_meminfo->veil_lwlock;
			/* By aquiring and releasing this lock, we ensure that Veil
			 * shared memory has been fully initialised, by a process
			 * following the else clause of this code path. */
			LWLockAcquire(VeilLWLock, LW_EXCLUSIVE);
			LWLockRelease(InitialLWLock);
			LWLockRelease(VeilLWLock);
		}
		else {
			/* Do minimum amount of initialisation while holding
			 * the initial lock.  We don't want to do anything that
			 * may cause other locks to be aquired as this could lead
			 * to deadlock with other add-ins.  Instead, we aquire the
			 * Veil-specific lock before finishing the initialisation. */

			shared_meminfo = do_vl_shmalloc(context0, sizeof(ShmemCtl));

			if (context0->lwlock != InitialLWLock) {
				/* Re-use the LWLock previously allocated to this memory 
				 * context */
				VeilLWLock = context0->lwlock;
			}
			else {
				/* Allocate new LWLock for this new shared memory
				 * context */
				VeilLWLock = LWLockAssign(); 
			}
			/* Record the lock id in context0 (for possible re-use if
			 * the current database is dropped and a new veil-using
			 * database created), and in the shared_meminfo struct */
			context0->lwlock = VeilLWLock;
			shared_meminfo->veil_lwlock = VeilLWLock;
			
			/* Exchange the initial lock for our Veil-specific one. */
			LWLockAcquire(VeilLWLock, LW_EXCLUSIVE);
			LWLockRelease(InitialLWLock);
	
			/* Now do the rest of the Veil shared memory initialisation */

			/* Set up the other memory context */
			context1 = get_shmem_context("VEIL_SHMEM1", size, &found);
			
			/* Record location of shmemctl structure in each context */
			context0->memctl = shared_meminfo;
			context1->memctl = shared_meminfo;

			/* Finish initialising the shmemctl structure */
			shared_meminfo->type = OBJ_SHMEMCTL;
			shared_meminfo->current_context = 0;
			shared_meminfo->total_allocated[0] = size;
			shared_meminfo->total_allocated[1] = size;
			shared_meminfo->switching = false;
			shared_meminfo->context[0] = context0;
			shared_meminfo->context[1] = context1;
			shared_meminfo->xid[0] = GetCurrentTransactionId();
			shared_meminfo->xid[1] = shared_meminfo->xid[0];
			shared_meminfo->initialised = true;

			/* Set up both shared hashes */
			hash0 = get_hash0();
			(void) get_hash1();

			/* Record the shmemctl structure in hash0 */
			var = (VarEntry *) hash_search(hash0, (void *) "VEIL_SHMEMCTL",
										   HASH_ENTER, &found);

			var->obj = (Object *) shared_meminfo;
			var->shared = true;

			var = (VarEntry *) hash_search(hash0, (void *) "VEIL_SHMEMCTL",
										   HASH_ENTER, &found);

			LWLockRelease(VeilLWLock);
		}
	}
}

/** 
 * Return the shared hash for the current context.
 * 
 * @return Pointer to the HTAB for the current context's shared hash.
 */
HTAB *
vl_get_shared_hash()
{
	int context;
	HTAB *hash;
	static bool initialised = false;

	if (!initialised) {
		(void) get_cur_context();  /* Ensure shared memory is set up. */
		initialised = true;
	}

	context = get_cur_context_id();

	if (context == 0) {
		hash = get_hash0();
	}
	else {
		hash = get_hash1();
	}
	
	return hash;
}

/** 
 * Reset one of the shared hashes.  This is one of the final steps in a
 * context switch.
 * 
 * @return hash The shared hash that is to be reset.
 */
static void
clear_hash(HTAB *hash)
{
	static HASH_SEQ_STATUS status;
	VarEntry *var;

	hash_seq_init(&status, hash);
	while ((var = hash_seq_search(&status))) {
		if (strncmp("VEIL_SHMEMCTL", var->key, strlen("VEIL_SHMEMCTL")) != 0) {
			(void) hash_search(hash, var->key, HASH_REMOVE, NULL);
		}
	}
}

/** 
 * Prepare for a switch to the alternate context.  Switching will
 * only be allowed if there are no transactions that may still be using
 * the context to which we are switching, and there is no other
 * process attempting the switch.
 * 
 * @return true if the switch preparation was successful.
 */
bool
vl_prepare_context_switch()
{
	int   context_curidx;
	int   context_newidx;
	HTAB *hash0 = get_hash0(); /* We must not attempt to create hashes
								* on the fly below as they also acquire
								* the lock */
	HTAB *hash1 = get_hash1(); 
	TransactionId oldest_xid;
	MemContext *context;

	(void) get_cur_context();  /* Ensure shared memory is set up */

	LWLockAcquire(VeilLWLock, LW_EXCLUSIVE);

	if (shared_meminfo->switching) {
		/* Another process is performing the switch */
		LWLockRelease(VeilLWLock);
		return false;
	}

	shared_meminfo->switching = true;

	/* We have claimed the switch.  If we decide that we cannot proceed,
	 * we will return it to its previous state. */

	context_curidx = shared_meminfo->current_context;
	context_newidx = OTHER_CONTEXT(context_curidx);

	/* In case the alternate context has been used before, we must
	 * clear it. */

	oldest_xid = GetOldestXmin(false, true);
	if (TransactionIdPrecedes(oldest_xid, 
							  shared_meminfo->xid[context_curidx])) 
	{
		/* There is a transaction running that precedes the time of
		 * the last context switch.  That transaction may still be
		 * using the chunk to which we wish to switch.  We cannot
		 * allow the switch. */
		shared_meminfo->switching = false;
		LWLockRelease(VeilLWLock);
		return false;
	}
	else {
		/* It looks like we can safely make the switch.  Reset the
		 * new context, and make it the current context for this
		 * session only. */
		context = shared_meminfo->context[context_newidx];
		context->next = sizeof(MemContext);

		/* If we are switching to context 0, reset the next field of
		 * the first chunk to leave space for the ShmemCtl struct. */
		if (context_newidx == 0) {
			context->next += sizeof(ShmemCtl);
			clear_hash(hash0);
		}
		else {
			clear_hash(hash1);
		}
	}

	LWLockRelease(VeilLWLock);
	prepared_for_switch = true;
	return true;
}

/** 
 * Complete the context switch started by vl_prepare_context_switch().
 * Raise an ERROR if the context switch cannot be completed.
 * 
 * @return true if the context switch is successfully completed.
 */
bool
vl_complete_context_switch()
{
	int  context_curidx;
	int  context_newidx;

    if (!prepared_for_switch) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to complete context switch"),
				 errdetail("Not prepared for switch - "
						   "invalid state for operation")));
	}

	LWLockAcquire(VeilLWLock, LW_EXCLUSIVE);
	context_curidx = shared_meminfo->current_context;
	context_newidx = OTHER_CONTEXT(context_curidx);

	if (!shared_meminfo->switching) {
		/* We do not claim to be switching.  We should. */
		LWLockRelease(VeilLWLock);

		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to complete context switch"),
				 errdetail("Session does not have switching set to true- "
						   "invalid state for operation")));
	}

	shared_meminfo->switching = false;
	shared_meminfo->current_context = context_newidx;
	shared_meminfo->xid[context_newidx] = GetCurrentTransactionId();
	LWLockRelease(VeilLWLock);
	prepared_for_switch = false;
	return true;
}

/** 
 * In desparation, if we are unable to complete a context switch, we
 * should use this function.
 */
void
vl_force_context_switch()
{
	int  context_curidx;
	int  context_newidx;
	MemContext *context;
	HTAB *hash0 = get_hash0();
	HTAB *hash1 = get_hash1();

	(void) get_cur_context();

	LWLockAcquire(VeilLWLock, LW_EXCLUSIVE);

	context_curidx = shared_meminfo->current_context;
	context_newidx = OTHER_CONTEXT(context_curidx);

	/* Clear the alternate context. */

	context = shared_meminfo->context[context_newidx];
	context->next = sizeof(MemContext);
	
	/* If we are switching to context 0, reset the next field of
	 * the first chunk to leave space for the ShmemCtl struct. */
	if (context_newidx == 0) {
		context->next += sizeof(ShmemCtl);
		clear_hash(hash0);
	}
	else {
		clear_hash(hash1);
	}
	
	shared_meminfo->switching = false;
	shared_meminfo->current_context = context_newidx;
	shared_meminfo->xid[context_newidx] = GetCurrentTransactionId();
	shared_meminfo->xid[0] = GetCurrentTransactionId();
	LWLockRelease(VeilLWLock);
	prepared_for_switch = false;
}

