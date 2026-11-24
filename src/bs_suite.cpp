#include "bs_suite.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <random>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace fs = std::filesystem;
static const std::string LEVELS_DIR = "/sdcard/ModData/com.beatgames.beatsaber/Mods/SongCore/CustomLevels";
static const std::string PL_DIR     = "/sdcard/ModData/com.beatgames.beatsaber/Mods/PlaylistManager/Playlists";
static const std::string COV_DIR    = "/sdcard/ModData/com.beatgames.beatsaber/Mods/PlaylistManager/Covers";
static const std::string HSV_PATH   = "/sdcard/ModData/com.beatgames.beatsaber/Mods/HitScoreVisualizer/HitScoreVisualizerConfig.json";

namespace bssuite {

static void writeSynthBeatWav(const std::string& wavPath, double bpm, int seconds) {
    const int sr = 44100, total = sr * seconds;
    const double spb = 60.0 / bpm;
    std::vector<int16_t> pcm(total, 0);
    for (int i = 0; i < total; ++i) {
        double t = (double)i / sr, bPos = std::fmod(t, spb), hPos = std::fmod(t, spb * 0.5), s = 0.0;
        if (bPos < 0.14) s += 0.65 * std::exp(-bPos * 28.0) * std::sin(6.2831853 * (52.0 + 110.0 * std::exp(-bPos * 45.0)) * bPos);
        if (hPos < 0.045) { uint32_t n = (uint32_t)(i * 1103515245u + 12345u); s += 0.22 * std::exp(-hPos * 85.0) * (((n >> 16) & 0x7FFF) / 16384.0 - 1.0); }
        pcm.at(i) = (int16_t)(std::max(-1.0, std::min(1.0, s)) * 30000.0);
    }
    std::ofstream out(wavPath, std::ios::binary);
    int32_t dataSz = total * 2, fileSz = 36 + dataSz, fmtLen = 16, byteRate = sr * 2;
    int16_t fmt = 1, ch = 1, align = 2, bits = 16;
    out.write("RIFF", 4); out.write((char*)&fileSz, 4); out.write("WAVEfmt ", 8);
    out.write((char*)&fmtLen, 4); out.write((char*)&fmt, 2); out.write((char*)&ch, 2);
    out.write((char*)&sr, 4); out.write((char*)&byteRate, 4); out.write((char*)&align, 2);
    out.write((char*)&bits, 2); out.write("data", 4); out.write((char*)&dataSz, 4);
    out.write((char*)pcm.data(), dataSz);
}
std::string runPatternTrainer(int mode, double bpm, int seconds) {
    std::string modeName = (mode == 2) ? "Jumps" : (mode == 3) ? "TechCrossovers" : (mode == 4) ? "Bursts" : (mode == 5) ? "DiagonalWindows" : (mode == 6) ? "WallDodgeStreams" : "Streams";
    fs::path outDir = fs::path(LEVELS_DIR) / ("CPP_VR_" + modeName + "_" + std::to_string((int)bpm) + "BPM");
    fs::create_directories(outDir);
    writeSynthBeatWav((outDir / "song.wav").string(), bpm, seconds);

    double totalBeats = (seconds * bpm) / 60.0, step = (mode == 4) ? 0.25 : 0.5;
    std::string notesJson, wallsJson;
    int idx = 0, count = 0;
    for (double b = 4.0; b < totalBeats - 2.0; b += step, ++idx) {
        if (mode == 4 && (idx % 8) >= 4) continue;
        int hand = idx % 2; bool down = ((idx / 2) % 2 == 0);
        int lane = (hand == 0) ? 1 : 2, layer = 0, dir = down ? 1 : 0;
        if (mode == 2) { lane = (hand == 0) ? 0 : 3; layer = down ? 0 : 2; }
        else if (mode == 3) { lane = (hand == 0) ? ((idx % 4 == 0) ? 2 : 0) : ((idx % 4 == 1) ? 1 : 3); dir = down ? ((hand == 0) ? 7 : 6) : ((hand == 0) ? 5 : 4); }
        else if (mode == 5) { lane = down ? ((hand == 0) ? 0 : 3) : ((hand == 0) ? 1 : 2); layer = down ? 0 : 2; dir = down ? ((hand == 0) ? 6 : 7) : ((hand == 0) ? 5 : 4); }
        else if (mode == 6) { int sh = ((int)(b / 8.0) % 2 == 0) ? 1 : 0; lane += sh; if (std::fmod(b, 8.0) < 0.01) wallsJson += (wallsJson.empty() ? "" : ",") + std::string("{\"_time\":") + std::to_string(b) + ",\"_lineIndex\":" + std::to_string(sh == 1 ? 0 : 2) + ",\"_type\":0,\"_duration\":4.0,\"_width\":2}"; }
        notesJson += (notesJson.empty() ? "" : ",") + std::string("{\"_time\":") + std::to_string(b) + ",\"_lineIndex\":" + std::to_string(lane) + ",\"_lineLayer\":" + std::to_string(layer) + ",\"_type\":" + std::to_string(hand) + ",\"_cutDirection\":" + std::to_string(dir) + "}";
        ++count;
    }
    std::ofstream(outDir / "ExpertPlusStandard.dat") << "{\"_version\":\"2.2.0\",\"_notes\":[" << notesJson << "],\"_obstacles\":[" << wallsJson << "],\"_events\":[]}";
    std::ofstream(outDir / "Info.dat") << "{\"_version\":\"2.0.0\",\"_songName\":\"C++ " << modeName << " (" << (int)bpm << " BPM)\",\"_songSubName\":\"T3RA//MIZU VR Suite\",\"_songAuthorName\":\"C++ Synth\",\"_levelAuthorName\":\"Jarrox33\",\"_beatsPerMinute\":" << bpm << ",\"_shuffle\":0,\"_shufflePeriod\":0.5,\"_previewStartTime\":0,\"_previewDuration\":10,\"_songFilename\":\"song.wav\",\"_coverImageFilename\":\"\",\"_environmentName\":\"DefaultEnvironment\",\"_songTimeOffset\":0,\"_difficultyBeatmapSets\":[{\"_beatmapCharacteristicName\":\"Standard\",\"_difficultyBeatmaps\":[{\"_difficulty\":\"ExpertPlus\",\"_difficultyRank\":9,\"_beatmapFilename\":\"ExpertPlusStandard.dat\",\"_noteJumpMovementSpeed\":18.0,\"_noteJumpStartBeatOffset\":0.0}]}]}";
    return "Pattern Trainer: " + std::to_string(count) + " notes (" + modeName + ")";
}

std::string randomizeHSV() {
    fs::create_directories(fs::path(HSV_PATH).parent_path());
    std::random_device rd; std::mt19937 gen(rd()); std::uniform_real_distribution<float> d(0.15f, 1.0f);
    auto col = [&]() { char buf[48]; std::snprintf(buf, sizeof(buf), "[%.2f,%.2f,%.2f,1.0]", d(gen), d(gen), d(gen)); return std::string(buf); };
    std::ofstream(HSV_PATH) << "{\"majorVersion\":3,\"minorVersion\":3,\"patchVersion\":0,\"useFixedPos\":false,\"judgments\":[{\"threshold\":115,\"text\":\"%s\",\"color\":" << col() << ",\"fade\":true},{\"threshold\":110,\"text\":\"%s\",\"color\":" << col() << ",\"fade\":true},{\"threshold\":100,\"text\":\"%s\",\"color\":" << col() << ",\"fade\":true},{\"threshold\":0,\"text\":\"%s\",\"color\":" << col() << ",\"fade\":true}]}";
    return "HSV Neon Colors Randomized!";
}
static void writeGeometricBmp(const std::string& bmpPath, uint32_t seed) {
    const int W = 256, H = 256; std::mt19937 rng(seed);
    uint8_t r1 = 40 + (rng() % 215), g1 = 40 + (rng() % 215), b1 = 40 + (rng() % 215);
    uint8_t r2 = 40 + (rng() % 215), g2 = 40 + (rng() % 215), b2 = 40 + (rng() % 215);
    std::vector<uint8_t> px(W * H * 3, 14);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        int dist = (int)std::sqrt((x - 128) * (x - 128) + (y - 128) * (y - 128));
        size_t i = (y * W + x) * 3;
        if (x % 32 == 0 || y % 32 == 0) { px.at(i) = b1 / 3; px.at(i + 1) = g1 / 3; px.at(i + 2) = r1 / 3; }
        if (std::abs(dist - 95) < 4 || std::abs(dist - 55) < 4) { px.at(i) = b1; px.at(i + 1) = g1; px.at(i + 2) = r1; }
        if (std::abs((x - y) + 25) < 5 || std::abs((x - y) - 25) < 5) { px.at(i) = b2; px.at(i + 1) = g2; px.at(i + 2) = r2; }
    }
    std::ofstream out(bmpPath, std::ios::binary);
    uint32_t imgSz = W * H * 3, fileSz = 54 + imgSz;
    uint8_t hdr[54] = {'B','M',0,0,0,0,0,0,0,0,54,0,0,0,40,0,0,0,0,1,0,0,0,1,0,0,1,0,24,0};
    *reinterpret_cast<uint32_t*>(hdr + 2) = fileSz; *reinterpret_cast<uint32_t*>(hdr + 34) = imgSz;
    out.write((char*)hdr, 54); out.write((char*)px.data(), imgSz);
}

std::string buildAllPlaylists() {
    fs::create_directories(PL_DIR); fs::create_directories(COV_DIR);
    std::vector<std::string> fast; int scanned = 0;
    if (fs::exists(LEVELS_DIR)) for (const auto& d : fs::directory_iterator(LEVELS_DIR)) {
        if (!d.is_directory()) continue;
        fs::path p = fs::exists(d.path() / "Info.dat") ? (d.path() / "Info.dat") : (d.path() / "info.dat");
        if (!fs::exists(p)) continue;
        std::ifstream in(p); std::string txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        auto pos = txt.find("_beatsPerMinute");
        if (pos != std::string::npos) { ++scanned; if (std::atof(txt.c_str() + txt.find(':', pos) + 1) >= 200.0) fast.push_back(d.path().filename().string()); }
    }
    writeGeometricBmp((fs::path(COV_DIR) / "T3RA_Speed_200.png").string(), 2002026);
    std::ofstream pl(fs::path(PL_DIR) / "T3RA_Speed_200.bplist");
    pl << "{\"playlistTitle\":\"T3RA 200+ BPM Speed\",\"playlistAuthor\":\"T3RA//MIZU VR\",\"songs\":[";
    for (size_t i = 0; i < fast.size(); ++i) pl << "{\"songName\":\"" << fast.at(i) << "\"}" << (i + 1 < fast.size() ? "," : "");
    pl << "]}";
    return "Scanned " + std::to_string(scanned) + " maps -> Built T3RA Playlists!";
}

std::string speedupLatestMap(double mult) {
    if (!fs::exists(LEVELS_DIR)) return "CustomLevels not found.";
    fs::path latest;
    for (const auto& d : fs::directory_iterator(LEVELS_DIR)) if (d.is_directory() && d.path().filename().string().find("T3RA_") != std::string::npos) latest = d.path();
    if (latest.empty()) return "Generate a T3RA map first!";
    fs::path dst = fs::path(LEVELS_DIR) / (latest.filename().string() + "_" + std::to_string((int)(mult * 100)) + "pct");
    fs::create_directories(dst);
    for (const auto& f : fs::directory_iterator(latest)) fs::copy_file(f.path(), dst / f.path().filename(), fs::copy_options::overwrite_existing);
    fs::path p = dst / "Info.dat"; std::ifstream in(p); std::string txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto pos = txt.find("_beatsPerMinute");
    if (pos != std::string::npos) { auto col = txt.find(':', pos), end = txt.find(',', col); txt.replace(col + 1, end - col - 1, " " + std::to_string(std::atof(txt.c_str() + col + 1) * mult)); std::ofstream(p) << txt; }
    return "Created " + std::to_string((int)(mult * 100)) + "% speed map!";
}

std::string buildShaffuruMedley(int sliceCount, int sliceSec) { return runPatternTrainer(3, 195.0, sliceCount * sliceSec); }
}
