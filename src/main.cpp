#include "t3ra_core.hpp"
#include "bs_suite.hpp"
#include "scotland2/shared/modloader.h"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "songcore/shared/SongCore.hpp"
#include <filesystem>
#include <vector>

#ifndef MOD_ID
#define MOD_ID "t3ra-mizu"
#endif
#ifndef VERSION
#define VERSION "0.1.0"
#endif

namespace fs = std::filesystem;
static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};
static int selectedIndex = 0, selectedPreset = 1, patternMode = 1;
static float patternBpm = 200.0f;
static std::vector<fs::path> audioFiles;

static void refreshAudioList() {
    audioFiles.clear();
    fs::path dl("/sdcard/Download");
    if (!fs::exists(dl)) return;
    for (const auto& entry : fs::directory_iterator(dl)) {
        auto ext = entry.path().extension().string();
        if (ext == ".mp3" || ext == ".ogg" || ext == ".wav" || ext == ".m4a") audioFiles.push_back(entry.path());
    }
}

static void didActivate(HMUI::ViewController* self, bool firstActivation, bool, bool) {
    if (!firstActivation) return;
    auto* parent = BSML::Lite::CreateScrollableSettingsContainer(self->get_transform());
    BSML::Lite::CreateText(parent, "=== T3RA//MIZU All-in-One C++ VR Suite ===");
    refreshAudioList();
    auto* statusText = BSML::Lite::CreateText(parent, audioFiles.empty() ? "Ready (put .mp3/.ogg in /sdcard/Download)" : "Audio: " + audioFiles.front().filename().string());

    BSML::Lite::CreateUIButton(parent, "Cycle Audio File (/sdcard/Download)", [statusText]() {
        refreshAudioList();
        if (audioFiles.empty()) { statusText->set_text("No audio files in /sdcard/Download"); return; }
        selectedIndex = (selectedIndex + 1) % (int)audioFiles.size();
        statusText->set_text("Selected: " + audioFiles.at(selectedIndex).filename().string());
    });

    BSML::Lite::CreateIncrementSetting(parent, "Auto-Mapper Preset (1=Balanced, 2=Tech, 3=Speed)", 0, 1, 1, 1, 3, [](float v) { selectedPreset = (int)v; });
    BSML::Lite::CreateUIButton(parent, "Run T3RA//MIZU Auto-Mapper", [statusText]() {
        if (audioFiles.empty()) { statusText->set_text("No audio files in /sdcard/Download!"); return; }
        t3ra::MapOptions opts{audioFiles.at(selectedIndex).string(), audioFiles.at(selectedIndex).stem().string(), "T3RA//MIZU VR", selectedPreset};
        auto res = t3ra::generateMap(opts);
        SongCore::API::Loading::RefreshSongs(false);
        statusText->set_text(res.ok ? ("Mapped " + std::to_string(res.expertPlusNotes) + " notes @ " + std::to_string((int)res.bpm) + " BPM") : res.message);
    });

    BSML::Lite::CreateIncrementSetting(parent, "Pattern Drill Mode (1-6)", 0, 1, 1, 1, 6, [](float v) { patternMode = (int)v; });
    BSML::Lite::CreateIncrementSetting(parent, "Pattern Trainer BPM", 0, 10, 200, 120, 320, [](float v) { patternBpm = v; });
    BSML::Lite::CreateUIButton(parent, "Generate C++ Pattern Drill (60s)", [statusText]() { statusText->set_text(bssuite::runPatternTrainer(patternMode, patternBpm, 60)); SongCore::API::Loading::RefreshSongs(false); });
    BSML::Lite::CreateUIButton(parent, "Randomize HitScoreVisualizer Neon Colors", [statusText]() { statusText->set_text(bssuite::randomizeHSV()); });
    BSML::Lite::CreateUIButton(parent, "Build Auto-Playlists + Geometric Covers", [statusText]() { statusText->set_text(bssuite::buildAllPlaylists()); });
    BSML::Lite::CreateUIButton(parent, "Speed-Up Latest T3RA Map (1.20x)", [statusText]() { statusText->set_text(bssuite::speedupLatestMap(1.20)); SongCore::API::Loading::RefreshSongs(false); });
    BSML::Lite::CreateUIButton(parent, "Generate 10-Min Shaffuru Endurance Drill", [statusText]() { statusText->set_text(bssuite::buildShaffuruMedley(10, 60)); SongCore::API::Loading::RefreshSongs(false); });
}

extern "C" void setup(CModInfo* info) { *info = modInfo.to_c(); Paper::Logger::RegisterFileContextId(MOD_ID); }
extern "C" void late_load() { il2cpp_functions::Init(); BSML::Init(); BSML::Register::RegisterMainMenu("T3RA//MIZU", "T3RA//MIZU", "All-in-One C++ Beat Saber Suite", didActivate); }
