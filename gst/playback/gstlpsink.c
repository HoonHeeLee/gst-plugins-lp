/* GStreamer Lightweight Playback Plugins
 * Copyright (C) 2013 LG Electronics.
 *	Author : Jeongseok Kim <jeongseok.kim@lge.com>
 *	         Wonchul Lee <wonchul86.lee@lge.com>
 *	         Hoonhee Lee <hoonhee.lee@lge.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "gstlpsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_lp_sink_debug);
#define GST_CAT_DEFAULT gst_lp_sink_debug

static GstStaticPadTemplate audiotemplate =
GST_STATIC_PAD_TEMPLATE ("audio_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate videotemplate =
GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate texttemplate = GST_STATIC_PAD_TEMPLATE ("text_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

/* props */
enum
{
  PROP_0,
  PROP_VIDEO_SINK,
  PROP_AUDIO_SINK,
  PROP_VIDEO_RESOURCE,
  PROP_AUDIO_RESOURCE,
  PROP_LAST
};

#define DEFAULT_THUMBNAIL_MODE FALSE

static void gst_lp_sink_dispose (GObject * object);
static void gst_lp_sink_finalize (GObject * object);
static void gst_lp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * spec);
static void gst_lp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * spec);

static GstPad *gst_lp_sink_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_lp_sink_release_request_pad (GstElement * element,
    GstPad * pad);
static gboolean gst_lp_sink_send_event (GstElement * element, GstEvent * event);
static gboolean gst_lp_sink_send_event_to_sink (GstLpSink * lpsink,
    GstEvent * event);
static GstStateChangeReturn gst_lp_sink_change_state (GstElement * element,
    GstStateChange transition);

static void gst_lp_sink_handle_message (GstBin * bin, GstMessage * message);

void gst_lp_sink_set_sink (GstLpSink * lpsink, GstLpSinkType type,
    GstElement * sink);
GstElement *gst_lp_sink_get_sink (GstLpSink * lpsink, GstLpSinkType type);
static GstFlowReturn gst_lp_sink_new_sample (GstElement * sink);
static void src_pad_added_cb (GstElement * osel, GstPad * pad,
    GstLpSink * lpsink);
static gboolean gst_lp_sink_do_reconfigure (GstLpSink * lpsink,
    GstLpSinkType type, GstPad * fnl_srcpad);
static gboolean add_chain (GstSinkChain * chain, gboolean add);
static gboolean activate_chain (GstSinkChain * chain, gboolean activate);

static void
_do_init (GType type)
{
  // TODO
}

G_DEFINE_TYPE_WITH_CODE (GstLpSink, gst_lp_sink, GST_TYPE_BIN,
    _do_init (g_define_type_id));

#define parent_class gst_lp_sink_parent_class

static void
gst_lp_sink_class_init (GstLpSinkClass * klass)
{
  GObjectClass *gobject_klass;
  GstElementClass *gstelement_klass;
  GstBinClass *gstbin_klass;

  gobject_klass = (GObjectClass *) klass;
  gstelement_klass = (GstElementClass *) klass;
  gstbin_klass = (GstBinClass *) klass;

  gobject_klass->dispose = gst_lp_sink_dispose;
  gobject_klass->finalize = gst_lp_sink_finalize;
  gobject_klass->set_property = gst_lp_sink_set_property;
  gobject_klass->get_property = gst_lp_sink_get_property;

  /**
   * GstPlaySink:video-sink:
   *
   * Set the used video sink element. NULL will use the default sink. playsink
   * must be in %GST_STATE_NULL
   *
   */
  g_object_class_install_property (gobject_klass, PROP_VIDEO_SINK,
      g_param_spec_object ("video-sink", "Video Sink",
          "the video output element to use (NULL = default sink)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPlaySink:audio-sink:
   *
   * Set the used audio sink element. NULL will use the default sink. playsink
   * must be in %GST_STATE_NULL
   *
   */
  g_object_class_install_property (gobject_klass, PROP_AUDIO_SINK,
      g_param_spec_object ("audio-sink", "Audio Sink",
          "the audio output element to use (NULL = default sink)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_VIDEO_RESOURCE,
      g_param_spec_uint ("video-resource", "Acquired video resource",
          "Acquired video resource", 0, 2, 0,
          G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_klass, PROP_AUDIO_RESOURCE,
      g_param_spec_uint ("audio-resource", "Acquired audio resource",
          "Acquired audio resource (the most significant bit - 0: ADEC, 1: MIX / the remains - channel number)",
          0, G_MAXUINT, 0, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));


  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&audiotemplate));
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&videotemplate));
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&texttemplate));
  gst_element_class_set_static_metadata (gstelement_klass,
      "Lightweight Player Sink", "Lightweight/Bin/Sink",
      "Convenience sink for multiple streams in a restricted system",
      "Jeongseok Kim <jeongseok.kim@lge.com>");

  gstelement_klass->change_state = GST_DEBUG_FUNCPTR (gst_lp_sink_change_state);

  gstelement_klass->send_event = GST_DEBUG_FUNCPTR (gst_lp_sink_send_event);
  gstelement_klass->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_lp_sink_request_new_pad);
  gstelement_klass->release_pad =
      GST_DEBUG_FUNCPTR (gst_lp_sink_release_request_pad);

  gstbin_klass->handle_message = GST_DEBUG_FUNCPTR (gst_lp_sink_handle_message);
}

static void
gst_lp_sink_init (GstLpSink * lpsink)
{
  GST_DEBUG_CATEGORY_INIT (gst_lp_sink_debug, "lpsink", 0,
      "Lightweight Play Sink");

  g_rec_mutex_init (&lpsink->lock);
  lpsink->audio_sink = NULL;
  lpsink->video_sink = NULL;
  lpsink->video_pad = NULL;
  lpsink->audio_pad = NULL;
  lpsink->thumbnail_mode = DEFAULT_THUMBNAIL_MODE;

  lpsink->video_resource = 0;
  lpsink->audio_resource = 0;

  lpsink->video_rfunnel = NULL;
  lpsink->audio_rfunnel = NULL;
  lpsink->text_rfunnel = NULL;

  lpsink->video_multiple_stream = FALSE;
  lpsink->audio_multiple_stream = FALSE;

  lpsink->sink_chain_list = NULL;

  lpsink->nb_video_bin = 0;
  lpsink->nb_audio_bin = 0;
  lpsink->nb_text_bin = 0;
}

static void
gst_lp_sink_dispose (GObject * obj)
{
  GstLpSink *lpsink;
  lpsink = GST_LP_SINK (obj);

  if (lpsink->audio_sink != NULL) {
    gst_element_set_state (lpsink->audio_sink, GST_STATE_NULL);
    gst_object_unref (lpsink->audio_sink);
    lpsink->audio_sink = NULL;
  }

  if (lpsink->video_sink != NULL) {
    gst_element_set_state (lpsink->video_sink, GST_STATE_NULL);
    gst_object_unref (lpsink->video_sink);
    lpsink->video_sink = NULL;
  }

  if (lpsink->audio_pad) {
    gst_object_unref (lpsink->audio_pad);
    lpsink->audio_pad = NULL;
  }

  if (lpsink->video_pad) {
    gst_object_unref (lpsink->video_pad);
    lpsink->video_pad = NULL;
  }
  if (lpsink->text_pad) {
    gst_object_unref (lpsink->text_pad);
    lpsink->text_pad = NULL;
  }
  if (lpsink->video_rfunnel) {
    //gst_object_unref (lpsink->video_rfunnel);
    lpsink->video_rfunnel = NULL;
  }
  if (lpsink->audio_rfunnel) {
    //gst_object_unref (lpsink->audio_rfunnel);
    lpsink->audio_rfunnel = NULL;
  }
  if (lpsink->text_rfunnel) {
    //gst_object_unref (lpsink->text_rfunnel);
    lpsink->text_rfunnel = NULL;
  }
  //g_list_foreach (lpsink->sink_chain_list, (GFunc) gst_object_unref, NULL);
  g_list_free (lpsink->sink_chain_list);
  lpsink->sink_chain_list = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_lp_sink_finalize (GObject * obj)
{
  GstLpSink *lpsink;

  lpsink = GST_LP_SINK (obj);

  g_rec_mutex_clear (&lpsink->lock);

  if (lpsink->audio_sink) {
    g_object_unref (lpsink->audio_sink);
    lpsink->audio_sink = NULL;
  }
  if (lpsink->video_sink) {
    g_object_unref (lpsink->video_sink);
    lpsink->video_sink = NULL;
  }

  if (lpsink->audio_pad) {
    gst_object_unref (lpsink->audio_pad);
    lpsink->audio_pad = NULL;
  }

  if (lpsink->video_pad) {
    gst_object_unref (lpsink->video_pad);
    lpsink->video_pad = NULL;
  }
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

void
gst_lp_sink_set_thumbnail_mode (GstLpSink * lpsink, gboolean thumbnail_mode)
{
  GST_DEBUG_OBJECT (lpsink, "set thumbnail mode as %d", thumbnail_mode);
  lpsink->thumbnail_mode = thumbnail_mode;
=======
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

void
gst_lp_sink_set_thumbnail_mode (GstLpSink * lpsink, gboolean thumbnail_mode)
{
  GST_DEBUG_OBJECT (lpsink, "set thumbnail mode as %d", thumbnail_mode);
  lpsink->thumbnail_mode = thumbnail_mode;
}

void
gst_lp_sink_set_multiple_stream (GstLpSink * lpsink, gchar * type,
    gboolean multiple_stream)
{
  GST_DEBUG_OBJECT (lpsink,
      "gst_lp_sink_set_multiple_stream : type = %s, multiple_stream = %d", type,
      multiple_stream);

  GST_LP_SINK_LOCK (lpsink);
  if (!strcmp (type, "audio")) {
    lpsink->audio_multiple_stream = multiple_stream;
  } else if (!strcmp (type, "video")) {
    lpsink->video_multiple_stream = multiple_stream;
  }
  GST_LP_SINK_UNLOCK (lpsink);
}

static GstElement *
try_element (GstLpSink * lpsink, GstElement * element, gboolean unref)
{
  GstStateChangeReturn ret;

  if (element) {
    ret = gst_element_set_state (element, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      GST_DEBUG_OBJECT (lpsink, "failed state change..");
      gst_element_set_state (element, GST_STATE_NULL);
      if (unref)
        gst_object_unref (element);
      element = NULL;
    }
  }
  return element;
}

static GstSinkChain *
gen_audio_chain (GstLpSink * lpsink)
{
  GstSinkChain *chain;
  gchar *bin_name = NULL;
  GstBin *bin = NULL;
  GstPad *queue_sinkpad = NULL;
  GstElement *sink_element = NULL;

  GST_LP_SINK_LOCK (lpsink);

  chain = g_slice_alloc0 (sizeof (GstSinkChain));
  chain->lpsink = lpsink;

  if (lpsink->thumbnail_mode)
    sink_element = gst_element_factory_make ("fakesink", NULL);
  else
    sink_element = gst_element_factory_make ("adecsink", NULL);

  if (lpsink->audio_resource & (1 << 31)) {
    g_object_set (sink_element, "mixer", TRUE, NULL);
  } else {
    g_object_set (sink_element, "mixer", FALSE, NULL);
  }
  g_object_set (sink_element, "index",
      (lpsink->audio_resource & ~(1 << 31)), NULL);
  GST_DEBUG_OBJECT (sink_element, "Request to acquire [%s:%x]",
      (lpsink->audio_resource & (1 << 31)) ? "MIXER" : "ADEC",
      (lpsink->audio_resource & ~(1 << 31)));
  chain->sink = try_element (lpsink, sink_element, TRUE);

  if (chain->sink)
    lpsink->audio_sink = gst_object_ref (chain->sink);

  bin_name = g_strdup_printf ("abin%d", lpsink->nb_audio_bin++);
  chain->bin = gst_bin_new (bin_name);
  bin = GST_BIN_CAST (chain->bin);
  gst_object_ref_sink (bin);
  gst_bin_add (bin, chain->sink);

  chain->queue = gst_element_factory_make ("queue", NULL);
  g_object_set (chain->queue, "silent", TRUE, NULL);
  gst_bin_add (bin, chain->queue);

  if (gst_element_link_pads_full (chain->queue, "src", chain->sink, NULL,
          GST_PAD_LINK_CHECK_TEMPLATE_CAPS) == FALSE) {
    GST_INFO_OBJECT (lpsink, "A fakesink will be deployed for audio sink.");

    gst_bin_remove (GST_BIN_CAST (bin), chain->sink);
    sink_element = gst_element_factory_make ("fakesink", NULL);
    chain->sink = try_element (lpsink, sink_element, TRUE);

    gst_bin_add (GST_BIN_CAST (bin), chain->sink);
    gst_element_link_pads_full (chain->queue, "src", chain->sink, NULL,
        GST_PAD_LINK_CHECK_TEMPLATE_CAPS);
  }

  queue_sinkpad = gst_element_get_static_pad (chain->queue, "sink");
  chain->bin_ghostpad = gst_ghost_pad_new ("sink", queue_sinkpad);

  gst_object_unref (queue_sinkpad);
  gst_element_add_pad (chain->bin, chain->bin_ghostpad);

  GST_LP_SINK_UNLOCK (lpsink);

  lpsink->sink_chain_list =
      g_list_append (lpsink->sink_chain_list, (GstSinkChain *) chain);

  return chain;
}

static GstSinkChain *
gen_video_chain (GstLpSink * lpsink)
{
  GstSinkChain *chain;
  gchar *bin_name = NULL;
  GstBin *bin = NULL;
  GstPad *queue_sinkpad = NULL;
  GstElement *sink_element = NULL;

  GST_LP_SINK_LOCK (lpsink);

  chain = g_slice_alloc0 (sizeof (GstSinkChain));
  chain->lpsink = lpsink;

  sink_element = gst_element_factory_make ("vdecsink", NULL);
  GST_DEBUG_OBJECT (sink_element,
      "Passing vdec ch property[%x] into vdecsink", lpsink->video_resource);
  g_object_set (sink_element, "vdec-ch", lpsink->video_resource, NULL);
  chain->sink = try_element (lpsink, sink_element, TRUE);

  if (chain->sink)
    lpsink->video_sink = gst_object_ref (chain->sink);

  bin_name = g_strdup_printf ("vbin%d", lpsink->nb_video_bin++);
  chain->bin = gst_bin_new (bin_name);
  bin = GST_BIN_CAST (chain->bin);
  gst_object_ref_sink (bin);
  gst_bin_add (bin, chain->sink);

  chain->queue = gst_element_factory_make ("queue", NULL);
  g_object_set (G_OBJECT (chain->queue), "max-size-buffers", 3,
      "max-size-bytes", 0, "max-size-time", (gint64) 0, "silent", TRUE, NULL);
  gst_bin_add (bin, chain->queue);

  gst_element_link_pads_full (chain->queue, "src", chain->sink, NULL,
      GST_PAD_LINK_CHECK_TEMPLATE_CAPS);

  queue_sinkpad = gst_element_get_static_pad (chain->queue, "sink");
  chain->bin_ghostpad = gst_ghost_pad_new ("sink", queue_sinkpad);

  gst_object_unref (queue_sinkpad);
  gst_element_add_pad (chain->bin, chain->bin_ghostpad);

  GST_LP_SINK_UNLOCK (lpsink);

  lpsink->sink_chain_list =
      g_list_append (lpsink->sink_chain_list, (GstSinkChain *) chain);

  return chain;
}

static GstSinkChain *
gen_text_chain (GstLpSink * lpsink)
{
  GstSinkChain *chain;
  gchar *bin_name = NULL;
  GstBin *bin = NULL;
  GstPad *queue_sinkpad = NULL;
  GstElement *sink_element = NULL;

  GST_LP_SINK_LOCK (lpsink);

  chain = g_slice_alloc0 (sizeof (GstSinkChain));
  chain->lpsink = lpsink;

  sink_element = gst_element_factory_make ("appsink", NULL);
  chain->sink = try_element (lpsink, sink_element, TRUE);

  g_object_set (sink_element, "emit-signals", TRUE, NULL);
  g_signal_connect (sink_element, "new-sample",
      G_CALLBACK (gst_lp_sink_new_sample), NULL);

  if (chain->sink)
    lpsink->text_sink = gst_object_ref (chain->sink);

  bin_name = g_strdup_printf ("tbin%d", lpsink->nb_text_bin++);
  chain->bin = gst_bin_new (bin_name);
  bin = GST_BIN_CAST (chain->bin);
  gst_object_ref_sink (bin);
  gst_bin_add (bin, chain->sink);

  chain->queue = gst_element_factory_make ("queue", NULL);
  g_object_set (G_OBJECT (chain->queue), "max-size-buffers", 3,
      "max-size-bytes", 0, "max-size-time", (gint64) GST_SECOND, "silent", TRUE,
      NULL);
  gst_bin_add (bin, chain->queue);

  gst_element_link_pads_full (chain->queue, "src", chain->sink, NULL,
      GST_PAD_LINK_CHECK_TEMPLATE_CAPS);

  queue_sinkpad = gst_element_get_static_pad (chain->queue, "sink");
  chain->bin_ghostpad = gst_ghost_pad_new ("sink", queue_sinkpad);

  gst_object_unref (queue_sinkpad);
  gst_element_add_pad (chain->bin, chain->bin_ghostpad);

  GST_LP_SINK_UNLOCK (lpsink);

  lpsink->sink_chain_list =
      g_list_append (lpsink->sink_chain_list, (GstSinkChain *) chain);

  return chain;
}


static void
src_pad_added_cb (GstElement * rfunnel, GstPad * pad, GstLpSink * lpsink)
{
  GST_DEBUG_OBJECT (lpsink, "src_pad_added_cb : osel = %s, pad = %s, caps = %s",
      gst_element_get_name (rfunnel), gst_pad_get_name (pad),
      gst_caps_to_string (gst_pad_get_current_caps (pad)));

  gchar *caps_str = NULL;
  caps_str = gst_caps_to_string (gst_pad_get_current_caps (pad));

  if (g_str_has_prefix (caps_str, "video/")) {
    gst_lp_sink_do_reconfigure (lpsink, GST_LP_SINK_TYPE_VIDEO, pad);
  } else if (g_str_has_prefix (caps_str, "audio/")) {
    gst_lp_sink_do_reconfigure (lpsink, GST_LP_SINK_TYPE_AUDIO, pad);
  } else if (g_str_has_prefix (caps_str, "text/")
      || g_str_has_prefix (caps_str, "application/")
      || g_str_has_prefix (caps_str, "subpicture/")) {
    gst_lp_sink_do_reconfigure (lpsink, GST_LP_SINK_TYPE_TEXT, pad);
  }

  g_free (caps_str);
}

static void
caps_notify_cb (GstPad * pad, GParamSpec * unused, GstLpSink * lpsink)
{
  gboolean reconfigure = FALSE;
  GstCaps *caps;
  gboolean raw;

  g_object_get (pad, "caps", &caps, NULL);
  if (!caps)
    return;

  GST_DEBUG_OBJECT (lpsink, "caps_notify_cb : caps = %s",
      gst_caps_to_string (caps));
  if (pad == lpsink->audio_pad) {
    if (gst_ghost_pad_get_target (GST_GHOST_PAD (pad)) == NULL)
      gst_lp_sink_do_reconfigure (lpsink, GST_LP_SINK_TYPE_AUDIO, NULL);
  } else if (pad == lpsink->video_pad) {
    if (gst_ghost_pad_get_target (GST_GHOST_PAD (pad)) == NULL)
      gst_lp_sink_do_reconfigure (lpsink, GST_LP_SINK_TYPE_VIDEO, NULL);
  }

  gst_caps_unref (caps);
}

static gboolean
gst_lp_sink_do_reconfigure (GstLpSink * lpsink, GstLpSinkType type,
    GstPad * fnl_srcpad)
{
  GstSinkChain *audiochain;
  GstSinkChain *videochain;
  GstSinkChain *textchain;
  //GstPad *os_srcpad;

  if (type == GST_LP_SINK_TYPE_AUDIO) {
    audiochain = gen_audio_chain (lpsink);

    add_chain (GST_SINK_CHAIN (audiochain), TRUE);
    activate_chain (GST_SINK_CHAIN (audiochain), TRUE);

    if (lpsink->audio_pad)
      gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (lpsink->audio_pad),
          audiochain->bin_ghostpad);

  } else if (type == GST_LP_SINK_TYPE_VIDEO) {
    videochain = gen_video_chain (lpsink);

    add_chain (GST_SINK_CHAIN (videochain), TRUE);
    activate_chain (GST_SINK_CHAIN (videochain), TRUE);

    if (lpsink->video_pad)
      gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (lpsink->video_pad),
          videochain->bin_ghostpad);

  } else if (type == GST_LP_SINK_TYPE_TEXT) {
    textchain = gen_text_chain (lpsink);

    add_chain (GST_SINK_CHAIN (textchain), TRUE);
    activate_chain (GST_SINK_CHAIN (textchain), TRUE);

    if (lpsink->text_rfunnel) {
      gst_pad_link_full (fnl_srcpad,
          textchain->bin_ghostpad, GST_PAD_LINK_CHECK_NOTHING);
    }
  }

  return TRUE;
}

/**
 * gst_lp_sink_request_pad
 * @lpsink: a #GstLpSink
 * @type: a #GstLpSinkType
 *
 * Create or return a pad of @type.
 *
 * Returns: a #GstPad of @type or %NULL when the pad could not be created.
 */
GstPad *
gst_lp_sink_request_pad (GstLpSink * lpsink, GstLpSinkType type)
{
  GstPad *res = NULL;
  const gchar *pad_name;
  GstElement *osel;
  GstPad *rfnl_sinkpad, *rfnl_srcpad;

  GST_LP_SINK_LOCK (lpsink);

  switch (type) {
    case GST_LP_SINK_TYPE_AUDIO:
      if (lpsink->thumbnail_mode)
        sink_name = "fakesink";
      else
        sink_name = "adecsink";
      pad_name = "audio_sink";
      if (!lpsink->audio_pad) {
        if (lpsink->audio_multiple_stream) {
          lpsink->audio_rfunnel =
              gst_element_factory_make ("reversefunnel", NULL);
          rfnl_sinkpad =
              gst_element_get_static_pad (lpsink->audio_rfunnel, "sink");
          gst_bin_add (GST_BIN_CAST (lpsink), lpsink->audio_rfunnel);

          lpsink->audio_pad = gst_ghost_pad_new (pad_name, rfnl_sinkpad);
          g_signal_connect (lpsink->audio_rfunnel, "src-pad-added",
              G_CALLBACK (src_pad_added_cb), lpsink);
        } else {
          lpsink->audio_pad =
              gst_ghost_pad_new_no_target (pad_name, GST_PAD_SINK);
          g_signal_connect (G_OBJECT (lpsink->audio_pad), "notify::caps",
              G_CALLBACK (caps_notify_cb), lpsink);
        }
      }
      res = lpsink->audio_pad;
      break;
    case GST_LP_SINK_TYPE_VIDEO:
      sink_name = "vdecsink";
      pad_name = "video_sink";
      if (!lpsink->video_pad) {
        if (lpsink->video_multiple_stream) {
          lpsink->video_rfunnel =
              gst_element_factory_make ("reversefunnel", NULL);
          rfnl_sinkpad =
              gst_element_get_static_pad (lpsink->video_rfunnel, "sink");
          gst_bin_add (GST_BIN_CAST (lpsink), lpsink->video_rfunnel);

          lpsink->video_pad = gst_ghost_pad_new (pad_name, rfnl_sinkpad);
          g_signal_connect (lpsink->video_rfunnel, "src-pad-added",
              G_CALLBACK (src_pad_added_cb), lpsink);
        } else {
          lpsink->video_pad =
              gst_ghost_pad_new_no_target (pad_name, GST_PAD_SINK);
          g_signal_connect (G_OBJECT (lpsink->video_pad), "notify::caps",
              G_CALLBACK (caps_notify_cb), lpsink);
        }
      }
      res = lpsink->video_pad;
      break;
    case GST_LP_SINK_TYPE_TEXT:
      sink_name = "appsink";
      pad_name = "text_sink";
      lpsink->text_rfunnel = gst_element_factory_make ("reversefunnel", NULL);
      rfnl_sinkpad = gst_element_get_static_pad (lpsink->text_rfunnel, "sink");
      gst_bin_add (GST_BIN_CAST (lpsink), lpsink->text_rfunnel);
      gst_element_set_state (lpsink->text_rfunnel, GST_STATE_PAUSED);
      if (!lpsink->text_pad) {
        lpsink->text_pad = gst_ghost_pad_new (pad_name, rfnl_sinkpad);
        g_signal_connect (lpsink->text_rfunnel, "src-pad-added",
            G_CALLBACK (src_pad_added_cb), lpsink);
      }
      res = lpsink->text_pad;
      break;
    default:
      res = NULL;
  }
  GST_LP_SINK_UNLOCK (lpsink);

  gst_pad_set_active (res, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (lpsink), res);

  return res;
}

static GstPad *
gst_lp_sink_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name, const GstCaps * caps)
{
  GstLpSink *lpsink;
  const gchar *tplname;
  GstPad *pad;
  GstLpSinkType type;

  g_return_val_if_fail (templ != NULL, NULL);
  GST_DEBUG_OBJECT (element, "name: %s", name);

  lpsink = GST_LP_SINK (element);
  tplname = GST_PAD_TEMPLATE_NAME_TEMPLATE (templ);

  if (!strncmp ("audio_sink", tplname, 10))
    type = GST_LP_SINK_TYPE_AUDIO;
  else if (!strncmp ("video_sink", tplname, 10))
    type = GST_LP_SINK_TYPE_VIDEO;
  else if (!strncmp ("text_sink", tplname, 9))
    type = GST_LP_SINK_TYPE_TEXT;
  else
    goto unknown_template;

  pad = gst_lp_sink_request_pad (lpsink, type);
  return pad;

unknown_template:
  GST_WARNING_OBJECT (element, "Unknown pad template");
  return NULL;
}

void
gst_lp_sink_release_pad (GstLpSink * lpsink, GstPad * pad)
{
  GstPad **res = NULL;
  gboolean untarget = TRUE;

  GST_DEBUG_OBJECT (lpsink, "release pad %" GST_PTR_FORMAT, pad);

  GST_LP_SINK_LOCK (lpsink);
  if (pad == lpsink->video_pad) {
    res = &lpsink->video_pad;
  } else if (pad == lpsink->audio_pad) {
    res = &lpsink->audio_pad;
  } else {
    res = &pad;
    untarget = FALSE;
  }

  GST_LP_SINK_UNLOCK (lpsink);
  if (*res) {
    GST_DEBUG_OBJECT (lpsink, "deactivate pad %" GST_PTR_FORMAT, *res);
    // TODO
    gst_pad_set_active (*res, FALSE);
    if (untarget)
      gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (*res), NULL);
    GST_DEBUG_OBJECT (lpsink, "remove pad %" GST_PTR_FORMAT, *res);
    gst_element_remove_pad (GST_ELEMENT_CAST (lpsink), *res);
    *res = NULL;
  }
}

static void
gst_lp_sink_release_request_pad (GstElement * element, GstPad * pad)
{
  GstLpSink *lpsink = GST_LP_SINK (element);

  gst_lp_sink_release_pad (lpsink, pad);
}

static void
gst_lp_sink_handle_message (GstBin * bin, GstMessage * message)
{
//  GstLpSink *lpsink;
//  lpsink = GST_LP_SINK_CAST (bin);

  switch (GST_MESSAGE_TYPE (message)) {
    default:
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
  }
}

static void
gst_lp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * spec)
{
  GstLpSink *lpsink = GST_LP_SINK (object);

  switch (prop_id) {
    case PROP_VIDEO_SINK:
      gst_lp_sink_set_sink (lpsink, GST_LP_SINK_TYPE_VIDEO,
          g_value_get_object (value));
      break;
    case PROP_AUDIO_SINK:
      gst_lp_sink_set_sink (lpsink, GST_LP_SINK_TYPE_AUDIO,
          g_value_get_object (value));
      break;
    case PROP_VIDEO_RESOURCE:
      lpsink->video_resource = g_value_get_uint (value);
      break;
    case PROP_AUDIO_RESOURCE:
      lpsink->audio_resource = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, spec);
      break;
  }
}

static void
gst_lp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * spec)
{
  GstLpSink *lpsink = GST_LP_SINK (object);
  switch (prop_id) {
    case PROP_VIDEO_SINK:
      g_value_take_object (value, gst_lp_sink_get_sink (lpsink,
              GST_LP_SINK_TYPE_VIDEO));
      break;
    case PROP_AUDIO_SINK:
      g_value_take_object (value, gst_lp_sink_get_sink (lpsink,
              GST_LP_SINK_TYPE_AUDIO));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, spec);
      break;
  }
}

/* We only want to send the event to a single sink (overriding GstBin's
 * behaviour), but we want to keep GstPipeline's behaviour - wrapping seek
 * events appropriately. So, this is a messy duplication of code. */
static gboolean
gst_lp_sink_send_event (GstElement * element, GstEvent * event)
{
  gboolean res = FALSE;
  GstEventType event_type = GST_EVENT_TYPE (event);
  GstLpSink *lpsink = GST_LP_SINK (element);

  switch (event_type) {
    case GST_EVENT_SEEK:
      GST_DEBUG_OBJECT (element, "Sending event to a sink");
      res = gst_lp_sink_send_event_to_sink (lpsink, event);
      break;
    default:
      res = GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
      break;
  }
  return res;
}

static gboolean
gst_lp_sink_send_event_to_sink (GstLpSink * lpsink, GstEvent * event)
{
  gboolean res = TRUE;

  if (lpsink->sink_chain_list) {
    GList *walk = lpsink->sink_chain_list;

    while (walk) {
      GstSinkChain *chain = (GstSinkChain *) walk->data;
      if (chain->sink) {
        gst_event_ref (event);
        if ((res = gst_element_send_event (chain->sink, event)))
          GST_DEBUG_OBJECT (lpsink, "Sent event successfully to sink");
        else
          GST_DEBUG_OBJECT (lpsink, "Event failed when sent to sink");
      }
      walk = g_list_next (walk);
    }
  }

  gst_event_unref (event);
  return res;
}

static gboolean
add_chain (GstSinkChain * chain, gboolean add)
{
  //if (chain->added == add)
  //return TRUE;

  if (add)
    gst_bin_add (GST_BIN_CAST (chain->lpsink), chain->bin);
  else {
    gst_bin_remove (GST_BIN_CAST (chain->lpsink), chain->bin);
    /* we don't want to lose our sink status */
    GST_OBJECT_FLAG_SET (chain->lpsink, GST_ELEMENT_FLAG_SINK);
  }

  return TRUE;
}

static void
free_chain (GstSinkChain * chain)
{
  if (chain) {
    if (chain->bin)
      gst_object_unref (chain->bin);
    //g_free (chain);
  }
}

static gboolean
activate_chain (GstSinkChain * chain, gboolean activate)
{
  GstState state;

  //if (chain->activated == activate)
  //return TRUE;

  GST_OBJECT_LOCK (chain->lpsink);
  state = GST_STATE_TARGET (chain->lpsink);
  GST_OBJECT_UNLOCK (chain->lpsink);
  if (activate)
    gst_element_set_state (chain->bin, state);
  else
    gst_element_set_state (chain->bin, GST_STATE_NULL);

  //chain->activated = activate;

  return TRUE;
}

static GstStateChangeReturn
gst_lp_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstStateChangeReturn bret;
  GstLpSink *lpsink;
  lpsink = GST_LP_SINK (element);

  switch (transition) {
    default:
      /* all other state changes return SUCCESS by default, this value can be
       * overridden by the result of the children */
      ret = GST_STATE_CHANGE_SUCCESS;
      break;
  }

  /* do the state change of the children */
  bret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  /* now look at the result of our children and adjust the return value */
  switch (bret) {
    case GST_STATE_CHANGE_FAILURE:
      /* failure, we stop */
      goto activate_failed;
    case GST_STATE_CHANGE_NO_PREROLL:
      /* some child returned NO_PREROLL. This is strange but we never know. We
       * commit our async state change (if any) and return the NO_PREROLL */
//      do_async_done (playsink);
      ret = bret;
      break;
    case GST_STATE_CHANGE_ASYNC:
      /* some child was async, return this */
      ret = bret;
      break;
    default:
      /* return our previously configured return value */
      break;
  }

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_lp_sink_release_pad (lpsink, lpsink->audio_pad);
      gst_lp_sink_release_pad (lpsink, lpsink->video_pad);
      gst_lp_sink_release_pad (lpsink, lpsink->text_pad);
      if (lpsink->sink_chain_list) {
        GList *walk = lpsink->sink_chain_list;

        while (walk) {
          GstSinkChain *chain = (GstSinkChain *) walk->data;
          activate_chain (chain, FALSE);
          add_chain (chain, FALSE);
          gst_bin_remove (GST_BIN_CAST (chain->bin), chain->sink);
          //gst_bin_remove (GST_BIN_CAST (chain->bin), chain->queue);
          if (chain->sink != NULL) {
            gst_element_set_state (chain->sink, GST_STATE_NULL);
            gst_bin_remove (GST_BIN_CAST (chain->bin), chain->sink);
            chain->sink = NULL;
          }

          free_chain ((GstSinkChain *) chain);
          walk = g_list_next (walk);
        }

      }
      break;
    default:
      break;
  }
  return ret;
  /* ERRORS */
activate_failed:
  {
    GST_DEBUG_OBJECT (element,
        "element failed to change states -- activation problem?");
    return GST_STATE_CHANGE_FAILURE;
  }
}

void
gst_lp_sink_set_sink (GstLpSink * lpsink, GstLpSinkType type, GstElement * sink)
{
  GstElement **elem = NULL, *old = NULL;

  GST_DEBUG_OBJECT (lpsink, "Setting sink %" GST_PTR_FORMAT " as sink type %d",
      sink, type);

  GST_LP_SINK_LOCK (lpsink);
  switch (type) {
    case GST_LP_SINK_TYPE_AUDIO:
      elem = &lpsink->audio_sink;
      break;
    case GST_LP_SINK_TYPE_VIDEO:
      elem = &lpsink->video_sink;
      break;
    default:
      break;
  }

  if (elem) {
    old = *elem;
    if (sink)
      gst_object_ref (sink);
    *elem = sink;
  }
  GST_LP_SINK_UNLOCK (lpsink);

  if (old) {
    if (old != sink)
      gst_element_set_state (old, GST_STATE_NULL);
    gst_object_unref (old);
  }
}

GstElement *
gst_lp_sink_get_sink (GstLpSink * lpsink, GstLpSinkType type)
{
  GstElement *result = NULL;
  GstElement *elem = NULL;

  GST_LP_SINK_LOCK (lpsink);
  switch (type) {
    case GST_LP_SINK_TYPE_AUDIO:
      elem = lpsink->audio_sink;
      break;
    case GST_LP_SINK_TYPE_VIDEO:
      elem = lpsink->video_sink;
      break;
    default:
      break;
  }

  if (elem)
    result = gst_object_ref (elem);
  GST_LP_SINK_UNLOCK (lpsink);

  return result;
}

static GstFlowReturn
gst_lp_sink_new_sample (GstElement * sink)
{
  GstSample *sample;
  GstBuffer *buffer;
  GstCaps *caps;
  GstStructure *structure;

  GstPad *pad;

  pad = gst_element_get_static_pad (sink, "sink");

  g_signal_emit_by_name (sink, "pull-sample", &sample);

  if (sample) {
    GstSample *out_sample;
    out_sample =
        GST_SAMPLE_CAST (gst_mini_object_copy (GST_MINI_OBJECT_CONST_CAST
            (sample)));

    structure =
        gst_structure_new ("subtitle_data", "sample", GST_TYPE_SAMPLE,
        out_sample, NULL);

    gst_element_post_message (sink,
        gst_message_new_application (GST_OBJECT_CAST (sink), structure));

    gst_sample_unref (sample);
  }

  return GST_FLOW_OK;
}
