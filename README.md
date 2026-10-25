# servicemp3 für GStreamer 1.26 – Patches und Begleitinformationen

Analysiert wurden: `servicemp3_pli` (Branch python3-upstream, HEAD 125c795), `gstreamer_oe-core_original` (1.22.12), `gstreamer_pli_26` (bbappends auf 1.26.10), `gst-plugin-dvbmediasink` (gst-1.0) und `gst-plugin-subsink`.

## 1. Kurzfassung

1. **servicemp3 ist API-seitig bereits 1.26-kompatibel.** Beide Quelldateien kompilieren gegen die 1.26-Header ohne eine einzige Deprecation-Warnung; alle benutzten playbin-Properties/-Signale existieren unverändert. Für dvbmediasink und subsink zeigte der Syntax-Check ebenfalls keine GStreamer-Deprecations (subsink nutzt nur das GLib-seitig veraltete `g_type_class_add_private`). Vollständig kompilieren ließen sie sich in der Sandbox nicht, weil dort die Dreambox/DVB-Kernel-Header (`AUDIO_GET_PTS`) und die generierten Marshaller fehlen; das sind keine 1.26-Probleme.
2. Der Anpassungsbedarf liegt in **latenten Fehlern** und im **Verhalten bei HLS/DASH**. Dafür gibt es 4 Patches plus 1 optionalen.
3. **HLS/DASH laufen unter `playbin` ausschließlich über die alten Demuxer aus plugins-bad.** `adaptivedemux2` (plugins-good) kann dort nie funktionieren und ist ohne Funktionsverlust entbehrlich.
4. **Reduktion:** Ein auf servicemp3 zugeschnittener Plugin-Satz ist möglich (Abschnitt 5). Mit einem auf 47 von 178 Plugins reduzierten Satz laufen meine Testmedien unverändert.

## 2. Testumgebung und Grenzen (bitte lesen)

| Was | Wie geprüft |
|---|---|
| GStreamer-Verhalten | Echtes GStreamer **1.26.11** (conda-forge, x86_64) in der Sandbox, plus Quellcode von **1.26.10** (GitHub-Mirror) |
| servicemp3 kompilieren | `g++ -fsyntax-only -Wall` gegen 1.26-Header, enigma2-Header von OpenPLi/master, libdvbsi++ (OpenVisionE2) |
| Medien | Lokaler HLS-Stream (MPEG-TS, Opus), MKV (Vorbis + SRT-Spur) |

**Nicht geprüft:** Lauf auf einer Box, echtes dvbmediasink/subsink-Zusammenspiel, bitbake-Build der Rezeptänderungen, Aufnahme von HLS (`servicemp3record`), Verhalten bei TLS-Fehlern, alle anderen Containerformate (hier gilt nur Code-Analyse). Der Kompilier-Check ist nur eine Syntaxprüfung, kein Link- oder Laufzeittest von servicemp3 selbst.

## 3. Patchserie (`patches/servicemp3/`)

Anwenden: `git am patches/servicemp3/*.patch` auf Stand 125c795 (getestet, läuft sauber durch; Ergebnis kompiliert `-Wall` ohne Warnung).

| Nr. | Thema | Problem | Beleg | Wirkung / Risiko |
|---|---|---|---|---|
| 0001 | Stream-Änderungserkennung | `hasChanges = … \|\| std::equal(…)` ohne Negation (steht auch im OpenPLi-Upstream) | Test mit den echten `audioStream`-Strukturen: alt entscheidet 2 von 3 Szenarien falsch, neu 0 | Unveränderte Liste löst nicht mehr bei jedem `ASYNC_DONE` (z. B. nach Seek) `evUpdatedInfo` aus; geänderte Sprachen bei gleicher Anzahl werden erkannt. **Verhaltensänderung, auf Box prüfen.** |
| 0002 | Cover/Anhang-Tags | `getInfoObject` liest `image`, `preview-image`, `attachment` mit `gst_value_get_buffer`; die Tags sind `GstSample` | Test gegen 1.26.11: `gst_buffer_map` scheitert (CRITICAL), `bufferData/bufferSize` blieben uninitialisiert und gingen an Python | Korrekt über `gst_value_get_sample`; `setBuffer` behält den Buffer nur bei erfolgreichem Mapping |
| 0003 | Leaks, NULL-Guards, `-Wall` | Caps/Strings nicht freigegeben (subsink-Caps, Recorder, Missing-Plugin), `gst_toc_unref` fehlte, NULL-Playbin im Destruktor, NULL-Audiopad | Code-Analyse, `-Wall` vorher/nachher | Keine funktionale Änderung beabsichtigt |
| 0004 | HLS/DASH-Erkennung | Typ wurde aus `strrchr(filename,'.')` abgeleitet: `…/stream.m3u8?token=x` ergab `.m3u8?token=x`, `is_hls` blieb false | Unit-Test über 10 URL/Pfad-Formen: alt 6 falsch, neu 0; Demuxer-Namen im echten Lauf bestätigt | `.mpd` wird als DASH erkannt; zusätzlich Erkennung zur Laufzeit über `deep-element-added` (`hlsdemux*`/`dashdemux*`/`mssdemux*`). Dadurch greift die Buffering-Ausnahme für adaptive Streams auch bei URLs ohne Endung. Nebenwirkung: `http://h/get?file=a.m3u8` (Punkt nur in der Query) gilt nicht mehr per Endung als HLS, wird aber zur Laufzeit erkannt. |

### Optional (`patches/servicemp3-optional/`)

**0001-OPTIONAL connection-speed 4495:** playbin-`connection-speed` ist in **kbit/s** spezifiziert. Der bestehende Wert `4495000` entspricht 4,5 Gbit/s und wird vom Demuxer auf sein Maximum 4294967 geklemmt (gemessen: playbin 4495 → hlsdemux 4495; playbin 4495000 → hlsdemux 4294967). Effektiv wählt der Demuxer also immer die höchste Variante. Ob das gewollt ist, kann ich nicht beurteilen. Der Patch ändert den Wert auf 4495 und damit die Variantenwahl beim Start. **Nur anwenden, wenn 4,5 Mbit/s beabsichtigt war.**

## 4. Befunde ohne Code-Patch

* **playbin vs. adaptivedemux2 (belegt).** In 1.26.10 liegen `hlsdemux2`/`dashdemux2`/`mssdemux2` (Rank primary+1) in plugins-good. Sie prüfen in `gst_adaptive_demux_change_state` das Flag `GST_BIN_FLAG_STREAMS_AWARE` ihres Parents. `playbin` nutzt den nicht streams-aware `decodebin`, daher: „Element requires a streams-aware context“, und GStreamer fällt auf `hlsdemux`/`dashdemux`/`mssdemux` aus plugins-bad zurück (im Lauf beobachtet). Folge: `dash hls smoothstreaming` in der bad-PACKAGECONFIG **müssen bleiben**.
* **User-Agent/Cookies bei HLS:** `playbinNotifySource` wirkt nur auf das Haupt-Source-Element. Die inneren `souphttpsrc` des alten `hlsdemux` bekommen User-Agent und Cookies aus dem Upstream-`http-headers`-Event (so ist es auch in adaptivedemux2 implementiert). Beliebige `extra-headers` werden **nicht** weitergereicht. Ob das unter 1.22 anders war, habe ich nicht geprüft.
* **Untertitel:** Unter 1.26 liefert die Textspur Caps `text/x-raw, format=pango-markup`; `getSubtitleType` ordnet das als Text ein. Hier ist nichts zu ändern.
* **playbin3** würde HLS/DASH modernisieren (adaptivedemux2 wäre dann nutzbar), ist aber ein Umbau: `get-audio-pad`, `current-audio`, `n-audio` etc. gibt es dort nicht (Stream-Collections statt dessen). Nicht ohne Hardware-Tests angehen.
* **Rezept-Optionen 1.22 → 1.26:** Ich habe alle `-D…`-Optionen Ihrer 1.22.12-Rezepte gegen die `meson_options.txt` von 1.26.10 abgeglichen. In 1.26 entfallen: `installed_tests` (nur bei PACKAGECONFIG `tests`), `kate` (Ihr bbappend entfernt es bereits), `amrnb`/`amrwbdec` in ugly (Ihr bbappend leert die PACKAGECONFIG-Einträge), `examples` (nicht gesetzt). **Kein Build-Breaker.**
* **Altlasten in den mitgelieferten Rezepten:** `gst-plugin-subsink.bb` verweist noch auf `gstreamer-0.10`-Pfade, im dvbmediasink-Rezept steht ein toter `aactranscode`-Pfad (im Code gibt es keinen AAC-Transcode). Nur relevant, falls genau diese `.bb` in Ihrer Build-Umgebung benutzt wird.
* **libsoup:** Die bbappends erzwingen libsoup-2.4. In 1.26.10 lädt das soup-Plugin libsoup per `dlopen` zur Laufzeit; 2 und 3 werden unterstützt. Im Image sollte nur eine Hauptversion liegen, damit nicht unbeabsichtigt die andere geladen wird (nicht getestet).
* **Aufnahme (`servicemp3record`):** Mit den alten Demuxern kann sich die Caps-Aushandlung des `uridecodebin` mit Container-Caps für HLS anders verhalten. Nicht geprüft.

## 5. Reduktion von GStreamer

### 5.1 Was die Quellen sagen

* **dvbmediasink-Caps** (aus `gstdvbaudiosink.c`/`gstdvbvideosink.c`): Die Sinks nehmen komprimierte Daten direkt an – Audio: MPEG-1 L1/L2 (L3 und AAC nur mit `HAVE_MP3`, AAC nur `profile=lc`), AC3, EAC3, DTS, LPCM, WMA, AMR, PCM (je nach `DVBMEDIASINK_CONFIG`); Video: MPEG-1/2 immer, H.264/H.265/MPEG4/WMV/VP6/VP8/VP9/… je nach Konfiguration. servicemp3 setzt `GST_PLAY_FLAG_NATIVE_VIDEO`; für diese Formate genügen **Demuxer und Parser**, keine Software-Decoder.
* **Software-Decoder** werden nur für Formate gebraucht, die der Sink nicht annimmt (z. B. Vorbis, Opus, FLAC, Theora, AAC außer LC, Mehrkanal-PCM, nicht aktivierte Videocodecs). Welche das auf Ihren Maschinen sind, hängt von `DVBMEDIASINK_CONFIG` pro Box ab – **das kenne ich nicht**.
* Textspur: servicemp3 setzt `text-sink=subsink`. Damit braucht playbin kein Text-Overlay, also kein `pango`.

### 5.2 Messungen (1.26.11, x86_64, Plugin-Dateigrößen, nur Größenordnung)

Entfernen einzelner Plugins, danach HLS (ts/opus) und MKV (Vorbis+SRT) erneut abspielen:

| Entfernt | HLS | MKV+Audio+SRT |
|---|---|---|
| nichts | OK | OK |
| adaptivedemux2 | OK | OK |
| pango | OK | OK |
| adaptivedemux2 + pango | OK | OK |
| **hls** / **soup** / **mpegtsdemux** (Negativkontrolle) | **fällt aus** | OK |
| **matroska** / **vorbis** (Negativkontrolle) | OK | **fällt aus** |
| alle 131 Plugins der Kategorie DROP | OK | OK |

Die Testmedien decken nur zwei Formate ab; für alle anderen stützt sich die Einstufung auf Code und Quellen (nicht auf Laufzeittests).

Die Größenverteilung des 1.26.11-Sets (178 Plugins, 19,2 MB; Conda-Build, unter anderem ohne ugly/libav): KEEP 37 Plugins / 7,7 MB, COND 10 / 0,8 MB, DROP 131 / 10,8 MB. **Die Zahlen sind kein Maß für Ihr Image:** Der größere Gewinn liegt in den Abhängigkeiten (pango, cairo, gdk-pixbuf, librsvg inkl. Rust, curl, bluez/sbc …), deren Größe ich nicht messen konnte. Messen Sie mit `buildhistory` bzw. `oe-pkgdata-util`.

### 5.3 Einstufung

**KEEP (Code/Quellen/Test belegen die Notwendigkeit)**

| Plugin(s) | Grund |
|---|---|
| coreelements, playback, typefindfunctions, pbtypes | playbin, decodebin, queue2, filesrc |
| soup (good) | HTTP(S)-Quelle, HLS-Segmente |
| hls, dash, smoothstreaming (bad) | HLS/DASH/MSS unter playbin (siehe 4.) |
| mpegtsdemux (bad) | TS, HLS-TS |
| mpegpsdemux, videoparsersbad (bad) | .mpg/.vob/.dat; Parser, die die Sinks (`parsed=true`) verlangen |
| matroska, isomp4, avi, flv, wavparse, flac, id3demux, apetag, icydemux, audioparsers, auparse (good) | Erweiterungsliste von `eServiceFactoryMP3` bzw. Internetradio (icydemux) |
| rtsp, rtp, rtpmanager, udp (good) | RTSP-Pfad im Code (rtsp-Header) |
| ogg, vorbis, opus, opusparse, subparse | Ogg/Opus; subparse für externe .srt/.ass (Pfad `suburi`) |
| audioconvert, audioresample, volume, videoconvertscale | in `gstreamer1.0-meta-base` enthalten |
| rtmp (bad, librtmp) | PLi-Patches 001/003 betreffen `ext/rtmp` |
| ugly: asf (wma/wmv), rm (nur falls .rm/.rmvb), dvdlpcm/dvdsub (VOB) | Erweiterungsliste |

**COND (abhängig von Ihrer Konfiguration)**

alsa (nur falls PCM über ALSA statt dvbaudiosink läuft), mpg123 (MP3 nur nativ mit `HAVE_MP3`), vpx (VP8/VP9 nur nativ mit `HAVE_VB8/VB9`), jpeg (MJPEG), amr/alaw/mulaw (winzig), multipart (MJPEG/IP-Kameras), gio, autodetect (in meta-base), faad (AAC außer LC), libav (formatabhängig, nicht reduzierbar ohne Maschinenliste), cdio (.cda), theora.

**DROP-Kandidaten (für servicemp3-Wiedergabe ohne Nutzung)**

| Gruppe | Begründung |
|---|---|
| **adaptivedemux2** (good) | läuft unter playbin nie (Test und Quellcode) |
| pango, closedcaption, ttml, rsvg, cairo, gdk-pixbuf | Overlay/Bild/Render-Plugins; mit `text-sink=subsink` ungenutzt. TTML-Untertitel kann servicemp3 ohnehin nicht darstellen |
| curl, dtls, faac, lame, sbc, sndfile, speex, taglib, bz2 (bad), uvch264, webp, png | Encoder/Sinks/WebRTC/Kamera/Bluetooth, nicht Teil der Wiedergabe |
| v4l2, gudev | V4L2-Kamera/-Codecs, auf dvbmediasink-Boxen ungenutzt |
| opengl, ximagesink, compositor, adder, audiomixer, videorate, audiotestsrc, videotestsrc, jack, pulseaudio, oss*, effectv, goom*, smpte, videobox, … und der Großteil von plugins-bad | keine Wiedergabekette von servicemp3 |
| ugly: a52dec, mpeg2dec | MPEG-1/2-Video und AC3 nehmen die Sinks nativ an; nur streichen, wenn Ihr Sink-Build das nicht abschaltet |

### 5.4 Umsetzung

* **Option A, Build-Ebene** (`patches/gstreamer-recipes-optional/`): ändert die drei bbappends (bad, base, good) und schaltet adaptivedemux2 per `-Dadaptivedemux2=disabled` ab. Anwenden im Verzeichnis `gstreamer_pli_26`: `patch -p1 < …patch` (Trockenlauf bestanden). **Kein bitbake-Lauf erfolgt.** Die entfernten PACKAGECONFIGs sind nur sicher, wenn nichts anderes in Ihrem Image Pakete wie `-good-v4l2`, `-bad-closedcaption` oder `-base-pango` per RDEPENDS verlangt. Prüfen Sie das vorher mit `oe-pkgdata-util what-rdepends`.
* **Option B, Image-Ebene** (risikoärmer): OE splittet pro Plugin ein Paket (`gstreamer1.0-plugins-<modul>-<plugin>`). Statt die Rezepte zu ändern, installieren Sie nur die KEEP/COND-Pakete in Ihrem Image und lassen alles andere weg. Minimalschritt mit sofortigem Nutzen: `gstreamer1.0-plugins-good-adaptivedemux2` nicht installieren.
* ugly-bbappend habe ich bewusst nicht angefasst (nur Empfehlung oben), da die Entscheidung von der Sink-Konfiguration abhängt.

## 6. Prüfliste für die Box

1. `git am` der Serie, bauen, dann: HLS-URL **mit** Query-String abspielen, DASH (.mpd) abspielen, MKV mit mehreren Audio-/Untertitelspuren umschalten und seeken (0001: kein Flackern/Refresh der Spurlisten mehr).
2. Eine MP3 mit Cover-Art abspielen (0002).
3. Im Log nach `CRITICAL`/`assertion` suchen und nach längerer Wiedergabe den Speicherverbrauch beobachten (0003).
4. Bei der optionalen Rezeptänderung: Rezepte bauen, Image-Größe vorher/nachher, dann HLS, DASH, MKV, MP4, TS, Internetradio, RTSP und externe SRT durchprobieren.

## 7. Enthaltene Testwerkzeuge (`tests/`)

`probe.c` (playbin-Lauf mit Demuxer-Ausgabe), `subprobe.c` (Audio-/Text-Enumeration wie in `ASYNC_DONE`), `t_sample.c` (GstSample-Test), `t_changes.cpp` (Stream-Änderungserkennung mit echten servicemp3-Strukturen), `t_ext.cpp` (URL-Erkennung), `redtest.sh` (Plugin-Reduktionstest). Sie brauchen ein GStreamer-1.26-Entwicklungsumfeld (und für `t_changes.cpp` die enigma2-Header) und sind Belege, kein Teil der Auslieferung.
