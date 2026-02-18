#include "Replay.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Battlescape/Pathfinding.h"
#include "../Engine/Yaml.h"
#include "../Engine/Logger.h"
#include "../Engine/RNG.h"
#include <fstream>
#include <sstream>
#include <cstdio>

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
    _rngSeed = RNG::getSeed();
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

void ReplayRecorder::updateLastDamageSeed(uint64_t seed)
{
    // Walk backwards to find the most recent STATE_ACTION event and set its explosionSeed
    // (used as a unified damage seed). Only sets if currently empty (0) so that chain
    // explosions don't overwrite the initial explosion's seed.
    for (auto it = _events.rbegin(); it != _events.rend(); ++it)
    {
        if (it->type == "STATE_ACTION")
        {
            if (it->explosionSeed == 0)
                it->explosionSeed = seed;
            return;
        }
    }
}

ParsedStateAction parseStateActionPayload(const std::string &payload)
{
    ParsedStateAction p;
    std::istringstream ss(payload);
    std::string token;
    while (ss >> token)
    {
        if (token == "action:")
        {
            std::string actionStr;
            ss >> actionStr;
            p.actionType = battleActionFromString(actionStr);
        }
        else if (token == "target:")
        {
            char comma;
            ss >> p.target.x >> comma >> p.target.y >> comma >> p.target.z;
        }
        else if (token == "weapon:")
            ss >> p.weaponType;
        else if (token == "value:")
            ss >> p.value;
        else if (token == "reactionFire:")
        {
            std::string val;
            ss >> val;
            p.reactionFire = (val == "true");
        }
        else if (token == "path:")
        {
            std::string pathStr;
            ss >> pathStr;
            std::istringstream ps(pathStr);
            int dir;
            while (ps >> dir)
            {
                p.path.push_back(dir);
                char c;
                if (!(ps >> c)) break;
            }
        }
    }
    return p;
}

void trimWalkPathFromReplay(std::vector<int> &path, const ReplayPlayer *player,
                            int actorId, const Position &actorPos)
{
    if (!player || path.empty()) return;

    const auto &allEvents = player->getEvents();
    size_t curIdx = player->getCurrentIndex();

    // Scan ahead to find the LAST WALK_END for this actor before its next STATE_ACTION.
    Position walkEndPos;
    bool foundWalkEnd = false;
    for (size_t i = curIdx + 1; i < allEvents.size(); ++i)
    {
        if (allEvents[i].type == "STATE_ACTION" && allEvents[i].actorId == actorId)
            break;
        if (allEvents[i].type == "WALK_END" && allEvents[i].actorId == actorId)
        {
            auto endPosStr = allEvents[i].payload.find("endPos: ");
            if (endPosStr != std::string::npos)
            {
                int ex, ey, ez;
                if (sscanf(allEvents[i].payload.c_str() + endPosStr + 8, "%d,%d,%d", &ex, &ey, &ez) == 3)
                {
                    walkEndPos = Position(ex, ey, ez);
                    foundWalkEnd = true;
                }
            }
        }
    }

    if (!foundWalkEnd) return;

    // Simulate the recorded path (LIFO: last element = first step) and trim
    // to the step that reaches walkEndPos.
    Position pos = actorPos;
    size_t stepsNeeded = 0;
    bool found = false;
    for (int pi = path.size() - 1; pi >= 0; --pi)
    {
        Position delta;
        Pathfinding::directionToVector(path[pi], &delta);
        pos += delta;
        stepsNeeded++;
        if (pos.x == walkEndPos.x && pos.y == walkEndPos.y)
        {
            found = true;
            break;
        }
    }
    if (found && stepsNeeded < path.size())
    {
        path.erase(path.begin(), path.begin() + (path.size() - stepsNeeded));
    }
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
        f << "rngSeed: " << _rngSeed << "\n";
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
            f << "    rngSeed: " << e.rngSeed << "\n";
            if (e.explosionSeed != 0)
                f << "    explosionSeed: " << e.explosionSeed << "\n";
            if (e.hitSeed != 0)
                f << "    hitSeed: " << e.hitSeed << "\n";
            if (!e.payload.empty())
            {
                std::string escaped = e.payload;
                for (std::string::size_type pos = 0; (pos = escaped.find('\\', pos)) != std::string::npos; pos += 2)
                {
                    escaped.replace(pos, 1, "\\\\");
                }
                for (std::string::size_type pos = 0; (pos = escaped.find('"', pos)) != std::string::npos; pos += 2)
                {
                    escaped.replace(pos, 1, "\\\"");
                }
                for (std::string::size_type pos = 0; (pos = escaped.find('\n', pos)) != std::string::npos; pos += 2)
                {
                    escaped.replace(pos, 1, "\\n");
                }
                f << "    payload: \"" << escaped << "\"\n";
            }
        }
        f << "...\n";
        f.flush();
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

/// Helper: unescape \\n, \\\\, \\" in a payload string
static std::string unescapePayload(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            char next = s[i + 1];
            if (next == 'n') { out += '\n'; ++i; }
            else if (next == '\\') { out += '\\'; ++i; }
            else if (next == '"') { out += '"'; ++i; }
            else { out += s[i]; }
        }
        else
        {
            out += s[i];
        }
    }
    return out;
}

/// Helper: trim leading/trailing whitespace
static std::string trim(const std::string &s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool ReplayPlayer::loadFromFile(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        Log(LOG_ERROR) << "ReplayPlayer: could not open " << path;
        return false;
    }

    _events.clear();
    _initialSaveYaml.clear();
    _eventIndex = 0;
    _paused = false;
    _speed = 1;

    // Parse states
    enum { S_HEADER, S_INITIAL_SAVE, S_EVENTS, S_EVENT_FIELDS } state = S_HEADER;
    ReplayEvent currentEvent;
    bool haveEvent = false;
    std::string line;

    while (std::getline(f, line))
    {
        // strip trailing \r
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        switch (state)
        {
        case S_HEADER:
            if (line.find("rngSeed:") == 0)
            {
                _rngSeed = std::stoull(trim(line.substr(8)));
            }
            else if (line.find("initialSave:") == 0)
            {
                state = S_INITIAL_SAVE;
            }
            // skip "---", "version: N", etc.
            break;

        case S_INITIAL_SAVE:
            if (line.find("events:") == 0)
            {
                state = S_EVENTS;
            }
            else if (line.size() >= 2 && line[0] == ' ' && line[1] == ' ')
            {
                // Block scalar content: remove the 2-space indent
                _initialSaveYaml += line.substr(2) + "\n";
            }
            break;

        case S_EVENTS:
        case S_EVENT_FIELDS:
            if (line.find("...") == 0)
            {
                // End of document
                if (haveEvent) { _events.push_back(currentEvent); haveEvent = false; }
                break;
            }
            if (line.find("  - tick:") != std::string::npos)
            {
                // New event entry
                if (haveEvent) { _events.push_back(currentEvent); }
                currentEvent = ReplayEvent();
                haveEvent = true;
                state = S_EVENT_FIELDS;
                // parse tick value
                size_t colon = line.find("tick:");
                if (colon != std::string::npos)
                    currentEvent.tick = std::stoull(trim(line.substr(colon + 5)));
            }
            else if (state == S_EVENT_FIELDS && line.find("    type:") != std::string::npos)
            {
                size_t colon = line.find("type:");
                if (colon != std::string::npos)
                    currentEvent.type = trim(line.substr(colon + 5));
            }
            else if (state == S_EVENT_FIELDS && line.find("    actorId:") != std::string::npos)
            {
                size_t colon = line.find("actorId:");
                if (colon != std::string::npos)
                    currentEvent.actorId = std::stoi(trim(line.substr(colon + 8)));
            }
            else if (state == S_EVENT_FIELDS && line.find("    rngSeed:") != std::string::npos)
            {
                size_t colon = line.find("rngSeed:");
                if (colon != std::string::npos)
                    currentEvent.rngSeed = std::stoull(trim(line.substr(colon + 8)));
            }
            else if (state == S_EVENT_FIELDS && line.find("    explosionSeed:") != std::string::npos)
            {
                size_t colon = line.find("explosionSeed:");
                if (colon != std::string::npos)
                    currentEvent.explosionSeed = std::stoull(trim(line.substr(colon + 14)));
            }
            else if (state == S_EVENT_FIELDS && line.find("    hitSeed:") != std::string::npos)
            {
                size_t colon = line.find("hitSeed:");
                if (colon != std::string::npos)
                    currentEvent.hitSeed = std::stoull(trim(line.substr(colon + 8)));
            }
            else if (state == S_EVENT_FIELDS && line.find("    payload:") != std::string::npos)
            {
                size_t colon = line.find("payload:");
                if (colon != std::string::npos)
                {
                    std::string val = trim(line.substr(colon + 8));
                    // Remove surrounding quotes if present
                    if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                        val = val.substr(1, val.size() - 2);
                    currentEvent.payload = unescapePayload(val);
                }
            }
            break;
        }
    }

    if (haveEvent) { _events.push_back(currentEvent); }

    Log(LOG_INFO) << "ReplayPlayer: loaded " << _events.size() << " events from " << path;
    return !_events.empty();
}

void ReplayPlayer::play() { _paused = false; }
void ReplayPlayer::pause() { _paused = true; }

const ReplayEvent *ReplayPlayer::getNextEventIfReady(uint64_t currentTick) const
{
    if (_eventIndex >= _events.size()) return nullptr;
    // In fast-forward mode, dispatch events immediately regardless of tick timing
    if (_fastForward || currentTick >= _events[_eventIndex].tick)
        return &_events[_eventIndex];
    return nullptr;
}

void ReplayPlayer::advanceEvent()
{
    if (_eventIndex < _events.size())
        ++_eventIndex;
}

} // namespace Replay
} // namespace OpenXcom
