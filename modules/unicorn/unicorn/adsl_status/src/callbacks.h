#include <gtk/gtk.h>


gboolean
on_main_window_destroy_event           (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_main_window_delete_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_StartButton_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_StopButton_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_PowerChart_expose_event             (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

gboolean
on_PowerChart_configure_event          (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data);

gboolean
on_RxPathLabel_configure_event         (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data);

gboolean
on_SNRLabel_configure_event            (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data);

gboolean
on_EchoNoiseLabel_configure_event      (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data);

void
on_GenerateLBcellsButton_toggled       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_ResetCountersButton_clicked         (GtkButton       *button,
                                        gpointer         user_data);
