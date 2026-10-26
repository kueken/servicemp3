#include <gst/gst.h>
static int crit = 0;
static void handler(const gchar *d, GLogLevelFlags l, const gchar *m, gpointer u){ crit++; g_print("   [GLib %s] %s\n", (l & G_LOG_LEVEL_CRITICAL) ? "CRITICAL" : "WARNING", m); }
int main(int argc, char **argv){
  gst_init(&argc,&argv);
  g_log_set_handler("GStreamer", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, handler, NULL);
  g_log_set_handler("GLib-GObject", G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING, handler, NULL);
  GstTagList *tl = gst_tag_list_new_empty();
  GstBuffer *b = gst_buffer_new_allocate(NULL, 16, NULL); gst_buffer_memset(b, 0, 0xAB, 16);
  GstSample *s = gst_sample_new(b, NULL, NULL, NULL);
  const char *tags[] = { GST_TAG_IMAGE, GST_TAG_PREVIEW_IMAGE, GST_TAG_ATTACHMENT };
  for (int i = 0; i < 3; i++) { gst_tag_list_add(tl, GST_TAG_MERGE_APPEND, tags[i], s, NULL);
    const GValue *gv = gst_tag_list_get_value_index(tl, tags[i], 0);
    printf("%-14s GValue type: %s  holds-sample=%d\n", tags[i], G_VALUE_TYPE_NAME(gv), GST_VALUE_HOLDS_SAMPLE(gv)); }
  const GValue *gv = gst_tag_list_get_value_index(tl, GST_TAG_IMAGE, 0);
  printf("\n== ALTER Code: gst_value_get_buffer(gv)\n"); GstBuffer *old = gst_value_get_buffer(gv);
  printf("   Ergebnis: %p\n", (void*)old);
  GstMapInfo map; int ok = gst_buffer_map(old, &map, GST_MAP_READ); printf("   gst_buffer_map(NULL) -> %d (map.data bleibt unbelegt)\n", ok);
  printf("\n== NEUER Code: gst_value_get_sample + gst_sample_get_buffer\n"); crit = 0;
  GstSample *smp = gst_value_get_sample(gv); GstBuffer *nb = smp ? gst_sample_get_buffer(smp) : NULL;
  ok = nb ? gst_buffer_map(nb, &map, GST_MAP_READ) : 0; printf("   Buffer=%p map=%d size=%zu first-byte=0x%02X criticals=%d\n", (void*)nb, ok, ok ? map.size : 0, ok ? map.data[0] : 0, crit);
  return 0; }
