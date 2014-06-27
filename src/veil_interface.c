/**
 * @file   veil_interface.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2012 Marc Munro
 *     License:      BSD
 *
 * \endcode
 * @brief  
 * Functions providing the SQL interface to veil, and utility functions
 * to support them.
 */

#include "postgres.h"
#include "access/xact.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"

#include "veil_version.h"
#include "veil_funcs.h"
#include "veil_datatypes.h"


PG_MODULE_MAGIC;


/** 
 * Create a dynamically allocated C string as a copy of a text value.
 * 
 * @param in text value from which the copy is made.
 * @return Dynamically allocated (by palloc()) copy of in.
 */
static char *
strfromtext(text *in)
{
    char *out = palloc(VARSIZE(in) - VARHDRSZ + 1);
    memcpy(out, VARDATA(in), VARSIZE(in) - VARHDRSZ);
    out[VARSIZE(in) - VARHDRSZ] = '\0';

    return out;
}

/** 
 * Create a dynamically allocated text value as a copy of a C string.
 * 
 * @param in String to be copied
 * @return Dynamically allocated (by palloc()) copy of in.
 */
static text *
textfromstr(char *in)
{
    int   len = strlen(in);
    text *out = palloc(len + VARHDRSZ);
    memcpy(VARDATA(out), in, len);
	SET_VARSIZE(out, (len + VARHDRSZ));

    return out;
}

/** 
 * Create a dynamically allocated text value as a copy of a C string,
 * applying a limit to the length.
 * 
 * @param in String to be copied
 * @param limit Maximum length of string to be copied.
 * @return Dynamically allocated (by palloc()) copy of in.
 */
static text *
textfromstrn(char *in, int limit)
{
    int   len = strlen(in);
    text *out;

    if (limit < len) {
        len = limit;
    }

    out = palloc(len + VARHDRSZ);
    memcpy(VARDATA(out), in, len);
	SET_VARSIZE(out, (len + VARHDRSZ));

    return out;
}

/** 
 * Create a dynamically allocated text value as a copy of a C string,
 * applying a limit to the length.
 * 
 * @param str String to be copied
 * @return Dynamically allocated (by palloc()) copy of str.
 */
static char *
copystr(char *str)
{
    char *new = palloc((sizeof(char) * strlen(str) + 1));
    strcpy(new, str);
    return new;
}

/** 
 * Create a dynamically allocated C string as a copy of an integer value.
 * 
 * @param val value to be stringified
 * @return Dynamically allocated string.
 */
static char *
strfromint(int4 val)
{
    char *new = palloc((sizeof(char) * 17)); /* Large enough for any 32 
											  * bit number */
    sprintf(new, "%d", val);
    return new;
}

/** 
 * Create a dynamically allocated C string as a copy of a boolean value.
 * 
 * @param val value to be stringified
 * @return Dynamically allocated string.
 */
static char *
strfrombool(bool val)
{
    char *new = palloc((sizeof(char) * 2));
    if (val) {
        strcpy(new, "t");
    }
    else {
        strcpy(new, "f");
    }
    return new;
}

/** 
 * Perform session initialisation once for the session.  This calls the
 * user-defined function veil_init which should create and possibly
 * initialise all session and, maybe, shared variables.  This function
 * may be safely called any number of times - it will only perform the
 * initialisation on the first call.
 * 
 */
static void
ensure_init()
{
    bool  success = false;
	TransactionId this_xid;
    int   ok;
	bool pushed;
    static bool done = false;
	static TransactionId xid = 0;

    if (!done) {
		this_xid =  GetCurrentTransactionId();
		if (xid == this_xid) {
			/* We must have been called recursively, so just return */
			return;
		}
		xid = this_xid;       /* Record our xid in case we recurse */
        ok = vl_spi_connect(&pushed);
        if (ok != SPI_OK_CONNECT) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("failed to initialise session (1)"),
					 errdetail("SPI_connect() failed, returning %d.", ok)));
        }

		(void) vl_get_shared_hash();  /* Init all shared memory constructs */
        (void) vl_bool_from_query("select veil.veil_init(FALSE)", &success);

        if (!success) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("failed to initialise session (2)"),
					 errdetail("veil_init() did not return true.")));
        }
        
        ok = vl_spi_finish(pushed);
        if (ok != SPI_OK_FINISH) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("failed to initialise session (3)"),
					 errdetail("SPI_finish() failed, returning %d.", ok)));
        }
        done = true;	/* init is done, we don't need to do it again. */
    }
}

/** 
 * Report, by raising an error, a type mismatch between the expected and
 * actual type of a VarEntry variable.
 * 
 * @param name The name of the variable
 * @param expected The expected type.
 * @param got The actual type
 */
extern void
vl_type_mismatch(char *name,
                 ObjType expected,
                 ObjType got)
{
	ereport(ERROR,
			(errcode(ERRCODE_INTERNAL_ERROR),
			 errmsg("type mismatch in %s: expected %s, got %s",
					name, vl_ObjTypeName(expected), vl_ObjTypeName(got)),
			 errdetail("Variable %s is not of the expected type.", name)));
}

/** 
 * Return the Int4Var variable matching the name parameter, possibly
 * creating the variable.  Raise an error if the named variable already
 * exists and is of the wrong type.
 * 
 * @param name The name of the variable.
 * @param create Whether to create the variable if it does not exist.
 * @return Pointer to the variable or null if the variable does not
 * exist and create was false.
 */
static Int4Var *
GetInt4Var(char *name,
           bool  create)
{
    VarEntry *var;
    Int4Var  *i4v;
    
    var = vl_lookup_variable(name);
    i4v = (Int4Var *) var->obj;

    if (i4v) {
        if (i4v->type != OBJ_INT4) {
            vl_type_mismatch(name, OBJ_INT4, i4v->type);
        }
    }
    else {
        if (create) {
            var->obj = (Object *) vl_NewInt4(var->shared);
            i4v = (Int4Var *) var->obj;
        }
        else {
            vl_type_mismatch(name, OBJ_INT4, OBJ_UNDEFINED);
        }
    }
    return i4v;
}

/** 
 * Return the Range variable matching the name parameter, possibly
 * creating the variable.  Raise an error if the named variable already
 * exists and is of the wrong type.
 * 
 * @param name The name of the variable.
 * @param create Whether to create the variable if it does not exist.
 * @return Pointer to the variable or null if the variable does not
 * exist and create was false.
 */
static Range *
GetRange(char *name,
         bool  create)
{
    VarEntry *var;
    Range    *range;
    
    var = vl_lookup_variable(name);
    range = (Range *) var->obj;

    if (range) {
        if (range->type != OBJ_RANGE) {
            vl_type_mismatch(name, OBJ_RANGE, range->type);
        }
    }
    else {
        if (create) {
            var->obj = (Object *) vl_NewRange(var->shared);
            range = (Range *) var->obj;
        }
        else {
            vl_type_mismatch(name, OBJ_RANGE, OBJ_UNDEFINED);
        }
    }
    return range;
}

/** 
 * Return the Bitmap from a bitmap variable.  This function exists
 * primarily to perform type checking, and to raise an error if the
 * variable is not a bitmap.
 * 
 * @param var The VarEntry that should contain a bitmap.
 * @param allow_empty Whether to raise an error if the variable has not
 * yet been initialised.
 * @param allow_ref Whether to (not) raise an error if the variable is a
 * bitmap_ref rather than a bitmap.
 * @return Pointer to the variable or null if the variable is undefined 
 * and allow_empty was true.
 */
static Bitmap *
GetBitmapFromVar(VarEntry *var,
                 bool allow_empty,
				 bool allow_ref)
{
    Bitmap *bitmap = (Bitmap *) var->obj;

    if (bitmap) {
        if (bitmap->type != OBJ_BITMAP) {
			if (allow_ref && (bitmap->type == OBJ_BITMAP_REF)) {
				BitmapRef *bmref = (BitmapRef *) bitmap;
				if (bmref->xid == GetCurrentTransactionId()) {
					bitmap = bmref->bitmap;
				}
				else {
					ereport(ERROR,
							(errcode(ERRCODE_INTERNAL_ERROR),
							 errmsg("BitmapRef %s is not defined",
									var->key),
							 errhint("Perhaps the name is mis-spelled, or its "
								     "definition is missing from "
									 "veil_init().")));
				}
			}
			else {
				vl_type_mismatch(var->key, OBJ_BITMAP, bitmap->type);
			}
        }
    }
	if (!bitmap) {
        if (!allow_empty) {
            vl_type_mismatch(var->key, OBJ_BITMAP, OBJ_UNDEFINED);
        }
    }
    return bitmap;
}

/** 
 * Return the Bitmap matching the name parameter, possibly creating the
 * VarEntry (variable) for it.  Raise an error if the named variable
 * already exists and is of the wrong type.
 * 
 * @param name The name of the variable.
 * @param allow_empty Whether to raise an error if the variable has not
 * been defined.
 * @param allow_ref Whether to (not) raise an error if the variable is a
 * bitmap_ref rather than a bitmap.
 * @return Pointer to the variable or null if the variable does not
 * exist and allow_empty was false.
 */
static Bitmap *
GetBitmap(char *name,
          bool allow_empty,
		  bool allow_ref)
{
    VarEntry *var;
    Bitmap   *bitmap;

    var = vl_lookup_variable(name);
    bitmap = GetBitmapFromVar(var, allow_empty, allow_ref);

	return bitmap;
}

/** 
 * Return the BitmapRef from a bitmap ref variable.  This function exists
 * primarily to perform type checking, and to raise an error if the
 * variable is not a bitmap ref.  Note that BitmapRef variables may not
 * be shared as they can contain references to non-shared objects.
 * 
 * @param var The VarEntry that should contain a bitmap ref.
 * @return Pointer to the variable.
 */
static BitmapRef *
GetBitmapRefFromVar(VarEntry *var)
{
    BitmapRef *bmref = (BitmapRef *) var->obj;

    if (bmref) {
        if (bmref->type != OBJ_BITMAP_REF) {
			vl_type_mismatch(var->key, OBJ_BITMAP_REF, bmref->type);
		}
    }
	else {
		if (var->shared) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("illegal attempt to define shared BitmapRef %s",
							var->key),
					 errhint("BitmapRefs may only be defined as session, "
							 "not shared, variables.")));
		}
		/* Create a new bmref (these are always session variables. */
		bmref = vl_malloc(sizeof(BitmapRef));
		bmref->type = OBJ_BITMAP_REF;
		bmref->bitmap = NULL;
		var->obj = (Object *) bmref;
	}

    return bmref;
}

/** 
 * Return the BitmapRef matching the name parameter, possibly creating the
 * VarEntry (variable) for it.  Raise an error if the named variable
 * already exists and is of the wrong type.
 * 
 * @param name The name of the variable.
 * @return Pointer to the variable
 */
static BitmapRef *
GetBitmapRef(char *name)
{
    VarEntry  *var;
    BitmapRef *bmref;

    var = vl_lookup_variable(name);
    bmref = GetBitmapRefFromVar(var);

	return bmref;
}

/** 
 * Return the BitmapArray from a bitmap array variable.  This function
 * exists primarily to perform type checking, and to raise an error if
 * the variable is not a bitmap array.
 * 
 * @param var The VarEntry that should contain a bitmap array.
 * @param allow_empty Whether to raise an error if the variable has not
 * yet been initialised.
 * @return Pointer to the variable or null if the variable is undefined 
 * and allow_empty was true.
 */
static BitmapArray *
GetBitmapArrayFromVar(VarEntry *var,
                      bool allow_empty)
{
    BitmapArray *bmarray;
    bmarray = (BitmapArray *) var->obj;

    if (bmarray) {
        if (bmarray->type != OBJ_BITMAP_ARRAY) {
            vl_type_mismatch(var->key, OBJ_BITMAP_ARRAY, bmarray->type);
        }
    }
    else {
        if (!allow_empty) {
            vl_type_mismatch(var->key, OBJ_BITMAP_ARRAY, OBJ_UNDEFINED);
        }
    }

    return bmarray;
}

/** 
 * Return the BitmapArray matching the name parameter, possibly creating
 * the (VarEntry) variable.  Raise an error if the named variable
 * already exists and is of the wrong type.
 * 
 * @param name The name of the variable.
 * @param allow_empty Whether to raise an error if the variable has not
 * been defined
 * @return Pointer to the variable or null if the variable does not
 * exist and create was false.
 */
static BitmapArray *
GetBitmapArray(char *name,
               bool allow_empty)
{
    VarEntry    *var;
    BitmapArray *bmarray;

    var = vl_lookup_variable(name);
    bmarray = GetBitmapArrayFromVar(var, allow_empty);

	return bmarray;
}

/** 
 * Return the BitmapHash from a bitmap hash variable.  This function
 * exists primarily to perform type checking, and to raise an error if
 * the variable is not a bitmap hash.
 * 
 * @param var The VarEntry that should contain a bitmap hash.
 * @param allow_empty Whether to raise an error if the variable has not
 * yet been initialised.
 * @return Pointer to the variable or null if the variable is undefined 
 * and allow_empty was true.
 */
static BitmapHash *
GetBitmapHashFromVar(VarEntry *var,
                      bool allow_empty)
{
    BitmapHash *bmhash;
    bmhash = (BitmapHash *) var->obj;

    if (bmhash) {
        if (bmhash->type != OBJ_BITMAP_HASH) {
            vl_type_mismatch(var->key, OBJ_BITMAP_HASH, bmhash->type);
        }
    }
    else {
        if (!allow_empty) {
            vl_type_mismatch(var->key, OBJ_BITMAP_HASH, OBJ_UNDEFINED);
        }
    }

    return bmhash;
}

/** 
 * Return the BitmapHash matching the name parameter, possibly creating
 * the VarEntry (variable) for it.  Raise an error if the named variable
 * already exists and is of the wrong type.
 * 
 * @param name The name of the variable.
 * @param allow_empty Whether to raise an error if the variable has not
 * been defined.
 * @return Pointer to the variable or null if the variable does not
 * exist and create was false.
 */
static BitmapHash *
GetBitmapHash(char *name,
              bool allow_empty)
{
    VarEntry   *var;
    BitmapHash *bmhash;
    
    var = vl_lookup_variable(name);
    bmhash = GetBitmapHashFromVar(var, allow_empty);

    return bmhash;
}

/** 
 * Return the Int4Array from an Int4Array variable.  This function
 * exists primarily to perform type checking, and to raise an error if
 * the variable is not an Int4Array.
 * 
 * @param var The VarEntry that should contain an Int4Array.
 * @param allow_empty Whether to raise an error if the variable has not
 * yet been initialised.
 * @return Pointer to the variable or null if the variable is undefined 
 * and allow_empty was true.
 */
static Int4Array *
GetInt4ArrayFromVar(VarEntry *var,
					bool allow_empty)
{
    Int4Array *array;
    array = (Int4Array *) var->obj;

    if (array) {
        if (array->type != OBJ_INT4_ARRAY) {
            vl_type_mismatch(var->key, OBJ_INT4_ARRAY, array->type);
        }
    }
    else {
        if (!allow_empty) {
            vl_type_mismatch(var->key, OBJ_INT4_ARRAY, OBJ_UNDEFINED);
        }
    }

    return array;
}

/** 
 * Return the Int4Array matching the name parameter, possibly creating
 * the VarEntry (variable) for it.  Raise an error if the named variable
 * already exists and is of the wrong type.
 * 
 * @param name The name of the variable.
 * @param allow_empty Whether to raise an error if the variable has not
 * been defined.
 * @return Pointer to the variable or null if the variable does not
 * exist and create was false.
 */
static Int4Array *
GetInt4Array(char *name,
			 bool allow_empty)
{
    VarEntry  *var;
    Int4Array *array;
    
    var = vl_lookup_variable(name);
    array = GetInt4ArrayFromVar(var, allow_empty);

    return array;
}

PG_FUNCTION_INFO_V1(veil_variables);
/** 
 * <code>veil_variables() returns setof veil_variable_t</code>
 * Return a <code>veil_variable_t</code> record for each defined
 * variable.  This includes both session and shared variables.
 *
 * @param fcinfo None
 * @return <code>setof veil_variable_t</code>
 */
Datum
veil_variables(PG_FUNCTION_ARGS)
{
    TupleDesc tupdesc;
    TupleTableSlot *slot;
    AttInMetadata *attinmeta;
    FuncCallContext *funcctx;

    veil_variable_t *var;
    char **values;
    HeapTuple tuple;
    Datum datum;

    if (SRF_IS_FIRSTCALL())
    {
        /* Only do this on first call for this result set */
        MemoryContext   oldcontext;

        ensure_init();
        funcctx = SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        tupdesc = RelationNameGetTupleDesc("veil.veil_variable_t");
        slot = TupleDescGetSlot(tupdesc);
        funcctx->slot = slot;
        attinmeta = TupleDescGetAttInMetadata(tupdesc);
        funcctx->attinmeta = attinmeta;

        MemoryContextSwitchTo(oldcontext);
		funcctx->user_fctx = NULL;
    }
    
    funcctx = SRF_PERCALL_SETUP();
    var = vl_next_variable(funcctx->user_fctx);
    funcctx->user_fctx = var;

    if (var) {
        values = (char **) palloc(3 * sizeof(char *));
        values[0] = copystr(var->name);
        values[1] = copystr(var->type);
        values[2] = strfrombool(var->shared);

        slot = funcctx->slot;
        attinmeta = funcctx->attinmeta;
        
        tuple = BuildTupleFromCStrings(attinmeta, values);
        datum = TupleGetDatum(slot, tuple);
        SRF_RETURN_NEXT(funcctx, datum);

    }
    else {
        SRF_RETURN_DONE(funcctx);
    }
}

PG_FUNCTION_INFO_V1(veil_share);
/** 
 * <code>veil_share(name text) returns bool</code>
 * Define a shared variable called NAME, returning true.  If the
 * variable is already defined as a session variable an ERROR will be
 * raised.
 *
 * Session variables are simply defined by their first usage.  Shared
 * variables must be defined using this function.  They may then be used
 * in exactly the same way as session variables.  Shared variables are
 * shared by all backends and so need only be initialised once.  The
 * result of this function tells the caller whether the variable needs
 * to be initialised.  The caller that first defines a shared variable
 * will get a false result and from this will know that the variable
 * must be initialised.  All subsequent callers will get a true result
 * and so will know that the variable is already initialised.
 * 
 * @param fcinfo <code>name text</code> name of variable.
 * @return <code>bool</code> true if the variable already exists
 */
Datum
veil_share(PG_FUNCTION_ARGS)
{
    char     *name;
    VarEntry *var;

    ensure_init();
    name = strfromtext(PG_GETARG_TEXT_P(0));

    var = vl_lookup_shared_variable(name);

    PG_RETURN_BOOL((var->obj != NULL));
}


PG_FUNCTION_INFO_V1(veil_init_range);
/** 
 * veil_init_range(name text, min int4, max int4) returns int4
 * Initialise a Range variable called NAME constrained by MIN and MAX,
 * returning the number of  elements in the range.  Ranges may be
 * examined using the veil_range() function.
 *
 * @param fcinfo <code>name text</code> The name of the variable to
 * initialise.
 * <br><code>min int4</code> The min value of the range.
 * <br><code>max int4</code> The max value of the range.
 * @return <code>int4</code> The size of the range ((max - min) + 1).
 */
Datum
veil_init_range(PG_FUNCTION_ARGS)
{
    int64     min;
    int64     max;
    Range    *range;
    char     *name;

	ensure_init();
    name = strfromtext(PG_GETARG_TEXT_P(0));
    min = PG_GETARG_INT32(1);
    max = PG_GETARG_INT32(2);

	range = GetRange(name, true);

    range->min = min;
    range->max = max;
    PG_RETURN_INT32(max + 1 - min);
}


/** 
 * Create a datum containing the values of a veil_range_t composite
 * type.
 * 
 * @param min Min value of range
 * @param max Max value of range
 * @return Composite (row) type datum containing the range elements.
 */
static Datum 
datum_from_range(int64 min, int64 max)
{
    static bool init_done = false;
    static TupleDesc tupdesc;
    static AttInMetadata *attinmeta;
    __attribute__ ((unused)) TupleTableSlot *slot;
    HeapTuple tuple;
    char **values;

    if (!init_done) {
        /* Keep all static data in top memory context where it will
         * safely remain during the session. */

        MemoryContext oldcontext = MemoryContextSwitchTo(TopMemoryContext);
        
        init_done = true;
        tupdesc = RelationNameGetTupleDesc("veil.veil_range_t");
        slot = TupleDescGetSlot(tupdesc);
        attinmeta = TupleDescGetAttInMetadata(tupdesc);
        
        MemoryContextSwitchTo(oldcontext);
    }

    /* Create value strings to be returned to caller. */
    values = (char **) palloc(2 * sizeof(char *));
    values[0] = strfromint(min);
    values[1] = strfromint(max);
    
    tuple = BuildTupleFromCStrings(attinmeta, values);
	slot = TupleDescGetSlot(tupdesc);

    /* make the tuple into a datum - this seems to use slot, so any
	 * compiler warning about slot being unused would seem to be
	 * spurious.  Most odd. */
    return TupleGetDatum(slot, tuple);
}


PG_FUNCTION_INFO_V1(veil_range);
/** 
 * <code>veil_range(name text) returns veil_range_t</code>
 * Return the range (as a SQL veil_range_t composite type) from the
 * named variable.
 * An Error will be raised if the variable is not defined or is of the
 * wrong type.
 *
 * @param fcinfo <code>name text</code> The name of the range variable.
 * @return <code>veil_range_t</code>  Composite type containing the min
 * and max values from the named variable.
 */
Datum
veil_range(PG_FUNCTION_ARGS)
{
    char  *name;
    Range *range;
	Datum  datum;

    ensure_init();

    name = strfromtext(PG_GETARG_TEXT_P(0));
    range = GetRange(name, false);
    
    datum = (datum_from_range(range->min, range->max));

	PG_RETURN_DATUM(datum);
}

PG_FUNCTION_INFO_V1(veil_init_bitmap);
/** 
 * <code>veil_init_bitmap(bitmap_name text, range_nametext) returns bool</code>
 * Create or re-initialise a Bitmap, for dealing with a named range of
 * values.
 * An error will be raised if the variable already exists and is not a
 * Bitmap.
 *
 * @param fcinfo <code>bitmap_name text</code> The name of the bitmap to
 * create or reset
 * <br><code>range_name text</code> The name of a Range variable that
 * defines the range of the new bitmap.
 * @return <code>bool</code> true
 */
Datum
veil_init_bitmap(PG_FUNCTION_ARGS)
{
    char     *bitmap_name;
    char     *range_name;
    Bitmap   *bitmap;
    VarEntry *bitmap_var;
    Range    *range;

    ensure_init();

    bitmap_name = strfromtext(PG_GETARG_TEXT_P(0));
    bitmap_var = vl_lookup_variable(bitmap_name);
    bitmap = GetBitmapFromVar(bitmap_var, true, false);
    range_name = strfromtext(PG_GETARG_TEXT_P(1));
    range = GetRange(range_name, false);

    vl_NewBitmap(&bitmap, bitmap_var->shared, range->min, range->max);

    bitmap_var->obj = (Object *) bitmap;

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_clear_bitmap);
/** 
 * <code>veil_clear_bitmap(name text) returns bool</code>
 * Clear all bits in the specified Bitmap.
 * An error will be raised if the variable is not a Bitmap or BitmapRef.
 *
 * @param fcinfo <code>name text</code> The name of the bitmap to
 * be cleared.
 * @return <code>bool</code> true
 */
Datum
veil_clear_bitmap(PG_FUNCTION_ARGS)
{
    char     *bitmap_name;
    VarEntry *bitmap_var;
    Bitmap   *bitmap;

    ensure_init();

    bitmap_name = strfromtext(PG_GETARG_TEXT_P(0));
    bitmap_var = vl_lookup_variable(bitmap_name);
    bitmap = GetBitmapFromVar(bitmap_var, false, true);

    vl_ClearBitmap(bitmap);

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_bitmap_setbit);
/** 
 * <code>veil_bitmap_setbit(name text, bit_number int4) returns bool</code>
 * Set the specified bit in the specified Bitmap.
 *
 * An error will be raised if the variable is not a Bitmap or BitmapRef.
 *
 * @param fcinfo <code>name text</code> The name of the bitmap variable.
 * <br><code>bit_number int4</code> The bit to be set.
 * @return <code>bool</code> true
 */
Datum
veil_bitmap_setbit(PG_FUNCTION_ARGS)
{
    char   *name;
    Bitmap *bitmap;
    int32   bit;

    ensure_init();

    name = strfromtext(PG_GETARG_TEXT_P(0));
    bit = PG_GETARG_INT32(1);
    bitmap = GetBitmap(name, false, true);
    vl_BitmapSetbit(bitmap, bit);

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_bitmap_clearbit);
/** 
 * <code>veil_bitmap_clearbit(name int4, bit_number text) returns bool</code>
 * Clear the specified bit in the specified Bitmap.
 *
 * An error will be raised if the variable is not a Bitmap or BitmapRef.
 *
 * @param fcinfo <code>name text</code> The name of the bitmap variable.
 * <br><code>bit_number int4</code> The bit to be cleared.
 * @return <code>bool</code> true
 */
Datum
veil_bitmap_clearbit(PG_FUNCTION_ARGS)
{
    char   *name;
    Bitmap *bitmap;
    int32   bit;

    ensure_init();

    name = strfromtext(PG_GETARG_TEXT_P(0));
    bit = PG_GETARG_INT32(1);
    bitmap = GetBitmap(name, false, true);
    vl_BitmapClearbit(bitmap, bit);

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_bitmap_testbit);
/** 
 * <code>veil_bitmap_testbit(name text, bit_number int4) returns bool</code>
 * Test the specified bit in the specified Bitmap, returning true if it
 * is set.
 *
 * An error will be raised if the variable is not a Bitmap or BitmapRef.
 *
 * @param fcinfo <code>name text</code> The name of the bitmap variable.
 * <br><code>bit_number int4</code> The bit to be tested.
 * @return <code>bool</code> true if the bit was set
 */
Datum
veil_bitmap_testbit(PG_FUNCTION_ARGS)
{
    char   *name;
    Bitmap *bitmap;
    int32   bit;
    bool    result;

    ensure_init();

    bit = PG_GETARG_INT32(1);
    name = strfromtext(PG_GETARG_TEXT_P(0));
    bitmap = GetBitmap(name, false, true);

    result = vl_BitmapTestbit(bitmap, bit);
    PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(veil_bitmap_union);
/** 
 * <code>veil_bitmap_union(result_name text, name2 text) returns bool</code>
 * Union the bitmap specified in parameter 1 with that in parameter 2,
 * with the result in parameter 1.
 *
 * An error will be raised if the variables are not of type Bitmap or
 * BitmapRef.
 *
 * @param fcinfo <code>result_name text</code> The target bitmap
 * <br><code>name2 text</code> The bitmap with which to union the target
 * @return <code>bool</code> true 
 */
Datum
veil_bitmap_union(PG_FUNCTION_ARGS)
{
    char        *bitmap1_name;
    char        *bitmap2_name;
    Bitmap      *target;
    Bitmap      *source;

    ensure_init();

    bitmap1_name = strfromtext(PG_GETARG_TEXT_P(0));
    bitmap2_name = strfromtext(PG_GETARG_TEXT_P(1));
    target = GetBitmap(bitmap1_name, false, true);
    source = GetBitmap(bitmap2_name, false, true);

	if (target && source) {
		vl_BitmapUnion(target, source);
	}

    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_bitmap_intersect);
/** 
 * <code>veil_bitmap_intersect(result_name text, name2 text) returns bool</code>
 * Intersect the bitmap specified in parameter 1 with that in parameter 2,
 * with the result in parameter 1.
 *
 * An error will be raised if the variables are not of type Bitmap or
 * BitmapRef.
 *
 * @param fcinfo <code>result_name text</code> The target bitmap
 * <br><code>name2 text</code> The bitmap with which to intersect the target
 * @return <code>bool</code> true 
 */
Datum
veil_bitmap_intersect(PG_FUNCTION_ARGS)
{
    char        *bitmap1_name;
    char        *bitmap2_name;
    Bitmap      *target;
    Bitmap      *source;

    ensure_init();

    bitmap1_name = strfromtext(PG_GETARG_TEXT_P(0));
    bitmap2_name = strfromtext(PG_GETARG_TEXT_P(1));
    target = GetBitmap(bitmap1_name, false, true);
    source = GetBitmap(bitmap2_name, false, true);

	vl_BitmapIntersect(target, source);
    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_bitmap_bits);
/** 
 * <code>veil_bitmap_bits(name text)</code> returns setof int4
 * Return the set of all bits set in the specified Bitmap or BitmapRef.
 *
 * @param fcinfo <code>name text</code> The name of the bitmap.
 * @return <code>setof int4</code>The set of bits that are set in the
 * bitmap.
 */
Datum
veil_bitmap_bits(PG_FUNCTION_ARGS)
{
	struct bitmap_bits_state {
		Bitmap *bitmap;
		int32   bit;
	} *state;
    FuncCallContext *funcctx;
	MemoryContext    oldcontext;
    char  *name;
    bool   found;
    Datum  datum;
    
    if (SRF_IS_FIRSTCALL())
    {
        /* Only do this on first call for this result set */
        ensure_init();

        funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		state = palloc(sizeof(struct bitmap_bits_state));
        MemoryContextSwitchTo(oldcontext);

        name = strfromtext(PG_GETARG_TEXT_P(0));
        state->bitmap = GetBitmap(name, false, true);

        if (!state->bitmap) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Bitmap %s is not defined",
							name),
					 errhint("Perhaps the name is mis-spelled, or its "
							 "definition is missing from veil_init().")));
        }

        state->bit = state->bitmap->bitzero;
		funcctx->user_fctx = state;
    }
    
    funcctx = SRF_PERCALL_SETUP();
	state = funcctx->user_fctx;
    
    state->bit = vl_BitmapNextBit(state->bitmap, state->bit, &found);
    
    if (found) {
        datum = Int32GetDatum(state->bit);
        state->bit++;
        SRF_RETURN_NEXT(funcctx, datum);
    }
    else {
        SRF_RETURN_DONE(funcctx);
    }
}

PG_FUNCTION_INFO_V1(veil_bitmap_range);
/** 
 * <code>veil_bitmap_range(name text) returns veil_range_t</code>
 * Return composite type giving the range of the specified Bitmap or
 * BitmapRef.
 *
 * @param fcinfo <code>name text</code> The name of the bitmap.
 * @return <code>veil_range_t</code>  Composite type containing the min
 * and max values of the bitmap's range
 */
Datum
veil_bitmap_range(PG_FUNCTION_ARGS)
{
    char   *name;
    Bitmap *bitmap;

    ensure_init();

    name = strfromtext(PG_GETARG_TEXT_P(0));
    bitmap = GetBitmap(name, false, true);

    if (!bitmap) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Bitmap %s is not defined",	name),
				 errhint("Perhaps the name is mis-spelled, or its "
						 "definition is missing from veil_init().")));
    }

    PG_RETURN_DATUM(datum_from_range(bitmap->bitzero, bitmap->bitmax));
}


PG_FUNCTION_INFO_V1(veil_init_bitmap_array);
/** 
 * <code>veil_init_bitmap_array(text, text, text) returns bool</code>
 * Create or reset a BitmapArray.
 * An error will be raised if any parameter is not of the correct type.
 *
 * @param fcinfo <code>bmarray text</code> The name of the bitmap array.
 * <br><code>array_range text</code> Name of the Range variable that
 * provides the range of the array part of the bitmap array.
 * <br><code>bitmap_range text</code> Name of the Range variable that
 * provides the range of each bitmap in the array.
 * @return <code>bool</code>  True
 */
Datum
veil_init_bitmap_array(PG_FUNCTION_ARGS)
{
    char        *bmarray_name;
    char        *arrayrange_name;
    char        *maprange_name;
    VarEntry    *bmarray_var;
    BitmapArray *bmarray;
    Range       *arrayrange;
    Range       *maprange;

    ensure_init();

    bmarray_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray_var = vl_lookup_variable(bmarray_name);
    bmarray = GetBitmapArrayFromVar(bmarray_var, true);

    arrayrange_name = strfromtext(PG_GETARG_TEXT_P(1));
    arrayrange = GetRange(arrayrange_name, false);
    maprange_name = strfromtext(PG_GETARG_TEXT_P(2));
    maprange = GetRange(maprange_name, false);

    vl_NewBitmapArray(&bmarray, bmarray_var->shared, 
					  arrayrange->min, arrayrange->max,
					  maprange->min, maprange->max);

    bmarray_var->obj = (Object *) bmarray;

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_clear_bitmap_array);
/** 
 * <code>veil_clear_bitmap_array(bmarray text) returns bool</code>
 * Clear the bits in an existing BitmapArray.
 * An error will be raised if the parameter is not of the correct type.
 *
 * @param fcinfo <code>bmarray text</code> The name of the BitmapArray.
 * @return <code>bool</code>  True
 */
Datum
veil_clear_bitmap_array(PG_FUNCTION_ARGS)
{
    char        *bmarray_name;
    VarEntry    *bmarray_var;
    BitmapArray *bmarray;

    ensure_init();

    bmarray_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray_var = vl_lookup_variable(bmarray_name);
    bmarray = GetBitmapArrayFromVar(bmarray_var, false);

    vl_ClearBitmapArray(bmarray);

    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_bitmap_from_array);
/** 
 * <code>veil_bitmap_from_array(bmref text, bmarray text, index int4) returns text</code>
 * Place a reference to the specified Bitmap from a BitmapArray into
 * the specified BitmapRef
 * An error will be raised if any parameter is not of the correct type.
 *
 * @param fcinfo <code>bmref text</code> The name of the BitmapRef into which
 * a reference to the relevant Bitmap will be placed.
 * <br><code>bmarray text</code> Name of the BitmapArray containing the Bitmap
 * in which we are interested.
 * <br><code>index int4</code> Index into the array of the bitmap in question.
 * @return <code>text</code>  The name of the BitmapRef
 */
Datum
veil_bitmap_from_array(PG_FUNCTION_ARGS)
{
	text        *bmref_text;
	char        *bmref_name;
	BitmapRef   *bmref;
	char        *bmarray_name;
    BitmapArray *bmarray;
    int32        arrayelem;
	Bitmap      *bitmap;

	bmref_text = PG_GETARG_TEXT_P(0);
    bmref_name = strfromtext(bmref_text);
    bmref = GetBitmapRef(bmref_name);

    bmarray_name = strfromtext(PG_GETARG_TEXT_P(1));
    bmarray = GetBitmapArray(bmarray_name, false);

	arrayelem = PG_GETARG_INT32(2);
    bitmap = vl_BitmapFromArray(bmarray, arrayelem);
	if (!bitmap) {
		ereport(ERROR,
			(errcode(ERRCODE_INTERNAL_ERROR),
			 errmsg("Range error for BitmapArray %s, %d not in %d - %d",
					bmarray_name, arrayelem,
					bmarray->arrayzero, bmarray->arraymax)));
	}

	bmref->bitmap = bitmap;
	bmref->xid = GetCurrentTransactionId();
    PG_RETURN_TEXT_P(bmref_text);
}

PG_FUNCTION_INFO_V1(veil_bitmap_array_testbit);
/** 
 * <code>veil_bitmap_array_testbit(bmarray text, arr_idx int4, bitno int4) returns bool</code>
 * Test a specified bit within a BitmapArray
 *
 * An error will be raised if the first parameter is not a BitmapArray.
 *
 * @param fcinfo <code>bmarray text</code> The name of the BitmapArray
 * <br><code>arr_idx int4</code> Index of the Bitmap within the array.
 * <br><code>bitno int4</code> Bit id of the bit within the Bitmap.
 * @return <code>bool</code>  True if the bit was set, false otherwise.
 */
Datum
veil_bitmap_array_testbit(PG_FUNCTION_ARGS)
{
    char        *name;
    BitmapArray *bmarray;
    Bitmap      *bitmap;
    int32        arrayelem;
    int32        bit;

    ensure_init();

    arrayelem = PG_GETARG_INT32(1);
    bit = PG_GETARG_INT32(2);

    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray = GetBitmapArray(name, false);
    
    bitmap = vl_BitmapFromArray(bmarray, arrayelem);
    if (bitmap) {
        PG_RETURN_BOOL(vl_BitmapTestbit(bitmap, bit));
    }
    else {
        PG_RETURN_BOOL(false);
    }
}


PG_FUNCTION_INFO_V1(veil_bitmap_array_setbit);
/** 
 * <code>veil_bitmap_array_setbit(bmarray text, arr_idx int4, bitno int4) returns bool</code>
 * Set a specified bit within a BitmapArray
 *
 * An error will be raised if the first parameter is not a BitmapArray.
 *
 * @param fcinfo <code>bmarray text</code> The name of the BitmapArray
 * <br><code>arr_idx int4</code> Index of the Bitmap within the array.
 * <br><code>ibitno nt4</code> Bit id of the bit within the Bitmap.
 * @return <code>bool</code>  True
 */
Datum
veil_bitmap_array_setbit(PG_FUNCTION_ARGS)
{
    char        *name;
    BitmapArray *bmarray;
    Bitmap      *bitmap;
    int32        arrayelem;
    int32        bit;

    ensure_init();

    arrayelem = PG_GETARG_INT32(1);
    bit = PG_GETARG_INT32(2);
    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray = GetBitmapArray(name, false);

    bitmap = vl_BitmapFromArray(bmarray, arrayelem);
    if (bitmap) {
		vl_BitmapSetbit(bitmap, bit);
        PG_RETURN_BOOL(true);
    }
    else {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Bitmap Array range error (%d not in %d..%d)", 
						arrayelem, bmarray->arrayzero, bmarray->arraymax),
				 errdetail("Attempt to reference BitmapArray element "
						   "outside of the BitmapArray's defined range")));
    }
	PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_bitmap_array_clearbit);
/** 
 * <code>veil_bitmap_array_clearbit(bmarray text, arr_idx int4, bitno int4) returns bool</code>
 * Clear a specified bit within a BitmapArray
 *
 * An error will be raised if the first parameter is not a BitmapArray.
 *
 * @param fcinfo <code>bmarray text</code> The name of the BitmapArray
 * <br><code>arr_idx int4</code> Index of the Bitmap within the array.
 * <br><code>bitno int4</code> Bit id of the bit within the Bitmap.
 * @return <code>bool</code>  True
 */
Datum
veil_bitmap_array_clearbit(PG_FUNCTION_ARGS)
{
    char        *name;
    BitmapArray *bmarray;
    Bitmap      *bitmap;
    int32        arrayelem;
    int32        bit;

    ensure_init();

    arrayelem = PG_GETARG_INT32(1);
    bit = PG_GETARG_INT32(2);
    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray = GetBitmapArray(name, false);

    bitmap = vl_BitmapFromArray(bmarray, arrayelem);
    if (bitmap) {
		vl_BitmapClearbit(bitmap, bit);
        PG_RETURN_BOOL(true);
    }
    else {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Bitmap Array range error (%d not in %d..%d)", 
						arrayelem, bmarray->arrayzero, bmarray->arraymax),
				 errdetail("Attempt to reference BitmapArray element "
						   "outside of the BitmapArray's defined range")));
    }
	PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_union_from_bitmap_array);
/** 
 * <code>veil_union_from_bitmap_array(bitmap text, bmarray text, arr_idx int4) returns bool</code>
 * Union a Bitmap with the specified Bitmap from a BitmapArray with the
 * result placed into the first parameter.
 *
 * An error will be raised if the parameters are not of the correct types.
 *
 * @param fcinfo <code>bitmap text</code> The name of the Bitmap into which the
 * resulting union will be placed.
 * <br><code>bmarray text</code> Name of the BitmapArray
 * <br><code>arr_idx int4</code> Index of the required bitmap in the array
 * @return <code>bool</code>  True
 */
Datum
veil_union_from_bitmap_array(PG_FUNCTION_ARGS)
{
    char        *bitmap_name;
    char        *bmarray_name;
    Bitmap      *target;
    BitmapArray *bmarray;
    Bitmap      *bitmap;
    int32        arrayelem;

    ensure_init();

    arrayelem = PG_GETARG_INT32(2);

    bitmap_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray_name = strfromtext(PG_GETARG_TEXT_P(1));
    target = GetBitmap(bitmap_name, false, true);
    bmarray = GetBitmapArray(bmarray_name, false);

    bitmap = vl_BitmapFromArray(bmarray, arrayelem);
    if (bitmap) {
        vl_BitmapUnion(target, bitmap);
    }
    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_intersect_from_bitmap_array);
/** 
 * <code>veil_intersect_from_bitmap_array(bitmap text, bmarray text, arr_idx int4) returns bool</code>
 * Intersect a Bitmap with the specified Bitmap from a BitmapArray with the
 * result placed into the first parameter.
 *
 * An error will be raised if the parameters are not of the correct types.
 *
 * @param fcinfo <code>bitmap text</code> The name of the Bitmap into which the
 * resulting intersection will be placed.
 * <br><code>bmarray text</code> Name of the BitmapArray
 * <br><code>arr_idx int4</code> Index of the required bitmap in the array
 * @return <code>bool</code>  True
 */
Datum
veil_intersect_from_bitmap_array(PG_FUNCTION_ARGS)
{
    char        *bitmap_name;
    char        *bmarray_name;
    Bitmap      *target;
    BitmapArray *bmarray;
    Bitmap      *bitmap;
    int32        arrayelem;

    ensure_init();

    arrayelem = PG_GETARG_INT32(2);

    bitmap_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray_name = strfromtext(PG_GETARG_TEXT_P(1));
    target = GetBitmap(bitmap_name, false, true);
    bmarray = GetBitmapArray(bmarray_name, false);

    bitmap = vl_BitmapFromArray(bmarray, arrayelem);
    if (bitmap) {
        vl_BitmapIntersect(target, bitmap);
    }
    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_bitmap_array_bits);
/** 
 * <code>veil_bitmap_array_bits(bmarray text, arr_idx int4)</code> returns setof int4
 * Return the set of all bits set in the specified Bitmap from the
 * BitmapArray.
 *
 * @param fcinfo <code>bmarray text</code> The name of the bitmap array.
 * <br><code>arr_idx int4</code> Index of the required bitmap in the array
 * @return <code>setof int4</code>The set of bits that are set in the
 * bitmap.
 */
Datum
veil_bitmap_array_bits(PG_FUNCTION_ARGS)
{
	struct bitmap_array_bits_state {
		Bitmap *bitmap;
		int32   bit;
	} *state;
    FuncCallContext *funcctx;
	MemoryContext    oldcontext;
    char   *name;
    bool    found;
    BitmapArray *bmarray;
    int     arrayelem;
    Datum   datum;
    
    if (SRF_IS_FIRSTCALL())
    {
        /* Only do this on first call for this result set */
        ensure_init();
        
        funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		state = palloc(sizeof(struct bitmap_array_bits_state));
        MemoryContextSwitchTo(oldcontext);

        name = strfromtext(PG_GETARG_TEXT_P(0));
        arrayelem = PG_GETARG_INT32(1);
        bmarray = GetBitmapArray(name, false);

        if (!bmarray) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("BitmapArray %s is not defined",
							name),
					 errhint("Perhaps the name is mis-spelled, or its "
							 "definition is missing from "
							 "veil_init().")));
        }

        state->bitmap = vl_BitmapFromArray(bmarray, arrayelem);
        if (!state->bitmap) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Bitmap Array range error (%d not in %d..%d)", 
							arrayelem, bmarray->arrayzero, bmarray->arraymax),
					 errdetail("Attempt to reference BitmapArray element "
							   "outside of the BitmapArray's defined range")));
        }

        state->bit = state->bitmap->bitzero;
		funcctx->user_fctx = state;
    }
    
    funcctx = SRF_PERCALL_SETUP();
	state = funcctx->user_fctx;

    state->bit = vl_BitmapNextBit(state->bitmap, state->bit, &found);
    
    if (found) {
        datum = Int32GetDatum(state->bit);
        state->bit++;
        SRF_RETURN_NEXT(funcctx, datum);
    }
    else {
        SRF_RETURN_DONE(funcctx);
    }
}

PG_FUNCTION_INFO_V1(veil_bitmap_array_arange);
/** 
 * <code>veil_bitmap_array_arange(bmarray text) returns veil_range_t</code>
 * Return composite type giving the range of the array part of the
 * specified BitmapArray
 *
 * @param fcinfo <code>bmarray text</code> The name of the bitmap array.
 * @return <code>veil_range_t</code>  Composite type containing the min
 * and max indices of the array
 */
Datum
veil_bitmap_array_arange(PG_FUNCTION_ARGS)
{
    char        *name;
    BitmapArray *bmarray;

    ensure_init();

    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray = GetBitmapArray(name, false);

    if (!bmarray) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("BitmapArray %s is not defined",
						name),
				 errhint("Perhaps the name is mis-spelled, or its "
						 "definition is missing from veil_init().")));
    }

    PG_RETURN_DATUM(datum_from_range(bmarray->arrayzero, bmarray->arraymax));
}


PG_FUNCTION_INFO_V1(veil_bitmap_array_brange);
/** 
 * <code>veil_bitmap_array_brange(bmarray text) returns veil_range_t</code>
 * Return composite type giving the range of every Bitmap within
 * the BitmapArray.
 *
 * @param fcinfo <code>bmarray text</code> The name of the bitmap array.
 * @return <code>veil_range_t</code>  Composite type containing the min
 * and max values of the bitmap array's range
 */
Datum
veil_bitmap_array_brange(PG_FUNCTION_ARGS)
{
    char        *name;
    BitmapArray *bmarray;

    ensure_init();

    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmarray = GetBitmapArray(name, false);

    if (!bmarray) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("BitmapArray %s is not defined",
						name),
				 errhint("Perhaps the name is mis-spelled, or its "
						 "definition is missing from veil_init().")));
    }

    PG_RETURN_DATUM(datum_from_range(bmarray->bitzero, bmarray->bitmax));
}



PG_FUNCTION_INFO_V1(veil_init_bitmap_hash);
/** 
 * <code>veil_init_bitmap_hash(bmhash text, range text) returns bool</code>
 * Create or reset a BitmapHash.
 * An error will be raised if any parameter is not of the correct type.
 *
 * @param fcinfo <code>bmhash text</code> The name of the bitmap hash.
 * <br><code>range text</code> Name of the Range variable that provides the
 * range of each bitmap in the hash.
 * @return <code>bool</code>  True
 */
Datum
veil_init_bitmap_hash(PG_FUNCTION_ARGS)
{
    char       *bmhash_name;
    char       *range_name;
    VarEntry   *bmhash_var;
    BitmapHash *bmhash;
    Range      *range;

    ensure_init();

    bmhash_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash_var = vl_lookup_variable(bmhash_name);
    bmhash = GetBitmapHashFromVar(bmhash_var, true);

    range_name = strfromtext(PG_GETARG_TEXT_P(1));
    range = GetRange(range_name, false);

	if (bmhash_var->shared) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("illegal attempt to define shared BitmapHash %s",
						bmhash_name),
				 errhint("BitmapHashes may only be defined as session, "
						 "not shared, variables.")));
	}
    vl_NewBitmapHash(&bmhash, bmhash_name,
					 range->min, range->max);

    bmhash_var->obj = (Object *) bmhash;

    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_clear_bitmap_hash);
/** 
 * <code>veil_clear_bitmap_hash(bmhash text) returns bool</code>
 * Clear the bits in an existing BitmapHash.
 * An error will be raised if the parameter is not of the correct type.
 *
 * @param fcinfo <code>bmhash text</code> The name of the BitmapHash.
 * @return <code>bool</code>  True
 */
Datum
veil_clear_bitmap_hash(PG_FUNCTION_ARGS)
{
    char       *bmhash_name;
    VarEntry   *bmhash_var;
    BitmapHash *bmhash;

    ensure_init();

    bmhash_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash_var = vl_lookup_variable(bmhash_name);
    bmhash = GetBitmapHashFromVar(bmhash_var, true);

    vl_NewBitmapHash(&bmhash, bmhash_name,
					 bmhash->bitzero, bmhash->bitmax);

    bmhash_var->obj = (Object *) bmhash;

    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_bitmap_hash_key_exists);
/** 
 * <code>veil_bitmap_hashkey_exists(bmhash text, key text) returns bool</code>
 * Return true if the key exists in the bitmap hash.
 *
 * @param fcinfo <code>bmhash text</code> Name of the BitmapHashin which we are
 * interested.
 * <br><code>key text</code> Key, into the hash, of the bitmap in question.
 * @return <code>boolean</code>  Whether the key is present in the BitmapHash
 */
Datum
veil_bitmap_hash_key_exists(PG_FUNCTION_ARGS)
{
	char       *bmhash_name;
    BitmapHash *bmhash;
    char       *hashelem;
	bool        found;

    bmhash_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash = GetBitmapHash(bmhash_name, false);

	hashelem = strfromtext(PG_GETARG_TEXT_P(1));
	
    found = vl_BitmapHashHasKey(bmhash, hashelem);
	
    PG_RETURN_BOOL(found);
}

PG_FUNCTION_INFO_V1(veil_bitmap_from_hash);
/** 
 * <code>veil_bitmap_from_hash(bmref text, bmhash text, key text) returns text</code>
 * Place a reference to the specified Bitmap from a BitmapHash into
 * the specified BitmapRef
 * An error will be raised if any parameter is not of the correct type.
 *
 * @param fcinfo <code>bmref text</code> The name of the BitmapRef into which
 * a reference to the relevant Bitmap will be placed.
 * <br><code>bmhash text</code> Name of the BitmapHash containing the Bitmap
 * in which we are interested.
 * <br><code>key text</code> Key, into the hash, of the bitmap in question.
 * @return <code>text</code>  The name of the BitmapRef
 */
Datum
veil_bitmap_from_hash(PG_FUNCTION_ARGS)
{
	text       *bmref_text;
	char       *bmref_name;
	BitmapRef  *bmref;
	char       *bmhash_name;
    BitmapHash *bmhash;
    char       *hashelem;
	Bitmap     *bitmap;

	bmref_text = PG_GETARG_TEXT_P(0);
    bmref_name = strfromtext(bmref_text);
    bmref = GetBitmapRef(bmref_name);

    bmhash_name = strfromtext(PG_GETARG_TEXT_P(1));
    bmhash = GetBitmapHash(bmhash_name, false);

	hashelem = strfromtext(PG_GETARG_TEXT_P(2));
    bitmap = vl_AddBitmapToHash(bmhash, hashelem);
	
	bmref->bitmap = bitmap;
	bmref->xid = GetCurrentTransactionId();
    PG_RETURN_TEXT_P(bmref_text);
}


PG_FUNCTION_INFO_V1(veil_bitmap_hash_testbit);
/** 
 * <code>veil_bitmap_hash_testbit(bmhash text, key text, bitno int4) returns bool</code>
 * Test a specified bit within a BitmapHash
 *
 * An error will be raised if the first parameter is not a BitmapHash.
 *
 * @param fcinfo <code>bmhash text</code> The name of the BitmapHash
 * <br><code>key text</code> Key of the Bitmap within the hash.
 * <br><code>bitno int4</code> Bit id of the bit within the Bitmap.
 * @return <code>bool</code>  True if the bit was set, false otherwise.
 */
Datum
veil_bitmap_hash_testbit(PG_FUNCTION_ARGS)
{
    char       *name;
    BitmapHash *bmhash;
    char       *hashelem;
    Bitmap     *bitmap;
    int32       bit;

    ensure_init();

    hashelem = strfromtext(PG_GETARG_TEXT_P(1));
    bit = PG_GETARG_INT32(2);

    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash = GetBitmapHash(name, false);
    
    bitmap = vl_BitmapFromHash(bmhash, hashelem);
    if (bitmap) {
        PG_RETURN_BOOL(vl_BitmapTestbit(bitmap, bit));
    }
    else {
        PG_RETURN_BOOL(false);
    }
}


PG_FUNCTION_INFO_V1(veil_bitmap_hash_setbit);
/** 
 * <code>veil_bitmap_hash_setbit(bmhash text, key text, bitno int4) returns bool</code>
 * Set a specified bit within a BitmapHash
 *
 * An error will be raised if the first parameter is not a BitmapHash.
 *
 * @param fcinfo <code>bmhash text</code> The name of the BitmapHash
 * <br><code>key text</code> Key of the Bitmap within the hash.
 * <br><code>bitno int4</code> Bit id of the bit within the Bitmap.
 * @return <code>bool</code>  True
 */
Datum
veil_bitmap_hash_setbit(PG_FUNCTION_ARGS)
{
    char       *name;
    BitmapHash *bmhash;
    Bitmap     *bitmap;
    char       *hashelem;
    int32       bit;

    ensure_init();

    hashelem = strfromtext(PG_GETARG_TEXT_P(1));
    bit = PG_GETARG_INT32(2);
    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash = GetBitmapHash(name, false);

    bitmap = vl_AddBitmapToHash(bmhash, hashelem);

	vl_BitmapSetbit(bitmap, bit);
    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_bitmap_hash_clearbit);
/** 
 * <code>veil_bitmap_hash_clearbit(bmhash text, key text, bitno int4) returns bool</code>
 * Clear a specified bit within a BitmapHash
 *
 * An error will be raised if the first parameter is not a BitmapHash.
 *
 * @param fcinfo <code>bmhash text</code> The name of the BitmapHash
 * <br><code>key text</code> Key of the Bitmap within the hash.
 * <br><code>bitno int4</code> Bit id of the bit within the Bitmap.
 * @return <code>bool</code>  True
 */
Datum
veil_bitmap_hash_clearbit(PG_FUNCTION_ARGS)
{
    char       *name;
    BitmapHash *bmhash;
    Bitmap     *bitmap;
    char       *hashelem;
    int32       bit;

    ensure_init();

    hashelem = strfromtext(PG_GETARG_TEXT_P(1));
    bit = PG_GETARG_INT32(2);
    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash = GetBitmapHash(name, false);

    bitmap = vl_AddBitmapToHash(bmhash, hashelem);

	vl_BitmapClearbit(bitmap, bit);
    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_union_into_bitmap_hash);
/** 
 * <code>veil_union_into_bitmap_hash(bmhash text, key text, bitmap text) returns bool</code>
 * Union a Bitmap with the specified Bitmap from a BitmapHash with the
 * result placed into the bitmap hash.
 *
 * An error will be raised if the parameters are not of the correct types.
 *
 * @param fcinfo <code>bmhash text</code> Name of the BitmapHash
 * <br><code>key text</code> Key of the required bitmap in the hash
 * <br><code>bitmap text</code> The name of the Bitmap into which the
 * resulting union will be placed.
 * @return <code>bool</code>  True
 */
Datum
veil_union_into_bitmap_hash(PG_FUNCTION_ARGS)
{
    char       *bitmap_name;
    char       *bmhash_name;
    Bitmap     *target;
    BitmapHash *bmhash;
    Bitmap     *bitmap;
    char       *hashelem;

    ensure_init();

    bmhash_name = strfromtext(PG_GETARG_TEXT_P(0));
    hashelem = strfromtext(PG_GETARG_TEXT_P(1));

    bitmap_name = strfromtext(PG_GETARG_TEXT_P(2));
    bitmap = GetBitmap(bitmap_name, false, true);
    bmhash = GetBitmapHash(bmhash_name, false);

    target = vl_AddBitmapToHash(bmhash, hashelem);
    if (target && bitmap) {
        vl_BitmapUnion(target, bitmap);
    }
    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_union_from_bitmap_hash);
/** 
 * <code>veil_union_from_bitmap_hash(bmhash text, key text, bitmap text) returns bool</code>
 * Union a Bitmap with the specified Bitmap from a BitmapHash with the
 * result placed into the bitmap parameter.
 *
 * An error will be raised if the parameters are not of the correct types.
 *
 * @param fcinfo <code>bmhash text</code> The name of the Bitmap into which the
 * resulting union will be placed.
 * <br><code>key text</code> Name of the BitmapHash
 * <br><code>bitmap text</code> Key of the required bitmap in the hash
 * @return <code>bool</code>  True
 */
Datum
veil_union_from_bitmap_hash(PG_FUNCTION_ARGS)
{
    char       *bitmap_name;
    char       *bmhash_name;
    Bitmap     *target;
    BitmapHash *bmhash;
    Bitmap     *bitmap;
    char       *hashelem;

    ensure_init();

    hashelem = strfromtext(PG_GETARG_TEXT_P(2));

    bitmap_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash_name = strfromtext(PG_GETARG_TEXT_P(1));
    target = GetBitmap(bitmap_name, false, true);
    bmhash = GetBitmapHash(bmhash_name, false);

    bitmap = vl_BitmapFromHash(bmhash, hashelem);
    if (bitmap) {
        vl_BitmapUnion(target, bitmap);
    }
    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_intersect_from_bitmap_hash);
/** 
 * <code>veil_intersect_from_bitmap_hash(bitmap text, bmhash text, key text) returns bool</code>
 * Intersect a Bitmap with the specified Bitmap from a BitmapArray with the
 * result placed into the bitmap parameter.
 *
 * An error will be raised if the parameters are not of the correct types.
 *
 * @param fcinfo <code>bitmap text</code> The name of the Bitmap into which the
 * resulting intersection will be placed.
 * <br><code>bmhash text</code> Name of the BitmapHash
 * <br><code>key text</code> Key of the required bitmap in the hash
 * @return <code>bool</code>  True
 */
Datum
veil_intersect_from_bitmap_hash(PG_FUNCTION_ARGS)
{
    char       *bitmap_name;
    char       *bmhash_name;
    Bitmap     *target;
    BitmapHash *bmhash;
    Bitmap     *bitmap;
    char       *hashelem;

    ensure_init();

    hashelem = strfromtext(PG_GETARG_TEXT_P(2));

    bitmap_name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash_name = strfromtext(PG_GETARG_TEXT_P(1));
    target = GetBitmap(bitmap_name, false, true);
    bmhash = GetBitmapHash(bmhash_name, false);

    bitmap = vl_BitmapFromHash(bmhash, hashelem);
    if (bitmap) {
        vl_BitmapIntersect(target, bitmap);
    }
	else {
		/* The bitmap from the hash does not exist, so it is logically
		 * empty.  Intersection with an empty set yields an empty set. */
		vl_ClearBitmap(target);
	}
    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_bitmap_hash_bits);
/** 
 * <code>veil_bitmap_hash_bits(bmhash text, key text)</code> returns setof int4
 * Return the set of all bits set in the specified Bitmap from the
 * BitmapHash.
 *
 * @param fcinfo <code>bmhashtext</code> The name of the bitmap hash.
 * <br><code>key text</code> Key of the required bitmap in the hash
 * @return <code>setof int4</code>The set of bits that are set in the
 * bitmap.
 */
Datum
veil_bitmap_hash_bits(PG_FUNCTION_ARGS)
{
	struct bitmap_hash_bits_state {
		Bitmap *bitmap;
		int32   bit;
	} *state;
    FuncCallContext *funcctx;
	MemoryContext    oldcontext;
    char   *name;
    bool    found;
    BitmapHash *bmhash;
    char   *hashelem;
    Datum   datum;
    
    if (SRF_IS_FIRSTCALL())
    {
        /* Only do this on first call for this result set */
        ensure_init();
        
        funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		state = palloc(sizeof(struct bitmap_hash_bits_state));
        MemoryContextSwitchTo(oldcontext);

        name = strfromtext(PG_GETARG_TEXT_P(0));
        hashelem = strfromtext(PG_GETARG_TEXT_P(1));
        bmhash = GetBitmapHash(name, false);

        if (!bmhash) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Bitmap Hash %s not defined", name)));
        }

        state->bitmap = vl_BitmapFromHash(bmhash, hashelem);
        if (!state->bitmap) {
			SRF_RETURN_DONE(funcctx);
        }

        state->bit = state->bitmap->bitzero;
		funcctx->user_fctx = state;
    }
    
    funcctx = SRF_PERCALL_SETUP();
	state = funcctx->user_fctx;

    state->bit = vl_BitmapNextBit(state->bitmap, state->bit, &found);
    
    if (found) {
        datum = Int32GetDatum(state->bit);
        state->bit++;
        SRF_RETURN_NEXT(funcctx, datum);
    }
    else {
        SRF_RETURN_DONE(funcctx);
    }
}

PG_FUNCTION_INFO_V1(veil_bitmap_hash_range);
/** 
 * <code>veil_bitmap_hash_range(bmhash text) returns veil_range_t</code>
 * Return composite type giving the range of every Bitmap within
 * the BitmapHash.
 *
 * @param fcinfo <code>bmhash text</code> The name of the bitmap array.
 * @return <code>veil_range_t</code>  Composite type containing the min
 * and max values of the bitmap hash's range
 */
Datum
veil_bitmap_hash_range(PG_FUNCTION_ARGS)
{
    char       *name;
    BitmapHash *bmhash;

    ensure_init();

    name = strfromtext(PG_GETARG_TEXT_P(0));
    bmhash = GetBitmapHash(name, false);

    if (!bmhash) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Bitmap Hash %s not defined", name)));
    }

    PG_RETURN_DATUM(datum_from_range(bmhash->bitzero, bmhash->bitmax));
}


PG_FUNCTION_INFO_V1(veil_bitmap_hash_entries);
/** 
 * <code>veil_bitmap_hash_entries(bmhash text) returns setof text</code>
 * Return  the key of every Bitmap within the BitmapHash.
 *
 * @param fcinfo <code>bmhash text</code> The name of the bitmap hash.
 * @return <code>setof text</code>  Every key in the hash.
 */
Datum
veil_bitmap_hash_entries(PG_FUNCTION_ARGS)
{
	struct bitmap_hash_entries_state {
		BitmapHash *bmhash;
		VarEntry   *var;
	} *state;
    FuncCallContext *funcctx;
	MemoryContext    oldcontext;
    char  *name;
    Datum  datum;
    text  *result;

    if (SRF_IS_FIRSTCALL())
    {
        /* Only do this on first call for this result set */
        ensure_init();
        
        funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
		state = palloc(sizeof(struct bitmap_hash_entries_state));
        MemoryContextSwitchTo(oldcontext);

        name = strfromtext(PG_GETARG_TEXT_P(0));
        state->bmhash = GetBitmapHash(name, false);

        if (!state->bmhash) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Bitmap Hash %s not defined", name)));
        }

        state->var = NULL;
		funcctx->user_fctx = state;
    }

    funcctx = SRF_PERCALL_SETUP();
	state = funcctx->user_fctx;

    state->var = vl_NextHashEntry(state->bmhash->hash, state->var);

    if (state->var) {
        result = textfromstrn(state->var->key, HASH_KEYLEN);
        datum = PointerGetDatum(result);
        SRF_RETURN_NEXT(funcctx, datum);
    }
    else {
        SRF_RETURN_DONE(funcctx);
    }
}


PG_FUNCTION_INFO_V1(veil_int4_set);
/** 
 * <code>veil_int4_set(name text,value int4) returns int4</code>
 * Set an Int4Var variable type to a specified value.
 * An Error will be raised if the variable is not defined or is of the
 * wrong type.
 *
 * @param fcinfo <code>name text</code> The name of the int4 variable.
 * @param fcinfo <code>value int4</code> The value to be set (may be null).
 * @return <code>int4</code>  The new value of the variable.
 */
Datum
veil_int4_set(PG_FUNCTION_ARGS)
{
    char     *name;
    Int4Var  *var;
    int32     value;

    ensure_init();

    if (PG_ARGISNULL(0)) {
        PG_RETURN_NULL();
	}

    name = strfromtext(PG_GETARG_TEXT_P(0));
    var = GetInt4Var(name, true);

    if (PG_ARGISNULL(1)) {
        var->isnull = true;
        PG_RETURN_NULL();
    }
    else {
        value = PG_GETARG_INT32(1);
        var->isnull = false;
        var->value = value;
        PG_RETURN_INT32(value);
    }
}

PG_FUNCTION_INFO_V1(veil_int4_get);
/** 
 * <code>veil_int4_get(name text) returns int4</code>
 * Return the value of an Int4Var variable.
 * An Error will be raised if the variable is not defined or is of the
 * wrong type.
 *
 * @param fcinfo <code>name text</code> The name of the int4 variable.
 * @return <code>int4</code>  The value of the variable.
 */
Datum
veil_int4_get(PG_FUNCTION_ARGS)
{
    char        *name;
    Int4Var     *var;

    ensure_init();

    name = strfromtext(PG_GETARG_TEXT_P(0));
    var = GetInt4Var(name, true);

    if (var->isnull) {
        PG_RETURN_NULL();
    }
    else {
        PG_RETURN_INT32(var->value);
    }
}

PG_FUNCTION_INFO_V1(veil_init_int4array);
/** 
 * <code>veil_init_int4array(arrayname text, range text) returns bool</code>
 * Initialise an Int4Array variable.  Each entry in the array will be
 * zeroed.
 *
 * @param fcinfo <code>arrayname text</code> The name of the Int4Array variable.
 * <br><code>range text</code> Name of the range variable defining the size of
 * the array.
 * @return <code>bool</code>  True
 */
Datum
veil_init_int4array(PG_FUNCTION_ARGS)
{
    char      *array_name;
    char      *range_name;
    VarEntry  *array_var;
    Int4Array *array;
	Range     *range;

    ensure_init();

    array_name = strfromtext(PG_GETARG_TEXT_P(0));
    array_var = vl_lookup_variable(array_name);
	array = GetInt4ArrayFromVar(array_var, true);

    range_name = strfromtext(PG_GETARG_TEXT_P(1));
    range = GetRange(range_name, false);

    array = vl_NewInt4Array(array, array_var->shared, range->min, range->max);
	array_var->obj = (Object *) array;

	PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_clear_int4array);
/** 
 * <code>veil_clear_int4array(name text) returns bool</code>
 * Clear an Int4Array variable.  Each entry in the array will be
 * zeroed.
 *
 * @param fcinfo <code>name text</code> The name of the Int4Array variable.
 * @return <code>bool</code>  True
 */
Datum
veil_clear_int4array(PG_FUNCTION_ARGS)
{
    char      *array_name;
    Int4Array *array;

    ensure_init();

    array_name = strfromtext(PG_GETARG_TEXT_P(0));
	array = GetInt4Array(array_name, false);

    vl_ClearInt4Array(array);

	PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_int4array_set);
/** 
 * <code>veil_int4array_set(array text, idx int4, value int4) returns int4</code>
 * Set an Int4Array entry. 
 *
 * @param fcinfo <code>array text</code> The name of the Int4Array variable.
 * <br><code>idx int4</code> Index of the entry to be set
 * <br><code>value int4</code> Value to which the entry will be set
 * @return <code>int4</code> The new value of the array entry
 */
Datum
veil_int4array_set(PG_FUNCTION_ARGS)
{
    char      *array_name;
    Int4Array *array;
    int32      idx;
    int32      value;

    ensure_init();

    array_name = strfromtext(PG_GETARG_TEXT_P(0));
	array = GetInt4Array(array_name, false);
	idx = PG_GETARG_INT32(1);
	value = PG_GETARG_INT32(2);
    vl_Int4ArraySet(array, idx, value);

	PG_RETURN_INT32(value);
}

PG_FUNCTION_INFO_V1(veil_int4array_get);
/** 
 * <code>veil_int4array_get(array text, idx int4) returns int4</code>
 * Get an Int4Array entry. 
 *
 * @param fcinfo <code>array text</code> The name of the Int4Array variable.
 * <br><code>idx int4</code> Index of the entry to be retrieved
 * @return <code>int4</code> The value of the array entry
 */
Datum
veil_int4array_get(PG_FUNCTION_ARGS)
{
    char      *array_name;
    Int4Array *array;
    int32      idx;
    int32      value;

    ensure_init();

    array_name = strfromtext(PG_GETARG_TEXT_P(0));
	array = GetInt4Array(array_name, false);
	idx = PG_GETARG_INT32(1);
    value = vl_Int4ArrayGet(array, idx);

	PG_RETURN_INT32(value);
}


PG_FUNCTION_INFO_V1(veil_init);
/** 
 * <code>veil_init(doing_reset bool) returns bool</code>
 * Initialise or reset a veil session.
 * The boolean parameter will be false when called for initialisation,
 * and true when performing a reset.
 *
 * This function may be redefined as a custom function in your
 * implementation, or will call initialisation functions registered in 
 * the table veil.veil_init_fns.
 *
 * @param fcinfo <code>doing_reset bool</code> Whether we are being
 * called in order to reset (true) the session or (false) simply to
 * initialise it. 
 * @return <code>bool</code> True
 */
Datum
veil_init(PG_FUNCTION_ARGS)
{
	bool param = PG_GETARG_BOOL(0);
	int rows = vl_call_init_fns(param);

	if (rows == 0) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("No user defined veil init functions found"),
				 errhint("You must redefine veil.veil_init() or register your "
					 "own init functions in the veil.veil_init_fns table.")));
	}
    PG_RETURN_BOOL(true);
}

PG_FUNCTION_INFO_V1(veil_perform_reset);
/** 
 * <code>veil_perform_reset() returns bool</code>
 * Reset veil shared memory for this database.  This creates a new
 * shared memory context with none of the existing shared variables.
 * All current transactions will be able to continue using the current
 * variables, all new transactions will see the new set, once this
 * function has completed.
 *
 * @param fcinfo 
 * @return <code>bool</code> True if the function is able to complete
 * successfully.  If it is unable, no harm will have been done but
 * neither will a memory reset have been performed.
 */
Datum
veil_perform_reset(PG_FUNCTION_ARGS)
{
    bool success = true;
	bool pushed;
    bool result;
    int  ok;

	ensure_init();
    ok = vl_spi_connect(&pushed);
    if (ok != SPI_OK_CONNECT) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to perform reset"),
				 errdetail("SPI_connect() failed, returning %d.", ok)));
    }

    success = vl_prepare_context_switch();  
    if (success) {
        result = vl_bool_from_query("select veil.veil_init(TRUE)", &success);
		elog(NOTICE, "veil_init returns %s to veil_perform_reset", 
			 success? "true": "false");
        success = vl_complete_context_switch();
		elog(NOTICE, 
			 "vl_complete_context_switch returns %s to veil_perform_reset", 
			 success? "true": "false");
		success &= result;
    }
    else {
		ereport(WARNING,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to perform reset"),
				 errdetail("Unable to prepare for memory reset.  "
						   "Maybe another process is performing a reset, "
					       "or maybe there is a long-running transaction that "
						   "is still using the previous memory context.")));
    }

    ok = vl_spi_finish(pushed);
    PG_RETURN_BOOL(success);
}

PG_FUNCTION_INFO_V1(veil_force_reset);
/** 
 * <code>veil_force_reset() returns bool</code>
 * Reset veil shared memory for this database, ignoring existing
 * transactions.  This function will always reset the shared memory
 * context, even for sessions that are still using it.  Having taken
 * this drastic action, it will then cause a panic to reset the server.
 *
 * Question - won't a panic reset the shared memory in any case?  Is the
 * panic superfluous, or maybe is this entire function superfluous?
 *
 * @param fcinfo 
 * @return <code>bool</code> True.
 */
Datum
veil_force_reset(PG_FUNCTION_ARGS)
{
	ensure_init();
    vl_force_context_switch();
    PG_RETURN_BOOL(true);
}


PG_FUNCTION_INFO_V1(veil_version);
/** 
 * <code>veil_version() returns text</code>
 * Return a string describing this version of veil.
 *
 * @param fcinfo 
 * @return <code>text</code> String describing the version.
 */
Datum
veil_version(PG_FUNCTION_ARGS)
{
    char *version_str;
	text *version_text;
	
    version_str = palloc(sizeof(char) * strlen(VEIL_VERSION) + 
						 strlen(VEIL_VERSION_INFO) + 4);
	sprintf(version_str, "%s (%s)", VEIL_VERSION, VEIL_VERSION_INFO);

	version_text = textfromstr(version_str);
	PG_RETURN_TEXT_P(version_text);
}

PG_FUNCTION_INFO_V1(veil_serialise);
/** 
 * <code>veil_serialise(varname text) returns text</code>
 * Return a string representing the contents of our variable.
 *
 * @param fcinfo 
 * <br><code>varname text</code> Name of the variable to be serialised.
 * @return <code>text</code> String containing the serialised variable.
 */
Datum
veil_serialise(PG_FUNCTION_ARGS)
{
    char  *name;
	char  *result;

    ensure_init();

    if (PG_ARGISNULL(0)) {
        PG_RETURN_NULL();
	}

    name = strfromtext(PG_GETARG_TEXT_P(0));
	result = vl_serialise_var(name);

	if (result) {
		PG_RETURN_TEXT_P(textfromstr(result));
	}
	else {
		PG_RETURN_NULL();
	}
}


PG_FUNCTION_INFO_V1(veil_deserialise);
/** 
 * <code>veil_deserialise(stream text) returns text</code>
 * Create or reset variables based on the output of previous
 * veil_serialise calls.
 *
 * @param fcinfo 
 * <br><code>stream text</code> Serialised variables string
 * @return <code>int4</code> Count of the items de-serialised from
 * the string.
 */
Datum
veil_deserialise(PG_FUNCTION_ARGS)
{
    char  *stream;
	int4   result;
	text  *txt;

    ensure_init();

    if (PG_ARGISNULL(0)) {
        PG_RETURN_NULL();
	}

	txt = PG_GETARG_TEXT_P(0);
	stream = strfromtext(txt);
	result = vl_deserialise(&stream);

	PG_RETURN_INT32(result);
}


