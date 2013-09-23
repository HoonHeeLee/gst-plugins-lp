#include <gst/gst.h>
#include <stdio.h>
#include "gst/playback/gstlpbin.h"

static GMainLoop *loop = NULL;

static gboolean
bus_call (GstBus * bus, GstMessage * message, gpointer data)
{
  GstLpBin *lpbin = (GstLpBin *) data;
  GstClock *clock;
  GstClockTime base_time, running_time;
  GstClockTime position;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_APPLICATION:
    {
      GstSample *sample;
      GstBuffer *buf;
      GstCaps *caps;
      const GstStructure *structure = gst_message_get_structure (message);

      GST_WARNING ("got application msg");

      if (!gst_structure_has_name (structure, "subtitle_data")) {
        break;
      }

      sample =
          gst_value_get_sample (gst_structure_get_value (structure, "sample"));

      if (!sample) {
        GST_WARNING ("invalid sample value");
        break;
      }

      caps = gst_sample_get_caps (sample);
      buf = gst_sample_get_buffer (sample);

      clock = gst_element_get_clock (GST_ELEMENT_CAST (lpbin));
      base_time = gst_element_get_base_time (GST_ELEMENT_CAST (lpbin));
      running_time = gst_clock_get_time (clock) - base_time;

      position = GST_BUFFER_PTS (buf);

      GST_WARNING ("buf is %p, caps is %s", buf, gst_caps_to_string (caps));
      GST_WARNING ("received a subtitle at position %" GST_TIME_FORMAT
          ", running_time %" GST_TIME_FORMAT, GST_TIME_ARGS (position),
          GST_TIME_ARGS (running_time));

      /* application should do unref after get the sample data */
      gst_sample_unref (sample);
    }
      break;
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR:
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }
  return TRUE;
}

gint
main (gint argc, gchar * argv[])
{
  GstElement *lpbin;
  GstBus *bus;
  guint bus_watch_id;
  gchar *uri;
  GstStateChangeReturn sret;

  gst_init (&argc, &argv);

  if (argc < 2) {
    GST_ERROR ("usage: %s <media file or uri>\n", argv[0]);
    return 1;
  }

  lpbin = gst_element_factory_make ("lpbin", NULL);
  if (!lpbin) {
    GST_ERROR ("lpbin plugin missing\n");
    return 1;
  }

  if (gst_uri_is_valid (argv[1]))
    uri = g_strdup (argv[1]);
  else
    uri = gst_filename_to_uri (argv[1], NULL);

  g_object_set (lpbin, "uri", uri, NULL);
  g_free (uri);
  loop = g_main_loop_new (NULL, FALSE);

  bus = gst_element_get_bus (lpbin);
  if (bus) {
    gst_bus_add_signal_watch (bus);
    g_signal_connect (bus, "message", G_CALLBACK (bus_call), lpbin);
  }
  g_object_unref (bus);

  if (gst_element_set_state (lpbin,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    GST_DEBUG_OBJECT (lpbin, "Failed to paused state");
    goto done;
  }

  sret = gst_element_get_state (lpbin, NULL, NULL, -1);

  if (sret == GST_STATE_CHANGE_FAILURE) {
    GST_DEBUG_OBJECT (lpbin, "Failed to get lpbin state");
    goto done;
  }

  sret = gst_element_get_state (lpbin, NULL, NULL, -1);

  if (sret == GST_STATE_CHANGE_FAILURE) {
    GST_DEBUG_OBJECT (lpbin, "Failed to get lpbin state");
    goto done;
  }
  g_main_loop_run (loop);

done:
  gst_element_set_state (lpbin, GST_STATE_NULL);
  g_object_unref (lpbin);
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);

  return 0;
}