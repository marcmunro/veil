/**
 * @file   veil_serialise.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2009 - 2011 Marc Munro
 *     License:      BSD
 *
 * \endcode
 * @brief  
 * Functions serialising and de-serialising session variables.	 The
 * purpose of this is to allow the contents of session variables to be
 * saved for later re-use.  They may be saved in files, temporary tables
 * or using some smart cache such as memcached (thru the pgmemcache
 * add-in).
 * 
 */

/*TODO: Provide a patch to postgres to make b64_encode and
 *      b64_decode extern functions and to elimiate the 
 *      redundant copies from pgcrypto 
 */

#include "postgres.h"
#include "veil_version.h"
#include "veil_funcs.h"
#include "veil_datatypes.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define INT4VAR_HDR       'V'
#define INT8VAR_HDR       '8'
#define RANGE_HDR         'R'
#ifdef USE_64_BIT
#define BITMAP_HDR        'B'
#else
#define BITMAP_HDR        'M'
#endif
#define BITMAP_ARRAY_HDR  'A'
#define BITMAP_HASH_HDR   'H'
#define INT4_ARRAY_HDR    'I'
#define BITMAP_HASH_MORE  '>'
#define BITMAP_HASH_DONE  '.'


#define HDRLEN                 8   /* HDR field plus int32 for length of
									* item */
#define INT32SIZE_B64          7   /* Actually 8 but the last char is
									* always '=' so we forget it. */
#define INT64SIZE_B64          12
#define BOOLSIZE               1


/* BEGIN SECTION OF CODE COPIED FROM pgcrypto.c */

static const char _base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int8 b64lookup[128] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
};

static unsigned
b64_encode(const char *src, unsigned len, char *dst)
{
	char	   *p,
			   *lend = dst + 76;
	const char *s,
			   *end = src + len;
	int			pos = 2;
	uint32		buf = 0;

	s = src;
	p = dst;

	while (s < end)
	{
		buf |= (unsigned char) *s << (pos << 3);
		pos--;
		s++;

		/* write it out */
		if (pos < 0)
		{
			*p++ = _base64[(buf >> 18) & 0x3f];
			*p++ = _base64[(buf >> 12) & 0x3f];
			*p++ = _base64[(buf >> 6) & 0x3f];
			*p++ = _base64[buf & 0x3f];

			pos = 2;
			buf = 0;
		}
		if (p >= lend)
		{
			*p++ = '\n';
			lend = p + 76;
		}
	}
	if (pos != 2)
	{
		*p++ = _base64[(buf >> 18) & 0x3f];
		*p++ = _base64[(buf >> 12) & 0x3f];
		*p++ = (pos == 0) ? _base64[(buf >> 6) & 0x3f] : '=';
		*p++ = '=';
	}

	return p - dst;
}

static unsigned
b64_decode(const char *src, unsigned len, char *dst)
{
	const char *srcend = src + len,
			   *s = src;
	char	   *p = dst;
	char		c;
	int			b = 0;
	uint32		buf = 0;
	int			pos = 0,
				end = 0;

	while (s < srcend)
	{
		c = *s++;

		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;

		if (c == '=')
		{
			/* end sequence */
			if (!end)
			{
				if (pos == 2)
					end = 1;
				else if (pos == 3)
					end = 2;
				else
					ereport(ERROR,
							(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
							 errmsg("unexpected \"=\"")));
			}
			b = 0;
		}
		else
		{
			b = -1;
			if (c > 0 && c < 127)
				b = b64lookup[(unsigned char) c];
			if (b < 0)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("invalid symbol")));
		}
		/* add it to buffer */
		buf = (buf << 6) + b;
		pos++;
		if (pos == 4)
		{
			*p++ = (buf >> 16) & 255;
			if (end == 0 || end > 1)
				*p++ = (buf >> 8) & 255;
			if (end == 0 || end > 2)
				*p++ = buf & 255;
			buf = 0;
			pos = 0;
		}
	}

	if (pos != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid end sequence")));

	return p - dst;
}

/* END SECTION OF CODE COPIED FROM pgcrypto.c */

#endif

/** 
 * Return the length of a base64 encoded stream for a binary stream of
 * ::bytes length.
 * 
 * @param bytes The length of the input binary stream in bytes
 * @return The length of the base64 character stream required to
 * represent the input stream.
 */
static int
streamlen(int bytes)
{
	return (4 * ((bytes + 2) / 3));
}

/** 
 * Return the length of the header part of a serialised data stream for
 * the given named variable.  Note that the header contains the name and
 * a base64 encode length indicator for the name.
 * 
 * @param name The variable name to be recorded in the header
 * @return The length of the base64 character stream required to
 * hold the serialised header for the named variable.
 */
static int
hdrlen(char *name)
{
	return HDRLEN + INT32SIZE_B64 + strlen(name);
}


/** 
 * Serialise an int4 value as a base64 stream (truncated to save a
 * byte) into *p_stream.
 *
 * @param p_stream Pointer into stream currently being written.  This
 * must be large enought to take the contents to be written.  This
 * pointer is updated to point to the next free slot in the stream after
 * writing the int4 value, and is null terminated at that position.
 * @param value The value to be written to the stream.
 */
static void
serialise_int4(char **p_stream, int32 value)
{
	int len = b64_encode((char *) &value, sizeof(int32), *p_stream);
	(*p_stream) += (len - 1);  /* X: dumb optimisation saves a byte */
	(**p_stream) = '\0';
}


/** 
 * De-serialise an int4 value from a base64 character stream.
 *
 * @param p_stream Pointer into the stream currently being read.
 * must be large enought to take the contents to be written.  This
 * pointer is updated to point to the next free slot in the stream after
 * reading the int4 value.
 * @return the int4 value read from the stream
 */
static int32
deserialise_int4(char **p_stream)
{
	int32 value;
	char *endpos = (*p_stream) + INT32SIZE_B64;
	char endchar = *endpos;
	*endpos = '=';	/* deal with dumb optimisation (X) above */
	b64_decode(*p_stream, INT32SIZE_B64 + 1, (char *) &value);
	*endpos = endchar;
	(*p_stream) += INT32SIZE_B64;
	return value;
}

#ifdef UNUSED_BUT_WORKS
/** 
 * Serialise an int8 value as a base64 stream into *p_stream.
 *
 * @param p_stream Pointer into stream currently being written.  This
 * must be large enought to take the contents to be written.  This
 * pointer is updated to point to the next free slot in the stream after
 * writing the int8 value, and is null terminated at that position.
 * @param value The value to be written to the stream.
 */
static void
serialise_int8(char **p_stream, int64 value)
{
	int len = b64_encode((char *) &value, sizeof(int64), *p_stream);
	(*p_stream) += len; 
	(**p_stream) = '\0';
}


/** 
 * De-serialise an int8 value from a base64 character stream.
 *
 * @param p_stream Pointer into the stream currently being read.
 * must be large enought to take the contents to be written.  This
 * pointer is updated to point to the next free slot in the stream after
 * reading the int8 value.
 * @return the int8 value read from the stream
 */
static int64
deserialise_int8(char **p_stream)
{
	int64 value;
	fprintf(stderr, "Deserialise %s\n", *p_stream);
	b64_decode(*p_stream, INT64SIZE_B64, (char *) &value);
	(*p_stream) += INT64SIZE_B64;
	return value;
}
#endif

/** 
 * Serialise a binary stream as a base64 stream into *p_stream.
 *
 * @param p_stream Pointer into stream currently being written.  This
 * pointer is updated to point to the next free slot in the stream after
 * writing the contents of instream and is null terminated at that
 * position.
 * @param bytes The number of bytes to be written.
 * @param instream The binary stream to be written.
 */
static void
serialise_stream(char **p_stream, int32 bytes, char *instream)
{
	int len = b64_encode(instream, bytes, *p_stream);
	(*p_stream)[len] = '\0';
	(*p_stream) += len;
}

/** 
 * De-serialise a binary stream.
 *
 * @param p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream.
 * @param bytes The number of bytes to be read
 * @param outstream Pointer into the pre-allocated memory are into which
 * the binary from p_stream is to be written.
 */
static void
deserialise_stream(char **p_stream, int32 bytes, char *outstream)
{
	int32 len = streamlen(bytes);
	b64_decode(*p_stream, len, outstream);
	(*p_stream) += len;
}


/** 
 * Serialise a boolean value into *p_stream.
 *
 * @param p_stream Pointer into stream currently being written.  This
 * pointer is updated to point to the next free slot in the stream after
 * writing the contents of value and is null terminated at that
 * position.  A true value is written as 'T', and false as 'F'.
 * @param value The boolean value to be written.
 */
static void
serialise_bool(char **p_stream, bool value)
{
	(**p_stream) = value? 'T': 'F';
	(*p_stream)++;
	(**p_stream) = '\0';
}

/** 
 * De-serialise a boolean value.
 *
 * @param p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream.
 * @return True or false depending on the contents of p_stream.
 */
static bool
deserialise_bool(char **p_stream)
{
	bool result = (**p_stream) == 'T';
	(*p_stream)++;

	return result;
}

/** 
 * Serialise a character value into *p_stream.
 *
 * @param p_stream Pointer into stream currently being written.  This
 * pointer is updated to point to the next free slot in the stream after
 * writing the contents of value and is null terminated at that
 * position.  The character is written as a single byte character.
 * @param value The character value to be written.
 */
static void
serialise_char(char **p_stream, char value)
{
	(**p_stream) = value;
	(*p_stream)++;
	(**p_stream) = '\0';
}

/** 
 * De-serialise a character value.
 *
 * @param p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream.
 * @return The value of the character read from p_stream.
 */
static char
deserialise_char (char **p_stream)
{
	char result = **p_stream;
	(*p_stream)++;

	return result;
}

/** 
 * Serialise a string (containing a name) into *p_stream.
 *
 * @param p_stream Pointer into stream currently being written.  This
 * pointer is updated to point to the next free slot in the stream after
 * writing the contents of value and is null terminated at that
 * position.  The character is written as a single byte character.
 * @param name The string to be written.
 */
static void
serialise_name(char **p_stream, char *name)
{
	static char *blank_name = "";
	char *safe_name;
	if (name) {
		safe_name = name;
	}
	else {
		safe_name = blank_name;
	}
		
	serialise_int4(p_stream, strlen(safe_name));
	strcpy((*p_stream), safe_name);
	(*p_stream) += strlen(safe_name);
	(**p_stream) = '\0';
}

/** 
 * De-serialise a string returning a dynamically allocated string.
 *
 * @param p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream.
 * @return Dynamically allocated copy of string read from *p_stream
 */
static char *
deserialise_name(char **p_stream)
{
	int32 name_len = deserialise_int4(p_stream);
	char *result = palloc((name_len + 1) * sizeof(char));
	strncpy(result, *p_stream, name_len);
	result[name_len] = '\0';
	(*p_stream) += name_len;
	return result;
}

/** 
 * Serialise a veil integer variable into a dynamically allocated string.
 *
 * @param var Pointer to the variable to be serialised
 * @param name The name of the variable
 * @return Dynamically allocated string containing the serialised
 * variable
 */
static char *
serialise_int4var(Int4Var *var, char *name)
{
    int stream_len = hdrlen(name) + BOOLSIZE + INT32SIZE_B64 + 1;
	char *stream = palloc(stream_len * sizeof(char));
	char *streamstart = stream;
	
	serialise_char(&stream, INT4VAR_HDR);
	serialise_name(&stream, name);
	serialise_bool(&stream, var->isnull);
	serialise_int4(&stream, var->value);
	return streamstart;
}

/** 
 * De-serialise a veil integer variable.
 *
 * @param **p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream
 * @return Pointer to the variable created or updated from the stream.
 */
static VarEntry *
deserialise_int4var(char **p_stream)
{
	char *name = deserialise_name(p_stream);
	VarEntry *var = vl_lookup_variable(name);
    Int4Var *i4v = (Int4Var *) var->obj;

    if (i4v) {
        if (i4v->type != OBJ_INT4) {
            vl_type_mismatch(name, OBJ_INT4, i4v->type);
        }
    }
	else {
        var->obj = (Object *) vl_NewInt4(var->shared);
        i4v = (Int4Var *) var->obj;
 	}
	i4v->isnull = deserialise_bool(p_stream);
	i4v->value = deserialise_int4(p_stream);
	return var;
}

/** 
 * Serialise a veil integer array variable into a dynamically allocated
 * string.
 *
 * @param array Pointer to the variable to be serialised
 * @param name The name of the variable
 * @return Dynamically allocated string containing the serialised
 * variable
 */
static char *
serialise_int4array(Int4Array *array, char *name)
{
    int elems = 1 + array->arraymax - array->arrayzero;
    int stream_len = hdrlen(name) + (2 * INT32SIZE_B64) +
		streamlen(elems * sizeof(int32)) + 1;
	char *stream = palloc(stream_len * sizeof(char));
	char *streamstart = stream;

	serialise_char(&stream, INT4_ARRAY_HDR);
	serialise_name(&stream, name);
	serialise_int4(&stream, array->arrayzero);
	serialise_int4(&stream, array->arraymax);
	serialise_stream(&stream, elems * sizeof(int32), 
					 (char *) &(array->array[0]));

	return streamstart;
}

/** 
 * De-serialise a veil integer array variable.
 *
 * @param **p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream
 * @return Pointer to the variable created or updated from the stream.
 */
static VarEntry *
deserialise_int4array(char **p_stream)
{
	char *name = deserialise_name(p_stream);
    int32 arrayzero;
	int32 arraymax;
	int32 elems;
	VarEntry *var = vl_lookup_variable(name);
	Int4Array *array = (Int4Array *) var->obj;

	arrayzero = deserialise_int4(p_stream);
	arraymax = deserialise_int4(p_stream);
	elems = 1 + arraymax - arrayzero;

    if (array) {
        if (array->type != OBJ_INT4_ARRAY) {
            vl_type_mismatch(name, OBJ_INT4_ARRAY, array->type);
        }
    }
	array = vl_NewInt4Array(array, var->shared, arrayzero, arraymax);
	var->obj = (Object *) array;

	deserialise_stream(p_stream, elems * sizeof(int32), 
					   (char *) &(array->array[0]));
	return var;
}

/** 
 * Serialise a veil range variable into a dynamically allocated
 * string.
 *
 * @param range Pointer to the variable to be serialised
 * @param name The name of the variable
 * @return Dynamically allocated string containing the serialised
 * variable
 */
static char *
serialise_range(Range *range, char *name)
{
    int stream_len = hdrlen(name) + (INT32SIZE_B64 * 2) + 1;
	char *stream = palloc(stream_len * sizeof(char));
	char *streamstart = stream;

	serialise_char(&stream, RANGE_HDR);
	serialise_name(&stream, name);
	serialise_int4(&stream, range->min);
	serialise_int4(&stream, range->max);
	return streamstart;
}

/** 
 * De-serialise a veil range variable.
 *
 * @param **p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream
 * @return Pointer to the variable created or updated from the stream.
 */
static VarEntry *
deserialise_range(char **p_stream)
{
	char *name = deserialise_name(p_stream);
	VarEntry *var = vl_lookup_variable(name);
    Range *range = (Range *) var->obj;

    if (range) {
        if (range->type != OBJ_RANGE) {
            vl_type_mismatch(name, OBJ_RANGE, range->type);
        }
    }
	else {
        var->obj = (Object *) vl_NewRange(var->shared);
        range = (Range *) var->obj;
 	}

	range->min = deserialise_int4(p_stream);
	range->max = deserialise_int4(p_stream);
	return var;
}

/** 
 * Serialise a single bitmap from a veil bitmap array or bitmap hash.
 *
 * @param p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * writing the stream.
 * @param bitmap The bitmap to be serialised.
 */
static void
serialise_one_bitmap(char **p_stream, Bitmap *bitmap)
{
    int elems = ARRAYELEMS(bitmap->bitzero, bitmap->bitmax);
	serialise_int4(p_stream, bitmap->bitzero);
	serialise_int4(p_stream, bitmap->bitmax);
	serialise_stream(p_stream, elems * sizeof(bm_int), 
					 (char *) &(bitmap->bitset));
}

/** 
 * Serialise a veil bitmap variable into a dynamically allocated
 * string.
 *
 * @param bitmap Pointer to the variable to be serialised
 * @param name The name of the variable
 * @return Dynamically allocated string containing the serialised
 * variable
 */
static char *
serialise_bitmap(Bitmap *bitmap, char *name)
{
    int elems = ARRAYELEMS(bitmap->bitzero, bitmap->bitmax);
    int stream_len = hdrlen(name) + (INT32SIZE_B64 * 2) + 
		             streamlen(sizeof(bm_int) * elems) + 1;
	char *stream = palloc(stream_len * sizeof(char));
	char *streamstart = stream;

	serialise_char(&stream, BITMAP_HDR);
	serialise_name(&stream, name);
	serialise_one_bitmap(&stream, bitmap);
	return streamstart;
}

/** 
 * De-serialise a single bitmap into a veil bitmap array or bitmap hash.
 *
 * @param p_bitmap Pointer to bitmap pointer.  This may be updated to
 * contain a dynamically allocated bitmap if none is already present.
 * @param name  The name of the variable, for error reporting purposes.
 * @param shared Whether the bitmap is part of a shared rather than
 * session variable.
 * @param p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream.
 */
static void
deserialise_one_bitmap(Bitmap **p_bitmap, char *name, 
					   bool shared, char **p_stream)
{
	Bitmap *bitmap = *p_bitmap;
    int32 bitzero;
	int32 bitmax;
	int32 elems;

	bitzero = deserialise_int4(p_stream);
	bitmax = deserialise_int4(p_stream);
	elems = ARRAYELEMS(bitzero, bitmax);

    if (bitmap) {
        if (bitmap->type != OBJ_BITMAP) {
            vl_type_mismatch(name, OBJ_BITMAP, bitmap->type);
        }
    }
	/* Check size and re-allocate memory if needed */
	vl_NewBitmap(p_bitmap, shared, bitzero, bitmax);
	bitmap = *p_bitmap;

	deserialise_stream(p_stream, elems * sizeof(bitmap->bitset[0]), 
					   (char *) &(bitmap->bitset[0]));

}

/** 
 * De-serialise a veil bitmap variable.
 *
 * @param **p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream
 * @return Pointer to the variable created or updated from the stream.
 */
static VarEntry *
deserialise_bitmap(char **p_stream)
{
	char *name = deserialise_name(p_stream);
	VarEntry *var = vl_lookup_variable(name);
	Bitmap *bitmap = (Bitmap *) var->obj;

	deserialise_one_bitmap(&bitmap, name, var->shared, p_stream);
	var->obj = (Object *) bitmap;
	return var;
}

/** 
 * Serialise a veil bitmap array variable into a dynamically allocated
 * string.
 *
 * @param bmarray Pointer to the variable to be serialised
 * @param name The name of the variable
 * @return Dynamically allocated string containing the serialised
 * variable
 */
static char *
serialise_bitmap_array(BitmapArray *bmarray, char *name)
{
    int bitset_elems = ARRAYELEMS(bmarray->bitzero, bmarray->bitmax);
    int array_elems = 1 + bmarray->arraymax - bmarray->arrayzero;
	int bitmap_len = (INT32SIZE_B64 * 2) + 
		             streamlen(sizeof(bm_int) * bitset_elems);
    int stream_len = hdrlen(name) + (INT32SIZE_B64 * 4) + 
		             (bitmap_len * array_elems) + 1;
	int idx;
	char *stream = palloc(stream_len * sizeof(char));
	char *streamstart = stream;

	serialise_char(&stream, BITMAP_ARRAY_HDR);
	serialise_name(&stream, name);
	serialise_int4(&stream, bmarray->bitzero);
	serialise_int4(&stream, bmarray->bitmax);
	serialise_int4(&stream, bmarray->arrayzero);
	serialise_int4(&stream, bmarray->arraymax);
	for (idx = 0; idx < array_elems; idx++) {
		serialise_one_bitmap(&stream, bmarray->bitmap[idx]);
	}
	return streamstart;
}

/** 
 * De-serialise a veil bitmap array variable.
 *
 * @param **p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream
 * @return Pointer to the variable created or updated from the stream.
 */
static VarEntry *
deserialise_bitmap_array(char **p_stream)
{
	char *name = deserialise_name(p_stream);
    int32 bitzero;
	int32 bitmax;
    int32 arrayzero;
	int32 arraymax;
    int32 array_elems;
    int32 idx;
	VarEntry *var = vl_lookup_variable(name);
	BitmapArray *bmarray = (BitmapArray *) var->obj;

	bitzero = deserialise_int4(p_stream);
	bitmax = deserialise_int4(p_stream);
	arrayzero = deserialise_int4(p_stream);
	arraymax = deserialise_int4(p_stream);

    if (bmarray) {
        if (bmarray->type != OBJ_BITMAP_ARRAY) {
            vl_type_mismatch(name, OBJ_BITMAP_ARRAY, bmarray->type);
        }
    }
	/* Check size and re-allocate memory if needed */
	vl_NewBitmapArray(&bmarray, var->shared, arrayzero, 
					  arraymax, bitzero, bitmax);
	var->obj = (Object *) bmarray;

    array_elems = 1 + arraymax - arrayzero;
	for (idx = 0; idx < array_elems; idx++) {
		deserialise_one_bitmap(&(bmarray->bitmap[idx]), "", 
							   var->shared, p_stream);
		
	}
	return var;
}


/** 
 * Calculate the size needed for a base64 stream to contain all of the
 * bitmaps in a bitmap hash including their keys.
 *
 * @param bmhash Pointer to the variable to be serialised
 * @param bitset_size The size, in bytes, of each bitset in the bitmap
 * hash.
 * @return Number of bytes required to contain all of the bitmaps and
 * keys in the bitmap_hash
 */
static int
sizeof_bitmaps_in_hash(BitmapHash *bmhash, int bitset_size)
{
	VarEntry *var = NULL;
	int size = 1;  /* Allow for final end of stream indicator */
	while ((var = vl_NextHashEntry(bmhash->hash, var))) {
		/* 1 byte below for record/end flag to precede each bitmap in
		 * the hash */
		size += 1 + bitset_size + hdrlen(var->key); 
	}
	return size;
}


/** 
 * Serialise a veil bitmap hash variable into a dynamically allocated
 * string.
 *
 * @param bmhash Pointer to the variable to be serialised
 * @param name The name of the variable
 * @return Dynamically allocated string containing the serialised
 * variable
 */
static char *
serialise_bitmap_hash(BitmapHash *bmhash, char *name)
{
    int bitset_elems = ARRAYELEMS(bmhash->bitzero, bmhash->bitmax);
    int bitset_size = (INT32SIZE_B64 * 2) + 
		              streamlen(sizeof(int32) * bitset_elems);
	int all_bitmaps_size = sizeof_bitmaps_in_hash(bmhash, bitset_size);
    int stream_len = hdrlen(name) + (INT32SIZE_B64 * 2) + 
		             all_bitmaps_size + 1;
	char *stream = palloc(stream_len * sizeof(char));
	char *streamstart = stream;
	VarEntry *var = NULL;

	serialise_char(&stream, BITMAP_HASH_HDR);
	serialise_name(&stream, name);
	serialise_int4(&stream, bmhash->bitzero);
	serialise_int4(&stream, bmhash->bitmax);
	while ((var = vl_NextHashEntry(bmhash->hash, var))) {
		serialise_char(&stream, BITMAP_HASH_MORE);
		serialise_name(&stream, var->key);
		serialise_one_bitmap(&stream, (Bitmap *) var->obj);
	}
	serialise_char(&stream, BITMAP_HASH_DONE);

	return streamstart;
}

/** 
 * De-serialise a veil bitmap hash variable.
 *
 * @param **p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream
 * @return Pointer to the variable created or updated from the stream.
 */
static VarEntry *
deserialise_bitmap_hash(char **p_stream)
{
	char *name = deserialise_name(p_stream);
	char *hashkey;
    int32 bitzero;
	int32 bitmax;
	VarEntry *var = vl_lookup_variable(name);
	BitmapHash *bmhash = (BitmapHash *) var->obj;
	Bitmap *tmp_bitmap = NULL;
	Bitmap *bitmap;

	bitzero = deserialise_int4(p_stream);
	bitmax = deserialise_int4(p_stream);

    if (bmhash) {
        if (bmhash->type != OBJ_BITMAP_HASH) {
            vl_type_mismatch(name, OBJ_BITMAP_HASH, bmhash->type);
        }
    }
	/* Check size and re-allocate memory if needed */
	vl_NewBitmapHash(&bmhash, name, bitzero, bitmax);
	var->obj = (Object *) bmhash;

	while (deserialise_char(p_stream) == BITMAP_HASH_MORE) {
		hashkey = deserialise_name(p_stream);
		deserialise_one_bitmap(&tmp_bitmap, "", var->shared, p_stream);
		/* tmp_bitmap now contains a (dynamically allocated) bitmap
		 * Now we want to copy that into the bmhash.  We don't worry
		 * about memory leaks here since this is allocated only once
		 * per call of this function, and the memory context will
		 * eventually be freed anyway.
		 */
		bitmap = vl_AddBitmapToHash(bmhash, hashkey);
		vl_BitmapUnion(bitmap, tmp_bitmap);
	}
	return var;
}

/** 
 * Serialise a veil variable
 *
 * @param name  The name of the variable to be serialised.
 * @return Dynamically allocated string containing the serialised value.
 */
extern char *
vl_serialise_var(char *name)
{
	VarEntry *var;
	char *result = NULL;

	var = vl_lookup_variable(name);
	if (var && var->obj) {
		switch (var->obj->type)	{
			case OBJ_INT4:
				result = serialise_int4var((Int4Var *)var->obj, name);
				break;
			case OBJ_INT4_ARRAY:
				result = serialise_int4array((Int4Array *)var->obj, name);
				break;
			case OBJ_RANGE:
				result = serialise_range((Range *)var->obj, name);
				break;
			case OBJ_BITMAP:
				result = serialise_bitmap((Bitmap *)var->obj, name);
				break;
			case OBJ_BITMAP_ARRAY:
				result = serialise_bitmap_array((BitmapArray *)var->obj, name);
				break;
			case OBJ_BITMAP_HASH:
				result = serialise_bitmap_hash((BitmapHash *)var->obj, name);
				break;
			default:
				ereport(ERROR,
						(errcode(ERRCODE_INTERNAL_ERROR),
						 errmsg("Unsupported type for variable serialisation"),
						 errdetail("Cannot serialise objects of type %d.", 
								   (int32) var->obj->type)));
				
		}
	}
	return result;
}

/** 
 * De-serialise the next veil variable from *p_stream
 *
 * @param **p_stream Pointer into the stream currently being read.
 * pointer is updated to point to the next free slot in the stream after
 * reading the stream
 * @return The deserialised variable.
 */
extern VarEntry *
vl_deserialise_next(char **p_stream)
{
	VarEntry *var = NULL;
	if ((**p_stream) != '\0') {
		char type = deserialise_char(p_stream);
		switch (type){
			case INT4VAR_HDR: var = deserialise_int4var(p_stream);
				break;
			case INT4_ARRAY_HDR: var = deserialise_int4array(p_stream);
				break;
			case RANGE_HDR: var = deserialise_range(p_stream);
				break;
			case BITMAP_HDR: var = deserialise_bitmap(p_stream);
				break;
			case BITMAP_ARRAY_HDR: var = deserialise_bitmap_array(p_stream);
				break;
			case BITMAP_HASH_HDR: var = deserialise_bitmap_hash(p_stream);
				break;
			default:
				ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("Unsupported type for variable deserialisation"),
					 errdetail("Cannot deserialise objects of type %c.", 
							   type)));
				
		}
	}
	return var;
}

/** 
 * De-serialise a base64 string containing, possibly many, derialised
 * veil variables.
 *
 * @param **p_stream Pointer into the stream currently being read.
 * @return A count of the number of variables that have been de-serialised.
 */
extern int32
vl_deserialise(char **p_stream)
{
	int count = 0;
	while ((**p_stream) != '\0') {
		(void) vl_deserialise_next(p_stream);
		count++;
	}
	return count;
}
