/********************************************************************\
 * gnc-order-xml-v2.c -- order xml i/o implementation         *
 *                                                                  *
 * Copyright (C) 2002 Derek Atkins <warlord@MIT.EDU>                *
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

#include "config.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "gnc-xml-helper.h"

#include "sixtp.h"
#include "sixtp-utils.h"
#include "sixtp-parsers.h"
#include "sixtp-utils.h"
#include "sixtp-dom-parsers.h"
#include "sixtp-dom-generators.h"

#include "gnc-xml.h"
#include "io-gncxml-gen.h"
#include "io-gncxml-v2.h"

#include "gncOrderP.h"
#include "gnc-order-xml-v2.h"
#include "gnc-owner-xml-v2.h"
#include "gnc-engine-util.h"

#include "gncObject.h"

#define _GNC_MOD_NAME	GNC_ORDER_MODULE_NAME

static short module = MOD_IO;

const gchar *order_version_string = "2.0.0";

/* ids */
#define gnc_order_string "gnc:GncOrder"
#define order_guid_string "order:guid"
#define order_id_string "order:id"
#define order_owner_string "order:owner"
#define order_opened_string "order:opened"
#define order_closed_string "order:closed"
#define order_notes_string "order:notes"
#define order_reference_string "order:reference"
#define order_active_string "order:active"

static void
maybe_add_string (xmlNodePtr ptr, const char *tag, const char *str)
{
  if (str && strlen(str) > 0)
    xmlAddChild (ptr, text_to_dom_tree (tag, str));
}

static xmlNodePtr
order_dom_tree_create (GncOrder *order)
{
    xmlNodePtr ret;
    Timespec ts;

    ret = xmlNewNode(NULL, gnc_order_string);
    xmlSetProp(ret, "version", order_version_string);

    xmlAddChild(ret, guid_to_dom_tree(order_guid_string,
				      gncOrderGetGUID (order)));

    xmlAddChild(ret, text_to_dom_tree(order_id_string,
                                      gncOrderGetID (order)));

    xmlAddChild(ret, gnc_owner_to_dom_tree (order_owner_string,
					    gncOrderGetOwner (order)));

    ts = gncOrderGetDateOpened (order);
    xmlAddChild(ret, timespec_to_dom_tree (order_opened_string, &ts));

    ts = gncOrderGetDateClosed (order);
    if (ts.tv_sec || ts.tv_nsec)
      xmlAddChild(ret, timespec_to_dom_tree (order_closed_string, &ts));
    
    maybe_add_string (ret, order_notes_string, gncOrderGetNotes (order));
    maybe_add_string (ret, order_reference_string, gncOrderGetReference (order));

    xmlAddChild(ret, int_to_dom_tree(order_active_string,
				     gncOrderGetActive (order)));

    return ret;
}

/***********************************************************************/

struct order_pdata
{
  GncOrder *order;
  GNCBook *book;
};

static gboolean
set_string(xmlNodePtr node, GncOrder* order,
           void (*func)(GncOrder *order, const char *txt))
{
  char* txt = dom_tree_to_text(node);
  g_return_val_if_fail(txt, FALSE);
    
  func(order, txt);
  
  g_free(txt);
  return TRUE;
}

static gboolean
set_timespec(xmlNodePtr node, GncOrder* order,
           void (*func)(GncOrder *order, Timespec ts))
{
  Timespec* ts = dom_tree_to_timespec(node);
  g_return_val_if_fail(ts, FALSE);
    
  func(order, *ts);

  g_free(ts);
  return TRUE;
}

static gboolean
order_guid_handler (xmlNodePtr node, gpointer order_pdata)
{
    struct order_pdata *pdata = order_pdata;
    GUID *guid;
    GncOrder *order;

    guid = dom_tree_to_guid(node);
    g_return_val_if_fail (guid, FALSE);
    order = gncOrderLookup (pdata->book, guid);
    if (order) {
      gncOrderDestroy (pdata->order);
      pdata->order = order;
      gncOrderBeginEdit (order);
    } else {
      gncOrderSetGUID(pdata->order, guid);
    }

    g_free(guid);
    
    return TRUE;
}

static gboolean
order_id_handler (xmlNodePtr node, gpointer order_pdata)
{
    struct order_pdata *pdata = order_pdata;

    return set_string(node, pdata->order, gncOrderSetID);
}

static gboolean
order_owner_handler (xmlNodePtr node, gpointer order_pdata)
{
  struct order_pdata *pdata = order_pdata;
  GncOwner owner;
  gboolean ret;

  ret = gnc_dom_tree_to_owner (node, &owner, pdata->book);
  if (ret)
    gncOrderSetOwner (pdata->order, &owner);

  return ret;
}

static gboolean
order_opened_handler (xmlNodePtr node, gpointer order_pdata)
{
    struct order_pdata *pdata = order_pdata;

    return set_timespec (node, pdata->order, gncOrderSetDateOpened);
}

static gboolean
order_closed_handler (xmlNodePtr node, gpointer order_pdata)
{
    struct order_pdata *pdata = order_pdata;

    return set_timespec (node, pdata->order, gncOrderSetDateClosed);
}

static gboolean
order_notes_handler (xmlNodePtr node, gpointer order_pdata)
{
    struct order_pdata *pdata = order_pdata;

    return set_string(node, pdata->order, gncOrderSetNotes);
}

static gboolean
order_reference_handler (xmlNodePtr node, gpointer order_pdata)
{
    struct order_pdata *pdata = order_pdata;

    return set_string(node, pdata->order, gncOrderSetReference);
}

static gboolean
order_active_handler (xmlNodePtr node, gpointer order_pdata)
{
    struct order_pdata *pdata = order_pdata;
    gint64 val;
    gboolean ret;

    ret = dom_tree_to_integer(node, &val);
    if (ret)
      gncOrderSetActive(pdata->order, (gboolean)val);

    return ret;
}

static struct dom_tree_handler order_handlers_v2[] = {
    { order_guid_string, order_guid_handler, 1, 0 },
    { order_id_string, order_id_handler, 1, 0 },
    { order_owner_string, order_owner_handler, 1, 0 },
    { order_opened_string, order_opened_handler, 1, 0 },
    { order_closed_string, order_closed_handler, 0, 0 },
    { order_notes_string, order_notes_handler, 0, 0 },
    { order_reference_string, order_reference_handler, 0, 0 },
    { order_active_string, order_active_handler, 1, 0 },
    { NULL, 0, 0, 0 }
};

static GncOrder*
dom_tree_to_order (xmlNodePtr node, GNCBook *book)
{
    struct order_pdata order_pdata;
    gboolean successful;

    order_pdata.order = gncOrderCreate(book);
    order_pdata.book = book;
    gncOrderBeginEdit (order_pdata.order);

    successful = dom_tree_generic_parse (node, order_handlers_v2,
                                         &order_pdata);

    if (successful)
      gncOrderCommitEdit (order_pdata.order);
    else
    {
        PERR ("failed to parse order tree");
        gncOrderDestroy (order_pdata.order);
        order_pdata.order = NULL;
    }

    return order_pdata.order;
}

static gboolean
gnc_order_end_handler(gpointer data_for_children,
			 GSList* data_from_children, GSList* sibling_data,
			 gpointer parent_data, gpointer global_data,
			 gpointer *result, const gchar *tag)
{
    int successful;
    GncOrder *order;
    xmlNodePtr tree = (xmlNodePtr)data_for_children;
    gxpf_data *gdata = (gxpf_data*)global_data;
    GNCBook *book = gdata->bookdata;

    successful = TRUE;

    if(parent_data)
    {
        return TRUE;
    }

    /* OK.  For some messed up reason this is getting called again with a
       NULL tag.  So we ignore those cases */
    if(!tag)
    {
        return TRUE;
    }

    g_return_val_if_fail(tree, FALSE);

    order = dom_tree_to_order(tree, book);
    if(order != NULL)
    {
        gdata->cb(tag, gdata->parsedata, order);
    }

    xmlFreeNode(tree);

    return order != NULL;
}

static sixtp *
order_sixtp_parser_create(void)
{
  return sixtp_dom_parser_new(gnc_order_end_handler, NULL, NULL);
}

static void
do_count (gpointer order_p, gpointer count_p)
{
  int *count = count_p;
  (*count)++;
}

static int
order_get_count (GNCBook *book)
{
  int count = 0;
  gncObjectForeach (_GNC_MOD_NAME, book, do_count, (gpointer) &count);
  return count;
}

static void
xml_add_order (gpointer order_p, gpointer out_p)
{
  xmlNodePtr node;
  GncOrder *order = order_p;
  FILE *out = out_p;

  node = order_dom_tree_create (order);
  xmlElemDump(out, NULL, node);
  fprintf(out, "\n");
  xmlFreeNode (node);
}

static void
order_write (FILE *out, GNCBook *book)
{
  gncObjectForeach (_GNC_MOD_NAME, book, xml_add_order, (gpointer) out);
}

void
gnc_order_xml_initialize (void)
{
  static GncXmlDataType_t be_data = {
    GNC_FILE_BACKEND_VERS,
    gnc_order_string,
    order_sixtp_parser_create,
    NULL,			/* add_item */
    order_get_count,
    order_write,
  };

  gncObjectRegisterBackend (_GNC_MOD_NAME,
			    GNC_FILE_BACKEND,
			    &be_data);
}
