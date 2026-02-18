#include "ReplaySaver.h"
#include "SavedBattleGame.h"
#include "../Engine/Logger.h"
#include "../Replay/Replay.h"

namespace OpenXcom
{

bool ReplaySaver::saveLastReplay(const SavedBattleGame *save, const std::string &path)
{
	if (!save) return false;
	try
	{
		// Use ReplayRecorder to serialize a proper replay file (initial save + events)
		Replay::ReplayRecorder recorder;
		if (!recorder.startRecording(save))
		{
			Log(LOG_ERROR) << "ReplaySaver: failed to start recorder";
			return false;
		}
		// no events recorded yet by this simple saver; export to disk
		if (!recorder.exportToFile(path))
		{
			Log(LOG_ERROR) << "ReplaySaver: could not export replay to " << path;
			return false;
		}
		return true;
	}
	catch (...) {
		Log(LOG_ERROR) << "ReplaySaver: exception while saving replay to " << path;
		return false;
	}
}

}
