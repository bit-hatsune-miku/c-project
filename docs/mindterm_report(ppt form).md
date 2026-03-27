# Midterm Report (PPT Form)

This version is slide-content only.

- no timestamps
- no speaking notes
- no flowchart/pseudocode instructions
- use real code examples from the current codebase

## Slide 1. Project Overview

Put on the slide:

- project title
- one-line description: visual novel game with integrated turn-based battles
- current prototype scope:
  - main menu
  - story / dialogue progression
  - save / load
  - battle transition from story
  - working battle runtime

References:

- [`main.cpp`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L1080)
- [`ScreenState`](/home/tim/Desktop/GameForC/c-project/src/GameMenu/menu_shared.h#L18)

## Slide 2. General Framework

Put on the slide:

- main coordinator: `main.cpp`
- shared runtime state: `AppState`
- main systems:
  - front UI session for menu and story screens
  - VN system for dialogue state
  - save/load system
  - battle demo session for combat entry
  - battle core and battle manager for combat logic
- important architecture fact:
  - menu/story uses RmlUi
  - battle still uses the SDL runtime

Real examples to mention on the slide:

- `shouldUseFrontUiScreen(state)` decides which UI path is active
- `beginBattle(...)` switches from story into battle
- `battleSession->update(...)` and `battleSession->render(...)` run combat

References:

- [`AppState`](/home/tim/Desktop/GameForC/c-project/src/GameMenu/menu_shared.h#L161)
- [`shouldUseFrontUiScreen`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L980)
- [`beginBattle`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L831)
- [`main loop`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L1125)
- [`battle session lifecycle`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L1342)

## Slide 3. OOP / Class Examples

Put on the slide:

- `Window`
  - owns SDL window, renderer, and OpenGL context
  - can switch between OpenGL mode and renderer mode
- `graphics::frontui::Session`
  - owns RmlUi context
  - handles document stack for menu, story, pause, and load
- `battle::demo::Session`
  - wraps the battle runtime used by the main app
- `BattleManager`
  - central gameplay class for combat state and actions
- `BattleCharacter`
  - runtime object for HP, shield, buffs, and ultimate charge

Use these as real C++ examples of encapsulation:

- methods grouped around one responsibility
- state kept inside the class instead of being spread across the whole program

References:

- [`Window`](/home/tim/Desktop/GameForC/c-project/src/window.h#L8)
- [`frontui::Session`](/home/tim/Desktop/GameForC/c-project/src/graphics/front_ui_session.h#L24)
- [`frontui stack logic`](/home/tim/Desktop/GameForC/c-project/src/graphics/front_ui_session.cpp#L27)
- [`battle::demo::Session`](/home/tim/Desktop/GameForC/c-project/src/game/demo_battle_session.h#L19)
- [`BattleManager`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.h#L235)
- [`BattleCharacter`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.h#L127)

## Slide 4. Key Data Structures

Put on the slide:

- `AppState`
  - `screen`
  - `settings`
  - `story`
  - `pendingBattleKey`
  - `pendingBattleWinScript`
  - save/load request fields
- `StorySession`
  - loaded script
  - current entry index
- `Script` and `ScriptEntry`
  - chapter title
  - dialogue entries
  - `speaker`, `text`, `background`, `voice`
  - `battleKey`, `battleWinScript`, `battleLoseScript`
- `SaveGame`
  - chapter
  - entry index
  - saved settings

Why these matter:

- they store the whole app state, story state, and persistence state
- they make the story data-driven instead of hardcoded

References:

- [`StorySession`](/home/tim/Desktop/GameForC/c-project/src/GameMenu/menu_shared.h#L142)
- [`AppState`](/home/tim/Desktop/GameForC/c-project/src/GameMenu/menu_shared.h#L161)
- [`ScriptEntry`](/home/tim/Desktop/GameForC/c-project/src/game/vn/vn_script.h#L15)
- [`Script`](/home/tim/Desktop/GameForC/c-project/src/game/vn/vn_script.h#L43)
- [`SaveGame`](/home/tim/Desktop/GameForC/c-project/src/game/save/save.h#L16)

## Slide 5. Key Battle Data Structures

Put on the slide:

- `BattleDefinition`
  - battle id
  - boss key
  - lineup
  - party size
- `AbilityDefinition`
  - ability type
  - target rule
  - multiplier
  - interaction type
- `TurnActor`
  - actor type
  - party index
  - speed
  - current action value
  - extra-turn state
- `BattleActionEvent`
  - actor
  - action used
  - HP before/after
  - targets hit

Real examples:

- lineup rules are stored in `BattleDefinition`
- rhythm/parry interaction comes from `InteractionType`
- battle UI feedback can read `BattleActionEvent`

References:

- [`BattleDefinition`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.h#L48)
- [`AbilityDefinition`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.h#L89)
- [`TurnActor`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.h#L191)
- [`BattleActionEvent`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.h#L217)
- [`battle loader API`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_loader.h#L18)

## Slide 6. Key Algorithms: Dialogue Progression and State Switching

Put on the slide:

- story script is loaded into `StorySession`
- `vn::showLine(...)` loads the current dialogue line into the VN runtime
- `vn::update(...)` advances:
  - typewriter text
  - icon animation
  - voice playback
  - background fade
- `vn::onSpacePressed()`:
  - finishes the current line immediately, or
  - requests advance to the next entry
- `vn::consumeAdvanceRequest()` is checked in `main.cpp`
- after advance:
  - next dialogue line loads, or
  - battle starts, or
  - chapter ends

Real examples:

- `battleKey` inside `ScriptEntry` triggers combat
- `battleWinScript` supports post-battle branching
- `ScreenState::Playing` and `ScreenState::BattleDemo` control scene switching

References:

- [`loadStoryScript`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L695)
- [`beginStory`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L792)
- [`story progression in main loop`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L1458)
- [`showLine`](/home/tim/Desktop/GameForC/c-project/src/game/vn/vn_system.cpp#L1074)
- [`update`](/home/tim/Desktop/GameForC/c-project/src/game/vn/vn_system.cpp#L1099)
- [`onSpacePressed`](/home/tim/Desktop/GameForC/c-project/src/game/vn/vn_system.cpp#L1308)
- [`consumeAdvanceRequest`](/home/tim/Desktop/GameForC/c-project/src/game/vn/vn_system.cpp#L1325)
- [`ScriptEntry battle fields`](/home/tim/Desktop/GameForC/c-project/src/game/vn/vn_script.h#L26)

## Slide 7. Key Algorithms: Battle Turn Management

Put on the slide:

- speed is converted into action value
- each character and the boss becomes a `TurnActor`
- the actor with the lowest current action value acts next
- after a turn:
  - all actors are shifted forward
  - the chosen action is executed
  - HP, heal, shield, and buffs are updated
  - extra turns can be queued
- automatic follow-up turns are processed until control returns to the player

Real examples:

- `actionValueFromSpeed(spd)`
- `peekNextTurnEvent()`
- `advanceToNextTurnEvent()`
- `executePlayerTurn()`
- `resolveBossAction()`
- `processAutomaticTurns()`

References:

- [`actionValueFromSpeed`](/home/tim/Desktop/GameForC/c-project/src/game/core/turn_system.cpp#L15)
- [`buildInitialTurnState`](/home/tim/Desktop/GameForC/c-project/src/game/core/turn_system.cpp#L36)
- [`peekNextTurnEvent`](/home/tim/Desktop/GameForC/c-project/src/game/core/turn_system.cpp#L77)
- [`advanceToNextTurnEvent`](/home/tim/Desktop/GameForC/c-project/src/game/core/turn_system.cpp#L109)
- [`queueExtraTurnForCharacter`](/home/tim/Desktop/GameForC/c-project/src/game/core/turn_system.cpp#L125)
- [`executePlayerTurn`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.cpp#L751)
- [`processAutomaticTurns`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.cpp#L774)
- [`resolvePlayerAction`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.cpp#L957)
- [`resolveBossAction`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.cpp#L1016)

## Slide 8. Key Algorithms: Ability Resolution and Prototype Evidence

Put on the slide:

- ability resolution uses:
  - `AbilityExecutionContext`
  - `AbilityDefinition`
  - `PresentationContext`
- real combat effects already implemented:
  - attack
  - heal
  - shield
  - special ultimate logic
- prototype evidence to show:
  - startup into menu
  - story dialogue line changing
  - battle trigger from story
  - one player action in battle
  - battle result returning to app flow

Real examples:

- damage calculation in `AbilityType::Attack`
- all-allies heal in `AbilityType::Heal`
- shield application in `AbilityType::Shield`
- battle outcome handling returns to story or menu

References:

- [`AbilityExecutionContext`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.h#L117)
- [`PresentationContext`](/home/tim/Desktop/GameForC/c-project/src/game/core/battle_manager.h#L105)
- [`executeAbilityEffect`](/home/tim/Desktop/GameForC/c-project/src/game/core/ability_system.cpp#L18)
- [`battle outcome return logic`](/home/tim/Desktop/GameForC/c-project/src/main.cpp#L1399)

## Short Final Checklist

Make sure the final PPT includes:

- General framework
- real C++ class examples
- key data structures
- key algorithms
- prototype evidence
- file and line references for every technical slide
