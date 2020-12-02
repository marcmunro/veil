var API_page =
[
    [ "The Veil API", "API-page.html#API-sec", null ],
    [ "Veil API Overview", "API-page.html#API-intro", null ],
    [ "Variables", "API-variables.html", [
      [ "share(name text)", "API-variables.html#API-variables-share", null ],
      [ "veil_variables()", "API-variables.html#API-variables-var", null ]
    ] ],
    [ "Basic Types: Integers and Ranges", "API-simple.html", [
      [ "init_range(name text, min int4, max int4)", "API-simple.html#API-basic-init-range", null ],
      [ "range(name text)", "API-simple.html#API-basic-range", null ],
      [ "int4_set(name text, value int4)", "API-simple.html#API-basic-int4-set", null ],
      [ "int4_get(name text)", "API-simple.html#API-basic-int4-get", null ]
    ] ],
    [ "Bitmaps and Bitmap Refs", "API-bitmaps.html", [
      [ "init_bitmap(bitmap_name text, range_name text)", "API-bitmaps.html#API-bitmap-init", null ],
      [ "clear_bitmap(bitmap_name text)", "API-bitmaps.html#API-bitmap-clear", null ],
      [ "bitmap_setbit(bitmap_name text, bit_number int4)", "API-bitmaps.html#API-bitmap-setbit", null ],
      [ "bitmap_clearbit(bitmap_name text, bit_number int4)", "API-bitmaps.html#API-bitmap-clearbit", null ],
      [ "bitmap_testbit(bitmap_name text, bit_number int4)", "API-bitmaps.html#API-bitmap-testbit", null ],
      [ "bitmap_union(result_name text, bm2_name text)", "API-bitmaps.html#API-bitmap-union", null ],
      [ "bitmap_intersect(result_name text, bm2_name text)", "API-bitmaps.html#API-bitmap-intersect", null ],
      [ "bitmap_bits(bitmap_name text)", "API-bitmaps.html#API-bitmap-bits", null ],
      [ "bitmap_range(bitmap_name text)", "API-bitmaps.html#API-bitmap-range", null ]
    ] ],
    [ "Bitmap Arrays", "API-bitmap-arrays.html", [
      [ "init_bitmap_array(bmarray text, array_range text, bitmap_range text)", "API-bitmap-arrays.html#API-bmarray-init", null ],
      [ "clear_bitmap_array(bmarray text)", "API-bitmap-arrays.html#API-bmarray-clear", null ],
      [ "bitmap_from_array(bmref_name text, bmarray text, index int4)", "API-bitmap-arrays.html#API-bmarray-bmap", null ],
      [ "bitmap_array_testbit(bmarray text, arr_idx int4, bitno int4)", "API-bitmap-arrays.html#API-bmarray-testbit", null ],
      [ "bitmap_array_setbit(bmarray text, arr_idx int4, bitno int4)", "API-bitmap-arrays.html#API-bmarray-setbit", null ],
      [ "bitmap_array_clearbit(bmarray text, arr_idx int4, bitno int4)", "API-bitmap-arrays.html#API-bmarray-clearbit", null ],
      [ "union_from_bitmap_array(bitmap text, bmarray text, arr_idx int4)", "API-bitmap-arrays.html#API-bmarray-union", null ],
      [ "intersect_from_bitmap_array(bitmap text, bmarray text, arr_idx int4)", "API-bitmap-arrays.html#API-bmarray-intersect", null ],
      [ "bitmap_array_bits(bmarray text, arr_idx int4)", "API-bitmap-arrays.html#API-bmarray-bits", null ],
      [ "bitmap_array_arange(bmarray text)", "API-bitmap-arrays.html#API-bmarray-arange", null ],
      [ "bitmap_array_brange(bmarray text)", "API-bitmap-arrays.html#API-bmarray-brange", null ]
    ] ],
    [ "Bitmap Hashes", "API-bitmap-hashes.html", [
      [ "init_bitmap_hash(bmhash text, range text)", "API-bitmap-hashes.html#API-bmhash-init", null ],
      [ "clear_bitmap_hash(bmhash text)", "API-bitmap-hashes.html#API-bmhash-clear", null ],
      [ "bitmap_hash_key_exists(bmhash text, key text)", "API-bitmap-hashes.html#API-bmhash-key-exists", null ],
      [ "bitmap_from_hash(bmref text, bmhash text, key text)", "API-bitmap-hashes.html#API-bmhash-from", null ],
      [ "bitmap_hash_testbit(bmhash text, key text, bitno int4)", "API-bitmap-hashes.html#API-bmhash-testbit", null ],
      [ "bitmap_hash_setbit(bmhash text, kay text, bitno int4)", "API-bitmap-hashes.html#API-bmhash-setbit", null ],
      [ "bitmap_hash_clearbit(bmhash text, key text, bitno int4)", "API-bitmap-hashes.html#API-bmhash-clearbit", null ],
      [ "union_into_bitmap_hash(bmhash text, key text, bitmap text)", "API-bitmap-hashes.html#API-bmhash-union-into", null ],
      [ "union_from_bitmap_hash(bmhash text, key text, bitmap text)", "API-bitmap-hashes.html#API-bmhash-union-from", null ],
      [ "intersect_from_bitmap_hash(bitmap text, bmhash text, key text)", "API-bitmap-hashes.html#API-bmhash-intersect-from", null ],
      [ "bitmap_hash_bits(bmhash text, key text)", "API-bitmap-hashes.html#API-bmhash-bits", null ],
      [ "bitmap_hash_range(bmhash text)", "API-bitmap-hashes.html#API-bmhash-range", null ],
      [ "bitmap_hash_entries(bmhash text)", "API-bitmap-hashes.html#API-bmhash-entries", null ]
    ] ],
    [ "Integer Arrays", "API-int-arrays.html", [
      [ "init_int4array(arrayname text, range text)", "API-int-arrays.html#API-intarray-init", null ],
      [ "clear_int4array(arrayname text)", "API-int-arrays.html#API-intarray-clear", null ],
      [ "int4array_set(arrayname text, idx int4, value int4)", "API-int-arrays.html#API-intarray-set", null ],
      [ "int4array_get(arrayname text, idx int4)", "API-int-arrays.html#API-intarray-get", null ]
    ] ],
    [ "Veil Serialisation Functions", "API-serialisation.html", [
      [ "serialise(varname text)", "API-serialisation.html#API-serialise", null ],
      [ "deserialise(stream text)", "API-serialisation.html#API-deserialise", null ],
      [ "serialize(varname text)", "API-serialisation.html#API-serialize", null ],
      [ "deserialize(stream text)", "API-serialisation.html#API-deserialize", null ]
    ] ],
    [ "Veil Control Functions", "API-control.html", [
      [ "registered initialisation functions", "API-control.html#API-control-registered-init", null ],
      [ "veil_init(doing_reset bool)", "API-control.html#API-control-init", null ],
      [ "veil_perform_reset()", "API-control.html#API-control-reset", null ],
      [ "version()", "API-control.html#API-version", null ]
    ] ]
];