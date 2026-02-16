#include "ListReplayState.h"
#include "DeleteGameState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"
#include "../Replay/ReplayLauncher.h"
#include "../Savegame/SavedGame.h"
#include "ErrorMessageState.h"
#include <algorithm>
#include <ctime>

namespace OpenXcom
{

ListReplayState::ListReplayState()
{
	_screen = false;

	// Create objects
	_window = new Window(this, 320, 200, 0, 0, POPUP_BOTH);
	_btnCancel = new TextButton(80, 16, 120, 172);
	_txtTitle = new Text(310, 17, 5, 7);
	_txtDelete = new Text(310, 9, 5, 23);
	_btnDelete = new ToggleTextButton(288, 16, 16, 23);
	_lstReplays = new TextList(288, 112, 8, 42);

	// Set palette
	setInterface("geoscape", true, _game->getSavedGame() ? _game->getSavedGame()->getSavedBattle() : 0);

	add(_window, "window", "saveMenus");
	add(_btnCancel, "button", "saveMenus");
	add(_txtTitle, "text", "saveMenus");
	add(_txtDelete, "text", "saveMenus");
	add(_btnDelete, "button", "saveMenus");
	add(_lstReplays, "list", "saveMenus");

	// Set up objects
	setWindowBackground(_window, "saveMenus");

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_SELECT_REPLAY"));

	_btnDelete->setVisible(false);
	_txtDelete->setAlign(ALIGN_CENTER);
	_txtDelete->setText(tr("STR_RIGHT_CLICK_TO_DELETE"));

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&ListReplayState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&ListReplayState::btnCancelClick, Options::keyCancel);

	_lstReplays->setColumns(2, 188, 100);
	_lstReplays->setSelectable(true);
	_lstReplays->setBackground(_window);
	_lstReplays->setMargin(8);
	_lstReplays->onMousePress((ActionHandler)&ListReplayState::lstReplaysPress);

	centerAllSurfaces();
}

ListReplayState::~ListReplayState()
{
}

void ListReplayState::init()
{
	State::init();
	populateList();
}

void ListReplayState::populateList()
{
	_lstReplays->clearList();
	_replays.clear();

	std::string replayDir = Options::getMasterUserFolder() + "replays";
	if (!CrossPlatform::folderExists(replayDir))
		return;

	// Also include last_replay.yaml from the user folder root
	std::string lastReplay = Options::getMasterUserFolder() + "last_replay.yaml";
	if (CrossPlatform::fileExists(lastReplay))
	{
		ReplayInfo info;
		info.filename = "last_replay.yaml";
		info.fullPath = lastReplay;
		info.timestamp = CrossPlatform::getDateModified(lastReplay);
		_replays.push_back(info);
	}

	auto contents = CrossPlatform::getFolderContents(replayDir, "yaml");
	for (const auto &entry : contents)
	{
		const std::string &name = std::get<0>(entry);
		bool isFolder = std::get<1>(entry);
		time_t mtime = std::get<2>(entry);
		if (isFolder) continue;

		ReplayInfo info;
		info.filename = name;
		info.fullPath = replayDir + "/" + name;
		info.timestamp = mtime;
		_replays.push_back(info);
	}

	// Sort newest first
	std::sort(_replays.begin(), _replays.end(),
		[](const ReplayInfo &a, const ReplayInfo &b) { return a.timestamp > b.timestamp; });

	for (const auto &r : _replays)
	{
		char dateBuf[64];
		struct tm *t = localtime(&r.timestamp);
		snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d %02d:%02d",
			t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
			t->tm_hour, t->tm_min);
		_lstReplays->addRow(2, r.filename.c_str(), dateBuf);
	}
}

void ListReplayState::lstReplaysPress(Action *action)
{
	if (_replays.empty()) return;
	size_t row = _lstReplays->getSelectedRow();
	if (row >= _replays.size()) return;

	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		// Launch the replay
		Replay::launchReplay(_game, _replays[row].fullPath, _palette);
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		// Delete replay with confirmation — reuse the same pattern as save deletion
		const std::string &path = _replays[row].fullPath;
		if (CrossPlatform::deleteFile(path))
		{
			populateList();
		}
		else
		{
			auto *itf = _game->getMod()->getInterface("errorMessages");
			_game->pushState(new ErrorMessageState("Failed to delete replay.", _palette,
				itf->getElement("geoscapeColor")->color, "BACK01.SCR",
				itf->getElement("geoscapePalette")->color));
		}
	}
}

void ListReplayState::btnCancelClick(Action *)
{
	_game->popState();
}

}
