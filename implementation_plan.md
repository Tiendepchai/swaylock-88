# Swaylock UI Enhancements

## Goal Description
Implement UI enhancements in a fork of swaylock (v1.8.4), including screenshot capture for background, effects (blur, pixelate, fade-in), widgets (clock, battery, username), and indicator animations (pulse, spin).

## Proposed Changes

### Configuration / CLI
#### [MODIFY] main.c
- Add `wlr-screencopy-unstable-v1` global variable to `swaylock_state`.
- Add CLI parsing for `--screenshots`, `--effect-blur`, `--effect-pixelate`, `--effect-fade-in`.
- Add CLI parsing for `--clock`, `--timestr`, `--datestr`, `--battery`, `--show-user`.
- Add CLI parsing for layout options (e.g., `--clock-pos x:y`, `--battery-pos x:y`, `--user-pos x:y`, defaulting to center/below).
- Add CLI parsing for `--indicator-anim`, `--indicator-anim-duration`, `--indicator-anim-intensity`.
- Include validations/error messages.

### Effects Pipeline & Background
#### [NEW] effects.c / effects.h
- Implement fast box blur (3-pass) algorithm modifying raw image surface data (via `cairo_image_surface_get_data`).
- Implement pixelate algorithm (downscale/upscale with `CAIRO_FILTER_NEAREST`).
#### [MODIFY] main.c
- Bind `zwlr_screencopy_manager_v1` from the registry.
- When `--screenshots` is enabled, loop over outputs before locking (or handle screencopy in `swaylock_surface` creation) and capture the screen into a Cairo image surface.
#### [MODIFY] meson.build
- Add `wlr-screencopy-unstable-v1.xml` to `client_protocols` list. Fetch it from curl or a local file created in the project.

### Fade-In Effect & Rendering
#### [MODIFY] render.c
- Implement `--effect-fade-in`. Record `fade_start_time` in `swaylock_surface`.
- Use `cairo_paint_with_alpha` using computed alpha value `clamp((current_time - fade_start_time) / fade_in_ms, 0.0, 1.0)`.
- Request continuous `wl_surface_frame` callbacks until alpha == 1.0 to ensure smooth animation.

### Widgets (Clock, Battery, Username)
#### [NEW] widgets.c / widgets.h
- Handle battery reading from `/sys/class/power_supply` (find the first directory where `type` is `Battery`).
- Format clock using `strftime`.
- Get username via `getuid()` and `getpwuid()`.
#### [MODIFY] loop.c / main.c
- If clock/battery is shown, add a 1Hz `timerfd` to the event loop, causing `damage_state()` to redraw the widgets.
#### [MODIFY] render.c
- Render widgets to the Cairo surface with HiDPI sizing (using `surface->scale`).
- Use parsed anchor/offset configuration for positioning widgets within the output bounds avoiding the indicator.

### Indicator Animation
#### [MODIFY] render.c
- Implement `pulse` animation: dynamically modify the indicator scale or radius (e.g., spring/damped sine curve based on time since last input action). Track input timestamp in `swaylock_state`.
- Implement `spin` animation: smoothly rotate the highlight arc when typing or verifying based on monotonic time difference.
- Trigger frame callbacks whenever animations are active to draw at display refresh rate.

### Documentation & Build
#### [MODIFY] swaylock.1.scd
- Document all new CLI flags.
#### [MODIFY] completions/bash/swaylock, completions/zsh/_swaylock, completions/fish/swaylock.fish
- Add new options.

## Verification Plan
### Automated Tests
- Run `meson build && ninja -C build` to ensure no warnings or compilation errors.
- Ensure the newly added `wget`/`curl` for `wlr-screencopy` is downloaded correctly during `meson` bootstrap, or place it locally.
### Manual Verification
1. Run `swaylock -d --screenshots --effect-blur 7 --effect-pixelate 10` checking that background corresponds to the screen, blurred and pixelated.
2. Run `swaylock --effect-fade-in 1000` checking the background smoothly fades in.
3. Run `swaylock --clock --battery --show-user` checking that widgets render correctly, scale with HiDPI correctly, and the clock ticks 1x per second. Provide bad battery paths to verify fallback.
4. Run `swaylock --indicator-anim pulse+spin` typing passwords to check that the rings spin smoothly and pulse correctly on press.
5. Provide multi-output tests and wrong password combinations.
