#include "tracks.h"
#include <algorithm>

const std::vector<StageDef> STAGES = {
    {"Practice Bowl", "Wide and forgiving. Learn to link the corners.", 230, 120, 2, 270, "grass", false,
     {{500,500},{2300,500},{2750,900},{2700,1500},{2250,1800},{1500,1700},{1150,2050},{1250,2550},{750,2750},{380,2250},{350,1200}}},
    {"Container Port", "Two long hairpins between the stacks. Keep the rear out.", 200, 90, 2, 290, "port", false,
     {{400,400},{2600,400},{2850,720},{2550,1000},{1150,1020},{880,1270},{1150,1520},{2550,1540},{2850,1850},{2550,2170},{1150,2200},{700,2500},{380,2150}}},
    {"Mountain Touge", "Narrow road, short run-off, trees everywhere. One mistake costs the zone.", 175, 60, 2, 300, "forest", false,
     {{500,300},{1500,380},{2500,300},{2850,650},{2450,1020},{1650,920},{1200,1250},{1700,1600},{2600,1520},{2950,1950},{2550,2450},{1650,2330},{950,2650},{420,2250},{720,1650},{300,1100}}},
    {"Crossover", "A figure eight under the lights. Watch the crossing.", 185, 75, 2, 310, "stadium", true, {}},
    {"Snake Canyon", "Endless direction changes. Transitions decide everything.", 155, 50, 2, 320, "desert", false,
     {{450,520},{950,300},{1450,620},{1950,300},{2450,620},{2950,420},{3150,950},{2800,1420},{3150,1920},{2850,2450},{2300,2750},{1800,2420},{1300,2750},{800,2420},{380,2620},{230,2000},{560,1500},{230,1020}}},
    {"Devil's Knot", "Night run. Tight, twisted and unforgiving. Only the best finish.", 138, 38, 2, 330, "night", false,
     {{500,420},{1700,420},{2050,720},{1750,1040},{1250,960},{950,1250},{1250,1550},{2200,1430},{2600,930},{3050,1200},{2950,2000},{2450,2250},{2150,1930},{1700,2250},{2000,2750},{1300,2850},{700,2550},{900,2020},{420,1720},{300,1000}}},
    {"Ocean Drive", "Fast sweepers between the sand and the palms. Carry your speed.", 170, 90, 2, 300, "beach", false,
     {{400,500},{1600,420},{2800,560},{3200,1000},{2900,1500},{2200,1450},{1700,1800},{2100,2300},{2900,2400},{3100,2800},{2500,3150},{1300,3050},{700,2700},{900,2100},{500,1600},{300,1000}}},
    {"Frozen Lake", "Wide loops on sheet ice. Less grip everywhere, so start the slide early.", 200, 110, 2, 260, "snow", false,
     {{500,600},{2000,500},{2800,900},{2700,1500},{2000,1600},{1500,1300},{900,1500},{1100,2100},{2000,2200},{2800,2500},{2500,3000},{1200,3000},{500,2500},{300,1500}}},
    {"Harbour Hairpins", "Five hairpins stacked between the containers. Flick it early and hold the angle.", 170, 70, 2, 300, "port", false,
     {{400,400},{2700,400},{3000,650},{2700,900},{900,900},{650,1150},{900,1400},{2700,1400},{3000,1650},{2700,1900},{900,1900},{650,2150},{900,2400},{2700,2400},{3050,2750},{2600,3050},{600,3000},{250,2500},{250,900}}},
    {"Neon Downtown", "Square city corners under the neon. Tight walls and no run-off to speak of.", 160, 50, 2, 310, "neon", false,
     {{500,500},{1500,500},{1700,700},{1700,1200},{1900,1400},{2600,1400},{2800,1600},{2800,2400},{2600,2600},{1900,2600},{1700,2400},{1700,2000},{1500,1800},{800,1800},{600,2000},{600,2600},{400,2800},{250,2600},{250,700}}},
    {"Canyon Run", "A long, fast run with snaking sections at both ends. Commit or lose the zone.", 150, 50, 2, 340, "desert", false,
     {{500,400},{1500,300},{2500,450},{3300,300},{3700,700},{3400,1200},{3700,1700},{3300,2200},{2500,2000},{1800,2300},{1200,2000},{600,2300},{300,1800},{700,1300},{300,800}}},
    {"Midnight Pass", "The narrowest, twistiest road of all, in the dark. The final test.", 135, 36, 2, 350, "night", false,
     {{500,400},{1400,350},{1800,650},{1500,1000},{1900,1300},{2600,1100},{3000,1400},{2800,1900},{2200,1800},{1800,2200},{2300,2600},{1800,3000},{1000,2900},{600,2500},{1000,2000},{700,1500},{300,1200},{350,700}}},
};

static Pts EightPts() {
    Pts p;
    for (int i = 0; i < 16; i++) {
        double t = i / 16.0 * PI * 2;
        p.push_back({1750 + 1400 * std::cos(t), 1450 + 1150 * std::sin(t) * std::cos(t)});
    }
    return p;
}

template <class T> static std::vector<T> Rot(const std::vector<T>& a, int off) {
    std::vector<T> r(a.begin() + off, a.end());
    r.insert(r.end(), a.begin(), a.begin() + off);
    return r;
}

Track BuildTrack(const StageDef& def) {
    const Pts P = def.eight ? EightPts() : def.pts;
    const int n = (int)P.size();
    Pts out;
    for (int i = 0; i < n; i++) {   // closed Catmull-Rom spline
        const V2 p0 = P[(i - 1 + n) % n], p1 = P[i], p2 = P[(i + 1) % n], p3 = P[(i + 2) % n];
        double seg = Hypot(p2.x - p1.x, p2.y - p1.y);
        int steps = std::max(4, (int)std::ceil(seg / 10));
        for (int s = 0; s < steps; s++) {
            double t = (double)s / steps, t2 = t * t, t3 = t2 * t;
            auto f = [&](double a, double b, double c, double d) {
                return 0.5 * ((2 * b) + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (-a + 3 * b - 3 * c + d) * t3);
            };
            out.push_back({f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)});
        }
    }
    // resample to even 18px spacing
    const int M = (int)out.size();
    std::vector<double> cum = {0};
    for (int i = 1; i <= M; i++) {
        V2 a = out[i - 1], b = out[i % M];
        cum.push_back(cum[i - 1] + Hypot(b.x - a.x, b.y - a.y));
    }
    const double L = cum[M];
    const int N = (int)JsRound(L / 18);
    Pts pts;
    int j = 0;
    for (int k = 0; k < N; k++) {
        double s = k * L / N;
        while (cum[j + 1] < s) j++;
        V2 a = out[j], b = out[(j + 1) % M];
        double d = cum[j + 1] - cum[j];
        double u = (s - cum[j]) / (d != 0 ? d : 1);
        pts.push_back({a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u});
    }
    // tangents and curvature
    Pts tan;
    std::vector<double> k;
    for (int i = 0; i < N; i++) {
        V2 a = pts[(i - 1 + N) % N], b = pts[(i + 1) % N];
        double d = Hypot(b.x - a.x, b.y - a.y);
        tan.push_back({(b.x - a.x) / d, (b.y - a.y) / d});
    }
    for (int i = 0; i < N; i++) {
        V2 t0 = tan[(i - 1 + N) % N], t1 = tan[(i + 1) % N];
        k.push_back((t0.x * t1.y - t0.y * t1.x) / 36);
    }
    std::vector<double> ks(N);
    for (int i = 0; i < N; i++) {
        double s = 0;
        for (int d = -4; d <= 4; d++) s += k[((i + d) % N + N) % N];
        ks[i] = s / 9;
    }
    // start line in the middle of the longest straight
    int bestLen = 0, bestMid = 0, run = 0;
    for (int i = 0; i < 2 * N; i++) {
        if (std::fabs(ks[i % N]) < 0.0012) {
            run++;
            if (run > bestLen) { bestLen = run; bestMid = i - run / 2; }
        } else run = 0;
    }
    int off = ((bestMid % N) + N) % N;
    if (def.eight) {
        int c = 0; double cd = 1e9;
        for (int i = 0; i < N; i++) {
            double d = Hypot(pts[i].x - 1750, pts[i].y - 1450);
            if (d < cd) { cd = d; c = i; }
        }
        off = (int)((c + JsRound(N * 0.09)) % N);
    }
    Track T;
    T.def = &def; T.N = N; T.L = L;
    T.pts = Rot(pts, off); T.tan = Rot(tan, off); T.k = Rot(ks, off);
    T.w = def.w; T.half = def.w / 2; T.edge = def.w / 2 + def.r;

    // drift zones: corner runs, padded and merged
    const double thr = 1 / (def.w * 3.3);
    std::vector<std::pair<int, int>> zones;
    for (int i = 0; i < N;) {
        if (std::fabs(T.k[i]) > thr) {
            int e = i;
            while (e < N && std::fabs(T.k[e]) > thr) e++;
            zones.push_back({i, e - 1});
            i = e;
        } else i++;
    }
    std::vector<std::pair<int, int>> padded, merged;
    for (auto z : zones)
        if ((z.second - z.first) * 18 > def.w * 1.1) padded.push_back({std::max(8, z.first - 7), std::min(N - 9, z.second + 5)});
    for (auto z : padded) {
        if (!merged.empty() && z.first - merged.back().second < 10) merged.back().second = z.second;
        else merged.push_back(z);
    }
    for (auto [a, b] : merged) {
        // clipping points: the sharpest spots of the zone, on the inside edge
        std::vector<int> peaks;
        for (int q = a + 1; q < b; q++) {
            double v = std::fabs(T.k[q]);
            if (v > thr * 1.5 && v >= std::fabs(T.k[q - 1]) && v >= std::fabs(T.k[q + 1]) &&
                std::none_of(peaks.begin(), peaks.end(), [&](int pk) { return std::abs(pk - q) < 16; }))
                peaks.push_back(q);
        }
        if (peaks.empty()) {
            int apex = a; double mk = 0;
            for (int q = a; q <= b; q++) if (std::fabs(T.k[q]) > mk) { mk = std::fabs(T.k[q]); apex = q; }
            peaks.push_back(apex);
        }
        Zone z{a, b, (b - a) * 18.0, {}};
        for (int q : peaks) {
            double side = Sign(T.k[q]);
            V2 p = T.pts[q], t = T.tan[q];
            double nx = -t.y * side, ny = t.x * side, o = T.half - 16;
            z.clips.push_back({q, p.x + nx * o, p.y + ny * o});
        }
        T.zones.push_back(z);
    }
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (V2 p : T.pts) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); }
    T.bounds[0] = x0 - T.edge - 300; T.bounds[1] = y0 - T.edge - 300;
    T.bounds[2] = x1 + T.edge + 300; T.bounds[3] = y1 + T.edge + 300;
    return T;
}

void StageGoals(Track& T, int i) {
    const StageDef& def = *T.def;
    double per = 0;
    for (const Zone& z : T.zones) per += z.len * 0.12 + 250 + z.clips.size() * 150;
    T.pass = (int)(JsRound(per * def.laps * (1 + std::min(i, 8) * 0.06) / 100) * 100);
    T.limit = (int)JsRound(def.laps * T.L / def.v);
    T.clipR = std::max(30, 52 - i * 3);
}
