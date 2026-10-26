#!/bin/bash
# usage: redtest.sh "<plugin .so names to remove>"  -> plays HLS(ts/opus) and MKV(vorbis+srt) with the reduced plugin set
export GST_REGISTRY=/tmp/reg_red_$$.bin GST_PLUGIN_SCANNER=/tmp/gst126env/libexec/gstreamer-1.0/gst-plugin-scanner LD_LIBRARY_PATH=/tmp/gst126env/lib
D=/tmp/redset_$$; rm -rf $D; mkdir $D
for f in /tmp/gst126env/lib/gstreamer-1.0/*.so; do b=$(basename $f); skip=0; for r in $1; do [ "$b" = "$r" ] && skip=1; done; [ $skip = 0 ] && ln -s $f $D/$b; done
export GST_PLUGIN_SYSTEM_PATH=$D
hls=$(timeout 25 /tmp/harness/probe "http://127.0.0.1:8099/index.m3u8" 4495 2>&1 | grep -cE "ASYNC_DONE  video=0 audio=1")
mkv=$(timeout 20 /tmp/harness/subprobe file:///tmp/sub/t.mkv 2>&1 | grep -cE "n-audio=1 n-text=1")
echo "entfernt: ${1:-nichts}  =>  HLS: $([ $hls -ge 1 ] && echo OK || echo FEHLT)   MKV+Audio+SRT: $([ $mkv -ge 1 ] && echo OK || echo FEHLT)"
rm -rf $D /tmp/reg_red_$$.bin
