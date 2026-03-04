# Swaylock UI Enhancements

## Phase 1: Planning and Research
- [ ] Read codebase to understand the structure (`main.c`, `render.c`, `cairo.c`, `include/`).
- [ ] Draft an implementation plan (`implementation_plan.md`) covering configuration, effects pipeline, widgets, and animations.
- [ ] Get user approval on the plan.

## Phase 2: Configuration and Options (CLI Parsing)
- [ ] Add new struct members in `struct swaylock_args` for the new options.
- [ ] Modify `main.c` to parse `--screenshots`, `--effect-blur`, `--effect-pixelate`, `--effect-fade-in`.
- [ ] Add parsing for `--clock`, `--timestr`, `--datestr`, `--battery`, `--show-user`.
- [ ] Add parsing for `--indicator-anim`, `--indicator-anim-duration`, `--indicator-anim-intensity`.
- [ ] Validate new options and print clear error messages where necessary.
- [ ] Make small reviewable commit for options/config.

## Phase 3: Effects Pipeline and Caching
- [ ] Reconfigure background surface capturing/creation.
- [ ] Implement `screenshot` reading per output.
- [ ] Implement fast gaussian-approx box blur (or stack blur) algorithm.
- [ ] Implement pixelate algorithm.
- [ ] Cache the processed buffers after rendering ONCE per output.
- [ ] Implement alpha fade-in on lock with frame callbacks (damage proper regions).
- [ ] Make a commit for effects pipeline and caching.

## Phase 4: Widgets and Timers
- [ ] Add timer to loop for 1Hz updates if clock or battery is enabled.
- [ ] Read `/sys/class/power_supply` for battery percentage and charging state.
- [ ] Retrieve username with `getuid()` and `getpwuid()`.
- [ ] Render widgets onto `cairo` context in `render.c` with HiDPI-consistent font sizing.
- [ ] Support anchor+offset positioning.
- [ ] Make a commit for widgets and timers.

## Phase 5: Indicator Animation
- [ ] Implement spring/damped sine pulse on keypress.
- [ ] Implement smooth spin while typing/verifying.
- [ ] Request frame callbacks and only damage indicator region during animations.
- [ ] Make a commit for indicator animation.

## Phase 6: Documentation and Cleanup
- [ ] Update `swaylock.1.scd`.
- [ ] Update shell completions (`bash`, `zsh`, `fish`).
- [ ] Produce patchset and PR description.
