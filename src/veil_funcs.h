/**
 * @file   veil_funcs.h
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2011 Marc Munro
 *     License:      BSD
 * \endcode
 * @brief  
 * Provide definitions for all non-local C-callable Veil functions.
 * 
 */

#include "veil_datatypes.h"
#include "fmgr.h"

/* veil_utils */
extern void *vl_malloc(size_t size);
char *vl_ObjTypeName(ObjType obj);


/* veil_variables */
extern VarEntry *vl_lookup_shared_variable(char *name);
extern VarEntry *vl_lookup_variable(char *name);
extern veil_variable_t *vl_next_variable(veil_variable_t *prev);
extern void vl_ClearInt4Array(Int4Array *array);
extern Int4Array *vl_NewInt4Array(Int4Array *current, bool shared,
								  int32 min, int32 max);
extern void vl_Int4ArraySet(Int4Array *array, int32 idx, int32 value);
extern int32 vl_Int4ArrayGet(Int4Array *array, int32 idx);


/* veil_datatypes */
extern Range *vl_NewRange(bool shared);
extern Int4Var *vl_NewInt4(bool shared);

/* veil_bitmap */
extern void vl_ClearBitmap(Bitmap *bitmap);
extern void vl_NewBitmap(Bitmap **p_bitmap, bool shared, int32 min, int32 max);
extern void vl_BitmapSetbit(Bitmap *bitmap, int32 bit);
extern void vl_BitmapClearbit(Bitmap *bitmap, int32 bit);
extern bool vl_BitmapTestbit(Bitmap *bitmap, int32 bit);
extern void vl_BitmapUnion(Bitmap *target,	Bitmap *source);
extern void vl_BitmapIntersect(Bitmap *target,	Bitmap *source);
extern int32 vl_BitmapNextBit(Bitmap *bitmap, int32 bit, bool *found);
extern Bitmap *vl_BitmapFromArray(BitmapArray *bmarray, int32 elem);
extern void vl_ClearBitmapArray(BitmapArray *bmarray);
extern void vl_NewBitmapArray(BitmapArray **p_bmarray, bool shared,
							  int32 arrayzero, int32 arraymax,
							  int32 bitzero, int32 bitmax);
extern VarEntry *vl_NextHashEntry(HTAB *hash, VarEntry *prev);
extern void vl_NewBitmapHash(BitmapHash **p_bmhash, char *name,
							 int32 bitzero, int32 bitmax);
extern Bitmap *vl_BitmapFromHash(BitmapHash *bmhash, char *hashelem);
extern Bitmap *vl_AddBitmapToHash(BitmapHash *bmhash, char *hashelem);
extern bool vl_BitmapHashHasKey(BitmapHash *bmhash, char *hashelem);

/* veil_shmem */
extern HTAB *vl_get_shared_hash(void);
extern bool vl_prepare_context_switch(void);
extern bool vl_complete_context_switch(void);
extern void vl_force_context_switch(void);
extern void *vl_shmalloc(size_t size);
extern void vl_free(void *mem);
extern void _PG_init(void);

/* veil_query */
extern int vl_spi_connect(bool *p_pushed);
extern int vl_spi_finish(bool pushed);
extern bool vl_bool_from_query(const char *qry, bool *result);
extern bool vl_db_exists(Oid db_id);
extern int  vl_call_init_fns(bool param);

/* veil_config */
extern void veil_config_init(void);
extern void veil_load_config(void);
extern int veil_shared_hash_elems(void);
extern int veil_dbs_in_cluster(void);
extern int veil_shmem_context_size(void);


/* veil_interface */
extern void vl_type_mismatch(char *name,  ObjType expected, ObjType got);
extern Datum veil_variables(PG_FUNCTION_ARGS);
extern Datum veil_share(PG_FUNCTION_ARGS);
extern Datum veil_init_range(PG_FUNCTION_ARGS);
extern Datum veil_range(PG_FUNCTION_ARGS);
extern Datum veil_init_bitmap(PG_FUNCTION_ARGS);
extern Datum veil_clear_bitmap(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_setbit(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_clearbit(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_testbit(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_union(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_intersect(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_bits(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_range(PG_FUNCTION_ARGS);
extern Datum veil_init_bitmap_array(PG_FUNCTION_ARGS);
extern Datum veil_clear_bitmap_array(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_from_array(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_array_testbit(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_array_setbit(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_array_clearbit(PG_FUNCTION_ARGS);
extern Datum veil_union_from_bitmap_array(PG_FUNCTION_ARGS);
extern Datum veil_intersect_from_bitmap_array(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_array_bits(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_array_arange(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_array_brange(PG_FUNCTION_ARGS);
extern Datum veil_init_bitmap_hash(PG_FUNCTION_ARGS);
extern Datum veil_clear_bitmap_hash(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_hash_key_exists(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_from_hash(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_hash_testbit(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_hash_setbit(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_hash_clearbit(PG_FUNCTION_ARGS);
extern Datum veil_union_into_bitmap_hash(PG_FUNCTION_ARGS);
extern Datum veil_union_from_bitmap_hash(PG_FUNCTION_ARGS);
extern Datum veil_intersect_from_bitmap_hash(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_hash_bits(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_hash_entries(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_from_hash(PG_FUNCTION_ARGS);
extern Datum veil_bitmap_hash_range(PG_FUNCTION_ARGS);
extern Datum veil_int4_set(PG_FUNCTION_ARGS);
extern Datum veil_int4_get(PG_FUNCTION_ARGS);
extern Datum veil_init_int4array(PG_FUNCTION_ARGS);
extern Datum veil_clear_int4array(PG_FUNCTION_ARGS);
extern Datum veil_int4array_set(PG_FUNCTION_ARGS);
extern Datum veil_int4array_get(PG_FUNCTION_ARGS);
extern Datum veil_init(PG_FUNCTION_ARGS);
extern Datum veil_perform_reset(PG_FUNCTION_ARGS);
extern Datum veil_force_reset(PG_FUNCTION_ARGS);
extern Datum veil_version(PG_FUNCTION_ARGS);
extern Datum veil_serialise(PG_FUNCTION_ARGS);
extern Datum veil_deserialise(PG_FUNCTION_ARGS);


/* veil_serialise */
extern char *vl_serialise_var(char *name);
extern int4 vl_deserialise(char **p_stream);
extern VarEntry *vl_deserialise_next(char **p_stream);
