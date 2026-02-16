#include "DebriefingState.h"
#include "../Engine/Options.h"
#include "../Engine/Action.h"
#include "../Replay/ReplayLauncher.h"

namespace OpenXcom
{

void DebriefingState::btnReplayClick(Action *action)
{
	std::string path = Options::getMasterUserFolder() + "last_replay.yaml";
	Replay::launchReplay(_game, path, _palette);
	if (action) action->getDetails()->type = SDL_NOEVENT;
}

} // namespace OpenXcom
