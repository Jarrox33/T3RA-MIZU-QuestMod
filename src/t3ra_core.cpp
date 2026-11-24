#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
#include "dr_mp3.h"
#include "dr_wav.h"
#include "t3ra_core.hpp"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <map>
#include <vector>

namespace fs = std::filesystem;
namespace t3ra {

struct Note { double b; int x, y, c, d; };
struct Slider { double b; int c, x, y, d; double tb; int tx, ty, tc; };
struct Event { double b; int et, i; };

static std::vector<float> decodeAudioToMono(const std::string& path, const std::string& outWavPath, int& outSr) {
    std::vector<float> mono;
    outSr = 44100;
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    drmp3_uint64 frameCount = 0;
    uint32_t channels = 1, sr = 44100;
    int16_t* pcm16 = nullptr;

    if (ext == ".mp3") {
        drmp3_config cfg{};
        pcm16 = drmp3_open_file_and_read_pcm_frames_s16(path.c_str(), &cfg, &frameCount, nullptr);
        channels = cfg.channels; sr = cfg.sampleRate;
    } else if (ext == ".wav") {
        pcm16 = drwav_open_file_and_read_pcm_frames_s16(path.c_str(), &channels, &sr, &frameCount, nullptr);
    }
    if (!pcm16 || frameCount == 0) return mono;

    outSr = static_cast<int>(sr);
    mono.resize(static_cast<size_t>(frameCount));
    for (size_t i = 0; i < mono.size(); ++i) {
        double sum = 0.0;
        for (uint32_t c = 0; c < channels; ++c) sum += pcm16[i * channels + c];
        mono.at(i) = static_cast<float>((sum / std::max(1u, channels)) / 32768.0);
    }

    // Write standard PCM WAV so Beat Saber plays it natively in VR without ffmpeg
    drwav_data_format fmt{};
    fmt.container = drwav_container_riff;
    fmt.format = DR_WAVE_FORMAT_PCM;
    fmt.channels = channels;
    fmt.sampleRate = sr;
    fmt.bitsPerSample = 16;
    drwav wavOut;
    if (drwav_init_file_write(&wavOut, outWavPath.c_str(), &fmt, nullptr)) {
        drwav_write_pcm_frames(&wavOut, frameCount, pcm16);
        drwav_uninit(&wavOut);
    }
    if (ext == ".mp3") drmp3_free(pcm16, nullptr);
    else drwav_free(pcm16, nullptr);
    return mono;
}
static std::string writeDiffJson(double bpm, const std::vector<Onset>& onsets, const std::string& diff, int preset, int& noteCount) {
    double minGap = (diff == "Hard") ? 0.5 : 0.25;
    double eGate = (diff == "Hard") ? 0.48 : (diff == "Expert") ? 0.30 : 0.18;
    std::vector<Note> notes; std::vector<Slider> sliders; std::vector<Event> events;
    int hand = 0, idx = 0;
    bool fh0 = true, fh1 = true;
    double lastBeat = -10.0;
    Note ln0{-1, 1, 0, 0, 1}, ln1{-1, 2, 0, 1, 1};

    for (const auto& o : onsets) {
        if (o.beat - lastBeat < minGap - 1e-4 || o.energy < eGate) continue;
        bool down = (hand == 0) ? fh0 : fh1;
        bool drop = (o.energy > 0.78 && o.bass > 0.72);
        int lane = (hand == 0) ? 1 : 2, layer = 0, dir = down ? 1 : 0;
        if (drop && diff != "Hard") { lane = (hand == 0) ? 0 : 3; layer = down ? 0 : 2; }
        else if (preset == 2 || (o.energy > 0.55 && idx % 2 == 1)) {
            lane = (hand == 0) ? ((idx % 4 == 0) ? 2 : 0) : ((idx % 4 == 1) ? 1 : 3);
            dir = down ? ((hand == 0) ? 7 : 6) : ((hand == 0) ? 5 : 4);
        }
        Note cur{o.beat, lane, layer, hand, dir};
        notes.push_back(cur);
        Note& prev = (hand == 0) ? ln0 : ln1;
        if (prev.b > 0 && (o.beat - prev.b) >= 0.5 && (o.beat - prev.b) <= 1.0 && diff != "Hard" && idx % 5 == 0)
            sliders.push_back({prev.b, hand, prev.x, prev.y, prev.d, o.beat, cur.x, cur.y, cur.d});
        events.push_back({o.beat, idx % 5, drop ? 11 : ((hand == 0) ? 7 : 3)});
        if (o.bass > 0.80) events.push_back({o.beat, 8, 5});
        prev = cur;
        if (hand == 0) fh0 = !fh0; else fh1 = !fh1;
        hand = 1 - hand; lastBeat = o.beat; ++idx;
    }

    noteCount = static_cast<int>(notes.size());
    std::string j = "{\"version\":\"3.3.0\",\"bpmEvents\":[{\"b\":0,\"m\":" + std::to_string(bpm) + "}],\"colorNotes\":[";
    for (size_t i = 0; i < notes.size(); ++i)
        j += "{\"b\":" + std::to_string(notes.at(i).b) + ",\"x\":" + std::to_string(notes.at(i).x) + ",\"y\":" + std::to_string(notes.at(i).y) + ",\"a\":0,\"c\":" + std::to_string(notes.at(i).c) + ",\"d\":" + std::to_string(notes.at(i).d) + "}" + (i + 1 < notes.size() ? "," : "");
    j += "],\"sliders\":[";
    for (size_t i = 0; i < sliders.size(); ++i)
        j += "{\"b\":" + std::to_string(sliders.at(i).b) + ",\"c\":" + std::to_string(sliders.at(i).c) + ",\"x\":" + std::to_string(sliders.at(i).x) + ",\"y\":" + std::to_string(sliders.at(i).y) + ",\"d\":" + std::to_string(sliders.at(i).d) + ",\"mu\":1,\"tb\":" + std::to_string(sliders.at(i).tb) + ",\"tx\":" + std::to_string(sliders.at(i).tx) + ",\"ty\":" + std::to_string(sliders.at(i).ty) + ",\"tc\":" + std::to_string(sliders.at(i).tc) + ",\"tmu\":1,\"m\":0}" + (i + 1 < sliders.size() ? "," : "");
    j += "],\"obstacles\":[],\"bombNotes\":[],\"basicBeatmapEvents\":[";
    for (size_t i = 0; i < events.size(); ++i)
        j += "{\"b\":" + std::to_string(events.at(i).b) + ",\"et\":" + std::to_string(events.at(i).et) + ",\"i\":" + std::to_string(events.at(i).i) + ",\"f\":1}" + (i + 1 < events.size() ? "," : "");
    j += "],\"useNormalEventsAsCompatibleEvents\":true}";
    return j;
}

BuildResult generateMap(const MapOptions& opts) {
    BuildResult res;
    std::string safe;
    for (char ch : opts.songName) safe += (std::isalnum((unsigned char)ch) || ch == '_' || ch == '-') ? ch : '_';
    if (safe.size() > 32) safe.resize(32);

    fs::path outDir = fs::path(opts.customLevelsDir) / ("T3RA_VR_" + safe);
    fs::create_directories(outDir);
    int sr = 44100;
    auto samples = decodeAudioToMono(opts.audioPath, (outDir / "song.wav").string(), sr);
    if (samples.empty()) { res.message = "Use .mp3 or .wav file!"; return res; }

    double duration = static_cast<double>(samples.size()) / sr;
    int hop = std::max(512, sr / 43);
    int nFrames = static_cast<int>(samples.size()) / hop;
    std::vector<double> lowE(nFrames, 0.0), totE(nFrames, 0.0), flux(nFrames, 0.0);
    double lp = 0.0;
    for (int f = 0; f < nFrames; ++f) {
        double sumTot = 0.0, sumLow = 0.0;
        for (int k = 0; k < hop; ++k) {
            double x = samples.at(f * hop + k);
            lp += 0.04 * (x - lp);
            sumLow += std::abs(lp); sumTot += std::abs(x);
        }
        lowE.at(f) = sumLow / hop;
        totE.at(f) = (sumTot / hop) + lowE.at(f) * 0.8;
        if (f > 0) flux.at(f) = std::max(0.0, totE.at(f) - totE.at(f - 1));
    }

    double fps = static_cast<double>(sr) / hop;
    int minLag = static_cast<int>(fps * 60.0 / 215.0), maxLag = static_cast<int>(fps * 60.0 / 85.0), bestLag = minLag;
    double bestAc = -1.0;
    for (int lag = minLag; lag < maxLag && lag < nFrames / 2; ++lag) {
        double s = 0.0;
        for (int i = 0; i < nFrames - lag; ++i) s += flux.at(i) * flux.at(i + lag);
        if (s > bestAc) { bestAc = s; bestLag = lag; }
    }
    double bpm = (60.0 * fps) / std::max(bestLag, 1);
    while (bpm < 132.0) bpm *= 2.0;
    bpm = std::round(bpm * 10.0) / 10.0;

    double spb = 60.0 / bpm, bestPhase = 0.0, bestScore = -1.0;
    for (int step = 0; step < 32; ++step) {
        double cand = (step / 32.0) * spb, sc = 0.0;
        for (int f = 0; f < nFrames; ++f) {
            double t = (f * hop) / static_cast<double>(sr);
            double dist = std::abs(((t - cand) / spb) - std::round((t - cand) / spb));
            if (dist < 0.08) sc += flux.at(f);
        }
        if (sc > bestScore) { bestScore = sc; bestPhase = cand; }
    }

    std::vector<double> sortedE = totE, sortedL = lowE;
    std::sort(sortedE.begin(), sortedE.end()); std::sort(sortedL.begin(), sortedL.end());
    double p95E = sortedE.at((size_t)(sortedE.size() * 0.95)) + 1e-6;
    double p95L = sortedL.at((size_t)(sortedL.size() * 0.95)) + 1e-6;

    std::map<double, Onset> grid;
    for (int f = 1; f < nFrames - 1; ++f) {
        if (flux.at(f) <= flux.at(f - 1) || flux.at(f) < flux.at(f + 1)) continue;
        double tSec = (f * hop) / static_cast<double>(sr) - bestPhase;
        if (tSec < 2.0 || tSec > duration - 2.0) continue;
        double qBeat = std::round(((tSec * bpm) / 60.0) * 4.0) / 4.0;
        if (qBeat < 4.0) continue;
        double e = std::min(1.5, totE.at(f) / p95E), b = std::min(1.5, lowE.at(f) / p95L);
        if (!grid.count(qBeat) || e > grid.at(qBeat).energy) grid[qBeat] = {qBeat, e, b};
    }

    std::vector<Onset> onsets;
    for (auto& kv : grid) onsets.push_back(kv.second);
    int epNotes = 0, exNotes = 0;
    std::ofstream(outDir / "ExpertPlusStandard.dat") << writeDiffJson(bpm, onsets, "ExpertPlus", opts.preset, epNotes);
    std::ofstream(outDir / "ExpertStandard.dat") << writeDiffJson(bpm, onsets, "Expert", opts.preset, exNotes);

    std::ofstream(outDir / "Info.dat") << "{\n  \"_version\": \"2.1.0\",\n  \"_songName\": \"" << opts.songName << "\",\n"
         << "  \"_songSubName\": \"Mapped in VR by T3RA//MIZU\",\n  \"_songAuthorName\": \"" << opts.artistName << "\",\n"
         << "  \"_levelAuthorName\": \"T3RA//MIZU VR\",\n  \"_beatsPerMinute\": " << bpm << ",\n"
         << "  \"_shuffle\": 0, \"_shufflePeriod\": 0.5, \"_previewStartTime\": 20, \"_previewDuration\": 12,\n"
         << "  \"_songFilename\": \"song.wav\", \"_coverImageFilename\": \"\",\n"
         << "  \"_environmentName\": \"DefaultEnvironment\", \"_songTimeOffset\": 0,\n"
         << "  \"_difficultyBeatmapSets\": [{\"_beatmapCharacteristicName\": \"Standard\",\"_difficultyBeatmaps\": [\n"
         << "    {\"_difficulty\": \"ExpertPlus\",\"_difficultyRank\": 9,\"_beatmapFilename\": \"ExpertPlusStandard.dat\",\"_noteJumpMovementSpeed\": 18.0,\"_noteJumpStartBeatOffset\": 0.0},\n"
         << "    {\"_difficulty\": \"Expert\",\"_difficultyRank\": 7,\"_beatmapFilename\": \"ExpertStandard.dat\",\"_noteJumpMovementSpeed\": 16.0,\"_noteJumpStartBeatOffset\": 0.0}\n"
         << "  ]}]\n}\n";

    res.ok = true; res.bpm = bpm; res.phaseMs = std::round(bestPhase * 10000.0) / 10.0;
    res.expertPlusNotes = epNotes; res.outputDir = outDir.string();
    return res;
}
}
