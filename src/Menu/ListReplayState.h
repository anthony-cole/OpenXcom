#pragma once
#include "../Engine/State.h"
#include <vector>
#include <string>
#include <ctime>

namespace OpenXcom
{

class TextButton;
class ToggleTextButton;
class Window;
class Text;
class TextList;

class ListReplayState : public State
{
private:
	struct ReplayInfo
	{
		std::string filename; // just the filename
		std::string fullPath; // full path to file
		time_t timestamp;
	};

	TextButton *_btnCancel;
	ToggleTextButton *_btnDelete;
	Window *_window;
	Text *_txtTitle, *_txtDelete;
	TextList *_lstReplays;
	std::vector<ReplayInfo> _replays;

	void populateList();
public:
	ListReplayState();
	~ListReplayState();
	void init() override;
	void lstReplaysPress(Action *action);
	void btnCancelClick(Action *action);
};

}
