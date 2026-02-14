#include "Replay.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Engine/Yaml.h"
#include "../Engine/Logger.h"
#include <fstream>
#include <sstream>

namespace OpenXcom
{
namespace Replay
{
ReplayRecorder::ReplayRecorder() {}
ReplayRecorder::~ReplayRecorder() {}

bool ReplayRecorder::startRecording(const SavedBattleGame *snapshot)
{
    if (!snapshot) return false;
    _recording = true;
    _events.clear();
    try
    {
        YAML::YamlRootNodeWriter w;
        auto node = w.toBase();
        node.setAsMap();
        // write the snapshot under "initialSave"
        auto init = node["initialSave"];
        snapshot->save(init);
        auto s = w.toBase().emit();
        _initialSaveYaml = s.yaml;
        return true;
    }
    catch (...) {
        Log(LOG_ERROR) << "ReplayRecorder: failed to serialize initial save";
        _initialSaveYaml.clear();
        _recording = false;
        return false;
    }
}

void ReplayRecorder::stopRecording()
{
    _recording = false;
}

void ReplayRecorder::recordEvent(const ReplayEvent &ev)
{
    if (!_recording) return;
    _events.push_back(ev);
}

bool ReplayRecorder::exportToFile(const std::string &path) const
{
    try
    {
        std::ofstream f(path, std::ios::out | std::ios::trunc);
        if (!f.is_open())
        {
            Log(LOG_ERROR) << "ReplayRecorder: could not open file " << path;
            return false;
        }
        // Simple YAML: write header and embed initialSave and events
        f << "---\n";
        f << "version: 1\n";
        f << "initialSave: |\n";
        // indent initial YAML by two spaces
        std::istringstream ss(_initialSaveYaml);
        std::string line;
        while (std::getline(ss, line))
        {
            f << "  " << line << "\n";
        }
        f << "events:\n";
        for (const auto &e : _events)
        {
            f << "  - tick: " << e.tick << "\n";
            f << "    type: " << e.type << "\n";
            f << "    actorId: " << e.actorId << "\n";
            if (!e.payload.empty())
            {
                f << "    payload: |\n";
                std::istringstream ps(e.payload);
                while (std::getline(ps, line))
                {
                    f << "      " << line << "\n";
                }
            }
        }
        f << "...\n";
        f.close();
        return true;
    }
    catch (...) {
        Log(LOG_ERROR) << "ReplayRecorder: exception while exporting replay to " << path;
        return false;
    }
}

ReplayPlayer::ReplayPlayer() {}
ReplayPlayer::~ReplayPlayer() {}

bool ReplayPlayer::loadFromFile(const std::string &path)
{
    // TODO: implement
    return false;
}

void ReplayPlayer::play() {}
void ReplayPlayer::pause() {}
void ReplayPlayer::stepForward() {}
void ReplayPlayer::stepBackward() {}

} // namespace Replay
} // namespace OpenXcom
