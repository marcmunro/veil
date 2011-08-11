/* ----------
 * veil_mainpage.c
 *
 *      Doxygen documentation root for Veil
 *
 *      Copyright (c) 2005 - 2011 Marc Munro
 *      Author:  Marc Munro
 *	License: BSD
 *
 */


/*! \mainpage Veil
\version 9.1.0 (Stable))
\section license License
BSD
\section intro_sec Introduction

Veil is a data security add-on for Postgres.  It provides an API
allowing you to control access to data at the row, or even column,
level.  Different users will be able to run the same query and see
different results.  Other database vendors describe this as a Virtual
Private Database.

\section Why Why do I need this?
If you have a database-backed application that stores sensitive data,
you will be taking at least some steps to protect that data.  Veil
provides a way of protecting your data with a security mechanism
within the database itself.  No matter how you access the database,
whether you are a legitimate user or not, you cannot by-pass Veil
without superuser privileges. 

\subsection Advantages Veil Advantages
By placing security mechanisms within the database itself we get a
number of advantages:
- Ubiquity.  Security is always present, no matter what application or
tool is used to connect to the database.  If your application is
compromised, your data is still protected by Veil.  If an intruder gets
past your outer defences and gains access to psql, your data is still
protected.
- Single Security Policy and Implementation.  If you have N applications
to secure, you have to implement your security policy N times.  With
Veil, all applications may be protected by a single implementation.
- Strength in Depth.  For the truly security conscious, Veil provides
yet another level of security.  If you want strength in depth, with
layers and layers of security like an onion, Veil gives you that extra
layer.
- Performance.  Veil is designed to be both flexible and efficient.
With a good implementation it is possible to build access controls with
a very low overhead, typically much lower than building the equivalent
security in each application.
- Cooperation.  The Veil security model is designed to cooperate with your
applications.  Although Veil is primarily concerned with data access
controls, it can also be used to provide function-level privileges.  If
your application has a sensitive function X, it can query the database,
through Veil functions, to ask the question, "Does the current user have
execute_X privilege?".  Also, that privilege can be managed in exactly
the same way as any other privilege.
- Flexibility.  Veil is a set of tools rather than a product.  How you
use it is up to you.

\subsection Limitations Veil Limitations
Veil can restrict the data returned by queries but cannot prevent the
query engine itself from seeing restricted data.  In particular,
functions executed during evaluation of the where clause of a query may
be able to see data that Veil is supposed to restrict access to.

As an example let's assume that we have a secured view, users, that
allows a user to see only their own record.  When Alice queries the
view, she will see this:

\verbatim
select * from users;

  userid  |   username
----------+-----------
   12345  |   Alice
\endverbatim

Alice should not be able to see any details for Bob or even, strictly
speaking, tell whether there is an entry for Bob.  This query though:

\verbatim
select * from users 
where 0 = 9 / (case username when 'Bob' then 0 else 1 end);
\endverbatim

will raise an exception if the where clause encounters the username
'Bob'.  So Alice can potentially craft queries that will enable her to
discover whether specific names appear in the database.

This is not something that Veil is intended to, or is able to, prevent.
Changes to the underlying query engine to attempt to plug such holes
have been proposed, but they all have their limitations and are likely
to lead to degraded performance.

A more serious problem occurs if a user is able to create user defined
functions as these can easily provide covert channels for leaking data.
Consider this query:

\verbatim
select * from users where leak_this(username);
\endverbatim

Although the query will only return what the secured view allows it to,
the where clause, if the function is deemed inexpensive enough, will see
every row in the table and so will be able to leak supposedly protected
data.  This type of exploit can be protected against easily enough by
preventing users from defining their own functions, however there are
postgres builtins that can be potentially be exploited in the same way.

\subsection GoodNews The Good News

The news is not all bad.  Although Veil can be circumvented, as shown
above, a database protected by Veil is a lot more secure than one which
is not.  Veil can provide extra security over and above that provided by
your application, and in combination with a well designed application
can provide security that is more than adequate for most commercial
purposes.

\section the-rest Veil Documentation
- \subpage overview-page
- \subpage API-page
- \subpage Building
- \subpage Demo 
- \subpage Management
- \subpage Esoteria
- \subpage install
- \subpage History
- \subpage Feedback
- \subpage Performance
- \subpage Credits

Next: \ref overview-page

*/
/*! \page overview-page Overview: a quick introduction to Veil

\section Overview-section Introduction
The section introduces a number of key concepts, and shows the basic
components of a Veil-protected system:
- \ref over-views
- \ref over-connections
- \ref over-privs
- \ref over-contexts
- \ref over-funcs2
- \ref over-roles

\subsection over-views Secured Views and Access Functions
Access controls are implemented using secured views and instead-of triggers.
Users connect to an account that has access only to the secured views.
For a table defined thus:
\verbatim
create table persons (
    person_id		integer not null,
    person_name		varchar(80) not null
);
\endverbatim
The secured view would be defined something like this:
\verbatim
create view persons(
       person_id, person_name) as
select person_id, person_name
  from persons
 where i_have_personal_priv(10013, person_id);
\endverbatim

A query performed on the view will return rows only for those persons
where the current user has privilege 10013
(<code>SELECT_PERSONS</code>).  We call the function
<code>i_have_personal_priv()</code>, an access function.  Such
functions are user-defined, and are used to determine whether the
connected user has a specific privilege in any of a number of security
contexts (see \ref over-contexts).  The example above is
taken from the Veil demo application (\ref demo-sec) and 
checks for privilege in the global and personal contexts.

\subsection over-connections The Connected User and Connection Functions
To determine a user's privileges, we have to know who that user is.
At the start of each database session the user must be identified, and
their privileges must be determined.  This is done by calling a
connection function, eg:
\verbatim
select connect_person('Wilma', 'AuthenticationTokenForWilma');
\endverbatim
The connection function performs authentication, and stores the user's
access privileges in Veil state variables.  These variables are then
interrogated by the access functions used in the secured views.

Prior to connection, or in the event of the connection failing, the
session will have no privileges and will probably be unable to see any
data.  Like access functions, connection functions are user-defined and 
may be written in any language supported by PostgreSQL.

\subsection over-privs Privileges
Veil-based systems define access rights in terms of privileges.  A
privilege is a named thing with a numerical value (actually, the name
is kind of optional).

An example will probably help.  Here is a definition of a privileges
table and a subset of its data:
\verbatim
create table privileges (
    privilege_id	integer not null,
    privilege_name	varchar(80) not null
);

copy privileges (privilege_id, privilege_name) from stdin;
10001	select_privileges
10002	insert_privileges
10003	update_privileges
10004	delete_privileges
. . .
10013	select_persons
10014	insert_persons
10015	update_persons
10016	delete_persons
10017	select_projects
10018	insert_projects
10019	update_projects
10020	delete_projects
. . .
10100	can_connect
\.

\endverbatim
Each privilege describes something that a user can do.  It is up to the
access and connection functions to make use of these privileges; the
name of the privilege is only a clue to its intended usage.  In the
example we might expect that a user that has not been given the
<code>can_connect</code> privilege would not be able to authenticate
using a connection function but this is entirely dependent on the
implementation.

\subsection over-contexts Security Contexts 

Users may be assigned privileges in a number of different ways.  They
may be assigned directly, indirectly through various relationships, or
may be inferred by some means.  To aid in the discussion and design of a
Veil-based security model we introduce the concept of security
contexts, and we say that a user has a given set of privileges in a
given context.  There are three types of security context:

 - Global Context.  This refers to privileges that a user has been given
   globally.  If a user has <code>select_persons</code> privilege in the
   global context, they will be able to select every record in the
   persons table.  Privileges in global context are exactly like
   database-level privileges: there is no row-level element to them.

 - Personal Context.  This context provides privileges on data that you
   may be said to own.  If you have <code>select_persons</code>
   privilege in only the personal context, you will only be able to
   select your own persons record.  Assignment of privileges in the
   personal context is often defined implicitly or globally, for all
   users, rather than granted explicitly to each user.  It is likely
   that everyone should have the same level of access to their own data
   so it makes little sense to have to explicitly assign the privileges
   for each individual user.

 - Relational Contexts.  These are the key to most row-level access
   controls.  Privileges assigned in a relational context are assigned
   through relationships between the connected user and the data to be
   accessed.  Examples of relational contexts include: assignments to
   projects, in which a user will gain access to project data only if
   they have been assigned to the project; and the management hierarchy
   within a business, in which a manager may have specific access to
   data about a staff member.  Note that determining a user's access
   rights in a relational context may require extra queries to be
   performed for each function call.  Your design should aim to minimise
   this.  Some applications may require several distinct relational 
   contexts.

\subsection over-funcs2 Access Functions and Security Contexts
Each access function will operate on privileges for a specific set of
contexts.  For some tables, access will only be through global context.
For others, it may be through global and personal as well as a number of
different relational contexts.  Here, from the demo application, are a
number of view definitions, each using a different access function that
checks different contexts.
\verbatim
create view privileges(
       privilege_id,
       privilege_name) as
select privilege_id,
       privilege_name
from   privileges
where  i_have_global_priv(10001);

. . .

create view persons(
       person_id,
       person_name) as
select person_id,
       person_name
from   persons
where  i_have_personal_priv(10013, person_id);

. . .

create view projects(
       project_id,
       project_name) as
select project_id,
       project_name
from   projects
where  i_have_project_priv(10017, project_id);

. . .

create view assignments (
       project_id,
       person_id,
       role_id) as
select project_id,
       person_id,
       role_id
from   assignments
where  i_have_proj_or_pers_priv(10025, project_id, person_id);
\endverbatim

In the <code>privileges</code> view, we only check for privilege in the
global context.  This is a look-up view, and should be visible to all
authenticated users.

The <code>persons</code> view checks for privilege in both the global
and personal contexts.  It takes an extra parameter identifying the
person who owns the record.  If that person is the same as the connected
user, then privileges in the personal context may be checked.  If not,
only the global context applies.

The <code>projects</code> view checks global and project contexts.  The
project context is a relational context.  In the demo application, a
user gains privileges in the project context through assignments.  An
assignment is a relationship between a person and a project.  Each 
assignment record has a role.  This role describes the set of privileges
the assignee (person) has within the project context.

The <code>assignments</code> view checks all three contexts (global,
personal and project).  An assignment contains data about a person and a
project so privileges may be acquired in either of the relational
contexts, or globally.

\subsection over-roles Grouping Privileges by Roles
Privileges operate at a very low-level.  In a database of 100 tables,
there are likely to be 500 to 1,000 privileges in use.  Managing
users access at the privilege level is, at best, tedious.  Instead, we
tend to group privileges into roles, and assign only roles to individual
users.  Roles act as function-level collections of privileges.  For
example, the role <code>project-readonly</code> might contain all of the
<code>select_xxx</code> privileges required to read all project data.

A further refinement allows roles to be collections of sub-roles.
Defining suitable roles for a system is left as an exercise for the
reader.

Next: \ref API-page

*/
/*! \page API-page The Veil API
\section API-sec The Veil API
This section describes the Veil API.  It consists of the following
sections

- \ref API-intro
- \subpage API-variables
- \subpage API-simple
- \subpage API-bitmaps
- \subpage API-bitmap-arrays
- \subpage API-bitmap-hashes
- \subpage API-int-arrays
- \subpage API-serialisation
- \subpage API-control

Note that all veil objects are placed in the veil schema.

\section API-intro Veil API Overview
Veil is an API that simply provides a set of state variable types, and 
operations on those variable types, which are optimised for privilege
examination and manipulation.

The fundamental data type is the bitmap.  Bitmaps are used to
efficiently record and test sets of privileges.  Bitmaps may be combined
into bitmap arrays, which are contiguous groups of bitmaps indexed by
integer, and bitmap hashes which are non-contiguous and may be indexed
by text strings.

In addition to the bitmap-based types, there are a small number of
support types that just help things along.  If you think you have a case
for defining a new type, please 
\ref Feedback "contact" 
the author.

Next: \ref API-variables
*/
/*! \page API-variables Variables
Veil variables exist to record session and system state.  They retain
their values across transactions.  Variables may be defined as either
session variables or shared variables.

All variables are referenced by name; the name of the variable is
passed as a text string to Veil functions.

Session variables are private to the connected session.  They are
created when first referenced and, once defined, their type is set for
the life of the session.

Shared variables are global across all sessions.  Once a shared variable
is defined, all sessions will have access to it.  Shared variables are
defined in two steps.  First, the variable is defined as shared, and
then it is initialised and accessed in the same way as for session
variables.  Note that shared variables should only be created within
\ref API-control-registered-init or \ref API-control-init.

Note that bitmap refs and bitmap hashes may not be stored in shared
variables.

The following types of variable are supported by Veil, and are described
in subsequent sections:
- integers 
- ranges
- bitmaps
- bitmap refs
- bitmap arrays
- bitmap hashes
- integer arrays

The following functions comprise the Veil variables API:

- <code>\ref API-variables-share</code>
- <code>\ref API-variables-var</code>

Note again that session variables are created on usage.  Their is no
specific function for creating a variable in the variables API.  For an
example of a function to create a variable see \ref API-bitmap-init.

\section API-variables-share share(name text)
\verbatim
function veil.share(name text) returns bool
\endverbatim

Implemented by C function veil_share(), this is used to define a
specific variable as being shared.  A shared variable is accessible to
all sessions and exists to reduce the need for multiple copies of
identical data.  For instance in the Veil demo, role_privileges are
recorded in a shared variable as they will be identical for all
sessions, and to create a copy for each session would be an unnecessary
overhead.  This function should only be called from 
\ref API-control-registered-init or \ref API-control-init. 

\section API-variables-var veil_variables()
\verbatim
function veil.veil_variables() returns setof veil_variable_t
\endverbatim

This function, implemented by C function veil_variables(), returns a
description for each variable known to the session.  It provides the
name, the type, and whether the variable is shared.  It is primarily
intended for interactive use when developing and debugging Veil-based
systems.

Next: \ref API-simple
*/
/*! \page API-simple Basic Types: Integers and Ranges

Veil's basic types are those that do not contain repeating groups
(arrays, hashes, etc).  

Ranges, implemented by the type \ref veil_range_t,
consist of a pair of values and are generally used to initialise the
bounds of array and bitmap types.

\anchor veil_range_t \ref veil_range_t is defined as:

\verbatim
create type veil.veil_range_t as (
    min  int4,
    max  int4
);
\endverbatim

Ranges may not contain nulls.

The int4 type is used to record a simple nullable integer.  This is
typically used to record the id of the connected user in a session.

The following functions comprise the Veil basic types API:

- <code>\ref API-basic-init-range</code>
- <code>\ref API-basic-range</code>
- <code>\ref API-basic-int4-set</code>
- <code>\ref API-basic-int4-get</code>

\section API-basic-init-range init_range(name text, min int4, max int4)
\verbatim
function veil.init_range(name text, min int4, max int4) returns int4
\endverbatim

This, implemented by veil_init_range() defines a range, and returns the
extent of that range.

\section API-basic-range range(name text)
\verbatim
function veil.range(name text) returns veil.range_t
\endverbatim

This, implemented by C function veil_range() returns the contents
of a range.  It is intended primarily for interactive use.

\section API-basic-int4-set int4_set(name text, value int4)
\verbatim
function veil.int4_set(name text, value int4) returns int4
\endverbatim

Sets an int4 variable to a value, returning that same value.  It is
implemented by C function veil_int4_set().

\section API-basic-int4-get int4_get(name text)
\verbatim
function veil.int4_get(name text) returns int4
\endverbatim

Returns the value of the int4 variable given by name.  Implemented by C
function veil_int4_get().


Next: \ref API-bitmaps
*/
/*! \page API-bitmaps Bitmaps and Bitmap Refs
Bitmaps are used to implement bounded sets.  Each bit in the bitmap may
be on or off representing presence or absence of a value in the set.
Typically bitmaps are used to record sets of privileges.

A bitmap ref is a variable that may temporarily reference another
bitmap.  These are useful for manipulating specific bitmaps within
bitmap arrays or bitmap hashes.  All bitmap operations except for \ref
API-bitmap-init may take the name of a bitmap ref instead of a bitmap.

Bitmap refs may not be shared, and the reference is only accessible
within the transaction that created it.  These restrictions exist to
eliminate the possibility of references to deleted objects or to objects
from other sessions.

The following functions comprise the Veil bitmaps API:

- <code>\ref API-bitmap-init</code>
- <code>\ref API-bitmap-clear</code>
- <code>\ref API-bitmap-setbit</code>
- <code>\ref API-bitmap-clearbit</code>
- <code>\ref API-bitmap-testbit</code>
- <code>\ref API-bitmap-union</code>
- <code>\ref API-bitmap-intersect</code>
- <code>\ref API-bitmap-bits</code>
- <code>\ref API-bitmap-range</code>

\section API-bitmap-init init_bitmap(bitmap_name text, range_name text)
\verbatim
function veil.init_bitmap(bitmap_name text, range_name text) returns bool
\endverbatim
This is used to create or resize a bitmap.  The first parameter provides
the name of the bitmap, the second is the name of a range variable that
will govern the size of the bitmap.  It is implemented by C function
veil_init_bitmap().

\section API-bitmap-clear clear_bitmap(bitmap_name text)
\verbatim
function veil.clear_bitmap(bitmap_name text) returns bool
\endverbatim
This is used to clear (set to zero) all bits in the bitmap.  It is
implemented by C function veil_clear_bitmap().

\section API-bitmap-setbit bitmap_setbit(bitmap_name text, bit_number int4)
\verbatim
function veil.bitmap_setbit(bitmap_name text, bit_number int4) returns bool
\endverbatim
This is used to set a specified bit, given by bit_number in the bitmap
identified by bitmap_name.  It is implemented by C function
veil_bitmap_setbit().

\section API-bitmap-clearbit bitmap_clearbit(bitmap_name text, bit_number int4)
\verbatim
function veil.bitmap_clearbit(bitmap_name text, bit_number int4) returns bool
\endverbatim
This is used to clear (set to zero) a specified bit in a bitmap.  It is
implemented by C function veil_bitmap_clearbit().

\section API-bitmap-testbit bitmap_testbit(bitmap_name text, bit_number int4)
\verbatim
function veil.bitmap_testbit(bitmap_name text, bit_number int4) returns bool
\endverbatim
This is used to test a specified bit in a bitmap.  It returns true if
the bit is set, false otherwise.  It is implemented by C function
veil_bitmap_testbit().

\section API-bitmap-union bitmap_union(result_name text, bm2_name text)
\verbatim
function veil.bitmap_union(result_name text, bm2_name text) returns bool
\endverbatim
Form the union of two bitmaps with the result going into the first.
Implemented by C function veil_bitmap_union().

\section API-bitmap-intersect bitmap_intersect(result_name text, bm2_name text)
\verbatim
function veil.bitmap_intersect(result_name text, bm2_name text) returns bool
\endverbatim
Form the intersection of two bitmaps with the result going into the
first.  Implemented by C function veil_bitmap_intersect().

\section API-bitmap-bits bitmap_bits(bitmap_name text)
\verbatim
function veil.bitmap_bits(bitmap_name text) returns setof int4
\endverbatim
This is used to list all bits set within a bitmap.  It is primarily for
interactive use during development and debugging of Veil-based systems.
It is implemented by C function veil_bitmap_bits().

\section API-bitmap-range bitmap_range(bitmap_name text)
\verbatim
function veil.bitmap_range(bitmap_name text) returns veil.range_t
\endverbatim
This returns the range, as a \ref veil_range_t, of a
bitmap.  It is primarily intended for interactive use.  It is
implemented by C function veil_bitmap_range().

Next: \ref API-bitmap-arrays
*/
/*! \page API-bitmap-arrays Bitmap Arrays
A bitmap array is an array of identically-ranged bitmaps, indexed
by an integer value.  They are initialised using two ranges, one for the
range of each bitmap, and one providing the range of indices for the
array.

Typically bitmap arrays are used for collections of privileges, where
each element of the collection is indexed by something like a role_id.

The following functions comprise the Veil bitmap arrays API:

- <code>\ref API-bmarray-init</code>
- <code>\ref API-bmarray-clear</code>
- <code>\ref API-bmarray-bmap</code>
- <code>\ref API-bmarray-testbit</code>
- <code>\ref API-bmarray-setbit</code>
- <code>\ref API-bmarray-clearbit</code>
- <code>\ref API-bmarray-union</code>
- <code>\ref API-bmarray-intersect</code>
- <code>\ref API-bmarray-bits</code>
- <code>\ref API-bmarray-arange</code>
- <code>\ref API-bmarray-brange</code>

\section API-bmarray-init init_bitmap_array(bmarray text, array_range text, bitmap_range text)
\verbatim
function veil.init_bitmap_array(bmarray text, array_range text, bitmap_range text) returns bool
\endverbatim
Creates or resets (clears) the bitmap array named <code>bmarray</code>.
The last two parameters are the names of ranges used to bound the
dimensions of the array, and the range of bits within the array's
bitmaps.  Implemented by C function veil_init_bitmap_array().

\section API-bmarray-clear clear_bitmap_array(bmarray text)
\verbatim
function veil.clear_bitmap_array(bmarray text) returns bool
\endverbatim
Clear all bits in all bitmaps of the bitmap array named
<code>bmarray</code>.  Implemented by C function veil_clear_bitmap_array().

\section API-bmarray-bmap bitmap_from_array(bmref_name text, bmarray text, index int4)
\verbatim
function veil.bitmap_from_array(bmref_name text, bmarray text, index int4) returns text
\endverbatim
Place a reference into <code>bmref_name</code> to the bitmap identified
by <code>index</code> in bitmap array <code>bmarray</code>.  Implemented
by C function veil_bitmap_from_array().

\section API-bmarray-testbit bitmap_array_testbit(bmarray text, arr_idx int4, bitno int4)
\verbatim
function veil.bitmap_array_testbit(bmarray text, arr_idx int4, bitno int4) returns bool
\endverbatim
Test a specific bit in a bitmap array.  Implemented by C function
veil_bitmap_array_testbit().

\section API-bmarray-setbit bitmap_array_setbit(bmarray text, arr_idx int4, bitno int4)
\verbatim
function veil.bitmap_array_setbit(bmarray text, arr_idx int4, bitno int4) returns bool
\endverbatim
Set a specific bit in a bitmap array.  Implemented by C function
veil_bitmap_array_setbit().

\section API-bmarray-clearbit bitmap_array_clearbit(bmarray text, arr_idx int4, bitno int4)
\verbatim
function veil.bitmap_array_clearbit(bmarray text, arr_idx int4, bitno int4) returns bool
\endverbatim
Clear a specific bit in a bitmap array.  Implemented by C function
veil_bitmap_array_clearbit().

\section API-bmarray-union union_from_bitmap_array(bitmap text, bmarray text, arr_idx int4)
\verbatim
function veil.union_from_bitmap_array(bitmap text, bmarray text, arr_idx int4) returns bool
\endverbatim
Union a bitmap with a specified bitmap from an array, with the result in
the bitmap.    Implemented by C function
veil_union_from_bitmap_array().  This is a faster shortcut for the
following logical construction:

\verbatim
veil.bitmap_union(<bitmap>, veil.bitmap_from_array(<bitmap_array>, <index>))
\endverbatim

\section API-bmarray-intersect intersect_from_bitmap_array(bitmap text, bmarray text, arr_idx int4)
\verbatim
function veil.intersect_from_bitmap_array(bitmap text, bmarray text, arr_idx int4) returns bool
\endverbatim
Intersect a bitmap with a specified bitmap from an array, with the result in
the bitmap.    Implemented by C function
veil_intersect_from_bitmap_array().  This is a faster shortcut for the
following logical construction:

\verbatim
veil.bitmap_intersect(<bitmap>, veil.bitmap_from_array(<bitmap_array>,<index>))
\endverbatim

\section API-bmarray-bits bitmap_array_bits(bmarray text, arr_idx int4)
\verbatim
function veil.bitmap_array_bits(bmarray text, arr_idx int4) returns setof int4
\endverbatim
Show all bits in the specific bitmap within an array.  This is primarily
intended for interactive use when developing and debugging Veil-based
systems.  Implemented by C function veil_bitmap_array_bits().

\section API-bmarray-arange bitmap_array_arange(bmarray text)
\verbatim
function veil.bitmap_array_arange(bmarray text) returns veil_range_t
\endverbatim
Return the range of array indices, as a \ref veil_range_t, for the
specified bitmap array.  Primarily for interactive use.  Implemented by
C function veil_bitmap_array_arange().

\section API-bmarray-brange bitmap_array_brange(bmarray text)
\verbatim
function veil.bitmap_array_brange(bmarray text) returns veil_range_t
\endverbatim
Show the range, as a \ref veil_range_t, of all bitmaps in the specified
bitmap array.  Primarily for interactive use.  Implemented by
C function veil_bitmap_array_range().


Next: \ref API-bitmap-hashes
*/
/*! \page API-bitmap-hashes Bitmap Hashes
A bitmap hashes is a hash table of identically-ranged bitmaps, indexed
by a text key.

Typically bitmap hashes are used for sparse collections of privileges.

Note that bitmap hashes may not be stored in shared variables as hashes
in shared memory are insufficiently dynamic.

The following functions comprise the Veil bitmap hashes API:

- <code>\ref API-bmhash-init</code>
- <code>\ref API-bmhash-clear</code>
- <code>\ref API-bmhash-key-exists</code>
- <code>\ref API-bmhash-from</code>
- <code>\ref API-bmhash-testbit</code>
- <code>\ref API-bmhash-setbit</code>
- <code>\ref API-bmhash-clearbit</code>
- <code>\ref API-bmhash-union-into</code>
- <code>\ref API-bmhash-union-from</code>
- <code>\ref API-bmhash-intersect-from</code>
- <code>\ref API-bmhash-bits</code>
- <code>\ref API-bmhash-range</code>
- <code>\ref API-bmhash-entries</code>

\section API-bmhash-init init_bitmap_hash(bmhash text, range text)
\verbatim
function veil.init_bitmap_hash(bmhash text, range text) returns bool
\endverbatim
Creates, or resets, a bitmap hash.  Implemented by
C function veil_init_bitmap_hash().

\section API-bmhash-clear clear_bitmap_hash(bmhash text)
\verbatim
function veil.clear_bitmap_hash(bmhash text) returns bool
\endverbatim
Clear all bits in all bitmaps of a bitmap hash.  Implemented by
C function veil_clear_bitmap_hash().  Implemented by
C function veil_clear_bitmap_hash().

\section API-bmhash-key-exists bitmap_hash_key_exists(bmhash text, key text)
\verbatim
function veil.bitmap_hash_key_exists(bmhash text, key text) returns bool
\endverbatim
Determine whether a given key exists in the hash (contains a bitmap).
Implemented by C function veil_bitmap_hash_key_exists().

\section API-bmhash-from bitmap_from_hash(bmref text, bmhash text, key text)
\verbatim
function veil.bitmap_from_hash(bmref text, bmhash text, key text) returns text
\endverbatim
Generate a reference to a specific bitmap in a bitmap hash.  Implemented by
C function veil_bitmap_from_hash().

\section API-bmhash-testbit bitmap_hash_testbit(bmhash text, key text, bitno int4)
\verbatim
function veil.bitmap_hash_testbit(bmhash text, key text, bitno int4) returns bool
\endverbatim
Test a specific bit in a bitmap hash.  Implemented by
C function veil_bitmap_hash_testbit().

\section API-bmhash-setbit bitmap_hash_setbit(bmhash text, kay text, bitno int4)
\verbatim
function veil.bitmap_hash_setbit(bmhash text, key text, bitno int4) returns bool
\endverbatim
Set a specific bit in a bitmap hash.  Implemented by
C function veil_bitmap_hash_setbit().

\section API-bmhash-clearbit bitmap_hash_clearbit(bmhash text, key text, bitno int4)
\verbatim
function veil.bitmap_hash_clearbit(bmhash text, key text, bitno int4) returns bool
\endverbatim
Clear a specific bit in a bitmap hash.  Implemented by
C function veil_bitmap_hash_clearbit().

\section API-bmhash-union-into union_into_bitmap_hash(bmhash text, key text, bitmap text)
\verbatim
function veil.union_into_bitmap_hash(bmhash text, key text, bitmap text) returns bool
\endverbatim
Union a specified bitmap from a hash with a bitmap, with the result in
the bitmap hash.  Implemented by C function
veil_union_into_bitmap_hash().  This is a faster shortcut for the
following logical construction:

\verbatim
veil.bitmap_union(veil.bitmap_from_hash(<bitmap_hash>, <key>), <bitmap>)
\endverbatim

\section API-bmhash-union-from union_from_bitmap_hash(bmhash text, key text, bitmap text)
\verbatim
function veil.union_from_bitmap_hash(bmhash text, key text, bitmap text) returns bool
\endverbatim
Union a bitmap with a specified bitmap from a hash, with the result in
the bitmap.  Implemented by C function veil_union_from_bitmap_hash().
This is a faster shortcut for the following logical construction:

\verbatim
veil.bitmap_union(<bitmap>, veil.bitmap_from_hash(<bitmap_array>, <key>))
\endverbatim

\section API-bmhash-intersect-from intersect_from_bitmap_hash(bitmap text, bmhash text, key text)
\verbatim
function veil.intersect_from_bitmap_hash(bitmap text, bmhash text, key text) returns bool
\endverbatim
Intersect a bitmap with a specified bitmap from a hash, with the result
in the bitmap.  Implemented by C function
veil_intersect_from_bitmap_hash().  This is a faster shortcut for the
following logical construction:

\verbatim
veil.bitmap_intersect(<bitmap>, veil.bitmap_from_hash(<bitmap_array>, <key>))
\endverbatim

\section API-bmhash-bits bitmap_hash_bits(bmhash text, key text)
\verbatim
function veil.bitmap_hash_bits(bmhash text, key text) returns setof int4
\endverbatim
Show all bits in the specific bitmap within a hash.  This is primarily
intended for interactive use when developing and debugging Veil-based
systems.  Implemented by C function veil_bitmap_hash_bits().

\section API-bmhash-range bitmap_hash_range(bmhash text)
\verbatim
function veil.bitmap_hash_range(bmhash text) returns veil_range_t
\endverbatim
Show the range, as a \ref veil_range_t, of all bitmaps in the hash.
Primarily intended for interactive use.   Implemented by
C function veil_bitmap_hash_range().

\section API-bmhash-entries bitmap_hash_entries(bmhash text)
\verbatim
function veil.bitmap_hash_entries(bmhash text) returns setof text
\endverbatim
Show every key in the hash.  Primarily intended for interactive use.
Implemented by C function veil_bitmap_hash_entries().

Next: \ref API-int-arrays
*/
/*! \page API-int-arrays Integer Arrays
Integer arrays are used to store simple mappings of keys to values.  In
the Veil demo (\ref demo-sec) they are used to record the extra privilege
required to access person_details and project_details of each
detail_type: the integer array being used to map the detail_type_id to
the privilege_id.

Note that integer array elements cannot be null.

The following functions comprise the Veil int arrays API:

- <code>\ref API-intarray-init</code>
- <code>\ref API-intarray-clear</code>
- <code>\ref API-intarray-set</code>
- <code>\ref API-intarray-get</code>

\section API-intarray-init init_int4array(arrayname text, range text)
\verbatim
function veil.init_int4array(arrayname text, range text) returns bool
\endverbatim
Creates, or resets the ranges of, an int array.  Implemented by
C function veil_init_int4array().

\section API-intarray-clear clear_int4array(arrayname text)
\verbatim
function veil.clear_int4array(arrayname text) returns bool
\endverbatim
Clears (zeroes) an int array.  Implemented by
C function veil_clear_int4array().

\section API-intarray-set int4array_set(arrayname text, idx int4, value int4)
\verbatim
function veil.int4array_set(arrayname text, idx int4, value int4) returns int4
\endverbatim
Set the value of an element in an int array.  Implemented by
C function veil_int4array_set().

\section API-intarray-get int4array_get(arrayname text, idx int4)
\verbatim
function int4array_get(arrayname text, idx int4) returns int4
\endverbatim
Get the value of an element from an int array.  Implemented by
C function veil_int4array_get().

Next: \ref API-serialisation
*/
/*! \page API-serialisation Veil Serialisation Functions
With modern web-based applications, database connections are often
pooled, with each connection representing many different users.  In
order to reduce the overhead of connection functions for such
applications, Veil provides a serialisation API.  This allows session
variables for a connected user to be saved for subsequent re-use.  This
is particularly effective in combination with pgmemcache  
http://pgfoundry.org/projects/pgmemcache/

Only session variables may be serialised.

The following functions comprise the Veil serialisatation API:

- <code>\ref API-serialise</code>
- <code>\ref API-deserialise</code>
- <code>\ref API-serialize</code>
- <code>\ref API-deserialize</code>

\section API-serialise serialise(varname text)
\verbatim
function veil.serialise(varname text) returns text
\endverbatim
This creates a serialised textual representation of the named session
variable.  The results of this function may be concatenated into a
single string, which can be deserialised in a single call to
veil_deserialise().  Implemented by C function veil_serialise().

\section API-deserialise deserialise(stream text)
\verbatim
function veil.deserialise(stream text) returns text
\endverbatim
This takes a serialised representation of one or more variables as
created by concatenating the results of veil_serialise(), and
de-serialises them, creating new variables as needed and resetting their
values to those they had when they were serialised.  Implemented by C
function veil_deserialise().

\section API-serialize serialize(varname text)
\verbatim
function veil.serialize(varname text) returns text
\endverbatim
Synonym for veil_serialise()

\section API-deserialize deserialize(stream text)
\verbatim
function veil.deserialize(stream text) returns text
\endverbatim
Synonym for veil_deserialise()

Next: \ref API-control
*/
/*! \page API-control Veil Control Functions
Veil generally requires no management.  The exception to this is when
you wish to reset shared variables.  You may wish to do this because
your underlying security definitions have changed, or because you have
added new features.  In this case, you may use veil_perform_reset() to
re-initialise your shared variables.  This function replaces the current
set of shared variables with a new set in a transaction-safe manner.
All current transactions will complete with the old set of variables in
place.  All subsequent transactions will see the new set.

The following functions comprise the Veil control functions API:

- <code>\ref API-control-registered-init</code>
- <code>\ref API-control-init</code>
- <code>\ref API-control-reset</code>
- <code>\ref API-version</code>

\section API-control-registered-init registered initialisation functions

A registered initialisation function is one which will be called from
the standard veil \ref API-control-init function.  Such functions are
responsible for defining and, usually, initialising shared variables.

Initialisation functions may be written in any language supported by
PostgreSQL, and must conform to the following function prototype:

\verbatim
function init_function(doing_reset bool) returns bool
\endverbatim

The <code>doing_reset</code> parameter will be set to true if we are
completely resetting veil and redefining all of its variables.  In this
case, we must declare and, probably, initialise shared variables prior to 
any session initialisation actions.  The parameter will be false, if the
function is solely being called to initialise a new session.  Check \ref
demo-sec for an example. 

Initialisation functions are registered by inserting their name into
the configuration table <code>veil.veil_init_fns</code>.  The functions
listed in this table are executed in order of the <code>priority</code>
column.  Eg, to register <code>veil.init_roles()</code> to execute
before <code>veil.init_role_privs()</code>, we would use the following
insert statement:

\verbatim
insert into veil.veil_init_fns
       (fn_name, priority)
values ('veil.init_roles', 1),
       ('veil.init_role_privs', 2);
\endverbatim

\section API-control-init veil_init(doing_reset bool)
\verbatim
function veil.veil_init(doing_reset bool) returns bool
\endverbatim

This function, implemented by the C function veil_init(), is reponsible
for initialising each veil session.  The <code>doing_reset</code>
parameter is true if we are to completely reset Veil, redefining all
shared variables.

The builtin implementation of veil_init() will call each registered
initialisation function (see \ref API-control-registered-init) in turn.

If no initialisation functions are registered, veil_init() raises an
exception.

As an alternative to registering initialisation functions, a Veil-based
application may instead simply redefine veil.veil_init(), though this
usage is deprecated.

\section API-control-reset veil_perform_reset()
\verbatim
function veil.veil_perform_reset() returns bool
\endverbatim
This is used to reset Veil's shared variables.  It causes \ref
API-control-init to be called.  Implemented by C function veil_perform_reset().

\section API-version version()
\verbatim
function veil.version() returns text
\endverbatim
This function returns a string describing the installed version of
veil.  Implemented by C function veil_version().

Next: \ref Building

*/
/*! \page Building Building a Veil-based secure database
\section Build-sec Building a Veil-based secure database

This section describes the steps necessary to secure a database using
Veil.  The steps are:
- \ref Policy
- \ref Schemas
- \ref Design
- \ref Implementation
- \ref Implementation2
- \ref Implementation3
- \ref Implementation4
- \ref Testing

\subsection Policy Determine your Policies

You must identify which security contexts exist for your application,
and how privileges should be assigned to users in those contexts.  You
must also figure out how privileges, roles, and the assignment of roles
to users are to be managed.  You must identify each object that is to be
protected by Veil, identify the security contexts applicable for that
object, and determine the privileges that will apply to each object in
each possible mode of use.  Use the Veil demo application (\ref
demo-sec) as a guide.

For data access controls, typically you will want specific privileges
for select, insert, update and delete on each table.  You may also want
separate admin privileges that allow you to grant those rights.

At the functional level, you will probably have an execute privilege for
each callable function, and you will probably want similar privileges
for individual applications and components of applications.  Eg, to
allow the user to execute the role_manager component of admintool, you
would probably create a privilege called
<code>exec_admintool_roleman</code>.

The hardest part of this is figuring out how you will securely manage
these privileges.  A useful, minimal policy is to not allow anyone to
assign a role that they themselves have not been assigned.

\subsection Schemas Design Your Database-Level Security

Veil operates within the security provided by PostgreSQL.  If you wish
to use Veil to protect underlying tables, then those tables must not be
directly accessible to the user.  Also, the Veil functions themselves,
as they provide privileged operations, must not be accessible to user
accounts. 

A sensible basic division of schema responsibilities would be as follows:

- An "owner" user will own the underlying objects (tables, views,
  functions, etc) that are to be secured.  Access to these objects will
  be granted only to "Veil".  The "owner" user will connect only when
  the underlying objects are to be modified.  No-one but a DBA will ever
  connect to this account, and generally, the password for this account
  should be disabled.

- A "Veil" user will own all secured views and access functions (see
  \ref over-views).  Access to these objects will be granted to the
  "Accessor" user.  Like the "owner" user, this user should not be
  directly used except by DBAs performing maintenance.  It will also own
  the Veil API, ie this is the account where Veil itself will be
  installed.  Direct access to Veil API functions should not be granted
  to other users.  If access to a specific function is needed, it should
  be wrapped in a local function to which access may then be granted.
 
- "Accessor" users are the primary point of contact.  These must have no
  direct access to the underlying objects owned by owner.  They will have
  access only to the secured views and access functions.  All
  applications may connect to these user accounts.

\subsection Design Design Your Access Functions

Provide a high-level view of the workings of each access function.  You
will need this in order to figure out what session and shared variables
you will need.  The following is part of the design from the Veil demo
application:
\verbatim
Access Functions are required for:
- Global context only (lookup-data, eg privileges, roles, etc)
- Personal and Global Context (personal data, persons, assignments, etc)
- Project and Global (projects, project_details)
- All 3 (assignments)

Determining privilege in Global Context:

User has priv X, if X is in role_privileges for any role R, that has
been assigned to the user.

Role privileges are essentially static so may be loaded into memory as a
shared variable.  When the user connects, the privileges associated with
their roles may be loaded into a session variable.

Shared initialisation code:
  role_privs ::= shared array of privilege bitmaps indexed by role.
  Populate role_privs with:
    select bitmap_array_setbit(role_privs, role_id, privilege_id)
    from   role_privileges;

Connection initialisation code:
  global_privs ::= session privileges bitmap
  Clear global_privs and then initialise with:
    select bitmap_union(global_privs, role_privs[role_id])
    from   person_roles
    where  person_id = connected_user;

i_have_global_priv(x):
  return bitmap_testbit(global_privs, x);

\endverbatim 

This gives us the basic structure of each function, and identifies what
must be provided by session and system initialisation to support those
functions.  It also allows us to identify the overhead that Veil imposes.

In the case above, there is a connect-time overhead of one extra query
to load the global_privs bitmap.  This is probably a quite acceptable
overhead as typically each user will have relatively few roles.

If the overhead of any of this seems too significant there are
essentially 4 options:
- Simplify the design.
- Defer the overhead until it is absolutely necessary.  This can be done
  with connection functions where we may be able to defer the overhead
  of loading relational context data until the time that we first need
  it.
- Implement a caching solution (check out pgmemcache).  Using an
  in-memory cache will save data set-up queries from having to be
  repeated.  This is pretty complex though and may require you to write
  code in C.
- Suffer the performance hit.

\subsection Implementation Implement the Initialisation Function

Proper initialisation of veil is critical.  There are two ways to manage
this.  The traditional way is to write your own version of \ref
API-control-init, replacing the supplied version.  The newer, better,
alternative is to register your own initialisation functions in the
table veil.veil_init_fns, and have the standard \ref API-control-init,
call them.  If there are multiple initialisation functions, they are
called in order of their priority values as specified in the table
<code>veil.veil_init_fns</code>.

The newer approach has a number of advantages:

- it fully supports the PostgreSQL extension mechanism, allowing
  extensions to be created and dropped;
- it allows different security subsystems to have their own separate
  initialisation routines, allowing more modular code and better
  separation of responsibilities;
- it is way cooler.

Initialisation functions \ref API-control-init are critical elements.
They will be called by automatically by Veil, when the first in-built
Veil function is invoked.  Initialisation functions are responsible for
three distinct tasks:

- Initialisation of session variables
- Initialisation of shared variables
- Re-initialisation of variables during reset

The boolean parameter to veil_init (which is passed to registered
initialisation functions) will be false on initial session
startup, and true when performing a reset (\ref API-control-reset).

Shared variables are created using \ref API-variables-share.  This
returns a boolean result describing whether the variable already
existed.  If so, and we are not performing a reset, the current session
need not initialise it.

Session variables are simply created by using them.  It is worth
creating and initialising all session variables to "fix" their data
types.  This will prevent other functions from misusing them.

If the boolean parameter to an initialisation fuction is true, then we
are performing a memory reset, and all shared variables should be
re-initialised.  A memory reset will be performed whenever underlying,
essentially static, data has been modified.  For example, when new
privileges have been added, we must rebuild all privilege bitmaps to
accommodate the new values.

\subsection Implementation2 Implement the Connection Functions

The connection functions have to authenticate the connecting user, and
then initialise the user's session.  

Authentication should use a secure process in which no plaintext
passwords are ever sent across the wire.  Veil does not provide
authentication services.  For your security needs you should probably
check out pgcrypto.

Initialising a user session is generally a matter of initialising
bitmaps that describe the user's base privileges, and may also involve
setting up bitmap hashes of their relational privileges.  Take a look at
the demo (\ref demo-sec) for a working example of this.

\subsection Implementation3 Implement the Access Functions

Access functions provide the low-level access controls to individual
records.  As such, their performance is critical.  It is generally better
to make the connection functions to more work, and the access functions
less.  Bear in mind that if you perform a query that returns 10,000 rows
from a table, your access function for that view is going to be called
10,000 times.  It must be as fast as possible.

When dealing with relational contexts, it is not always possible to keep
all privileges for every conceivable relationship in memory.  When this
happens, your access function will have to perform a query itself to
load the specific data into memory.  If your application requires this,
you should: 

- Ensure that each such query is as simple and efficient as possible 
- Cache your results in some way

You may be able to trade-off between the overhead of connection
functions and that of access functions.  For instance if you have a
relational security context based upon a tree of relationships, you may
be able to load all but the lowest level branches of the tree at connect
time.  The access function then has only to load the lowest level branch
of data at access time, rather than having to perform a full tree-walk.

Caching can be very effective, particularly for nested loop joins.  If
you are joining A with B, and they both have the same access rules, once
the necessary privilege to access a record in A has been determined and
cached, we will be able to use the cached privileges when checking for
matching records in B (ie we can avoid repeating the fetch).

\subsection Implementation4 Implement the views and instead-of triggers

This is the final stage of implementation.  For every base table you
must create a secured view and a set of instead-of triggers for insert,
update and delete.  Refer to the demo (\ref demo-sec) for details of
this.

\subsection Testing Testing

Be sure to test it all.  Specifically, test to ensure that failed
connections do not provide any privileges, and to ensure that all
privileges assigned to highly privileged users are cleared when a more
lowly privileged user takes over a connection.  Also ensure that
the underlying tables and raw veil functions are not accessible from
user accounts.

\section Automation Automatic code generation 

Note that the bulk of the code in a Veil application is in the
definition of secured views and instead-of triggers, and that this code
is all very similar.  Consider using a code-generation tool to implement
this. 

Next: \ref Demo

*/
/*! \page Demo A Full Example Application: The Veil Demo
\section demo-sec The Veil Demo Application

The Veil demo application serves two purposes:
- it provides a demonstration of Veil-based access controls;
- it provides a working example of how to build a secured system using Veil.

This section covers the following topics:

- \ref demo-install
- \subpage demo-model
- \subpage demo-security
- \subpage demo-explore
- \subpage demo-code
- \subpage demo-uninstall

\subsection demo-install Installing the Veil demo

The veil_demo application, is packaged as an extension just like Veil
itself, and is installed as part of the installation of Veil.  To create
a database containing the veil_demo application:
- create a new database
- connect to that database
- execute <code>create extension veil;</code>
- execute <code>create extension veil_demo;</code>

Next: \ref demo-model

*/
/*! \page demo-model The Demo Database ERD, Tables and Views
\section demo-erd The Demo Database ERD

\image html veil_demo.png "The Veil Demo Database" width=10cm

\section demo-tables Table Descriptions

\subsection demo-privs Privileges

This table describes each privilege.  A privilege is a right to do
something.  Most privileges are concerned with providing access to
data.  For each table, "X" there are 4 data privileges, SELECT_X, UPDATE_X,
INSERT_X and DELETE_X.  There are separate privileges to provide access
to project and person details, and there is a single function privilege,
<code>can_connect</code>.

\subsection demo-roles Roles

A role is a named collection of privileges.  Privileges are assigned to
roles through role_privileges.  Roles exist to reduce the number of
individual privileges that have to be assigned to users, etc.  Instead
of assigning twenty or more privileges, we assign a single role that
contains those privileges.

In this application there is a special role, <code>Personal
Context</code> that contains the set of privileges that apply to all
users in their personal context.  Since all users have the same personal
context privileges, the demo application provides this role to all users
implicitly; there is no need for it to be explicitly assigned.

Assignments of roles in the global context are made through
person_roles, and in the project (relational) context through
assignments.

\subsection demo-role-privs Role_Privileges

Role privileges describe the set of privileges for each role.

\subsection demo-role-roles Role_Roles

This is currently unused in the Veil demo application.  Role roles
provides the means to assign roles to other roles.  This allows new
roles to be created as combinations of existing roles.  The use of this
table is currently left as an exercise for the reader.

\subsection demo-persons Persons

This describes each person.  A person is someone who owns data and who
may connect to the database.  This table should contain authentication
information etc.  In the demo it just maps a name to a person_id.

\subsection demo-projects Projects

A project represents a real-world project, to which many persons may be
assigned to work.

\subsection demo-person-roles Person_Roles

This table describes the which roles have been assigned to users in the
global context.

\subsection demo-assignments Assignments

This describes the roles that have been assigned to a person on a
specific project.  Assignments provide privilege to a user in the
project context.

\subsection demo-detail_types Detail_Types

This is a lookup-table that describes general-purpose attributes that
may be assigned to persons or project.  An example of an attribute for a
person might be birth-date.  For a project it might be inception date.
This allows new attributes to be recorded for persons, projects, etc
without having to add columns to the table.

Each detail_type has a required_privilege field.  This identifies the
privilege that a user must have in order to be able to see attributes of
the specific type.

\subsection demo-person_details Person_Details

These are instances of specific attributes for specific persons.

\subsection demo-project-details Project_Details

These are instances of specific attributes for specific projects.

\section Demo-Views The Demo Application's Helper Views

Getting security right is difficult.  The Veil demo provides a number of
views that help you view the privileges you have in each context.

- my_global_privs shows you the privileges you have in the global
  context
- my_personal_privs shows you the privileges you have in the
  personal context
- my_project_privs shows you the privileges you have for each project
  in the project context
- my_privs shows you all your privileges in all contexts
- my_projects shows you all the projects to which you have been assigned

Using these views, access control mysteries may be more easily tracked
down.

Next: \ref demo-security

*/
/*! \page demo-security The Demo Database Security Model
\section demo-secmodel The Demo Database Security Model

The Veil demo has three security contexts.

- Personal Context applies to personal data that is owned by the
  connected user.  All users have the same privileges in personal
  context, as defined by the role <code>Personal Context</code>.
- Global Context applies equally to every record in a table.  If a user
  has <code>SELECT_X</code> privilege in the global context, they will
  be able to select every record in <code>X</code>, regardless of
  ownership.  Privileges in global context are assigned through
  <code>person_roles</code>.
- Project Context is a relational context and applies to project data.
  If you are assigned a role on a project, you will be given specific
  access to certain project tables.  The roles you have been assigned
  will define your access rights.

The following sections identify which tables may be accessed in which
contexts.

\subsection demo-global-context The Global Context
The global context applies to all tables.  All privilege checking
functions will always look for privileges in the global context.

\subsection demo-personal-context Personal Context
The following tables may be accessed using rights assigned in the
personal context:
- persons
- assignments
- person_details

\subsubsection demo-project-context Project Context
The following tables may be accessed using rights assigned in the
project context:
- projects
- assignments
- project_details

Next: \ref demo-explore

*/
/*! \page demo-explore Exploring the Demo
\section demo-use Exploring the Demo
\subsection demo-connect Accessing the Demo Database
Using your favourite tool connect to your veil_demo database.

You will be able to see all of the demo views, both the secured views and
the helpers.  But you will not initially be able to see any records:
each view will appear to contain no data.  To gain some privileges you
must identify yourself using the \ref demo-code-connect-person function.

There are 6 persons in the demo.  You may connect as any of them and see
different subsets of data.  The persons are

- 1 Deb (the DBA).  Deb has global privileges on everything.  She needs
  them as she is the DBA.
- 2 Pat (the PM).  Pat has the manager role globally, and is the project
  manager of project 102.  Pat can see all but the most confidential
  personal data, and all data about her project.
- 3 Derick (the director).  Derick can see all personal and project
  data.  He is also the project manager for project 101, the secret
  project.
- 4 Will (the worker).  Will has been assigned to both projects.  He has
  minimal privileges and cannot access project confidential data.
- 5 Wilma (the worker).  Willma has been assigned to project 101.  She has
  minimal privileges and cannot access project confidential data.
- 6 Fred (the fired DBA).  Fred has all of the privileges of Deb, except
  for can_connect privilege.  This prevents Fred from being able to do
  anything.

Here is a sample session, showing the different access enjoyed by
different users.

\verbatim
veildemo=> select connect_person(4);
 connect_person 
----------------
 t
(1 row)

veildemo=> select * from persons;
 person_id |    person_name    
-----------+-------------------
         4 | Will (the worker)
(1 row)

veildemo=> select * from person_details;
 person_id | detail_type_id |    value     
-----------+----------------+--------------
         4 |           1003 | 20050105
         4 |           1002 | Employee
         4 |           1004 | 30,000
         4 |           1005 | 19660102
         4 |           1006 | 123456789
         4 |           1007 | Subservience
(6 rows)

veildemo=> select * from project_details;
 project_id | detail_type_id |  value   
------------+----------------+----------
        102 |           1001 | 20050101
        102 |           1002 | Ongoing
(2 rows)

veildemo=> select connect_person(2);
 connect_person 
----------------
 t
(1 row)

veildemo=> select * from person_details;
 person_id | detail_type_id |       value       
-----------+----------------+-------------------
         1 |           1003 | 20050102
         2 |           1003 | 20050103
         3 |           1003 | 20050104
         4 |           1003 | 20050105
         5 |           1003 | 20050106
         6 |           1003 | 20050107
         1 |           1002 | Employee
         2 |           1002 | Employee
         3 |           1002 | Employee
         4 |           1002 | Employee
         5 |           1002 | Employee
         6 |           1002 | Terminated
         2 |           1004 | 50,000
         1 |           1005 | 19610102
         2 |           1005 | 19600102
         3 |           1005 | 19650102
         4 |           1005 | 19660102
         5 |           1005 | 19670102
         2 |           1006 | 123456789
         1 |           1007 | Oracle, C, SQL
         2 |           1007 | Soft peoply-stuff
         3 |           1007 | None at all
         4 |           1007 | Subservience
         5 |           1007 | Subservience
(24 rows)

veildemo=> select * from project_details;
 project_id | detail_type_id |  value   
------------+----------------+----------
        102 |           1001 | 20050101
        102 |           1002 | Ongoing
        102 |           1008 | $100,000
(3 rows)

veildemo=>

\endverbatim

Next: \ref demo-code

*/
/*! \page demo-code The Demo Code
\section demo-codesec The Code
\subsection demo-code-veil-init veil.veil_demo_init(performing_reset bool)

This function is called at the start of each session, and whenever
\ref API-control-reset is called.  The parameter, doing_reset, is
false when called to initialise a session and true when called from
veil_perform_reset().  It is registered with \ref API-control-init
through the <code>veil.veil_demo_init_fns</code> table which is created
as an inherited table of <code>veil.veil_init_fns</code>.  By
registering the initialisation functions using a veil_demo-specific
inherited table, when the veil_demo extension is dropped, so is the
registration data for \ref demo-code-veil-init.

\dontinclude veil_demo.sqs
\skip veil_demo_init(doing
\until init_reqd =

The first task of veil_init() is to declare a set of Veil shared
variables.  This is done by calling \ref API-variables-share.  This function
returns true if the variable already exists, and creates the variable
and returns false, if not.

These variables are defined as shared because they will be identical for
each session.  Making them shared means that only one session has to
deal with the overhead of their initialisation.

\dontinclude veil_demo.sqs
\skip init_reqd =
\until end if;

We then check whether the shared variables must be initialised.  We will
initialise them if they have not already been initialised by another
session, or if we are performing a reset (see \ref API-control-reset).

Each variable is initialised in its own way.  

Ranges are set by a single call to \ref API-basic-init-range.  Ranges are
used to create bitmap and array types of a suitable size.

Int4Arrays are used to record mappings of one integer to another.  In
the demo, they are used to record the mapping of detail_type_id to
required_privilege_id.  We use this variable so that we can look-up the
privilege required to access a given project_detail or person_detail
without having to explicitly fetch from attribute_detail_types.

Int4Arrays are initialised by a call to \ref API-intarray-init, and
are populated by calling \ref API-intarray-set for each value to
be recorded.  Note that rather than using a cursor to loop through each
detail_type record, we use select count().  This requires less code and
has the same effect.

We use a BitmapArray to record the set of privileges for each role.  Its
initialisation and population is handled in much the same way as
described above for Int4Arrays, using the functions \ref
API-bmarray-init and \ref API-bmarray-setbit.

\until language

The final section of code defines and initialises a set of session
variables.  These are defined here to avoid getting undefined variable
errors from any access function that may be called before an
authenticated connection has been established.

Note that this and all Veil related functions are defined with
<code>security definer</code> attributes.  This means that the function
will be executed with the privileges of the function's owner, rather
than those of the invoker.  This is absolutely critical as the invoker
must have no privileges on the base objects, or on the raw Veil
functions themselves.  The only access to objects protected by Veil must
be through user-defined functions and views.

\subsection demo-code-connect-person connect_person(_person_id int4)

This function is used to establish a connection from a specific person.
In a real application this function would be provided with some form of
authentication token for the user.  For the sake of simplicity the demo
allows unauthenticated connection requests.

\skip connect_person(_per
\until language

This function identifies the user, ensures that they have can_connect
privilege.  It initialises the global_context bitmap to contain the
union of all privileges for each role the person is assigned through
person_roles.  It also sets up a bitmap hash containing a bitmap of
privileges for each project to which the person is assigned.

\subsection demo-code-global-priv i_have_global_priv(priv_id int4)

This function is used to determine whether a user has a specified
privilege in the global context.  It tests that the user is connected
using \ref API-basic-int4-get, and then checks whether the
specified privilege is present in the <code>global_context</code>
bitmap.

\skip function i_have_global_priv(priv
\until security definer;

The following example shows this function in use by the secured view,
<code>privileges</code>:

\skip create view privileges
\until grant

The privileges used above are <code>select_privileges</code> (10001),
<code>insert_privileges</code> (10002), <code>update_privileges</code>
(10003), and <code>delete_privileges</code> (10004).

\subsection demo-code-personal-priv i_have_personal_priv(priv_id int4, person_id int4)

This function determines whether a user has a specified privilege to a
specified user's data, in the global or personal contexts.  It performs
the same tests as for \ref demo-code-global-priv.  If the user
does not have access in the global context, and the connected user is
the same user as the owner of the data we are looking at, then we test
whether the specified privilege exists in the <code>role_privs</code>
bitmap array for the <code>Personal Context</code> role.

\dontinclude veil_demo.sqs
\skip function i_have_personal_priv(pr
\until language

Here is an example of this function in use from the persons secured view:

\skip create view persons
\until grant

\subsection demo-code-project-priv i_have_project_priv(priv_id int4, project_id int4)
This function determines whether a user has a specified privilege in the
global or project contexts.  If the user does not have the global
privilege, we check whether they have the privilege defined in the
project_context BitmapHash.

\dontinclude veil_demo.sqs
\skip function i_have_project_priv(pr
\until language

Here is an example of this function in use from the instead-of insert
trigger for the projects secured view:

\skip create rule ii_projects 
\until i_have_project_priv(10018, new.project_id);

\subsection demo-code-proj-pers-priv i_have_proj_or_pers_priv(priv_id int4, project_id int4, person_id int4)
This function checks all privileges.  It starts with the cheapest check
first, and short-circuits as soon as a privilege is found.

\dontinclude veil_demo.sqs
\skip function i_have_proj_or_pers_priv(
\until language

Here is an example of this function in use from the instead-of update
trigger for the assignments secured view:

\skip create rule ii_assignments 
\until i_have_proj_or_pers_priv(10027, old.project_id, old.person_id);

\subsection demo-code-pers-detail-priv i_have_person_detail_priv(detail_id int4, person_id int4)
This function is used to determine which types of person details are
accessible to each user.  This provides distinct access controls to each
attribute that may be recorded for a person.

\dontinclude veil_demo.sqs
\skip function i_have_person_detail_priv(
\until language

The function is shown in use, below, in the instead-of delete trigger
for person_details.  Note that two distinct access functions are being
used here.

\skip create rule id_person_details
\until i_have_person_detail_priv(old.detail_type_id, old.person_id);

Next: \ref demo-uninstall

*/
/*! \page demo-uninstall Removing The Demo Database
\section demo-clean-up Removing The Demo Database
In your veil_demo database execute:
- <code>drop extension veil_demo;</code> 

Next: \ref Management

*/
/*! \page Management Managing Privileges, etc
\section Management-sec Managing Privileges, etc
The management of privileges and their assignments to roles, persons,
etc are the key to securing a veil-based application.  It is therefore
vital that privilege assignment is itself a privileged operation.

The veil demo does not provide an example of how to do this, and this
section does little more than raise the issue.

IT IS VITAL THAT YOU CAREFULLY LIMIT HOW PRIVILEGES ARE MANIPULATED AND
ASSIGNED!

Here are some possible rules of thumb that you may wish to apply:

- give only the most senior and trusted users the ability to assign
  privileges;
- allow only the DBAs to create privileges;
- allow only 1 or 2 security administrators to manage roles;
- allow roles or privileges to be assigned only by users that have both
  the "assign_privileges"/"assign_roles" privileges, and that themselves
  have the privilege or role they are assigning;
- consider having an admin privilege for each table and only allow users
  to assign privileges on X if they have "admin_x" privilege;
- limit the users who have access to the role/privilege management
  functions, and use function-level privileges to enforce this;
- audit/log all assignments of privileges and roles;
- send email to the security administrator whenever role_privileges are
  manipulated and when roles granting high-level privileges are granted.

Next: \ref Esoteria

*/
/*! \page Esoteria Exotic and Esoteric uses of Veil

\section Esoteria-sec Exotic and Esoteric uses of Veil
Actually this is neither exotic nor particularly esoteric.  The title is
simply wishful thinking on the author's part.
\subsection layered-sessions Multi-Layered Connections
So far we have considered access controls based only on the user.  If we
wish to be more paranoid, and perhaps we should, we may also consider
limiting the access rights of each application.

This might mean that reporting applications would have no ability to
update data, that financial applications would have no access to
personnel data, and that personnel apps would have no access to business
data.

This can be done in addition to the user-level checks, so that even if I
have DBA privilege, I can not subvert the personnel reporting tools to
modify financial data.

All access functions would check the service's privileges in addition to
the user's before allowing any operation.

This could be implemented with a connect_service() function that would
be called only once per session and that *must* be called prior to
connecting any users.  Alternatively, the connected service could be
inferred from the account to which the service is connected.

\subsection columns Column-Level Access Controls

Although veil is primarily intended for row-based access controls,
column-based is also possible.  If this is required it may be better to
use a highly normalised data model where columns are converted instead
into attributes, much like the person_details and project_details tables
from the demo application (\ref demo-sec).

If this is not possible then defining access_views that only show
certain columns can be done something like this:

\verbatim
create view wibble(key, col1, col2, col3) as
select key, 
       case when have_col_priv(100001) then col1 else null end,
       case when have_col_priv(100002) then col2 else null end,
       case when have_col_priv(100003) then col3 else null end
where  have_row_priv(1000);
\endverbatim

The instead-of triggers for this are left as an exercise.

Next: \ref install

*/
/*! \page install Installation and Configuration
\section install_sec Installation
\subsection Get Getting Veil
Veil can be downloaded as a gzipped tarball from
http://pgfoundry.org/projects/veil/

The latest development version can also be retrieved from the cvs
repository using the following commands.  When prompted for a password
for anonymous, simply press the Enter key. 

\verbatim
cvs -d :pserver:anonymous@cvs.pgfoundry.org:/cvsroot/veil login
cvs -d :pserver:anonymous@cvs.pgfoundry.org:/cvsroot/veil checkout veil
\endverbatim

\subsection Pre-requisites Pre-requisites
You must have a copy of the Postgresql header files available in order
to build Veil.  For this, you may need to install the postgres developer
packages for your OS, or even build postgres from source.
\subsection build-sub Building Veil
Veil is built using the postgresql extension building infastructure,
PGXS.  In order to be able to build using PGXS, the following command
must return the location of the PGXS makefile:
\verbatim
$ pg_config --pgxs
\endverbatim
You may need to modify your PATH variable to make pg_config usable, or
ensure it is for the correct postgresql version.

To build the postgres extensions, simply run make with no target:
\verbatim
$ make
\endverbatim

To build the veil documentation (the documentation you are now reading)
use make docs.

Note that the build system deliberately avoids using make recursively.
Search the Web for "Recursive Make Considered Harmful" for the reasons
why.  This makes the construction of the build system a little different
from what you may be used to.  This may or may not turn out to be a good
thing.  \ref Feedback "Feedback" is welcomed.

\subsection Install Installing Veil
As the postgres, pgsql, or root user, run <code>make install</code>.
\verbatim
make install
\endverbatim

\section configuration Configuration
To configure Veil, the following lines should be added to your
postgresql.conf:
\code
shared_preload_libraries = '<path to shared library>/veil.so' 

custom_variable_classes = 'veil'

#veil.dbs_in_cluster = 1
#veil.shared_hash_elems = 32
#veil.shmem_context_size = 16384
\endcode

The three configuration options, commented out above, are:
- dbs_in_cluster
  The number of databases, within the database cluster, that
  will use Veil.  Each such database will be allocated 2 chunks of
  shared memory (of shmem_context_size), and a single LWLock.
  It defaults to 1.

- shared_hash_elems
  This describes how large a hash table should be created for veil
  shared variables.  It defaults to 32.  If you have more than about 20
  shared variables you may want to increase this to improve
  performance.  This setting does not limit the number of variables that
  may be defined, it just limits how efficiently they may be accessed.

- shmem_context_size
  This sets an upper limit on the amount of shared memory for a single
  Veil shared memory context (there will be two of these).  It defaults 
  to 16K.  Increase this if you have many shared memory structures.

\subsection Regression Regression Tests
Veil comes with a built-in regression test suite.  Use <code>make
regress</code> or <code>make check</code> (after installing and
configuring Veil) to run this.  You will need superuser access to
Postgres in order to create the regression test database.  The
regression test assumes you will have a postgres superuser account named
the same as your OS account.  If pg_hba.conf disallows "trust"ed access
locally, then you will need to provide a password for this account in
your .pgpass file (see postgres documentation for details).

The regression tests are all contained within the regress directory and
are run by the regress.sh shell script.  Use the -h option to get
fairly detailed help.

\subsection Debugging Debugging
If you encounter problems with Veil, you may want to try building with
debug enabled.  Define the variable VEIL_DEBUG on the make command line
to add extra debug code to the executable:
\verbatim
$ make clean; make VEIL_DEBUG=1 all
\endverbatim

This is a transient feature and not as pervasive as it could be.  If you
need help with debugging please contact the author.

Next: \ref History

*/
/*! \page History History and Compatibility
\section past Changes History
\subsection v1_0 Version 9.1.0 (Stable) (2011-07-22)
This is the first version of Veil to be considered production ready and
completely stable.  It is for use only with PostgreSQL 9.1.  Support for
older versions of PostgreSQL has been removed in this version.

Major changes include:
- revamp of the build system to use PGXS and the new PostgreSQL 9.1
  extensions mechanism.  Veil is now built as an extension.
- modification to the veil_init() mechanism, allowing custom
  initialisation functions to be registered through the table
  veil.veil_init_fns
- removal of the old veil_trial mechanism, which allowed Veil to be
  tried out without fully installing it.  This has removed much
  unnecessary complexity.
- much general code cleanup, including the removal of conditional code
  for older PostgreSQL versions.
- documentation changes, including improved comments for Veil
  functions.

\subsection v9_12 Version 0.9.12 (2010-11-19)
Release for compatibility with PostgreSQL V9.0.  Minor bugfixes and
improvements to the build system.  Also added documentation about Veil's
limitations.

\subsection v9_11 Version 0.9.11 (2010-03-12)
Bugfix release, fixing a serious memory corruption bug that has existed
in all previous versions.  Users are strongly encouraged to avoid using
older versions of Veil.

The version number has been deliberatley bumped past 0.9.10 to emphasize
that the last part of the version is a two digit number.

\subsection v9_9 Version 0.9.9 (2009-07-06)
New release to coincide with PostgreSQL V8.4.

\subsection v9_8 Version 0.9.8 (2008-02-06)
This is the first Beta release.  It incorporates a few bug fixes, a new
serialisation API, improvements to the autoconf setup and makefiles, and
some documentation improvements.  The status of Veil has been raised to
Beta in recognition of its relative stability.

\subsection v9_6 Version 0.9.6 (2008-02-06)
This release has minor changes to support PostgreSQL 8.3.

\subsection v9_5 Version 0.9.5 (2007-07-31)
This is a bugifx release, fixing a memory allocation bug in the use of 
bitmap_refs.  There are also fixes for minor typos, etc.

\subsection v9_4 Version 0.9.4 (2007-02-21)
This is a bugifx release, providing:
 - fix for major bug with recursive handling of spi connect, etc;
 - improvement to session initialisation code to do more up-front work
   in ensure_init();
 - safer initialisation of malloc'd data structures;
 - improved error messages for shared memory exhaustion cases;
 - addition of debug code including canaries in data structures;
 - improvement to autoconf to better support Debian GNU/Linux, and OSX;
 - improvement to autoconf/make for handling paths containing spaces;
 - improvement to regression tests to better support OSX;
 - removal of spurious debug warning messages.

\subsection v9_3 Version 0.9.3 (2006-10-31) 
This version uses the new Postgres API for reserving shared memory for
add-ins.  It also allows the number of Veil-enabled databases for a
cluster to be configured, and refactors much of the shared memory code.
A small fix for the Darwin makefile was also made.

\subsection v9_2 Version 0.9.2 (2006-10-01) 
This version was released to coincide with Postgres 8.2beta1 and first
made use of new Postgres APIs to allow Veil to be a good Postgres
citizen.  

With prior versions of Veil, or prior versions of Postgres, Veil steals
from Postgres the shared memory that it requires.  This can lead to the
exhaustion of Postgres shared memory.

Unfortunately, the Postgres API for shared memory reservation had to
change follwing 8.2.beta1, and this version of Veil is therefore deprecated.

\subsection v9_1 Version 0.9.1 (2006-07-04)
This release fixed a small number of bugs and deficiencies:
- major error in veil_perform_reset that prevented proper use of the two
interdependant shared memory contexts
- minor improvements in the build process to "configure" and friends
- minor documentation improvements

\subsection v9_0 Version 0.9.0  (2005-10-04) 
This was the first public alpha release of Veil.

\section forecast Change Forecast
New versions will be released with each new major version of
PostgreSQL.  Once there are three PostgreSQL versions for which Veil has
been at production status, the change history and support matrix for for
pre-production versions will be removed from this documentation.

\section compatibility Supported versions of Postgres
<TABLE>
  <TR>
    <TD rowspan=2>Veil version</TD>
    <TD colspan=9>Postgres Version</TD>
  </TR>
  <TR>
    <TD>7.4</TD>
    <TD>8.0</TD>
    <TD>8.1</TD>
    <TD>8.2beta1</TD>
    <TD>8.2</TD>
    <TD>8.3</TD>
    <TD>8.4</TD>
    <TD>9.0</TD>
    <TD>9.1</TD>
  </TR>
  <TR>
    <TD>0.9.0 Alpha</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.1 Alpha</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.2 Alpha</TD>
    <TD>-</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>2</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.3 Alpha</TD>
    <TD>-</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>3</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.4 Alpha</TD>
    <TD>-</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>3</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.5 Alpha</TD>
    <TD>-</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>3</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.6 Alpha</TD>
    <TD>-</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>3</TD>
    <TD>3</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.8 Beta</TD>
    <TD>-</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>3</TD>
    <TD>3</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.9 Beta</TD>
    <TD>-</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>3</TD>
    <TD>3</TD>
    <TD>3</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.11 Beta</TD>
    <TD>-</TD>
    <TD>1</TD>
    <TD>1</TD>
    <TD>-</TD>
    <TD>3</TD>
    <TD>3</TD>
    <TD>3</TD>
    <TD>-</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>0.9.12 Beta</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>3</TD>
    <TD>3</TD>
    <TD>3</TD>
    <TD>-</TD>
  </TR>
  <TR>
    <TD>9.1.0 (Stable)</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>-</TD>
    <TD>Yes</TD>
  </TR>
</TABLE>
Notes:

1) These combinations of Veil and Postgres provide no configuration
   options for shared memory.  Veil's shared memory may be exhausted by
   too many requests for large shared objects.  Furthermore, Postgres'
   own shared memory may be easily exhausted by creating too many
   Veil-using databases within a cluster.

2) This version is deprecated

3) These combinations of Veil and Postgres provide full configuration
   options for shared memory usage, and Veil cooperates with Postgres
   for the allocation of such memory meaning that it is not possible to
   use Veil to exhaust Postgres' shared memory.  This is the minimum
   Veil configuration recommended for production use.

\section platforms Supported Platforms
Veil should be buildable on any platform supported by PostgreSQL and
PGXS. 

Next: \ref Feedback

*/
/*! \page Feedback Bugs and Feedback
\section Feedback Bugs and Feedback
For general feedback, to start and follow discussions, etc please join
the veil-general@pgfoundry.org mailing list.

If you wish to report a bug or request a feature, please send mail to
veil-general@pgfoundry.org

If you encounter a reproducible veil bug that causes a database server
crash, a gdb backtrace would be much appreciated.  To generate a
backtrace, you will need to login to the postgres owner account on the
database server.  Then identify the postgres backend process associated
with the database session that is going to crash.  The following command
identifies the backend pid for a database session started by marc:

\verbatim
$ ps auwwx | grep ^postgres.*ma[r]c | awk '{print $2}'
\endverbatim

Now you invoke gdb with the path to the postgres binary and the pid for
the backend, eg:

\verbatim
$ gdb /usr/lib/postgresql/9.1/bin/postgres 5444
\endverbatim

Hit c and Enter to get gdb to allow the session to continue.   Now,
reproduce the crash from your database session.  When the crash occurs,
your gdb session will return to you.  Now type bt and Enter to get a
backtrace. 

If you wish to contact the author offlist, you can find him at his website
http://bloodnok.com/Marc.Munro

Next: \ref Performance

*/
/*! \page Performance Performance
\section perf Performance
Attempts to benchmark veil using pgbench have not been very successful.
It seems that the overhead of veil is small enough that it is
overshadowed by the level of "noise" within pgbench.

Based on this inability to properly benchmark veil, the author is going
to claim that "it performs well".

To put this into perspective, if your access functions do not require
extra fetches to be performed in order to establish your access rights,
you are unlikely to notice or be able to measure any performance hit
from veil.

If anyone can provide good statistical evidence of a performance hit,
the author would be most pleased to hear from you.

Next: \ref Credits

*/
/*! \page Credits Credits
\section Credits
The Entity Relationship Diagram in section \ref demo-erd was produced
automatically from an XML definition of the demo tables, using Autograph
from CF Consulting.  Thanks to Colin Fox for allowing its use.

Thanks to the PostgreSQL core team for providing PostgreSQL.

Thanks to pgfoundry for providing a home for this project.
*/

