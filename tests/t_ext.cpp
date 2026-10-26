#include <string>
#include <cstring>
#include <cstdio>
#include <strings.h>
static std::string getTypeExtension(const char *filename)
{
	std::string path(filename);
	if (path.find("://") != std::string::npos)
	{
		size_t q = path.find_first_of("?#");
		if (q != std::string::npos)
			path.erase(q);
	}
	size_t slash = path.find_last_of('/');
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return "";
	return path.substr(dot);
}
static const char *OLD(const char *f){ const char *e = strrchr(f,'.'); return e ? e : f+strlen(f); }
int main(){
  struct { const char *url; const char *want; } t[] = {
    {"http://h/live/stream.m3u8?token=abc.def",".m3u8"},{"http://h/live/stream.m3u8",".m3u8"},{"https://h/a/b.mpd?x=1#frag",".mpd"},
    {"http://h:8000/live/user/pass/123",""},{"http://h/play?file=a.m3u8",""},{"http://h.example.com/stream",""},
    {"/media/hdd/movie/film.mkv",".mkv"},{"/media/dir.d/film",""},{"/media/a b/Film.Name.MP4",".MP4"},{"http://h/x.mp4?sig=1",".mp4"} };
  int bad=0, oldbad=0;
  for (auto &c : t) { std::string w = c.want; if (w.size()>5 && (unsigned char)w[0]==0xE2) w = w.substr(3);
    std::string n = getTypeExtension(c.url); const char *o = OLD(c.url);
    bool okn = (n==w); bool oko = (strcasecmp(o, w.c_str())==0) && w.size();
    if (!w.size()) oko = (strchr(o,'/')==NULL && strlen(o)==0);
    bad += !okn; oldbad += !oko;
    printf("%-44s neu=%-8s %s   alt=%-18s %s\n", c.url, n.c_str(), okn?"ok":"FALSCH", o, oko?"ok":"FALSCH"); }
  printf("\nFehler: neu=%d alt=%d\n", bad, oldbad); return bad; }
