#! /bin/sh
#
# regress.sh
#
#      Regression test script for Veil
#
#      Copyright (c) 2005-2011 Marc Munro
#      Author:  Marc Munro
#      License: BSD
#

bail()
{
    echo "Failed while $*.  Bailing out..." 1>&2
    exit 2
}

db_create()
{
    version=`psql --version | grep '[0-9]\.[0-9]'`
    version=`echo ${version} | sed -e 's/[^0-9]*\([0-9]*\.[0-9]*\).*/\1/'`
    major=`echo ${version} | cut -d. -f1`
    minor=`echo ${version} | cut -d. -f2`

    psql -d template1 <<EOF
\echo - Creating regressdb...
create database regressdb owner $1;
EOF
}

define_veil()
{
    echo "
drop extension if exists veil cascade;
create extension veil;"
}

db_build_schema()
{
    psql -d regressdb 2>&1 <<EOF
\echo - Creating regression test objects...
\set ECHO ALL
`define_veil`
create or replace
function veil.veil_init(doing_reset bool) returns bool as
'
begin
    return true;
end;
' language plpgsql;

\i ${dir}/regress_tables.sql
\i ${dir}/regress_data.sql

EOF
    if [ $? -ne 0 ]; then
	bail creating regression test objects
    fi
}

db_drop()
{
    (
	echo - Dropping regressdb...
	dropdb regressdb
    ) 2>&1 | psql_clean regress.log
}

do_test()
{
    psql -q -t -d regressdb <<EOF
set client_min_messages = 'notice';
`cat -`
EOF
    if [ $? -ne 0 ]; then
	bail running test set $1
    fi
}

# The format of a TEST definition line is:
# TEST <testno> <condition> #<pattern to test using condition>#<description>
#   testno is a string identifying the test (eg 1.4)
#   condition is one of "=", "!=", "~", "!~", specifying exact string match,
#     exact string mismatch, regular expression match and regular
#     expression mismatch respectively
#  <pattern to test using condition> is a string or regexp that must appear
#      or not in the test output for the test to pass.
# <description> is a short phrase describing the test
#
regress_1()
{
    echo - 
    echo "- ...test set 1: variables..."
    do_test 1 <<EOF
\echo PREP
select veil.veil_perform_reset();
select veil.veil_perform_reset();

-- Ensure the base variable VEIL_SHMEMCTL exists and that nothing else does
\echo TEST 1.1 ~ #1 *| *VEIL_SHMEMCTL#Initial variable listing
select count(*), max(name)
from   veil.veil_variables();
\echo - NOTE Test 1.1 will fail (correctly) if this is not a freshly created database
\echo -
-- Create session and shared integers
\echo TEST 1.2 = #42#create session int4
select veil.int4_set('sess_int4', 42);

\echo TEST 1.3 = #42#retrieve session int4
select veil.int4_get('sess_int4');

\echo 'TEST 1.4 ~ #sess_int4 *\\\| *Int4 *\\\| *f#list defined variables'
select * from veil.veil_variables();

\echo 'TEST 1.5 ~ #shared_int4 *\\\| *Undefined *\\\| *t#create shared int4'
select veil.share('shared_int4');
select * from veil.veil_variables();

\echo TEST 1.6 = #99#get shared int4
select coalesce(veil.int4_get('shared_int4'), 99);

\echo 'TEST 1.7 ~ #shared_int4 *\\\| *Int4 *\\\| *t#list defined variables'
select * from veil.veil_variables();

-- Create session and shared ranges
\echo TEST 1.8 ~ #ERROR.*mismatch#access non-existant session range
select * from veil.range('sess_range');

\echo TEST 1.9 = #14#create session range
select * from veil.init_range('sess_range', 11, 24);

\echo PREP
select veil.share('shared_range');
\echo TEST 1.10 = #1#create shared ranged
select * from veil.init_range('shared_range', 17, 17);

\echo TEST 1.11 ~ #5 *| *3#list defined variables
select count(*), sum(case when shared then 1 else 0 end) as shared
from   veil.veil_variables();

\echo - NOTE Test 1.11 will fail (correctly) if this is not a freshly created database
\echo -

\echo TEST 1.12 ~ #11 *| *24#show session range
select * from veil.range('sess_range');

\echo TEST 1.13 ~ #17 *| *17#show shared range
select * from veil.range('shared_range');

-- Create session and shared int4 arrays
\echo TEST 1.14 ~ #ERROR.*mismatch#fetch against undefined int4 array
select veil.int4array_get('sess_int4array', 12);

\echo TEST 1.15 ~ #ERROR.*mismatch#set against undefined int4 array
select veil.int4array_set('sess_int4array', 12, 14);

\echo TEST 1.16 = #t#create session int4 array
select veil.init_int4array('sess_int4array', 'shared_range');

\echo TEST 1.17 ~ #range error#attempt to set arrary element out of range
select veil.int4array_set('sess_int4array', 12, 14);

\echo TEST 1.18 = #14#set int4 array element
select veil.int4array_set('sess_int4array', 17, 14);

\echo TEST 1.19 = #14#fetch int4 array element
select veil.int4array_get('sess_int4array', 17);

\echo PREP
select veil.clear_int4array('sess_int4array');
\echo TEST 1.20 = #0#clear array and fetch element
select veil.int4array_get('sess_int4array', 17);

\echo PREP
select veil.share('shared_in4array');
select * from veil.init_int4array('shared_int4array', 'sess_range');
\echo TEST 1.21 = #14#define and fetch from shared int4 array
select veil.int4array_set('shared_int4array', 12, 14);

\echo TEST 1.22 = #14#shared int4 array get
select veil.int4array_get('shared_int4array', 12);

-- Serialise the integer and range variables for de-serialisation in the other
-- session.
\echo PREP
select veil.init_range('range2', 11, 24);
select veil.init_int4array('array2', 'range2');
select veil.clear_int4array('array2');
select veil.int4array_set('array2', 11, 24);
select veil.int4array_set('array2', 24, 11);
create table my_text (contents text);
insert into my_text(contents) select veil.serialise('sess_int4');
insert into my_text(contents) select veil.serialise('sess_range');
insert into my_text(contents) select veil.serialise('sess_int4array');
insert into my_text(contents) select veil.serialise('range2');
insert into my_text(contents) select veil.serialise('array2');

-- Note that bitmap types will be tested in their own regression test sets

\! $0 -T 1a

EOF
}

regress_1a()
{
    echo - 
    echo "- ...test set 1a: variables (second process)..."
    do_test 1a <<EOF	
-- Ensure that all expected shared variables still exist and that none of
-- the session variables do
\echo TEST 1.23 ~ #4 *| *4#list all shared variables
select count(*), sum(case when shared then 1 else 0 end) as shared
from   veil.veil_variables();
\echo - NOTE Test 1.23 may fail if this is not a freshly created database
\echo -

\echo TEST 1.24 = #99#get shared int4
select coalesce(veil.int4_get('shared_int4'), 99);

\echo TEST 1.25 = #0#Before de-serialising session variables
select sum(case when shared then 0 else 1 end) as session
from   veil.veil_variables();

\echo PREP
select veil.deserialise(contents) from my_text;
drop table my_text;

\echo TEST 1.25 = #5#Checking session variables are de-serialised
select sum(case when shared then 0 else 1 end) as session
from   veil.veil_variables();

\echo TEST 1.26 ~ #17 *| *17#Checking Range of range variable
select * from veil.range('shared_range');

\echo TEST 1.27 ~ #11 *| *24#Checking Range of range2 variable
select * from veil.range('range2');

\echo TEST 1.28 = #24#Checking array2 variable(1)
select * from veil.int4array_get('array2', 11);

\echo TEST 1.29 = #11#Checking array2 variable(2)
select * from veil.int4array_get('array2', 24);

\echo TEST 1.30 = #0#Checking array2 variable(3)
select * from veil.int4array_get('array2', 23);

EOF
}

regress_2()
{
    echo - ...test set 2: bitmaps...
    do_test 2 <<EOF
-- Load ranges for lookup_types
\echo PREP
select veil.init_range('privs_range', min(privilege_id),
                       max(privilege_id))
from   privileges;

select veil.init_range('roles_range', min(role_id), max(role_id))
from   roles;

-- Make a shared variable for privs_bmap
select veil.share('privs_bmap');

\echo TEST 2.1 ~ #OK#Initialise shared and session bitmaps
-- Create bitmaps for roles and privileges
select 'OK', 
       veil.init_bitmap('roles_bmap', 'roles_range'),
       veil.init_bitmap('privs_bmap', 'privs_range');

\echo TEST 2.2 = #3#Populate shared bitmap
-- Populate the privileges bitmap
select count(veil.bitmap_setbit('privs_bmap', privilege_id))
from   privileges;

\echo TEST 2.3 = #2#Populate session bitmap
-- Populate the roles bitmap
select count(veil.bitmap_setbit('roles_bmap', role_id))
from   roles;

-- Record the current bitmap details for testset 2a
\echo PREP
create table my_text (contents text);
insert into my_text(contents) select veil.serialise('roles_bmap');

-- Test for known true values
\echo TEST 2.4 = #t#Test for known true values in session and shared bitmaps
select veil.bitmap_testbit('roles_bmap', 10001) and 
       veil.bitmap_testbit('privs_bmap', 20070);

-- Test for known false value
\echo TEST 2.5 = #f#Test for known false value
select veil.bitmap_testbit('privs_bmap', 20071);

-- Test clear bitmap
\echo TEST 2.6 = #t#Clear sessionbitmap
select veil.clear_bitmap('roles_bmap');

-- Test for previous truth value
\echo TEST 2.7 = #f#Test for absence of previous true value in shared bitmap
select veil.bitmap_testbit('roles_bmap', 10001);

-- bitmap_range
\echo TEST 2.8 ~ #20001.*|.*20070#Test bitmap range
select * from veil.bitmap_range('privs_bmap');

-- bitmap_bits
\echo TEST 2.9 ~ #3.*|.*20070#Test bitmap bits
select count(*), max(bitmap_bits) from veil.bitmap_bits('privs_bmap');

-- bitmap_bits again
\echo TEST 2.10 = #20070#Further test bitmap bits
select * from veil.bitmap_bits('privs_bmap');

-- Clearbits using the shared bitmap
\! $0 -T 2a

\echo PREP
-- Union tests
-- Create bitmaps for roles and privileges
select veil.init_bitmap('privs_bmap', 'privs_range'),
       veil.init_bitmap('privs2_bmap', 'privs_range'),
       veil.init_bitmap('privs3_bmap', 'privs_range');

select veil.bitmap_setbit('privs2_bmap', 20001),
       veil.bitmap_setbit('privs2_bmap', 20002),
       veil.bitmap_setbit('privs3_bmap', 20002),
       veil.bitmap_setbit('privs3_bmap', 20003);

select veil.bitmap_union('privs_bmap', 'privs2_bmap'),
       veil.bitmap_union('privs_bmap', 'privs3_bmap');

\echo TEST 2.13 ~ #20001 *| *20003 *| *3#Check union of bitmaps
select min(bitmap_bits), max(bitmap_bits), count(*)
from   veil.bitmap_bits('privs_bmap');

-- Intersect tests
\echo PREP
select veil.bitmap_intersect('privs_bmap', 'privs2_bmap'),
       veil.bitmap_intersect('privs_bmap', 'privs3_bmap');

\echo TEST 2.14 ~ #20002 *| *20002 *| *1#Check bitmap intersection
select min(veil.bitmap_bits), max(veil.bitmap_bits), count(*)
from   veil.bitmap_bits('privs_bmap');

EOF
}

regress_2a()
{
    echo - 
    echo "- ...test set 2a: bitmaps (second process)..."
    do_test 2a <<EOF	
-- Clearbit test
\echo PREP
select veil.bitmap_clearbit('privs_bmap', 20070);
\echo TEST 2.11 != #20070#Check that shared bitmap has bit cleared
select * from veil.bitmap_bits('privs_bmap');

\echo TEST 2.12 = #2#Check that shared bitmap has other bits still set
select count(*) from veil.bitmap_bits('privs_bmap');

\echo PREP
select veil.deserialise(contents) from my_text;
drop table my_text;

-- Test for known true values
\echo TEST 2.13 = #t#Test for known true values in serialised session bitmap
select veil.bitmap_testbit('roles_bmap', 10001);


EOF
}


regress_3()
{
    echo - 
    echo "- ...test set 3: bitmap arrays..."
    do_test 3 <<EOF	
\echo PREP
-- Reset range variables
select veil.init_range('roles_range', min(role_id), max(role_id))
from   roles;

select veil.init_range('privs_range', min(privilege_id), max(privilege_id))
from   privileges;

-- Create a bitmap array - requires that range variables are set up 
\echo TEST 3.1 = #t#Create bitmap array
begin;
select veil.init_bitmap_array('role_privs', 'roles_range', 'privs_range');

-- Test a bit
\echo TEST 3.2 = #f#Test false bit
select veil.bitmap_array_testbit('role_privs', 10001, 20001);

-- Set a bit
\echo TEST 3.3 = #t#Set bit
select veil.bitmap_array_setbit('role_privs', 10001, 20001);

-- Test a bit
\echo TEST 3.4 = #t#Test newly true bit
select veil.bitmap_array_testbit('role_privs', 10001, 20001);

-- Clear the array
\echo TEST 3.5 = #t#Clear the aray
select veil.clear_bitmap_array('role_privs');

-- Test a bit
\echo TEST 3.6 = #f#Test that bit again
select veil.bitmap_array_testbit('role_privs', 10001, 20001);
commit;

-- Bitmap Ref tests
\echo PREP
select veil.share('shared_bitmap_ref');

\echo TEST 3.7 ~ #ERROR.*illegal#Attempt to create shared bitmap ref
select veil.bitmap_from_array('shared_bitmap_ref', 'role_privs', 10001);

\echo TEST 3.8 ~ #session_bitmap_refx#Create session bitmap ref
select veil.bitmap_from_array('session_bitmap_ref', 'role_privs', 10001) || 'x';

-- Check for bitmap ref not in same transaction
\echo TEST 3.9 ~ #ERROR.*not.*defined#Check for bitmap ref not in transaction
select veil.bitmap_setbit('session_bitmap_ref', 20001);

-- Now set some bits and display them
\echo PREP
begin;
select veil.bitmap_from_array('session_bitmap_ref', 'role_privs', 10001);

select veil.bitmap_setbit('session_bitmap_ref', 20001),
       veil.bitmap_setbit('session_bitmap_ref', 20003);

\echo TEST 3.10 ~ #2 *| *20001 *| *20003#Add bits thru ref, check bits in array
select count(*), min(bitmap_array_bits), max(bitmap_array_bits)
from   veil.bitmap_array_bits('role_privs', 10001);
commit;

-- Union tests
\echo PREP
begin;
select veil.bitmap_from_array('session_bitmap_ref', 'role_privs', 10002);

select veil.union_from_bitmap_array('session_bitmap_ref', 'role_privs', 10001);

\echo TEST 3.11 ~ #2 *| *20001 *| *20003#Union through ref, check bits in array
select count(*), min(bitmap_array_bits), max(bitmap_array_bits)
from   veil.bitmap_array_bits('role_privs', 10002);

\echo PREP
-- Intersect tests
select veil.bitmap_array_setbit('role_privs', 10002, 20002);
select veil.bitmap_array_clearbit('role_privs', 10002, 20001);
select veil.intersect_from_bitmap_array('session_bitmap_ref', 'role_privs', 10001);

\echo TEST 3.12 ~ #1 *| *20003 *| *20003#Intersect thru ref, check bits in array
select count(*), min(veil.bitmap_array_bits), max(veil.bitmap_array_bits)
from   veil.bitmap_array_bits('role_privs', 10002);

commit;

-- Record the current bitmap details for testset 2a
\echo PREP
create table my_text (contents text);
insert into my_text(contents) select veil.serialise('role_privs');

-- Test ranges of bitmap array
\echo TEST 3.13 ~ #10001.*10002#Check array range
select * from veil.bitmap_array_arange('role_privs');

\echo TEST 3.14 ~ #20001.*20070#Check bitmaps range
select * from veil.bitmap_array_brange('role_privs');

\echo PREP
-- Test shared bitmap array
select veil.share('shared_role_privs');
select veil.init_bitmap_array('shared_role_privs', 'roles_range', 'privs_range');
begin;
select veil.bitmap_from_array('session_bitmap_ref', 'shared_role_privs', 10002);
select veil.union_from_bitmap_array('session_bitmap_ref', 'role_privs', 10002);
commit;

\! $0 -T 3a

EOF
}

regress_3a()
{
    echo - 
    echo "- ...test set 3a: bitmap arrays (second process)..."
    do_test 3a <<EOF	

\echo TEST 3.15 ~ #1 *| *20003 *| *20003#Check bits in shared bitmap array
select count(*), min(veil.bitmap_array_bits), max(veil.bitmap_array_bits)
from   veil.bitmap_array_bits('shared_role_privs', 10002);

\echo PREP
select veil.deserialise(contents) from my_text;
drop table my_text;

\echo TEST 3.16 ~ #10001.*10002#Check array range after de-serialisation
select * from veil.bitmap_array_arange('role_privs');

\echo TEST 3.17 ~ #20001.*20070#Check bitmaps range after de-serialisation
select * from veil.bitmap_array_brange('role_privs');

\echo 'TEST 3.18 ~ #1 *\\\| *20003 *\\\| *20003#Check bits in array after de-ser.'
select count(*), min(bitmap_array_bits), max(bitmap_array_bits)
from   veil.bitmap_array_bits('role_privs', 10002);
EOF
}

regress_4()
{
    echo - 
    echo "- ...test set 4: bitmap hashes..."
    do_test 4 <<EOF	

\echo PREP
-- Reset range variables

select veil.init_range('privs_range', min(privilege_id), max(privilege_id))
from   privileges;

-- Create a bitmap hash - requires that range variables are set up 
\echo TEST 4.1 = #t#Create session bitmap hash
begin;
select veil.init_bitmap_hash('role_privs', 'privs_range');

-- Test a bit
\echo TEST 4.2 = #f#Check for known false
select veil.bitmap_hash_testbit('role_privs', 'wibble', 20001);

-- Set a bit
\echo TEST 4.3 = #t#Set a bit
select veil.bitmap_hash_setbit('role_privs', 'wibble', 20001);

-- Test a bit
\echo TEST 4.4 = #t#Check that it is now true
select veil.bitmap_hash_testbit('role_privs', 'wibble', 20001);

-- Record the current bitmap hash details for testset 4a
\echo PREP
create table my_text (contents text);
insert into my_text(contents) select veil.serialise('role_privs');

-- Clear the array
\echo TEST 4.5 = #t#Clear the hash
select veil.clear_bitmap_hash('role_privs');

-- Test a bit
\echo TEST 4.6 = #f#Check that bit again
select veil.bitmap_hash_testbit('role_privs', 'wibble', 20001);
commit;

-- Bitmap Ref tests

\echo PREP
select veil.share('shared_bitmap_ref');

\echo TEST 4.7 ~ #ERROR.*illegal#Attempt to create shared bitmap hash
select veil.bitmap_from_hash('shared_bitmap_ref', 'role_privs', 'wibble');

\echo TEST 4.8 ~ #session_bitmap_refx#Get bitmap ref from bitmap hash
select veil.bitmap_from_hash('session_bitmap_ref', 'role_privs', 'wibble') || 'x';

-- Check for bitmap ref not in same transaction
\echo TEST 4.9 ~ #ERROR.*not.*defined#Check ref in transaction
select veil.bitmap_setbit('session_bitmap_ref', 20001);

-- Now set some bits and display them
\echo PREP
begin;
select veil.bitmap_from_hash('session_bitmap_ref', 'role_privs', 'wibble');

select veil.bitmap_setbit('session_bitmap_ref', 20001),
       veil.bitmap_setbit('session_bitmap_ref', 20003);

\echo TEST 4.10 ~ #2 *| *20001 *| *20003#Test bits after setting them
select count(*), min(bitmap_hash_bits), max(bitmap_hash_bits)
from   veil.bitmap_hash_bits('role_privs', 'wibble');
commit;

\echo PREP
begin;
-- Union tests
select veil.bitmap_from_hash('session_bitmap_ref', 'role_privs', 'rubble');

select veil.union_from_bitmap_hash('session_bitmap_ref', 'role_privs', 'wibble');

\echo TEST 4.11 ~ #2 *| *20001 *| *20003#Union and then test bits
begin;
select count(*), min(bitmap_hash_bits), max(bitmap_hash_bits)
from   veil.bitmap_hash_bits('role_privs', 'rubble');

\echo PREP
-- Intersect tests
select veil.bitmap_hash_setbit('role_privs', 'rubble', 20002);
select veil.bitmap_hash_clearbit('role_privs', 'rubble', 20001);
select veil.intersect_from_bitmap_hash('session_bitmap_ref', 'role_privs', 'wibble');

\echo TEST 4.12 ~ #1 *| *20003 *| *20003#Intersect and test bits
select count(*), min(veil.bitmap_hash_bits), max(veil.bitmap_hash_bits)
from   veil.bitmap_hash_bits('role_privs', 'rubble');

commit;

-- Test ranges of bitmap hash

-- Output from the next test is split across two lines so we split the test
\echo TEST 4.13 ~ #wibble#Test bitmap hash entry (a)
select * from veil.bitmap_hash_entries('role_privs');

\echo TEST 4.14 ~ #rubble#Test bitmap hash entry (b)
select * from veil.bitmap_hash_entries('role_privs');

\echo TEST 4.15 ~ #20001.*20070#Test range of bitmaps in hash
select * from veil.bitmap_hash_range('role_privs');

-- Test shared bitmap array
\echo PREP
select veil.share('shared_role_privs2');

\echo TEST 4.16 ~ #ERROR.*illegal.*BitmapHash#Attempt to create shared bitmap hash
select veil.init_bitmap_hash('shared_role_privs2', 'privs_range');

-- Test union_into
\echo PREP
select veil.bitmap_hash_setbit('role_privs', 'rubble', 20002);
select veil.union_into_bitmap_hash('role_privs', 'rubble',
                 veil.bitmap_from_hash('session_bitmap_ref',
                                       'role_privs', 'wibble'));

\echo TEST 4.17 ~ #3.*20001.*20003#Test union into bitmap hash
select count(*), min(bitmap_hash_bits), max(bitmap_hash_bits)
from veil.bitmap_hash_bits('role_privs', 'rubble');

\echo TEST 4.18 ~ #truex#Check for defined bitmap in hash
select veil.bitmap_hash_key_exists('role_privs', 'rubble') || 'x';

\echo TEST 4.19 ~ #falsex#Check for undefined bitmap in hash
select veil.bitmap_hash_key_exists('role_privs', 'bubble') || 'x';
EOF

    do_test 4b <<EOF	
\echo PREP
select veil.deserialise(contents) from my_text;
drop table my_text;

-- Test a bit
\echo TEST 4.20 = #t#Check a bit in the deserialised bitmap hash
select veil.bitmap_hash_testbit('role_privs', 'wibble', 20001);

\echo TEST 4.21 ~ #1 *| *20003 *| *20003#Count those bits
select count(*), min(bitmap_hash_bits), max(bitmap_hash_bits)
from   veil.bitmap_hash_bits('role_privs', 'wibble');

EOF
}


regress_5()
{
    echo - 
    echo "- ...test set 5: control functions..."

    do_test 5 <<EOF	
\echo PREP IGNORE
-- Reload default version of init to check that it fails.
`define_veil`

\echo TEST 5.1 ~ #ERROR.*veil init#Default init
select veil.init_range('range_x', 1, 10);
EOF

    # Need a new session in order to perform init again
    do_test 5 <<EOF	
\echo PREP
-- Restore regression version of init
create or replace
function veil.veil_init(doing_reset bool) returns bool as '
begin
    return true;
end;
' language plpgsql;

\echo TEST 5.2 = #10#Demo init
select veil.init_range('range_x', 1, 10);

\echo PREP
\echo TEST 5.3 = #t#Reset once
select veil.veil_perform_reset();

\echo TEST 5.4 = #1#Check variables after reset
select count(*) from veil.veil_variables();

\echo TEST 5.5 = #t#Reset again
select veil.veil_perform_reset();

\echo TEST 5.6 = #2#Check variables again
select count(*) from veil.veil_variables();

-- Try performing a reset with the standard version of init.  This 
-- will prevent the reset from completing and leave the system in an
-- undesirable state.

\echo PREP IGNORE
-- Reload default version of init to check that it fails.
`define_veil`
\echo TEST 5.7 ~ #ERROR.*veil init#Reset with failing veil_init
select veil.veil_perform_reset();

-- Now subsequent attempts to perform reset will fail.
\echo TEST 5.8 ~ #^ *f *\$#Failing reset (context switch was left incomplete)
select veil.veil_perform_reset();

\echo TEST 5.9 ~ #WARNING.*reset#Again (looking for WARNING message)
select veil.veil_perform_reset();

\echo PREP IGNORE
-- Use force_reset to fix this problem.  This is not normally available in
-- Veil, so we have to explicitly make it available.
create or replace
function veil.veil_force_reset() returns bool
     as '@LIBPATH@', 'veil_force_reset'
     language C stable;

\echo TEST 5.10 ~ #^ *t *\$#Force reset
select veil.veil_force_reset();

-- And now try normal reset again
\echo PREP
-- Restore regression version of init
create or replace
function veil.veil_init(doing_reset bool) returns bool as '
begin
    return true;
end;
' language plpgsql;

\echo TEST 5.11 = #t#Reset again
select veil.veil_perform_reset();

-- Test that existing session retains its variables when reset is performed
-- by another session

\echo PREP
select veil.share('bitmap_x');
select veil.init_range('range_x', 1, 100);
select veil.init_bitmap('bitmap_x', 'range_x');
select veil.bitmap_setbit('bitmap_x', 2);
select veil.bitmap_setbit('bitmap_x', 3);
select veil.bitmap_setbit('bitmap_x', 5);
select veil.bitmap_setbit('bitmap_x', 7);
select veil.bitmap_setbit('bitmap_x', 11);
select veil.bitmap_setbit('bitmap_x', 13);
select veil.bitmap_setbit('bitmap_x', 17);
select veil.bitmap_setbit('bitmap_x', 19);
select veil.bitmap_setbit('bitmap_x', 23);
select veil.bitmap_setbit('bitmap_x', 29);
select veil.bitmap_setbit('bitmap_x', 31);
select veil.bitmap_setbit('bitmap_x', 37);
select veil.bitmap_setbit('bitmap_x', 41);
select veil.bitmap_setbit('bitmap_x', 43);
select veil.bitmap_setbit('bitmap_x', 47);
select veil.bitmap_setbit('bitmap_x', 53);
select veil.bitmap_setbit('bitmap_x', 59);
select veil.bitmap_setbit('bitmap_x', 61);
select veil.bitmap_setbit('bitmap_x', 67);
select veil.bitmap_setbit('bitmap_x', 71);
select veil.bitmap_setbit('bitmap_x', 73);
select veil.bitmap_setbit('bitmap_x', 79);
select veil.bitmap_setbit('bitmap_x', 83);
select veil.bitmap_setbit('bitmap_x', 89);
select veil.bitmap_setbit('bitmap_x', 97);
begin;
\echo TEST 5.12 = #25#Original bitmap before reset from other session
select count(*) from veil.bitmap_bits('bitmap_x');

\! $0 -T 5a

\echo - ...(back from 5a)...
\echo TEST 5.19 = #25#Original Bitmap (from original transaction)
select count(*) from veil.bitmap_bits('bitmap_x');
select * from veil.veil_variables();
commit;

\echo TEST 5.20 = #15#New bitmap created by other session
begin;
select count(*) from veil.bitmap_bits('bitmap_x');

\! $0 -T 5b

\echo - ...(back from 5b)...
\echo TEST 5.98 = #15#Original Bitmap (from original transaction)
select count(*) from veil.bitmap_bits('bitmap_x');
select * from veil.veil_variables();
commit;


\echo TEST 5.99 = #24#New bitmap created by other session
begin;
select count(*) from veil.bitmap_bits('bitmap_x');


EOF
}

regress_5a()
{
    echo - 
    echo "- ...test set 5a: reset from second session (second process)..."
    do_test 5a <<EOF	

\echo TEST 5.13 = #25#Bitmap variable from other session with bits
select count(*) from veil.bitmap_bits('bitmap_x');

\echo TEST 5.14 = #t#Successful reset
select count(*) from veil.bitmap_bits('bitmap_x');
select veil.veil_perform_reset();

\echo TEST 5.15 ~ #ERROR.*Undefined#No bitmap variable from other session
select veil.share('bitmap_x');
select count(*) from veil.bitmap_bits('bitmap_x');

\echo TEST 5.16 = #f#Failed reset as session still in use
select veil.veil_perform_reset();

\echo TEST 5.17 ~ #WARNING.*reset#Failed reset as session still in use (again)
select veil.veil_perform_reset();

\echo PREP
select veil.init_range('range_x', 1, 100);
select veil.init_bitmap('bitmap_x', 'range_x');
select veil.bitmap_setbit('bitmap_x', 1);
select veil.bitmap_setbit('bitmap_x', 4);
select veil.bitmap_setbit('bitmap_x', 6);
select veil.bitmap_setbit('bitmap_x', 8);
select veil.bitmap_setbit('bitmap_x', 10);
select veil.bitmap_setbit('bitmap_x', 14);
select veil.bitmap_setbit('bitmap_x', 16);
select veil.bitmap_setbit('bitmap_x', 60);
select veil.bitmap_setbit('bitmap_x', 64);
select veil.bitmap_setbit('bitmap_x', 72);
select veil.bitmap_setbit('bitmap_x', 75);
select veil.bitmap_setbit('bitmap_x', 78);
select veil.bitmap_setbit('bitmap_x', 82);
select veil.bitmap_setbit('bitmap_x', 88);
select veil.bitmap_setbit('bitmap_x', 95);

EOF
}

regress_5b()
{
    echo - 
    echo "- ...test set 5b: (like 5a but with opposite contexts)..."
    do_test 5b <<EOF	

\echo TEST 5.21 = #15#Bitmap variable from other session with bits
select count(*) from veil.bitmap_bits('bitmap_x');

\echo TEST 5.22 = #t#Successful reset
select count(*) from veil.bitmap_bits('bitmap_x');
select veil.veil_perform_reset();

\echo TEST 5.23 ~ #ERROR.*Undefined#No bitmap variable from other session
select veil.share('bitmap_x');
select count(*) from veil.bitmap_bits('bitmap_x');

\echo TEST 5.24 = #f#Failed reset as session still in use
select veil.veil_perform_reset();

\echo TEST 5.25 ~ #WARNING.*reset#Failed reset as session still in use (again)
select veil.veil_perform_reset();

\echo PREP
select veil.init_range('range_x', 1, 100);
select veil.init_bitmap('bitmap_x', 'range_x');
select veil.init_bitmap('bitmap_x', 'range_x');
select veil.bitmap_setbit('bitmap_x', 2);
select veil.bitmap_setbit('bitmap_x', 3);
select veil.bitmap_setbit('bitmap_x', 5);
select veil.bitmap_setbit('bitmap_x', 7);
select veil.bitmap_setbit('bitmap_x', 11);
select veil.bitmap_setbit('bitmap_x', 13);
select veil.bitmap_setbit('bitmap_x', 17);
select veil.bitmap_setbit('bitmap_x', 19);
select veil.bitmap_setbit('bitmap_x', 23);
select veil.bitmap_setbit('bitmap_x', 29);
select veil.bitmap_setbit('bitmap_x', 31);
select veil.bitmap_setbit('bitmap_x', 37);
select veil.bitmap_setbit('bitmap_x', 41);
select veil.bitmap_setbit('bitmap_x', 43);
select veil.bitmap_setbit('bitmap_x', 47);
select veil.bitmap_setbit('bitmap_x', 53);
select veil.bitmap_setbit('bitmap_x', 59);
select veil.bitmap_setbit('bitmap_x', 61);
select veil.bitmap_setbit('bitmap_x', 67);
select veil.bitmap_setbit('bitmap_x', 71);
select veil.bitmap_setbit('bitmap_x', 73);
select veil.bitmap_setbit('bitmap_x', 79);
select veil.bitmap_setbit('bitmap_x', 83);
select veil.bitmap_setbit('bitmap_x', 89);

EOF
}


regress_6()
{
    echo - 
    echo "- ...test set 6: veil_init() doing something useful..."

    do_test 6 <<EOF	
\echo PREP IGNORE
-- Reload default version of init to check that it fails.
`define_veil`
\echo TEST 6.1 ~ #ERROR.*veil init#Default init
select veil.init_range('range_x', 1, 10);

\echo PREP IGNORE
-- Multiple nnit functions with shared variables.  If they are
-- invoked in the wrong order, there will be failures
create or replace
function veil.veil_init1(bool) returns bool as '
begin
    perform veil.share(''shared1'');
    return true;
end
'
language plpgsql;

create or replace
function veil.veil_init2(bool) returns bool as '
begin
    perform veil.share(''shared2'');
    perform veil.int4_set(''shared1'', 123);
    return true;
end
'
language plpgsql;

insert into veil.veil_init_fns
       (fn_name, priority)
values ('veil.veil_init1', 1),
       ('veil.veil_init2', 2);

select veil.veil_init(true);

\echo TEST 6.2 ~ #shared2.*Undefined.*t#Undefined shared variable
select * from veil.veil_variables();
\echo TEST 6.3 ~ #shared1.*Int4.*t#Defined shared variable
select * from veil.veil_variables();

\echo PREP IGNORE
select veil.veil_perform_reset();

\echo TEST 6.4 ~ #shared2.*Undefined.*t#Undefined shared variable
select * from veil.veil_variables();
\echo TEST 6.5 ~ #shared1.*Int4.*t#Defined shared variable
select * from veil.veil_variables();

EOF
}


regress_7()
{
    echo - 
    echo "- ...test set 7: veil_demo..."

    do_test 7 <<EOF	
\echo PREP IGNORE
-- Reload default version of init to check that it fails.
`define_veil`

drop table if exists role_privileges;
drop table if exists roles;
drop table if exists privileges;

\echo TEST 7.1 ~ #ERROR.*veil init#Default init
select veil.init_range('range_x', 1, 10);

create extension veil_demo;

\echo TEST 7.2 ~ #10#veil_demo init
select veil.init_range('range_x', 1, 10);

\echo TEST 7.3 ~ #0#Not connected
select count(*) from persons;

\echo TEST 7.4 ~ #t#connect_person
select connect_person(4);

\echo TEST 7.5 ~ #1#non-privileged access
select count(*) from persons;

\echo PREP IGNORE
select connect_person(2);

\echo TEST 7.6 ~ #6#privileged access
select count(*) from persons;

\echo PREP IGNORE
select disconnect_person();

\echo TEST 7.7 ~ #0#Disconnect
select count(*) from persons;

-- Drop veil_demo extension
\echo PREP IGNORE
drop extension veil_demo;

\echo TEST 7.8 ~ #ERROR.*veil init#Drop veil_demo (default init)
select veil.veil_perform_reset();

EOF
}


tests_done()
{
    echo "COMPLETE"
}

db_regress()
{
    echo Performing regression tests...
    regress_1	# Variables
    regress_2	# Bitmaps
    regress_3   # Bitmap Arrays
    regress_4   # Bitmap Hashes
    regress_5   # Control Functions and context switches
    regress_6   # More context switches - tracking bug reported by Phil Frost
    regress_7   # Tests for specific bugs
    tests_done
}

db_test()
{
    db_create $1
    (db_build_schema | psql_clean regress.log) || bail building schema
    if [ "x$2" = "xtest" ]; then
	db_regress 2>&1 | psql_collate regress.log; STATUS=$?
	if [ "x$3" = "xdrop" ]; then
	    db_drop
	fi	
    fi
}

usage()
{
    prog=`basename $0`
    echo "
Usage: ${prog} -h
       ${prog} -c
       ${prog} -d
       ${prog} [-r] -b
       ${prog} [-r] -t <testset#>...
       ${prog} [-r] -T <testset#>...
       ${prog}

With no parameters, ${prog} creates a test database, runs all tests,
and finally drops the database.  If tests fail, the database will not be 
automatically dropped.

${prog} expects to be able to create a database locally called regressdb
using the psql command.  This must be available in \$PATH.

All tests are run using psql, with the output being parsed and tidied
for human consumption.  The raw output is available in regress.log in
case the parser eats something important.

The -h option prints this usage message.
-b causes the test database to just be created
-c runs the preload library check and reports on status
-d causes the test database to be dropped
-t runs specified test sets against an already created database
-T runs specified test sets against an already created database with 
no parsing and tidying of psql output.

If you cannot create regressdb locally, you are going to have to hack the 
script.  Sorry.
"

}

# Return 0 if eq, 1 if $1 > $2, 2 if $2 > $1
simple_cmp() 
{
    if [ $1 -eq $2 ]; then return 0; fi
    if [ $1 -gt $2 ]; then return 1; fi
    return 2
}

# Return 0 if eq, 1 if $1 > $2, 2 if $2 > $1
version_cmp() {
    version1=$1
    version2=$2
    major1=`echo ${version1} | cut -d. -f1`
    minor1=`echo ${version1} | cut -d. -f2`
    major2=`echo ${version2} | cut -d. -f1`
    minor2=`echo ${version2} | cut -d. -f2`
    simple_cmp $major1 $major2 && simple_cmp $minor1 $minor2
    return $?
}


# Check whether shared_preload_libraries has been defined appropriately.
# Returns the following result codes:
# 0 - shared_preload_libraries is defined correctly, or we are using
#     a version of postgres prior to 8.2 so no warnings are necessary
#     and we can use veil_interface.sql
# 1 - shared_preload_libraries has not been defined
# 2 - shared_preload_libraries does not contain veil shared library
# 3 - shared_preload_libraries references old (or missing) version of
#     veil shared library
#
check_preload_libraries()
{
    version=`psql --version | grep '[0-9]\.[0-9]'`
    version=`echo ${version} | sed -e 's/[^0-9]*\([0-9]*\.[0-9]*\).*/\1/'`
    # This is version 8.2 or higher, so we can expect some cooperation
    # from postgres wrt shared memory.
    # To get this cooperation the veil shared library must be preloaded
    # so, test whether this is being done.
    mylib=`dirname $0`/../veil.so
    setting=`psql -d template1 -q -t <<EOF
select setting from pg_settings where name = 'shared_preload_libraries';
EOF`
    libs=`echo "${setting}" | sed -e 's/,/ /g' | xargs -n1`
    if [ "x${libs}" = "x" ]; then
        # No definition of shared_preload_libraries
        return 1
    else
        echo "${libs}" | grep veil >/dev/null
        if [ $? -ne 0 ]; then
            # Definition of shared_preload_libraries does not contain veil
    	    return 2
        else
            echo "${libs}" | while read a; do
                cmp ${a} ${mylib} 2>/dev/null && echo FOUND
            done | grep FOUND >/dev/null; status=$?
            if [ ${status} -ne 0 ]; then
                # Definition of shared_preload_libraries has different 
    	        # version of veil library from that built locally
    		return 3
    	    fi
        fi
    fi	
}

preload_library_message()
{
    if [ "x$1" = "x1" -o "x$1" = "x2" ]; then
	echo "
WARNING: The veil shared library is not defined in shared_preload_libraries 
(which is defined in postgresql.conf) Without this definition you will be 
unable to define veil.dbs_in_cluster, veil.shared_hash_elems, and 
veil.shmem_context_size which will limit the amount of shared memory
available to veil."
    elif [ "x$1" = "x3" ]; then
	echo "
WARNING: The version of veil.so in shared_preload_libraries (defined in
postgresql.conf) is not up to date (not the same as the latest build).
"
    fi
}

# Load helper functions
export dir=`dirname $0`
. ${dir}/../tools/psql_funcs.sh

owner=`whoami`

export STATUS=0

if [ "x$1" = "x-h" ]; then
    usage; exit
elif [ "x$1" = "x-c" ]; then
    # Check and report on the preload library situation
    check_preload_libraries; preload_status=$?
    rm -f regress.log
    preload_library_message ${preload_status}
elif [ "x$1" = "x-d" ]; then
    # Drop the database
    db_drop
elif [ "x$1" = "x-b" ]; then
    # Just build the database
    check_preload_libraries; preload_status=$?
    rm -f regress.log
    db_test ${owner} ${preload_status}
    preload_library_message ${preload_status}
elif [ "x$1" = "x-n" ]; then
    # Don't drop the database after running the tests
    rm -f regress.log
    check_preload_libraries; preload_status=$?
    db_test ${owner} test 
    preload_library_message ${preload_status}
elif [ "x$1" = "x-t" ]; then
    # Run specific tests.  No build or drop.
    (
        while [ "x$2" != "x" ]; do 
	    regress_$2 2>&1 
	    shift
        done
        tests_done
    ) | psql_collate regress.log
    check_preload_libraries; preload_status=$?
    preload_library_message ${preload_status}
elif [ "x$1" = "x-T" ]; then
    # Run specific tests in raw form (no parser).  No build or drop.
    while [ "x$2" != "x" ]; do 
	regress_$2 2>&1
	shift
    done
else
    # Build, run all tests and drop.
    rm -f regress.log
    check_preload_libraries; preload_status=$?
    if [ ${preload_status} -gt 1 ]; then
	preload_library_message ${preload_status}
	echo "ERROR: Refusing to run tests as shared_preload_library is defined
but does not reference an up to date veil shared library." 1>&2
	exit 5
    fi
    db_test ${owner} test drop
    preload_library_message ${preload_status}
fi

exit ${STATUS}
