/********************************************************************\
 * dialog-database-connection.c -- dialog for opening a connection  *
 *                        to a libgda database, either predefined   *
 *                        in ~/.libgda/config or explicit using     *
 *                        provider and database.                    *
 *                                                                  *
 * Copyright (C) 2007-8 Phil Longstaff (plongstaff@rogers.com)      *
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
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
\********************************************************************/

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>

#include <libgda/libgda.h>

#include "gnc-ui.h"
#include "dialog-utils.h"
#include "dialog-database-connection.h"
#include "gnc-file.h"
#include "gnc-session.h"

void gnc_database_connection_response_cb( GtkDialog *, gint, GtkDialog * );

struct DatabaseConnectionWindow
{
  /* Parts of the dialog */
  GtkWidget* dialog;
  GtkWidget* rb_predefined;
  GtkWidget* rb_general;
  GtkWidget* cb_predefined;
  GtkWidget* tf_general;

  /* The final settings */
};

static void
save_to_db( const gchar* url )
{
	gnc_file_do_save_as( url );
}

static void
discard_and_open_db( const gchar* url )
{
  	QofBook* current_book;

    current_book = qof_session_get_book( gnc_get_current_session() );
	qof_book_mark_saved( current_book );
	gnc_file_open_file( url );
}

void
gnc_database_connection_response_cb(GtkDialog *dialog, gint response, GtkDialog *unused)
{
    struct DatabaseConnectionWindow* dcw;
	gchar* url;

    g_return_if_fail( dialog != NULL );

    dcw = g_object_get_data( G_OBJECT(dialog), "DatabaseConnectionWindow" );
    g_return_if_fail(dcw);

    switch( response ) {
    case GTK_RESPONSE_HELP:
        gnc_gnome_help( HF_HELP, HL_GLOBPREFS );
        break;
  
    case GTK_RESPONSE_OK:
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(dcw->rb_predefined) ) ) {
			/* Selection from predefined list */
			url = g_strdup_printf( "gda://@%s", gtk_combo_box_get_active_text( GTK_COMBO_BOX(dcw->cb_predefined) ) );

		} else {
			/* Selection using entered info */
			url = g_strdup_printf( "gdk://%s", gtk_entry_get_text( GTK_ENTRY(dcw->tf_general) ) );
		}
		save_to_db( url );

      /* FALLTHROUGH */ 
    default:
        gtk_widget_destroy( GTK_WIDGET(dialog) );
        break;
    }
}

void gnc_ui_database_connection( void )
{
    struct DatabaseConnectionWindow *dcw;
    GladeXML* xml;
    GtkWidget* box;
	GList* ds_list;
	GList* ds_node;
	int n_rows;

    dcw = g_new0(struct DatabaseConnectionWindow, 1);
    g_return_if_fail(dcw);

    /* Open the dialog */
    xml = gnc_glade_xml_new( "dialog-database-connection.glade", "Database Connection" );
    dcw->dialog = glade_xml_get_widget( xml, "Database Connection" );

    /* Predefined */
    dcw->rb_predefined = glade_xml_get_widget( xml, "rb_predefined" );
	box = glade_xml_get_widget( xml, "predefined_connection_box" );
//    dcw->cb_predefined = glade_xml_get_widget( xml, "cb_predefined_db_connections" );
	dcw->cb_predefined = gtk_combo_box_new_text();
	ds_list = gda_config_get_data_source_list();
	n_rows = 0;
	for( ds_node = ds_list; ds_node != NULL; ds_node = ds_node->next ) {
		GdaDataSourceInfo* ds_info = ds_node->data;
		gtk_combo_box_append_text( GTK_COMBO_BOX(dcw->cb_predefined), g_strdup(ds_info->name) );
		n_rows++;
	}
	gda_config_free_data_source_list( ds_list );
	if( n_rows != 0 ) {
		gtk_combo_box_set_active( GTK_COMBO_BOX(dcw->cb_predefined), 0 );
	} else {
		gtk_widget_set_sensitive( dcw->rb_predefined, FALSE );
	}
	gtk_box_pack_start( GTK_BOX(box), dcw->cb_predefined, TRUE, TRUE, 0 );

    /* General */
    dcw->rb_general = glade_xml_get_widget( xml, "rb_general" );
    dcw->tf_general = glade_xml_get_widget( xml, "tf_general_db_info" );

    /* Autoconnect signals */
    glade_xml_signal_autoconnect_full( xml, gnc_glade_autoconnect_full_func,
				    					dcw->dialog );

    /* Clean up the xml data structure when the dialog is destroyed */
    g_object_set_data_full( G_OBJECT(dcw->dialog), "dialog-database-connection.glade",
			 				xml, g_object_unref );
    g_object_set_data_full( G_OBJECT(dcw->dialog), "DatabaseConnectionWindow", dcw,
			 				g_free );

    /* Run the dialog */
    gtk_widget_show_all( dcw->dialog );
}

