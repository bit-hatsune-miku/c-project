# Game State Architecture

This document is based on `src/` and a small amount of asset data needed to understand runtime flow (`assets/vn/json/*.json`, `assets/combat/*.json`). Existing `docs/` and `build/` were intentionally not used as source material.

## 1. Runtime Overview

There is one top-level executable loop in `src/main.cpp`.

At runtime, the game is split into two layers of state:

1. App-level shell state
   - Owned by `AppState` in `src/GameMenu/menu_shared.h`
   - Controls which screen is active: main menu, settings, story VN, pause, or battle
2. Mode-specific runtime state
   - Story/VN state lives partly in `AppState::story` and partly inside the global `vn` module in `src/game/vn/vn_system.cpp`
   - Battle state lives inside `battle::demo::Session`, which delegates to `BattleSessionCore`, `BattleManager`, and `DemoNarrativeFlow`

High-level flow:

```text
main.cpp loop
  -> AppState.screen decides input/update/render branch
  -> MainMenu / Settings / Pause use AppState directly
  -> Story uses AppState.story + global vn state
  -> Battle uses battle::demo::Session
       -> BattleSessionCore
            -> BattleManager        (authoritative combat state)
            -> HUD / feedback / camera / presentation runtime
       -> DemoNarrativeFlow         (tutorial dialogue gating around battle)
```

## 2. Top-Level Screen State

`AppState` is the only persistent state object owned directly by `main.cpp`.

Important fields:

- `screen`
  - Current high-level mode
- `settingsReturnScreen`
  - Where settings exits back to
- `mainSelection`, `settingsSelection`, `pauseSelection`, `confirmSelection`
  - Pure UI cursor state
- `pauseContext`
  - Whether pause is being used for story or battle
- `settings`
  - `fullscreen`, `voiceVolume`, `textSpeed`
- `story`
  - `vn::Script script`
  - `size_t entryIndex`
  - `bool loaded`
- `noticeText`, `noticeTimer`
  - Ephemeral toast-like UI state

Screen transitions currently look like this:

```text
MainMenu
  Start    -> Playing
  Battle   -> BattleDemo
  Settings -> Settings
  Exit     -> quit

Playing
  ESC      -> PauseMenu (story context)
  SPACE    -> advance VN
  battleId -> BattleDemo
  end      -> MainMenu

PauseMenu / PauseConfirmExit
  Continue -> Playing or BattleDemo
  Settings -> Settings
  Exit     -> MainMenu

BattleDemo
  create Session on entry
  destroy Session on return to MainMenu
```

## 3. Story / VN State

Story mode is not fully self-contained in one object.

### 3.1 What owns story progression

`AppState::story` stores:

- the loaded script data
- the current line index (`entryIndex`)
- whether the chapter JSON has already been loaded

`main.cpp` drives progression:

1. `beginStory()` loads `assets/vn/json/ch0.json` if needed
2. `entryIndex` is set to `0`
3. `applyCurrentEntry()` pushes the current line into the VN runtime
4. While in `ScreenState::Playing`, `vn::update()` runs every frame
5. When `vn::consumeAdvanceRequest()` returns true:
   - current entry's `battleId` is checked
   - `entryIndex` is incremented
   - if `battleId >= 0`, battle starts
   - else the next VN line is shown

### 3.2 What the global VN system owns

The `vn` module in `src/game/vn/vn_system.cpp` uses file-scope globals for:

- current text / speaker / icon / background / voice path
- typewriter progress
- pause state
- loaded voice buffer and active SDL audio device
- current background/icon textures
- viewport size and current renderer pointer
- line-finished / advance-request flags

This means story state is split across:

- serializable progression data in `AppState::story`
- non-serializable live presentation state in `vn`

### 3.3 Save/load implication for story

For a first pass, save only story progression and reconstruct VN presentation by replaying `applyCurrentEntry()` after load.

Recommended story save payload:

- chapter id or script path
- `entryIndex`
- `GameSettings`
- current top-level screen/mode

Do not try to save these in v1:

- partially revealed typewriter character count
- current audio playback offset
- live SDL textures/audio device state

Those are runtime presentation details and can be rebuilt.

## 4. Battle Runtime Architecture

`main.cpp` creates `battle::demo::Session` when `screen == BattleDemo` and no battle session exists yet.

Live path:

```text
battle::demo::Session
  -> SessionImpl
     -> BattleSessionCore core_
     -> DemoNarrativeFlow narrative_
```

### 4.1 `BattleSessionCore`

`BattleSessionCore` is the reusable battle runtime shell. It owns:

- `BattleManager manager_`
- scene entities and loaded textures/icons
- camera state and combat intro animation
- HUD, hint text, and feedback systems
- currently playing ability presentation
- callback hooks back into the host session

Its responsibilities:

- initialize combat data and scene resources
- handle battle input
- update camera, HUD feedback, and combat begin animation
- render world, HUD, feedback, and overlay hooks
- run interactive ability presentations

### 4.2 `BattleManager`

`BattleManager` is the authoritative combat model. If you only serialize one battle object, it should be this one plus a few host-level flags.

It owns:

- immutable-ish combat definitions in `state_`
  - boss definition
  - party character definitions
- mutable combat values
  - `bossCurrentHp_`
  - `bossUltimateCharge_`
  - `characters_` (`BattleCharacter`, each with hp and ultimate charge)
  - `turnState_` (`TurnActor` list and current action values)
- loaded ability definitions
- recent battle action events
- presentation hit bookkeeping flags
- `simulatedActions_`

What actually changes during combat:

- HP
- ultimate charge / orb count
- turn queue contents and each actor's `currentActionValue`
- extra-turn actor insertion/removal
- recent action events

### 4.3 `DemoNarrativeFlow`

The demo battle has extra tutorial/dialogue gating around combat. That logic is separate from `BattleManager`.

It owns:

- intro/tutorial/victory dialogue script arrays
- which dialogue sequence is active
- current line index within that dialogue sequence
- booleans such as:
  - `hasShownMikuFirstSkillTutorial_`
  - `hasShownMikuFirstUltimateTutorial_`
  - `pendingPostLyooAttackAfterMikuUltimateTutorial_`
  - `hasShownBossDefeatedDialogue_`
- `spaceEnabledForBattle_`
- `dialogueInProgress_`

This is save-worthy if you want to resume an in-progress battle cleanly, because it gates whether SPACE advances dialogue or executes turns.

### 4.4 What is derived and should not be persisted

These can be reconstructed on load:

- textures, icons, floor tile texture
- camera staging/interpolation
- HUD transition animations
- damage flashes and shake feedback
- `activePresentation_`
- `activeOverlay_`
- audio one-shots and BGM playback handles

Persisting these would make the first save/load implementation much harder for low value.

## 5. Battle Turn Flow

The core turn loop is simpler than it first looks.

```text
SPACE in battle
  -> BattleSessionCore::handleEvent()
  -> flow::executeDefaultPlayerTurn(manager_)
  -> BattleManager::executePlayerTurn()
       extra turn -> ultimate
       else skill if available
       else standard
  -> BattleManager::resolvePlayerAction()
  -> BattleManager::executeCharacterAction()
  -> hook: DemoNarrativeFlow may interrupt for tutorial dialogue
  -> if allowed, BattleManager::processAutomaticTurns()
       runs boss turns until next actor is not boss
```

Notes:

- Current player action selection is automatic, not menu-driven
- Extra turns are represented by extra `TurnActor` entries
- Ability presentations may apply damage in real time via callbacks, not only at action end

That means a battle save must capture both:

- the logical combat state
- whether the host is currently in dialogue-gated mode versus free battle mode

## 6. Important Architectural Gaps Before Save/Load

These matter more than the file format.

### 6.1 Battle pause menu is wired visually but not entered from gameplay

`main.cpp` only calls `openPauseMenu(state)` from story mode.  
During `ScreenState::BattleDemo`, input is routed directly into the battle session, and `ESC` inside battle marks the session finished.

Practical result:

- the battle pause UI exists
- but the live battle path does not enter it
- so "Load" from battle pause is currently unreachable

### 6.2 Load buttons are placeholders

Main menu and pause menu both show load actions, but they only set notice text.

### 6.3 VN state is global, not object-owned

This makes full-fidelity mid-line saves harder. Rebuilding the current line on load is the pragmatic approach.

### 6.4 `BattleManager` has no snapshot API

Its critical mutable fields are private, so save/load cleanly wants explicit `snapshot()` / `restore()` support rather than external code reaching into internals.

### 6.5 `DemoNarrativeFlow` uses active sequence pointers

For save/load, this should become a stable enum or id, not a raw pointer to one of several vectors.

Suggested enum shape:

```text
None
Intro
PostMikuFirstSkill
PostMikuFirstUltimate
PostLyooAttackAfterMikuUltimate
BossDefeated
```

### 6.6 Battle sessions do not currently remember what they should return to

`BattleDemo` is treated as a self-contained mode. When the battle session finishes, `main.cpp` restores renderer UI and returns to `MainMenu`.

That is fine for the current chapter data, because `ch0.json` ends on a battle trigger. It is not enough for a future story structure where battle is followed by more VN entries.

If you want save/load to be future-proof, battle save data should eventually include resume context such as:

- battle launched from main menu vs story
- chapter/script id
- story `entryIndex` to return to after battle

## 7. What To Save

If the goal is "resume from title screen or pause menu", this is the minimum useful payload.

### 7.1 App-level save payload

- save format version
- current mode
  - `story`
  - `battle`
- `GameSettings`

### 7.2 Story payload

- script id/path, probably `ch0`
- `entryIndex`

Optional:

- whether the game was in pause menu when saved

### 7.3 Battle payload

- battle id or explicit `bossKey` + `partyKeys`
- `BattleManager` snapshot:
  - boss current HP
  - boss ultimate charge
  - each character current HP
  - each character ultimate charge
  - `turnState_.actors`
    - type
    - key
    - asset id
    - title
    - party index
    - priority
    - `isExtraTurn`
    - speed
    - `baseActionValue`
    - `currentActionValue`
  - maybe `simulatedActions_`
- `DemoNarrativeFlow` snapshot:
  - current active sequence id
  - current line index
  - `dialogueInProgress`
  - `spaceEnabledForBattle`
  - tutorial/victory flags
- whether combat-begin animation has already finished

For v1, do not save battle mid-presentation. Save only when no ability presentation is active.

## 8. Recommended Implementation Plan

This is the order that keeps risk down.

### Phase 1: Define save data models

Create plain snapshot structs, separate from rendering/runtime classes.

Suggested groups:

- `SaveGame`
- `StorySaveState`
- `BattleSaveState`
- `BattleManagerSnapshot`
- `DemoNarrativeSnapshot`

### Phase 2: Add explicit snapshot APIs

Add methods instead of exposing internals.

Recommended additions:

- `BattleManager::snapshot() const`
- `bool BattleManager::restore(const BattleManagerSnapshot&)`
- `DemoNarrativeFlow::snapshot() const`
- `bool DemoNarrativeFlow::restore(const DemoNarrativeSnapshot&)`

This is the biggest missing seam right now.

### Phase 3: Add a save system module

Create something like:

```text
src/game/save/save_game.h
src/game/save/save_game.cpp
```

Responsibilities:

- convert snapshot structs to JSON
- read/write a save file
- validate version and required fields

### Phase 4: Wire app-level save creation/loading

In `main.cpp`:

- save from story mode using `AppState`
- save from battle mode using `battleSession`
- on load, recreate the correct mode:
  - story: rebuild VN line from `entryIndex`
  - battle: recreate session and restore snapshots

### Phase 5: Make battle resume a first-class flow

Before exposing battle saves in UI, change battle `ESC` behavior so it opens pause instead of finishing immediately.

Without this, battle save/load from pause is awkward and inconsistent.

### Phase 6: Add v1 restrictions explicitly

Ship a narrow, stable version first:

- allow save in story between lines
- allow save in battle only when:
  - no presentation is active
  - no combat-begin animation is active
  - no dialogue transition is closing

That avoids serializing transient animation/audio state.

## 9. Best First Refactor

If you want the single best starting change before implementing actual saving, do this:

1. Add snapshot/restore support to `BattleManager`
2. Replace `DemoNarrativeFlow`'s active vector pointer with a stable sequence enum
3. Expose `battle::demo::Session` methods to snapshot/restore battle progress

Once those seams exist, the JSON file format is the easy part.

## 10. Concrete Starting Point

If I were implementing this codebase, I would start here:

1. Add `BattleManagerSnapshot` and `BattleManager::snapshot()/restore()`
2. Add `DemoNarrativeSnapshot` and `DemoNarrativeFlow::snapshot()/restore()`
3. Add `battle::demo::Session::snapshot()/restore()`
4. Add a `SaveGame` JSON serializer
5. Hook main menu `Load` to restore story or battle
6. Change battle `ESC` from "finish session" to "open battle pause"

## 11. Current Decisions

Based on follow-up answers, the current intended save behavior is:

- save/load should happen during story
- reloading should return to before combat, not inside combat
- battle state does not need to be serialized for v1

That simplifies the first implementation a lot.

### 11.1 Recommended v1 save model

Persist only story progression and restore back into `ScreenState::Playing`.

Recommended payload:

- save format version
- `GameSettings`
- chapter/script id
- story `entryIndex`

### 11.2 How to handle battle triggers

Because reload should return to before combat:

- if the current story entry triggers `battleId`, saving should anchor to that story position
- on load, the game should restore the VN state so the player is back on the pre-battle line
- battle should only start again once the player advances from that line

This means v1 does not need:

- `BattleManager` serialization
- `DemoNarrativeFlow` serialization
- battle session reconstruction

It only needs a stable story checkpoint policy.

### 11.3 Main implementation impact

If this direction holds, the first real code pass should shift from battle snapshots to:

1. define a story save file format
2. decide exactly what "current position" means inside one VN line
3. wire save/load into story mode
4. ensure a battle-trigger line reloads to before battle begins

## 12. Remaining Question

These are the questions that affect the design most:

1. What does "be specific" mean for story-line resume?
   - Option A: load at the start of the current VN line
   - Option B: load with exact partially revealed text progress on that line
   - Option C: load with exact text progress and current voice playback position
