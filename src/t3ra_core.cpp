#include "t3ra_core.hpp"
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <map>
#include <cstdlib>

namespace fs = std::filesystem;

namespace t3ra {

struct Note { double b; int x, y, c, d; };
struct Slider { double b; int c, x, y, d; double tb; int tx, ty, tc; };
struct Event { double b; int et, i; };

static std::vector<float> decodeWavMono(const std::string& wavPath, int& sampleRate) {
    std::ifstream in(wavPath, std::ios::binary);
    std::vector<float> out;
    if (!in) return out;
    char hdr[44];
    in.read(hdr, 44);
    sampleRate = *reinterpret_cast<int32_t*>(&hdr[24]);
    int16_t s;
    while (in.read(reinterpret_cast<char*>(&s), sizeof(s))) {
        out.push_back(static_cast<float>(s) / 32768.0f);
    }
    return out;
}
static std::string writeDiffJson(double bpm, const std::vector<Onset>& onsets, const std::string& diff, int preset, int& noteCount) {
    double minGap = (diff == "Hard") ? 0.5 : 0.25;
    double eGate = (diff == "Hard") ? 0.48 : (diff == "Expert") ? 0.30 : 0.18;
    std::vector<Note> notes;
    std::vector<Slider> sliders;
    std::vector<Event> events;

    int hand = 0;
    bool forehand[2] = {true, true};
    double lastBeat = -10.0;
    Note lastNote[2] = {{-1, 1, 0, 0, 1}, {-1, 2, 0, 1, 1}};
    int idx = 0;

    for (const auto& o : onsets) {
        if (o.beat - lastBeat < minGap - 1e-4) continue;
        if (o.energy < eGate) continue;

        bool down = forehand[hand];
        bool drop = (o.energy > 0.78 && o.bass > 0.72);
        int lane = (hand == 0) ? 1 : 2;
        int layer = 0;
        int dir = down ? 1 : 0;

        if (drop && diff != "Hard") {
            lane = (hand == 0) ? 0 : 3;
            layer = down ? 0 : 2;
        } else if (preset == 2 || (o.energy > 0.55 && idx % 2 == 1)) {
            lane = (hand == 0) ? ((idx % 4 == 0) ? 2 : 0) : ((idx % 4 == 1) ? 1 : 3);
            dir = down ? ((hand == 0) ? 7 : 6) : ((hand == 0) ? 5 : 4);
        }

        Note cur{o.beat, lane, layer, hand, dir};
        notes.push_back(cur);

        if (lastNote[hand].b > 0 && (o.beat - lastNote[hand].b) >= 0.5 && (o.beat - lastNote[hand].b) <= 1.0 && diff != "Hard" && idx % 5 == 0) {
            sliders.push_back({lastNote[hand].b, hand, lastNote[hand].x, lastNote[hand].y, lastNote[hand].d, o.beat, cur.x, cur.y, cur.d});
        }

        events.push_back({o.beat, idx % 5, drop ? 11 : ((hand == 0) ? 7 : 3)});
        if (o.bass > 0.80) events.push_back({o.beat, 8, 5});

        lastNote[hand] = cur;
        forehand[hand] = !forehand[hand];
        hand = 1 - hand;
        lastBeat = o.beat;
        ++idx;
    }

    noteCount = static_cast<int>(notes.size());
    std::string j = "{\"version\":\"3.3.0\",\"bpmEvents\":[{\"b\":0,\"m\":" + std::to_string(bpm) + "}],\"colorNotes\":[";
    for (size_t i = 0; i < notes.size(); ++i) {
        j += "{\"b\":" + std::to_string(notes[i].b) + ",\"x\":" + std::to_string(notes[i].x) +
             ",\"y\":" + std::to_string(notes[i].y) + ",\"a\":0,\"c\":" + std::to_string(notes[i].c) +
             ",\"d\":" + std::to_string(notes[i].d) + "}" + (i + 1 < notes.size() ? "," : "");
    }
    j += "],\"sliders\":[";
    for (size_t i = 0; i < sliders.size(); ++i) {
        j += "{\"b\":" + std::to_string(sliders[i].b) + ",\"c\":" + std::to_string(sliders[i].c) +
             ",\"x\":" + std::to_string(sliders[i].x) + ",\"y\":" + std::to_string(sliders[i].y) +
             ",\"d\":" + std::to_string(sliders[i].d) + ",\"mu\":1,\"tb\":" + std::to_string(sliders[i].tb) +
             ",\"tx\":" + std::to_string(sliders[i].tx) + ",\"ty\":" + std::to_string(sliders[i].ty) +
             ",\"tc\":" + std::to_string(sliders[i].tc) + ",\"tmu\":1,\"m\":0}" + (i + 1 < sliders.size() ? "," : "");
    }
    j += "],\"obstacles\":[],\"bombNotes\":[],\"basicBeatmapEvents\":[";
    for (size_t i = 0; i < events.size(); ++i) {
        j += "{\"b\":" + std::to_string(events[i].b) + ",\"et\":" + std::to_string(events[i].et) +
             ",\"i\":" + std::to_string(events[i].i) + ",\"f\":1}" + (i + 1 < events.size() ? "," : "");
    }
    j += "],\"useNormalEventsAsCompatibleEvents\":true}";
    return j;
}
BuildResult generateMap(const MapOptions& opts) {
    BuildResult res;
    std::string safe;
    for (char ch : opts.songName) safe += (std::isalnum((unsigned char)ch) || ch == '_' || ch == '-') ? ch : '_';
    if (safe.size() > 32) safe.resize(32);

    fs::path outDir = fs::path(opts.customLevelsDir) / ("T3RA_CPP_" + safe);
    fs::create_directories(outDir);

    std::string tmpWav = (outDir / "tmp_scan.wav").string();
    std::string eggPath = (outDir / "song.egg").string();
    std::system(("ffmpeg -y -i \"" + opts.audioPath + "\" -ac 1 -ar 22050 -vn \"" + tmpWav + "\" >/dev/null 2>&1").c_str());
    std::system(("ffmpeg -y -i \"" + opts.audioPath + "\" -vn -ar 44100 -ac 2 -c:a libvorbis -q:a 5 -f ogg \"" + eggPath + "\" >/dev/null 2>&1").c_str());

    int sr = 22050;
    auto samples = decodeWavMono(tmpWav, sr);
    fs::remove(tmpWav);
    if (samples.empty()) { res.message = "Failed to decode audio."; return res; }

    double duration = static_cast<double>(samples.size()) / sr;
    int hop = 512;
    int nFrames = static_cast<int>(samples.size()) / hop;
    std::vector<double> lowE(nFrames, 0.0), totE(nFrames, 0.0), flux(nFrames, 0.0);
    double lp = 0.0;
    for (int f = 0; f < nFrames; ++f) {
        double sumTot = 0.0, sumLow = 0.0;
        for (int k = 0; k < hop; ++k) {
            double x = samples[f * hop + k];
            lp += 0.08 * (x - lp); // 1-pole sub-bass lowpass filter
            sumLow += std::abs(lp);
            sumTot += std::abs(x);
        }
        lowE[f] = sumLow / hop;
        totE[f] = (sumTot / hop) + lowE[f] * 0.8;
        if (f > 0) flux[f] = std::max(0.0, totE[f] - totE[f - 1]);
    }

    double fps = static_cast<double>(sr) / hop;
    int minLag = static_cast<int>(fps * 60.0 / 215.0);
    int maxLag = static_cast<int>(fps * 60.0 / 85.0);
    int bestLag = minLag;
    double bestAc = -1.0;
    for (int lag = minLag; lag < maxLag && lag < nFrames / 2; ++lag) {
        double s = 0.0;
        for (int i = 0; i < nFrames - lag; ++i) s += flux[i] * flux[i + lag];
        if (s > bestAc) { bestAc = s; bestLag = lag; }
    }
    double bpm = (60.0 * fps) / std::max(bestLag, 1);
    while (bpm < 132.0) bpm *= 2.0;
    bpm = std::round(bpm * 10.0) / 10.0;

    double spb = 60.0 / bpm;
    double bestPhase = 0.0, bestScore = -1.0;
    for (int step = 0; step < 32; ++step) {
        double cand = (step / 32.0) * spb;
        double sc = 0.0;
        for (int f = 0; f < nFrames; ++f) {
            double t = (f * hop) / static_cast<double>(sr);
            double dist = std::abs(((t - cand) / spb) - std::round((t - cand) / spb));
            if (dist < 0.08) sc += flux[f];
        }
        if (sc > bestScore) { bestScore = sc; bestPhase = cand; }
    }

    std::vector<double> sortedE = totE, sortedL = lowE;
    std::sort(sortedE.begin(), sortedE.end());
    std::sort(sortedL.begin(), sortedL.end());
    double p95E = sortedE[(size_t)(sortedE.size() * 0.95)] + 1e-6;
    double p95L = sortedL[(size_t)(sortedL.size() * 0.95)] + 1e-6;

    std::map<double, Onset> grid;
    for (int f = 1; f < nFrames - 1; ++f) {
        if (flux[f] <= flux[f - 1] || flux[f] < flux[f + 1]) continue;
        double tSec = (f * hop) / static_cast<double>(sr) - bestPhase;
        if (tSec < 2.0 || tSec > duration - 2.0) continue;
        double qBeat = std::round(((tSec * bpm) / 60.0) * 4.0) / 4.0;
        if (qBeat < 4.0) continue;
        double e = std::min(1.5, totE[f] / p95E);
        double b = std::min(1.5, lowE[f] / p95L);
        if (!grid.count(qBeat) || e > grid[qBeat].energy) grid[qBeat] = {qBeat, e, b};
    }

    std::vector<Onset> onsets;
    for (auto& kv : grid) onsets.push_back(kv.second);

    int epNotes = 0, exNotes = 0;
    std::ofstream(outDir / "ExpertPlusStandard.dat") << writeDiffJson(bpm, onsets, "ExpertPlus", opts.preset, epNotes);
    std::ofstream(outDir / "ExpertStandard.dat") << writeDiffJson(bpm, onsets, "Expert", opts.preset, exNotes);

    std::ofstream info(outDir / "Info.dat");
    info << "{\n  \"_version\": \"2.1.0\",\n  \"_songName\": \"" << opts.songName << "\",\n"
         << "  \"_songSubName\": \"Mapped by T3RA//MIZU C++ Core\",\n  \"_songAuthorName\": \"" << opts.artistName << "\",\n"
         << "  \"_levelAuthorName\": \"T3RA//MIZU C++\",\n  \"_beatsPerMinute\": " << bpm << ",\n"
         << "  \"_shuffle\": 0, \"_shufflePeriod\": 0.5, \"_previewStartTime\": 20, \"_previewDuration\": 12,\n"
         << "  \"_songFilename\": \"song.egg\", \"_coverImageFilename\": \"\",\n"
         << "  \"_environmentName\": \"DefaultEnvironment\", \"_songTimeOffset\": 0,\n"
         << "  \"_difficultyBeatmapSets\": [{\"_beatmapCharacteristicName\": \"Standard\",\"_difficultyBeatmaps\": [\n"
         << "    {\"_difficulty\": \"ExpertPlus\",\"_difficultyRank\": 9,\"_beatmapFilename\": \"ExpertPlusStandard.dat\",\"_noteJumpMovementSpeed\": 18.0,\"_noteJumpStartBeatOffset\": 0.0},\n"
         << "    {\"_difficulty\": \"Expert\",\"_difficultyRank\": 7,\"_beatmapFilename\": \"ExpertStandard.dat\",\"_noteJumpMovementSpeed\": 16.0,\"_noteJumpStartBeatOffset\": 0.0}\n"
         << "  ]}]\n}\n";

    res.ok = true;
    res.bpm = bpm;
    res.phaseMs = std::round(bestPhase * 10000.0) / 10.0;
    res.expertPlusNotes = epNotes;
    res.outputDir = outDir.string();
    return res;
}
}
