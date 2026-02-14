#pragma once
#include <string>

namespace OpenXcom
{
class SavedBattleGame;

class ReplaySaver
{
public:
    // Saves a minimal replay YAML to the given path. Non-fatal on failure.
    static bool saveLastReplay(const SavedBattleGame *save, const std::string &path);
};
}
