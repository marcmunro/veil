/**
 * @file   veil_bitmap.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2011 Marc Munro
 *     License:      BSD
 * 
 * \endcode
 * @brief  
 * Functions for manipulating Bitmaps, BitmapHashes and BitmapArrays
 * 
 */

#include <stdio.h>
#include "postgres.h"
#include "veil_datatypes.h"
#include "veil_funcs.h"



/**
 * Array of bit positions for int32, indexed by bitno.
 */
static
uint32 bitmasks[] = {0x00000001, 0x00000002, 0x00000004, 0x00000008, 
				  	 0x00000010, 0x00000020, 0x00000040, 0x00000080, 
				  	 0x00000100, 0x00000200, 0x00000400, 0x00000800, 
				  	 0x00001000, 0x00002000, 0x00004000, 0x00008000,
				  	 0x00010000, 0x00020000, 0x00040000, 0x00080000,
				  	 0x00100000, 0x00200000, 0x00400000, 0x00800000,
				  	 0x01000000, 0x02000000, 0x04000000, 0x08000000,
				  	 0x10000000, 0x20000000, 0x40000000, 0x80000000};


/** 
 * Clear all bits in a ::Bitmap.
 * 
 * @param bitmap The ::Bitmap in which all bits are to be cleared
 */
void
vl_ClearBitmap(Bitmap *bitmap)
{
	int elems = ARRAYELEMS(bitmap->bitzero, bitmap->bitmax);
	int i;
	
	for (i = 0; i < elems; i++) {
		bitmap->bitset[i] = 0;
	}
}

/** 
 * Return a newly initialised (empty) ::Bitmap.  The bitmap may already
 * exist in which case it will be re-used if possible.  The bitmap may
 * be created in either session or shared memory depending on the value
 * of shared.
 * 
 * @param p_bitmap Pointer to an existing bitmap if one exists
 * @param shared Whether to create the bitmap in shared memory
 * @param min The smallest bit to be stored in the bitmap
 * @param max The largest bit to be stored in the bitmap
 */
void
vl_NewBitmap(Bitmap **p_bitmap, bool shared,
			 int32 min, int32 max)
{
	Bitmap *bitmap = *p_bitmap;
	int     elems = ARRAYELEMS(min, max);

	if (bitmap) {
		int cur_elems = ARRAYELEMS(bitmap->bitzero, bitmap->bitmax);
		/* OK, there is an old bitmap in place.  If it is the same size
		 * or larger than we need we will re-use it, otherwise we will
		 * dispose of it and get a new one. */

		if (elems <= cur_elems) {
			vl_ClearBitmap(bitmap);
		}
		else {
			if (shared) {
				vl_free(bitmap);
			}
			else {
				pfree(bitmap);
			}
			bitmap = NULL;
		}
	}
	if (!bitmap) {
		/* We need a new bitmap */
		if (shared) {
			bitmap = vl_shmalloc(sizeof(Bitmap) + (sizeof(int32) * elems));
		}
		else {
			bitmap = vl_malloc(sizeof(Bitmap) + (sizeof(int32) * elems));
		}
	}

	DBG_SET_CANARY(*bitmap);
	DBG_SET_ELEMS(*bitmap, elems);
	DBG_SET_TRAILER(*bitmap, bitset);
	bitmap->type = OBJ_BITMAP;
	bitmap->bitzero = min;
	bitmap->bitmax = max;
	vl_ClearBitmap(bitmap);

	DBG_TEST_CANARY(*bitmap);
	DBG_TEST_TRAILER(*bitmap, bitset);
	*p_bitmap = bitmap;
}

/** 
 * Set a bit within a ::Bitmap.  If the bit is outside of the acceptable
 * range, raise an error.
 * 
 * @param bitmap The ::Bitmap within which the bit is to be set. 
 * @param bit The bit to be set.
 */
void
vl_BitmapSetbit(Bitmap *bitmap,
				int32 bit)
{
    int relative_bit = bit - BITZERO(bitmap->bitzero);
    int element = BITSET_ELEM(relative_bit);

	if ((bit > bitmap->bitmax) ||
		(bit < bitmap->bitzero)) 
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Bitmap range error"),
				 errdetail("Bit (%d) not in range %d..%d.  ", bit,
						   bitmap->bitzero, bitmap->bitmax)));
	}

	DBG_CHECK_INDEX(*bitmap, element);
    bitmap->bitset[element] |= bitmasks[BITSET_BIT(relative_bit)];
	DBG_TEST_CANARY(*bitmap);
	DBG_TEST_TRAILER(*bitmap, bitset);
}

/** 
 * Clear a bit within a ::Bitmap.  If the bit is outside of the acceptable
 * range, raise an error.
 * 
 * @param bitmap The ::Bitmap within which the bit is to be cleared. 
 * @param bit The bit to be cleared.
 */
void
vl_BitmapClearbit(Bitmap *bitmap,
				  int32 bit)
{
    int relative_bit = bit - BITZERO(bitmap->bitzero);
    int element = BITSET_ELEM(relative_bit);

	if ((bit > bitmap->bitmax) ||
		(bit < bitmap->bitzero)) 
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Bitmap range error"),
				 errdetail("Bit (%d) not in range %d..%d.  ", bit,
						   bitmap->bitzero, bitmap->bitmax)));
	}

    bitmap->bitset[element] &= ~(bitmasks[BITSET_BIT(relative_bit)]);
}

/** 
 * Test a bit within a ::Bitmap.  If the bit is outside of the acceptable
 * range return false.
 * 
 * @param bitmap The ::Bitmap within which the bit is to be set. 
 * @param bit The bit to be tested.
 * 
 * @return True if the bit is set, false otherwise.
 */
bool
vl_BitmapTestbit(Bitmap *bitmap,
				 int32 bit)
{
    int relative_bit = bit - BITZERO(bitmap->bitzero);
    int element = BITSET_ELEM(relative_bit);

	if ((bit > bitmap->bitmax) ||
		(bit < bitmap->bitzero)) 
	{
		return false;
	}

    return (bitmap->bitset[element] & bitmasks[BITSET_BIT(relative_bit)]) != 0;
}

/** 
 * Create the union of two bitmaps, updating the first with the result.
 * 
 * @param target The ::Bitmap into which the result will be placed.
 * @param source The ::Bitmap to be unioned into target.
 */
void
vl_BitmapUnion(Bitmap *target,
			   Bitmap *source)
{
	int i;
	int elems = ARRAYELEMS(target->bitzero, target->bitmax);

	/* TODO: Make this tolerant of range mismatches. */
	if ((target->bitzero != source->bitzero) ||
		(target->bitmax != source->bitmax))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Incompatible bitmaps"),
				 errdetail("Target range is %d..%d.  Source range %d..%d.",
						   target->bitzero, target->bitmax,
						   source->bitzero, source->bitmax)));
	}
	for (i = 0; i < elems; i++) {
		target->bitset[i] |= source->bitset[i];
	}
}

/** 
 * Create the intersection of two bitmaps, updating the first with the
 * result.
 * 
 * @param target The ::Bitmap into which the result will be placed.
 * @param source The ::Bitmap to be intersected into target.
 */
void
vl_BitmapIntersect(Bitmap *target,
				   Bitmap *source)
{
	int i;
	int elems = ARRAYELEMS(target->bitzero, target->bitmax);

	/* TODO: Make this tolerant of range mismatches. */
	if ((target->bitzero != source->bitzero) ||
		(target->bitmax != source->bitmax))
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Incompatible bitmaps"),
				 errdetail("Target range is %d..%d.  Source range %d..%d.",
						   target->bitzero, target->bitmax,
						   source->bitzero, source->bitmax)));
	}
	for (i = 0; i < elems; i++) {
		target->bitset[i] &= source->bitset[i];
	}
}

/** 
 * Return the next set bit in the ::Bitmap.
 * 
 * @param bitmap The ::Bitmap being scanned.
 * @param bit The starting bit from which to scan the bitmap
 * @param found Boolean that will be set to true when a set bit has been
 * found.
 * 
 * @return The bit id of the found bit, or zero if no set bits were found. 
 */
int32
vl_BitmapNextBit(Bitmap *bitmap,
				 int32 bit,
				 bool *found)
{
	while (bit <= bitmap->bitmax) {
		if (vl_BitmapTestbit(bitmap, bit)) {
			*found = true;
			return bit;
		}
		bit++;
	}
	*found = false;
	return 0;
}

/** 
 * Return a specified ::Bitmap from a ::BitmapArray.
 * 
 * @param bmarray The ::BitmapArray from which the result is to be
 * returned.
 * @param elem The index of the ::Bitmap within the array. 
 * 
 * @return The bitmap corresponding to the parameters, or NULL if no
 *such entry exists within the array.
 */
Bitmap *
vl_BitmapFromArray(BitmapArray *bmarray,
				   int32 elem)
{
	DBG_TEST_CANARY(*bmarray);
	DBG_TEST_TRAILER(*bmarray, bitmap);
	if ((elem < bmarray->arrayzero) || (elem > bmarray->arraymax)) {
		return NULL;
	}
	else {
		DBG_CHECK_INDEX(*bmarray, elem - bmarray->arrayzero);
		return bmarray->bitmap[elem - bmarray->arrayzero];
	}
}

/** 
 * Clear all bitmaps in the given ::BitmapArray
 * 
 * @param bmarray The ::BitmapArray to be cleared
 */
void
vl_ClearBitmapArray(BitmapArray *bmarray)
{
	int bitmaps = bmarray->arraymax + 1 - bmarray->arrayzero;
	int i;

	DBG_TEST_CANARY(*bmarray);
	DBG_TEST_TRAILER(*bmarray, bitmap);
	for (i = 0; i < bitmaps; i++) {
		DBG_CHECK_INDEX(*bmarray, i);
		vl_ClearBitmap(bmarray->bitmap[i]);
	}
}

/** 
 * Return a newly initialised (empty) ::BitmapArray.  It may already
 * exist in which case it will be re-used if possible.  It may
 * be created in either session or shared memory depending on the value
 * of shared.
 * 
 * @param p_bmarray Pointer to an existing bitmap if one exists.
 * @param shared Whether to create the bitmap in shared memory
 * @param arrayzero The lowest array index
 * @param arraymax The highest array index
 * @param bitzero The smallest bit to be stored in the bitmap
 * @param bitmax The largest bit to be stored in the bitmap
 */
void
vl_NewBitmapArray(BitmapArray **p_bmarray, bool shared,
				  int32 arrayzero, int32 arraymax,
				  int32 bitzero, int32 bitmax)
{
	BitmapArray *bmarray = *p_bmarray;
	int bitsetelems = ARRAYELEMS(bitzero, bitmax);
	int bitmaps = arraymax + 1 - arrayzero;
	int i;

	if (bmarray) {
		/* We already have a bitmap array.  If possible, we re-use it. */
		int cur_elems = ARRAYELEMS(bmarray->bitzero, bmarray->bitmax);
		int cur_maps = bmarray->arraymax + 1 - bmarray->arrayzero;

		DBG_TEST_CANARY(*bmarray);
		DBG_TEST_TRAILER(*bmarray, bitmap);
		if ((cur_elems >= bitsetelems) && (cur_maps >= bitmaps)) {
			vl_ClearBitmapArray(bmarray);
		}
		else {
			if (shared) {
				for (i = 0; i < cur_maps; i++) {
					vl_free(bmarray->bitmap[i]);
				}
				vl_free(bmarray);
			}
			else {
				for (i = 0; i < cur_maps; i++) {
					pfree(bmarray->bitmap[i]);
				}
				pfree(bmarray);
			}
			bmarray = NULL;
		}
	}

	if (!bmarray) {
		if (shared) {
			bmarray = vl_shmalloc(sizeof(BitmapArray) + 
								 (sizeof(Bitmap *) * bitmaps));

		}
		else {
			bmarray = vl_malloc(sizeof(BitmapArray) + 
								(sizeof(Bitmap *) * bitmaps));
		}

		for (i = 0; i < bitmaps; i++) {
			bmarray->bitmap[i] = NULL;
			vl_NewBitmap(&(bmarray->bitmap[i]), shared, bitzero, bitmax);
		}

		bmarray->type = OBJ_BITMAP_ARRAY;
		DBG_SET_CANARY(*bmarray);
		DBG_SET_ELEMS(*bmarray, bitmaps);
		DBG_SET_TRAILERP(*bmarray, bitmap);
	}
	bmarray->bitzero = bitzero;
	bmarray->bitmax = bitmax;
	bmarray->arrayzero = arrayzero;
	bmarray->arraymax = arraymax;

	for (i = 0; i < bitmaps; i++) {
		bmarray->bitmap[i]->type = OBJ_BITMAP;
		bmarray->bitmap[i]->bitzero = bitzero;
		bmarray->bitmap[i]->bitmax = bitmax;
	}

	*p_bmarray = bmarray;
}

/** 
 * Create a new hash table.  This is allocated from session memory as
 * BitmapHashes may not be declared as shared variables.
 * 
 * @param name The name of the hash to be created.  Note that we prefix
 * this with "vl_" to prevent name collisions from other subsystems.
 * 
 * @return Pointer to the newly created hash table.
 */
static HTAB *
new_hash(char *name)
{
	char     vl_name[HASH_KEYLEN];
	HTAB    *hash;
	HASHCTL  hashctl;

	(void) snprintf(vl_name, HASH_KEYLEN - 1, "vl_%s", name);

	hashctl.keysize = HASH_KEYLEN;
	hashctl.entrysize = sizeof(VarEntry);

	hash = hash_create(vl_name, 200, &hashctl, HASH_ELEM);
    return hash;
}

/** 
 * Utility function for scanning the hash table of a BitmapHash.
 * 
 * @param hash The hash table being scanned
 * @param prev The entry from which to scan, starting with NULL.
 * 
 * @return The next element in the hash table (a VarEntry) or NULL when
 * the last element has already been scanned.
 */
VarEntry *
vl_NextHashEntry(HTAB *hash,
				 VarEntry *prev)
{
	VarEntry *next;
	static HASH_SEQ_STATUS status;
	if (!prev) {
		/* Initialise the hash search */
		
		hash_seq_init(&status, hash);
	}
	next = (VarEntry *) hash_seq_search(&status);
	return (next);
}

/** 
 * Return a newly initialised (empty) ::BitmapHash.  It may already
 * exist in which case it will be re-used if possible.  BitmapHash
 * variables may only be created as session (not shared) variables.
 * 
 * @param p_bmhash Pointer to an existing bitmap if one exists.
 * @param name The name to be used for the hash table
 * @param bitzero The smallest bit to be stored in the bitmap
 * @param bitmax The largest bit to be stored in the bitmap
 */
void
vl_NewBitmapHash(BitmapHash **p_bmhash, char *name,
				 int32 bitzero, int32 bitmax)
{
	BitmapHash *bmhash = *p_bmhash;

	if (bmhash) {
		VarEntry *entry = NULL;
		HTAB *hash = bmhash->hash;
		bool found;

		while ((entry = vl_NextHashEntry(hash, entry))) {
			if (entry->obj) {
				if (entry->obj->type != OBJ_BITMAP) {
					ereport(ERROR,
							(errcode(ERRCODE_INTERNAL_ERROR),
							 errmsg("Bitmap hash contains invalid object %d",
									entry->obj->type),
							 errdetail("Object type is %d, expected is %d.",
									   entry->obj->type, OBJ_BITMAP)));
				}
				pfree(entry->obj);  /* Free the bitmap */
			}
			/* Now remove the entry from the hash */
			(void) hash_search(hash, entry->key, HASH_REMOVE, &found);
		}
	}
	else {
		bmhash = vl_malloc(sizeof(BitmapHash));
		bmhash->type = OBJ_BITMAP_HASH;
		bmhash->hash = new_hash(name);
	}
	bmhash->bitzero = bitzero;
	bmhash->bitmax = bitmax;

	*p_bmhash = bmhash;
}

/** 
 * Return a specified ::Bitmap from a ::BitmapHash.  Raise an error if
 * the returned object from the hash search is not a bitmap.
 * 
 * @param bmhash The ::BitmapHash from which the result is to be
 * returned.
 * @param hashelem The key of the ::Bitmap within the hash. 
 * 
 * @return The bitmap corresponding to the parameters, or NULL if no
 *such entry exists within the hash.
 */
Bitmap *
vl_BitmapFromHash(BitmapHash *bmhash,
				  char *hashelem)
{
	VarEntry *var;
	Bitmap   *bitmap;
	bool      found;

	var = hash_search(bmhash->hash, hashelem, HASH_FIND, &found);
	if (!found) {
		return NULL;
	}
	
	if (!var->obj) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("BitmapFromHash - empty VarEntry")));
	}

	if (var->obj->type != OBJ_BITMAP) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Bitmap hash contains invalid object %d",
						var->obj->type),
				 errdetail("Object type is %d, expected is %d.",
						   var->obj->type, OBJ_BITMAP)));
	}

	bitmap = (Bitmap *) var->obj;
	return bitmap;
}

/** 
 * Create a newly allocated empty ::Bitmap to a ::BitmapHash
 * 
 * @param bmhash The ::BitmapHash to which to add the new ::Bitmap.
 * @param hashelem The key for the new entry
 * 
 * @return Pointer to the newly allocated empty ::Bitmap.
 */
Bitmap *
vl_AddBitmapToHash(BitmapHash *bmhash,
				   char *hashelem)
{
	VarEntry *var;
	Bitmap   *bitmap = NULL;
	bool      found;

	var = hash_search(bmhash->hash, hashelem, HASH_ENTER, &found);
	if (found) {
		if (!var->obj) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("AddBitmapToHash - empty VarEntry")));
		}
		if (var->obj->type != OBJ_BITMAP) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("AddBitmapToHash - type mismatch %d",
							var->obj->type),
					 errdetail("Object type is %d, expected is %d.",
							   var->obj->type, OBJ_BITMAP)));
		}
		return (Bitmap *) var->obj;
	}

	/* We've created a new entry.  Now create the bitmap for it. */

	vl_NewBitmap(&bitmap, FALSE, bmhash->bitzero, bmhash->bitmax);
	var->obj = (Object *) bitmap;
	return bitmap;
}

/** 
 * Determine whether the supplied key exists in the ::BitmapHash.
 * 
 * @param bmhash The ::BitmapHash to be tested
 * @param hashelem The key to be tested
 * 
 * @return True if the key already exists in the hash.
 */
bool
vl_BitmapHashHasKey(BitmapHash *bmhash,
				    char *hashelem)
{
	bool found;

	(void) hash_search(bmhash->hash, hashelem, HASH_FIND, &found);
	return found;
}
