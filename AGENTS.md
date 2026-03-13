# Agent Guidelines

- If code appears faulty or unnecessarily weak, refactor or replace it with a better solution.
- Ask the user for clarification when the correct path is unclear or high-risk.
- Use available tools pragmatically: browse the web, inspect libraries, or spin up subagents to clone (library) and look up references.
- Prefer maintainable, production-grade solutions over quick hacks.
- Keep code quality high and align changes with established best practices.
- Split code up as components so that an AI agent would not need to read all of the code just to check or make a change. (To avoid context bloat)
- No not hardcode long text especially ones with voice lines, find an existing json file or create a new one if it does not exist.