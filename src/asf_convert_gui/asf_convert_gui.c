/* This program is a simple GUI wrapper around the asf_convert
   tool.  */

#include "asf_convert_gui.h"

GladeXML *glade_xml;
GtkListStore *list_store = NULL;
gboolean keep_going;
gboolean processing;

int
main(int argc, char **argv)
{
    GtkWidget *widget;

    gtk_init(&argc, &argv);

    gchar *glade_xml_file = (gchar *)find_in_path("asf_convert_gui.glade");
    glade_xml = glade_xml_new(glade_xml_file, NULL, NULL);

    g_free(glade_xml_file);

    /* select defaults for dropdowns */
    widget = glade_xml_get_widget (glade_xml, "input_data_type_combobox");
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), INPUT_TYPE_AMP);
    widget = glade_xml_get_widget (glade_xml, "input_data_format_combobox");
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), INPUT_FORMAT_CEOS_LEVEL1);
    widget = glade_xml_get_widget (glade_xml, "output_format_combobox");
    gtk_combo_box_set_active(GTK_COMBO_BOX(widget), OUTPUT_FORMAT_JPEG);

    /* fire handlers for hiding/showing stuff */
    output_format_combobox_changed();
    input_data_format_combobox_changed();
    show_execute_button(TRUE);

    /* build columns in the files section */
    setup_files_list(argc, argv);

    /* allow multiple selects */
    widget = glade_xml_get_widget(glade_xml, "input_file_selection");
    gtk_file_selection_set_select_multiple(GTK_FILE_SELECTION(widget), TRUE);

    /* drag-n-drop setup */
    setup_dnd();

    /* right-click menu setup */
    setup_popup_menu();

    /* set initial vpanel setting */
    widget = glade_xml_get_widget(glade_xml, "vertical_pane");

    /* not sure why this doesn't work
    gtk_widget_style_get_property(widget, "max-position", &val);
    p = (gint) floor (0.75 * (double) ((gint) g_value_get_uint(&val)));
    printf("%d\n", p);
    gtk_paned_set_position(GTK_PANED(widget), p);
    */
    gtk_paned_set_position(GTK_PANED(widget), 360);

    /* Connect signal handlers.  */
    glade_xml_signal_autoconnect (glade_xml);

    processing = FALSE;

    gtk_main ();

    exit (EXIT_SUCCESS);
}
