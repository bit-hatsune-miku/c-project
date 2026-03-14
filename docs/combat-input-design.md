# Combat Input Design

This document captures the intended scoring math for the first two interactive combat abilities:

- **Cupcakke**: pre-attack `SPACE` mashing to increase outgoing damage
- **Lyoo boss**: timed `SPACE` presses to reduce incoming damage

These formulas are intentionally documented before full implementation so balancing and UI feedback can stay consistent.

## Shared model

Both mechanics map player input to a final ability multiplier:

- Cupcakke raises damage above the base value
- Lyoo reduces damage taken below the base value

The common runtime shape is:

1. Open an input window during the presentation
2. Record `SPACE` presses during that window
3. Convert the recorded input into a normalized score
4. Convert that score into the final damage multiplier
5. Show a result summary in the battle hint UI

## Cupcakke: spam window

### Intent

The player repeatedly presses `SPACE` during Cupcakke's startup animation, before her forward attack motion begins.

### Input window

The valid input window starts when the presentation begins and ends the moment Cupcakke transitions from wind-up into the dash/lunge portion of the attack.

Presses after the lunge begins do not affect the result.

### Damage formula

Let $p$ be the number of valid `SPACE` presses during the startup window.

The outgoing damage multiplier is:

$$
M_{cupcakke} = \min(1.0 + 0.04p,\ 1.5)
$$

### Interpretation

- Each valid press adds `+0.04` to the multiplier
- The cap is `1.5x` damage
- `12` or more valid presses reaches the cap because:

$$
1.0 + 0.04 \cdot 12 = 1.48
$$

and the next few presses clamp at `1.5`

### Examples

- `0` presses -> $M = 1.00$
- `5` presses -> $M = 1.20$
- `9` presses -> $M = 1.36$
- `13` presses -> $M = 1.50$ after clamping

### Suggested result text

- `Pressed SPACE 9x · Damage x1.36`
- `Pressed SPACE 13x · Damage x1.50`

## Lyoo boss: rhythm parry

### Intent

The player presses `SPACE` in sync with four incoming hit pulses during Lyoo's boss attack. Better timing reduces the damage taken.

### Planned pulse timing

The current design assumes four pulse spawn points in a presentation that lasts about `1.55s`:

$$
\{0.12,\ 0.36,\ 0.62,\ 0.86\} \times 1.55s
$$

Each pulse travels for about `0.52s` before impact.

That gives approximate ideal hit moments of:

$$
T_{impact} \approx \{0.64s,\ 0.88s,\ 1.14s,\ 1.38s\}
$$

These are design targets and may be adjusted if the presentation timing changes.

### Per-hit accuracy formula

For each pulse, let $\Delta t$ be the absolute difference between the player's press time and the ideal impact time.

Each hit uses a `±0.20s` timing window with linear falloff:

$$
A_i = \max\left(0, 1 - \frac{|\Delta t|}{0.20}\right)
$$

where $A_i$ is a normalized accuracy score in the range `[0, 1]`.

If the UI wants a percentage, then:

$$
A_{i,\%} = 100 \cdot A_i
$$

### Average accuracy

With four pulse checks, the overall accuracy is the mean of the individual scores:

$$
\bar{A} = \frac{1}{4}\sum_{i=1}^{4} A_i
$$

### Damage reduction formula

Let $\bar{A}$ be the average normalized accuracy.

The final incoming damage multiplier is:

$$
M_{lyoo} = 1.0 - 0.5\bar{A}
$$

### Interpretation

- Perfect average accuracy (`1.0`) gives:

$$
M_{lyoo} = 0.5
$$

which means damage taken is reduced by `50%`

- Zero average accuracy gives:

$$
M_{lyoo} = 1.0
$$

which means no reduction

### Examples

- Average accuracy `0.00` -> damage taken `x1.00`
- Average accuracy `0.50` -> damage taken `x0.75`
- Average accuracy `0.80` -> damage taken `x0.60`
- Average accuracy `1.00` -> damage taken `x0.50`

### Suggested result text

- `Accuracy 75% · Damage taken -37%`
- `Accuracy 100% · Damage taken -50%`

## Miku: 4-lane note tap game

### Intent

Before Miku's normal singing projectile animation starts, the player completes a short 4-note rhythm challenge.

### Controls

The lane bindings are fixed and shown as a hint when the presentation starts:

- Lane 1: `D`
- Lane 2: `F`
- Lane 3: `J`
- Lane 4: `K`

### Note pattern

- Exactly `4` notes per cast
- One note in each lane (unique columns)
- Lane order is randomized each cast
- Spawn times are staggered so notes fall at different intervals
- No hold notes; tap only

### Timing judgement

Each note has a target hit time and three judgement windows based on absolute timing error $\Delta t = |t_{press} - t_{hit}|$:

- Perfect: $\Delta t \le 0.055s$
- Good: $0.055s < \Delta t \le 0.110s$
- Ok: $0.110s < \Delta t \le 0.165s$
- Miss: $\Delta t > 0.165s$ (or no press)

Each judgement maps to normalized accuracy:

- Perfect -> `1.00`
- Good -> `0.66`
- Ok -> `0.33`
- Miss -> `0.00`

Average accuracy over the 4 notes:

$$
\bar{A}_{miku} = \frac{1}{4}\sum_{i=1}^{4} A_i
$$

### Damage formula

Miku's final outgoing damage multiplier is:

$$
M_{miku} = 1.0 + 1.2\bar{A}_{miku}
$$

This yields a range from `x1.00` (all misses) to `x2.20` (all perfect).

### Flow integration

1. Start Miku presentation
2. Run rhythm mini-game until all 4 notes are resolved
3. Lock in multiplier and result text
4. Play the existing singing projectile presentation
5. Apply multiplier to the outgoing damage

This guarantees the mini-game always completes before Miku's current visual attack sequence begins.

### Suggested result text

- `Accuracy 82% · Damage +98% (x1.98)`
- `Accuracy 100% · Damage +120% (x2.20)`

## Implementation notes

When this is implemented, the cleanest structure is:

1. The battle session records key press events each frame
2. The active presentation consumes those events during its own input window
3. The presentation computes its multiplier internally
4. The presentation exposes a short result string for the HUD hint banner

This keeps input logic close to the ability presentation while leaving the HUD responsible only for displaying the result.

## Balancing notes

These numbers are intentionally easy to tune:

- Cupcakke balance knobs:
  - per-press gain (`0.04`)
  - maximum multiplier (`1.5`)
  - input-window length
- Lyoo balance knobs:
  - pulse timings
  - travel time (`0.52s`)
  - timing tolerance (`0.20s`)
  - max reduction (`50%`)

If gameplay feel changes, update this document at the same time as the code so the formulas remain the single source of truth for combat-input tuning.
