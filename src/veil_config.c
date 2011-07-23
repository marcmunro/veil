/**
 * @file   veil_config.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2006-2011 Marc Munro
 *     License:      BSD
 *
 * \endcode
 * @brief  
 * Code for setting and reading configuration data.
 * 
 */

#include "postgres.h"
#include "storage/fd.h"
#include "utils/guc.h"
#include "veil_version.h"
#include "veil_funcs.h"


/** 
 * The number of buckets to create in the hash for shared variables.
 * This defaults to 32 and may be defined in postgresql.conf using eg:
 * "veil.shared_hash_elems = 64"
 */
static int shared_hash_elems = 32;

/** 
 * The number of databases within the db cluster that will use veil.
 * Every veil-using database within the cluster will get the same
 * allocation of shared memory.
 */
static int dbs_in_cluster = 2;

/** 
 * The size in KBytes, of each of Veil's shared memory contexts.  Veil
 * will pre-allocate (from Postgres 8.2 onwards) twice this amount of
 * shared memory, one for each context area.
 * The default is 16384 Bytes and may be defined in postgresql.conf
 * using eg: "veil.shmem_context_size = 8192"
 */
static int shmem_context_size = 16384;

/** 
 * Return the number of databases, within the database cluster, that
 * will use Veil.  Each such database will be allocated 2 chunks of
 * shared memory (of shmem_context_size), and a single LWLock.
 * It defaults to 1 and may be defined in postgresql.conf using eg:
 * "veil.dbs_in_cluster = 2"
 */
int
veil_dbs_in_cluster()
{
	veil_load_config();
	return dbs_in_cluster;
}

/** 
 * Return the number of entries that should be allocated for shared
 * variables in our shared hashes.  This defaults to 32 and may be
 * defined in postgresql.conf using eg:
 * "veil.shared_hash_elems = 64"
 */
int
veil_shared_hash_elems()
{
	veil_load_config();
	return shared_hash_elems;
}

/** 
 * Return the amount of shared memory to be requested for each of the
 * two shared memory contexts.  This variable has no effect unless
 * shared_preload_libraries has been defined in postgresql.conf to load
 * the Veil shared library
 * Note that this must be large enough to allocate at least one chunk of
 * memory for each veil-using database in the Postgres cluster.  It
 * defaults to 16K and may be defined in postgresql.conf using eg: 
 * "veil.shmem_context_size = 16384"
 */
int
veil_shmem_context_size()
{
	veil_load_config();
	return shmem_context_size;
}

/** 
 * Retrieve Veil's GUC variables for this session.
 */
void
veil_load_config()
{
	static bool first_time = true;
	if (!first_time) {
		return;
	}

	dbs_in_cluster = atoi(GetConfigOption("veil.dbs_in_cluster", FALSE));
	shared_hash_elems = atoi(GetConfigOption("veil.shared_hash_elems", 
											 FALSE));
	shmem_context_size = atoi(GetConfigOption("veil.shmem_context_size", 
											  FALSE));
	first_time = false;
}


