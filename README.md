# servicemp3 for GStreamer 1.26 – patches and notes

Analysis and patches for `servicemp3` (OpenPLi, base: branch python3-upstream, commit 125c795) running on GStreamer **1.26** inside an OE/Yocto build that still carries the 1.22.12 recipes (Scarthgap) and bumps them to 1.26.10 via bbappends. dvbmediasink (gst-1.0) and gst-plugin-subsink were read as well.

A German version of this document is in `README.de.md`.

## 1. Summary

1. **servicemp3 was already API-compatible with 1.26.** Both `servicemp3.cpp` and `servicemp3record.cpp` compile against the 1.26 headers without a single deprecation warning, and every playbin property/signal they use still exists. dvbmediasink and subsink showed no GStreamer deprecations in a syntax check either.
2. What needed attention are **latent bugs** and **HLS/DASH handling**. This branch contains 4 patches plus 1 optional patch.
3. **Under `playbin`, HLS/DASH/MSS are handled by the classic demuxers from gst-plugins-bad** (`hlsdemux`, `dashdemux`, `mssdemux`). The newer `adaptivedemux2` plugin from gst-plugins-good cannot work under `playbin` and is not needed by servicemp3.
4. Build status: reported to build through in a Yocto build with all patches applied. On-device testing is still open (see section 6).

## 2. How this was checked, and its limits

| What | How |
|---|---|
| GStreamer behaviour | Real GStreamer **1.26.11** (conda-forge, x86_64) in a sandbox, plus the **1.26.10** sources |
| Compiling servicemp3 | `g++ -fsyntax-only -Wall` against the 1.26 headers, enigma2 headers from OpenPLi master, libdvbsi++ headers |
| Media | A local HLS stream (MPEG-TS, Opus) and an MKV (Vorbis + SRT track) |

**Not verified in the sandbox:** behaviour on a box, the real dvbmediasink/subsink interplay, recording of HLS (`servicemp3record`), TLS error behaviour, any container other than HLS-TS and MKV (those rely on code reading only). The compile check is a syntax check of servicemp3, not a link or run test.

## 3. Patch series (`patches/servicemp3/`)

Apply with `git am patches/servicemp3/*.patch` on top of commit 125c795 (verified: applies cleanly, builds `-Wall` clean).

| # | Subject | Problem | Evidence | Effect / risk |
|---|---|---|---|---|
| 0001 | Stream-list change detection | `hasChanges = … \|\| std::equal(…)` without negation (also present in upstream OpenPLi python3) | Test with the real `audioStream` struct: old expression decides 2 of 3 scenarios wrongly, the new one none | An unchanged list no longer triggers `evUpdatedInfo` on every `ASYNC_DONE` (e.g. after each seek); changed languages with the same stream count are now detected. **Behaviour change – verify on a box.** |
| 0002 | Image/attachment tags | `getInfoObject` read `image`, `preview-image`, `attachment` with `gst_value_get_buffer`; these tags hold a `GstSample` | Test against 1.26.11: `gst_buffer_map` fails (CRITICAL), `bufferData`/`bufferSize` stayed uninitialised and were passed to Python | Now uses `gst_value_get_sample`/`gst_sample_get_buffer`; `setBuffer` keeps the buffer only if mapping succeeds |
| 0003 | Leaks, NULL guards, `-Wall` | Caps/strings not freed (subsink caps, recorder, missing-plugin), missing `gst_toc_unref`, NULL playbin in destructor, NULL audio pad | Code analysis, `-Wall` before/after | No functional change intended |
| 0004 | HLS/DASH detection | Type was derived from `strrchr(filename,'.')`, so `…/stream.m3u8?token=x` became `.m3u8?token=x` and `is_hls` stayed false | Unit test over 10 URL/path forms: old 6 wrong, new 0; demuxer names observed in a real run | `.mpd` recognised as DASH; additionally detected at runtime via `deep-element-added` (`hlsdemux*`/`dashdemux*`/`mssdemux*`), so the buffering exception for adaptive streams also applies to URLs without a telling extension. Side effect: `http://h/get?file=a.m3u8` (dot only in the query) is no longer HLS by extension, but is caught at runtime. |

### Optional (`patches/servicemp3-optional/`)

**0001-OPTIONAL connection-speed 4495:** playbin's `connection-speed` is specified in **kbit/s**. The existing value `4495000` is 4.5 Gbit/s and is clamped by the demuxer to its maximum 4294967 (measured: playbin 4495 → hlsdemux 4495; playbin 4495000 → hlsdemux 4294967). Effectively the demuxer always picks the highest variant. Whether that is intended cannot be judged from the code. The patch changes the value to 4495 and therefore the initial variant choice. **Apply only if 4.5 Mbit/s was the intent.**

## 4. Findings without a code patch

* **playbin vs. adaptivedemux2 (verified).** In 1.26.10, `hlsdemux2`/`dashdemux2`/`mssdemux2` (rank primary+1) live in gst-plugins-good. They check `GST_BIN_FLAG_STREAMS_AWARE` on their parent. `playbin` uses the non-streams-aware `decodebin`, so they fail with "Element requires a streams-aware context" and GStreamer falls back to `hlsdemux`/`dashdemux`/`mssdemux` from -bad (observed in a real run). Consequently `dash hls smoothstreaming` in the -bad PACKAGECONFIG **must stay**.
* **User-Agent/cookies with HLS:** `playbinNotifySource` only affects the main source element. The `souphttpsrc` elements inside the classic `hlsdemux` receive User-Agent and cookies from the upstream `http-headers` event (the same mechanism adaptivedemux2 implements). Arbitrary `extra-headers` are **not** forwarded. Whether this differed under 1.22 was not checked.
* **Subtitles:** on 1.26 the text pad has caps `text/x-raw, format=pango-markup`, which `getSubtitleType` classifies as text. Nothing to change.
* **playbin3** would allow `adaptivedemux2`, but is a rewrite: `get-audio-pad`, `current-audio`, `n-audio`, `get-text-tags` etc. do not exist there (stream collections and select-streams events instead). Not something to attempt without hardware testing.
* **Recipe options 1.22 → 1.26:** all `-D…` options of the 1.22.12 recipes were compared with the 1.26.10 `meson_options.txt`. Gone in 1.26: `installed_tests` (only with PACKAGECONFIG `tests`), `kate` (the bbappend already removes it), `amrnb`/`amrwbdec` in -ugly (the bbappend blanks those PACKAGECONFIG entries), `examples` (not set). **No build breaker.**
* **Stale items in the bundled recipes:** `gst-plugin-subsink.bb` still references `gstreamer-0.10` paths, and the dvbmediasink recipe has a dead `aactranscode` path (the code has no AAC transcode). Relevant only if exactly these `.bb` files are used.
* **libsoup:** the bbappends force libsoup-2.4. In 1.26.10 the soup plugin `dlopen()`s libsoup at runtime (2 and 3 are supported). Keep only one major version in the image so the other is not picked up unintentionally (not tested).
* **Recording (`servicemp3record`):** with the classic demuxers, caps negotiation of `uridecodebin` with container caps for HLS may behave differently than before. Not tested.
* **Subsink buffer ownership:** subsink emits `new-buffer` with `gst_buffer_ref`, so the `unref` in servicemp3's container is correct.

## 5. Reducing the GStreamer footprint

### 5.1 What the sources say

* **dvbmediasink caps** (`gstdvbaudiosink.c` / `gstdvbvideosink.c`): the sinks accept compressed data directly. Audio: MPEG-1 L1/L2 (L3 and AAC only with `HAVE_MP3`, AAC only `profile=lc`), AC3, EAC3, DTS, LPCM, WMA, AMR, PCM (depending on `DVBMEDIASINK_CONFIG`). Video: MPEG-1/2 always; H.264/H.265/MPEG-4/WMV/VP6/VP8/VP9 depending on configuration. servicemp3 sets `GST_PLAY_FLAG_NATIVE_VIDEO`, so for these formats demuxers and parsers are enough; no software decoders.
* Software decoders are only needed for formats the sink does not accept. Which ones those are depends on `DVBMEDIASINK_CONFIG` per machine.
* servicemp3 sets `text-sink=subsink`, so playbin needs no text overlay and therefore no `pango`.
* **The servicemp3 recipe is the authoritative list.** Its RDEPENDS pin down what the plugin needs at runtime: base `alsa app audioconvert audiorate audioresample vorbis ogg opus playback rawparse subparse typefindfunctions videoconvertscale`, good `amrnb amrwbdec flac mpg123 soup …`, bad `autoconvert dash faad hls mpegpsdemux mpegtsdemux opusparse rtmp smoothstreaming videoparsersbad`, ugly `asf cdio dvdsub`. `adaptivedemux2` is not in it.

### 5.2 Measurements (1.26.11, x86_64, plugin file sizes – orders of magnitude only)

Removing individual plugins, then replaying HLS (ts/opus) and MKV (Vorbis + SRT):

| Removed | HLS | MKV + audio + SRT |
|---|---|---|
| nothing | OK | OK |
| adaptivedemux2 | OK | OK |
| pango | OK | OK |
| adaptivedemux2 + pango | OK | OK |
| **hls** / **soup** / **mpegtsdemux** (negative control) | **fails** | OK |
| **matroska** / **vorbis** (negative control) | OK | **fails** |
| 131 plugins of a candidate "drop" class | OK | OK |

Caveats: the test media cover only two formats. The 131-plugin run was done **before** cross-checking against the servicemp3 recipe; the removed set included `app`, `audiorate` and `autoconvert`, which the recipe does require. The result is valid for the test media, but the recipe's package list, not that experiment, defines the real minimum.

Size distribution of the 1.26.11 conda set (178 plugins, 19.2 MB, built without e.g. ugly/libav) is **not** a measure for a real image. Savings, if any, are mostly in dependencies (pango, cairo, gdk-pixbuf, librsvg incl. Rust, curl, bluez/sbc, …). Measure with `buildhistory` (`installed-package-sizes.txt`) or `oe-pkgdata-util`.

### 5.3 Classification

**Required by evidence (code / recipe / test)**

| Plugin(s) | Reason |
|---|---|
| coreelements, playback, typefindfunctions, pbtypes | playbin, decodebin, queue2, filesrc |
| soup (good) | HTTP(S) source, HLS segments |
| hls, dash, smoothstreaming (bad) | HLS/DASH/MSS under playbin (see section 4) |
| mpegtsdemux (bad) | TS, HLS-TS |
| mpegpsdemux, videoparsersbad (bad) | .mpg/.vob/.dat; parsers the sinks require (`parsed=true`) |
| matroska, isomp4, avi, flv, wavparse, flac, id3demux, apetag, icydemux, audioparsers (good) | extension list of `eServiceFactoryMP3`, internet radio (icydemux) |
| amrnb, amrwbdec, mpg123 (good) | listed in the servicemp3 recipe |
| rtsp, rtp, rtpmanager, udp (good) | RTSP path in the code |
| ogg, vorbis, opus, opusparse, subparse | Ogg/Opus; subparse for external .srt/.ass (`suburi` path) |
| app, alsa, audioconvert, audiorate, audioresample, rawparse, videoconvertscale (base); autoconvert (bad) | listed in the servicemp3 recipe / `gstreamer1.0-meta-base` |
| rtmp (bad, librtmp) | PLi patches 001/003 target `ext/rtmp` |
| asf, cdio, dvdsub (ugly) | listed in the servicemp3 recipe |

**Depends on your configuration**

faad (AAC other than LC; in the recipe), libav (format dependent, not reducible without a machine list), vpx (VP8/VP9 only native with `HAVE_VB8/VB9`), jpeg (MJPEG), alaw/mulaw (tiny), multipart (MJPEG/IP cameras), gio, autodetect (in meta-base), theora, rm (only for .rm/.rmvb).

**Candidates not used by servicemp3 playback (if present in the image)**

| Group | Rationale |
|---|---|
| **adaptivedemux2** (good) | never works under playbin (test and source) |
| pango, closedcaption, ttml, rsvg, cairo, gdk-pixbuf | overlay/image/render plugins, unused with `text-sink=subsink`; servicemp3 cannot render TTML subtitles anyway |
| curl, dtls, faac, lame, sbc, sndfile, speex, taglib, bz2 (bad), uvch264, webp, png | encoders/sinks/WebRTC/camera/Bluetooth, not part of playback |
| v4l2, gudev | V4L2 camera/codecs, unused on dvbmediasink boxes |
| opengl, ximagesink, compositor, adder, videorate, audiotestsrc, videotestsrc, jack, pulseaudio, oss*, effectv, goom*, smpte, videobox, … and most of plugins-bad | not part of servicemp3's playback chain |
| ugly: a52dec, mpeg2dec | the sinks take MPEG-1/2 video and AC3 natively; only drop them if your sink build does not disable that |

### 5.4 Applying it

* **Option A, build level** (`patches/gstreamer-recipes-optional/`): changes the bad/base/good bbappends and disables adaptivedemux2 with `-Dadaptivedemux2=disabled`. Apply inside the `gstreamer_pli_26` directory with `patch -p1 < …patch` (dry run passes). Every package listed in the servicemp3 recipe's RDEPENDS is still built with this patch (amrnb, amrwb, flac, mpg123, faad, rtmp, alsa, cdio were kept on purpose). It is only safe if nothing else in the image requires packages such as `-good-v4l2`, `-bad-closedcaption` or `-base-pango`; check with `oe-pkgdata-util what-rdepends` first.
* **Option B, image level** (lower risk): OE splits one package per plugin (`gstreamer1.0-plugins-<module>-<plugin>`). Install only the required packages and leave the rest out. Minimal step with immediate effect: do not install `gstreamer1.0-plugins-good-adaptivedemux2`.
* **Expected effect on image size: possibly none.** The servicemp3 recipe already pulls in a tight list of packages, and plugins nobody requires are usually not installed in the first place. The build-level option mainly saves build time and build dependencies. Check `installed-package-sizes.txt` before expecting savings.

## 6. Test checklist for a box

1. Apply the series, build, then:
   * play an HLS URL **with** a query string (`…m3u8?token=…`): no stalls, no pause/resume at 0 %/100 % buffering;
   * play DASH (`.mpd`) (so far verified only by source reading);
   * switch between several audio/subtitle tracks and seek (0001: track lists must no longer be rebuilt on every seek, but must refresh when tracks change);
   * play an MP3 with cover art (0002).
2. Search the log for `CRITICAL`/`assertion`, and watch memory over a long playback (0003).
3. If you record IPTV, test a recording of an HLS stream (`servicemp3record`).
4. Optional patches only afterwards and one at a time: first `connection-speed`, then the recipe reduction (compare image size before/after, then run HLS, DASH, MKV, MP4, TS, internet radio, RTSP and external SRT).

## 7. Test tools (`tests/`)

`probe.c` (playbin run with demuxer output), `subprobe.c` (audio/text enumeration as in `ASYNC_DONE`), `t_sample.c` (GstSample test), `t_changes.cpp` (stream-change detection with the real servicemp3 structs), `t_ext.cpp` (URL classification), `redtest.sh` (plugin-reduction test). They need a GStreamer 1.26 development environment (and the enigma2 headers for `t_changes.cpp`). They are evidence, not part of the deliverable.
