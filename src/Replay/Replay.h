#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cinttypes>

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
    uint64_t rngSeed = 0; // RNG state at time of recording
    uint64_t explosionSeed = 0; // RNG state right before TileEngine::explode()
    uint64_t hitSeed = 0; // RNG state right before TileEngine::hit()
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
    bool isRecording() const { return _recording; }
    // Update the rngSeed on the most recently recorded STATE_ACTION event.
    // Used to capture the RNG state at ProjectileFlyBState::init() time
    // (more precise than the seed captured at event recording time).
    void updateLastProjectileRngSeed(uint64_t seed);
    void updateLastExplosionSeed(uint64_t seed);
    void updateLastHitSeed(uint64_t seed);
    /// Unified damage seed: sets explosionSeed on most recent STATE_ACTION only if empty (0).
    void updateLastDamageSeed(uint64_t seed);

private:
    bool _recording = false;
    std::vector<ReplayEvent> _events;
    std::string _initialSaveYaml; // ASCII YAML snapshot string
    uint64_t _rngSeed = 0;
};

class ReplayPlayer
{
public:
    ReplayPlayer();
    ~ReplayPlayer();

    bool loadFromFile(const std::string &path);

    // Playback control
    void play();
    void pause();
    bool isPaused() const { return _paused; }
    bool isFinished() const { return _eventIndex >= _events.size(); }

    void setSpeed(int speed) { _speed = speed; }
    int getSpeed() const { return _speed; }

    void setFastForward(bool ff) { _fastForward = ff; }
    bool isFastForward() const { return _fastForward; }

    const std::string &getInitialSaveYaml() const { return _initialSaveYaml; }
    const std::vector<ReplayEvent> &getEvents() const { return _events; }
    uint64_t getRngSeed() const { return _rngSeed; }

    /// Returns pointer to next event if currentTick >= event.tick, else nullptr
    const ReplayEvent *getNextEventIfReady(uint64_t currentTick) const;
    /// Advance to next event
    void advanceEvent();
    /// Get the current event index (for peeking ahead)
    size_t getCurrentIndex() const { return _eventIndex; }

private:
    std::vector<ReplayEvent> _events;
    std::string _initialSaveYaml;
    uint64_t _rngSeed = 0;
    size_t _eventIndex = 0;
    bool _paused = false;
    bool _fastForward = false;
    int _speed = 1;
};

} // namespace Replay
} // namespace OpenXcom
