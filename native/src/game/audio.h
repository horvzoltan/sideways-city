// audio.h - everything the web version did with Web Audio, as a small mixer on raylib's audio
// stream callback: the engine (granular playback of a real Supra dyno recording), tyre squeal,
// the blow-off valve and pops, crash and interface sounds, and the menu music.
#pragma once
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <mutex>
#include <vector>
#include "raylib.h"

enum OscType { SINE, TRIANGLE, SQUARE, SAWTOOTH };
enum FilterType { LOWPASS, HIGHPASS, BANDPASS };
enum UiKind { UI_MOVE, UI_SELECT, UI_BACK, UI_START };

class Audio {
public:
    bool Init(int volume, bool muted);
    void Shutdown();
    void Update(float dt, bool focused);   // music fades and streaming; call once a frame

    // ---- engine: call UpdateEngine every frame while driving ----
    void UpdateEngine(float dt, float speed, float slip, bool softSurface, float throttle);
    void ResetEngine() { es = EngineState{}; }
    void Silence();          // engine and tyres fade out (pause, menus)
    int Gear() const { return es.gear; }
    float Rpm() const { return es.rpm; }
    static constexpr float LIMIT = 7200;

    // ---- one-shots ----
    void Notes(std::initializer_list<float> freqs, OscType type, float step, float vol);
    void Burst(float delay, float dur, FilterType type, float f0, float f1, float q, float vol);
    void Crash(float power);
    void Ui(UiKind kind);
    void BlowOff(float b);
    void Crackle();

    // ---- volume and music ----
    void SetVolume(int v);   // 0..10
    void SetMuted(bool m);
    int Volume() const { return vol; }
    bool Muted() const { return muted; }
    void SetMusic(bool on);

    // Called on the audio thread.
    void Render(float* out, unsigned frames);

private:
    struct EngineState { float rpm = 1100; int gear = 0; float cut = 0, boost = 0, load = 0, limT = 0; bool prevThr = false; };
    EngineState es;
    struct Param { float v = 0, target = 0, tau = 0.03f; };
    struct Osc {
        uint64_t start; OscType type; float f0, f1, fdur;
        float g0, peak, ta, gEnd, td, stop;
        bool toMaster; double phase = 0;
    };
    struct Noise {
        uint64_t start; FilterType type; float f0, f1, dur, q, vol;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0, b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };
    struct Grain { uint64_t start; int layer; double pos, step; float age = 0; };
    struct Shared {   // written by the game, read by the audio thread
        float master = 0, layerOn = 0, layerOff = 0, bus = 1, skidGain = 0, skidFreq = 1500;
        float layerTau = 0.03f, skidTau = 0.05f;
        bool grains = false; float hz = 60, load = 0;
    };

    void QueueOsc(float delay, OscType type, float f0, float f1, float fdur, float g0, float peak, float ta, float gEnd, float td, float stop, bool toMaster);
    float MasterLevel() const;
    void Lookup(bool off, float hz, double& pos, float& rec);
    void SpawnGrain(int layer, uint64_t at, float hz);
    static void Callback(void* buffer, unsigned int frames);

    AudioStream stream{};
    Music music{};
    bool ok = false, musicOk = false;
    int vol = 6;
    bool muted = false;
    float uiT = 0;

    // music (main thread)
    bool musicOn = false;
    float musicGain = 0, musicStopT = 0;

    std::mutex m;
    Shared shared;
    std::vector<Osc> pendingOsc;
    std::vector<Noise> pendingNoise;

    // audio thread only
    Shared cur;
    Param pMaster, pOn, pOff, pBus, pSkidG, pSkidF;
    std::vector<Osc> oscs;
    std::vector<Noise> noises;
    std::vector<Grain> grains;
    std::vector<float> engine;   // the recording, mono
    int engineRate = 48000;
    double nextGrain = 0;
    std::atomic<uint64_t> clock{0};
    uint32_t rng = 22222;
    float skx1 = 0, skx2 = 0, sky1 = 0, sky2 = 0;   // tyre bandpass state
    float compEnv = 0;
};
