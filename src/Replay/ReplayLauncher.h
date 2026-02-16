#pragma once
#include <string>
#include <SDL.h>

namespace OpenXcom
{

class Game;

namespace Replay
{

/// Launch a replay from a YAML file. Returns true on success.
/// On failure, pushes an ErrorMessageState and returns false.
bool launchReplay(Game *game, const std::string &filePath, SDL_Color *palette);

} // namespace Replay
} // namespace OpenXcom
