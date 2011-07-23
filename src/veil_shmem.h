/**
 * @file   veil_shmem.h
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2011 Marc Munro
 *     License:      BSD
 * 
 * \endcode
 * @brief  
 * Define the basic veil shared memory structures
 * 
 */

#ifndef VEIL_DATATYPES
/** Prevent multiple definitions of the contents of this file.
 */
#define VEIL_DATATYPES 1

#include "utils/hsearch.h"
#include "storage/lwlock.h"

/**
 * Chunks od shared memory are allocated in multiples of this size.
 */
#define CHUNK_SIZE 8192

/**
 * Limits the total amount of memory available for veil shared
 * variables.
 */
#define MAX_ALLOWED_SHMEM CHUNK_SIZE * 100


/** 
 * Chunks provide a linked list of dynamically allocated shared memory
 * segments, with the most recently allocated chunk at the tail.
 * Shmalloc allocates space from this list of chunks, creating new
 * chunks as needed	up to MAX_ALLOWED_SHMEM.
 */
typedef struct MemChunk {
    struct MemChunk *next_chunk;  /**< Pointer to next allocated chunk */
	size_t  next;                 /**< Offset, within this chunk, of 1st
								   * free byte */
	size_t  limit;                /**< Offset, of 1st byte beyond chunk */
    void   *memory[0];            /**< The rest of the chunk, from which
								   * memory is allocated */
} MemChunk;


/** 
 * MemContexts are large single chunks of shared memory from which 
 * smaller allocations may be made
 */
typedef struct MemContext {
	Oid       db_id;              /**< Identifier for the database for
								   * which this context was created,
								   * or by which it has been taken
								   * over. */
	LWLockId  lwlock;             /**< The LWLock associated with this
								   *  memory context */
	size_t    next;               /**< Offset of 1st free byte */
	size_t    limit;              /**< Offset, of 1st byte beyond this 
								   * struct */
	
	struct ShmemCtl *memctl;      /**< Pointer to shared memory control
								   * structure. */
    void     *memory[0];          /**< The rest of the chunk, from which
								   * memory is allocated */
} MemContext;



/**
 * The key length for veil hash types.
 */
#define HASH_KEYLEN           60


/**
 * Describes the type of an Object record or one of its subtypes.
 */
typedef enum { 
	OBJ_UNDEFINED = 0,
    OBJ_SHMEMCTL,
	OBJ_INT4,
	OBJ_RANGE,
	OBJ_BITMAP,
	OBJ_BITMAP_ARRAY,
	OBJ_BITMAP_HASH,
	OBJ_BITMAP_REF,
	OBJ_INT4_ARRAY
} ObjType;

/** 
 * General purpose object-type.  All veil variables are effectively
 * sub-types of this.
 */
typedef struct Object {
    ObjType type;
} Object;

/** 
 * The ShmemCtl structure is the first object allocated from the first
 * chunk of shared memory in context 0.  This object describes and
 * manages shared memory allocated by shmalloc()
 */
typedef struct ShmemCtl {
    ObjType type;				  /**< This must have the value OBJ_SHMEMCTL */
    bool      initialised;        /**< Set to true once struct is setup */
    LWLockId  veil_lwlock;        /** dynamically allocated LWLock */
	int       current_context;    /**< Index of the current context (0
								   * or 1) */
    size_t    total_allocated[2]; /**< Total shared memory allocated in
								   * chunks in each context */ 
    bool      switching;          /**< Whether a context-switch is in
								   * progress */
	MemContext *context[2];       /**< Array (pair) of contexts */
	TransactionId xid[2];         /**< The transaction id of the
								   * transaction that initialised each
								   * context: this is used to determine
								   * whether there are transactions
								   * still runnning that may be using an
								   * earlier context. */
} ShmemCtl;

/**
 * Subtype of Object for storing simple int4 values.  These values are
 * allowed to be null.
 */
typedef struct Int4Var {
    ObjType type;		/**< This must have the value OBJ_INT4 */
	bool    isnull;     /**< if true, the value is null */
    int32   value;      /**< the integer value of the variable */
} Int4Var;

/**
 * Subtype of Object for storing range values.  A range has an upper and
 * lower bound, both stored as int4s.  Nulls are not allowed.
 */
typedef struct Range {
    ObjType type;		/**< This must have the value OBJ_RANGE */
    int32   min;
    int32   max;
} Range;

/**
 * Subtype of Object for storing bitmaps.  A bitmap is stored as an
 * array of int4 values.  See veil_bitmap.c for more information.  Note
 * that the size of a Bitmap structure is determined dynamically at run
 * time as the size of the array is only known then.
 */
typedef struct Bitmap {
    ObjType type;		/**< This must have the value OBJ_BITMAP */
    int32   bitzero;	/**< The index of the lowest bit the bitmap can
						 * store */
    int32   bitmax;		/**< The index of the highest bit the bitmap can
						 * store */
	uint32   bitset[0]; /**< Element zero of the array of int4 values
						 * comprising the bitmap. */
} Bitmap;

/**
 * Subtype of Object for storing bitmap refs.  A bitmapref is like a
 * bitmap but instead of containing a bitmap it contains a reference to
 * one.  This reference may be set during a transaction and then
 * referenced only from within the setting transaction.
 */
typedef struct BitmapRef {
    ObjType        type;	/**< This must have the value OBJ_BITMAP_REF */
    TransactionId  xid;		/**< The xid for which this variable is
							 * valid */
	Bitmap        *bitmap;
} BitmapRef;

/**
 * Subtype of Object for storing bitmap arrays.  A bitmap array is
 * simply an array of pointers to dynamically allocated Bitmaps.  Note
 * that the size of a Bitmap structure is determined dynamically at run
 * time as the size of the array is only known then.
 */
typedef struct BitmapArray {	// subtype of Object
    ObjType type;		/**< This must have the value OBJ_BITMAP_ARRAY */
    int32   bitzero;	/**< The index of the lowest bit each bitmap can
						 * store */
    int32   bitmax;		/**< The index of the highest bit each bitmap can
						 * store */
	int32   arrayzero;  /**< The index of array element zero: the
						 * index of the lowest numbered bitmap in the
						 * array */
	int32   arraymax;   /**< The index of the lowest numbered bitmap in
						 * the array */
	Bitmap *bitmap[0];  /** Element zero of the array of Bitmap pointers
						 * comprising the array. */
} BitmapArray;

/** 
 * Subtype of Object for storing bitmap hashes.  A bitmap hash is a hash
 * of dynamically allocated bitmaps, keyed by strings.  Note that these
 * cannot be created as shared variables.
 */
typedef struct BitmapHash {
    ObjType type;		/**< This must have the value OBJ_BITMAP_HASH */
    int32   bitzero;    /**< The index of the lowest bit each bitmap can
						 * store */
    int32   bitmax;     /**< The index of the highest bit each bitmap can
						 * store */
	HTAB   *hash;       /**< Pointer to the (Postgresql dynahash) hash
						 * table */ 
} BitmapHash;


/** 
 * Subtype of Object for storing arrays of integers.
 */
typedef struct Int4Array {
    ObjType type;		/**< This must have the value OBJ_INT4_ARRAY */
    int32   arrayzero;  /**< The index of array element zero: the
						 * index of the lowest numbered bitmap in the
						 * array */
    int32   arraymax;   /**< The index of the lowest numbered bitmap in
						 * the array */
	int32   array[0];   /** Element zero of the array of integers */
} Int4Array;


/**
 * A Veil variable.  These may be session or shared variables, and may
 * contain any Veil variable type.  They are created and accessed by
 * vl_lookup_shared_variable() and vl_lookup_variable(), and are stored
 * in either the shared hash or one of the session hashes.  See
 * veil_shmem.c and veil_variables.c for more details.
 */
typedef struct VarEntry {
    char  key[HASH_KEYLEN];	/**< String containing variable name */
	bool  shared;           /**< Whether this is a shared variable */
    Object *obj;            /**< Pointer to the contents of the variable */
} VarEntry;


/**
 * Describes a veil shared or session variable.  This matches the SQL
 * veil_variable_t which is defined as:
\verbatim
create type veil_variable_t as (
    name    text,
    type    text,
    shared  bool,
);
\endverbatim
 */
typedef struct veil_variable_t {
    char *name;			/**< The name of the variable */
    char *type;			/**< The type of the variable (eg "Bitmap") */
    bool shared;		/**< Whether this is a shared variable (as
						   opposed to a session variable) */
} veil_variable_t;


#endif

