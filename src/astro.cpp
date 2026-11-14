#include "astro.h"
#include <cmath>

static const double RAD = M_PI / 180.0;
static double rev(double x) { return x - floor(x / 360.0) * 360.0; }
static double dayNumber(time_t t) { return t / 86400.0 + 2440587.5 - 2451543.5; }  // days since 2000 Jan 0.0 UT

struct Elements { double N, i, w, a, e, M; };

static Elements elementsFor(int p, double d) {
  switch (p) {
    case 0: return {48.3313 + 3.24587e-5 * d, 7.0047 + 5.00e-8 * d, 29.1241 + 1.01444e-5 * d, 0.387098,
                    0.205635 + 5.59e-10 * d, 168.6562 + 4.0923344368 * d};                         // Mercury
    case 1: return {76.6799 + 2.46590e-5 * d, 3.3946 + 2.75e-8 * d, 54.8910 + 1.38374e-5 * d, 0.723330,
                    0.006773 - 1.302e-9 * d, 48.0052 + 1.6021302244 * d};                          // Venus
    case 2: return {49.5574 + 2.11081e-5 * d, 1.8497 - 1.78e-8 * d, 286.5016 + 2.92961e-5 * d, 1.523688,
                    0.093405 + 2.516e-9 * d, 18.6021 + 0.5240207766 * d};                          // Mars
    case 3: return {100.4542 + 2.76854e-5 * d, 1.3030 - 1.557e-7 * d, 273.8777 + 1.64505e-5 * d, 5.20256,
                    0.048498 + 4.469e-9 * d, 19.8950 + 0.0830853001 * d};                          // Jupiter
    default: return {113.6634 + 2.38980e-5 * d, 2.4886 - 1.081e-7 * d, 339.3939 + 2.97661e-5 * d, 9.55475,
                     0.055546 - 9.499e-9 * d, 316.9670 + 0.0334442282 * d};                        // Saturn
  }
}

static double eccentricAnomaly(double M, double e) {  // degrees
  double E = M + e / RAD * sin(M * RAD) * (1 + e * cos(M * RAD));
  for (int k = 0; k < 8; k++) E = E - (E - e / RAD * sin(E * RAD) - M) / (1 - e * cos(E * RAD));
  return E;
}

// sun: geocentric ecliptic longitude and distance; also the mean longitude for sidereal time
static void sunEcliptic(double d, double &lon, double &r, double &L) {
  double w = 282.9404 + 4.70935e-5 * d, e = 0.016709 - 1.151e-9 * d, M = rev(356.0470 + 0.9856002585 * d);
  double E = eccentricAnomaly(M, e);
  double xv = cos(E * RAD) - e, yv = sqrt(1 - e * e) * sin(E * RAD);
  double v = atan2(yv, xv) / RAD;
  r = sqrt(xv * xv + yv * yv);
  lon = rev(v + w);
  L = rev(M + w);
}

static void toEquatorial(double x, double y, double z, double d, double &ra, double &dec) {
  double ecl = (23.4393 - 3.563e-7 * d) * RAD;
  double xe = x, ye = y * cos(ecl) - z * sin(ecl), ze = y * sin(ecl) + z * cos(ecl);
  ra = rev(atan2(ye, xe) / RAD);
  dec = atan2(ze, sqrt(xe * xe + ye * ye)) / RAD;
}

static double localSiderealDeg(double d, double L, time_t t, double lon) {
  double ut = fmod(t / 3600.0, 24.0);
  return rev(L + 180 + ut * 15 + lon);  // GMST0 (deg) + UT + east longitude
}

static void altAz(double ra, double dec, double lst, double lat, double &alt, double &az) {
  double ha = (lst - ra) * RAD;
  double x = cos(ha) * cos(dec * RAD), y = sin(ha) * cos(dec * RAD), z = sin(dec * RAD);
  double xh = x * sin(lat * RAD) - z * cos(lat * RAD), zh = x * cos(lat * RAD) + z * sin(lat * RAD);
  az = rev(atan2(y, xh) / RAD + 180);
  alt = asin(zh) / RAD;
}

void planetPositions(time_t t, double lat, double lon, std::vector<PlanetPos> &out) {
  static const char *NAMES[5] = {"MERCURY", "VENUS", "MARS", "JUPITER", "SATURN"};
  static const char LET[5] = {'M', 'V', 'R', 'J', 'S'};  // R for Mars: M is Mercury
  static const uint32_t COL[5] = {0xC8B8A0, 0xFFF4C0, 0xFF6A3A, 0xF0D8B0, 0xE8C878};
  out.clear();
  double d = dayNumber(t), slon, sr, L;
  sunEcliptic(d, slon, sr, L);
  double xs = sr * cos(slon * RAD), ys = sr * sin(slon * RAD);
  double lst = localSiderealDeg(d, L, t, lon);
  for (int p = 0; p < 5; p++) {
    Elements el = elementsFor(p, d);
    double M = rev(el.M), E = eccentricAnomaly(M, el.e);
    double xv = el.a * (cos(E * RAD) - el.e), yv = el.a * sqrt(1 - el.e * el.e) * sin(E * RAD);
    double v = atan2(yv, xv) / RAD, r = sqrt(xv * xv + yv * yv);
    double N = el.N * RAD, i = el.i * RAD, vw = (v + el.w) * RAD;
    double xh = r * (cos(N) * cos(vw) - sin(N) * sin(vw) * cos(i));
    double yh = r * (sin(N) * cos(vw) + cos(N) * sin(vw) * cos(i));
    double zh = r * sin(vw) * sin(i);
    double ra, dec;
    toEquatorial(xh + xs, yh + ys, zh, d, ra, dec);
    PlanetPos pp{NAMES[p], LET[p], COL[p], 0, 0};
    altAz(ra, dec, lst, lat, pp.alt, pp.az);
    out.push_back(pp);
  }
}

void sunSubpoint(time_t t, double &lat, double &lon) {
  double d = dayNumber(t), slon, sr, L;
  sunEcliptic(d, slon, sr, L);
  double ra, dec;
  toEquatorial(sr * cos(slon * RAD), sr * sin(slon * RAD), 0, d, ra, dec);
  double gmst = localSiderealDeg(d, L, t, 0);
  lat = dec;
  lon = rev(ra - gmst + 180) - 180;
}

double sunAltitude(time_t t, double lat, double lon) {
  double sl, so;
  sunSubpoint(t, sl, so);
  double c = sin(lat * RAD) * sin(sl * RAD) + cos(lat * RAD) * cos(sl * RAD) * cos((lon - so) * RAD);
  return asin(c) / RAD;
}

const char *compass8(double az) {
  static const char *D[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return D[(int)floor(rev(az + 22.5) / 45.0) % 8];
}
