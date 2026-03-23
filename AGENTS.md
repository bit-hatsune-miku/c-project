# Agent Guidelines

- If code appears faulty or unnecessarily weak, refactor or replace it with a better solution.
- Ask the user for clarification when the correct path is unclear or high-risk.
- Use available tools pragmatically: browse the web, inspect libraries, or spin up subagents to clone (library) and look up references.
- Prefer maintainable, production-grade solutions over quick hacks.
- Keep code quality high and align changes with established best practices.
- Split code up as components so that an AI agent would not need to read all of the code just to check or make a change. (To avoid context bloat)
- No not hardcode long text especially ones with voice lines, find an existing json file or create a new one if it does not exist.
- Chinese text rendering:
  Use `src/platform/text_fallback.h` for all SDL_ttf fallback decisions. Latin text stays on the normal UI font; only CJK characters and full-width/CJK punctuation should switch to the CJK font.
- Do not use broad checks like `containsNonAscii()` or switch the whole string to the CJK font. That makes English numbers/symbols look wrong.
- Prefer run-based rendering, not per-glyph rendering, for mixed-language strings. Keep Latin runs intact so spacing and appearance stay normal; only split where the font actually changes.
- Align mixed-font text by ascent/baseline when combining Latin and CJK runs, or Chinese will sit too low and centered labels/bars will look off.
