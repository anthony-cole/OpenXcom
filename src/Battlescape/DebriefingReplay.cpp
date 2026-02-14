#include "DebriefingState.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/Options.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Menu/ErrorMessageState.h"
#include "../Menu/LoadGameState.h"
#include "../Mod/RuleInterface.h"

namespace OpenXcom
{

void DebriefingState::btnReplayClick(Action *action)
{
	std::string path = Options::getMasterUserFolder() + "last_replay.yaml";
	try
	{
		if (CrossPlatform::fileExists(path))
		{
			// Launch the loader to open the replay as a battlescape save and start it
			// Use the battlescape origin so LoadGameState will restore battlescape state
			_game->pushState(new LoadGameState(OPT_BATTLESCAPE, "last_replay.yaml", _palette));
		}
		else
		{
			std::string msg = tr("STR_NO_REPLAY_FOUND");
			if (msg.empty()) msg = "No replay was recorded for this mission.";
			// use errorMessages interface colors
			auto *itf = _game->getMod()->getInterface("errorMessages");
			Uint8 color = itf->getElement("geoscapeColor")->color;
			int bgPal = itf->getElement("geoscapePalette")->color;
			_game->pushState(new ErrorMessageState(msg, _palette, color, "BACK01.SCR", bgPal));
		}
	}
	catch (...) { /* ignore errors launching replay */ }
	if (action) action->getDetails()->type = SDL_NOEVENT;
}

} // namespace OpenXcom
