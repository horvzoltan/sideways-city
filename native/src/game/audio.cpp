#include "audio.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr int RATE = 48000;
constexpr float PI_F = 3.14159265f;
Audio* g_audio = nullptr;

// Engine recording map: [seconds into recording, engine pitch in Hz], for load (on) and overrun (off).
const float ENG_ON[][2] = {{1.971f,101},{2.071f,103},{2.171f,105},{2.271f,110},{2.371f,119},{2.471f,121},{2.571f,122},{2.871f,130},{2.971f,131},{3.071f,133},{3.171f,140},{3.271f,152},{3.371f,153},{3.471f,159},{11.371f,163},{11.471f,163},{11.571f,163},{11.671f,163},{11.771f,163},{11.871f,163},{11.971f,163},{12.071f,163},{12.171f,163},{12.271f,163},{12.371f,164},{12.471f,164},{12.571f,164},{12.671f,165},{12.771f,165},{12.871f,165},{12.971f,165},{13.071f,165},{13.171f,166},{13.271f,166},{13.371f,167},{13.471f,167},{13.571f,167},{13.671f,167},{13.771f,168},{13.871f,168},{13.971f,168},{14.071f,169},{14.171f,169},{14.271f,169},{14.371f,170},{14.471f,170},{14.571f,170},{14.671f,171},{14.771f,171},{14.871f,171},{14.971f,172},{15.071f,172},{15.171f,172},{15.271f,172},{15.371f,172},{15.471f,173},{15.571f,174},{15.671f,174},{15.771f,174},{15.871f,174},{15.971f,174},{16.071f,175},{16.171f,175},{16.271f,175},{16.371f,175},{16.471f,175},{16.571f,176},{16.671f,176},{16.771f,177},{16.871f,177},{16.971f,177},{17.071f,177},{17.171f,177},{17.271f,178},{17.371f,178},{17.471f,178},{17.571f,178},{17.671f,178},{17.771f,179},{17.871f,180},{17.971f,180},{18.071f,180},{18.171f,180},{18.271f,181},{18.371f,181},{18.471f,182},{18.571f,183},{18.671f,184},{18.771f,184},{18.871f,185},{18.971f,185},{19.071f,187},{19.171f,187},{19.271f,188},{19.371f,189},{19.471f,191},{19.571f,193},{19.671f,195},{19.771f,195},{19.871f,198},{19.971f,199},{20.071f,201},{20.171f,204},{20.271f,206},{20.371f,208},{20.471f,209},{20.571f,212},{20.671f,214},{20.771f,215},{20.871f,217},{20.971f,218},{21.071f,224},{21.171f,224},{21.271f,229},{21.371f,231},{21.471f,234},{21.571f,237},{21.671f,239},{21.771f,245},{21.871f,246},{21.971f,250},{22.071f,251},{22.171f,257},{22.271f,261},{22.371f,267},{22.471f,269},{22.571f,279},{22.671f,281},{22.771f,285},{22.971f,308},{23.071f,308},{23.171f,317},{23.271f,318},{23.371f,326},{23.471f,329},{23.571f,339},{23.671f,341},{23.771f,349},{23.871f,352},{23.971f,356},{24.071f,362},{24.171f,371},{24.271f,372},{24.371f,375},{24.471f,381},{24.571f,387},{24.671f,390},{24.771f,395}};
const float ENG_OFF[][2] = {{26.271f,301},{26.371f,299},{26.471f,297},{26.571f,285},{26.671f,267},{26.771f,264},{26.871f,250},{26.971f,246},{27.071f,246},{27.171f,226},{27.271f,217},{27.371f,202},{27.471f,202},{27.571f,192},{28.171f,144},{28.271f,141},{28.471f,131},{28.571f,121},{28.671f,113},{28.771f,108},{28.871f,107},{28.971f,93},{29.071f,93},{29.171f,93},{29.271f,90},{29.371f,89},{29.471f,87},{29.571f,87},{29.671f,86},{29.771f,85},{29.871f,85},{29.971f,81},{30.071f,80},{30.171f,80},{30.271f,78},{30.371f,78},{30.471f,78},{30.571f,78},{30.671f,76},{30.771f,75},{30.871f,72},{30.971f,72},{31.071f,72}};
const float IDLE_SPAN[2] = {31.75f, 33.4f}, IDLE_F = 65;   // a stretch of steady idle near the end of the clip
constexpr float GRAIN = 0.11f, HOP = GRAIN / 3;
const float GEAR_TOP[6] = {150, 245, 340, 440, 545, 660};
constexpr float IDLE = 1100, REDLINE = 7000;
constexpr float MUSIC_LEVEL = 0.45f;

float Frand() { return (float)std::rand() / RAND_MAX; }
float ExpInterp(float a, float b, float u) { return a * std::pow(b / a, std::clamp(u, 0.0f, 1.0f)); }
float Smooth(float tau) { return 1 - std::exp(-1.0f / (std::max(tau, 1e-4f) * RATE)); }

void Biquad(FilterType t, float f, float q, float& b0, float& b1, float& b2, float& a1, float& a2) {
    f = std::clamp(f, 10.0f, RATE * 0.49f);
    float w = 2 * PI_F * f / RATE, cs = std::cos(w), sn = std::sin(w);
    // Web Audio: lowpass and highpass Q is in dB, bandpass Q is plain
    float Q = t == BANDPASS ? std::max(q, 1e-3f) : std::pow(10.0f, q / 20);
    float alpha = sn / (2 * Q), a0 = 1 + alpha;
    if (t == LOWPASS) { b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2; }
    else if (t == HIGHPASS) { b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2; }
    else { b0 = alpha; b1 = 0; b2 = -alpha; }
    a1 = -2 * cs; a2 = 1 - alpha;
    b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
}
float PolyBlep(double t, double dt) {
    if (t < dt) { t /= dt; return (float)(t + t - t * t - 1); }
    if (t > 1 - dt) { t = (t - 1) / dt; return (float)(t * t + t + t + 1); }
    return 0;
}
}  // namespace

bool Audio::Init(int volume, bool m) {
    vol = volume; muted = m;
    g_audio = this;
    InitAudioDevice();
    ok = IsAudioDeviceReady();
    if (!ok) return false;
    // the engine recording, decoded once to mono floats
    if (FileExists("assets/supra-engine.mp3")) {
        Wave w = LoadWave("assets/supra-engine.mp3");
        if (w.frameCount) {
            engineRate = (int)w.sampleRate;
            float* s = LoadWaveSamples(w);
            engine.resize(w.frameCount);
            for (unsigned i = 0; i < w.frameCount; i++) {
                float v = 0;
                for (unsigned c = 0; c < w.channels; c++) v += s[i * w.channels + c];
                engine[i] = v / w.channels;
            }
            UnloadWaveSamples(s);
        }
        UnloadWave(w);
    }
    if (engine.empty()) TraceLog(LOG_WARNING, "Engine sound could not load: assets/supra-engine.mp3");
    SetAudioStreamBufferSizeDefault(1024);
    stream = LoadAudioStream(RATE, 32, 1);
    SetAudioStreamCallback(stream, Callback);
    PlayAudioStream(stream);
    // menu music streams from disk: "Liquid Flame" by Of Far Different Nature (CC0)
    if (FileExists("assets/music/menu-liquid-flame.mp3")) {
        music = LoadMusicStream("assets/music/menu-liquid-flame.mp3");
        musicOk = music.frameCount > 0;
        if (musicOk) { music.looping = true; SetMusicVolume(music, 0); }
    }
    std::lock_guard<std::mutex> lk(this->m);
    shared.master = MasterLevel();
    pMaster.v = pMaster.target = shared.master;
    return true;
}

void Audio::Shutdown() {
    if (!ok) return;
    if (musicOk) UnloadMusicStream(music);
    UnloadAudioStream(stream);
    CloseAudioDevice();
    ok = false;
    g_audio = nullptr;
}

float Audio::MasterLevel() const { return muted ? 0 : std::pow(vol / 10.0f, 1.6f); }

void Audio::SetVolume(int v) {
    vol = std::clamp(v, 0, 10);
    if (muted && vol > 0) muted = false;
    std::lock_guard<std::mutex> lk(m);
    shared.master = MasterLevel();
}
void Audio::SetMuted(bool mu) {
    muted = mu;
    std::lock_guard<std::mutex> lk(m);
    shared.master = MasterLevel();
}

void Audio::SetMusic(bool on) { musicOn = on; }

void Audio::Update(float dt, bool focused) {
    if (!ok) return;
    uiT -= dt;
    {   // the browser suspended all sound when the window lost focus
        std::lock_guard<std::mutex> lk(m);
        shared.master = focused ? MasterLevel() : 0;
    }
    if (!musicOk) return;
    // fade in on the menu (time constant 0.5 s), out when driving (0.25 s), then pause
    float target = musicOn ? MUSIC_LEVEL : 0, tau = musicOn ? 0.5f : 0.25f;
    musicGain += (target - musicGain) * (1 - std::exp(-dt / tau));
    if (musicOn) {
        musicStopT = 0;
        if (!IsMusicStreamPlaying(music)) PlayMusicStream(music);
    } else if (IsMusicStreamPlaying(music) && (musicStopT += dt) > 1.5f) PauseMusicStream(music);
    SetMusicVolume(music, musicGain * (focused ? MasterLevel() : 0));
    if (IsMusicStreamPlaying(music)) UpdateMusicStream(music);
}

// ---------- engine ----------
void Audio::UpdateEngine(float dt, float s, float slip, bool soft, float throttle) {
    float thr = throttle;
    if (es.cut > 0) { es.cut -= dt; thr = 0; }
    const bool on = thr > 0.15f;
    float wr = s / GEAR_TOP[es.gear] * REDLINE;
    if (wr > 6750 && es.gear < 5 && throttle > 0.15f) { es.gear++; es.cut = 0.16f; if (Frand() < 0.6f) Crackle(); }
    else if (es.gear > 0 && wr < 2600 && s / GEAR_TOP[es.gear - 1] * REDLINE < 6000) es.gear--;
    wr = s / GEAR_TOP[es.gear] * REDLINE;
    float target = std::max(IDLE, wr);
    if (on && s < 60) target = std::max(target, IDLE + (4300 - IDLE) * thr);
    if (on && slip > 150) target += 1000 * std::min(1.0f, (slip - 150) / 200);
    target = std::min(LIMIT, target);
    es.rpm += (target - es.rpm) * std::min(1.0f, (target > es.rpm ? 9 : 5) * dt);
    es.load += (thr - es.load) * std::min(1.0f, 10 * dt);

    float lim = 1;
    if (on && es.rpm > 6950) { es.limT += dt; lim = ((int)std::floor(es.limT * 24) % 2) ? 0.3f : 1; }
    else es.limT = 0;

    // blow-off valve and pops come from the synth, layered on top of the recording
    float bt = on ? thr * std::min(1.0f, std::max(0.0f, (es.rpm - 2500) / 3000)) : 0;
    es.boost += (bt - es.boost) * std::min(1.0f, (bt > es.boost ? 1.4f : 5) * dt);
    if (es.prevThr && !on && es.boost > 0.3f) BlowOff(es.boost);
    es.prevThr = on;
    if (throttle < 0.15f && es.rpm > 3400 && Frand() < dt * 5) Crackle();

    float sk = std::min(1.0f, std::max(0.0f, (slip - 90) / 280));
    if (soft) sk *= 0.25f;
    float t = (float)clock.load() / RATE;
    std::lock_guard<std::mutex> lk(m);
    shared.layerOn = es.load * 0.9f;
    shared.layerOff = (1 - es.load) * 0.75f;
    shared.layerTau = 0.03f;
    shared.bus = lim;
    shared.skidGain = sk * 0.26f;
    shared.skidTau = 0.05f;
    shared.skidFreq = (soft ? 500 : 1500) + slip * 0.9f + std::sin(t * 37) * 120;
    shared.grains = !engine.empty();
    shared.hz = es.rpm / 18;
    shared.load = es.load;
}

void Audio::Silence() {
    es.load = 0;
    std::lock_guard<std::mutex> lk(m);
    shared.layerOn = shared.layerOff = shared.skidGain = 0;
    shared.layerTau = shared.skidTau = 0.05f;
    shared.grains = false;
}

void Audio::Lookup(bool off, float hz, double& pos, float& rec) {
    const float(*tab)[2] = off ? ENG_OFF : ENG_ON;
    const int n = off ? (int)(sizeof(ENG_OFF) / sizeof(ENG_OFF[0])) : (int)(sizeof(ENG_ON) / sizeof(ENG_ON[0]));
    for (int i = 0; i < n - 1; i++) {
        float f0 = tab[i][1], f1 = tab[i + 1][1], lo = std::min(f0, f1), hi = std::max(f0, f1);
        if (hz >= lo && hz <= hi && hi > lo) {
            float k = (hz - f0) / (f1 - f0);
            pos = tab[i][0] + k * (tab[i + 1][0] - tab[i][0]); rec = hz;
            return;
        }
    }
    int best = 0; float bd = 1e9f;
    for (int i = 0; i < n; i++) { float d = std::fabs(tab[i][1] - hz); if (d < bd) { bd = d; best = i; } }
    pos = tab[best][0]; rec = tab[best][1];
}

void Audio::SpawnGrain(int layer, uint64_t at, float hz) {
    double pos; float rec;
    if (layer == 1 && hz < 70) { pos = IDLE_SPAN[0] + Frand() * (IDLE_SPAN[1] - IDLE_SPAN[0]); rec = IDLE_F; }
    else Lookup(layer == 1, hz, pos, rec);
    float rate = std::clamp(hz / rec, 0.5f, 2.0f);
    double startSec = std::max(0.0, pos - GRAIN * rate / 2 + (Frand() - .5) * 0.02);
    grains.push_back({at, layer, startSec * engineRate, (double)rate * engineRate / RATE, 0});
}

// ---------- one-shots ----------
void Audio::QueueOsc(float delay, OscType type, float f0, float f1, float fdur, float g0, float peak, float ta, float gEnd, float td, float stop, bool toMaster) {
    if (!ok) return;
    uint64_t start = clock.load() + (uint64_t)(delay * RATE);
    std::lock_guard<std::mutex> lk(m);
    pendingOsc.push_back({start, type, f0, f1, fdur, g0, peak, ta, gEnd, td, stop, toMaster, 0});
}

void Audio::Burst(float delay, float dur, FilterType type, float f0, float f1, float q, float v) {
    if (!ok) return;
    Noise n{};
    n.start = clock.load() + (uint64_t)(delay * RATE);
    n.type = type; n.f0 = f0; n.f1 = f1; n.dur = dur; n.q = q; n.vol = v;
    std::lock_guard<std::mutex> lk(m);
    pendingNoise.push_back(n);
}

void Audio::Notes(std::initializer_list<float> freqs, OscType type, float step, float v) {
    if (muted) return;
    int i = 0;
    for (float fr : freqs) { QueueOsc(i * step, type, fr, fr, 1, 0.0001f, v, 0.01f, 0.0001f, step * 1.8f, step * 2, true); i++; }
}

void Audio::BlowOff(float b) {
    Burst(0, 0.45f, BANDPASS, 3200, 700, 1.4f, 0.35f * b);
    for (int i = 0; i < 5; i++) Burst(0.05f + i * 0.045f, 0.04f, BANDPASS, 1800 - i * 200, 900, 3, 0.12f * b);   // flutter
}
void Audio::Crackle() {
    int k = 1 + std::rand() % 3;
    for (int i = 0; i < k; i++) Burst(i * 0.035f + Frand() * 0.02f, 0.05f, HIGHPASS, 700, 400, 0.7f, 0.25f + Frand() * 0.25f);
}
void Audio::Crash(float power) {
    if (muted) return;
    float v = std::min(1.0f, power);
    Burst(0, 0.35f, LOWPASS, 900, 300, 0.7f, 0.7f * v);
    QueueOsc(0, SINE, 110, 38, 0.25f, 0.8f * v, 0.8f * v, 0, 0.001f, 0.3f, 0.32f, true);
}
void Audio::Ui(UiKind kind) {
    if (muted) return;
    const float t = 0.005f;
    auto tone = [&](float f0, float f1, float dur, OscType type, float v, float delay) {
        QueueOsc(t + delay, type, f0, f1, dur, 0.0001f, v, 0.006f, 0.0001f, dur, dur + 0.02f, true);
    };
    if (kind == UI_MOVE) {
        if (uiT > 0) return;
        uiT = 0.035f;
        tone(2400, 1900, 0.045f, SINE, 0.05f, 0);
        Burst(t, 0.03f, HIGHPASS, 5200, 3000, 1, 0.04f);
    } else if (kind == UI_SELECT) { tone(880, 1320, 0.09f, TRIANGLE, 0.08f, 0); tone(1320, 1760, 0.12f, SINE, 0.05f, 0.05f); }
    else if (kind == UI_BACK) tone(900, 520, 0.1f, TRIANGLE, 0.07f, 0);
    else {
        Burst(t, 0.4f, BANDPASS, 400, 4200, 1.2f, 0.16f);
        tone(110, 50, 0.28f, SINE, 0.25f, 0);
        tone(660, 990, 0.12f, SQUARE, 0.035f, 0.05f);
        tone(990, 1320, 0.2f, SQUARE, 0.03f, 0.13f);
    }
}

// ---------- the mixer (audio thread) ----------
void Audio::Callback(void* buffer, unsigned int frames) {
    if (g_audio) g_audio->Render((float*)buffer, frames);
    else std::fill((float*)buffer, (float*)buffer + frames, 0.0f);
}

void Audio::Render(float* out, unsigned frames) {
    {
        std::lock_guard<std::mutex> lk(m);
        cur = shared;
        oscs.insert(oscs.end(), pendingOsc.begin(), pendingOsc.end());
        noises.insert(noises.end(), pendingNoise.begin(), pendingNoise.end());
        pendingOsc.clear(); pendingNoise.clear();
    }
    const uint64_t t0 = clock.load();
    pMaster.target = cur.master; pMaster.tau = 0.03f;
    pOn.target = cur.layerOn; pOn.tau = cur.layerTau;
    pOff.target = cur.layerOff; pOff.tau = cur.layerTau;
    pBus.target = cur.bus; pBus.tau = 0.01f;
    pSkidG.target = cur.skidGain; pSkidG.tau = cur.skidTau;
    pSkidF.target = cur.skidFreq; pSkidF.tau = 0.03f;

    // engine grains: short overlapping slices of the recording, scheduled ahead
    if (cur.grains) {
        double now = (double)t0 / RATE;
        if (nextGrain < now) nextGrain = now + 0.01;
        while (nextGrain < now + (double)frames / RATE + 0.02) {
            uint64_t at = (uint64_t)(nextGrain * RATE);
            if (cur.load > 0.01f) SpawnGrain(0, at, cur.hz);
            if (1 - cur.load > 0.01f) SpawnGrain(1, at, cur.hz);
            nextGrain += HOP;
        }
    }

    const float kMaster = Smooth(pMaster.tau), kOn = Smooth(pOn.tau), kOff = Smooth(pOff.tau), kBus = Smooth(pBus.tau);
    const float kSkG = Smooth(pSkidG.tau), kSkF = Smooth(pSkidF.tau);
    const int grainLen = (int)(GRAIN * RATE);
    // compressor: threshold -16 dB, ratio 4, 30 dB knee, 3 ms attack, 250 ms release, like Web Audio's defaults
    const float atk = 1 - std::exp(-1.0f / (0.003f * RATE)), rel = 1 - std::exp(-1.0f / (0.25f * RATE));
    const float T = -16, R = 4, K = 30, makeup = std::pow(10.0f, 1.9f / 20);
    float sb0 = 0, sb1 = 0, sb2 = 0, sa1 = 0, sa2 = 0;

    for (unsigned i = 0; i < frames; i++) {
        const uint64_t now = t0 + i;
        pMaster.v += (pMaster.target - pMaster.v) * kMaster;
        pOn.v += (pOn.target - pOn.v) * kOn;
        pOff.v += (pOff.target - pOff.v) * kOff;
        pBus.v += (pBus.target - pBus.v) * kBus;
        pSkidG.v += (pSkidG.target - pSkidG.v) * kSkG;
        pSkidF.v += (pSkidF.target - pSkidF.v) * kSkF;

        float comp = 0, direct = 0, layer[2] = {0, 0};
        // grains
        for (Grain& g : grains) {
            if (now < g.start) continue;
            float u = g.age / grainLen;
            if (u < 1 && !engine.empty()) {
                size_t k = (size_t)g.pos;
                float fr = (float)(g.pos - k);
                float s = k + 1 < engine.size() ? engine[k] + (engine[k + 1] - engine[k]) * fr : 0;
                float win = std::sin(PI_F * u);
                layer[g.layer] += s * win * win / 1.5f;
            }
            g.pos += g.step;
            g.age += 1;
        }
        comp += (layer[0] * pOn.v + layer[1] * pOff.v) * pBus.v;

        // tyres: looping noise through a narrow bandpass
        if ((i & 15) == 0) Biquad(BANDPASS, pSkidF.v, 7, sb0, sb1, sb2, sa1, sa2);
        if (pSkidG.v > 1e-5f) {
            rng = rng * 1664525u + 1013904223u;
            float x = ((rng >> 8) & 0xFFFF) / 32768.0f - 1;
            float y = sb0 * x + sb1 * skx1 + sb2 * skx2 - sa1 * sky1 - sa2 * sky2;
            skx2 = skx1; skx1 = x; sky2 = sky1; sky1 = y;
            comp += y * pSkidG.v;
        }

        // noise bursts: filtered noise with falling cutoff and gain
        for (Noise& n : noises) {
            if (now < n.start) continue;
            float t = (float)(now - n.start) / RATE;
            if (t > n.dur + 0.02f) continue;
            if (((now - n.start) & 15) == 0) Biquad(n.type, ExpInterp(n.f0, n.f1, t / n.dur), n.q, n.b0, n.b1, n.b2, n.a1, n.a2);
            rng = rng * 1664525u + 1013904223u;
            float x = ((rng >> 8) & 0xFFFF) / 32768.0f - 1;
            float y = n.b0 * x + n.b1 * n.x1 + n.b2 * n.x2 - n.a1 * n.y1 - n.a2 * n.y2;
            n.x2 = n.x1; n.x1 = x; n.y2 = n.y1; n.y1 = y;
            comp += y * ExpInterp(n.vol, 0.0008f, t / n.dur);
        }

        // oscillators
        for (Osc& o : oscs) {
            if (now < o.start) continue;
            float t = (float)(now - o.start) / RATE;
            if (t > o.stop) continue;
            float f = t < o.fdur ? ExpInterp(o.f0, o.f1, t / o.fdur) : o.f1;
            double dt = f / RATE;
            o.phase += dt;
            if (o.phase >= 1) o.phase -= 1;
            double ph = o.phase;
            float s;
            switch (o.type) {
                case SINE: s = std::sin(2 * PI_F * (float)ph); break;
                case TRIANGLE: s = (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph); break;
                case SQUARE: s = (ph < 0.5 ? 1.0f : -1.0f) + PolyBlep(ph, dt) - PolyBlep(std::fmod(ph + 0.5, 1.0), dt); break;
                default: s = (float)(2 * ph - 1) - PolyBlep(ph, dt); break;
            }
            float g = t < o.ta ? ExpInterp(o.g0, o.peak, t / o.ta) : t < o.td ? ExpInterp(o.peak, o.gEnd, (t - o.ta) / (o.td - o.ta)) : o.gEnd;
            if (o.toMaster) direct += s * g; else comp += s * g;
        }

        // compressor on the effects bus
        float lvl = std::fabs(comp);
        compEnv += (lvl - compEnv) * (lvl > compEnv ? atk : rel);
        float db = 20 * std::log10(std::max(compEnv, 1e-6f)), gainDb = 0;
        if (db > T + K) gainDb = (1 / R - 1) * (db - T - K / 2);
        else if (db > T) gainDb = (1 / R - 1) * (db - T) * (db - T) / (2 * K);
        float mix = comp * std::pow(10.0f, gainDb / 20) * makeup + direct;
        out[i] = std::clamp(mix * pMaster.v, -1.0f, 1.0f);
    }
    clock.store(t0 + frames);

    // drop what has finished
    const uint64_t end = t0 + frames;
    grains.erase(std::remove_if(grains.begin(), grains.end(), [&](const Grain& g) { return end > g.start && g.age >= grainLen + 0.01f * RATE; }), grains.end());
    noises.erase(std::remove_if(noises.begin(), noises.end(), [&](const Noise& n) { return end > n.start && (float)(end - n.start) / RATE > n.dur + 0.02f; }), noises.end());
    oscs.erase(std::remove_if(oscs.begin(), oscs.end(), [&](const Osc& o) { return end > o.start && (float)(end - o.start) / RATE > o.stop; }), oscs.end());
}
