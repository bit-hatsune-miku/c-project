# Midterm Report

This report is written based on the checklist in [`docs/2026-Midterm Report checklist.md`](/c-project/docs/2026-Midterm%20Report%20checklist.md).

The purpose of this report is to explain the current program as a system, not just as a collection of files. The main focus is the shipped main application flow, while `battle_testing` is included as a secondary experimental battle architecture.

Relevant supporting documents:

- [`docs/architecture.md`](/c-project/docs/architecture.md)
- [`docs/mermaid/gamestate.mermaid`](/c-project/docs/mermaid/gamestate.mermaid)

## Project Overview

This project is a visual novel game with a battle system.

At the current stage, the program already supports:

- main menu
- story / visual novel progression
- story pause menu
- story settings
- load/save flow
- battle entry from the menu
- battle entry from story script events
- party setup for battles that allow lineup selection
- tutorial-style battle dialogue overlays
- standalone battle testing programs

The most important architectural fact is that the current project uses two rendering paths inside one codebase:

- the main menu and story-related UI use the RmlUi front-end stack when enabled
- the main shipped battle path still uses the SDL battle runtime

So the game is already modular, but the rendering architecture is still partly split.

---

## 1. General Framework

This section explains the overall architecture of the program, how modules are divided, and how they work together during execution.

### 1.1 High-Level System Structure

The program can be understood as five major layers:

1. Application shell
2. Story / visual novel system
3. Front-end UI system
4. Battle gameplay system
5. Battle rendering and presentation system

At runtime, the main application uses [`src/main.cpp`](/c-project/src/main.cpp) as the top-level coordinator. It owns the main loop, the screen state, the active UI mode, and the transitions between story and battle.

High-level runtime flow:

```text
Program start
-> create Window
-> initialize VN and/or front-ui systems
-> enter main loop
-> process input
-> update active state
-> render current screen
-> present frame
```

The game state transitions themselves are represented by `AppState.screen`, which is documented visually in [`docs/mermaid/gamestate.mermaid`](/c-project/docs/mermaid/gamestate.mermaid).

### 1.2 Executable Structure

The build configuration in [`CMakeLists.txt`](/c-project/CMakeLists.txt) creates multiple executables.

The most important ones are:

- `OurUndergroundBITIdol`
- `ch0`
- `finale`
- `battle_testing`
- `demo` and the demo variants

The main report focuses on `OurUndergroundBITIdol` / `ch0` / `finale`, because these represent the main shipped application flow.

#### Main shipped executables

These executables include:

- `src/main.cpp`
- story runtime (`src/game/vn/`)
- save system (`src/game/save/`)
- battle logic (`src/game/core/`)
- main-app battle wrapper (`src/game/demo_battle_session.cpp`)
- battle presentations (`src/game/presentation/`)
- front-end RmlUi screens (`src/graphics/front_ui_*`) when `ENABLE_RMLUI=ON`

#### Standalone battle app

`battle_testing` is a side architecture used for battle HUD and rendering experiments. It uses:

- [`src/rmlui_battle_main.cpp`](/c-project/src/rmlui_battle_main.cpp)
- [`src/game/app_battle_session.cpp`](/c-project/src/game/app_battle_session.cpp)

This is not the main story battle path, but it is still important because it shows an alternative battle UI and rendering design inside the same codebase.

### 1.3 Header Files and Source Files

The project follows a standard C++ modular structure:

- header files (`.h`) define interfaces, structs, enums, and class declarations
- source files (`.cpp`) implement the behavior

Examples:

- [`src/window.h`](/c-project/src/window.h) declares the `Window` class
- [`src/window.cpp`](/c-project/src/window.cpp) implements SDL window creation, renderer setup, OpenGL setup, and frame presentation

- [`src/game/vn/vn_system.h`](/c-project/src/game/vn/vn_system.h) declares the VN interface
- [`src/game/vn/vn_system.cpp`](/c-project/src/game/vn/vn_system.cpp) implements dialogue rendering, voice playback, typewriter text, and presentation state export

- [`src/game/core/battle_manager.h`](/c-project/src/game/core/battle_manager.h) declares the core battle state and APIs
- [`src/game/core/battle_manager.cpp`](/c-project/src/game/core/battle_manager.cpp) implements turn progression, damage, healing, buff logic, and battle resolution

This separation matters because it keeps the program understandable:

- other modules can depend on a header without needing to know internal implementation details
- interfaces remain reusable
- the codebase is easier to navigate by responsibility

### 1.4 Module Organization

The project is organized into several functional areas.

#### Application shell

- [`src/main.cpp`](/c-project/src/main.cpp)
- [`src/window.cpp`](/c-project/src/window.cpp)
- [`src/GameMenu/menu_shared.h`](/c-project/src/GameMenu/menu_shared.h)

Responsibilities:

- create the window
- run the main loop
- own `AppState`
- switch between screens
- connect story and battle
- handle save/load side effects

#### Story / VN system

- [`src/game/vn/vn_system.cpp`](/c-project/src/game/vn/vn_system.cpp)
- [`src/game/vn/vn_script.cpp`](/c-project/src/game/vn/vn_script.cpp)

Responsibilities:

- load VN JSON scripts
- hold the current line, speaker, portrait, and background
- update the typewriter effect
- play voice audio
- render VN content in SDL mode
- expose presentation data for the RmlUi story screen

#### Front-end UI system

- [`src/graphics/front_ui_session.cpp`](/c-project/src/graphics/front_ui_session.cpp)
- [`src/graphics/front_ui_main_menu.cpp`](/c-project/src/graphics/front_ui_main_menu.cpp)
- [`src/graphics/front_ui_story.cpp`](/c-project/src/graphics/front_ui_story.cpp)
- [`src/graphics/front_ui_pause.cpp`](/c-project/src/graphics/front_ui_pause.cpp)
- [`src/graphics/front_ui_load.cpp`](/c-project/src/graphics/front_ui_load.cpp)
- [`src/graphics/front_ui_settings.cpp`](/c-project/src/graphics/front_ui_settings.cpp)

Responsibilities:

- initialize RmlUi
- manage document stack
- map UI controls back to `AppState`
- mirror VN state into Rml documents
- support menu, story overlay, pause, load, and settings screens

#### Battle gameplay system

- [`src/game/core/battle_loader.cpp`](/c-project/src/game/core/battle_loader.cpp)
- [`src/game/core/battle_manager.cpp`](/c-project/src/game/core/battle_manager.cpp)
- [`src/game/core/turn_system.cpp`](/c-project/src/game/core/turn_system.cpp)
- [`src/game/core/battle_turn_flow.cpp`](/c-project/src/game/core/battle_turn_flow.cpp)
- [`src/game/core/battle_flow_controller.cpp`](/c-project/src/game/core/battle_flow_controller.cpp)
- [`src/game/core/ability_system.cpp`](/c-project/src/game/core/ability_system.cpp)

Responsibilities:

- load battle definitions and ability definitions from JSON
- initialize battle state
- control turn order
- resolve actions
- apply damage, healing, buffs, and shield effects
- trigger presentations

#### Battle rendering and presentation

- [`src/game/battle_session_core.cpp`](/c-project/src/game/battle_session_core.cpp)
- [`src/game/render/`](/c-project/src/game/render)
- [`src/game/presentation/`](/c-project/src/game/presentation)
- [`src/game/demo_battle_session.cpp`](/c-project/src/game/demo_battle_session.cpp)

Responsibilities:

- create scene entities
- position battle characters in world space
- move the camera
- draw the world and HUD
- show feedback and ability presentations
- wrap battle flow for the main application

### 1.5 How Modules Communicate

The project uses a mostly direct, modular communication model.

#### Main application to story system

`src/main.cpp` calls functions like:

- `vn::showLine(...)`
- `vn::update(...)`
- `vn::consumeAdvanceRequest()`
- `vn::reset()`

This means `main.cpp` controls story progression, while `vn_system` controls the actual presentation state of the current line.

#### Main application to front UI

`main.cpp` passes `AppState` into `graphics::frontui::Session`.

The front UI session:

- reads `AppState` to decide which Rml documents should be visible
- lets document controllers update the visual state
- returns commands such as main-menu activation or settings changes

So the data direction is mostly:

```text
AppState -> front_ui_session -> Rml documents
Rml events -> document controllers -> command/app state updates
```

#### Main application to battle

When story or menu flow requests a battle, `main.cpp` creates `battle::demo::Session`.

That wrapper:

- loads the selected battle definition
- optionally opens party setup
- creates `BattleSessionCore`
- returns an outcome to the main app

#### Battle logic to battle rendering

`BattleSessionCore` owns a `BattleManager`.

The render layer reads from that manager:

- current HP
- turn preview
- party alive/dead state
- recent action events

So battle logic remains the authority, and rendering mirrors that state.

#### Battle logic to presentations

The ability system generates `PresentationContext`.

The active battle session injects a presentation interaction runner:

- the logic layer triggers a presentation request
- the session wrapper executes the visual presentation
- the result is sent back to the gameplay logic

This is important because it separates core combat rules from attack-specific presentation details.

### 1.6 Main Shipped App Flow

The main shipped app flow is the most important architecture to explain in the midterm.

The current flow is:

```text
Boot
-> Main Menu
-> Story / VN
-> optional Pause / Load / Settings
-> battle trigger from script or menu
-> battle session
-> return to story or main menu
```

The state transitions are controlled by the enum `ScreenState` in [`src/GameMenu/menu_shared.h`](/c-project/src/GameMenu/menu_shared.h).

Important states:

- `MainMenu`
- `Playing`
- `PauseMenu`
- `Settings`
- `LoadMenu`
- `BattleDemo`

Even though the name `BattleDemo` sounds temporary, it is the current main-app battle state used by story battles as well.

### 1.7 Main App Rendering Architecture

The main app currently switches rendering backend depending on screen type.

#### Front-end screens

For these screens, the app uses the front-ui system:

- main menu
- story UI
- story pause
- story load
- story settings

This path uses:

- OpenGL window mode
- RmlUi
- `graphics::frontui::Session`

#### Main-app battle

For battle, the app switches back to:

- SDL renderer mode
- `battle::demo::Session`
- `BattleSessionCore`

So one `AppState` controls both paths, but the underlying rendering backend changes at runtime.

This is already functional, but it is also one of the most important architectural complexities in the current project.

### 1.8 Side Architecture: `battle_testing`

The project also contains a separate battle runtime used for experimentation.

This path is:

```text
src/rmlui_battle_main.cpp
-> battle::app::Session
-> BattleManager
-> software scene renderer
-> OpenGL screen blitter
-> RmlUi HUD
```

This side architecture matters because it demonstrates:

- a cleaner Rml-based HUD layer
- a separate pause/settings/tutorial/rhythm overlay system
- an alternate battle runtime design

However, it is not yet the main shipped battle path, and it currently simplifies the world scene to one visible party member plus the boss.

---

## 2. Key Data Structures

This section explains the most important data structures in the system and why they were chosen.

### 2.1 `AppState`

Defined in [`src/GameMenu/menu_shared.h`](/c-project/src/GameMenu/menu_shared.h).

This is the main state container for the application shell.

Important fields include:

- `screen`
- `settingsReturnScreen`
- `loadReturnScreen`
- `mainSelection`
- `pauseSelection`
- `settings`
- `story`
- `pendingBattleKey`
- `pendingBattleLaunchedFromStory`
- `pendingBattleWinScript`
- `pendingBattleLoseScript`
- save/load request fields

Why this structure was chosen:

- the main loop needs one central object representing the global UI and navigation state
- the current architecture is screen-driven, so an app-level state structure is appropriate
- it allows story, menu, save/load, and battle transition logic to communicate through one shared state object

How it supports the system:

- screen routing becomes straightforward
- return paths for pause/settings/load are explicit
- story-to-battle-to-story transitions can be coordinated without hidden globals

### 2.2 `GameSettings`

Also defined in [`src/GameMenu/menu_shared.h`](/c-project/src/GameMenu/menu_shared.h).

Fields:

- `fullscreen`
- `voiceVolume`
- `textSpeed`

Why it matters:

- it represents user-adjustable live settings
- it is shared by story, menus, and battle UI
- it is also saved in `SaveGame`

This is a good example of a small state structure used across multiple subsystems.

### 2.3 `StorySession`

Defined in [`src/GameMenu/menu_shared.h`](/c-project/src/GameMenu/menu_shared.h).

Fields:

- `vn::Script script`
- `std::size_t entryIndex`
- `bool loaded`

Why it was chosen:

- the story system needs to know both the full script and the exact current line
- a dedicated structure avoids scattering these values across the app

How it supports the design:

- save/load can restore the story position
- story progression logic can advance line by line
- battle triggers can be embedded into story entries while still using the same session state

### 2.4 `vn::ScriptEntry` and `vn::Script`

Defined in [`src/game/vn/vn_script.h`](/c-project/src/game/vn/vn_script.h).

`vn::ScriptEntry` contains:

- `speaker`
- `text`
- `type`
- `background`
- `voice`
- `fontPath`
- `icon`
- icon animation settings
- `autoAdvanceOnVoiceEnd`
- `battleKey`
- `battleId`
- `battleWinScript`
- `battleLoseScript`

`vn::Script` contains:

- `chapter`
- `title`
- `entries`

Why these structures were chosen:

- the VN system is content-driven
- each dialogue line must carry both text and metadata
- battle can be triggered directly from story data

How they support the game:

- one script entry can represent dialogue, narration, or thought
- the same data structure controls both presentation and progression
- battle transitions are authored in JSON instead of hardcoded in the main loop

This is one of the most important structures in the whole project because it connects story content, UI presentation, and gameplay transitions.

### 2.5 `save::SaveGame` and `save::SlotInfo`

Defined in [`src/game/save/save.h`](/c-project/src/game/save/save.h).

`SaveGame` contains:

- `version`
- `timestamp`
- `label`
- `chapter`
- `entryIndex`
- saved settings

`SlotInfo` contains:

- file path
- display label
- timestamp
- autosave flag

Why these structures matter:

- save/load is part of the current prototype
- the app needs a persistent representation of story progress
- load menu UI needs compact display information for available saves

These structures show that the project already supports state persistence, not just temporary runtime flow.

### 2.6 `BattleDefinition`, `BossDefinition`, and `CharacterDefinition`

Defined in [`src/game/core/battle_manager.h`](/c-project/src/game/core/battle_manager.h), filled by [`src/game/core/battle_loader.cpp`](/c-project/src/game/core/battle_loader.cpp).

#### `BattleDefinition`

Important fields:

- `key`
- `id`
- `name`
- `description`
- `type`
- `bossKey`
- `stageKey`
- `isLineupFixed`
- `partySize`
- `lineup`
- `lockedLineup`

Purpose:

- represents authored battle setup
- determines who the boss is
- determines whether party setup is needed

#### `BossDefinition`

Important fields:

- identity and asset fields
- stats such as `spd`, `atk`, `hp`
- ability identifiers
- orb settings
- BGM settings

Purpose:

- represents boss design data loaded from JSON

#### `CharacterDefinition`

Important fields:

- identity and asset fields
- class
- stats
- ability identifiers
- ultimate settings
- `baseShield`

Purpose:

- represents playable unit data used by party setup and battle runtime

Why these structures were chosen:

- battle content is meant to be authored externally
- definitions separate design-time data from runtime state

How they support the system:

- new battles can be created in JSON
- party setup can validate lineups against battle rules
- battle manager can build runtime state from clean data definitions

### 2.7 `BattleState`

Defined in [`src/game/core/battle_manager.h`](/c-project/src/game/core/battle_manager.h).

Fields:

- `BossDefinition boss`
- `std::vector<CharacterDefinition> party`

Why it matters:

- this structure groups the currently active battle participants
- it is simple, readable, and shared across battle logic and rendering

It is a compact "who is in this fight" representation.

### 2.8 `BattleCharacter`

Defined in [`src/game/core/battle_manager.h`](/c-project/src/game/core/battle_manager.h), implemented in [`src/game/core/battle_manager.cpp`](/c-project/src/game/core/battle_manager.cpp).

This is the runtime version of a party member.

It stores:

- the original `CharacterDefinition`
- current HP
- current ultimate charge
- shield
- speed buff bonus
- attack buff bonus
- party index

Why it was chosen:

- definitions alone are not enough for battle
- runtime values such as HP and buffs change during play

How it supports the system:

- keeps mutable battle state separated from immutable design data
- encapsulates behavior like `receiveDamage`, `receiveHealing`, and `canUseUltimate`

### 2.9 `AbilityDefinition`

Defined in [`src/game/core/battle_manager.h`](/c-project/src/game/core/battle_manager.h).

Important fields:

- `id`
- `name`
- `instructionHint`
- `type`
- `targetRule`
- `multiplier`
- `flatHeal`
- buff/debuff fields
- `interactionType`
- `presentationId`

Why it matters:

- abilities are core to the battle system
- a single structure describes both gameplay effect and presentation linkage

This is a strong data design choice because it lets authored content control combat behavior without hardcoding every move.

### 2.10 `TurnActor`, `TurnState`, and `TurnEvent`

Defined in [`src/game/core/battle_manager.h`](/c-project/src/game/core/battle_manager.h).

#### `TurnActor`

Contains:

- participant type
- key and asset ID
- title
- party index
- priority
- extra-turn flags
- speed
- base action value
- current action value

#### `TurnState`

Contains:

- `std::vector<TurnActor> actors`

#### `TurnEvent`

Contains:

- consumed action value
- validity flag
- acting actor index

Why these structures were chosen:

- the battle system is based on action-value turn order, not a simple alternating turn model
- turn scheduling needs an explicit representation of current and queued actors

How they support the system:

- next actor preview can be shown in the HUD
- extra turns can be queued
- speed buffs can affect turn order
- battle rendering can focus the camera on the next acting unit

These are among the most important data structures in the battle system.

### 2.11 `BattleActionEvent`

Defined in [`src/game/core/battle_manager.h`](/c-project/src/game/core/battle_manager.h).

This structure records the result of executed battle actions.

It includes:

- acting side
- action type
- ability metadata
- boss HP before/after
- party target HP before/after
- flags indicating whether presentation handled audio

Why it matters:

- the UI, feedback, and audio layers need to react after gameplay logic resolves
- this structure acts as a bridge between core logic and presentation/feedback systems

This is a good design because it reduces direct coupling between the battle manager and rendering code.

### 2.12 `render::SceneEntity`

Defined in [`src/game/render/battle_scene_types.h`](/c-project/src/game/render/battle_scene_types.h).

Important fields:

- `key`
- `assetName`
- `isBoss`
- `worldX`, `worldY`, `worldZ`
- `fallbackColor`
- `partyIndex`
- visibility fields
- sprite alpha and offset

Why this structure exists:

- gameplay state and rendering state are not identical
- render code needs world positions, visibility, alpha, and visual offsets

How it supports the design:

- the battle session can stage a renderable scene each frame
- visual transitions can be controlled without changing battle logic

### 2.13 `BattleSessionCore::Hooks`

Defined in [`src/game/battle_session_core.h`](/c-project/src/game/battle_session_core.h).

This is one of the most important extension structures in the project.

It contains function callbacks for:

- dialogue state queries
- space-key interception
- pre-update narrative logic
- presentation audio hooks
- post-render overlays
- shutdown cleanup

Why it was chosen:

- the shared SDL battle core must be reusable
- tutorial battles and plain battles need different wrapper behavior

How it supports the system:

- `BattleSessionCore` stays reusable
- `demo_battle_session.cpp` can inject tutorial dialogue and audio behavior without rewriting the battle core

### 2.14 Why These Data Structures Fit the Project

Taken together, these structures show the design direction of the project:

- app-level flow is controlled by explicit screen/state structs
- story content is data-driven through JSON-backed `ScriptEntry`
- battle content is data-driven through definitions loaded from JSON
- mutable runtime state is separated from authored definitions
- render state is separated from gameplay state
- callback structures are used where wrapper customization is needed

This is a reasonable architecture for a game that mixes story systems and battle systems inside one application.

---

## 3. Key Algorithms / Logic

This section explains the core logic that makes the system work.

### 3.1 Startup and Scene Switching

The central scene-switching logic lives in [`src/main.cpp`](/c-project/src/main.cpp).

The algorithm is roughly:

1. initialize window and global systems
2. create `AppState`
3. if front UI should be active, initialize `graphics::frontui::Session`
4. enter main loop
5. poll events
6. route events according to `AppState.screen`
7. process save/load requests and pending battle requests
8. update the active subsystem
9. render the current screen

The key decision function is `shouldUseFrontUiScreen(state)`.

This function decides whether the app should use:

- front-ui OpenGL + RmlUi

or:

- SDL renderer mode

This is one of the most important algorithms in the current architecture because it determines the rendering path and the active UI system.

### 3.2 Story Progression Algorithm

Story progression uses the VN runtime plus script state.

The logic is:

1. `StorySession` stores current script and current `entryIndex`
2. `applyCurrentEntry(...)` reads the current `vn::ScriptEntry`
3. it resolves background, icon, voice, and text resources
4. it calls `vn::showLine(...)`
5. `vn_system` handles typewriter progression through `vn::update(...)`
6. when the player advances, `vn::consumeAdvanceRequest()` is checked
7. `entryIndex` increments
8. the next entry is applied, or a battle is started, or the story ends

Why this algorithm is effective:

- story content and runtime progression are clearly separated
- each script line is handled in a consistent way
- battle transitions can be inserted naturally into narrative progression

### 3.3 Dialogue Rendering and Typewriter Logic

The VN system contains an internal typewriter algorithm.

The logic is:

1. when a line starts, visible character count is reset to zero
2. every frame, `deltaSeconds * charsPerSecond` is accumulated
3. visible character count increases accordingly
4. if voice playback finishes and `autoAdvanceOnVoiceEnd` is enabled, advance is requested
5. if the player presses space before the line is fully visible, all characters become visible immediately
6. if the player presses space again after the line is complete, the line advances

This algorithm is important because it connects:

- timing
- player input
- voice playback
- script progression

### 3.4 Save / Load State Restoration

The save/load algorithm is also central to the working prototype.

Saving:

1. build a `save::SaveGame` from `AppState`
2. include chapter ID, entry index, and current settings
3. write autosave or manual save JSON to disk

Loading:

1. read `SaveGame` from file
2. map chapter ID back to a VN script path
3. reload the script
4. restore `StorySession`
5. restore `GameSettings`
6. call `vn::reset()`
7. apply the current script entry
8. return to `Playing`

Why this matters:

- it proves the game state is restorable, not just playable in one session
- it demonstrates state serialization and deserialization
- it shows that story and settings state are tied together coherently

### 3.5 Battle Initialization Algorithm

Battle entry can happen from:

- main menu
- story script event

Main logic:

1. a battle key or battle ID is requested
2. `battle_loader` validates and loads the `BattleDefinition`
3. `AppState` stores pending battle information
4. `screen` becomes `BattleDemo`
5. `battle::demo::Session` is created
6. if the battle supports lineup selection, `BattlePartySetupScreen` starts first
7. after lineup confirmation, `BattleSessionCore` is initialized

This algorithm is important because it shows how story and battle are connected through authored content instead of hardcoded fight scripts.

### 3.6 Party Setup Logic

The party setup system is implemented in [`src/game/demo/battle_party_setup.cpp`](/c-project/src/game/demo/battle_party_setup.cpp).

The algorithm is:

1. load roster from `characters.json`
2. read the active `BattleDefinition`
3. apply locked lineup requirements
4. seed an initial selection
5. allow player input to change selected party members
6. validate that the lineup satisfies `partySize`
7. return `PartySetupResult`

Why this is important:

- battle configuration is not hardcoded
- the same system supports fixed battles and selectable-lineup battles
- the prototype already supports a practical pre-battle setup step

### 3.7 Turn Order Algorithm

Turn order is handled through the turn system and the battle manager.

This is not a simple alternating turn loop. It uses action values derived from speed.

General logic:

1. create `TurnActor` objects for boss and characters
2. compute action values from speed
3. preview the next actor by choosing the smallest effective action value, with tie-breaking rules
4. execute the chosen actor
5. reset or advance action values
6. queue extra turns when required
7. update the preview for the next frame

This algorithm supports:

- speed-based differentiation
- boss vs player turn preview
- extra-turn mechanics
- dynamic turn order after buffs or special actions

This is one of the strongest internal logic systems in the battle architecture.

### 3.8 Player Action Resolution

Battle actions are handled through `BattleManager`.

Main logic:

1. determine whether a player action is legal
2. resolve the relevant ability definition
3. build `AbilityExecutionContext`
4. apply gameplay effects such as damage, heal, shield, or buff
5. optionally trigger a presentation through `PresentationContext`
6. update HP, charges, buffs, and recent action events
7. process automatic turns if needed

This algorithm is important because it shows the separation between:

- battle rules
- authored ability data
- presentation logic

### 3.9 Recent Action Event Pipeline

After actions are resolved, the manager stores `BattleActionEvent` records.

Then other systems consume them:

- HUD
- battle feedback
- audio triggers
- wrapper logic

This is effectively a lightweight event pipeline:

```text
BattleManager resolves action
-> records BattleActionEvent
-> feedback/audio/UI systems consume event
-> event list is cleared
```

This is a useful algorithmic design because it prevents the logic layer from directly controlling every presentation detail.

### 3.10 Tutorial Battle Narrative Gating

The tutorial battle wrapper adds another important algorithm layer.

Through `DemoNarrativeFlow`:

1. intro tutorial dialogue starts before battle control opens
2. while dialogue is active, battle space input is blocked
3. after specific actions, tutorial dialogue sequences can start again
4. automatic progression is delayed until dialogue conditions are satisfied
5. after boss defeat, final dialogue can play before the session ends

This is a good example of wrapper-level control built on top of reusable battle core logic.

### 3.11 Main-App Battle Rendering Logic

`BattleSessionCore` stages battle rendering in a fixed order.

General render algorithm:

1. check for combat intro animation
2. choose the focused entity
3. update camera staging
4. render battle floor
5. render world entities
6. render current presentation at the correct layer
7. render HUD
8. render feedback overlays
9. render post-render overlay hooks such as VN dialogue

Why this matters:

- it separates gameplay state from visible battle staging
- it gives ability presentations multiple visual layers
- it makes the render order deterministic and easier to reason about

### 3.12 Front UI Document Synchronization

The front-ui architecture also contains an important algorithmic pattern.

For the story screen:

1. `vn::getPresentationState()` exports current VN presentation data
2. `front_ui_story.cpp` compares current values against cached values
3. only changed document elements are updated
4. background, portrait, speaker, and text are mirrored into the Rml document

This is a retained-state synchronization algorithm rather than immediate drawing.

That is important because it demonstrates a more modern UI architecture than raw frame-by-frame manual drawing.

### 3.13 Side Algorithm: `battle_testing`

The separate `battle_testing` runtime uses a different rendering logic:

1. create `BattleManager`
2. render battle world to `SoftwareSceneRenderer`
3. upload surface to `GlScreenBlitter`
4. draw full-screen GL quad
5. update Rml battle HUD

This is useful for showing implemented progress in experimental architecture:

- it already has pause/settings/tutorial/rhythm overlay logic
- it already updates a Rml HUD document from battle state
- it already supports an alternate battle UI model

However, it is still a side architecture, not the main shipped battle runtime.

---

## 4. Current Prototype and Implemented Progress

This section focuses only on what is already implemented and demonstrable.

### 4.1 Startup and Main Menu

Implemented:

- application starts into the main menu
- main menu supports start, load, battle, settings, and exit
- front-end UI presentation exists through RmlUi when enabled

This satisfies the prototype requirement of showing program startup.

### 4.2 Story / Visual Novel Flow

Implemented:

- VN scripts load from JSON
- backgrounds, portraits, and dialogue lines are shown
- typewriter text works
- voice playback works
- line advancement works
- story can automatically trigger battles from script entries

This is already a functional story system rather than a placeholder.

### 4.3 Pause, Settings, and Load

Implemented:

- pause menu
- story settings
- load menu
- save and overwrite flow
- autosave support
- restoring story from save

These are important signs of real technical progress because they depend on state management, persistence, and return-path logic.

### 4.4 Main-App Battle System

Implemented:

- battle entry from menu
- battle entry from story
- party setup for configurable battles
- turn order system
- action execution
- boss and player HP/orb logic
- shield and buff support
- feedback overlays
- ability presentations
- battle return to story or main menu depending on outcome

This is already beyond a mock-up. It is a working gameplay loop connected to the story system.

### 4.5 Tutorial Battle Narrative Layer

Implemented:

- dialogue before battle control starts
- action-triggered tutorial dialogue
- boss-defeat dialogue
- gating of battle input while tutorial dialogue is active

This is especially valuable in a prototype presentation because it shows integration between the VN system and the battle system.

### 4.6 Standalone Battle Experiments

Implemented in side executables:

- `battle_testing` with Rml HUD and OpenGL presentation
- demo battle executables with different default encounters

These are useful as evidence that the project is exploring multiple rendering and UI approaches while still keeping shared battle logic.

### 4.7 What the Prototype Demonstrates

A prototype video can already show:

1. program startup
2. main menu interaction
3. story progression
4. battle trigger
5. battle gameplay sequence
6. return path after battle

This aligns directly with the checklist's requirement for a functional gameplay sequence and evidence of implemented systems.

---

## 5. Suggested Presentation Structure

Since the midterm presentation is limited to 4-5 minutes, the report should be turned into a short, focused structure.

A practical presentation order would be:

### Slide 1: Game Overview

- visual novel + battle system
- current prototype scope
- one sentence on current architecture split

### Slide 2: General Framework

- use the game state diagram from [`docs/mermaid/gamestate.mermaid`](/c-project/docs/mermaid/gamestate.mermaid)
- explain main menu -> story -> battle -> return flow
- explain that `main.cpp` coordinates the whole program

### Slide 3: Module Architecture

- app shell
- VN system
- front UI
- battle core
- battle rendering/presentation

Use a simple architecture diagram instead of code.

### Slide 4: Key Data Structures

Focus on:

- `AppState`
- `vn::ScriptEntry`
- `BattleDefinition`
- `TurnState`
- `BattleActionEvent`

Explain design reasoning, not syntax detail.

### Slide 5: Key Algorithms

Focus on:

- scene switching
- dialogue progression
- battle initialization
- turn order and action resolution

Keep this conceptual and flow-based.

### Slide 6: Prototype Demo

End with:

- startup
- story sequence
- battle sequence

This follows the checklist advice to save the best for last.

---

## 6. Conclusion

The current project already demonstrates meaningful technical progress.

From an architecture perspective, the codebase already has:

- a clear application shell
- data-driven VN content
- data-driven battle content
- reusable battle gameplay logic
- a working main-app battle wrapper
- a working front-end UI stack
- save/load support
- a prototype that connects story and battle in one flow

The most important point for the midterm is not just that code exists, but that the system can be explained as an organized program:

- global app state controls screen flow
- story content is loaded from script data
- battle content is loaded from authored combat data
- gameplay logic is handled by a shared battle core
- rendering and UI layers reflect that state through dedicated modules

That means the project already satisfies the core midterm expectation: it can be presented as a structured system, not just as isolated code fragments.
