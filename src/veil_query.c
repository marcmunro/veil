/**
 * @file   veil_query.c
 * \code
 *     Author:       Marc Munro
 *     Copyright (c) 2005 - 2011 Marc Munro
 *     License:      BSD
 * 
 * \endcode
 * @brief  
 * Functions to simplify SPI-based queries.  These are way more
 * sophisticated than veil really needs but are nice and generic.
 * 
 */


#include <stdio.h>
#include "postgres.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "veil_version.h"
#include "access/xact.h"
#include "veil_funcs.h"

/**
 * A Fetch_fn is a function that processes records, one at a time,
 * returned from a query.
 */
typedef bool (Fetch_fn)(HeapTuple, TupleDesc, void *);



/**
 * The number of records to fetch in one go from the query executor.
 */
#define FETCH_SIZE    20


/**
 * If already connected in this session, push the current connection,
 * and get a new one.
 * We are already connected, if:
 * - are within a query
 * - and the current transaction id matches the saved transaction id
 */
int
vl_spi_connect(bool *p_pushed)
{
	int result = SPI_connect();
	if (result == SPI_ERROR_CONNECT) {
		SPI_push();
		*p_pushed = TRUE;
		return SPI_connect();
	}
	*p_pushed = FALSE;
	return result;
}

/**
 * Reciprocal function for vl_spi_connect()
 */
int
vl_spi_finish(bool pushed)
{
	int spi_result = SPI_finish();
	if (pushed) {
		SPI_pop();
	}
	return spi_result;
}

/** 
 * Prepare a query for query().  This creates and executes a plan.  The
 * caller must have established SPI_connect.  It is assumed that no
 * parameters to the query will be null.
 * \param qry The text of the SQL query to be performed.
 * \param nargs The number of input parameters ($1, $2, etc) to the query
 * \param argtypes Pointer to an array containing the OIDs of the data
 * \param args Actual parameters
 * types of the parameters 
 * \param read_only Whether the query should be read-only or not
 * \param saved_plan Adress of void pointer into which the query plan
 * will be saved.  Passing the same void pointer on a subsequent call
 * will cause the saved query plan to be re-used.
 */
static void
prepare_query(const char *qry,
			  int nargs,
			  Oid *argtypes,
			  Datum *args,
			  bool read_only,
			  void **saved_plan)
{
    void   *plan;
    int     exec_result;
	
    if (saved_plan && *saved_plan) {
		/* A previously prepared plan is available, so use it */
		plan = *saved_plan;
    }
    else {
		if (!(plan = SPI_prepare(qry, nargs, argtypes))) {
			ereport(ERROR,
					(errcode(ERRCODE_INTERNAL_ERROR),
					 errmsg("prepare_query fails"),
					 errdetail("SPI_prepare('%s') returns NULL "
							   "(SPI_result = %d)", 
							   qry, SPI_result)));
		}

		if (saved_plan) {
			/* We have somewhere to put the saved plan, so save  it. */
			*saved_plan = SPI_saveplan(plan);
		}
    }
	
	exec_result = SPI_execute_plan(plan, args, NULL, read_only, 0);
	if (exec_result < 0) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("prepare_query fails"),
				 errdetail("SPI_execute_plan('%s') returns error %d",
						   qry, exec_result)));
    }
}

/** 
 * Prepare and execute a query.  Query execution consists of a call to
 * process_row for each returned record.  Process_row can return a
 * single value to the caller of this function through the fn_param
 * parameter.  It is the caller's responsibility to establish an SPI
 * connection with SPI_connect.  It is assumed that no parameters to
 * the query, and no results will be null.
 * \param qry The text of the SQL query to be performed.
 * \param nargs The number of input parameters ($1, $2, etc) to the query
 * \param argtypes Pointer to an array containing the OIDs of the data
 * \param args Actual parameters
 * types of the parameters 
 * \param read_only Whether the query should be read-only or not
 * \param saved_plan Adress of void pointer into which the query plan
 * will be saved.  Passing the same void pointer on a subsequent call
 * will cause the saved query plan to be re-used.
 * \param process_row  The ::Fetch_fn function to be called for each
 * fetched row to process it.  If this is null, we simply count the row,
 * doing no processing on the tuples returned.
 * \param fn_param  An optional parameter to the process_row function.
 * This may be used to return a value to the caller.
 * \return The total number of records fetched and processed by
 * process_row.
 */
static int
query(const char *qry,
      int nargs,
      Oid *argtypes,
      Datum *args,
	  bool  read_only,
      void **saved_plan,
      Fetch_fn process_row,
      void *fn_param)
{
    int  row;
	int  fetched;
	int  processed = 0;
	bool cntinue;
	SPITupleTable *tuptab;

    prepare_query(qry, nargs, argtypes, args, read_only, saved_plan);
	fetched = SPI_processed;
	tuptab = SPI_tuptable;
	for(row = 0; row < fetched; row++) {
		processed++;
		/* Process a row using the processor function */
		cntinue = process_row(tuptab->vals[row], 
							  tuptab->tupdesc,
							  fn_param);
		if (!cntinue) {
			break;
		}
	}
    return processed;
}


/** 
 * ::Fetch_fn function for processing a single row of a single integer for 
 * ::query.
 * \param tuple The row to be processed
 * \param tupdesc Descriptor for the types of the fields in the tuple.
 * \param p_result Pointer to an int4 variable into which the value
 * returned from the query will be placed.
 * \return false.  This causes ::query to terminate after processing a
 * single row.
 */
static bool
fetch_one_bool(HeapTuple tuple, TupleDesc tupdesc, void *p_result)
{
	static bool ignore_this = false;
    bool col = DatumGetBool(SPI_getbinval(tuple, tupdesc, 1, &ignore_this));
    *((bool *) p_result) = col;
	
    return false;
}

/** 
 * ::Fetch_fn function for processing a single row of a single integer for 
 * ::query.
 * \param tuple The row to be processed
 * \param tupdesc Descriptor for the types of the fields in the tuple.
 * \param p_result Pointer to an int4 variable into which the value
 * returned from the query will be placed.
 * \return false.  This causes ::query to terminate after processing a
 * single row.
 */
static bool
fetch_one_str(HeapTuple tuple, TupleDesc tupdesc, void *p_result)
{
    char *col = SPI_getvalue(tuple, tupdesc, 1);
	char **p_str = (char **) p_result;
	*p_str = col;
	
    return false;
}

/** 
 * Executes a query that returns a single bool value.
 * 
 * @param qry The text of the query to be performed.
 * @param result Variable into which the result of the query will be placed.
 * 
 * @return true if the query returned a record, false otherwise.
 */
bool
vl_bool_from_query(const char *qry,
				   bool *result)
{
	int     rows;
    Oid     argtypes[0];
    Datum   args[0];
	rows = query(qry, 0, argtypes, args, false, NULL, 
				 fetch_one_bool, (void *)result);
	return (rows > 0);
}

/** 
 * Executes a query by oid, that returns a single string value.
 * 
 * @param qry The text of the query to be performed.
 * @param param The oid of the row to be fetched.
 * @param result Variable into which the result of the query will be placed.
 * 
 * @return true if the query returned a record, false otherwise.
 */
static bool
str_from_oid_query(const char *qry,
				   const Oid param,
				   char *result)
{
	int     rows;
    Oid     argtypes[1] = {OIDOID};
    Datum   args[1];
	
	args[0] = ObjectIdGetDatum(param);
	rows = query(qry, 1, argtypes, args, false, NULL, 
				 fetch_one_str, (void *)result);
	return (rows > 0);
}

/** 
 * Determine whether the given oid represents an existing database or not.
 * 
 * @param db_id Oid of the database in which we are interested.
 * 
 * @result True if the database exists.
 */
extern bool
vl_db_exists(Oid db_id)
{
	char dbname[NAMEDATALEN + 1];

	return str_from_oid_query("select datname from pg_database where oid = $1",
							  db_id, dbname);
}


/** 
 * ::Fetch_fn function for executing registered veil_init() functions for
 * ::query.
 * \param tuple The row to be processed
 * \param tupdesc Descriptor for the types of the fields in the tuple.
 * \param p_param Pointer to a boolean value which is the value of the
 * argument to the init function being called.
 * \return true.  This allows ::query to process further rows.
 */
static bool
exec_init_fn(HeapTuple tuple, TupleDesc tupdesc, void *p_param)
{
    char *col = SPI_getvalue(tuple, tupdesc, 1);
	char *qry = palloc(strlen(col) + 15);
	bool pushed;
	bool result;
	bool found;
	int ok;

	(void) sprintf(qry, "select %s(%s)", col, 
				   *((bool *) p_param)? "true": "false");

	ok = vl_spi_connect(&pushed);
	if (ok != SPI_OK_CONNECT) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to execute exec_init_fn() (1)"),
				 errdetail("SPI_connect() failed, returning %d.", ok)));
	}

	found = vl_bool_from_query(qry, &result);


	ok = vl_spi_finish(pushed);
	if (ok != SPI_OK_FINISH) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to execute exec_init_fn() (2)"),
				 errdetail("SPI_finish() failed, returning %d.", ok)));
	}

    return true;
}


/** 
 * Identify any registered init_functions and execute them.
 * 
 * @param param The boolean parameter to be passed to each init_function.
 * 
 * @result The number of init_functions executed.
 */
int
vl_call_init_fns(bool param)
{
    Oid     argtypes[0];
    Datum   args[0];
	char   *qry = "select fn_name from veil.veil_init_fns order by priority";
	bool    pushed;
	int     rows;
	int     ok;

	ok = vl_spi_connect(&pushed);
	if (ok != SPI_OK_CONNECT) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to execute vl_call_init_fns() (1)"),
				 errdetail("SPI_connect() failed, returning %d.", ok)));
	}

	rows = query(qry, 0, argtypes, args, false, NULL, 
				 exec_init_fn, (void *) &param);

	ok = vl_spi_finish(pushed);
	if (ok != SPI_OK_FINISH) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("failed to execute vl_call_init_fns() (2)"),
				 errdetail("SPI_finish() failed, returning %d.", ok)));
	}
    return rows;
}
