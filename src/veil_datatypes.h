/**
 * @file   veil_datatypes.h
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2011 Marc Munro
 *     License:      BSD
 * 
 * \endcode
 * @brief  
 * Define all Veil public datatypes 
 * 
 */

#ifndef VEIL_DATATYPES
/** 
 * Prevent this header from being included multiple times.
 */
#define VEIL_DATATYPES 1

#ifndef VEIL_DEBUG
/**
 * Enables various debugging constructs, such as canaries, in code and
 * data structures.  If such debugging is required, define VEIL_DEBUG in
 * the make invocation, eg: "make VEIL_DEBUG=1".
 */
// TODO: REMOVE THE FOLLOWING DEFINITION
#define VEIL_DEBUG 1
#endif 

#if VEIL_DEBUG == 1
/** 
 * Value to be set in sacrificial "canary" fields.  If this value is not
 * as expected, the canary has been killed by something inappropriately 
 * stomping on memory.
 */
#define DBG_CANARY 0xca96ca96

/** 
 * Defines a canary element in a data structure.
 */
#define DBG_CANARY_ENTRY int32 canary;

/** 
 * Field to record the size of an array so that its canary element can
 * be found.
 */
#define DBG_ELEMS_ENTRY  int32 dbgelems;

/** 
 * Code to records the size of an array so that its canary element can
 * be found.
 */
#define DBG_SET_ELEMS(x,y) (x).dbgelems = y

/** 
 * Code to initialise a canary.
 */
#define DBG_SET_CANARY(x) (x).canary = DBG_CANARY

/** 
 * Code to test for a canary having been overwritten.
 */
#define DBG_TEST_CANARY(x) if ((x).canary != DBG_CANARY) {\
		elog(ERROR, "canary fault"); }

/** 
 * Base size for an array containing a canary.  This is zero if
 * VEIL_DEBUG is not defined, when this is defined arrays will be one
 * element longer to allow a canary to be placed at the end of the array.
 */
#define EMPTY 1

/**
 * Set a trailing canary in an array.
 */
#define DBG_SET_TRAILER(x, y) (x).y[(x).dbgelems] = DBG_CANARY;
/**
 * Set a trailing canary in an array (using a void pointer).
 */
#define DBG_SET_TRAILERP(x, y) (x).y[(x).dbgelems] = (void *) DBG_CANARY;

/**
 * Test the trailing canary in an array.
 */
#define DBG_TEST_TRAILER(x, y)							\
	if ((int) (x).y[(x).dbgelems] != DBG_CANARY) {		\
		elog(ERROR, "trailing canary fault"); }

/**
 * Check whether an array index is in bounds.
 */
#define DBG_CHECK_INDEX(x,i) if (i >= (x).dbgelems) {\
		elog(ERROR, "Element index out of range %d", i); }

#else
#define DBG_CANARY_ENTRY 
#define DBG_ELEMS_ENTRY 
#define DBG_SET_ELEMS(x,y)
#define DBG_SET_CANARY(x) 
#define DBG_TEST_CANARY(x)
#define EMPTY 0
#define DBG_SET_TRAILER(x,Y)
#define DBG_TEST_TRAILER(x,Y)
#define DBG_CHECK_INDEX(x,i)
#endif

#include "utils/hsearch.h"
#include "storage/lwlock.h"

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
    ObjType type;				  /**< Identifies the type of the object. */
} Object;

/** 
 * The ShmemCtl structure is the first object allocated from the first
 * chunk of shared memory in context 0.  This object describes and
 * manages shared memory allocated by shmalloc()
 */
typedef struct ShmemCtl {
    ObjType type;				  /**< This must have the value OBJ_SHMEMCTL */
    bool      initialised;        /**< Set to true once struct is setup */
    LWLockId  veil_lwlock;        /**< dynamically allocated LWLock */
	int       current_context;    /**< Index of the current context (0
								   * or 1) */
    size_t    total_allocated[2]; /**< Total shared memory allocated in
								   * chunks in each context */ 
    bool      switching;          /**< Whether a context-switch is in
								   * progress */
	MemChunk *context[2];         /**< The first chunks of each context */
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
    int32   min;        /**< Lower limit for range. */
    int32   max;        /**< Upper limit for range. */
} Range;



/**
 * Gives the bitmask index for the bitzero value of a Bitmap.  This is
 * part of the "normalisation" process for bitmap ranges.  This process
 * allows unlike bitmaps to be more easily compared by forcing bitmap
 * indexes to be normalised around 32-bit word boundaries.  Eg, 2 bitmaps
 * with domains 1 to 50 and 3 to 55, will have identical bit patterns
 * for bits 3 to 50.
 * 
 * @param x The bitzero value of a bitmap
 * 
 * @return The bitmask index representing x.
 */
#define BITZERO(x) (x & 0xffffffe0)

/**
 * Gives the bitmask index for the bitmax value of a bitmap.  See
 * BITZERO() for more information.
 *
 * @param x The bitmax value of a bitmap
 *
 * @return The bitmask index representing x.
 */
#define BITMAX(x) (x | 0x1f)

/**
 * Gives the index of a bit within the array of 32-bit words that
 * comprise the bitmap.
 *
 * @param x The bit in question
 *
 * @return The array index of the bit.
 */
#define BITSET_ELEM(x) (x >> 5)

/**
 * Gives the index into ::bitmasks for the bit specified in x.
 *
 * @param x The bit in question
 *
 * @return The bitmask index
 */
#define BITSET_BIT(x) (x & 0x1f)

/**
 * Gives the number of array elements in a ::Bitmap that runs from
 * element min to element max.
 *
 * @param min
 * @param max
 *
 * @return The number of elements in the bitmap.
 */
#define ARRAYELEMS(min,max) (((max - BITZERO(min)) >> 5) + 1)


/**
 * Return the smaller of a or b.  Note that expressions a and b may be
 * evaluated more than once.
 *
 * @param a
 * @param b
 *
 * @return The smaller value of a or b.
 */
#define MIN(a,b) ((a < b)? a: b)


/**
 * Subtype of Object for storing bitmaps.  A bitmap is stored as an
 * array of int4 values.  See veil_bitmap.c for more information.  Note
 * that the size of a Bitmap structure is determined dynamically at run
 * time as the size of the array is only known then.
 */
typedef struct Bitmap {
    ObjType type;		/**< This must have the value OBJ_BITMAP */
    DBG_CANARY_ENTRY	/**< Debugging entry */
	DBG_ELEMS_ENTRY		/**< Debugging entry */
    int32   bitzero;	/**< The index of the lowest bit the bitmap can
						 * store */
    int32   bitmax;		/**< The index of the highest bit the bitmap can
						 * store */
	uint32   bitset[EMPTY]; /**< Element zero of the array of int4 values
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
	Bitmap        *bitmap;  /**< Pointer to the referenced bitmap */
} BitmapRef;

/**
 * Subtype of Object for storing bitmap arrays.  A bitmap array is
 * simply an array of pointers to dynamically allocated Bitmaps.  Note
 * that the size of a Bitmap structure is determined dynamically at run
 * time as the size of the array is only known then.
 */
typedef struct BitmapArray {	// subtype of Object
    ObjType type;		/**< This must have the value OBJ_BITMAP_ARRAY */
    DBG_CANARY_ENTRY    /**< Debugging entry */
	DBG_ELEMS_ENTRY     /**< Debugging entry */
    int32   bitzero;	/**< The index of the lowest bit each bitmap can
						 * store */
    int32   bitmax;		/**< The index of the highest bit each bitmap can
						 * store */
	int32   arrayzero;  /**< The index of array element zero: the
						 * index of the lowest numbered bitmap in the
						 * array */
	int32   arraymax;   /**< The index of the lowest numbered bitmap in
						 * the array */
	Bitmap *bitmap[EMPTY];  /**< Element zero of the array of Bitmap pointers
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
	int32   array[0];   /**< Element zero of the array of integers */
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

