/********************************************************************\
 * gnc-engine-util.h -- GnuCash engine utility functions            *
 * Copyright (C) 1997 Robin D. Clark                                *
 * Copyright (C) 1998-2002 Linas Vepstas <linas@linas.org>          *
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
 *   Author: Rob Clark (rclark@cs.hmc.edu)                          *
 *   Author: Linas Vepstas (linas@linas.org)                        *
\********************************************************************/

#ifndef GNC_ENGINE_UTIL_H
#define GNC_ENGINE_UTIL_H

#include <errno.h>
#include <glib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#include "gnc-commodity.h"
#include "gnc-numeric.h"

/** DEBUGGING MACROS ************************************************/
/* The debuging macros enable the setting of trace messages */

/** If you modify this, modify the loglevel table in the .c file. */
typedef enum
{
  MOD_DUMMY   =  0,
  MOD_ENGINE  =  1,
  MOD_IO      =  2,
  MOD_REGISTER=  3,
  MOD_LEDGER  =  4,
  MOD_HTML    =  5,
  MOD_GUI     =  6,
  MOD_SCRUB   =  7,
  MOD_GTK_REG =  8,
  MOD_GUILE   =  9,
  MOD_BACKEND = 10,
  MOD_QUERY   = 11,
  MOD_PRICE   = 12,
  MOD_EVENT   = 13,
  MOD_TXN     = 14,
  MOD_KVP     = 15,
  MOD_SX      = 16,
  MOD_BOOK    = 17,
  MOD_TEST    = 18,
  MOD_LOT     = 19,
  MOD_ACCOUNT = 20,
  MOD_IMPORT  = 21,
  MOD_BUSINESS= 22,
  MOD_LAST    = 22
} gncModuleType;

typedef enum
{
  GNC_LOG_FATAL   = 0,
  GNC_LOG_ERROR   = 1,
  GNC_LOG_WARNING = 2,
  GNC_LOG_INFO    = 3,
  GNC_LOG_DEBUG   = 4,
  GNC_LOG_DETAIL  = 5,
  GNC_LOG_TRACE   = 6,
} gncLogLevel;


typedef void (*GNCGuiMessage) (const gchar *format, va_list args);
void gnc_set_warning_message (GNCGuiMessage func);
void gnc_set_error_message (GNCGuiMessage func);

gboolean gnc_send_gui_warning (const gchar *format, ...);
gboolean gnc_send_gui_error (const gchar *format, ...);

/* FIXME: these logging functions should proably get replaced by
 * the glib.h g_error(), etc functions. That way, we would have
 * unified logging mechanism, instead of having some messages
 * work one way, and other a different way ... 
 *
 * FIXME: the if test should not be a subroutine call, it should
 * not use that many CPU cycles.  These logging functions are supposed
 * to be lightweight.  Who changed this ??? Why ??? 
 *
 */
gboolean gnc_should_log (gncModuleType module, gncLogLevel log_level);
void gnc_log (gncModuleType module, gncLogLevel log_level,
              const char *prefix, const char *function_name,
              const char *format, ...) G_GNUC_PRINTF(5,6);

#define FATAL(format, args...) {                   \
  if (gnc_should_log (module, GNC_LOG_FATAL))      \
    gnc_log (module, GNC_LOG_FATAL, "Fatal Error", \
             __FUNCTION__, format, ## args);       \
}

#define PERR(format, args...) {                    \
  if (gnc_should_log (module, GNC_LOG_ERROR))      \
    gnc_log (module, GNC_LOG_ERROR, "Error",       \
             __FUNCTION__, format, ##args);        \
}

#define PWARN(format, args...) {                   \
  if (gnc_should_log (module, GNC_LOG_WARNING))    \
    gnc_log (module, GNC_LOG_WARNING, "Warning",   \
             __FUNCTION__, format, ## args);       \
}

#define PWARN_GUI(format, args...) {               \
  if (!gnc_send_gui_error(format, ## args))        \
    PWARN(format, ## args);                        \
}

#define PINFO(format, args...) {                   \
  if (gnc_should_log (module, GNC_LOG_INFO))       \
    gnc_log (module, GNC_LOG_INFO, "Info",         \
             __FUNCTION__, format, ## args);       \
}

#define DEBUG(format, args...) {                   \
  if (gnc_should_log (module, GNC_LOG_DEBUG))      \
    gnc_log (module, GNC_LOG_DEBUG, "Debug",       \
             __FUNCTION__, format, ## args);       \
}

#define ENTER(format, args...) {                   \
  if (gnc_should_log (module, GNC_LOG_DEBUG))      \
    gnc_log (module, GNC_LOG_DEBUG, "Enter",       \
             __FUNCTION__, format, ## args);       \
}

#define LEAVE(format, args...) {                   \
  if (gnc_should_log (module, GNC_LOG_DEBUG))      \
    gnc_log (module, GNC_LOG_DEBUG, "Leave",       \
            __FUNCTION__, format, ## args);        \
}

#define DETAIL(format, args...) {                  \
  if (gnc_should_log (module, GNC_LOG_DETAIL))     \
    gnc_log (module, GNC_LOG_DETAIL, "Detail",     \
             __FUNCTION__, format, ## args);       \
}


#define DEBUGCMD(x) { if (gnc_should_log (module, GNC_LOG_DEBUG)) { x; }}

#define ERROR()     fprintf(stderr,"%s: Line %d, error = %s\n", \
			    __FILE__, __LINE__, strerror(errno));

#define TRACE(format, args...) {                   \
  if (gnc_should_log (module, GNC_LOG_TRACE))      \
    gnc_log (module, GNC_LOG_TRACE, "Trace",       \
             __FUNCTION__, format, ## args);       \
}

void gnc_start_clock (int clockno, gncModuleType module, gncLogLevel log_level,
                      const char *function_name, const char *format, ...);

void gnc_report_clock (int clockno,
                       gncModuleType module,
                       gncLogLevel log_level,
                       const char *function_name,
                       const char *format, ...);

void gnc_report_clock_total (int clockno,
                             gncModuleType module,
                             gncLogLevel log_level,
                             const char *function_name,
                             const char *format, ...);

#define START_CLOCK(clockno,format, args...) {     \
  if (gnc_should_log (module, GNC_LOG_INFO))       \
    gnc_start_clock (clockno, module, GNC_LOG_INFO,\
             __FUNCTION__, format, ## args);       \
}

#define REPORT_CLOCK(clockno,format, args...) {    \
  if (gnc_should_log (module, GNC_LOG_INFO))       \
    gnc_report_clock (clockno, module, GNC_LOG_INFO,\
             __FUNCTION__, format, ## args);       \
}

#define REPORT_CLOCK_TOTAL(clockno,format, args...) {       \
  if (gnc_should_log (module, GNC_LOG_INFO))                \
    gnc_report_clock_total (clockno, module, GNC_LOG_INFO,  \
             __FUNCTION__, format, ## args);                \
}

/* Set the logging level of the given module. */
void gnc_set_log_level(gncModuleType module, gncLogLevel level);

/* Set the logging level for all modules. */
void gnc_set_log_level_global(gncLogLevel level);

/* Pipe log output to pipe or file */
void gnc_set_logfile (FILE *outfile);

/** Macros *****************************************************/
#define EPS  (1.0e-6)
#define DEQEPS(x,y,eps) (((((x)+(eps))>(y)) ? 1 : 0) && ((((x)-(eps))<(y)) ? 1 : 0))
#define DEQ(x,y) DEQEPS(x,y,EPS)

#define SAFE_STRCMP_REAL(fcn,da,db) {	\
  if ((da) && (db)) {			\
    int retval = fcn ((da), (db));	\
    /* if strings differ, return */	\
    if (retval) return retval;		\
  } else 				\
  if ((!(da)) && (db)) {		\
    return -1;				\
  } else 				\
  if ((da) && (!(db))) {		\
    return +1;				\
  }					\
}

#define SAFE_STRCMP(da,db) SAFE_STRCMP_REAL(strcmp,(da),(db))
#define SAFE_STRCASECMP(da,db) SAFE_STRCMP_REAL(strcasecmp,(da),(db))

/* Define the long long int conversion for scanf */
#if HAVE_SCANF_LLD
# define GNC_SCANF_LLD "%lld"
#else
# define GNC_SCANF_LLD "%qd"
#endif


/** Prototypes *************************************************/

/* The safe_strcmp compares strings a and b the same way that strcmp()
 * does, except that either may be null.  This routine assumes that
 * a non-null string is always greater than a null string.
 */
int safe_strcmp (const char * da, const char * db);
int safe_strcasecmp (const char * da, const char * db);

/* The null_strcmp compares strings a and b the same way that strcmp()
 * does, except that either may be null.  This routine assumes that
 * a null string is equal to the empty string.
 */
int null_strcmp (const char * da, const char * db);

/* Search for str2 in first nchar chars of str1, ignore case. Return
 * pointer to first match, or null. These are just like that strnstr
 * and the strstr functions, except that they ignore the case. */
extern char *strncasestr(const char *str1, const char *str2, size_t len);
extern char *strcasestr(const char *str1, const char *str2);

/* The ultostr() subroutine is the inverse of strtoul(). It accepts a
 * number and prints it in the indicated base.  The returned string
 * should be g_freed when done.  */
char * ultostr (unsigned long val, int base);

/* Returns true if string s is a number, possibly surrounded by
 * whitespace. */
gboolean gnc_strisnum(const char *s);

/* Define a gnucash stpcpy */
char * gnc_stpcpy (char *dest, const char *src);

#ifndef HAVE_STPCPY
#define stpcpy gnc_stpcpy
#endif

/***********************************************************************\

  g_hash_table_key_value_pairs(hash): Returns a GSList* of all the
  keys and values in a given hash table.  Data elements of lists are
  actual hash elements, so be careful, and deallocation of the
  GHashTableKVPairs in the result list are the caller's
  responsibility.  A typical sequence might look like this:

    GSList *kvps = g_hash_table_key_value_pairs(hash);  
    ... use kvps->data->key and kvps->data->val, etc. here ...
    g_slist_foreach(kvps, g_hash_table_kv_pair_free_gfunc, NULL);
    g_slist_free(kvps);

*/

typedef struct {
  gpointer key;
  gpointer value;
} GHashTableKVPair;

GSList *g_hash_table_key_value_pairs(GHashTable *table);
void g_hash_table_kv_pair_free_gfunc(gpointer data, gpointer user_data);

/***********************************************************************/

#endif
