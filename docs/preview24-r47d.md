# Local r47d: fade only the zoning confirmation text

The current picture and loading phrase are retained when the new zone becomes ready. Only “Click to continue...” fades in, smoothly over 900 ms. Clicking or pressing a fresh key still fades the complete screen out over 320 ms. The preference remains off by default, with the user's stored choice preserved.

The painter caches the background and phrase in one offscreen DIB. It prepares the confirmation text once and blends only that small strip during the animation, then presents the completed pixels in a single copy. Readiness does not clear the visible window, reselect/redecode the picture, recreate its bitmap or restart the whole-window fade. The confirmation text stays with the picture during dismissal. The bitmap is released after the screen finishes hiding.

Checks cover five window proportions, exact picture equivalence at text opacity zero, unchanged pixels outside the text at half/full opacity, bounded intermediate colours, 900 ms timing, unchanged picture selection while ready, fresh input, subsequent automatic transitions, focus preservation, shutdown, settings persistence, and install/repeat/ownership/revert transactions. These are fixture checks; a retail flicker retest remains useful.

Installed into the F: test client with settings and 15,425 prepared material-cache files preserved. The graphics core and shaders are unchanged from the r47c storm checks. No cliff prototype is installed by this update.

Evidence: `build/preview24/zoning-r47d/`. Install and rollback helpers: `tools/Install-Preview24-r47d.cmd`, `tools/Revert-Preview24-r47d.cmd`. Published Preview 24 is unchanged.
