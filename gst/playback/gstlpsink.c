/* GStreamer Lightweight Playback Plugins
 * Copyright (C) 2013 LG Electronics.
 *	Author : Justin Joy <justin.joy.9to5@gmail.com> 
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
static GstStaticPadTemplate texttemplate =
GST_STATIC_PAD_TEMPLATE ("text_sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

/* props */
enum
{
  PROP_0,
  PROP_VIDEO_SINK,
  PROP_AUDIO_SINK,
  PROP_LAST
};
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
   * Since: 0.10.36
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
   * Since: 0.10.36
   */
  g_object_class_install_property (gobject_klass, PROP_AUDIO_SINK,
      g_param_spec_object ("audio-sink", "Audio Sink",
          "the audio output element to use (NULL = default sink)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&audiotemplate));
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&videotemplate));
  gst_element_class_add_pad_template (gstelement_klass,
      gst_static_pad_template_get (&texttemplate));
  gst_element_class_set_static_metadata (gstelement_klass,
      "Lightweight Player Sink", "Lightweight/Bin/Sink",
      "Convenience sink for multiple streams in a restricted system",
      "Justin Joy <justin.joy.9to5@gmail.com>");

  gstelement_klass->change_state = GST_DEBUG_FUNCPTR (gst_lp_sink_change_state);

  gstelement_klass->send_event = GST_DEBUG_FUNCPTR (gst_lp_sink_send_event);
  gstelement_klass->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_lp_sink_request_new_pad);
  gstelement_klass->release_pad =
      GST_DEBUG_FUNCPTR (gst_lp_sink_release_request_pad);

  gstbin_klass->handle_message = GST_DEBUG_FUNCPTR (gst_lp_sink_handle_message);

  // klass->reconfigure = GST_DEBUG_FUNCPTR (gst_lp_sink_reconfigure);
//  klass->convert_sample = GST_DEBUG_FUNCPTR (gst_lp_sink_convert_sample);

}

static void
gst_lp_sink_init (GstLpSink * lpsink)
{
  g_rec_mutex_init (&lpsink->lock);
  lpsink->audio_sink = NULL;
  lpsink->video_sink = NULL;
  lpsink->video_pad = NULL;
  lpsink->audio_pad = NULL;
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

  lpsink->video_pad = NULL;
  lpsink->audio_pad = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (obj);
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
  GstPad *sinkpad;
  const gchar *sink_name;
  const gchar *pad_name;
  GstElement *sink_element;

  //GST_LP_SINK_LOCK (lpsink);

  switch (type) {
    case GST_LP_SINK_TYPE_AUDIO:
      sink_name = "adecsink";
      pad_name = "audio_sink";

      break;
    case GST_LP_SINK_TYPE_VIDEO:
      sink_name = "vdecsink";
      pad_name = "video_sink";
      break;
    default:
      res = NULL;
  }

  sink_element = gst_element_factory_make (sink_name, NULL);

  if (!sink_element) {
    /*post_missing_element_message (lpsink, "adecsink");
       GST_ELEMENT_ERROR (lpsink, CORE, MISSING_PLUGIN,
       (_("Missing element '%s' - check your Gstreamer Installation."), "adecsink"), NULL);
       res = NULL; */
    goto beach;
  }

  gst_bin_add (GST_BIN_CAST (lpsink), sink_element);
  gst_element_set_state (sink_element, GST_STATE_PAUSED);
  res = gst_ghost_pad_new_no_target (pad_name, GST_PAD_SINK);

  if (type == GST_LP_SINK_TYPE_AUDIO) {
    lpsink->audio_sink = sink_element;
    lpsink->audio_pad = res;
  } else if (type == GST_LP_SINK_TYPE_VIDEO) {
    lpsink->video_sink = sink_element;
    lpsink->video_pad = res;
  }

  gst_pad_set_active (res, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (lpsink), res);

  sinkpad = gst_element_get_static_pad (sink_element, "sink");
  gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (res), sinkpad);
  gst_object_unref (sinkpad);
beach:

  //GST_LP_SINK_UNLOCK (lpsink);

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

  GST_DEBUG_OBJECT (lpsink, "release pad %" GST_PTR_FORMAT, pad);

  GST_LP_SINK_LOCK (lpsink);

  GST_LP_SINK_UNLOCK (lpsink);
  if (*res) {
    GST_DEBUG_OBJECT (lpsink, "deactivate pad %" GST_PTR_FORMAT, *res);
    // TODO
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

  if (lpsink->video_sink) {
    gst_event_ref (event);
    if ((res = gst_element_send_event (lpsink->video_sink, event))) {
      GST_DEBUG_OBJECT (lpsink, "Sent event successfully to video sink");
      goto done;
    }
    GST_DEBUG_OBJECT (lpsink, "Event failed when sent to video sink");
  }
  if (lpsink->audio_sink) {
    gst_event_ref (event);
    if ((res = gst_element_send_event (lpsink->audio_sink, event))) {
      GST_DEBUG_OBJECT (lpsink, "Sent event successfully to audio sink");
      goto done;
    }
    GST_DEBUG_OBJECT (lpsink, "Event failed when sent to audio sink");
  }

done:
  gst_event_unref (event);
  return res;
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
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (lpsink->audio_pad != NULL)
        gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (lpsink->audio_pad), NULL);
      if (lpsink->video_pad != NULL)
        gst_ghost_pad_set_target (GST_GHOST_PAD_CAST (lpsink->video_pad), NULL);
      if (lpsink->audio_sink != NULL) {
        gst_element_set_state (lpsink->audio_sink, GST_STATE_NULL);
        gst_bin_remove (GST_BIN_CAST (lpsink), lpsink->audio_sink);
        //g_assert (lpsink->audio_sink != NULL);
        lpsink->audio_sink = NULL;
      }
      if (lpsink->video_sink != NULL) {
        gst_element_set_state (lpsink->video_sink, GST_STATE_NULL);
        gst_bin_remove (GST_BIN_CAST (lpsink), lpsink->video_sink);
        //g_assert (lpsink->video_sink != NULL);
        lpsink->video_sink = NULL;
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
