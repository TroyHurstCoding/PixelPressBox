// sim5_d.cpp - renders the batch 5 data screens (render5d.cpp) into out/D*.ppm
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "Arduino.h"
#include <cstdlib>
#include "../../src/desk.h"
#include "../../src/render.h"
#include "../../src/render5.h"
#include "../../src/feeds5.h"
#include "simdump.h"

Settings CFG;

__attribute__((weak)) PlayoffLine playoffLine(const StandGroup &g, const String &team) {
  PlayoffLine p;
  for (unsigned i = 0; i < g.rows.size(); i++)
    if (g.rows[i].abbr == team) {
      p.ok = true, p.rank = i + 1, p.seed = g.rows[i].seed, p.gb = g.rows[i].gb;
      p.record = String(g.rows[i].wins) + "-" + String(g.rows[i].losses);
      p.magic = i == 0 ? 4 : -1;
    }
  return p;
}

static Game mkGame(const char *id, const char *lg, char st, const char *a, uint32_t ac, int as, const char *h, uint32_t hc, int hs) {
  Game g;
  g.id = id, g.league = lg, g.state = st;
  g.away.abbr = a, g.away.color = ac, g.away.score = as, g.away.id = String("A") + id;
  g.home.abbr = h, g.home.color = hc, g.home.score = hs, g.home.id = String("H") + id;
  return g;
}

int main() {
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();
  displayBegin();
  time_t now = parseIsoUtc("2026-11-12T17:42:00");
  time_t real = time(nullptr);

  // launches
  Launch l;
  l.name = "Falcon 9 Block 5 | SDA Tranche 1";
  l.rocket = "Falcon 9"; l.mission = "SDA Tranche 1 Transport Layer";
  l.pad = "Space Launch Complex 4E, Vandenberg SFB"; l.provider = "SpaceX"; l.status = "Go";
  l.vandenberg = true;
  l.net = now + 2 * 3600 + 14 * 60 + 33;
  drawLaunch(l, now, 0); dump("D1_launch_go");
  drawLaunch(l, now, 50); dump("D1_launch_lookup");
  l.net = now + 3 * 86400 + 4 * 3600 + 12 * 60; l.status = "TBC"; l.vandenberg = false;
  l.rocket = "Starship"; l.mission = "Flight 12"; l.pad = "Starbase, Boca Chica";
  drawLaunch(l, now, 10); dump("D1_launch_days");
  l.net = now + 5; l.status = "Hold"; l.vandenberg = true;
  drawLaunch(l, now, 3); dump("D1_launch_hold");
  l.net = now - 4; l.status = "Go";
  drawLaunch(l, now, 6); dump("D1_launch_liftoff");
  l.net = now - 600;
  drawLaunch(l, now, 6); dump("D1_launch_tplus");

  // playoff
  StandGroup sg;
  sg.league = "nfl"; sg.name = "AFC East";
  auto row = [&](const char *a, int w, int ls, int seed, const char *gb) {
    StandRow r; r.abbr = a; r.wins = w; r.losses = ls; r.seed = seed; r.gb = gb; sg.rows.push_back(r);
  };
  row("BUF", 9, 3, 2, "-"); row("MIA", 7, 5, 6, "2"); row("NYJ", 4, 8, 0, "5"); row("NE", 3, 9, 0, "6");
  drawPlayoff(sg, "BUF", 0x00338D, 0); dump("D2_playoff_lead");
  drawPlayoff(sg, "NYJ", 0x125740, 0); dump("D2_playoff_chase");
  drawPlayoff(sg, "NE", 0x002244, 0); dump("D2_playoff_last");

  // park waits
  ParkWaits pw; pw.ok = true; pw.park = "DISNEYLAND";
  const char *rn[] = {"Star Wars: Rise of the Resistance", "Indiana Jones Adventure", "Space Mountain", "Big Thunder Mountain Railroad", "Matterhorn Bobsleds", "Pirates of the Caribbean", "Mr. Toad's Wild Ride", "Haunted Mansion"};
  int rw[] = {85, 65, 45, 35, 25, 20, 15, 10};
  for (int i = 0; i < 8; i++) { RideWait r; r.name = rn[i]; r.wait = rw[i]; r.open = true; pw.rides.push_back(r); }
  drawParkWaits(pw, 0, 0); dump("D3_park_p1");
  drawParkWaits(pw, 1, 0); dump("D3_park_p2");
  pw.park = "CALIFORNIA ADVENTURE"; drawParkWaits(pw, 0, 0); dump("D3_park_long");

  // stocks
  StockQuote q; q.symbol = "AAPL"; q.ok = true; q.price = 231.47f; q.prevClose = 228.10f; q.open = 229; q.dayHigh = 232.9f; q.dayLow = 227.6f;
  for (int i = 0; i < 40; i++) q.spark.push_back(228.5f + 3.0f * sinf(i / 6.0f) + i * 0.08f + (i % 3) * 0.2f);
  q.spark.back() = q.price;
  drawStock(q, 0); dump("D4_stock_up");
  StockQuote s2; s2.symbol = "SPY"; s2.ok = true; s2.price = 598.12f; s2.prevClose = 603.4f; s2.dayHigh = 604.2f; s2.dayLow = 597.5f; s2.halted = true;
  for (int i = 0; i < 40; i++) s2.spark.push_back(603.5f - i * 0.14f + 1.2f * sinf(i / 3.0f));
  drawStock(s2, 0); dump("D4_stock_down_halt");
  StockQuote s3 = q; s3.symbol = "GOOGL"; s3.price = 1234.56f; s3.dayHigh = 1240; s3.dayLow = 1200; s3.prevClose = 0; s3.open = 1210;
  drawStock(s3, 0); dump("D4_stock_wide");

  // news
  NewsItem n; n.league = "NFL"; n.why = "BUF"; n.color = 0x1F5FBF; n.published = real - 300;
  n.headline = "Bills sign OT Dion Dawkins to 3-year extension through 2029";
  for (int f : {0, 2, 4, 6, 20}) { drawNewsBanner(n, f, f * 3); dump((std::string("D5_banner_f") + std::to_string(f)).c_str()); }
  std::vector<NewsItem> items;
  items.push_back(n);
  NewsItem n2; n2.league = "NBA"; n2.why = "LAL"; n2.color = 0x7B3FA0; n2.published = real - 2 * 3600 - 100; n2.headline = "Lakers trade rumors heat up ahead of deadline"; items.push_back(n2);
  NewsItem n3; n3.league = "NFL"; n3.why = "J.ALLEN"; n3.color = 0x1F5FBF; n3.published = real - 26 * 3600; n3.headline = "Josh Allen limited in practice Wednesday with elbow soreness"; items.push_back(n3);
  NewsItem n4; n4.league = "MLB"; n4.why = "LAD"; n4.color = 0x005A9C; n4.published = real - 50 * 60; n4.headline = "Dodgers re-sign Teoscar Hernandez"; items.push_back(n4);
  drawNewsList(items, 0, 0); dump("D5_list_sel0");
  drawNewsList(items, 2, 20); dump("D5_list_sel2");

  // last play
  Game g = mkGame("401", "nfl", 'i', "BUF", 0x00338D, 17, "MIA", 0x008E97, 14);
  g.detail = "4:12 - 3rd"; g.down = "3rd & 7"; g.spot = "MIA 35"; g.poss = g.away.id; g.hasSit = true;
  g.lastPlay = "J.Allen pass deep left to K.Coleman for 18 yards to the MIA 35, 1st down";
  drawLastPlay(g, 0); dump("D6_lastplay_nfl");
  drawLastPlay(g, 120); dump("D6_lastplay_nfl_scrolled");
  g.redzone = true; g.down = "2nd & Goal"; g.spot = "MIA 4"; g.poss = g.home.id;
  drawLastPlay(g, 40); dump("D6_lastplay_redzone");
  Game gm = mkGame("402", "mlb", 'i', "LAD", 0x005A9C, 3, "SF", 0xFD5A1E, 2);
  gm.detail = "Bot 7th"; gm.outs = 2; gm.balls = 3; gm.strikes = 2; gm.lastPlay = "Ohtani singles on a line drive to center field";
  drawLastPlay(gm, 0); dump("D6_lastplay_mlb");

  // drive time
  DriveTime dr; dr.ok = true; dr.label = "WORK"; dr.minutes = 31; dr.typical = 19; dr.delay = 12; dr.km = 21;
  drawDriveTime(dr, now, 0); dump("D7_drive_bad");
  dr.minutes = 23; dr.delay = 4; drawDriveTime(dr, now, 0); dump("D7_drive_amber");
  dr.minutes = 18; dr.delay = 0; drawDriveTime(dr, now, 0); dump("D7_drive_clear");

  // lotto
  LottoDraw ld; ld.ok = true; ld.drawDate = parseIsoUtc("2026-11-08T00:00:00");
  int wb[5] = {7, 19, 34, 52, 68}; for (int i = 0; i < 5; i++) ld.white[i] = wb[i];
  ld.power = 11; ld.multiplier = 3; ld.jackpot = "$412 MILLION"; ld.nextDraw = now + 26 * 3600;
  drawLotto(ld, now, 0); dump("D8_lotto_days");
  ld.nextDraw = now + 5 * 3600 + 7 * 60 + 9; ld.white[0] = 3; ld.power = 5;
  drawLotto(ld, now, 10); dump("D8_lotto_hours");
  ld.jackpot = ""; ld.multiplier = 0; drawLotto(ld, now, 0); dump("D8_lotto_nojack");

  // pick'em
  std::vector<Game> gs;
  gs.push_back(mkGame("p1", "nfl", 'f', "BUF", 0x00338D, 31, "MIA", 0x008E97, 24));
  gs.push_back(mkGame("p2", "nfl", 'f', "KC", 0xE31837, 17, "DEN", 0xFB4F14, 20));
  gs.push_back(mkGame("p3", "nfl", 'i', "LAR", 0x003594, 10, "SEA", 0x69BE28, 13));
  gs.push_back(mkGame("p4", "nfl", 'p', "DAL", 0x0D2A5C, 0, "PHI", 0x004C54, 0)); gs.back().start = now + 3 * 86400 + 3600 * 3;
  gs.push_back(mkGame("p5", "nfl", 'i', "GB", 0x1F5F3F, 21, "CHI", 0xE0602A, 7));
  gs.push_back(mkGame("p6", "nfl", 'p', "SF", 0xAA0000, 0, "ARI", 0xC0203F, 0)); gs.back().start = now + 3 * 86400 + 3600 * 6;
  std::vector<PickemPick> pk;
  const char *who[] = {"BUF", "DEN", "LAR", "PHI", "GB", "SF"};
  const char *ids[] = {"p1", "p2", "p3", "p4", "p5", "p6"};
  const char res[] = {'W', 'W', ' ', ' ', ' ', ' '};
  for (int i = 0; i < 6; i++) { PickemPick p; p.league = "nfl"; p.gameId = ids[i]; p.pick = who[i]; p.result = res[i]; pk.push_back(p); }
  pk[1].pick = "KC"; pk[1].result = 'L';
  drawPickem(pk, gs, 41, 23, 0); dump("D9_pickem_p1");
  drawPickem(pk, gs, 41, 23, 1); dump("D9_pickem_p2");
  return 0;
}
