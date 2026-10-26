/* Minimal probe: mimics servicemp3's playbin usage against a real GStreamer 1.26 runtime. */
#include <gst/gst.h>
#include <string.h>
static void deep_added(GstBin *b, GstBin *sub, GstElement *e, gpointer u)
{
    GstElementFactory *f = gst_element_get_factory(e);
    const char *n = f ? gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(f)) : "-";
    if (strstr(n, "demux") || strstr(n, "soup") || strstr(n, "queue2") || strstr(n, "multiqueue") || strstr(n, "decodebin") || strstr(n, "parse"))
        g_print("  deep-element-added: %-22s factory=%s\n", GST_ELEMENT_NAME(e), n);
}
static void src_setup(GstElement *pb, GstElement *src, gpointer u)
{
    GstElementFactory *f = gst_element_get_factory(src);
    g_print("  source-setup: %s\n", f ? gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(f)) : "?");
}
int main(int argc, char **argv)
{
    gst_init(&argc, &argv);
    GstElement *pb = gst_element_factory_make("playbin", "playbin");
    GstElement *as = gst_element_factory_make("fakesink", NULL), *vs = gst_element_factory_make("fakesink", NULL);
    g_object_set(as, "sync", TRUE, NULL);
    g_object_set(pb, "uri", argv[1], "audio-sink", as, "video-sink", vs,
                 "flags", 0x1 | 0x2 | 0x4 | 0x40 | 0x100, /* video audio text native-video buffering */
                 "buffer-duration", (gint64)(5 * GST_SECOND), "buffer-size", 5 * 1024 * 1024, NULL);
    if (argc > 2) g_object_set(pb, "connection-speed", (guint64)atoi(argv[2]), NULL);
    g_signal_connect(pb, "deep-element-added", G_CALLBACK(deep_added), NULL);
    g_signal_connect(pb, "source-setup", G_CALLBACK(src_setup), NULL);
    GstBus *bus = gst_element_get_bus(pb);
    GstStateChangeReturn r = gst_element_set_state(pb, GST_STATE_PLAYING);
    g_print("set_state(PLAYING) -> %s\n", gst_element_state_change_return_get_name(r));
    gint64 end = g_get_monotonic_time() + 12 * G_USEC_PER_SEC; int nbuf = 0, done = 0;
    while (g_get_monotonic_time() < end) {
        GstMessage *m = gst_bus_timed_pop(bus, 200 * GST_MSECOND);
        if (!m) continue;
        switch (GST_MESSAGE_TYPE(m)) {
        case GST_MESSAGE_BUFFERING: { gint p; gst_message_parse_buffering(m, &p); if (nbuf++ < 6) g_print("  BUFFERING %d%% (from %s)\n", p, GST_OBJECT_NAME(m->src)); break; }
        case GST_MESSAGE_ASYNC_DONE: if (m->src == GST_OBJECT(pb)) { gint a, t, v; g_object_get(pb, "n-audio", &a, "n-text", &t, "n-video", &v, NULL); g_print("  ASYNC_DONE  video=%d audio=%d text=%d\n", v, a, t); } break;
        case GST_MESSAGE_ERROR: { GError *e; gchar *d; gst_message_parse_error(m, &e, &d); g_print("  ERROR from %s: %s\n", GST_OBJECT_NAME(m->src), e->message); g_error_free(e); g_free(d); done = 1; break; }
        case GST_MESSAGE_EOS: g_print("  EOS\n"); done = 1; break;
        default: break; }
        gst_message_unref(m); if (done) break;
    }
    gst_element_set_state(pb, GST_STATE_NULL);
    gst_object_unref(bus); gst_object_unref(pb); return 0; }
