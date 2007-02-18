/********************************************************************\
 * gnc-frequency.c -- GnuCash widget for frequency editing.         *
 * Copyright (C) 2001,2002,2007 Joshua Sled <jsled@asynchronous.org>*
 * Copyright (C) 2003 Linas Vepstas <linas@linas.org>               *
 * Copyright (C) 2006 David Hampton <hampton@employees.org>         *
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
 *                                                                  *
\********************************************************************/

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gtypes.h>
#include "glib-compat.h"
#include <math.h>
#include <time.h>

#include "FreqSpec.h"
#include "dialog-utils.h"
#include "gnc-component-manager.h"
#include "gnc-engine.h"
#include "gnc-frequency.h"
#include "gnc-ui-util.h"

#define LOG_MOD "gnc.gui.frequency"
static QofLogModule log_module = LOG_MOD;
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN LOG_MOD

/** Private Defs ********************/

typedef enum {
    GNCFREQ_CHANGED,
    LAST_SIGNAL
} GNCF_Signals;

static guint gnc_frequency_signals[LAST_SIGNAL] = { 0 };

/** Private Prototypes ********************/

static void gnc_frequency_class_init( GncFrequencyClass *klass );

static void freq_combo_changed( GtkComboBox *b, gpointer d );
static void start_date_changed( GNCDateEdit *gde, gpointer d );
static void spin_changed_helper( GtkAdjustment *adj, gpointer d );

static void weekly_days_changed( GtkButton *b, gpointer d );

static void monthly_sel_changed( GtkButton *b, gpointer d );
static void semimonthly_sel_changed( GtkButton *b, gpointer d );
static void yearly_sel_changed( GtkButton *b, gpointer d );
static void quarterly_sel_changed( GtkButton *b, gpointer d );
static void triyearly_sel_changed( GtkButton *b, gpointer d );
static void semiyearly_sel_changed( GtkButton *b, gpointer d );

static void year_range_sels_changed( GncFrequency *gf,
                                     int monthsInRange,
                                     GtkWidget *occurW,
                                     GtkWidget *dayOfMonthW );
static void year_range_menu_helper( GtkWidget *dayOptMenu,
                                    GtkWidget *occurOptMenu,
                                    gint monthsInRange,
                                    time_t date );

/** Static Inits ********************/

enum
{
    PAGE_NONE = 0,
    PAGE_ONCE,
    PAGE_DAILY,
    PAGE_WEEKLY,
    PAGE_SEMI_MONTHLY,
    PAGE_MONTHLY
};

static const struct pageDataTuple PAGES[] =
{
    { PAGE_NONE,         UIFREQ_NONE,         "None" },
    { PAGE_ONCE,         UIFREQ_ONCE,         "Once" },
    { PAGE_DAILY,        UIFREQ_DAILY,        "Daily" },
    { PAGE_WEEKLY,       UIFREQ_WEEKLY,       "Weekly" },
    { PAGE_SEMI_MONTHLY, UIFREQ_SEMI_MONTHLY, "Semi-Monthly" },
    { PAGE_MONTHLY,      UIFREQ_MONTHLY,      "Monthly" },
    { 0, 0, 0 }
};

static const char *CHECKBOX_NAMES[] = {
    "wd_check_sun",
    "wd_check_mon",
    "wd_check_tue",
    "wd_check_wed",
    "wd_check_thu",
    "wd_check_fri",
    "wd_check_sat",
    NULL
};

/** Implementations ********************/

GType
gnc_frequency_get_type()
{
    static GType gncfreq_type = 0;
    if (gncfreq_type == 0) {
        static GTypeInfo gncfreq_info = {
            sizeof(GncFrequencyClass),
            NULL,
            NULL,
            (GClassInitFunc)gnc_frequency_class_init,
            NULL,
            NULL,
            sizeof(GncFrequency),
            0,
            (GInstanceInitFunc)gnc_frequency_init
        };
                
        gncfreq_type = g_type_register_static (GTK_TYPE_VBOX,
                                               "GncFrequency",
                                               &gncfreq_info, 0);
    }

    return gncfreq_type;
}

static void
gnc_frequency_class_init( GncFrequencyClass *klass )
{
    GObjectClass *object_class;
        
    object_class = G_OBJECT_CLASS (klass);

    gnc_frequency_signals[GNCFREQ_CHANGED] =
        g_signal_new ("changed",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (GncFrequencyClass, changed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);
}

void
gnc_frequency_init(GncFrequency *gf)
{
    int    i;
    GtkVBox  *vb;
    GtkWidget   *o;
    GtkAdjustment  *adj;

    static const struct comboBoxTuple {
        char *name;
        void (*fn)();
    } comboBoxes[] = {
        { "freq_combobox",      freq_combo_changed },
        { "semimonthly_first",  semimonthly_sel_changed },
        { "semimonthly_second", semimonthly_sel_changed },
        { "monthly_day",        monthly_sel_changed },
        { NULL,                 NULL }
    };

    static const struct spinvalTuple {
        char *name;
        void (*fn)();
    } spinVals[] = {
        { "daily_spin",       spin_changed_helper },
        { "weekly_spin",      spin_changed_helper },
        { "semimonthly_spin", spin_changed_helper },
        { "monthly_spin",     spin_changed_helper },
        { NULL,               NULL }
    };

    gf->gxml = gnc_glade_xml_new("sched-xact.glade", "gncfreq_vbox");
    o = glade_xml_get_widget(gf->gxml, "gncfreq_nb");
    gf->nb = GTK_NOTEBOOK(o);
    o = glade_xml_get_widget(gf->gxml, "freq_combobox");
    gf->freqComboBox = GTK_COMBO_BOX(o);
    gf->startDate = GNC_DATE_EDIT(gnc_date_edit_new(time(NULL), FALSE, FALSE));
    /* Add the new widget to the table. */
    {
        GtkWidget *table = glade_xml_get_widget(gf->gxml, "gncfreq_table");
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(gf->startDate),
                         1, 2, 1, 2, (GTK_EXPAND | GTK_FILL), 0,
                         0, 0);
    }
    vb = GTK_VBOX(glade_xml_get_widget(gf->gxml, "gncfreq_vbox"));
    gf->vb = vb;
    gtk_container_add(GTK_CONTAINER(&gf->widget), GTK_WIDGET(gf->vb));

    /* initialize the combo boxes */
    for (i=0; comboBoxes[i].name != NULL; i++)
    {
        o = glade_xml_get_widget(gf->gxml, comboBoxes[i].name);
        gtk_combo_box_set_active(GTK_COMBO_BOX(o), 0);
        if (comboBoxes[i].fn != NULL)
        {
            g_signal_connect(o, "changed", G_CALLBACK(comboBoxes[i].fn), gf);
        }
    }

    /* initialize the spin buttons */
    for (i=0; spinVals[i].name != NULL; i++)
    {
        if (spinVals[i].fn != NULL) 
        {
            o = glade_xml_get_widget(gf->gxml, spinVals[i].name);
            adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(o));
            g_signal_connect(adj, "value_changed", G_CALLBACK(spinVals[i].fn), gf);
        }
    }

    /* initialize the weekly::day-of-week checkbox-selection hooks */
    for (i=0; i < 7; i++)
    {
        o = glade_xml_get_widget(gf->gxml, CHECKBOX_NAMES[i]);
        g_signal_connect(o, "clicked",
                          G_CALLBACK(weekly_days_changed), gf);
    }

    gtk_widget_show_all(GTK_WIDGET(&gf->widget));

    /* respond to start date changes */
    g_signal_connect(gf->startDate, "date_changed", G_CALLBACK(start_date_changed), gf);
}

static void
do_frequency_setup(GncFrequency *gf, FreqSpec *fs, time_t *secs)
{
        UIFreqType uift;
        int i, page;

        /* Set the start date, but only if present. */
        if (secs)
        {
                gnc_date_edit_set_time( gf->startDate, *secs);
                if (NULL == fs) 
                {
                        g_signal_emit_by_name( gf, "changed" );
                }
        }
 
        /* If freq spec not present, then we are done; 
         * don't change any other settings.  */
        if (NULL == fs) return;

#if 0        
        uift = xaccFreqSpecGetUIType( fs );
        page = -1;
        for ( i=0; i < UIFREQ_NUM_UI_FREQSPECS+1; i++ ) 
        {
                if ( PAGES[i].uiFTVal == uift ) 
                {
                         page = PAGES[i].idx;
                         break;
                }
        }
        g_assert( page != -1 );

        gtk_notebook_set_current_page( gf->nb, page );
        gtk_combo_box_set_active( gf->freqComboBox, page );
#endif // 0

        switch ( uift ) 
        {
        case UIFREQ_NONE:
                break;
        case UIFREQ_ONCE:
        {
                GDate theDate;
                struct tm stm;
                /* set the date */
                if ( xaccFreqSpecGetOnce( fs, &theDate ) < 0 ) {
                     g_warning("Inappropriate FreqSpec type [gnc-frequency: %d vs. FreqSpec: %d]",
                               uift, xaccFreqSpecGetUIType( fs ));
                     return;
                }
                g_date_to_struct_tm( &theDate, &stm );
                gnc_date_edit_set_time( gf->startDate, mktime(&stm) );

                gtk_notebook_set_current_page( gf->nb, PAGE_ONCE);
                gtk_combo_box_set_active( gf->freqComboBox, PAGE_ONCE);
        }
        break;
        case UIFREQ_DAILY:
        { 
                GtkWidget *o;
                int dailyMult = -1;
                if ( xaccFreqSpecGetDaily( fs, &dailyMult ) < 0 ) {
                     g_warning("Inappropriate FreqSpec type [gnc-frequency: %d vs. FreqSpec: %d]",
                               uift, xaccFreqSpecGetUIType( fs ) );
                     return;
                }
                o = glade_xml_get_widget( gf->gxml, "daily_spin" );
                gtk_spin_button_set_value( GTK_SPIN_BUTTON( o ), dailyMult );

                gtk_notebook_set_current_page( gf->nb, PAGE_DAILY);
                gtk_combo_box_set_active( gf->freqComboBox, PAGE_DAILY);
        }
        break;
        case UIFREQ_DAILY_MF:
        case UIFREQ_WEEKLY:
        case UIFREQ_BI_WEEKLY:
        {
                const char * str;
                int weeklyMult = -1;
                int dayOfWeek;
                GtkWidget *o;
                FreqSpec *subFS;
                GList *list;

                for ( list = xaccFreqSpecCompositeGet( fs );
                      list; list = g_list_next(list) ) {
                        subFS = (FreqSpec*)(list->data);
                        if ( weeklyMult == -1 ) {
                                if ( subFS == NULL ) {
                                     g_critical("subFS is null");
                                     return;
                                }
                                if ( xaccFreqSpecGetWeekly( subFS,
                                                            &weeklyMult,
                                                            &dayOfWeek ) < 0 ) {
                                     g_warning("Inappropriate FreqSpec type [gnc-frequency: %d, FreqSpec: %d]",
                                               uift, xaccFreqSpecGetUIType( fs ) );
                                     return;
                                }
                        } else {
                                int otherWeeklyMult = -1;

                                if ( subFS == NULL ) {
                                     g_critical("subFS is null");
                                     return;
                                }
                                if ( xaccFreqSpecGetWeekly( subFS,
                                                            &otherWeeklyMult,
                                                            &dayOfWeek ) < 0 ) {
                                     g_warning("Inappropriate FreqSpec type [gnc-frequency: %d, FreqSpec: %d]",
                                               uift, xaccFreqSpecGetUIType( fs ) );
                                     return;
                                }
                                if ( weeklyMult != otherWeeklyMult ) {
                                     g_warning("Inconsistent weekly FreqSpec multipliers seen [first: %d vs. other: %d]",
                                               weeklyMult, otherWeeklyMult );
                                     return;
                                }
                        }
                        if ( dayOfWeek > 6 ) {
                             g_warning( "dayOfWeek > 6 [%d]", dayOfWeek );
                             return;
                        }
                        str = CHECKBOX_NAMES[dayOfWeek];
                        o = glade_xml_get_widget( gf->gxml, str );
                        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(o), TRUE );
                }
                o = glade_xml_get_widget( gf->gxml, "weekly_spin" );
                if (uift == UIFREQ_BI_WEEKLY)
                    weeklyMult = 2;
                gtk_spin_button_set_value( GTK_SPIN_BUTTON(o), weeklyMult );

                gtk_notebook_set_current_page( gf->nb, PAGE_WEEKLY);
                gtk_combo_box_set_active( gf->freqComboBox, PAGE_WEEKLY);
        }
        break;
        case UIFREQ_SEMI_MONTHLY:
        {
                GtkWidget *o;
                GList *list;
                int monthlyMult;
                FreqSpec *subFS;
                int firstDayOfMonth, secondDayOfMonth, monthOffset;

                list = xaccFreqSpecCompositeGet( fs );
                /*  mult */
                o = glade_xml_get_widget( gf->gxml, "semimonthly_spin" );
                subFS = (FreqSpec*)(g_list_nth( list, 0 )->data);
                if ( xaccFreqSpecGetMonthly( subFS, &monthlyMult,
                                             &firstDayOfMonth, &monthOffset ) < 0 ) {
                     g_warning("Inappropriate FreqSpec type [gnc-frequency: %d, FreqSpec: %d]",
                               uift, xaccFreqSpecGetUIType( fs ) );
                     return;
                }
                gtk_spin_button_set_value( GTK_SPIN_BUTTON(o), monthlyMult );
                /*  first date */
                o = glade_xml_get_widget( gf->gxml, "semimonthly_first" );
                gtk_combo_box_set_active( GTK_COMBO_BOX(o), firstDayOfMonth-1 );
                /*  second date */
                subFS = (FreqSpec*)(g_list_nth( list, 1 )->data);
                o = glade_xml_get_widget( gf->gxml, "semimonthly_second" );
                if ( xaccFreqSpecGetMonthly( subFS, &monthlyMult,
                                             &secondDayOfMonth, &monthOffset ) < 0 ) {
                     g_warning( "Inappropriate FreqSpec type" );
                     return;
                }
                gtk_combo_box_set_active( GTK_COMBO_BOX(o), secondDayOfMonth-1 );

                gtk_notebook_set_current_page( gf->nb, PAGE_SEMI_MONTHLY);
                gtk_combo_box_set_active( gf->freqComboBox, PAGE_SEMI_MONTHLY);
        }
        break;
        case UIFREQ_MONTHLY:
        case UIFREQ_QUARTERLY:
        case UIFREQ_TRI_ANUALLY:
        case UIFREQ_SEMI_YEARLY:
        case UIFREQ_YEARLY:
        {
                GtkWidget *o;
                int monthlyMult, dayOfMonth, monthOffset;

                if ( xaccFreqSpecGetMonthly( fs, &monthlyMult,
                                             &dayOfMonth, &monthOffset ) < 0 ) {
                     g_warning("Inappropriate FreqSpec type [gnc-frequency: %d, FreqSpec: %d]",
                               uift, xaccFreqSpecGetUIType( fs ) );
                     return;
                }
                o = glade_xml_get_widget( gf->gxml, "monthly_spin" );
                gtk_spin_button_set_value( GTK_SPIN_BUTTON(o), monthlyMult );
                o = glade_xml_get_widget( gf->gxml, "monthly_day" );
                gtk_combo_box_set_active( GTK_COMBO_BOX(o), dayOfMonth-1 );
                /*  set the day-of-month */

                gtk_notebook_set_current_page( gf->nb, PAGE_MONTHLY);
                gtk_combo_box_set_active( gf->freqComboBox, PAGE_MONTHLY);
        }
        break;
        default:
             g_critical( "unknown ui freq type %d", uift);
             break;
        }

        g_signal_emit_by_name( gf, "changed" );
}

static void
gnc_frequency_setup_default( GncFrequency *gf, FreqSpec *fs, GDate *date )
{
    time_t secs;

    /* If no freq-spec, then set the widget to blank */
    if (NULL == fs)
    {
        UIFreqType uift = UIFREQ_NONE;
        int i, page;

        page = -1;
        for ( i=0; i < UIFREQ_NUM_UI_FREQSPECS+1; i++ ) 
        {
            if ( PAGES[i].uiFTVal == uift ) 
            {
                page = PAGES[i].idx;
                break;
            }
        }
        g_assert( page != -1 );
   
        gtk_notebook_set_current_page( gf->nb, page );
        gtk_combo_box_set_active( gf->freqComboBox, page );
    }

    /* Setup the start date */
    if (!date ||  ! g_date_valid(date) ) 
    {
        secs = time(NULL);
    } 
    else 
    {
        struct tm stm;
        g_date_to_struct_tm( date, &stm);
        secs = mktime (&stm);
    }
 
    do_frequency_setup(gf, fs, &secs);
}

void
gnc_frequency_setup( GncFrequency *gf, FreqSpec *fs, GDate *date )
{
    time_t secs;

    if (!gf) return;

    /* Setup the start date */
    if (!date || !g_date_valid(date)) 
    {
        do_frequency_setup(gf, fs, NULL);
    } 
    else 
    {
        struct tm stm;
        g_date_to_struct_tm( date, &stm);
        secs = mktime (&stm);
        do_frequency_setup(gf, fs, &secs);
    }
}

GtkWidget *
gnc_frequency_new( FreqSpec *fs, GDate *date )
{
        GncFrequency  *toRet;
        toRet = g_object_new( gnc_frequency_get_type(), NULL );
        gnc_frequency_setup_default( toRet, fs, date );
        return GTK_WIDGET(toRet);
}

void
gnc_frequency_save_state( GncFrequency *gf, FreqSpec *fs, GDate *outDate )
{
        gint page;
        gint day;
        GtkWidget *o;
        UIFreqType uift;
        FreqSpec *tmpFS;
        gint tmpInt;
        int i;
        GDate gd;
        time_t start_tt;

        start_tt = gnc_date_edit_get_date( gf->startDate );
        if ( NULL != outDate ) 
        {
                g_date_set_time_t( outDate, start_tt );
        }

        if (NULL == fs) return;

        /* Get the current tab */
        page = gtk_notebook_get_current_page( gf->nb );

        /* We're going to be creating/destroying FreqSpecs, 
         * which will cause GUI refreshes. :( */
        gnc_suspend_gui_refresh();

        g_date_clear (&gd, 1);
        g_date_set_time_t( &gd, start_tt );

        /*uift = xaccFreqSpecGetUIType( fs );*/
        uift = PAGES[page].uiFTVal;

        /* Based on value, parse widget values into FreqSpec */
        switch ( uift ) {
        case UIFREQ_NONE:
                xaccFreqSpecSetNone(fs);
                xaccFreqSpecSetUIType(fs, UIFREQ_NONE);
                break;
        case UIFREQ_ONCE:
                xaccFreqSpecSetOnceDate(fs, &gd);
                xaccFreqSpecSetUIType( fs, uift );
                break;
        case UIFREQ_DAILY:
                o = glade_xml_get_widget( gf->gxml, "daily_spin" );
                /* FIXME: initial date should be set correctly. */
                {
                        gint foo;

                        foo = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(o) );
                        xaccFreqSpecSetDaily( fs, &gd, foo );
                        xaccFreqSpecSetUIType( fs, uift );
                }
                break;
        case UIFREQ_WEEKLY:
        {
                struct tm stm;
                GDate gd2;
                xaccFreqSpecSetComposite( fs );
                xaccFreqSpecSetUIType( fs, uift );
                o = glade_xml_get_widget( gf->gxml, "weekly_spin" );
                tmpInt = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(o) );

                /*  assume we have a good calendar that allows week selection. */
                /*  for-now hack: normalize to Sunday. */
                g_date_to_struct_tm( &gd, &stm);
                stm.tm_mday -= stm.tm_wday % 7;
                g_date_set_time_t( &gd, mktime(&stm) );

                /*  now, go through the check boxes and add composites based on that date. */
                for ( i=0; CHECKBOX_NAMES[i]!=NULL; i++ ) {
                        o = glade_xml_get_widget( gf->gxml, CHECKBOX_NAMES[i] );
                        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(o) ) ) {

                                tmpFS = xaccFreqSpecMalloc
                                        (gnc_get_current_book ());
                                xaccFreqSpecSetUIType( tmpFS, uift );

                                g_date_clear (&gd2, 1);
                                gd2 = gd;
                                /*  Add 'i' days off of Sunday... */
                                g_date_add_days( &gd2, i );
                                xaccFreqSpecSetWeekly( tmpFS, &gd2, tmpInt );
                                xaccFreqSpecCompositeAdd( fs, tmpFS );
                        }
                }
                break;
        }
        case UIFREQ_SEMI_MONTHLY:
        {
                struct tm stm;
                /* FIXME: this is b0rken date calculation for mday>28 */
                xaccFreqSpecSetComposite( fs );
                xaccFreqSpecSetUIType( fs, uift );
    
                o = glade_xml_get_widget( gf->gxml, "semimonthly_spin" );
                tmpInt = gtk_spin_button_get_value_as_int( GTK_SPIN_BUTTON(o) );

                o = glade_xml_get_widget( gf->gxml, "semimonthly_first" );
                day = gtk_combo_box_get_active( GTK_COMBO_BOX(o) )+1;
                if (day > 30)
                {
                    g_critical("freq spec doesn't support last-day-of-month");
                    break;
                }
                tmpFS = xaccFreqSpecMalloc(gnc_get_current_book ());
                g_date_to_struct_tm( &gd, &stm);
                if ( day >= stm.tm_mday ) {
                        /* next month */
                        stm.tm_mon += 1;
                }
                /* else, this month */
                stm.tm_mday = day;
                g_date_set_time_t( &gd, mktime( &stm) );
                xaccFreqSpecSetMonthly( tmpFS, &gd, tmpInt );
                xaccFreqSpecCompositeAdd( fs, tmpFS );

                o = glade_xml_get_widget( gf->gxml, "semimonthly_second" );
                day = gtk_combo_box_get_active( GTK_COMBO_BOX(o) )+1;
                if (day > 30)
                {
                    g_critical("freq spec doesn't support last-day-of-month");
                    break;
                }
                tmpFS = xaccFreqSpecMalloc(gnc_get_current_book ());
                start_tt = gnc_date_edit_get_date( gf->startDate );
                g_date_set_time_t( &gd, start_tt );
                g_date_to_struct_tm( &gd, &stm);
                if ( day >= stm.tm_mday ) {
                        /* next month */
                        stm.tm_mon += 1;
                }
                /* else, this month */
                stm.tm_mday = day;
                g_date_set_time_t( &gd, mktime( &stm ) );
                xaccFreqSpecSetMonthly( tmpFS, &gd, tmpInt );
                xaccFreqSpecCompositeAdd( fs, tmpFS );

                break;
        }
        case UIFREQ_MONTHLY:
        {
                o = glade_xml_get_widget( gf->gxml, "monthly_spin" );
                tmpInt = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(o));

                o = glade_xml_get_widget( gf->gxml, "monthly_day" );
                day = gtk_combo_box_get_active( GTK_COMBO_BOX(o) ) + 1;
                if (day > 30)
                {
                    g_critical("freq spec doesn't support last-day-of-month");
                }
                g_date_set_time_t(&gd, time(NULL));
                g_date_set_month(&gd, 1);
                g_date_set_day(&gd, day);
                {
                     gchar buf[128];
                     g_date_strftime(buf, 127, "%c", &gd);
                     g_debug("monthly date [%s]\n", buf);
                }
                xaccFreqSpecSetMonthly( fs, &gd, tmpInt );
                xaccFreqSpecSetUIType( fs, uift );
                break;
        }
        default:
             g_critical("Unknown UIFreqType %d", uift);
             break;
        }
        gnc_resume_gui_refresh();
}

static void
spin_changed_helper( GtkAdjustment *adj, gpointer d )
{
        g_signal_emit_by_name( GNC_FREQUENCY(d), "changed" );
}

static void
weekly_days_changed( GtkButton *b, gpointer d )
{
        GncFrequency *gf;

        gf = GNC_FREQUENCY(d);
        g_signal_emit_by_name( gf, "changed" );
}

static void
monthly_sel_changed( GtkButton *b, gpointer d )
{
        GncFrequency  *gf;
        GtkWidget  *o;
        guint    dayOfMonth;
        struct tm  *tmptm;
        time_t    tmptt;
  
        gf = (GncFrequency*)d;

        o = glade_xml_get_widget( ((GncFrequency*)d)->gxml,
                                  "monthly_day" );
        dayOfMonth = gtk_combo_box_get_active( GTK_COMBO_BOX(o) ) + 1;

        tmptt = gnc_date_edit_get_date( gf->startDate );
        tmptm = localtime( &tmptt );
        while ( ! g_date_valid_dmy( dayOfMonth,
                                    tmptm->tm_mon + 1,
                                    tmptm->tm_year+1900 ) ) {
                dayOfMonth -= 1;
        }
        tmptm->tm_mday = dayOfMonth;
        tmptt = mktime( tmptm );
        gnc_date_edit_set_time( gf->startDate, tmptt );

        g_signal_emit_by_name( d, "changed" );
}

static void
semimonthly_sel_changed( GtkButton *b, gpointer d )
{
        GncFrequency  *gf;
        GtkWidget  *o;
        gint    tmpint;
        time_t    tmptt;
        struct tm  *tmptm;

        gf = (GncFrequency*)d;

        tmptt = gnc_date_edit_get_date( gf->startDate );
        tmptm = localtime( &tmptt );

        o = glade_xml_get_widget( gf->gxml, "semimonthly_first" );
        tmpint = gtk_combo_box_get_active( GTK_COMBO_BOX(o) )+1;
        o = glade_xml_get_widget( gf->gxml, "semimonthly_second" );
        if ( tmpint > gtk_combo_box_get_active( GTK_COMBO_BOX(o) )+1 ) {
                tmpint = gtk_combo_box_get_active( GTK_COMBO_BOX(o) )+1;
        }

        tmptm->tm_mday = tmpint;
        while ( ! g_date_valid_dmy( tmptm->tm_mday,
                                    tmptm->tm_mon+1,
                                    tmptm->tm_year+1900 ) ) {
                tmptm->tm_mday -= 1;
        }
        tmptt = mktime( tmptm );
        gnc_date_edit_set_time( gf->startDate, tmptt );

        g_signal_emit_by_name( gf, "changed" );
}

static inline guint32 minn( guint32 a, guint32 b )
{
        return a > b ? b : a;
}

static inline guint32 maxn( guint32 a, guint32 b )
{
        return a > b ? a : b;
}

static void
freq_combo_changed( GtkComboBox *b, gpointer d )
{
        GncFrequency *gf = (GncFrequency*)d;
        int optIdx;
        UIFreqType uift;
        time_t startDate, tmpDate;
        struct tm *tmpTm;
        GtkWidget *o;

        /* Set the new page. */
        optIdx = gtk_combo_box_get_active( GTK_COMBO_BOX(((GncFrequency*)d)->freqComboBox) );
        gtk_notebook_set_current_page( ((GncFrequency*)d)->nb, optIdx );

        /* setup initial values for new page, as possible. */
        uift = PAGES[optIdx].uiFTVal;
        startDate = gnc_date_edit_get_date( gf->startDate );
        tmpTm = localtime( &startDate );

        switch ( uift ) {
        case UIFREQ_SEMI_MONTHLY:
        {
                gint tmpDayOfMonth;
                /* first on the <startdate_dom>, then on the <startdate_dom+2w> */
                o = glade_xml_get_widget( gf->gxml, "semimonthly_first" );
                tmpDayOfMonth = tmpTm->tm_mday;
                tmpTm->tm_mday += 14;
                tmpDate = mktime( tmpTm );
                tmpTm = localtime( &tmpDate );
                gtk_combo_box_set_active( GTK_COMBO_BOX(o),
                                          minn( tmpTm->tm_mday, tmpDayOfMonth ) - 1 );
                o = glade_xml_get_widget( gf->gxml, "semimonthly_second" );
                gtk_combo_box_set_active( GTK_COMBO_BOX(o),
                                          maxn( tmpTm->tm_mday, tmpDayOfMonth ) - 1 );
        }
        break;
        case UIFREQ_MONTHLY:
                /* on the <startdate_dom> */
                o = glade_xml_get_widget( gf->gxml, "monthly_day" );
                gtk_combo_box_set_active( GTK_COMBO_BOX(o),
                                          tmpTm->tm_mday - 1 );
                break;
        default:
                /* nuttin can be done, for whatever reason. */
                break;
        }
        g_signal_emit_by_name( gf, "changed" );
}

static void
start_date_changed( GNCDateEdit *gde, gpointer d )
{
        GncFrequency  *gf;
        GtkWidget  *o;
        struct tm  *tmpTm;
        time_t    dateFromGDE;
        gint    page;
        UIFreqType  uift;
  
        gf = (GncFrequency*)d;

        dateFromGDE = gnc_date_edit_get_date( gde );

        page = gtk_notebook_get_current_page( gf->nb );
        uift = PAGES[page].uiFTVal;

        o = NULL;

        switch (uift) {
        case UIFREQ_ONCE:      /* FALLTHROUGH */
        case UIFREQ_DAILY:     /* FALLTHROUGH */
        case UIFREQ_WEEKLY:    /* FALLTHROUGH */
                break;

        case UIFREQ_SEMI_MONTHLY:
        {
                gint first_day;
                o = glade_xml_get_widget( gf->gxml, "semimonthly_first" );
                first_day = gtk_combo_box_get_active( GTK_COMBO_BOX(o) )+1;
                o = glade_xml_get_widget( gf->gxml, "semimonthly_second" );
                if ( first_day < gtk_combo_box_get_active(
                             GTK_COMBO_BOX(o) )+1 ) {
                        o = glade_xml_get_widget( gf->gxml,
                                                  "semimonthly_first" );
                }

                tmpTm = localtime( &dateFromGDE );
                gtk_combo_box_set_active( GTK_COMBO_BOX(o),
                                             tmpTm->tm_mday-1 );
        }
        break;
        case UIFREQ_MONTHLY:
                o = glade_xml_get_widget( gf->gxml, "monthly_day" );
                tmpTm = localtime( &dateFromGDE );
                gtk_combo_box_set_active( GTK_COMBO_BOX(o),
                                             (tmpTm->tm_mday-1) );
                break;
        default:
             g_critical("unknown uift value %d", uift);
             break;
        }
        g_signal_emit_by_name( gf, "changed" );
}

/* ================================================================= */
/* Relabel some of the labels */

void 
gnc_frequency_set_frequency_label_text(GncFrequency *gf, const gchar *txt)
{
    GtkLabel *lbl;
    if (!gf || !txt) return;
    lbl = GTK_LABEL (glade_xml_get_widget (gf->gxml, "freq label"));
    gtk_label_set_text (lbl, txt);
}

void 
gnc_frequency_set_date_label_text(GncFrequency *gf, const gchar *txt)
{
    GtkLabel *lbl;
    if (!gf || !txt) return;
    lbl = GTK_LABEL (glade_xml_get_widget (gf->gxml, "startdate label"));
    gtk_label_set_text (lbl, txt);
}


GtkWidget*
gnc_frequency_new_from_recurrence(GList *recurrences, GDate *start_date)
{
    GncFrequency *toRet;
    toRet = g_object_new(gnc_frequency_get_type(), NULL);
    gnc_frequency_setup_recurrence(toRet, recurrences, start_date);
    return GTK_WIDGET(toRet);
}

static gboolean
_test_for_semi_monthly(GList *recurrences)
{
    if (g_list_length(recurrences) != 2)
        return FALSE;

    // should be a "semi-monthly":
    {
        Recurrence *first = (Recurrence*)g_list_nth_data(recurrences, 0);
        Recurrence *second = (Recurrence*)g_list_nth_data(recurrences, 1);
        PeriodType first_period, second_period;
        first_period = recurrenceGetPeriodType(first);
        second_period = recurrenceGetPeriodType(second);
             
        if (!((first_period == PERIOD_MONTH
               || first_period == PERIOD_END_OF_MONTH
               || first_period == PERIOD_LAST_WEEKDAY)
              && (second_period == PERIOD_MONTH
                  || second_period == PERIOD_END_OF_MONTH
                  || second_period == PERIOD_LAST_WEEKDAY)))
        {
            /*g_error("unknown 2-recurrence composite with period_types first [%d] second [%d]",
              first_period, second_periodD);*/
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean
_test_for_weekly_multiple(GList *recurrences)
{
    GList *r_iter;
   
    for (r_iter = recurrences; r_iter != NULL; r_iter = r_iter->next)
    {
        Recurrence *r = (Recurrence*)r_iter->data;

        if (!(recurrenceGetPeriodType(r) == PERIOD_WEEK))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static void
_setup_weekly_recurrence(GncFrequency *gf, Recurrence *r)
{
    GDate recurrence_date;
    GDateWeekday day_of_week;
    guint multiplier = recurrenceGetMultiplier(r);
    const char *checkbox_widget_name;
    GtkWidget *weekday_checkbox;

    GtkWidget *multipler_spin = glade_xml_get_widget(gf->gxml, "weekly_spin");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(multipler_spin), multiplier);

    recurrence_date = recurrenceGetDate(r);
    day_of_week = g_date_get_weekday(&recurrence_date);
    g_assert(day_of_week >= G_DATE_MONDAY && day_of_week <= G_DATE_SUNDAY);
    // this `mod 7' is explicit knowledge of the values of (monday=1)-based
    // GDateWeekday, vs. our (sunday=0)-based checkbox names array.
    checkbox_widget_name = CHECKBOX_NAMES[day_of_week % 7];
    weekday_checkbox = glade_xml_get_widget(gf->gxml, checkbox_widget_name);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(weekday_checkbox), TRUE);
}

static int
_get_monthly_combobox_index(Recurrence *r)
{
    GDate recurrence_date = recurrenceGetDate(r);
    int day_of_month_index = g_date_get_day(&recurrence_date) - 1;
    if (recurrenceGetPeriodType(r) == PERIOD_LAST_WEEKDAY)
    {
        gint last_day_of_month_list_offset = 30;
        day_of_month_index
            = last_day_of_month_list_offset
            + g_date_get_weekday(&recurrence_date);
    }
    return day_of_month_index;
}

void
gnc_frequency_setup_recurrence(GncFrequency *gf, GList *recurrences, GDate *start_date)
{
     gboolean made_changes = FALSE;

     // setup start-date, if present
     if (start_date != NULL
         && g_date_valid(start_date))
     {
          gnc_date_edit_set_gdate(gf->startDate, start_date);
          made_changes = TRUE;
     }

     if (recurrences == NULL)
     {
          goto maybe_signal;
          // return...
     }

     if (g_list_length(recurrences) > 1)
     {
         if (_test_for_weekly_multiple(recurrences))
         {
             for (; recurrences != NULL; recurrences = recurrences->next)
             {
                 _setup_weekly_recurrence(gf, (Recurrence*)recurrences->data);
             }
             gtk_notebook_set_current_page(gf->nb, PAGE_WEEKLY);
             gtk_combo_box_set_active(gf->freqComboBox, PAGE_WEEKLY);
         }
         else if (_test_for_semi_monthly(recurrences))
         {
             Recurrence *first, *second;
             GtkWidget *multiplier_spin;
             GtkWidget *dom_combobox;

             first = (Recurrence*)g_list_nth_data(recurrences, 0);
             second = (Recurrence*)g_list_nth_data(recurrences, 1);

             multiplier_spin = glade_xml_get_widget(gf->gxml, "semimonthly_spin");
             gtk_spin_button_set_value(GTK_SPIN_BUTTON(multiplier_spin), recurrenceGetMultiplier(first));

             dom_combobox = glade_xml_get_widget(gf->gxml, "semimonthly_first");
             gtk_combo_box_set_active(GTK_COMBO_BOX(dom_combobox), _get_monthly_combobox_index(first));
             dom_combobox = glade_xml_get_widget(gf->gxml, "semimonthly_second");
             gtk_combo_box_set_active(GTK_COMBO_BOX(dom_combobox), _get_monthly_combobox_index(second));

             gtk_notebook_set_current_page(gf->nb, PAGE_SEMI_MONTHLY);
             gtk_combo_box_set_active(gf->freqComboBox, PAGE_SEMI_MONTHLY);
         }
         else
         {
             g_error("unknown composite recurrence with [%d] entries", g_list_length(recurrences));
         }
     }
     else
     {
         Recurrence *r = (Recurrence*)recurrences->data;
         g_debug("recurrence period [%d]", recurrenceGetPeriodType(r));
         switch (recurrenceGetPeriodType(r))
         {
         case PERIOD_ONCE: {
             GDate recurrence_date = recurrenceGetDate(r);
             if (g_date_compare(start_date, &recurrence_date) != 0)
             {
                 char start_date_str[128], recur_date_str[128];
                 g_date_strftime(start_date_str, 127, "%x", start_date);
                 g_date_strftime(recur_date_str, 127, "%x", &recurrence_date);
                 g_critical("start_date [%s] != recurrence_date [%s]", start_date_str, recur_date_str);
             }

             gtk_notebook_set_current_page(gf->nb, PAGE_ONCE);
             gtk_combo_box_set_active(gf->freqComboBox, PAGE_ONCE);
         } break;
         case PERIOD_DAY: {
             guint multiplier;
             GtkWidget *spin_button;
             multiplier = recurrenceGetMultiplier(r);
             spin_button = glade_xml_get_widget(gf->gxml, "daily_spin");
             gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button), multiplier);
             made_changes = TRUE;

             gtk_notebook_set_current_page(gf->nb, PAGE_DAILY);
             gtk_combo_box_set_active(gf->freqComboBox, PAGE_DAILY);
         } break;
         case PERIOD_WEEK: {
             _setup_weekly_recurrence(gf, r);

             gtk_notebook_set_current_page(gf->nb, PAGE_WEEKLY);
             gtk_combo_box_set_active(gf->freqComboBox, PAGE_WEEKLY);
         } break;
         case PERIOD_END_OF_MONTH:
         case PERIOD_MONTH:
         case PERIOD_YEAR:
         case PERIOD_LAST_WEEKDAY: {
             guint multiplier;
             GtkWidget *multipler_spin, *day_of_month;
             GDate recurrence_day;
             int day_of_month_index;
             
             multipler_spin = glade_xml_get_widget(gf->gxml, "monthly_spin");
             multiplier = recurrenceGetMultiplier(r);
             if (recurrenceGetPeriodType(r) == PERIOD_YEAR)
                 multiplier *= 12;
             gtk_spin_button_set_value(GTK_SPIN_BUTTON(multipler_spin), multiplier);

             day_of_month = glade_xml_get_widget(gf->gxml, "monthly_day");
             
             gtk_combo_box_set_active(GTK_COMBO_BOX(day_of_month), _get_monthly_combobox_index(r));

             gtk_notebook_set_current_page(gf->nb, PAGE_MONTHLY);
             gtk_combo_box_set_active(gf->freqComboBox, PAGE_MONTHLY);
         } break; 
         case PERIOD_NTH_WEEKDAY:
             g_critical("unhandled period type [%d]", recurrenceGetPeriodType(r));
             break;
         default:
             g_error("unknown recurrence period type [%d]", recurrenceGetPeriodType(r));
             break;
         }
     }

maybe_signal:
     if (made_changes)
          g_signal_emit_by_name(gf, "changed");
}

static gint
_get_multiplier_from_widget(GncFrequency *gf, char *widget_name)
{
    GtkWidget *multiplier_spin;
    multiplier_spin = glade_xml_get_widget(gf->gxml, widget_name);
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(multiplier_spin));
}

static Recurrence*
_get_day_of_month_recurrence(GncFrequency *gf, GDate *start_date, int multiplier, char *combo_name)
{
    Recurrence *r;
    GtkWidget *day_of_month_combo = glade_xml_get_widget(gf->gxml, combo_name);
    int day_of_month_index = gtk_combo_box_get_active(GTK_COMBO_BOX(day_of_month_combo));
        
    r = g_new0(Recurrence, 1);
    if (day_of_month_index > 30)
    {
        GDate *day_of_week_date = g_date_new_julian(g_date_get_julian(start_date));
        // increment until we align on the DOW, but stay inside the month
        g_date_set_day(day_of_week_date, 1);
        while (g_date_get_weekday(day_of_week_date) != (day_of_month_index - 30))
            g_date_add_days(day_of_week_date, 1);
        recurrenceSet(r, multiplier, PERIOD_LAST_WEEKDAY, day_of_week_date);
    }
    else
    {
        GDate *day_of_month = g_date_new_julian(g_date_get_julian(start_date));
        g_date_set_month(day_of_month, 1);
        g_date_set_day(day_of_month, day_of_month_index + 1);
        recurrenceSet(r, multiplier, PERIOD_MONTH, day_of_month);
    }
    return r;
}

void
gnc_frequency_save_to_recurrence(GncFrequency *gf, GList **recurrences, GDate *out_start_date)
{
    GDate start_date;
    gint page_index;

    gnc_date_edit_get_gdate(gf->startDate, &start_date);
    if (out_start_date != NULL)
        *out_start_date = start_date;

    if (recurrences == NULL)
        return;

    page_index = gtk_notebook_get_current_page(gf->nb);

    switch (page_index)
    {
    case PAGE_NONE:
        g_critical("recurrence can't support none");
        break;
    case PAGE_ONCE: {
        Recurrence *r = g_new0(Recurrence, 1);
        recurrenceSet(r, 1, PERIOD_ONCE, &start_date);
        *recurrences = g_list_append(*recurrences, r);
    } break;
    case PAGE_DAILY: {
        gint multiplier = _get_multiplier_from_widget(gf, "daily_spin");
        Recurrence *r = g_new0(Recurrence, 1);
        recurrenceSet(r, multiplier, PERIOD_DAY, &start_date);
        *recurrences = g_list_append(*recurrences, r);
    } break;
    case PAGE_WEEKLY: {
        int multiplier = _get_multiplier_from_widget(gf, "weekly_spin");
        int checkbox_idx;
        for (checkbox_idx = 0; CHECKBOX_NAMES[checkbox_idx] != NULL; checkbox_idx++)
        {
            GDate *day_of_week_date;
            Recurrence *r;
            const char *day_widget_name = CHECKBOX_NAMES[checkbox_idx];
            GtkWidget *weekday_checkbox = glade_xml_get_widget(gf->gxml, day_widget_name);

            if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(weekday_checkbox)))
                continue;

            day_of_week_date = g_date_new_julian(g_date_get_julian(&start_date));
            // increment until we align on the DOW.
            g_date_set_day(day_of_week_date, 1);
            while ((g_date_get_weekday(day_of_week_date) % 7) != checkbox_idx)
                g_date_add_days(day_of_week_date, 1);

            r = g_new0(Recurrence, 1);
            recurrenceSet(r, multiplier, PERIOD_WEEK, day_of_week_date);
            
            *recurrences = g_list_append(*recurrences, r);
        }
    } break;
    case PAGE_SEMI_MONTHLY: {
        int multiplier = _get_multiplier_from_widget(gf, "semimonthly_spin");
        *recurrences = g_list_append(*recurrences, _get_day_of_month_recurrence(gf, &start_date, multiplier, "semimonthly_first"));
        *recurrences = g_list_append(*recurrences, _get_day_of_month_recurrence(gf, &start_date, multiplier, "semimonthly_second"));
    } break;
    case PAGE_MONTHLY: {
        int multiplier = _get_multiplier_from_widget(gf, "monthly_spin");
        Recurrence *r = _get_day_of_month_recurrence(gf, &start_date, multiplier, "monthly_day");
        *recurrences = g_list_append(*recurrences, r);
    } break;
    default:
        g_error("unknown page index [%d]", page_index);
        break;
    }
}
