#include "tracks.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

std::vector<StageDef> STAGES;

namespace {
std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
uint32_t Fnv1a(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}
}  // namespace

bool ParseStage(const std::string& text, const std::string& id, StageDef& d, std::string* error) {
    d = StageDef{};
    d.id = id;
    d.fingerprint = Fnv1a(text);
    std::istringstream in(text);
    std::string line;
    int n = 0;
    auto fail = [&](const std::string& why) { if (error) *error = id + ".stage line " + std::to_string(n) + ": " + why; return false; };
    while (std::getline(in, line)) {
        n++;
        line = Trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        const size_t eq = line.find('=');
        if (eq != std::string::npos) {
            const std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
            const double num = std::atof(v.c_str());
            if (k == "name") d.name = v;
            else if (k == "desc") d.desc = v;
            else if (k == "width") d.w = num;
            else if (k == "runoff") d.r = num;
            else if (k == "laps") d.laps = std::max(1, (int)num);
            else if (k == "pace") d.v = num;
            else if (k == "sky") d.sky = v;
            else if (k == "weather") d.weather = v;
            else if (k == "grip") d.grip = num;
            else if (k == "pass_scale") d.passScale = num;
            else if (k == "seed") d.seed = (int)num;
            else return fail("unknown setting '" + k + "'");
            continue;
        }
        std::istringstream ls(line);
        std::string word;
        ls >> word;
        if (word == "pt") {
            V2 p;
            if (!(ls >> p.x >> p.y)) return fail("pt needs x and y");
            d.pts.push_back(p);
        } else if (word == "landmark") {
            Landmark m;
            if (!(ls >> m.type >> m.x >> m.y)) return fail("landmark needs a type, x and y");
            static const char* types[] = {"arch", "oasis", "salt", "hoodoo", "ruin", "wall", "rock", "lantern", "palm", "balloon", "flags"};
            if (std::none_of(std::begin(types), std::end(types), [&](const char* t) { return m.type == t; })) return fail("unknown landmark '" + m.type + "'");
            ls >> m.size >> m.angle;
            d.landmarks.push_back(m);
        } else return fail("unknown line '" + word + "'");
    }
    if (d.name.empty()) return fail("missing name");
    if (d.pts.size() < 4) return fail("a track needs at least 4 points");
    if (d.w <= 0 || d.v <= 0) return fail("width and pace must be positive");
    return true;
}

bool LoadStages(const std::string& dir, std::string* error) {
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.is_regular_file() && e.path().extension() == ".stage") files.push_back(e.path());
    if (ec) { if (error) *error = "cannot read " + dir; return false; }
    std::sort(files.begin(), files.end());
    std::vector<StageDef> out;
    for (const auto& f : files) {
        std::ifstream in(f, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        StageDef d;
        if (!ParseStage(ss.str(), f.stem().string(), d, error)) return false;
        out.push_back(std::move(d));
    }
    STAGES = std::move(out);
    return true;
}

template <class T> static std::vector<T> Rot(const std::vector<T>& a, int off) {
    std::vector<T> r(a.begin() + off, a.end());
    r.insert(r.end(), a.begin(), a.begin() + off);
    return r;
}

Track BuildTrack(const StageDef& def) {
    const Pts& P = def.pts;
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
        if ((z.second - z.first) * 18 > def.w * 1.1) padded.push_back({std::max(8, z.first - 4), std::min(N - 9, z.second + 3)});
    for (auto z : padded) {
        if (!merged.empty() && z.first - merged.back().second < 4) merged.back().second = z.second;   // corners closer than ~70 px share a zone
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
    T.pass = (int)(JsRound(per * def.laps * (1 + std::min(i, 8) * 0.06) * def.passScale / 100) * 100);
    T.limit = (int)JsRound(def.laps * T.L / def.v);
    T.clipR = std::max(30, 52 - i * 3);
}
