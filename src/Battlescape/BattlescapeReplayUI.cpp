#include "BattlescapeState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Interface/BattlescapeButton.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"

namespace OpenXcom
{

// Minimal, non-invasive replay UI handler implemented in a separate file
// to avoid making large edits to BattlescapeState.cpp. This toggles an
// internal recording flag and updates the button visual/tooltip. Backend
// replay integration will be added later.

void BattlescapeState::btnReplayClick(Action *action)
{
	if (!allowButtons())
	{
		action->getDetails()->type = SDL_NOEVENT;
		return;
	}

	// Lazily create the button if it wasn't created in the constructor
	if (!_btnReplay)
	{
		_btnReplay = new BattlescapeButton(32, 24, 2, 135);
		add(_btnReplay);
		// optional icon if mod provides it
		if (_game->getMod() && _game->getMod()->getSurface("ReplayIcon", false))
		{
			_game->getMod()->getSurface("ReplayIcon")->blitNShade(_btnReplay, 0, 0);
		}
		_btnReplay->setTooltip("STR_TOGGLE_REPLAY_RECORDING");
		_btnReplay->onMouseIn((ActionHandler)&BattlescapeState::txtTooltipIn);
		_btnReplay->onMouseOut((ActionHandler)&BattlescapeState::txtTooltipOut);
		_btnReplay->onMouseClick((ActionHandler)&BattlescapeState::btnReplayClick);
	}

	// Toggle recording state
	_isRecording = !_isRecording;
	if (_btnReplay)
	{
		_btnReplay->toggle(_isRecording);
	}

	// Brief feedback in tooltip area
	if (_isRecording)
	{
		if (_txtTooltip) _txtTooltip->setText(tr("STR_REPLAY_RECORDING_ON"));
	}
	else
	{
		if (_txtTooltip) _txtTooltip->setText(tr("STR_REPLAY_RECORDING_OFF"));
	}

	action->getDetails()->type = SDL_NOEVENT; // consume the event
}

} // namespace OpenXcom
