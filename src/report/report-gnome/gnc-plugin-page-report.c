/* gnc-plugin-page-report.c
 * Copyright (C) 2004 Joshua Sled <jsled@asynchronous.org>
 *
 * Originally from window-report.c:
 * Copyright (C) 1997 Robin D. Clark
 * Copyright (C) 1998 Linas Vepstas
 * Copyright (C) 1999 Jeremy Collins ( gtk-xmhtml port )
 * Copyright (C) 2000 Dave Peticolas
 * Copyright (C) 2000 Bill Gribble
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
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
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652
 * Boston, MA  02111-1307,  USA       gnu@gnu.org
 */

#include "config.h"

#include <gtk/gtk.h>
#include <g-wrap-wct.h>
#include <libguile.h>

#include "egg-menu-merge.h"
#include "gnc-component-manager.h"
#include "gnc-engine-util.h"
#include "gnc-gnome-utils.h"
#include "gnc-html-history.h"
#include "gnc-html.h"
#include "gnc-plugin-page-report.h"
#include "gnc-report.h"
#include "gnc-ui-util.h"
#include "gnc-ui.h"
#include "gnc-window.h"
#include "messages.h"
#include "option-util.h"

#define WINDOW_REPORT_CM_CLASS "window-report"

static short module = MOD_TEST; // MOD_GUI; // MOD_REPORT;?

static GObjectClass *parent_class = NULL;
static GList *active_pages = NULL;

// Property-id values.
enum {
    PROP_0,
    PROP_REPORT_ID,
  };

static void gnc_plugin_page_report_class_init( GncPluginPageReportClass *klass );
static void gnc_plugin_page_report_init( GncPluginPageReport *plugin_page );
static void gnc_plugin_page_report_finalize (GObject *object);
static void gnc_plugin_page_report_setup( GncPluginPage *ppage );

static GtkWidget* gnc_plugin_page_report_create_widget( GncPluginPage *plugin_page );
static void gnc_plugin_page_report_destroy_widget( GncPluginPage *plugin_page );
static void gnc_plugin_page_report_merge_actions( GncPluginPage *plugin_page, EggMenuMerge *merge );
static void gnc_plugin_page_report_unmerge_actions( GncPluginPage *plugin_page, EggMenuMerge *merge );
static void gnc_plugin_page_report_destroy_widget( GncPluginPage *plugin_page );

static int gnc_plugin_page_report_check_urltype(URLType t);
static void gnc_plugin_page_report_load_cb(gnc_html * html, URLType type,
                                      const gchar * location, const gchar * label,
                                      gpointer data);
static void gnc_plugin_page_report_draw_cb(GtkWidget *unused, GdkRectangle *unused1, gpointer data);
static void gnc_plugin_page_report_refresh (gpointer data);
static void gnc_plugin_page_report_set_fwd_button(GncPluginPageReportPrivate * win, int enabled);
static void gnc_plugin_page_report_set_back_button(GncPluginPageReportPrivate * win, int enabled);
static void gnc_plugin_page_report_history_destroy_cb(gnc_html_history_node * node, gpointer user_data);
static void close_handler(gpointer user_data);
void gnc_plugin_page_report_destroy(GncPluginPageReportPrivate * win);
static void gnc_plugin_page_report_option_change_cb(gpointer data);

void gnc_plugin_page_report_remove_edited_report(GncPluginPageReportPrivate * win, SCM report);
void gnc_plugin_page_report_add_edited_report(GncPluginPageReportPrivate * win, SCM report);
void gnc_plugin_page_report_raise_editor(SCM report);

static void gnc_plugin_page_report_back_cb(EggAction *action, GncPluginPageReport *rep);
static void gnc_plugin_page_report_forw_cb(EggAction *action, GncPluginPageReport *rep);
static void gnc_plugin_page_report_reload_cb(EggAction *action, GncPluginPageReport *rep);
static void gnc_plugin_page_report_stop_cb(EggAction *action, GncPluginPageReport *rep);
static void gnc_plugin_page_report_export_cb(EggAction *action, GncPluginPageReport *rep);
static void gnc_plugin_page_report_options_cb(EggAction *action, GncPluginPageReport *rep);
static void gnc_plugin_page_report_print_cb(EggAction *action, GncPluginPageReport *rep);

struct GncPluginPageReportPrivate
{
        /// The report-id
        int reportId;

        /// The report which this Page is satisifying
        SCM cur_report;
        /// The Option DB for this report.
        GNCOptionDB *cur_odb;
        SCM option_change_cb_id;

        /* initial_report is special; it's the one that's saved and 
         * restored.  The name_change_callback only gets called when
         * the initial_report name is changed. */
        SCM          initial_report; 
        GNCOptionDB  * initial_odb;
        SCM          name_change_cb_id;
        
        SCM          edited_reports;

        /* This is set to mark the fact that we need to reload the html */
        gboolean	need_reload;

        /// the gnc_html abstraction this PluginPage contains
        gnc_html *html;

        /// the container the above HTML widget is in.
        GtkContainer *container;

        /// The EggActionGroup to use for [un]merging our UI.
        EggActionGroup *action_group;

        EggMenuMerge *ui_merge;
        gint merge_id;
};

GType
gnc_plugin_page_report_get_type (void)
{
	static GType gnc_plugin_page_report_type = 0;

	if (gnc_plugin_page_report_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GncPluginPageReportClass),
			NULL,
			NULL,
			(GClassInitFunc) gnc_plugin_page_report_class_init,
			NULL,
			NULL,
			sizeof (GncPluginPageReport),
			0,
			(GInstanceInitFunc) gnc_plugin_page_report_init
		};
		
		gnc_plugin_page_report_type = g_type_register_static (GNC_TYPE_PLUGIN_PAGE,
                                                                      "GncPluginPageReport",
                                                                      &our_info, 0);
	}

	return gnc_plugin_page_report_type;
}

static void
gnc_plugin_page_report_get_property( GObject *obj,
                                     guint prop_id,
                                     GValue *value,
                                     GParamSpec *pspec )
{
        GncPluginPageReport *rep;
        GncPluginPageReportPrivate * priv;

        rep = GNC_PLUGIN_PAGE_REPORT( obj );
        priv = (GncPluginPageReportPrivate*)rep->priv;
        
        switch ( prop_id )
        {
        case PROP_REPORT_ID:
                g_value_set_int( value, priv->reportId );
                break;
        default:
                PERR( "Unknown property id %d", prop_id );
                break;
        }
}

static void
gnc_plugin_page_report_set_property( GObject *obj,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec )
{
        GncPluginPageReport *rep;
        GncPluginPageReportPrivate *priv;

        rep = GNC_PLUGIN_PAGE_REPORT( obj );
        priv = (GncPluginPageReportPrivate*)rep->priv;

        DEBUG( "setting property with id %d / %p to value %d",
               prop_id, rep->priv, g_value_get_int( value ) );

        switch ( prop_id )
        {
        case PROP_REPORT_ID:
                priv->reportId = g_value_get_int( value );
                break;
        default:
                PERR( "unknown property id %d", prop_id );
                break;
        }
               
}

static void
gnc_plugin_page_report_class_init (GncPluginPageReportClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GncPluginPageClass *gnc_plugin_page_class = GNC_PLUGIN_PAGE_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gnc_plugin_page_report_finalize;

        object_class->set_property = gnc_plugin_page_report_set_property;
        object_class->get_property = gnc_plugin_page_report_get_property;

        // FIXME: stock reporting icon?
	//gnc_plugin_page_class->tab_icon        = GNC_STOCK_ACCOUNT;
	gnc_plugin_page_class->plugin_name     = GNC_PLUGIN_PAGE_REPORT_NAME;

	gnc_plugin_page_class->create_widget   = gnc_plugin_page_report_create_widget;
	gnc_plugin_page_class->destroy_widget  = gnc_plugin_page_report_destroy_widget;
	gnc_plugin_page_class->merge_actions   = gnc_plugin_page_report_merge_actions;
	gnc_plugin_page_class->unmerge_actions = gnc_plugin_page_report_unmerge_actions;

        // create the "reportId" property
        g_object_class_install_property( object_class,
                                         PROP_REPORT_ID,
                                         g_param_spec_int( "report_id",
                                                           _("The numeric ID of the report."),
                                                           _("The numeric ID of the report."),
                                                           -1, G_MAXINT, -1, G_PARAM_READWRITE ) );

/* JSLED: report-selected?
	plugin_page_signals[ACCOUNT_SELECTED] =
	  g_signal_new ("account_selected",
			G_OBJECT_CLASS_TYPE (object_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (GncPluginPageReportClass, account_selected),
			NULL, NULL,
			g_cclosure_marshal_VOID__POINTER,
			G_TYPE_NONE, 1,
			G_TYPE_POINTER);
*/
}

static void
gnc_plugin_page_report_finalize (GObject *object)
{
	GncPluginPageReport *page;
	GncPluginPageReportPrivate *priv;

	ENTER("object %p", object);
	page = GNC_PLUGIN_PAGE_REPORT (object);
	g_return_if_fail (GNC_IS_PLUGIN_PAGE_REPORT (page));
	priv = page->priv;
	g_return_if_fail (priv != NULL);

	active_pages = g_list_remove (active_pages, page);

	G_OBJECT_CLASS (parent_class)->finalize (object);
	LEAVE(" ");
}

static
GtkWidget*
gnc_plugin_page_report_create_widget( GncPluginPage *page )
{
        GncPluginPageReport *gppReport;
        GncPluginPageReportPrivate *report;
        GtkWindow *topLvl;
        URLType type;
        char * id_name;
        char * child_name;
        char * url_location = NULL;
        char * url_label = NULL;

        gppReport = GNC_PLUGIN_PAGE_REPORT(page);

        report = (GncPluginPageReportPrivate*)gppReport->priv;
        topLvl = GTK_WINDOW(gnc_ui_get_toplevel());
        report->html              = gnc_html_new( topLvl );

        gnc_html_history_set_node_destroy_cb(gnc_html_get_history(report->html),
                                             gnc_plugin_page_report_history_destroy_cb,
                                             (gpointer)report);
  
        report->container = GTK_CONTAINER(gtk_frame_new(NULL));
        gtk_frame_set_shadow_type(GTK_FRAME(report->container), GTK_SHADOW_NONE);
  
        gtk_container_add(GTK_CONTAINER(report->container), 
                          gnc_html_get_widget(report->html));
  
        gnc_register_gui_component( WINDOW_REPORT_CM_CLASS, NULL,
                                    close_handler, report );
  
        gnc_html_set_urltype_cb(report->html, gnc_plugin_page_report_check_urltype);
        gnc_html_set_load_cb(report->html, gnc_plugin_page_report_load_cb, report);

        // FIXME.  This is f^-1(f(x)), isn't it?
        DEBUG( "id=%d", report->reportId );
        id_name = g_strdup_printf("id=%d", report->reportId );
        child_name = gnc_build_url( URL_TYPE_REPORT, id_name, NULL );
        type = gnc_html_parse_url( report->html, child_name, &url_location, &url_label);
        DEBUG( "passing id_name=[%s] child_name=[%s] type=[%s], location=[%s], label=[%s]",
               id_name, child_name, type, url_location, url_label );

        gnc_window_set_progressbar_window( GNC_WINDOW(gnc_ui_get_toplevel()) );
        gnc_html_show_url(report->html, type, url_location, url_label, 0);
        gnc_window_set_progressbar_window( NULL );

        gtk_signal_connect(GTK_OBJECT(report->container), "draw",
                           GTK_SIGNAL_FUNC(gnc_plugin_page_report_draw_cb), report);
  
        gtk_widget_show_all( GTK_WIDGET(report->container) );

        return GTK_WIDGET( report->container );
}

/********************************************************************
 * gnc_plugin_page_report_check_urltype
 * is it OK to show a certain URLType in this window?
 ********************************************************************/
static int
gnc_plugin_page_report_check_urltype(URLType t)
{
  if (!safe_strcmp (t, URL_TYPE_REPORT)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
 * Simply get the initial report given the id, so we can do initialization
 * things like setup the tab name based on the report's name.
 **/
static void
gnc_plugin_page_report_setup( GncPluginPage *ppage )
{
        GncPluginPageReport *page = GNC_PLUGIN_PAGE_REPORT(ppage);
        GncPluginPageReportPrivate *report = (GncPluginPageReportPrivate*)page->priv;
        SCM  find_report = scm_c_eval_string("gnc:find-report");
        SCM  inst_report;
        int  report_id;

        report->cur_report        = SCM_BOOL_F;
        report->initial_report    = SCM_BOOL_F;
        report->edited_reports    = SCM_EOL;
        report->name_change_cb_id = SCM_BOOL_F;

        scm_protect_object(report->cur_report);
        scm_protect_object(report->initial_report);
        scm_protect_object(report->edited_reports);

        g_object_get( ppage, "report_id", &report_id, NULL );
        
        /* get the inst-report from the Scheme-side hash, and get its
         * options and editor thunk */
        if ((inst_report = scm_call_1(find_report, scm_int2num(report_id)))
            == SCM_BOOL_F) {
                return;
        }
        
        if (report->initial_report == SCM_BOOL_F) {
                scm_unprotect_object(report->initial_report);
                report->initial_report = inst_report;
                scm_protect_object(report->initial_report);
        }
}

/********************************************************************
 * gnc_plugin_page_report_load_cb
 * called after a report is loaded into the gnc_html widget 
 ********************************************************************/
static void 
gnc_plugin_page_report_load_cb(gnc_html * html, URLType type, 
                               const gchar * location, const gchar * label, 
                               gpointer data)
{
        //GncPluginPageReport *report = GNC_PLUGIN_PAGE_REPORT();
        GncPluginPageReportPrivate *win = (GncPluginPageReportPrivate*)data;
        int  report_id;
        SCM  find_report    = scm_c_eval_string("gnc:find-report");
        SCM  get_options    = scm_c_eval_string("gnc:report-options");
        SCM  set_needs_save = scm_c_eval_string("gnc:report-set-needs-save?!");
        SCM  inst_report;

        ENTER( "load_cb: type=[%s], location=[%s], label=[%s]",
               type, location, label );

        /* we get this callback if a new report is requested to be loaded OR
         * if any URL is clicked.  If an options URL is clicked, we want to
         * know about it */
        if (!safe_strcmp (type, URL_TYPE_REPORT)
            && location
            && (strlen(location) > 3)
            && !strncmp("id=", location, 3)) {
                sscanf(location+3, "%d", &report_id);
                DEBUG( "parsed id=%d", report_id );
        }
        else if (!safe_strcmp( type, URL_TYPE_OPTIONS)
                 && location
                 && (strlen(location) > 10)
                 && !strncmp("report-id=", location, 10)) {
                sscanf(location+10, "%d", &report_id);
                inst_report = scm_call_1(find_report, scm_int2num(report_id));
                if (inst_report != SCM_BOOL_F) {
                        gnc_plugin_page_report_add_edited_report(win, inst_report);
                }
                return;
        } else {
                LEAVE( " unknown URL type [%s] location [%s]", type, location );
                return;
        }
        
        /* get the inst-report from the Scheme-side hash, and get its
         * options and editor thunk */
        if ((inst_report = scm_call_1(find_report, scm_int2num(report_id)))
            == SCM_BOOL_F) {
                LEAVE( "error getting inst_report" );
                return;
        }
        
        if (win->initial_report == SCM_BOOL_F) {    
                scm_unprotect_object(win->initial_report);
                win->initial_report = inst_report;
                scm_protect_object(win->initial_report);
                
                scm_call_2(set_needs_save, inst_report, SCM_BOOL_T);
                
                win->initial_odb = gnc_option_db_new(scm_call_1(get_options, inst_report));  
                win->name_change_cb_id = 
                        gnc_option_db_register_change_callback(win->initial_odb,
                                                               gnc_plugin_page_report_refresh,
                                                               win,
                                                               "General", "Report name");
        }
        
        if ((win->cur_report != SCM_BOOL_F) && (win->cur_odb != NULL)) {
                gnc_option_db_unregister_change_callback_id(win->cur_odb,
                                                            win->option_change_cb_id);
                gnc_option_db_destroy(win->cur_odb);
                win->cur_odb = NULL;
        }
        
        if (win->cur_report != SCM_BOOL_F)
                scm_unprotect_object(win->cur_report);
        win->cur_report = inst_report;
        scm_protect_object(win->cur_report);
        
        win->cur_odb = gnc_option_db_new(scm_call_1(get_options, inst_report));  
        win->option_change_cb_id = 
                gnc_option_db_register_change_callback(win->cur_odb,
                                                       gnc_plugin_page_report_option_change_cb,
                                                       win, NULL, NULL);
        
        if (gnc_html_history_forward_p(gnc_html_get_history(win->html))) {
                gnc_plugin_page_report_set_fwd_button(win, TRUE); 
        } else {
                gnc_plugin_page_report_set_fwd_button(win, FALSE); 
        }
        
        if(gnc_html_history_back_p(gnc_html_get_history(win->html))) {
                gnc_plugin_page_report_set_back_button(win, TRUE); 
        } else {
                gnc_plugin_page_report_set_back_button(win, FALSE); 
        }

        LEAVE( "done" );
}

static void
gnc_plugin_page_report_option_change_cb(gpointer data)
{
        GncPluginPageReportPrivate * report = data;
        SCM               dirty_report = scm_c_eval_string("gnc:report-set-dirty?!");

        if (report->cur_report != SCM_BOOL_F) {
                /* it's probably already dirty, but make sure */
                scm_call_2(dirty_report, report->cur_report, SCM_BOOL_T);

                /* Now queue the fact that we need to reload this report */
                report->need_reload = TRUE;
                gtk_widget_queue_draw(GTK_WIDGET(report->container));
        }
}

static void 
gnc_plugin_page_report_history_destroy_cb(gnc_html_history_node * node, 
                                          gpointer user_data)
{
        static SCM         remover = SCM_BOOL_F;
        int                report_id;
  
        if(remover == SCM_BOOL_F) {
                remover = scm_c_eval_string("gnc:report-remove-by-id");
        }
  
        if(node && 
           !safe_strcmp (node->type, URL_TYPE_REPORT) && 
           !strncmp("id=", node->location, 3)) {
                sscanf(node->location+3, "%d", &report_id);
                /*    printf("unreffing report %d and children\n", report_id);
                      scm_call_1(remover, scm_int2num(report_id)); */
        }
        else {
                return;
        }
}

/* We got a draw event.  See if we need to reload the report */
static void
gnc_plugin_page_report_draw_cb(GtkWidget *unused, GdkRectangle *unused1, gpointer data)
{
        GncPluginPageReportPrivate *win = data;

        DEBUG( "drawin'" );
        ENTER( "report_draw" );
        if (!win->need_reload)
        {
                LEAVE( "no reload needed" );
                return;
        }

        win->need_reload = FALSE;
        gnc_window_set_progressbar_window( GNC_WINDOW(gnc_ui_get_toplevel()) );
        gnc_html_reload(win->html);
        gnc_window_set_progressbar_window( NULL );
        LEAVE( "reload forced" );
}

// @param data is actually GncPluginPageReportPrivate
static void
gnc_plugin_page_report_refresh (gpointer data)
{
        // FIXME?
        DEBUG( "report-refresh called" );
        return;
}

static void
gnc_plugin_page_report_set_fwd_button(GncPluginPageReportPrivate * win, int enabled)
{
#if 0
        GnomeApp    * app = win->mc->app;
        GnomeUIInfo * info;

        /* the code below is broken, so just return */
        return;

        if(app) {
                info = gnome_mdi_get_child_menu_info(app);
                if(info) gtk_widget_set_sensitive(info[1].widget, enabled);
        }
#endif /* 0 -- broken */
}

static void
gnc_plugin_page_report_set_back_button(GncPluginPageReportPrivate * win, int enabled)
{
#if 0
        GnomeApp    * app = win->mc->app;
        GnomeUIInfo * info;

        /* the code below is broken, so just return */
        return;

        if(app) {
                info = gnome_mdi_get_child_menu_info(app);
                if(info) gtk_widget_set_sensitive(info[0].widget, enabled);
        }
#endif /* 0 */
}

static
void
gnc_plugin_page_report_destroy_widget( GncPluginPage *plugin_page )
{
        // FIXME: cleanup resources.
}

/********************************************************************
 * gnc_report_window_destroy 
 * free and destroy a window 
 ********************************************************************/
void
gnc_plugin_page_report_destroy(GncPluginPageReportPrivate * win)
{
        SCM  get_editor = scm_c_eval_string("gnc:report-editor-widget");
        SCM  set_editor = scm_c_eval_string("gnc:report-set-editor-widget!");
        SCM  edited, editor; 

        gnc_unregister_gui_component_by_data (WINDOW_REPORT_CM_CLASS, win);

        /* close any open editors */
        for(edited = scm_list_copy(win->edited_reports); !SCM_NULLP(edited); 
            edited = SCM_CDR(edited)) {
                editor = scm_call_1(get_editor, SCM_CAR(edited));
                scm_call_2(set_editor, SCM_CAR(edited), SCM_BOOL_F);
                if(editor != SCM_BOOL_F) {
                        gtk_widget_destroy(GTK_WIDGET(gw_wcp_get_ptr(editor)));
                }
        }

        if(win->initial_odb) {
                gnc_option_db_unregister_change_callback_id(win->initial_odb, 
                                                            win->name_change_cb_id);
    
                gnc_option_db_destroy(win->initial_odb);
                win->initial_odb = NULL;
        }

        gnc_html_destroy(win->html);
  
        win->container     = NULL;
        win->html          = NULL;
  
        scm_unprotect_object(win->cur_report);
        scm_unprotect_object(win->edited_reports);
  
        g_free(win);
}

static
void
gnc_plugin_page_report_merge_actions( GncPluginPage *plugin_page,
                                      EggMenuMerge *ui_merge )
{
        GncPluginPageReport *page;
        GncPluginPageReportPrivate *priv;

        page = GNC_PLUGIN_PAGE_REPORT(plugin_page);
        priv = (GncPluginPageReportPrivate*)page->priv;

        DEBUG( "merge actions" );
        priv->ui_merge = ui_merge;
        priv->merge_id
                = gnc_menu_merge_add_actions( priv->ui_merge,
                                              priv->action_group,
                                              "gnc-plugin-page-report-ui.xml" );
        DEBUG( "done merging" );
}

static
void
gnc_plugin_page_report_unmerge_actions( GncPluginPage *plugin_page,
                                        EggMenuMerge *ui_merge )
{
        GncPluginPageReport *rep = GNC_PLUGIN_PAGE_REPORT( plugin_page );
        GncPluginPageReportPrivate *priv = (GncPluginPageReportPrivate*)rep->priv;

        DEBUG( "unmerge actions" );
	egg_menu_merge_remove_ui( ui_merge, priv->merge_id );
	egg_menu_merge_remove_action_group( ui_merge, priv->action_group );

	priv->ui_merge = NULL;
        DEBUG( "done unmerging" );
}

static EggActionEntry report_actions[] =
{
        { "ReportBackAction", N_("Back"), GTK_STOCK_GO_BACK, NULL, NULL,
          G_CALLBACK(gnc_plugin_page_report_back_cb) },
        { "ReportForwAction", N_("Forward"), GTK_STOCK_GO_FORWARD, NULL, NULL,
          G_CALLBACK(gnc_plugin_page_report_forw_cb) },
        { "ReportReloadAction", N_("Reload"), GTK_STOCK_REFRESH, NULL, NULL,
          G_CALLBACK(gnc_plugin_page_report_reload_cb) },
        { "ReportStopAction", N_("Stop"), GTK_STOCK_STOP, NULL, NULL,
          G_CALLBACK(gnc_plugin_page_report_stop_cb) },

        { "ReportExportAction", N_("Export"), GTK_STOCK_SAVE, NULL, NULL,
          G_CALLBACK(gnc_plugin_page_report_export_cb) },
        { "ReportOptionsAction", N_("Options"), GTK_STOCK_PROPERTIES, NULL, NULL,
          G_CALLBACK(gnc_plugin_page_report_options_cb) },
        { "ReportPrintAction", N_("Print"), GTK_STOCK_PRINT, NULL, NULL,
          G_CALLBACK(gnc_plugin_page_report_print_cb) },
};
static guint num_report_actions = G_N_ELEMENTS( report_actions );

static void
gnc_plugin_page_report_init ( GncPluginPageReport *plugin_page )
{
	EggActionGroup *action_group;
	GncPluginPageReportPrivate *priv;
	GncPluginPage *parent;
        GString *tmpStr;
        gint reportId;

	priv = plugin_page->priv = g_new0( GncPluginPageReportPrivate, 1 );

        reportId = -42;
        g_object_get( plugin_page, "report_id", &reportId, NULL );
        DEBUG( "property reportId=%d", reportId );

        gnc_plugin_page_report_setup( GNC_PLUGIN_PAGE(plugin_page) );

	/* Init parent declared variables */
	parent = GNC_PLUGIN_PAGE(plugin_page);
        tmpStr = g_string_sized_new( 32 );
        g_string_sprintf( tmpStr, "%s: %s", _("Report"),
                          gnc_report_name( priv->initial_report ) );
	parent->title       = g_strdup(tmpStr->str);
        parent->tab_name = g_strdup( tmpStr->str );
	parent->uri         = g_strdup("default:");

	/* change me when the system supports multiple books */
	gnc_plugin_page_add_book(parent, gnc_get_current_book());

	/* Create menu and toolbar information. */

        /* Note that we're not actually doing the merge, here ... just setup
         * the UI objects.  See gnc_plugin_page_report_[un]merge_actions(...) */

        // Report actions [1.<historical> order]
        // * fwd, back, reload, stop, 
        // * export
        // * options
        // * print

	action_group = egg_action_group_new ("GncPluginPageReportActions");
	priv->action_group = action_group;
	egg_action_group_add_actions( action_group,
				      report_actions,
				      num_report_actions,
				      plugin_page );

	active_pages = g_list_append (active_pages, plugin_page);
}

GncPluginPage*
gnc_plugin_page_report_new( int reportId )
{
	GncPluginPageReport *plugin_page;

        DEBUG( "report id = %d", reportId );
	plugin_page = g_object_new( GNC_TYPE_PLUGIN_PAGE_REPORT,
                                    "report_id", reportId, NULL );
        DEBUG( "plugin_page: %p", plugin_page );
        DEBUG( "set %d on page %p", reportId, plugin_page );
	return GNC_PLUGIN_PAGE( plugin_page );
}

void
gnc_plugin_page_report_remove_edited_report(GncPluginPageReportPrivate * win, SCM report)
{ 
        SCM new_edited = scm_delete(win->edited_reports, report);
        scm_unprotect_object(win->edited_reports);
        win->edited_reports = new_edited;
        scm_protect_object(win->edited_reports);
}

void
gnc_plugin_page_report_add_edited_report(GncPluginPageReportPrivate * win, SCM report)
{
        SCM new_edited = scm_cons(report, win->edited_reports);
        scm_unprotect_object(win->edited_reports);
        win->edited_reports = new_edited;
        scm_protect_object(win->edited_reports);
}

void
gnc_plugin_page_report_raise_editor(SCM report)
{
        SCM get_editor = scm_c_eval_string("gnc:report-editor-widget");
        SCM editor = scm_call_1(get_editor, report);
        gtk_window_present(gw_wcp_get_ptr(editor));
}

static void
close_handler (gpointer user_data)
{
        GncPluginPageReportPrivate *win = user_data;  
        DEBUG("in close handler\n");
        gnc_plugin_page_report_destroy (win);
}

// ------------------------------------------------------------
// GTK ACTION CALLBACKS

static
void
gnc_plugin_page_report_back_cb( EggAction *action, GncPluginPageReport *report )
{
        DEBUG( "back" );
}

static
void
gnc_plugin_page_report_forw_cb( EggAction *action, GncPluginPageReport *report )
{
        DEBUG( "forw" );
}

static
void
gnc_plugin_page_report_reload_cb( EggAction *action, GncPluginPageReport *report )
{
        DEBUG( "reload" );
}

static
void
gnc_plugin_page_report_stop_cb( EggAction *action, GncPluginPageReport *report )
{
        DEBUG( "stop" );
}

static
void
gnc_plugin_page_report_export_cb( EggAction *action, GncPluginPageReport *report )
{
        DEBUG( "export" );
}

static
void
gnc_plugin_page_report_options_cb( EggAction *action, GncPluginPageReport *report )
{
        DEBUG( "options" );
}

static
void
gnc_plugin_page_report_print_cb( EggAction *action, GncPluginPageReport *report )
{
        DEBUG( "print" );
}
