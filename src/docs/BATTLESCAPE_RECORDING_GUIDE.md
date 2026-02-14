# Battlescape Recording Hooks Guide

## Overview

The `BattlescapeGame` class now provides a centralized recording system for battle events. This allows all battle actions to be recorded for replay functionality without needing to scatter recording calls throughout the codebase.

## Architecture

### Recording Initialization

The `BattlescapeGame` constructor automatically initializes the replay recorder:

```cpp
BattlescapeGame::BattlescapeGame(SavedBattleGame *save, BattlescapeState *parentState)
{
    // ...
    _recorder = std::make_unique<Replay::ReplayRecorder>();
    _replayTick = 0;
    // ...
}
```

### Core Recording Interface

Three public methods are available:

#### 1. `startReplayRecording()`

Starts the recording process by taking a snapshot of the current battle state:

```cpp
void BattlescapeGame::startReplayRecording()
```

**Usage:**
```cpp
_game->getGame()->getBattleGame()->startReplayRecording();
```

#### 2. `stopReplayRecording()`

Stops the recording process:

```cpp
void BattlescapeGame::stopReplayRecording()
```

**Usage:**
```cpp
_game->getGame()->getBattleGame()->stopReplayRecording();
```

#### 3. `recordBattleEvent()`

Records a specific battle event. This is the primary hook for recording game events:

```cpp
void BattlescapeGame::recordBattleEvent(
    const std::string &eventType,
    BattleUnit *actor = nullptr,
    const std::string &payload = ""
)
```

**Parameters:**
- `eventType`: Type identifier for the event (e.g., "UNIT_MOVED", "WEAPON_FIRED", "GRENADE_THROWN")
- `actor`: The unit performing the action (optional, can be nullptr)
- `payload`: Optional YAML fragment or string data containing event details

**Usage Examples:**

```cpp
// Record a unit movement
_game->recordBattleEvent("UNIT_MOVED", unit, "target: 10, 20, 5");

// Record a shot fired
_game->recordBattleEvent("WEAPON_FIRED", shooter, "weapon: rifle\ntarget: 15, 25, 3");

// Record a grenade throw
_game->recordBattleEvent("GRENADE_THROWN", thrower, "type: frag\ntarget: 12, 18, 4");

// Record an event with no actor
_game->recordBattleEvent("MAP_EXPLOSION", nullptr, "position: 20, 30, 2\nradius: 5");
```

## Integration Points

### Common Places to Add Recording Hooks

#### 1. Unit Movement - `UnitWalkBState`

```cpp
// After successful movement
if (success) {
    auto *battleGame = _game->getBattleGame();
    std::string payload = "target: " + std::to_string(targetPos.x) + ", " 
                        + std::to_string(targetPos.y) + ", " 
                        + std::to_string(targetPos.z);
    battleGame->recordBattleEvent("UNIT_MOVED", unit, payload);
}
```

#### 2. Weapon Fire - `ProjectileFlyBState`

```cpp
// When projectile is fired
auto *battleGame = _game->getBattleGame();
std::string payload = "weapon: " + action.weapon->getRules()->getType() + 
                      "\ntarget: " + std::to_string(action.target.x) + ", " +
                      std::to_string(action.target.y) + ", " +
                      std::to_string(action.target.z);
battleGame->recordBattleEvent("WEAPON_FIRED", action.actor, payload);
```

#### 3. Melee Attack - `MeleeAttackBState`

```cpp
// When melee attack is executed
auto *battleGame = _game->getBattleGame();
battleGame->recordBattleEvent("MELEE_ATTACK", attacker, "target_id: " + std::to_string(target->getId()));
```

#### 4. Grenade Throw - `ProjectileFlyBState` or dedicated handler

```cpp
// When grenade is thrown
auto *battleGame = _game->getBattleGame();
std::string payload = "type: grenade\ntarget: " + std::to_string(targetPos.x) + ", " +
                      std::to_string(targetPos.y) + ", " + std::to_string(targetPos.z);
battleGame->recordBattleEvent("GRENADE_THROWN", thrower, payload);
```

#### 5. Psionic Attack - `PsiAttackBState`

```cpp
// When psi attack is executed
auto *battleGame = _game->getBattleGame();
std::string payload = "type: " + std::string(action.type == BA_MINDCONTROL ? "mind_control" : "panic") +
                      "\ntarget_id: " + std::to_string(target->getId());
battleGame->recordBattleEvent("PSI_ATTACK", attacker, payload);
```

#### 6. Unit Status Changes - `UnitDieBState` or `UnitPanicBState`

```cpp
// When unit dies
auto *battleGame = _game->getBattleGame();
battleGame->recordBattleEvent("UNIT_DIED", unit, "damage_type: " + damageTypeName);

// When unit panics
battleGame->recordBattleEvent("UNIT_PANICKED", unit, "panic_type: flee");
```

## Recording Tick System

Each recorded event is automatically tagged with a tick number obtained from `getNextReplayTick()`:

```cpp
uint64_t tick = _game->recordBattleEvent(...); // internally calls getNextReplayTick()
```

This ensures events are properly ordered and can be replayed sequentially.

## Event Payloads

Event payloads should be simple YAML or text format. Examples:

```yaml
# Movement event
target: 10, 20, 5
duration: 100

# Combat event
weapon: rifle
target: 15, 25, 3
accuracy: 75
damage: 20

# Status change
new_status: unconscious
damage_type: explosive
```

## Data Access

To retrieve recorded events and exported replay:

```cpp
auto *recorder = battleGame->getRecorder();

// Get all recorded events
const auto& events = recorder->getEvents();

// Get initial save YAML
const auto& initialSave = recorder->getInitialSaveYaml();

// Export to file
recorder->exportToFile("path/to/replay.yaml");
```

## Best Practices

1. **Centralized Recording**: Always use `recordBattleEvent()` through the `BattlescapeGame` instance. Do not create separate recorder instances.

2. **Event Type Naming**: Use consistent, descriptive event type names. Suggested naming convention:
   - `UNIT_*` for unit-related events
   - `WEAPON_*` for weapon actions
   - `MAP_*` for map/environment events
   - `GAME_*` for game-level events

3. **Minimal Payloads**: Keep payloads small but informative. Only record essential data - complex calculations should happen during replay, not recording.

4. **No Circular Dependencies**: Recording hooks should never call back into battle state logic. They only observe and record.

5. **Error Handling**: Recording failures should not affect battle flow. The `recordBattleEvent()` method gracefully handles null recorders.

## Testing Recording

To verify recording is working:

```cpp
// In DebriefingState or at end of battle
auto *battleGame = _game->getBattleGame();
if (battleGame && battleGame->getRecorder()) {
    Log(LOG_DEBUG) << "Recorded " << battleGame->getRecorder()->getEvents().size() << " events";
    
    // Export for inspection
    battleGame->getRecorder()->exportToFile("debug_replay.yaml");
}
```

## Future Extensions

The recording system can be extended to:

1. **Streaming Records**: Support saving incremental events instead of holding all in memory
2. **Filtering**: Only record specific event types based on user preferences
3. **Compression**: Compress recorded events to reduce file size
4. **Network Streaming**: Send events over network for remote replays
5. **Event Analysis**: Build analytics on recorded events (e.g., soldier performance, map heatmaps)

## Troubleshooting

### No events recorded
- Ensure `startReplayRecording()` was called before recording events
- Check that `recordBattleEvent()` is being called with non-empty event types

### Memory issues with large battles
- Consider implementing event streaming instead of in-memory storage
- Periodically export and clear the event buffer

### Events not in order
- Verify that `getNextReplayTick()` is being called to get the tick number
- Check that events are recorded in the order they occur in game logic

## References

- `Battlescape/BattlescapeGame.h` - Main interface
- `Replay/Replay.h` - ReplayRecorder implementation
- `Savegame/ReplaySaver.cpp` - Example usage of recorder
