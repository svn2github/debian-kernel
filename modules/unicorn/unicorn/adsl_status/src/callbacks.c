#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "status.h"


void
on_StartButton_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
	start_msw();
}


void
on_StopButton_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
	stop_msw();
}


gboolean
on_PowerChart_expose_event             (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data)
{
  draw_chart(widget,event);
  return FALSE;
}


gboolean
on_PowerChart_configure_event          (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data)
{
  create_chart(widget);
  return FALSE;
}


gboolean
on_RxPathLabel_configure_event         (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data)
{
  create_RxPathLabel(widget);
  return FALSE;
}


gboolean
on_SNRLabel_configure_event            (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data)
{
  create_SNRLabel(widget);
  return FALSE;
}


gboolean
on_EchoNoiseLabel_configure_event      (GtkWidget       *widget,
                                        GdkEventConfigure *event,
                                        gpointer         user_data)
{
  create_EchoNoiseLabel(widget);
  return FALSE;
}


void
on_GenerateLBcellsButton_toggled       (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  gboolean active;
  
  active = gtk_toggle_button_get_active(togglebutton);
  if (active) {
    start_send_oam_lb();
  } else {
    stop_send_oam_lb();
  } 
}


void
on_ResetCountersButton_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
  oam_reset_counters();
}

