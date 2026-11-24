#pragma once
#include <string>

namespace bssuite {
    std::string runPatternTrainer(int mode, double bpm, int seconds);
    std::string randomizeHSV();
    std::string buildAllPlaylists();
    std::string speedupLatestMap(double mult);
    std::string buildShaffuruMedley(int sliceCount, int sliceSec);
}
