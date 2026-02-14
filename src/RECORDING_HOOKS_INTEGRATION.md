# Integration of Recording Hooks with Push Event System

## Overview

The existing push event system in `BattlescapeGame` can be hooked to automatically record all battle state transitions without requiring individual recording calls throughout the codebase. This document describes how to integrate the recording system with `statePushFront()`, `statePushNext()`, and `statePushBack()`.

## Implementation Steps

### 1. Add Method Declaration to Header

Add this to `Battlescape/BattlescapeGame.h`:

```cpp
private:
    /// Records a battle state action for replay
    void recordStateAction(BattleAction *action, const std::string &stateType);
```

### 2. Implement Recording Method

Add this implementation to `Battlescape/BattlescapeGame.cpp` before the closing namespace brace:

```cpp
/**
 * Records a battle state action for replay.
 * @param action The battle action to record.
 * @param stateType The type of state action (e.g., "STATE_PUSHED_BACK", etc).
 */
void BattlescapeGame::recordStateAction(BattleAction *action, const std::string &stateType)
{
	if (!action || !_recorder || !_recorder->getInitialSaveYaml().size())
	{
		return; // Not recording or recorder not initialized
	}

	std::ostringstream payload;
	payload << "state_type: " << stateType;
	
	if (action->actor)
	{
		payload << "\nactor_id: " << action->actor->getId();
		payload << "\naction_type: " << action->type;
	}
	
	if (action->targeting)
	{
		payload << "\ntarget: " << action->target.x << ", " << action->target.y << ", " << action->target.z;
	}
	
	if (action->weapon)
	{
		payload << "\nweapon: " << action->weapon->getRules()->getType();
	}

	recordBattleEvent("BATTLE_STATE", action->actor, payload.str());
}
```

### 3. Hook Recording into State Push Methods

Modify the three state push methods in `BattlescapeGame.cpp`:

#### statePushFront()

```cpp
void BattlescapeGame::statePushFront(BattleState *bs)
{
	_states.push_front(bs);
	if (bs)
	{
		recordStateAction(&bs->getAction(), "STATE_PUSHED_FRONT");
		bs->init();
	}
}
```

#### statePushNext()

```cpp
void BattlescapeGame::statePushNext(BattleState *bs)
{
	if (_states.empty())
	{
		_states.push_front(bs);
		if (bs)
		{
			recordStateAction(&bs->getAction(), "STATE_PUSHED_NEXT");
			bs->init();
		}
	}
	else
	{
		_states.insert(++_states.begin(), bs);
		if (bs)
		{
			recordStateAction(&bs->getAction(), "STATE_PUSHED_NEXT");
		}
	}
}
```

#### statePushBack()

```cpp
void BattlescapeGame::statePushBack(BattleState *bs)
{
	if (_states.empty())
	{
		_states.push_front(bs);
		// end turn request?
		if (_states.front() == 0)
		{
			_states.pop_front();
			endTurn();
			return;
		}
		else if (bs)
		{
			recordStateAction(&bs->getAction(), "STATE_PUSHED_BACK");
			bs->init();
		}
	}
	else
	{
		_states.push_back(bs);
		if (bs)
		{
			recordStateAction(&bs->getAction(), "STATE_PUSHED_BACK");
		}
	}
}
```

## How It Works

1. **Centralized Recording Point**: Every time a battle state (walk, shoot, melee, etc.) is pushed onto the state queue, it automatically records the action.

2. **Automatic Event Ordering**: The recording hooks automatically capture actions in the exact order they're executed, maintaining proper sequence.

3. **Minimal Overhead**: Recording only happens if:
   - Recording has been started (`_recorder->getInitialSaveYaml().size()` is non-zero)
   - The action has valid data

4. **Complete Action Context**: Each recorded event includes:
   - State type (FRONT, NEXT, or BACK)
   - Actor unit ID
   - Action type (WALK, FIRE, MELEE, etc.)
   - Target position (if targeting)
   - Weapon being used (if applicable)

## Example Recorded Events

When a player performs a move and then fires a weapon, the following events are recorded automatically:

```yaml
- tick: 1
  type: BATTLE_STATE
  actor_id: 0
  payload: |
    state_type: STATE_PUSHED_BACK
    actor_id: 0
    action_type: BA_WALK
    target: 10, 15, 2

- tick: 2
  type: BATTLE_STATE
  actor_id: 0
  payload: |
    state_type: STATE_PUSHED_BACK
    actor_id: 0
    action_type: BA_SNAPSHOT
    target: 20, 25, 2
    weapon: RIFLE
```

## Benefits Over Manual Recording Hooks

1. **No Code Duplication**: Recording happens at one point rather than scattered throughout action handlers
2. **Automatic Synchronization**: All states are captured in the correct order automatically
3. **Complete Coverage**: Even newly-added states automatically get recorded without code changes
4. **Maintainability**: Adding a new action type doesn't require adding recording code
5. **Performance**: Recording only happens when recording is active

## Integration Points

The recording system already has:
- ? `recordBattleEvent()` - Main recording interface
- ? `startReplayRecording()` - Enables recording
- ? `stopReplayRecording()` - Ends recording
- ? `getRecorder()` - Access to replay recorder
- ? `getNextReplayTick()` - Tick counter

With the push hooks integrated, all battle state transitions will be automatically recorded without requiring any additional integration work.

## Testing

To test the integration:

1. Enable replay recording at battle start:
   ```cpp
   battleGame->startReplayRecording();
   ```

2. Play through battle normally

3. Check recorded events:
   ```cpp
   auto& events = battleGame->getRecorder()->getEvents();
   Log(LOG_DEBUG) << "Total events recorded: " << events.size();
   ```

4. Export for inspection:
   ```cpp
   battleGame->getRecorder()->exportToFile("debug_replay.yaml");
   ```
