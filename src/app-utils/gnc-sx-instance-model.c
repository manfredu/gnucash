/* 
 * gnc-sx-instance-model.c
 *
 * Copyright (C) 2006 Josh Sled <jsled@asynchronous.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

#include "config.h"
#include <glib.h>
#include <glib-object.h>
#include <stdlib.h>

#include "Account.h"
#include "SX-book.h"
#include "SchedXaction.h"
#include "Scrub.h"
#include "Split.h"
#include "Transaction.h"
#include "gnc-book.h"
#include "gnc-commodity.h"
#include "gnc-event.h"
#include "gnc-exp-parser.h"
#include "gnc-glib-utils.h"
#include "gnc-sx-instance-model.h"
#include "gnc-ui-util.h"
#include "qof.h"

static QofLogModule log_module = GNC_MOD_SX;

static GObjectClass *parent_class = NULL;

static void gnc_sx_instance_model_class_init (GncSxInstanceModelClass *klass);
static void gnc_sx_instance_model_init(GTypeInstance *instance, gpointer klass);
static GncSxInstanceModel* gnc_sx_instance_model_new(void);

static GncSxInstance* gnc_sx_instance_new(GncSxInstances *parent, GncSxInstanceState state, GDate *date, void *temporal_state, gint sequence_num);

static gint _get_vars_helper(Transaction *txn, void *var_hash_data);

static GncSxVariable* gnc_sx_variable_new(gchar *name);

static void _gnc_sx_instance_event_handler(QofEntity *ent, QofEventId event_type, gpointer user_data, gpointer evt_data);

/* ------------------------------------------------------------ */

static void
_sx_var_to_raw_numeric(gchar *name, GncSxVariable *var, GHashTable *parser_var_hash)
{
     g_hash_table_insert(parser_var_hash, name, &var->value);
}

static void
_var_numeric_to_sx_var(gchar *name, gnc_numeric *num, GHashTable *sx_var_hash)
{
     gpointer p_var;
     if (!g_hash_table_lookup_extended(sx_var_hash, name, NULL, &p_var))
     {
          p_var = (gpointer)gnc_sx_variable_new(name);
          g_hash_table_insert(sx_var_hash, name, p_var);
     }
     ((GncSxVariable*)p_var)->value = *num;
}

static void
_wipe_parsed_sx_var(gchar *key, GncSxVariable *var, gpointer unused_user_data)
{
     var->value = gnc_numeric_error(GNC_ERROR_ARG);
}

/**
 * @return caller-owned.
 **/
GHashTable*
gnc_sx_instance_get_variables_for_parser(GHashTable *instance_var_hash)
{
     GHashTable *parser_vars;
     parser_vars = g_hash_table_new(g_str_hash, g_str_equal);
     g_hash_table_foreach(instance_var_hash, (GHFunc)_sx_var_to_raw_numeric, parser_vars);
     return parser_vars;
}

int
parse_vars_from_formula(const char *formula,
                        GHashTable *var_hash,
                        gnc_numeric *result)
{
     gnc_numeric num;
     char *errLoc = NULL;
     int toRet = 0;
     GHashTable *parser_vars;

     // convert var_hash -> variables for the parser.
     parser_vars = gnc_sx_instance_get_variables_for_parser(var_hash);

     num = gnc_numeric_zero();
     if (!gnc_exp_parser_parse_separate_vars(formula, &num, &errLoc, parser_vars))
     {
          toRet = -1;
     }

     // convert back.
     g_hash_table_foreach(parser_vars, (GHFunc)_var_numeric_to_sx_var, var_hash);
     g_hash_table_destroy(parser_vars);

     if (result != NULL)
     {
          *result = num;
     }

     return toRet;
}

static GncSxVariable*
gnc_sx_variable_new(gchar *name)
{
     GncSxVariable *var = g_new0(GncSxVariable, 1);
     var->name = name;
     var->value = gnc_numeric_error(GNC_ERROR_ARG);
     var->editable = TRUE;
     return var;
}

GncSxVariable*
gnc_sx_variable_new_full(gchar *name, gnc_numeric value, gboolean editable)
{
     GncSxVariable *var = gnc_sx_variable_new(name);
     var->value = value;
     var->editable = editable;
     return var;
}

static gint
_get_vars_helper(Transaction *txn, void *var_hash_data)
{
     GHashTable *var_hash = (GHashTable*)var_hash_data;
     GList *split_list;
     kvp_frame *kvpf;
     kvp_value *kvp_val;
     Split *s;
     char *str;
     gnc_commodity *first_cmdty = NULL;

     split_list = xaccTransGetSplitList(txn);
     if (split_list == NULL)
     {
          return 1;
     }

     for ( ; split_list; split_list = split_list->next)
     {
          gnc_commodity *split_cmdty = NULL;
          GUID *acct_guid;
          Account *acct;

          s = (Split*)split_list->data;
          kvpf = xaccSplitGetSlots(s);
          kvp_val = kvp_frame_get_slot_path(kvpf,
                                            GNC_SX_ID,
                                            GNC_SX_ACCOUNT,
                                            NULL);
          acct_guid = kvp_value_get_guid(kvp_val);
          acct = xaccAccountLookup(acct_guid, gnc_get_current_book());
          split_cmdty = xaccAccountGetCommodity(acct);
          if (first_cmdty == NULL)
          {
               first_cmdty = split_cmdty;
          }
                
          if (! gnc_commodity_equal(split_cmdty, first_cmdty))
          {
               GncSxVariable *var;
               GString *var_name;

               var_name = g_string_sized_new(16);
               g_string_printf(var_name, "%s -> %s",
                               gnc_commodity_get_mnemonic(split_cmdty),
                               gnc_commodity_get_mnemonic(first_cmdty));
               var = gnc_sx_variable_new(g_strdup(var_name->str));
               g_hash_table_insert(var_hash, var->name, var);
               g_string_free(var_name, TRUE);
          }

          // existing... ------------------------------------------
          kvp_val = kvp_frame_get_slot_path(kvpf,
                                            GNC_SX_ID,
                                            GNC_SX_CREDIT_FORMULA,
                                            NULL);
          if (kvp_val != NULL)
          {
               str = kvp_value_get_string(kvp_val);
               if (str && strlen(str) != 0)
               {
                    parse_vars_from_formula(str, var_hash, NULL);
               }
          }

          kvp_val = kvp_frame_get_slot_path(kvpf,
                                            GNC_SX_ID,
                                            GNC_SX_DEBIT_FORMULA,
                                            NULL);
          if (kvp_val != NULL)
          {
               str = kvp_value_get_string(kvp_val);
               if (str && strlen(str) != 0)
               {
                    parse_vars_from_formula(str, var_hash, NULL);
               }
          }
     }

     return 0;
}

Account*
gnc_sx_get_template_transaction_account(SchedXaction *sx)
{
     AccountGroup *template_group;
     Account *sx_template_acct;
     const char *sx_guid_str;

     template_group = gnc_book_get_template_group(gnc_get_current_book());
     sx_guid_str = guid_to_string(xaccSchedXactionGetGUID(sx));
     /* Get account named after guid string. */
     sx_template_acct = xaccGetAccountFromName(template_group, sx_guid_str);
     return sx_template_acct;
}

void
sxsl_get_sx_vars(SchedXaction *sx, GHashTable *var_hash)
{
     Account *sx_template_acct;
     sx_template_acct = gnc_sx_get_template_transaction_account(sx);
     xaccAccountForEachTransaction(sx_template_acct, _get_vars_helper, var_hash);
}

static void
_set_var_to_random_value(gchar *key, GncSxVariable *var, gpointer unused_user_data)
{
     var->value = double_to_gnc_numeric(rand() + 2, 1,
                                        GNC_NUMERIC_RND_MASK
                                        | GNC_RND_FLOOR);
}

void
gnc_sx_variable_free(GncSxVariable *var)
{
     // g_free(var->name);
     g_free(var);
}

void
randomize_variables(GHashTable *vars)
{
     g_hash_table_foreach(vars, (GHFunc)_set_var_to_random_value, NULL);
}

static void
_clone_sx_var_hash_entry(gpointer key, gpointer value, gpointer user_data)
{
     GHashTable *to = (GHashTable*)user_data;
     GncSxVariable *to_copy = (GncSxVariable*)value;
     GncSxVariable *var = gnc_sx_variable_new(to_copy->name);
     var->value = to_copy->value;
     var->editable = to_copy->editable;
     g_hash_table_insert(to, key, var);
}

static GncSxInstance*
gnc_sx_instance_new(GncSxInstances *parent, GncSxInstanceState state, GDate *date, void *temporal_state, gint sequence_num)
{
     GncSxInstance *rtn = g_new0(GncSxInstance, 1);
     rtn->parent = parent;
     rtn->orig_state = state;
     rtn->state = state;
     g_date_clear(&rtn->date, 1);
     rtn->date = *date;
     rtn->temporal_state = gnc_sx_clone_temporal_state(temporal_state);

     if (! parent->variable_names_parsed)
     {
          parent->variable_names = g_hash_table_new(g_str_hash, g_str_equal);
          sxsl_get_sx_vars(parent->sx, parent->variable_names);
          g_hash_table_foreach(parent->variable_names, (GHFunc)_wipe_parsed_sx_var, NULL);
          parent->variable_names_parsed = TRUE;
     }

     rtn->variable_bindings = g_hash_table_new(g_str_hash, g_str_equal);
     g_hash_table_foreach(parent->variable_names, _clone_sx_var_hash_entry, rtn->variable_bindings);

     {
          int instance_i_value;
          gnc_numeric i_num;
          GncSxVariable *as_var;

          instance_i_value = gnc_sx_get_instance_count(rtn->parent->sx, rtn->temporal_state);
          i_num = gnc_numeric_create(instance_i_value, 1);
          as_var = gnc_sx_variable_new_full("i", i_num, FALSE);

          g_hash_table_insert(rtn->variable_bindings, "i", as_var);
     }

     return rtn;
}

static void
_build_list_from_hash_elts(gpointer key, gpointer value, gpointer user_data)
{
     GList **list = (GList**)user_data;
     *list = g_list_append(*list, value);
}

GList *
gnc_sx_instance_get_variables(GncSxInstance *inst)
{
     GList *vars = NULL;
     g_hash_table_foreach(inst->variable_bindings, _build_list_from_hash_elts, &vars);
     // @@fixme sort by name
     // @@fixme make sure the returned list is freed by callers.
     return vars;
}

static GncSxInstances*
_gnc_sx_gen_instances(gpointer *data, gpointer user_data)
{
     GncSxInstances *instances = g_new0(GncSxInstances, 1);
     SchedXaction *sx = (SchedXaction*)data;
     GDate *range_end = (GDate*)user_data;
     GDate creation_end, remind_end;
     GDate cur_date;
     void *sequence_ctx;

     instances->sx = sx;

     creation_end = *range_end;
     g_date_add_days(&creation_end, xaccSchedXactionGetAdvanceCreation(sx));
     remind_end = creation_end;
     g_date_add_days(&remind_end, xaccSchedXactionGetAdvanceReminder(sx));

     /* postponed */
     {
          GList *postponed = gnc_sx_get_defer_instances(sx);
          for ( ; postponed != NULL; postponed = postponed->next)
          {
               GDate inst_date;
               int seq_num;
               GncSxInstance *inst;

               g_date_clear(&inst_date, 1);
               inst_date = xaccSchedXactionGetNextInstance(sx, postponed->data);
               seq_num = gnc_sx_get_instance_count(sx, postponed->data);
               inst = gnc_sx_instance_new(instances, SX_INSTANCE_STATE_POSTPONED, &inst_date, postponed->data, seq_num);
               //inst->temporal_state = postponed->data;
               instances->list = g_list_append(instances->list, inst);
          }
     }

     /* to-create */
     g_date_clear(&cur_date, 1);
     sequence_ctx = gnc_sx_create_temporal_state(sx);
     cur_date = xaccSchedXactionGetInstanceAfter(sx, &cur_date, sequence_ctx);
     instances->next_instance_date = cur_date;
     while (g_date_valid(&cur_date) && g_date_compare(&cur_date, &creation_end) <= 0)
     {
          GncSxInstance *inst;
          int seq_num;
          seq_num = gnc_sx_get_instance_count(sx, sequence_ctx);
          inst = gnc_sx_instance_new(instances, SX_INSTANCE_STATE_TO_CREATE, &cur_date, sequence_ctx, seq_num);
          instances->list = g_list_append(instances->list, inst);
          gnc_sx_incr_temporal_state(sx, sequence_ctx);
          cur_date = xaccSchedXactionGetInstanceAfter(sx, &cur_date, sequence_ctx);
     }

     /* reminders */
     while (g_date_valid(&cur_date) && g_date_compare(&cur_date, &remind_end) <= 0)
     {
          GncSxInstance *inst;
          int seq_num;
          seq_num = gnc_sx_get_instance_count(sx, sequence_ctx);
          inst = gnc_sx_instance_new(instances, SX_INSTANCE_STATE_REMINDER, &cur_date, sequence_ctx, seq_num);
          instances->list = g_list_append(instances->list, inst);
          gnc_sx_incr_temporal_state(sx, sequence_ctx);
          cur_date = xaccSchedXactionGetInstanceAfter(sx, &cur_date, sequence_ctx);
     }

     return instances;
}

GncSxInstanceModel*
gnc_sx_get_instances(GDate *range_end)
{
     GncSxInstanceModel *instances;
     GList *sxes;

     g_assert(range_end != NULL);
     g_assert(g_date_valid(range_end));

     instances = gnc_sx_instance_model_new();
     instances->range_end = *range_end;
     sxes = gnc_book_get_schedxactions(gnc_get_current_book())->sx_list;
     instances->sx_instance_list = gnc_g_list_map(sxes, (GncGMapFunc)_gnc_sx_gen_instances, range_end);

     return instances;
}

static GncSxInstanceModel*
gnc_sx_instance_model_new(void)
{
     return GNC_SX_INSTANCE_MODEL(g_object_new(GNC_TYPE_SX_INSTANCE_MODEL, NULL));
}

GType
gnc_sx_instance_model_get_type(void)
{
     static GType type = 0;
     if (type == 0) {
          static const GTypeInfo info = {
               sizeof (GncSxInstanceModelClass),
               NULL,   /* base_init */
               NULL,   /* base_finalize */
               (GClassInitFunc)gnc_sx_instance_model_class_init,   /* class_init */
               NULL,   /* class_finalize */
               NULL,   /* class_data */
               sizeof (GncSxInstanceModel),
               0,      /* n_preallocs */
               (GInstanceInitFunc)gnc_sx_instance_model_init    /* instance_init */
          };
          type = g_type_register_static (G_TYPE_OBJECT,
                                         "GncSxInstanceModelType",
                                         &info, 0);
     }
     return type;
}

static void
gnc_sx_instance_model_dispose(GObject *object)
{
     GncSxInstanceModel *model;
     g_return_if_fail(object != NULL);
     model = GNC_SX_INSTANCE_MODEL(object);

     g_return_if_fail(!model->disposed);
     model->disposed = TRUE;

     qof_event_unregister_handler(model->qof_event_handler_id);

     G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
gnc_sx_instance_free(GncSxInstance *instance)
{
     // @fixme:
     // variable_bindings elts + map
     // temporal_state (iff not postponed?)
     
     g_free(instance);
}

static void
gnc_sx_instances_free(GncSxInstances *instances)
{
     GList *instance_iter;
     // @fixme:
     // variable_names
     // sx = null

     for (instance_iter = instances->list; instance_iter != NULL; instance_iter = instance_iter->next)
     {
          GncSxInstance *inst = (GncSxInstance*)instance_iter->data;
          gnc_sx_instance_free(inst);
     }
     g_list_free(instances->list);
     instances->list = NULL;

     g_free(instances);
}

static void
gnc_sx_instance_model_finalize (GObject *object)
{
     GncSxInstanceModel *model;
     GList *sx_list_iter;

     g_return_if_fail(object != NULL);

     model = GNC_SX_INSTANCE_MODEL(object);
     for (sx_list_iter = model->sx_instance_list; sx_list_iter != NULL; sx_list_iter = sx_list_iter->next)
     {
          GncSxInstances *instances = (GncSxInstances*)sx_list_iter->data;
          gnc_sx_instances_free(instances);
     }
     g_list_free(model->sx_instance_list);
     model->sx_instance_list = NULL;

     G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gnc_sx_instance_model_class_init (GncSxInstanceModelClass *klass)
{
     GObjectClass *object_class = G_OBJECT_CLASS(klass);

     parent_class = g_type_class_peek_parent(klass);

     object_class->dispose = gnc_sx_instance_model_dispose;
     object_class->finalize = gnc_sx_instance_model_finalize;

     klass->removing_signal_id =
          g_signal_new("removing",
                       GNC_TYPE_SX_INSTANCE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       0, /* class offset */
                       NULL, /* accumulator */
                       NULL, /* accum data */
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE,
                       1,
                       G_TYPE_POINTER);

     klass->updated_signal_id =
          g_signal_new("updated",
                       GNC_TYPE_SX_INSTANCE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       0, /* class offset */
                       NULL, /* accumulator */
                       NULL, /* accum data */
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE,
                       1,
                       G_TYPE_POINTER);

     klass->added_signal_id =
          g_signal_new("added",
                       GNC_TYPE_SX_INSTANCE_MODEL,
                       G_SIGNAL_RUN_FIRST,
                       0, /* class offset */
                       NULL, /* accumulator */
                       NULL, /* accum data */
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE,
                       1,
                       G_TYPE_POINTER);
}

static void
gnc_sx_instance_model_init(GTypeInstance *instance, gpointer klass)
{
     GncSxInstanceModel *inst = (GncSxInstanceModel*)instance;

     g_date_clear(&inst->range_end, 1);
     inst->sx_instance_list = NULL;
     inst->qof_event_handler_id = qof_event_register_handler(_gnc_sx_instance_event_handler, inst);
}

static gint
_gnc_sx_instance_find_by_sx(GncSxInstances *in_list_instances, SchedXaction *sx_to_find)
{
     if (in_list_instances->sx == sx_to_find)
          return 0;
     return -1;
}

static void
_gnc_sx_instance_event_handler(QofEntity *ent, QofEventId event_type, gpointer user_data, gpointer evt_data)
{
     GncSxInstanceModel *instances = GNC_SX_INSTANCE_MODEL(user_data);

     /* selection rules {
     //   (gnc_collection_get_schedxaction_list(book), GNC_EVENT_ITEM_ADDED)
     //   (gnc_collection_get_schedxaction_list(book), GNC_EVENT_ITEM_REMOVED)
     //   (GNC_IS_SX(ent), QOF_EVENT_MODIFIED)
     // } */
     if (!(GNC_IS_SX(ent) || GNC_IS_SXES(ent)))
          return;

     if (GNC_IS_SX(ent))
     {
          SchedXaction *sx;
          gboolean sx_is_in_model = FALSE;

          sx = GNC_SX(ent);
          // only send `updated` if it's actually in the model
          sx_is_in_model = (g_list_find_custom(instances->sx_instance_list, sx, (GCompareFunc)_gnc_sx_instance_find_by_sx) != NULL);
          if (sx_is_in_model && event_type & QOF_EVENT_MODIFY)
          {
               g_signal_emit_by_name(instances, "updated", (gpointer)sx);
          }
          /* else { unsupported event type; ignore } */
     }
     else if (GNC_IS_SXES(ent))
     {
          SchedXactions *sxes = GNC_SXES(ent);
          SchedXaction *sx = GNC_SX(evt_data);

          sxes = NULL;
          if (event_type & GNC_EVENT_ITEM_REMOVED)
          {
               GList *instances_link;
               instances_link = g_list_find_custom(instances->sx_instance_list, sx, (GCompareFunc)_gnc_sx_instance_find_by_sx);
               if (instances_link != NULL)
               {
                    g_signal_emit_by_name(instances, "removing", (gpointer)sx);
               }
               else
               {
                    // @@ fixme:
                    printf("err\n");
               }
          }
          else if (event_type & GNC_EVENT_ITEM_ADDED)
          {
               /* generate instances, add to instance list, emit update. */
               instances->sx_instance_list
                    = g_list_append(instances->sx_instance_list,
                                    _gnc_sx_gen_instances((gpointer)sx, (gpointer)&instances->range_end));
               g_signal_emit_by_name(instances, "added", (gpointer)sx);
          }
          /* else { printf("unsupported event type [%d]\n", event_type); } */
     }
}

void
gnc_sx_instance_model_update_sx_instances(GncSxInstanceModel *model, SchedXaction *sx)
{
     GncSxInstances *existing, *new_instances;
     GList *link;

     link = g_list_find_custom(model->sx_instance_list, sx, (GCompareFunc)_gnc_sx_instance_find_by_sx);
     if (link == NULL)
     {
          // @fixme: log/error
          printf("couldn't find sx [%p]\n", sx);
          return;
     }

     // merge the new instance data into the existing structure, mutating as little as possible.
     existing = (GncSxInstances*)link->data;
     new_instances = _gnc_sx_gen_instances((gpointer)sx, &model->range_end);
     existing->sx = new_instances->sx;
     // @fixme: variable names stuff
     existing->next_instance_date = new_instances->next_instance_date;
     {
          GList *existing_iter, *new_iter;
          gboolean existing_remain, new_remain;

          // step through the lists pairwise, and retain the existing
          // instance if the dates align, as soon as they don't stop and
          // cleanup.
          existing_iter = existing->list;
          new_iter = new_instances->list;
          for (; existing_iter != NULL && new_iter != NULL; existing_iter = existing_iter->next, new_iter = new_iter->next)
          {
               GncSxInstance *existing_inst, *new_inst;
               gboolean same_instance_date;
               existing_inst = (GncSxInstance*)existing_iter->data;
               new_inst = (GncSxInstance*)new_iter->data;

               same_instance_date = g_date_compare(&existing_inst->date, &new_inst->date) == 0;
               if (!same_instance_date)
                    break;
          }

          existing_remain = (existing_iter != NULL);
          new_remain = (new_iter != NULL);

          if (existing_remain)
          {
               // delete excess
               gnc_g_list_cut(&existing->list, existing_iter);
               g_list_foreach(existing_iter, (GFunc)gnc_sx_instance_free, NULL);
          }

          if (new_remain)
          {
               // append new
               GList *new_iter_iter;
               gnc_g_list_cut(&new_instances->list, new_iter);

               for (new_iter_iter = new_iter; new_iter_iter != NULL; new_iter_iter = new_iter_iter->next)
               {
                    existing->list = g_list_append(existing->list, new_iter_iter->data);
               }
               g_list_free(new_iter);
          }
     }
     gnc_sx_instances_free(new_instances);
}

void
gnc_sx_instance_model_remove_sx_instances(GncSxInstanceModel *model, SchedXaction *sx)
{
     GList *instance_link = NULL;

     instance_link = g_list_find_custom(model->sx_instance_list, sx, (GCompareFunc)_gnc_sx_instance_find_by_sx);
     if (instance_link == NULL)
     {
          // @fixme: warn
          // printf("instance not found!\n");
          return;
     }

     model->sx_instance_list = g_list_remove_link(model->sx_instance_list, instance_link);
     gnc_sx_instances_free((GncSxInstances*)instance_link->data);
}

static void
increment_sx_state(GncSxInstance *inst, GDate **last_occur_date, int *instance_count, int *remain_occur_count)
{
     if (!g_date_valid(*last_occur_date)
         || (g_date_valid(*last_occur_date)
             && g_date_compare(*last_occur_date, &inst->date) <= 0))
     {
          *last_occur_date = &inst->date;
     }

     *instance_count = gnc_sx_get_instance_count(inst->parent->sx, inst->temporal_state);

     if (*remain_occur_count > 0)
     {
          *remain_occur_count -= 1;
     }
}

typedef struct _SxTxnCreationData
{
     GncSxInstance *instance;
     GList **created_txn_guids;
     GList **creation_errors;
} SxTxnCreationData;

static gboolean
_get_template_split_account(GncSxInstance *instance, Split *template_split, Account **split_acct, GList **creation_errors)
{
     GUID *acct_guid;
     kvp_frame *split_kvpf;
     kvp_value *kvp_val;

     split_kvpf = xaccSplitGetSlots(template_split);
     /* contains the guid of the split's actual account. */
     kvp_val = kvp_frame_get_slot_path(split_kvpf,
                                       GNC_SX_ID,
                                       GNC_SX_ACCOUNT,
                                       NULL);
     if (kvp_val == NULL)
     {
          // @@fixme: this should be more of an assert...
          GString *err = g_string_new("");
          g_string_printf(err, "Null account kvp value for SX [%s], cancelling creation.",
                          xaccSchedXactionGetName(instance->parent->sx));
          *creation_errors = g_list_append(*creation_errors, err);
          return FALSE;
     }
     acct_guid = kvp_value_get_guid( kvp_val );
     *split_acct = xaccAccountLookup(acct_guid, gnc_get_current_book());
     if (*split_acct == NULL)
     {
          const char *guid_str;
          GString *err;
          guid_str = guid_to_string((const GUID*)acct_guid);
          err = g_string_new("");
          g_string_printf(err, "Unknown account for guid [%s], cancelling SX [%s] creation.",
                          guid_str, xaccSchedXactionGetName(instance->parent->sx));
          g_free((char*)guid_str);
          *creation_errors = g_list_append(*creation_errors, err);
          return FALSE;
     }

     return TRUE;
}

static void
_get_sx_formula(GncSxInstance *instance, Split *template_split, gnc_numeric *numeric, GList **creation_errors, const char *formula_key)
{
     kvp_frame *split_kvpf;
     kvp_value *kvp_val;
     char *formula_str, *parseErrorLoc;

     split_kvpf = xaccSplitGetSlots(template_split);
     kvp_val = kvp_frame_get_slot_path(split_kvpf,
                                       GNC_SX_ID,
                                       formula_key,
                                       NULL);
     formula_str = kvp_value_get_string(kvp_val);
     if (formula_str != NULL && strlen(formula_str) != 0)
     {
          GHashTable *parser_vars = gnc_sx_instance_get_variables_for_parser(instance->variable_bindings);
          if (!gnc_exp_parser_parse_separate_vars(formula_str,
                                                  numeric,
                                                  &parseErrorLoc,
                                                  parser_vars))
          {
               GString *err = g_string_new("");
               g_string_printf(err, "Error parsing SX [%s] key [%s]=formula [%s] at [%s]: %s",
                               xaccSchedXactionGetName(instance->parent->sx),
                               formula_key,
                               formula_str,
                               parseErrorLoc,
                               gnc_exp_parser_error_string());
               *creation_errors = g_list_append(*creation_errors, err);
          }
          g_hash_table_destroy(parser_vars);
     }
}

static void
_get_credit_formula(GncSxInstance *instance, Split *template_split, gnc_numeric *credit_num, GList **creation_errors)
{
     _get_sx_formula(instance, template_split, credit_num, creation_errors, GNC_SX_CREDIT_FORMULA);
}

static void
_get_debit_formula(GncSxInstance *instance, Split *template_split, gnc_numeric *debit_num, GList **creation_errors)
{
     _get_sx_formula(instance, template_split, debit_num, creation_errors, GNC_SX_DEBIT_FORMULA);
}

static gboolean
create_each_transaction_helper(Transaction *template_txn, void *user_data)
{
     Transaction *new_txn;
     GList *txn_splits, *template_splits;
     Split *copying_split;
     gnc_commodity *first_cmdty = NULL;
     gboolean err_flag = FALSE;
     SxTxnCreationData *creation_data;

     creation_data = (SxTxnCreationData*)user_data;

     /* FIXME: In general, this should [correctly] deal with errors such
        as not finding the approrpiate Accounts and not being able to
        parse the formula|credit/debit strings. */

     new_txn = xaccTransClone(template_txn);
     xaccTransBeginEdit(new_txn);

     /* clear any copied KVP data */
     qof_instance_set_slots(QOF_INSTANCE(new_txn), kvp_frame_new());

     xaccTransSetDate(new_txn,
                      g_date_get_day(&creation_data->instance->date),
                      g_date_get_month(&creation_data->instance->date),
                      g_date_get_year(&creation_data->instance->date));
        
     /* the accounts and amounts are in the kvp_frames of the splits. */
     template_splits = xaccTransGetSplitList(template_txn);
     txn_splits = xaccTransGetSplitList(new_txn);
     if ((template_splits == NULL) || (txn_splits == NULL))
     {
          PERR("\tseen transaction w/o splits. :(");
          xaccTransDestroy(new_txn);
          xaccTransCommitEdit(new_txn);
          return FALSE;
     }

     for (;
          txn_splits && template_splits;
          txn_splits = txn_splits->next, template_splits = template_splits->next)
     {
          Split *template_split;
          Account *split_acct;
          gnc_commodity *split_cmdty = NULL;
             
          /* FIXME: Ick.  This assumes that the split lists will be ordered
             identically. :( They are, but we'd rather not have to count on
             it. --jsled */
          template_split = (Split*)template_splits->data;
          copying_split = (Split*)txn_splits->data;

          /* clear out any copied Split frame data. */
          qof_instance_set_slots(QOF_INSTANCE(copying_split), kvp_frame_new());

          if (!_get_template_split_account(creation_data->instance, template_split, &split_acct, creation_data->creation_errors))
          {
               err_flag = TRUE;
               break;
          }
             
          split_cmdty = xaccAccountGetCommodity(split_acct);
          if (first_cmdty == NULL)
          {
               first_cmdty = split_cmdty;
               xaccTransSetCurrency(new_txn, first_cmdty);
          }

          xaccAccountBeginEdit(split_acct);
          xaccAccountInsertSplit(split_acct, copying_split);

          {
               gnc_numeric credit_num, debit_num, final;
               gint gncn_error;

               credit_num = gnc_numeric_zero();
               debit_num = gnc_numeric_zero();

               _get_credit_formula(creation_data->instance, template_split, &credit_num, creation_data->creation_errors);
               _get_debit_formula(creation_data->instance, template_split, &debit_num, creation_data->creation_errors);
                       
               final = gnc_numeric_sub_fixed( debit_num, credit_num );
                        
               gncn_error = gnc_numeric_check(final);
               if (gncn_error != GNC_ERROR_OK) {
                    GString *err = g_string_new("");
                    g_string_printf(err, "Error %d in SX [%s] final gnc_numeric value, using 0 instead.", 
                                    gncn_error,
                                    xaccSchedXactionGetName(creation_data->instance->parent->sx));
                    *creation_data->creation_errors = g_list_append(*creation_data->creation_errors, err);
                    final = gnc_numeric_zero();
               }

               xaccSplitSetValue(copying_split, final);
               if (! gnc_commodity_equal(split_cmdty, first_cmdty))
               {
                    GString *exchange_rate_var_name = g_string_sized_new(16);
                    GncSxVariable *exchange_rate_var;
                    gnc_numeric exchange_rate, amt;

                    /*
                      GNCPriceDB *price_db = gnc_pricedb_get_db(gnc_get_current_book());
                      GNCPrice *price;

                      price = gnc_pricedb_lookup_latest(price_db, first_cmdty, split_cmdty);
                      if (price == NULL)
                      {
                      price = gnc_pricedb_lookup_latest(price_db, split_cmdty, first_cmdty);
                      if (price == NULL)
                      {
                      GString *err = g_string_new("");
                      g_string_printf(err, "could not find pricedb entry for commodity-pair (%s, %s).",
                      gnc_commodity_get_mnemonic(first_cmdty),
                      gnc_commodity_get_mnemonic(split_cmdty));
                      exchange = gnc_numeric_create(1, 1);
                      *creation_data->creation_errors = g_list_append(*creation_data->creation_errors, err);

                      }
                      else
                      {
                      exchange = gnc_numeric_div(gnc_numeric_create(1,1),
                      gnc_price_get_value(price),
                      1000, GNC_HOW_RND_ROUND);
                      }
                      }
                      else
                      {
                      exchange = gnc_price_get_value(price);
                      }
                    */

                    exchange_rate = gnc_numeric_zero();
                    g_string_printf(exchange_rate_var_name, "%s -> %s",
                                    gnc_commodity_get_mnemonic(split_cmdty),
                                    gnc_commodity_get_mnemonic(first_cmdty));
                    exchange_rate_var = (GncSxVariable*)g_hash_table_lookup(creation_data->instance->variable_bindings,
                                                                            exchange_rate_var_name->str);
                    if (exchange_rate_var != NULL)
                    {
                         exchange_rate = exchange_rate_var->value;
                    }
                    g_string_free(exchange_rate_var_name, TRUE);

                    amt = gnc_numeric_mul(final, exchange_rate, 1000, GNC_HOW_RND_ROUND);
                    xaccSplitSetAmount(copying_split, amt);
               }

               xaccSplitScrub(copying_split);
          }

          xaccAccountCommitEdit(split_acct);
     }

     if (err_flag)
     {
          PERR("Some error in new transaction creation...");
          xaccTransDestroy(new_txn);
          xaccTransCommitEdit(new_txn);
          return FALSE;
     }

     {
          kvp_frame *txn_frame;
          /* set a kvp-frame element in the transaction indicating and
           * pointing-to the SX this was created from. */
          txn_frame = xaccTransGetSlots(new_txn);
          kvp_frame_set_guid(txn_frame, "from-sched-xaction", xaccSchedXactionGetGUID(creation_data->instance->parent->sx));
     }

     xaccTransCommitEdit(new_txn);

     if (creation_data->created_txn_guids != NULL)
     {
          *creation_data->created_txn_guids
               = g_list_append(*(creation_data->created_txn_guids), (gpointer)xaccTransGetGUID(new_txn));
     }

     return TRUE;
}

static void
create_transactions_for_instance(GncSxInstance *instance, GList **created_txn_guids, GList **creation_errors)
{
     SxTxnCreationData creation_data;
     Account *sx_template_account;

     sx_template_account = gnc_sx_get_template_transaction_account(instance->parent->sx);

     creation_data.instance = instance;
     creation_data.created_txn_guids = created_txn_guids;
     creation_data.creation_errors = creation_errors;

     xaccAccountForEachTransaction(sx_template_account,
                                   create_each_transaction_helper,
                                   &creation_data);
}

void
gnc_sx_instance_model_effect_change(GncSxInstanceModel *model,
                                    gboolean auto_create_only,
                                    GList **created_transaction_guids,
                                    GList **creation_errors)
{
     GList *iter;
     for (iter = model->sx_instance_list; iter != NULL; iter = iter->next)
     {
          GList *instance_iter;
          GncSxInstances *instances = (GncSxInstances*)iter->data;
          GDate *last_occur_date;
          gint instance_count = 0;
          gint remain_occur_count = 0;

          last_occur_date = xaccSchedXactionGetLastOccurDate(instances->sx);
          instance_count = gnc_sx_get_instance_count(instances->sx, NULL);
          remain_occur_count = xaccSchedXactionGetRemOccur(instances->sx);

          for (instance_iter = instances->list; instance_iter != NULL; instance_iter = instance_iter->next)
          {
               GncSxInstance *inst = (GncSxInstance*)instance_iter->data;
               gboolean sx_is_auto_create;

               xaccSchedXactionGetAutoCreate(inst->parent->sx, &sx_is_auto_create, NULL);
               if (auto_create_only && !sx_is_auto_create)
               {
                    if (inst->state != SX_INSTANCE_STATE_TO_CREATE)
                    {
                         break;
                    }
                    continue;
               }

               if (inst->orig_state == SX_INSTANCE_STATE_POSTPONED
                   && inst->state != SX_INSTANCE_STATE_POSTPONED)
               {
                    // remove from postponed list
                    g_assert(inst->temporal_state != NULL);
                    gnc_sx_remove_defer_instance(inst->parent->sx, inst->temporal_state);
               }

               switch (inst->state)
               {
               case SX_INSTANCE_STATE_CREATED:
                    // nop: we've already processed this.
                    break;
               case SX_INSTANCE_STATE_IGNORED:
                    increment_sx_state(inst, &last_occur_date, &instance_count, &remain_occur_count);
                    break;
               case SX_INSTANCE_STATE_POSTPONED:
                    if (inst->orig_state != SX_INSTANCE_STATE_POSTPONED)
                    {
                         gnc_sx_add_defer_instance(instances->sx, inst->temporal_state);
                    }
                    increment_sx_state(inst, &last_occur_date, &instance_count, &remain_occur_count);
                    break;
               case SX_INSTANCE_STATE_TO_CREATE:
                    create_transactions_for_instance(inst, created_transaction_guids, creation_errors);
                    increment_sx_state(inst, &last_occur_date, &instance_count, &remain_occur_count);
                    gnc_sx_instance_model_change_instance_state(model, inst, SX_INSTANCE_STATE_CREATED);
                    break;
               case SX_INSTANCE_STATE_REMINDER:
                    // do nothing
                    // assert no non-remind instances after this?
                    break;
               default:
                    g_assert_not_reached();
                    break;
               }
          }
          
          xaccSchedXactionSetLastOccurDate(instances->sx, last_occur_date);
          gnc_sx_set_instance_count(instances->sx, instance_count);
          xaccSchedXactionSetRemOccur(instances->sx, remain_occur_count);
     }
}

void
gnc_sx_instance_model_change_instance_state(GncSxInstanceModel *model,
                                            GncSxInstance *instance,
                                            GncSxInstanceState new_state)
{
     if (instance->state == new_state)
          return;

     instance->state = new_state;

     // ensure 'remind' constraints are met:
     {
          GList *inst_iter;
          inst_iter = g_list_find(instance->parent->list, instance);
          g_assert(inst_iter != NULL);
          if (instance->state != SX_INSTANCE_STATE_REMINDER)
          {
               // iterate backwards, making sure reminders are changed to 'postponed'
               for (inst_iter = inst_iter->prev; inst_iter != NULL; inst_iter = inst_iter->prev)
               {
                    GncSxInstance *prev_inst = (GncSxInstance*)inst_iter->data;
                    if (prev_inst->state != SX_INSTANCE_STATE_REMINDER)
                         continue;
                    prev_inst->state = SX_INSTANCE_STATE_POSTPONED;
               }
          }
          else
          {
               // iterate forward, make sure transactions are set to 'remind'
               for (inst_iter = inst_iter->next; inst_iter != NULL; inst_iter = inst_iter->next)
               {
                    GncSxInstance *next_inst = (GncSxInstance*)inst_iter->data;
                    if (next_inst->state == SX_INSTANCE_STATE_REMINDER)
                         continue;
                    next_inst->state = SX_INSTANCE_STATE_REMINDER;
               }
          }
     }

     g_signal_emit_by_name(model, "updated", (gpointer)instance->parent->sx);
}

void
gnc_sx_instance_model_set_variable(GncSxInstanceModel *model,
                                   GncSxInstance *instance,
                                   GncSxVariable *variable,
                                   gnc_numeric *new_value)
{

     if (gnc_numeric_equal(variable->value, *new_value))
          return;
     variable->value = *new_value;
     g_signal_emit_by_name(model, "updated", (gpointer)instance->parent->sx);
}

static void
_list_from_hash_elts(gpointer key, gpointer value, GList **result_list)
{
     *result_list = g_list_append(*result_list, value);
}

GList*
gnc_sx_instance_model_check_variables(GncSxInstanceModel *model)
{
     GList *rtn = NULL;
     GList *sx_iter, *inst_iter, *var_list = NULL, *var_iter;

     for (sx_iter = model->sx_instance_list; sx_iter != NULL; sx_iter = sx_iter->next)
     {
          GncSxInstances *instances = (GncSxInstances*)sx_iter->data;
          for (inst_iter = instances->list; inst_iter != NULL; inst_iter = inst_iter->next)
          {
               GncSxInstance *inst = (GncSxInstance*)inst_iter->data;

               if (inst->state != SX_INSTANCE_STATE_TO_CREATE)
                    continue;

               g_hash_table_foreach(inst->variable_bindings, (GHFunc)_list_from_hash_elts, &var_list);
               for (var_iter = var_list; var_iter != NULL; var_iter = var_iter->next)
               {
                    GncSxVariable *var = (GncSxVariable*)var_iter->data;
                    if (gnc_numeric_check(var->value) != GNC_ERROR_OK)
                    {
                         GncSxVariableNeeded *need = g_new0(GncSxVariableNeeded, 1);
                         need->instance = inst;
                         need->variable = var;
                         rtn = g_list_append(rtn, need);
                    }
               }
               g_list_free(var_list);
               var_list = NULL;
          }
     }
     return rtn;
}
