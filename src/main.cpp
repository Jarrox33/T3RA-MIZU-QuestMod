#include "t3ra_core.hpp"
#include "bs_suite.hpp"
#include "scotland2/shared/modloader.h"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "songcore/shared/SongCore.hpp"
#include "UnityEngine/UI/VerticalLayoutGroup.hpp"
#include "UnityEngine/UI/LayoutElement.hpp"
#include <filesystem>
#include <vector>

#define MOD_ID "t3ra-mizu"
#define VERSION "0.2.0"

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
        if (ext == ".mp3" || ext == ".wav") audioFiles.push_back(entry.path());
    }
}

static void didActivate(HMUI::ViewController* self, bool firstActivation, bool, bool) {
    if (!firstActivation) return;
    auto* parent = BSML::Lite::CreateScrollableSettingsContainer(self->get_transform());
    if (auto* vlg = parent->GetComponent<UnityEngine::UI::VerticalLayoutGroup*>()) {
        vlg->set_spacing(2.5f);
        vlg->set_childForceExpandHeight(false);
    }

    auto addRowHeight = [](UnityEngine::Component* comp, float h) {
        if (!comp) return;
        auto* le = comp->get_gameObject()->AddComponent<UnityEngine::UI::LayoutElement*>();
        le->set_preferredHeight(h);
        le->set_minHeight(h);
    };

    refreshAudioList();
    auto* statusText = BSML::Lite::CreateText(parent, audioFiles.empty()
        ? "Put .mp3 or .wav in /sdcard/Download"
        : "File: " + audioFiles.front().filename().string());
    statusText->set_fontSize(3.6f);
    addRowHeight(statusText, 7.0f);

    addRowHeight(BSML::Lite::CreateUIButton(parent, "Next Audio File (/sdcard/Download)", [statusText]() {
        refreshAudioList();
        if (audioFiles.empty()) { statusText->set_text("No .mp3 or .wav files in /sdcard/Download"); return; }
        selectedIndex = (selectedIndex + 1) % (int)audioFiles.size();
        statusText->set_text("File: " + audioFiles.at(selectedIndex).filename().string());
    }), 8.5f);

    BSML::Lite::CreateIncrementSetting(parent, "Auto-Mapper Preset (1=Balanced, 2=Tech, 3=Speed)", 0, 1, 1, 1, 3, [](float v) { selectedPreset = (int)v; });
    addRowHeight(BSML::Lite::CreateUIButton(parent, "Run T3RA//MIZU Auto-Mapper", [statusText]() {
        if (audioFiles.empty()) { statusText->set_text("No .mp3 or .wav files found!"); return; }
        t3ra::MapOptions opts{audioFiles.at(selectedIndex).string(), audioFiles.at(selectedIndex).stem().string(), "T3RA//MIZU VR", selectedPreset};
        auto res = t3ra::generateMap(opts);
        SongCore::API::Loading::RefreshSongs(false);
        statusText->set_text(res.ok ? ("Mapped " + std::to_string(res.expertPlusNotes) + " notes @ " + std::to_string((int)res.bpm) + " BPM!") : res.message);
    }), 8.5f);

    BSML::Lite::CreateIncrementSetting(parent, "Pattern Drill Mode (1-6)", 0, 1, 1, 1, 6, [](float v) { patternMode = (int)v; });
    BSML::Lite::CreateIncrementSetting(parent, "Pattern Trainer BPM", 0, 10, 200, 120, 320, [](float v) { patternBpm = v; });
    addRowHeight(BSML::Lite::CreateUIButton(parent, "Generate C++ Pattern Drill (60s)", [statusText]() { statusText->set_text(bssuite::runPatternTrainer(patternMode, patternBpm, 60)); SongCore::API::Loading::RefreshSongs(false); }), 8.5f);
    addRowHeight(BSML::Lite::CreateUIButton(parent, "Randomize HitScoreVisualizer Neon Colors", [statusText]() { statusText->set_text(bssuite::randomizeHSV()); }), 8.5f);
    addRowHeight(BSML::Lite::CreateUIButton(parent, "Build Auto-Playlists + Geometric Covers", [statusText]() { statusText->set_text(bssuite::buildAllPlaylists()); }), 8.5f);
    addRowHeight(BSML::Lite::CreateUIButton(parent, "Speed-Up Latest T3RA Map (1.20x)", [statusText]() { statusText->set_text(bssuite::speedupLatestMap(1.20)); SongCore::API::Loading::RefreshSongs(false); }), 8.5f);
    addRowHeight(BSML::Lite::CreateUIButton(parent, "Generate 10-Min Shaffuru Endurance Drill", [statusText]() { statusText->set_text(bssuite::buildShaffuruMedley(10, 60)); SongCore::API::Loading::RefreshSongs(false); }), 8.5f);
}

extern "C" void setup(CModInfo* info) { *info = modInfo.to_c(); Paper::Logger::RegisterFileContextId(MOD_ID); }
extern "C" void late_load() { il2cpp_functions::Init(); BSML::Init(); BSML::Register::RegisterMainMenu("T3RA//MIZU", "T3RA//MIZU v0.2.0", "All-in-One C++ Beat Saber Suite", didActivate); }
