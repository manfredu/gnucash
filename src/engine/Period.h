/*
 * FILE:
 * Period.h
 *
 * FUNCTION:
 * Implement accounting periods.
 *
 * CAUTION: this is currently a semi-functional, untested implementation
 * of the design described in src/doc/book.txt
 *
 * HISTORY:
 * created by Linas Vepstas November 2001
 * Copyright (c) 2001 Linas Vepstas <linas@linas.org>
 */


#ifndef XACC_PERIOD_H
#define XACC_PERIOD_H

#include "gnc-book.h"
#include "gnc-engine.h"
#include "Query.h"


/* The gnc_book_calve_period() routine will split the indicated
 *    book into two books, returning a newly created book with 
 *    the older transactions placed in it. 
 *
-- Make an equity transfer so that we can carry forward the balances.
-- hack alert -- should not allow closed books to have unreconciled
   transactions ???
-- It will use the following kvp entries in /book/:

Implemented in book:
/book/start-date        earliest date in this book.
/book/end-date          latest date in this book. must not change ...
/book/close-date        date on which book was closed.
/book/next-book         guid of next book
/book/prev-book         guid of previous book

Mot imlemented (yet):
/book/closing-balance-of-account-guid
/book/name=some-user-supplied-name
/book/notes=user-supplied-descriptive-comments
/book/accounting-period=enum {none, week, month, quarter, trimester, year}

To go into account:
/book/balancing-trans  GUID of equity-balancing transaction.


 */
GNCBook * gnc_book_calve_period (GNCBook *, Timespec);

/* The gnc_book_partition() uses the result of the indicated query
 *    to partition an existing book into two parts.  It returns 
 *    a newly created book, containing a copy of all of the accounts,
 *    and it moves all of the transactions returned by the query to 
 *    the copied accounts in the copied book.   Any query will work
 *    to partition a book; however, its expected that this routine 
 *    will mostly serve as a utility to break up a book into 
 *    accounting periods. 
 *
 *    This routine intentionally does not copy scheduled/recurring 
 *    transactions.
 *
 *    When an account is copied, the copy is issued a new GUID.
 *    The GUID of its sibling is placed in the 'gemini' KVP value
 *    (See kvp_doc.txt for more detail).   Transactions and splits
 *    are moved without reassigning them a new GUID.  Note they
 *    are removed from one book's entity table and placed into the 
 *    other book:  Once moved, they won't be findable in the entity
 *    table of the old book.
 */
GNCBook * gnc_book_partition (GNCBook *, Query *);

/* The gnc_book_insert_trans() routine takes an existing transaction
 *    that is located in one book, and moves it to another book.
 *    It moves all of the splits as well.  In the course of the 
 *    move, the transaction is literally deleted from the first 
 *    book as its placed into the second.  The transaction and
 *    split GUID's are not changed in the move.  This routine 
 *    assumes that twin accounts already exist in both books 
 *    (and can be located with the standard twining proceedure).
 */

void gnc_book_insert_trans (GNCBook *book, Transaction *trans);

#endif /* XACC_PERIOD_H */

