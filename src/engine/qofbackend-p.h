/********************************************************************\
 * qofbackend-p.h -- private api for data storage backend           *
 *                                                                  *
 * Copyright (c) 2000, 2001 Linas Vepstas <linas@linas.org>         *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

/* 
 * FILE:
 * qofbackend-p.h
 *
 * FUNCTION:
 * Pseudo-object defining how the engine can interact with different
 * back-ends (which may be SQL databases, or network interfaces to 
 * remote GnuCash servers.  In theory, file-io should be a type of 
 * backend).
 * 
 * The callbacks will be called at the appropriate times during 
 * a book session to allow the backend to store the data as needed.
 *
 */

#ifndef QOF_BACKEND_P_H
#define QOF_BACKEND_P_H

#include "config.h"

#include "qofbackend.h"
#include "qofbook.h"
#include "qofquery.h"
#include "qofsession.h"

/*
 * The session_begin() routine gives the backend a second initialization
 *    opportunity.  It is suggested that the backend check that 
 *    the URL is syntactically correct, and that it is actually
 *    reachable.  This is probably(?) a good time to initialize
 *    the actual network connection.
 *
 *    The 'ignore_lock' argument indicates whether the single-user
 *    lock on the backend should be cleared.  The typical GUI sequence
 *    leading to this is: (1) GUI attempts to open the backend
 *    by calling this routine with FALSE==ignore_lock.  (2) If backend  
 *    error'ed BACKEND_LOCK, then GUI asks user what to do. (3) if user 
 *    answers 'break & enter' then this routine is called again with
 *    TRUE==ignore_lock.
 *
 *    The 'create_if_nonexistent' argument indicates whether this
 *    routine should create a new 'database', if it doesn't already
 *    exist. For example, for a file-backend, this would create the
 *    file, if it didn't already exist.  For an SQL backend, this
 *    would create the database (the schema) if it didn't already 
 *    exist.  This flag is used to implement the 'SaveAs' GUI, where
 *    the user requests to save data to a new backend.
 *
 * The load() routine should return at least an account tree,
 *    all currencies, pricedb, and any other data that needs to be
 *    loaded at start time.  It does not have to return any
 *    transactions whatsoever, as these are obtained at a later stage
 *    when a user opens a register, resulting in a query being sent to
 *    the backend.
 *
 *    (Its OK to send over transactions at this point, but one should 
 *    be careful of the network load; also, its possible that whatever 
 *    is sent is not what the user wanted anyway, which is why its 
 *    better to wait for the query).
 *
 * The begin() routine is called when the engine is about to
 *    make a change to a data structure.  It can provide an advisory
 *    lock on data.
 *
 * The commit() routine commits the changes from the engine to the
 *    backend data storage.
 *
 * The rollback() routine is used to revert changes in the engine
 *    and unlock the backend.  For transactions it is invoked in one
 *    of two different ways.  In one case, the user may hit 'undo' in
 *    the GUI, resulting in xaccTransRollback() being called, which in
 *    turn calls this routine.  In this manner, xaccTransRollback()
 *    implements a single-level undo convenience routine for the GUI.
 *    The other way in which this routine gets invoked involves
 *    conflicting edits by two users to the same transaction.  The
 *    second user to make an edit will typically fail in
 *    trans_commit_edit(), with trans_commit_edit() returning an error
 *    code.  This causes xaccTransCommitEdit() to call
 *    xaccTransRollback() which in turn calls this routine.  Thus,
 *    this routine gives the backend a chance to clean up failed
 *    commits.
 *
 *    If the second user tries to modify a transaction that
 *    the first user deleted, then the backend should set the error
 *    to ERR_BACKEND_MOD_DESTROY from this routine, so that the 
 *    engine can properly clean up.
 *
 * The compile_query() method compiles a Gnucash query object into
 *    a backend-specific data structure and returns the compiled
 *    query.  For an SQL backend, the contents of the query object
 *    need to be turned into a corresponding SQL query statement, and
 *    sent to the database for evaluation.
 *
 * The free_query() method frees the data structure returned from 
 *    compile_query()
 *
 * The run_query() callback takes a compiled query (generated by
 *    compile_query) and runs the query in across the backend,
 *    inserting the responses into the engine.  The database will
 *    return a set of splits and transactions, and this callback needs
 *    to poke these into the account-group hierarchy held by the query
 *    object.
 *
 *    For a network-communications backend, essentially the same is 
 *    done, except that this routine would convert the query to wire 
 *    protocol, get an answer from the remote server, and push that
 *    into the account-group object.
 *
 *    Note a peculiar design decision we've used here. The query
 *    callback has returned a list of splits; these could be returned
 *    directly to the caller. They are not.  By poking them into the
 *    existing account hierarchy, we are essentially building a local
 *    cache of the split data.  This will allow the GnuCash client to 
 *    continue functioning even when disconnected from the server:
 *    this is because it will have its local cache of data to work from.
 *
 * The sync() routine synchronizes the engine contents to the backend.
 *    This is done by using version numbers (hack alert -- the engine
 *    does not currently contain version numbers).
 *    If the engine contents are newer than what's in the backend, the 
 *    data is stored to the backend.  If the engine contents are older,
 *    then the engine contents are updated.  
 *
 *    Note that this sync operation is only meant to apply to the 
 *    current contents of the engine.  This routine is not intended
 *    to be used to fetch account/transaction data from the backend.
 *    (It might pull new splits from the backend, if this is what is
 *    needed to update an existing transaction.  It might pull new 
 *    currencies (??))
 *
 * The counter() routine increments the named counter and returns the
 *    post-incremented value.  Returns -1 if there is a problem.
 *
 * The events_pending() routines should return true if there are
 *    external events which need to be processed to bring the
 *    engine up to date with the backend.
 *
 * The process_events() routine should process any events indicated
 *    by the events_pending() routine. It should return TRUE if
 *    the engine was changed while engine events were suspended.
 *
 * The last_err member indicates the last error that occurred.
 *    It should probably be implemented as an array (actually,
 *    a stack) of all the errors that have occurred.
 *
 * For support of book partitioning, use special "Book"  begin_edit()
 *    and commit_edit() QOF_ID types.
 *
 *    Call the book begin() at the begining of a book partitioning.  A
 *    'partitioning' is the splitting off of a chunk of the current
 *    book into a second book by means of a query.  Every transaction
 *    in that query is to be moved ('transfered') to the second book
 *    from the existing book.  The argument of this routine is a
 *    pointer to the second book, where the results of the query
 *    should go.
 *
 *    Cann the book commit() to complete the book partitioning.
 *
 *    After the begin(), there will be a call to run_query(), followed
 *    probably by a string of account and transaction calls, and
 *    completed by commit().  It should be explicitly understood that
 *    the results of that run_query() precisely constitute the set of
 *    transactions that are to be moved between the initial and the
 *    new book. This specification can be used by a clever backend to
 *    avoid excess data movement between the server and the gnucash
 *    client, as explained below.
 *
 *    There are several possible ways in which a backend may choose to
 *    implement the book splitting process.  A 'file-type' backend may
 *    choose to ignore this call, and the subsequent query, and simply
 *    write out the new book to a file when the commit() call is made.
 *    By that point, the engine will have performed all of the
 *    nitty-gritty of moving transactions from one book to the other.
 * 
 *    A 'database-type' backend has several interesting choices.  One
 *    simple choice is to simply perform the run_query() as it
 *    normally would, and likewise treat the account and transaction
 *    edits as usual.  In this scenario, the commit() is more or less
 *    a no-op.  This implementation has a drawback, however: the
 *    run_query() may cause the transfer of a *huge* amount of data
 *    between the backend and the engine.  For a large dataset, this
 *    is quite undesirable.  In addition, there are risks associated
 *    with the loss of network connectivity during the transfer; thus
 *    a partition might terminate half-finished, in some indeterminate
 *    state, due to network errors.  That might be difficult to
 *    recover from: the engine does not take any special transactional
 *    safety measures during the transfer.
 *
 *    Thus, for a large database, an alternate implementation 
 *    might be to use the run_query() call as an opportunity to 
 *    transfer transactions between the two books in the database,
 *    and not actually return any new data to the engine.  In
 *    this scenario, the engine will attempt to transfer those 
 *    transactions that it does know about.  It does not, however,
 *    need to know about all the other transactions that also would  
 *    be transfered over.  In this way, a backend could perform
 *    a mass transfer of transactions between books without having
 *    to actually move much (or any) data to the engine.
 *
 */

struct _QofBackend
{
  void (*session_begin) (QofBackend *be,
                         QofSession *session,
                         const char *book_id, 
                         gboolean ignore_lock,
                         gboolean create_if_nonexistent);
  void (*session_end) (QofBackend *);
  void (*destroy_backend) (QofBackend *);

  void (*load) (QofBackend *, QofBook *);

  void (*begin) (QofBackend *, QofIdTypeConst, gpointer);
  void (*commit) (QofBackend *, QofIdTypeConst, gpointer);
  void (*rollback) (QofBackend *, QofIdTypeConst, gpointer);

  gpointer (*compile_query) (QofBackend *, QofQuery *);
  void (*free_query) (QofBackend *, gpointer);
  void (*run_query) (QofBackend *, gpointer);

  void (*sync) (QofBackend *, QofBook *);

  gint64 (*counter) (QofBackend *, const char *counter_name);

  gboolean (*events_pending) (QofBackend *be);
  gboolean (*process_events) (QofBackend *be);

  QofBePercentageFunc percentage;

  QofBackendError last_err;
  char * error_msg;

  /* XXX price_lookup should be removed during the redesign
   * of the SQL backend... prices can now be queried using
   * the generic query mechanism.
   *
   * Note the correct signature for this call is 
   * void (*price_lookup) (QofBackend *, GNCPriceLookup *);
   * we use gpointer to avoid an unwanted include file dependency. 
   */
  void (*price_lookup) (QofBackend *, gpointer);

  /* XXX Export should really _NOT_ be here, but is left here for now.
   * I'm not sure where this should be going to. It should be
   * removed ASAP. 
   */
  void (*export) (QofBackend *, QofBook *);
};

/*
 * The qof_backend_set_error() routine pushes an error code onto the error
 *   stack. (FIXME: the stack is 1 deep in current implementation).
 *
 * The qof_backend_get_error() routine pops an error code off the error
 *   stack.
 *
 * The qof_backend_set_message() assigns a string to the backend error
 *   message.
 *
 * The qof_backend_get_message() pops the error message string from
 *   the Backend.  This string should be freed with g_free().
 */

void qof_backend_set_error (QofBackend *be, QofBackendError err);
QofBackendError qof_backend_get_error (QofBackend *be);
void qof_backend_set_message(QofBackend *be, const char *format, ...);
char * qof_backend_get_message(QofBackend *be);

void qof_backend_init(QofBackend *be);

#endif /* QOF_BACKEND_P_H */
