#pragma once
#include <string>
#include <vector>

namespace t3ra {
    struct Onset {
        double beat;
        double energy;
        double bass;
    };

    struct MapOptions {
        std::string audioPath;
        std::string songName;
        std::string artistName;
        int preset = 1; // 1=Balanced, 2=Tech, 3=Camellia Speed
        std::string customLevelsDir = "/sdcard/ModData/com.beatgames.beatsaber/Mods/SongCore/CustomLevels";
    };

    struct BuildResult {
        bool ok = false;
        double bpm = 0.0;
        double phaseMs = 0.0;
        int expertPlusNotes = 0;
        std::string outputDir;
        std::string message;
    };

    BuildResult generateMap(const MapOptions& opts);
}
