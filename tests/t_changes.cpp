#include <servicemp3.h>
#include <cstdio>
#include <algorithm>
#include <vector>
typedef eServiceMP3::audioStream A;
static A mk(const char *lang, const char *codec) { A a; a.type = atAC3; a.language_code = lang; a.codec = codec; return a; }
template<class V> static bool oldExpr(V &cur, V &tmp) { return cur.size() != tmp.size() || std::equal(cur.begin(), cur.end(), tmp.begin()); }
template<class V> static bool newExpr(V &cur, V &tmp) { return cur.size() != tmp.size() || !std::equal(cur.begin(), cur.end(), tmp.begin()); }
int main()
{
	std::vector<A> cur = { mk("deu","AC3"), mk("eng","AC3") };
	std::vector<A> same = cur;
	std::vector<A> swapped = { mk("eng","AC3"), mk("deu","AC3") };   /* gleiche Anzahl, anderer Inhalt */
	std::vector<A> more = { mk("deu","AC3"), mk("eng","AC3"), mk("fra","AC3") };
	std::printf("%-44s %-12s %-12s %s\n", "Szenario", "alt", "neu", "erwartet");
	struct { const char *n; std::vector<A> *t; bool want; } c[] = {
		{ "Liste unveraendert (z.B. nach Seek)", &same, false },
		{ "gleiche Anzahl, Inhalt/Sprache anders", &swapped, true },
		{ "Anzahl geaendert", &more, true } };
	int bad_old = 0, bad_new = 0;
	for (auto &x : c) { bool o = oldExpr(cur, *x.t), n = newExpr(cur, *x.t); bad_old += (o != x.want); bad_new += (n != x.want);
		std::printf("%-44s %-12s %-12s %s %s\n", x.n, o ? "Aenderung" : "keine", n ? "Aenderung" : "keine", x.want ? "Aenderung" : "keine", (o != x.want) ? "<- alt FALSCH" : ""); }
	std::printf("\nFehlentscheidungen: alt=%d neu=%d\n", bad_old, bad_new);
	return bad_new;
}
