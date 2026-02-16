#include "ReplayLauncher.h"
#include "Replay.h"
#include "../Battlescape/BattlescapeState.h"
#include "../Battlescape/NextTurnState.h"
#include "../Engine/CrossPlatform.h"
#include "../Engine/Options.h"
#include "../Engine/Game.h"
#include "../Engine/Screen.h"
#include "../Engine/Logger.h"
#include "../Engine/RNG.h"
#include "../Menu/ErrorMessageState.h"
#include "../Mod/Armor.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleSoldier.h"
#include "../Savegame/Base.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Soldier.h"

namespace OpenXcom
{
namespace Replay
{

bool launchReplay(Game *game, const std::string &filePath, SDL_Color *palette)
{
	try
	{
		if (!CrossPlatform::fileExists(filePath))
		{
			auto *itf = game->getMod()->getInterface("errorMessages");
			Uint8 color = itf->getElement("geoscapeColor")->color;
			int bgPal = itf->getElement("geoscapePalette")->color;
			game->pushState(new ErrorMessageState("No replay file found.", palette, color, "BACK01.SCR", bgPal));
			return false;
		}

		// 1. Load and parse the replay file
		auto replayPlayer = std::make_unique<ReplayPlayer>();
		if (!replayPlayer->loadFromFile(filePath))
		{
			auto *itf = game->getMod()->getInterface("errorMessages");
			game->pushState(new ErrorMessageState("Failed to load replay file.", palette,
				itf->getElement("geoscapeColor")->color, "BACK01.SCR",
				itf->getElement("geoscapePalette")->color));
			return false;
		}

		// 2. Parse the initialSave YAML to reconstruct the battle state
		const std::string &initialYaml = replayPlayer->getInitialSaveYaml();
		YAML::YamlRootNodeReader rootReader(YAML::YamlString(initialYaml), "replay_initialSave");
		auto rootNode = rootReader.toBase();

		auto initNode = rootNode["initialSave"];
		Mod *mod = game->getMod();
		Language *lang = game->getLanguage();

		// Create a SavedGame if we don't have one (e.g. launching from main menu)
		bool createdSavedGame = false;
		if (!game->getSavedGame())
		{
			game->setSavedGame(new SavedGame());
			createdSavedGame = true;
		}

		// If we just created a fresh SavedGame, we need to populate it with stub
		// Soldier objects so that SavedBattleGame::load() can look them up by ID.
		if (createdSavedGame)
		{
			const auto &soldierTypes = mod->getSoldiersList();
			RuleSoldier *defaultRules = !soldierTypes.empty() ? mod->getSoldier(soldierTypes.front()) : nullptr;

			if (defaultRules)
			{
				Base *stubBase = new Base(mod);
				for (const auto &unitReader : initNode["units"].children())
				{
					int id = unitReader["id"].readVal<int>();
					if (id < BattleUnit::MAX_SOLDIER_ID)
					{
						std::string armorName = unitReader["genUnitArmor"].readVal(std::string());
						Armor *armor = !armorName.empty() ? mod->getArmor(armorName) : nullptr;
						if (!armor)
							armor = defaultRules->getDefaultArmor();
						Soldier *soldier = new Soldier(defaultRules, armor, 0, id);
						stubBase->getSoldiers()->push_back(soldier);
					}
				}
				game->getSavedGame()->getBases()->push_back(stubBase);
			}
		}

		SavedBattleGame *battleSave = new SavedBattleGame(mod, lang);
		battleSave->load(initNode, mod, game->getSavedGame());
		battleSave->loadMapResources(mod);

		// 3. Restore the RNG seed so combat outcomes are deterministic
		if (replayPlayer->getRngSeed() != 0)
		{
			RNG::setSeed(replayPlayer->getRngSeed());
		}

		// 4. Install the battle save and replay player
		replayPlayer->setFastForward(true);
		game->getSavedGame()->setBattleGame(battleSave);
		battleSave->setReplayPlayer(std::move(replayPlayer));

		// Reveal the full map so the viewer can see all units (including aliens)
		battleSave->setDebugMode();

		// 5. Switch to battlescape resolution and push BattlescapeState
		Options::baseXResolution = Options::baseXBattlescape;
		Options::baseYResolution = Options::baseYBattlescape;
		game->getScreen()->resetDisplay(false);

		BattlescapeState *bs = new BattlescapeState();
		game->pushState(bs);
		battleSave->setBattleState(bs);
		game->pushState(new NextTurnState(battleSave, bs));

		// Skip inventory phase — start the battle immediately
		battleSave->startFirstTurn();
		return true;
	}
	catch (std::exception &e)
	{
		Log(LOG_ERROR) << "Replay launch failed: " << e.what();
		auto *itf = game->getMod()->getInterface("errorMessages");
		Uint8 color = itf->getElement("geoscapeColor")->color;
		int bgPal = itf->getElement("geoscapePalette")->color;
		game->pushState(new ErrorMessageState(std::string("Replay error: ") + e.what(), palette, color, "BACK01.SCR", bgPal));
		return false;
	}
}

} // namespace Replay
} // namespace OpenXcom
