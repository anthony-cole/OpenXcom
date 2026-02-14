#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace OpenXcom
{
class SavedBattleGame;
namespace YAML { class YamlNodeWriter; class YamlString; }

namespace Replay
{
struct ReplayEvent
{
    uint64_t tick = 0;
    std::string type;
    int actorId = -1;
    std::string payload; // YAML fragment or simple string
};

class ReplayRecorder
{
public:
    ReplayRecorder();
    ~ReplayRecorder();

    // Begin recording, capture an initial SavedBattleGame snapshot immediately.
    bool startRecording(const SavedBattleGame *snapshot);
    // Stop recording.
    void stopRecording();
    // Append an event to the in-memory log.
    void recordEvent(const ReplayEvent &ev);
    // Export the recorded replay to a YAML file. Returns true on success.
    bool exportToFile(const std::string &path) const;

    // Simple accessors for tests / UI
    const std::vector<ReplayEvent>& getEvents() const { return _events; }
    const std::string& getInitialSaveYaml() const { return _initialSaveYaml; }

private:
    bool _recording = false;
    std::vector<ReplayEvent> _events;
    std::string _initialSaveYaml; // ASCII YAML snapshot string
};

class ReplayPlayer
{
public:
    ReplayPlayer();
    ~ReplayPlayer();

    bool loadFromFile(const std::string &path);
    // Playback control stubs
    void play();
    void pause();
    void stepForward();
    void stepBackward();

private:
    // parsed data
};

} // namespace Replay
} // namespace OpenXcom
