/* Replays servicemp3's ASYNC_DONE enumeration (audio caps/tags, text tags/pad caps) on a real 1.26 playbin. */
#include <gst/gst.h>
#include <string.h>
static void dump(const char *tag, GstCaps *c){ gchar *s = c ? gst_caps_to_string(c) : NULL; g_print("    %-18s %s\n", tag, s ? s : "(null)"); g_free(s); }
int main(int argc, char **argv)
{
    gst_init(&argc, &argv);
    GstElement *pb = gst_element_factory_make("playbin", "playbin");
    GstElement *as = gst_element_factory_make("fakesink", NULL), *vs = gst_element_factory_make("fakesink", NULL), *ts = gst_element_factory_make("fakesink", "subtitle_sink");
    g_object_set(pb, "uri", argv[1], "audio-sink", as, "video-sink", vs, "text-sink", ts, "flags", 0x1|0x2|0x4|0x40, NULL);
    GstBus *bus = gst_element_get_bus(pb);
    gst_element_set_state(pb, GST_STATE_PLAYING);
    gint64 end = g_get_monotonic_time() + 6 * G_USEC_PER_SEC;
    while (g_get_monotonic_time() < end) {
        GstMessage *m = gst_bus_timed_pop(bus, 100 * GST_MSECOND); if (!m) continue;
        if (GST_MESSAGE_TYPE(m) == GST_MESSAGE_ASYNC_DONE && m->src == GST_OBJECT(pb)) {
            gint na = 0, nt = 0; g_object_get(pb, "n-audio", &na, "n-text", &nt, NULL);
            g_print("ASYNC_DONE n-audio=%d n-text=%d\n", na, nt);
            for (int i = 0; i < na; i++) { GstPad *p = NULL; GstTagList *t = NULL; gchar *codec = NULL, *lang = NULL;
                g_signal_emit_by_name(pb, "get-audio-pad", i, &p); GstCaps *c = gst_pad_get_current_caps(p); dump("audio caps", c);
                g_signal_emit_by_name(pb, "get-audio-tags", i, &t);
                if (t) { gst_tag_list_get_string(t, GST_TAG_AUDIO_CODEC, &codec); gst_tag_list_get_string(t, GST_TAG_LANGUAGE_CODE, &lang); gst_tag_list_unref(t); }
                g_print("    audio[%d] codec-tag=%s lang-tag=%s\n", i, codec ? codec : "(null)", lang ? lang : "(null)");
                if (c) gst_caps_unref(c); gst_object_unref(p); g_free(codec); g_free(lang); }
            for (int i = 0; i < nt; i++) { GstPad *p = NULL; GstTagList *t = NULL; gchar *codec = NULL, *lang = NULL;
                g_signal_emit_by_name(pb, "get-text-tags", i, &t);
                if (t) { gst_tag_list_get_string(t, GST_TAG_SUBTITLE_CODEC, &codec); gst_tag_list_get_string(t, GST_TAG_LANGUAGE_CODE, &lang); gst_tag_list_unref(t); }
                g_signal_emit_by_name(pb, "get-text-pad", i, &p);
                g_print("  text[%d] codec-tag=%s lang-tag=%s\n", i, codec ? codec : "(null)", lang ? lang : "(null)");
                if (p) { GstCaps *c = gst_pad_get_current_caps(p); dump("current caps", c); if (c) gst_caps_unref(c);
                         c = gst_pad_get_allowed_caps(p); dump("allowed caps", c); if (c) gst_caps_unref(c); gst_object_unref(p); }
                g_free(codec); g_free(lang); }
            break; }
        gst_message_unref(m); }
    gst_element_set_state(pb, GST_STATE_NULL); return 0; }
