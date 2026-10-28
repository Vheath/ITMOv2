# Feature Specification: Pause Menu with Audio Volume Controls

**Feature Branch**: `feat/pause-audio-menu`

**Created**: 2026-10-28

**Status**: Draft

**Input**: User description: "Project have audio effects. I want you to add UI menu, which would be opened on ESC press. This menu would have controls of audio, so i can change volume of audio effects. This menu should also have "Continue" button, and "Exit" which will close the game."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Pause and Resume the Flight (Priority: P1)

While flying, the player presses ESC. The game freezes instantly: the bird
holds its position and attitude, the world stops scrolling, and a pause menu
appears on screen. From the menu the player chooses "Continue" (or presses
ESC again) and the flight resumes exactly where it stopped — same position,
speed, score, and run state.

**Why this priority**: Pausing is the MVP backbone of this feature. Today ESC
kills the game instantly, destroying the run; a safe pause/resume is the
single most valuable behaviour and every other menu function depends on the
paused state it introduces.

**Independent Test**: Can be fully tested by starting a flight, pressing ESC,
verifying the world is frozen and inputs are ignored, then resuming and
verifying the run continues without lost progress. Delivers value on its own
even before volume or exit controls exist.

**Acceptance Scenarios**:

1. **Given** the player is in active flight, **When** ESC is pressed, **Then** the simulation stops updating, gameplay input is ignored, and the pause menu is shown.
2. **Given** the pause menu is open, **When** the player activates "Continue", **Then** the menu closes and the flight resumes with position, speed, score, and run state unchanged from the moment of pausing.
3. **Given** the pause menu is open, **When** ESC is pressed again, **Then** the menu closes and the flight resumes (ESC toggles).
4. **Given** the pause menu has been open for any length of time, **When** the player resumes, **Then** the simulation does NOT fast-forward through the paused interval (no "time jump").

---

### User Story 2 - Change Sound Effect Volume (Priority: P2)

With the menu open, the player moves a volume control up and down. The
current volume level is shown as a percentage. The change applies immediately
to every sound effect in the game — the wind rush loop, wing flap whooshes,
ring chimes, and the crash thud — from fully silent at 0% to full loudness at
100%, without restarting the game.

**Why this priority**: The user explicitly asked for audio volume controls;
it is the second half of the feature's purpose. It requires the pause menu
from Story 1 to exist as its host, but is independently valuable the moment
the menu renders.

**Independent Test**: Can be fully tested by opening the menu, setting volume
to 0%, resuming, and hearing total silence during flight; then setting 100%
and hearing all four effects at full loudness. Immediate audibility of the
change is the testable outcome.

**Acceptance Scenarios**:

1. **Given** the pause menu is open, **When** the player lowers the volume control, **Then** all sound effects play quieter and the displayed percentage decreases.
2. **Given** the volume is set to 0%, **When** any sound effect would trigger (wind, flap, ring, crash), **Then** nothing is audible.
3. **Given** the volume is set to 100%, **When** any sound effect triggers, **Then** it plays at the game's normal full loudness.
4. **Given** the player changes the volume, **When** the change is made, **Then** it takes effect immediately for already-playing and newly-triggered sounds, with no restart of the game or the audio.

---

### User Story 3 - Exit the Game from the Menu (Priority: P3)

With the menu open, the player selects "Exit". The game shuts down cleanly —
window closes, audio stops, and the process ends — replacing the old
"ESC quits instantly" behaviour with a deliberate, confirmed exit path.

**Why this priority**: Requested explicitly, but it is the smallest slice and
depends on the menu (Story 1) existing. Lowest risk, lowest effort, so it is
last in priority order despite being mandatory for the feature.

**Independent Test**: Can be fully tested by opening the menu, choosing
"Exit", and confirming the application terminates cleanly without needing the
window's close button.

**Acceptance Scenarios**:

1. **Given** the pause menu is open, **When** the player activates "Exit", **Then** the game closes cleanly (audio stops, window closes, process ends).
2. **Given** the pause menu is open, **When** the window's close button is used, **Then** the game still closes cleanly (existing behaviour is preserved while the menu is open).

---

### Edge Cases

- What happens when ESC is pressed on the game-over (crash) screen? The menu must open there too, so the player can still lower volume or exit; the run state must not be reset by opening or closing the menu, and "Continue" returns to the game-over screen (not a restart).
- What happens when ESC is pressed rapidly, or while the menu is animating/appearing? The menu must toggle reliably — each press flips the state exactly once, with no lock-up, stuck state, or double-apply.
- What happens when the volume control is moved to its extremes? 0% must be true silence (not a faint residual), 100% must be the game's normal loudness (no clipping or distortion beyond the game's existing character), and values must never wrap around or go out of range.
- What happens if the menu is opened at the exact moment a sound triggers (ring chime or crash)? The pause must not corrupt audio state; the volume setting must apply to subsequent playback, and sustained flight sounds must stop or fade while paused rather than looping forever behind the menu.
- What happens when the window is closed while the menu is open, or Exit is chosen mid-sound? The game must still shut down cleanly with no hang, stuck process, or audio device error.
- What happens when the player holds a flight input key (e.g. W) while opening the menu? The key must not leak into the flight after resume beyond what is still physically held when the game unpauses.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST open the pause menu and pause the simulation when ESC is pressed during a run (in flight or on the game-over screen).
- **FR-002**: While the pause menu is open, the system MUST NOT advance the flight simulation and MUST ignore gameplay input (steering, boost, restart).
- **FR-003**: The pause menu MUST provide a "Continue" action that closes the menu and resumes the run with the player's position, speed, score, and run state preserved exactly as at the moment of pausing.
- **FR-004**: Pressing ESC while the pause menu is open MUST act the same as "Continue" (toggle behaviour).
- **FR-005**: The pause menu MUST provide a volume control that adjusts the loudness of ALL sound effects in the game — wind rush, wing flap whoosh, ring chime, and crash thud — within a range from silent (0%) to full loudness (100%).
- **FR-006**: Volume changes MUST take effect immediately, without restarting the game or the current run.
- **FR-007**: The pause menu MUST display the current volume level as a percentage at all times while the menu is open.
- **FR-008**: The pause menu MUST provide an "Exit" action that closes the game cleanly.
- **FR-009**: While the pause menu is open, continuous flight sounds (the wind rush loop) MUST stop or fade out so that pausing is audibly silent, and sound effect state MUST remain valid for playback after resume.
- **FR-010**: While the pause menu is open, the system MUST still respond to the window close request by shutting down cleanly.
- **FR-011**: The pause menu MUST be operable entirely with the keyboard (item selection, volume adjustment, and action activation), consistent with the game's keyboard-driven controls.

### Key Entities *(include if feature involves data)*

- **Pause Menu**: An overlay screen with a currently highlighted item, toggled by ESC, that owns the session's paused/active state; it hosts the volume control and the "Continue" and "Exit" actions.
- **Volume Setting**: The current loudness level applied to all sound effects, expressed as a value between 0% (silent) and 100% (full); it is readable (for display) and adjustable (via the menu control) at any time the menu is open.
- **Menu Actions**: The two selectable commands exposed by the pause menu — "Continue" (resume the run) and "Exit" (close the game) — plus the volume adjustment interaction.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A player can pause and resume a flight in a single round trip within 2 seconds, and in 10 consecutive pause/resume cycles no run state (position, speed, score, run status) is lost or altered.
- **SC-002**: Adjusting the volume control produces an audible change in every one of the four sound effects; a player can distinguish silent (0%), mid-range, and full (100%) loudness for each effect without restarting.
- **SC-003**: A player can exit the game entirely from the pause menu in at most two interactions (open menu, activate "Exit") without using the window close button, and the game terminates without hanging.
- **SC-004**: Opening or closing the pause menu causes no perceptible stutter or freeze of the interface — the menu appears and disappears within the same interactive moment as the key press.
- **SC-005**: Every menu function (navigate, adjust volume, continue, exit) is reachable using the keyboard alone, so a player never needs the mouse to operate the menu.

## Assumptions

- A single master volume control governs all sound effects together (one slider), not per-effect sliders; the game's four existing effects share one loudness setting.
- "Audio effects" means the four sounds the game already produces (wind rush, wing flap whoosh, ring chime, crash thud); no new audio content (music, new effects) is added by this feature.
- The menu is navigated with the keyboard (arrow keys to select/adjust, Enter to activate), matching the game's existing keyboard-only input; mouse support is out of scope for v1.
- The volume setting applies for the current play session; persisting it across game restarts (e.g., a settings file) is out of scope for v1 and may be a follow-up feature.
- ESC previously quit the game immediately; that behaviour is intentionally replaced — quitting is now done via the menu's "Exit" or the window close button.
- The pause menu is available during flight and on the game-over screen; "Continue" from the game-over screen returns to the game-over screen without resetting the run.
- The menu's visual style follows the game's existing retro flat-shaded HUD look; visual polish beyond legibility is not a goal of this feature.
- No saving or resuming across process restarts is expected — "pause" means within a running session only.
