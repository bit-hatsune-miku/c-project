# **ROUGH PLAN**

# **What we are building**

## **Project Identity**

We are developing Hatsune Miku: Our Underground BIT Idol, a data-driven Visual Novel engine with modular turn-based combat extensions and map exploration. The narrative follows Hatsune Miku, a BIT student who dreams of being a world famous idol but is actually a regular BIT student in real life. However, everything changes once she discovers the Underground of BIT where she participates in the BIT idol competition.

## **Technical Scope**

**Language:** *C++*

**Library:** SDL, as recommended by the teacher as it is the industry standard and we are “computer guys”. Unfortunately, none of us have experience using SDL, so the first week will be learning week.

**Build System:** CMake for cross-platform compatibility between Windows and Arch Linux

*Note: We would consider C++ as we are computer guys but the learning curve is already steep from SDL. The AI project may have forgiven late starts, but a technical project will not be as forgiving of late starts.*

## **Why this is feasible in 6 weeks**

We operate on a "VN-First, Combat-Second" risk mitigation strategy. The Visual Novel engine (title screen, scene system, branching dialogue, save/load) is the mandatory course requirement and constitutes our Week 1–2 baseline deliverable. The turn-based combat system is explicitly classified as an optional extension; we will only proceed if the VN engine stabilizes by Week 2\. If combat implementation risks the deadline, we will pivot to an "Auto-Battle" mode (stat checks without player input) or remove it entirely, ensuring a functional, complete narrative experience remains shippable.

Our asset pipeline relies on AI-generated backgrounds and curated open-source sprites, eliminating the bottleneck of original art creation. We have standardized on keyboard-only input (arrow keys \+ Z/X/C), avoiding the complexity of mouse handling or real-time physics calculations.

## **Scope Boundaries**

To prevent feature creep and ensure a shippable product, we have locked the following restrictions:

* **No real-time platformer physics:** Exploration will be menu-based or static scene transitions; we will not implement gravity, collision detection, or platforming mechanics.  
* **No procedural map generation:** All scenes will be hand-authored JSON files with static backgrounds; we will not implement tilemap systems or random dungeon generation.  
* **No mouse input:** All interactions (menus, combat, dialogue choices) will use keyboard exclusively to standardize the input abstraction layer.

## **Success Criteria**

The project succeeds if we deliver: (1) A playable Visual Novel from title screen to credits with at least three endings; (2) A Windows executable (.exe) that runs without requiring Visual Studio installation; (3) Complete documentation of AI usage and weekly meeting logs. Combat mechanics are a stretch goal, not a requirement for success.

# **Team Strength Analysis & Role Assignment**

**CEO: Zerkane Lyes Messaoud Aprian (哲川)** **Strengths:**

- Former game developer on Roblox (Lua) and C with graphical interface experience  
- Prior leadership experience with CaiNiao; excellent individual contributor and tactical coordinator  
- Advanced time management (early bird, 6 AM routine), self-hosts Nextcloud for team organization  
- Avid writer for documentation, scripts, and project reports  
- Drawing tablet proficiency for emergency asset touch-ups

**Weaknesses:**

- Jack of all trades, master of none  
- Overly optimistic scoping tendencies; tendency to cut corners under pressure  
- Self-identified poor strategic leader (compensated by Tech Lead oversight)  
- Hard time starting projects (hesitation/rethinking phase)

**Role in the Project:**

- Core Gameplay Programming (Battle System implementation)  
- Asset Design (primary UI, sprites, and visual elements)  
- Writing Oversee (script assistance, dialogue polishing, narrative structure)  
- Voice Acting coordination  
- Backup Role: Programmer / Designer

---

**Tech Lead: Jaden (楊枝順)** **Strengths:**

- Elite programming capability ("insanely good" at *C++*/systems development); Arch Linux power user  
- Strategic vision for technical architecture and long-term maintainability  
- Workflow and build system expertise (CMake, CI/CD, cross-platform compilation)  
- AI-augmented development (Claude/Codex subscription holder) for rapid prototyping  
- Clear technical communication; "I use Arch BTW" level of systems knowledge

**Weaknesses:**

- Risk of over-engineering solutions given high skill level  
- Designer nightmare (strong implementation skills, weaker aesthetic design intuition)  
- May gold-plate systems without external veto oversight

**Role in the Project:**

- Technical Gatekeeper: Final authority on API design, SDL patterns, code review, and merge approval  
- Build Master: CMake configuration, cross-platform compilation (Windows/Arch Linux), Valgrind memory protocols  
- VN Engine Core Development (Week 1–2 critical path)  
- Emergency Override: Authorized to reject CEO scope creep unilaterally to preserve velocity  
- Backup Role: Core Gameplay Programmer

---

**CIO: Timothy (陈帝成)** **Strengths:**

- Strong programming foundation (*C++*, systems architecture) with AI-augmented coding (Claude/Codex subscription)  
- Infrastructure and DevOps management (VPS, cloud storage, backup systems)  
- Reliable execution and deadline adherence; detail-oriented progress monitoring  
- Technical documentation and communication bridge between CEO and implementers  
- Fast learner with strong adaptability to new tools

**Weaknesses:**

- Horrible at public speaking (stutters/freezes with large audiences, actively improving)  
- Limited UI/UX design creativity  
- Reluctant to take risks (prefers safe, proven approaches)

**Role in the Project:**

- Submission Authority: Owns all Lexue uploads, GitHub repository maintenance, final ZIP packaging  
- Progress Tracking: Maintains GitHub Projects/Trello boards; enforces mini-deadlines and milestone checkpoints  
- Documentation Lead: Ensures AI usage logs, weekly meeting minutes, and build instructions are current  
- Technical Support: Pair-programming backup for non-coders (JSON scripting, tool troubleshooting)  
- Backup Role: QA/Tester

---

**Architect & Programmer: Anmol Sah (魏明轩)** **Strengths:**

- Technical experience: C, Java, Python, XML, HTML; built working Android apps and C-based applications  
- Project-oriented & product-focused: checks broader vision, minimizes friction, rapidly converts ideas to working code  
- Practical hardware experience: hands-on repair (smartphones, laptops), test gear (oscilloscope, multimeter)  
- Clear technical communicator; prior leadership experience (high school magazine coordination)  
- Growth mindset: actively studying best practices and larger open-source projects

**Weaknesses:**

- UI/UX design is complicated and hard to implement perfectly in real life  
- Limited experience with large-scale codebases (compensated by active study)  
- May need extra time for advanced AI concept research

**Role in the Project:**

- Technical Accuracy: Ensures algorithm and Connectionist AI explanations are factually correct  
- Research Lead: Gathers sources/examples, summarizes into digestible content for the team  
- Design Oversight: Works with Canva/animation tools to ensure visual elements are polished and connected  
- Team Support: Helps members understand technical details and align with the big picture  
- Backup Role: Technical reviewer and design implementation

---

**Designer: Fabian Russel Suryadi (楊享禄)** **Strengths:**

- Adaptability: adjusts quickly to new situations; stays calm when things go wrong  
- Problem-solver: breaks down challenges and finds effective solutions without giving up  
- Time management: balances multiple tasks while maintaining quality to meet deadlines  
- Attention to detail: catches errors through thorough analysis; commits to work schedules  
- Open-mindedness: considers different personalities and ideas in group settings

**Weaknesses:**

- Overthinker: spends excessive time analyzing single points, complicating simple problems  
- Horrible at public speaking: incredibly shy with large audiences (stutters/freezes)  
- Reluctant to take risks: fears making big mistakes that could disappoint teammates

**Role in the Project:**

- Design: Creates slides, diagrams, and layouts; ensures brand consistency (color palettes, typography)  
- Bug Detection: Carefully reviews programs to find and document errors before submission  
- Quality Assurance: Tests product from end-user perspective for usability and smooth experience  
- Research: Compiles supplementary resources and provides insights on Big Data and Connectionist AI  
- Backup Role: Documentation and general support

---

**Tester & Writer: Revell Ephraim Hosea Tius (彭利和)** **Strengths:**

- Technical foundation: Python, JavaScript, HTML/CSS; familiar with algorithms and data structures  
- Problem-solver: strong logical/analytical thinking; breaks down complex technical challenges  
- Research skills: efficiently finds and filters high-quality information from reliable sources  
- Curiosity & growth mindset: highly interested in AI/algorithms, shares knowledge with teammates  
- Hard-working and reliable: maintains high work ethic; committed to upholding team standards  
- Supportive teammate: experienced in collaborative environments, gives assistance when needed

**Weaknesses:**

- Perfectionist: spends too much time fine-tuning details, potentially hindering team progress  
- Designer nightmare: not experienced in visual design (though continuously trying to improve)  
- Shallow knowledge pool: still learning CS fundamentals and advanced AI (C newbie); may need extra research time for complex concepts

**Role in the Project:**

- Testing: Reviews materials, videos, quizzes, and PPTs to ensure no errors or misinformation  
- Writing: Drafts and polishes technical explanations; assists with scriptwriting and documentation  
- Technical Review: Double-checks drafts for consistency and clarity before final submission  
- Team Support: Helps other members understand technical details and aligns everyone with project goals  
- Backup Role: Research assistance and quality verification

# **Execution Strategy**

## **Task Decomposition**

- **Week 1 Validation:** Each member must submit screenshot of compiled SDL demo → GitHub Issue checklist  
- **Module Ownership:**  
  - VN Engine: \[Name\] (Week 1-2)  
  - Combat System: CEO (Week 3\) with \[Name\] as backup if unstable  
  - JSON Scripting: Non-coders handle dialogue files (no C required)

## **Progress Tracking**

- **Daily:** Async updates in WeChat/Discord group  
- **Weekly:** Sunday "Kill Decision" meetings (30 min max) with pass/fail criteria  
- **Repository:** GitHub Projects board with "Blocked" column for immediate escalation

## **Integration Management**

- **Build Master Authority:** Lyes has final say on CMake/Make decisions; no local workarounds allowed  
- **API Freeze Protocol:** Week 3 Character Kit interface locked; post-freeze changes require Tech Lead \+ CEO unanimous approval  
- **Static Analysis:** All PRs require Valgrind clean bill before merge (enforced via CI if possible, manually if not)

## **Conflict & Delay Handling**

- **48-Hour Rule:** If milestone not progressing, activate backup role immediately (original owner shifts to docs/testing)  
- **Exam Buffer:** Week 3-4 academic overlap managed by front-loading (Week 2 completion of VN engine core)  
- **Scope Creep Prevention:** Tech Lead authorized to reject post-Week 4 feature requests; "Kill Switch" invoked unilaterally if velocity dies 48h before deadline

## **Communication Protocol**

- **Emergency Powers:** Flat hierarchy by default, but CEO invokes unilateral decisions only when "velocity dies"  
- **Documentation Standard:** Weekly meeting minutes stored in /docs/minutes/ with contribution logging

# **Rough 6-Week Timeline**

**Week 1: SDL Bootcamp & Toolchain Validation**  
*Goal:* Validate that the development environment works identically across all team hardware.

- CMake build system configured for cross-platform compilation (Windows/Arch Linux).  
- Every team member compiles and runs an SDL2 application with PNG (SDL\_image) and TTF (SDL\_ttf) support.  
- cJSON library integrated and tested for scene data loading.  
- AI asset generation pipeline established for background art.  
- Write most of the chapters.

**Week 2: VN Vertical Slice & Kill Decision**  
*Goal:* Deliver a playable narrative prototype proving the core loop functions.

- Complete Visual Novel engine: state machine (Menu → Dialogue → Battle placeholder), text rendering with typewriter effect, and scene transition system.  
- JSON-driven dialogue system loading external scripts (not hardcoded).  
- First playable prologue: Miku wakes up, interacts with Cupcakke, presents one meaningful choice, and transitions to battle placeholder.  
- **Critical Milestone (Kill Decision):** If the VN engine is unstable by Sunday, combat system is cut entirely; project pivots to pure visual novel with stat-check auto-battles.

**Week 3: Combat MVP & API Lock**  
*Goal:* Prove the turn-based combat loop works with one complete encounter.

- Turn queue system implemented  
- `CharacterKit` API standardized (function pointer interface for skills/ultimates) and documented.  
- One functional battle: player-controlled character vs. test enemy with win/lose conditions.  
- Keyboard-only input system locked (arrow keys \+ action buttons).

*Deliverable:* Combat prototype playable; Character Kit interface frozen (no API changes permitted after this week).

**Week 4: Content Expansion**  
*Goal:* Scale from prototype to full game content.

- Implement a full character roster (minimum 4 playable characters) using the standardized Week 3 template.  
- Complete narrative content: chapters 1–2 fully scripted in JSON and traversable from title screen to credits.  
- Inventory system implemented (if Week 3 delivered ahead of schedule; else deferred to Week 5 polish).

**Week 5: Polish, Debug & Feature Freeze**  
*Goal:* Stabilize and optimize; no new features.

- **Feature Freeze enforced:** All new mechanics, characters, or input modes prohibited.  
- Memory leak detection and remediation (Valgrind/SDL debug tools).  
- Bug hunt: resolve NULL pointer crashes, texture cleanup failures, and JSON parsing edge cases.  
- Gameplay video recorded as presentation backup.  
- Performance optimization: consistent 60FPS target achieved.

**Week 6: Release Packaging & Submission**  
*Goal:* Deliver shippable product and documentation.

- Windows executable (.exe) created with all DLLs bundled (no Visual Studio installation required on target machine).  
- Final documentation package: AI usage log with full transparency, weekly meeting minutes, individual contribution scores (differentiated by minimum 5% with justification), and commented source code.  
- Presentation materials: slides, gameplay video backup, and live demo preparation.

*Deliverable:* Submitted ZIP file containing executable, source code, and documentation; presentation delivered.

# **Risk Awareness & Scope Control**

**Overall Team Risk Assessment**  
We have a critical asymmetry: **extreme technical strength, creative vulnerability.** We field three Arch Linux power users, four strong C programmers with AI-assisted development tools (Claude/Copilot/Codex), and solid algorithmic foundations from high C exam scores. However, we lack dedicated art specialists—only one member has digital art experience (tablet proficiency, but non-professional grade)—and we miss the diversity factors (no female or Chinese members) that often provide alternative perspectives on narrative and user experience. This creates a risk of "perfect code, ugly product" or neglecting documentation/presentation polish in favor of systems programming.

**Main Technical Risks**

| Risk | Probability | Impact | Mitigation Strategy |
| :---- | :---- | :---- | :---- |
| **SDL2 Learning Curve** | High | Critical | **Week 1 Sacred Bootcamp:** No feature coding until all six laptops compile identical SDL+TTF+Image demo. Build Master (CIO) dedicated solely to toolchain resolution for the first 14 days. |
| **Memory Management in C** | Medium | High | **stb\_ds.h Library:** Use single-header dynamic arrays (no manual realloc hell). **Safe SDL Wrappers:** Tech Lead creates `safe_sdl.h` macros for guaranteed texture cleanup. **Valgrind Gate:** Week 5 mandatory memory leak check; zero-tolerance for unfreed surfaces. |
| **Save/Load System Complexity** | Medium | Medium | **JSON-Only Serialization:** cJSON library for human-readable save files. No binary serialization. **Delta Saves:** Store only player choices and stats, not full engine state. |
| **Integration Hell (Character Kits)** | High | Medium | **API Freeze Week 3:** Character Kit struct (function pointers) locked after Week 3; no new input types accepted. **Template Enforcement:** All characters inherit identical interface; non-compliant code rejected in review. |
| **Asset Acquisition Failure** | Medium | Medium | **AI Pipeline \+ Programmer Art Fallback:** Stable Diffusion for backgrounds; if character sprites fail, use colored rectangles with nametags (game remains playable). **No Original Drawing Mandate:** Explicitly avoiding hand-drawn animation to prevent art bottlenecks. |

**Team Risks**

- **Skill Asymmetry:** Four strong programmers vs. two beginner programmers (Designer, Tester). Risk of "two-tier" development where non-coders are sidelined.  
  *Mitigation:* **"Everyone Codes a Character" Rule** (Week 4). Non-programmers implement simple kits using provided templates; pair programming with CEO ensures contribution equity. Tester owns JSON dialogue scripting (no C required) and AI logging.  
- **Creative Deficit:** Lack of professional art direction risks generic cyberpunk aesthetic.  
  *Mitigation:* **AI-Assisted Asset Pipeline** (stable diffusion for backgrounds, touched up via tablet). **Narrative Voice:** "Ghost Stories Abridged" humor style compensates for visual limitations with strong writing (Tester/Writer role). **Voice Acting:** Team performs character voices for presentation to add personality without art cost.

**Time Risks**

- **Academic Overlap:** Week 3-4 coincides with midterm examinations for some members.  
  *Mitigation:* **Front-Loading:** Critical path (VN engine) completed Week 2 before exam pressure. **Buffer Days:** Week 5 includes 2-day "emergency reserve" for exam-delayed integration work.  
- **Toolchain Delay:** If CMake/SDL setup exceeds Week 1, cascade delay destroys timeline.  
  *Mitigation:* **Makefile Fallback:** If CMake proves intractable, switch to simple `gcc` Makefiles Week 2 to maintain momentum. **Build Master Authority:** CIO has carte blanche to simplify build requirements (e.g., static linking only) to meet deadlines.

**Kill Switch Protocol**

If Week 2 VN engine is unstable: **Cut combat entirely.** Pivot to pure Visual Novel with stat-check auto-battles (no player input).  
If Week 3 combat is buggy: **Disable Character Kits.** Reduce to single-character story (Miku only) with narrative choices driving outcomes.  
If Week 4 integration fails: **Disable Inventory System.** Focus solely on VN \+ basic combat.

**Risk Philosophy**  
We acknowledge our "master of none" creative core and homogeneous team composition. Rather than hide these limitations, we have engineered the project around them: AI-assisted assets compensate for art weakness; strict JSON data structures allow non-programmers to contribute content; and the "Kill Switch" ensures we ship a complete narrative experience even if technical ambitions exceed 6-week reality.

# **Organization & Professional Presentation**

**Document Structure**  
The plan follows a hierarchical engineering format: Big Picture → Team Analysis → Execution → Timeline → Risk → Leadership. Each section uses numbered headers, bullet points for technical specifications, and tables for comparative data (role assignments, risk matrices). This mirrors IEEE software proposal standards, progressing from concept to implementation to contingency planning.

**Writing Standards**

- **Precision:** Measurable criteria only ("Compiles warning-free on Windows and Arch," "60 FPS target," "JSON schema frozen Week 3") replace vague qualifiers ("good performance").  
- **Active Voice:** Responsibilities assigned with direct verbs ("The Build Master configures CMake," "The Tech Lead reviews pull requests," "The CEO enforces scope lock").  
- **Conciseness:** No filler phrases ("due to the fact that," "in order to"). Technical terms (SDL, cJSON, Valgrind) are used without patronizing explanation.

**Visual Communication**

- **Timeline Diagram:** A Gantt-style horizontal bar illustrates phase dependencies—Weeks 1–2 (Sequential/Waterfall) → Weeks 3–4 (Parallel/Sprint) → Weeks 5–6 (Lock/Polish). Critical milestones (Toolchain Validation, Kill Decision, Feature Freeze) marked as diamond nodes.  
- **Risk Matrix:** Tabular probability/impact grid in Section 5 enables at-a-glance vulnerability assessment.  
- **Repository Structure:** Tree-notation file hierarchy clarifies separation between engine source (`/src/vn_engine/`, `/src/combat/`), data assets (`/assets/scenes/`), and documentation (`/docs/`).

**Professional Tone**  
The document maintains technical proposal register:

- **No Hedging:** Absence of "we will try our best" or "hopefully." Commitments are binary ("We will deliver") or conditionally explicit ("If Week 2 milestone is missed, combat is cut").  
- **Evidence-Based:** Assertions cite specific tools (stb\_ds.h for dynamic arrays, cJSON for serialization) rather than vague capability claims.  
- **Academic Compliance:** Explicit integration of course policies (AI transparency logging, weekly contribution scoring with 5% minimum differentiation, C99 standard adherence).  
- 

**Deliverable Specifications**  
Final submission includes:

1. **PDF Document:** This structured plan (concise, 11pt sans-serif, clear headings).  
2. **Source Archive:** Repository with `README.md` containing identical build instructions to those specified in Section 3\.  
3. **Video Backup:** Gameplay demonstration (MP4, 1080p) as insurance against live demo failure.  
4. **Signature Page:** Attestation of contribution percentages (differentiated ≥5%) signed by all members.

# **Leadership Mindset Reflection**

**The Leader I Want to Be**  
A tactical contributor-coordinator, not a Slack administrator. I will write 30-40% of the core code while ensuring others are unblocked. Leadership is labor, not authority. If someone is debugging at 2 AM, I am beside them. I run a flat hierarchy with emergency powers: decisions are consensus by default, but if velocity dies 48 hours before a deadline, I invoke the Kill Switch unilaterally to save the project. I also want to be more of a strategic leader this semester, which is what I crucially lacked last semester.

**Motivating the Team**  
I motivate through shared ownership. The "everyone codes their own character" ensures everyone has equity in the final product. We are building a sandbox, not my game. We will all take part in the writing process and the voice acting, and hope to have tons of fun despite the technical difficulties.

**Handling Underperformance**  
I distinguish between learning mode (Weeks 1–2) and **blocking mode** (Weeks 3+).

- **Learning:** If someone cannot compile SDL, I pair-program with them until it works—no judgment, we are all C newbies.  
- **Blocking:** If a missed milestone holds up the team (e.g., Character Kit API delayed), I activate backup role protocol and the task moves to the backup (usually me or the Tech Lead), and the original owner shifts to documentation/testing where they can still contribute without killing velocity.

I want to have the uncomfortable conversation early. If someone struggles, I ask immediately: "Do you need help, or do you need a different task?" Underperformance is usually mismatch, not laziness.

**Fairness and Accountability**  
I despise group projects where everyone gets 100% while one person carries the team. Our accountability is radically transparent:

- Git commits are the objective record, weighted by meaningful code, JSON authorship, and documented debugging hours, not "effort."  
- The 5% differentiation rule is sacred**.** If someone is coasted, their score reflects it, friendship regardless.

**Ownership of Outcome**  
If the game crashes during presentation, that is my fault—poor scoping, missed Feature Freeze, or overlooked memory leak. I will not blame SDL or a teammate. The repository is under my GitHub; the final ZIP has my name first.

I acknowledge my own flaws: I am overly optimistic and I cut corners when stressed. To check this, I have given the Tech Lead veto power over my bad ideas. If I suggest adding features after Week 4, he is authorized to tell me to shut up, and I will listen.

**Final Commitment**  
I am not a perfect leader, I am a 20-year-old Roblox with a tendency to over-scope, but I bring authenticity, technical competence, and a refusal to leave anyone behind. This project lives or dies by my judgment calls, and I am ready to wear that weight.

# **Self-Evaluation**

| Dimension | Self-Rating | Justification |
| :---- | :---- | :---- |
| Big Picture Clarity | ★★★★★ Outstanding | Binary scope boundaries ("No mouse input"), SDL justification, Kill Switch criteria defined |
| Role Assignment | ★★★★☆ Good/Excellent | Complete roster with backups; slight asymmetry remains (heavy technical bias) |
| Execution Plan | ★★★★☆ Good | "Jaden Veto" and API Freeze Week 3 show process control; new section may need field-testing |
| Timeline Feasibility | ★★★★★ Excellent | Front-loaded Week 1–2 critical path accommodates midterms; 6 programmers makes combat MVP achievable |
| Risk Awareness | ★★★★★ Outstanding | Tabular risk matrix with specific mitigations (stb\_ds.h, Valgrind Gate, AI art fallback) |
| Professionalism | ★★★★★ Excellent | IEEE-standard structure, active voice, no hedging, evidence-based tool selection |

