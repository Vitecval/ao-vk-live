# Fresh login and permanent day/night follow-up

The November 19 captures at 17:02–17:04 distinguish the remaining login bug
from the previously corrected lighting and temporal reactivity. The fresh
login dump reports `login studio 0`; the returned login reports `login studio
1`. Their cameras and three authored studio lights are the same. With no N3
engine instance on fresh startup, the previous presentation policy selected
world lighting indefinitely until the first confirmed login state after world
entry. Thus the earlier shader corrections were bypassed on a fresh start.

A new device now defaults to login presentation while the engine is unavailable.
An unknown query on the same device retains the last confirmed login or world
context. Confirmed world entry and logout change the context immediately. This
uses the accepted login appearance and its corrected additive-only temporal
reactivity, without applying another colour adjustment.

Day/Night Cycle now offers Server (0), Longer Day (1), Enhanced Daylights (2),
PermaDay (3) and PermaNight (4). The permanent options select an effective hour
of 13.5 or 0 and an effective clock speed of zero. Their selection is saved in
the normal settings file and reused after process restart. Saved manual hour
and speed preferences are preserved; selecting another cycle restores them.
An explicit Instant Day/Night shortcut exits the permanent mode. The enhanced
daylight profile remains exclusive to option 2. Horizon's catalogue and the
in-game settings menu expose the same five names and values.

## Validation

- Presentation-policy checks cover a fresh unavailable engine, temporary unknown
  states, world entry, logout and replacement devices.
- Fresh-start and returned-login renderer sequences are byte-for-byte identical.
  The prior temporal improvement remains 84.84% in the static-detail fixture.
  World, temple and interface comparison images are unchanged.
- Settings checks pass for both permanent choices, restored manual preferences,
  the Instant shortcut, enhancement toggles and separate-process restarts.
- Horizon's 59 self-checks pass, including both new option names and values.
- Renderer installer apply/revert and atomic GPU-cache replacement fixtures pass.

The verified renderer hash is
`156a8095cdbde6b67310607dd691e6fe919f441b5c957b6e2e552837ea1440ae`.
`build/login-start-cycle-fix/VERIFIED.json` records the checks and matching cache.
The renderer and prepared cache have been installed in the closed F: test client,
with backups and settings/textures preserved. Retail visual confirmation is
still pending; controlled fixtures do not prove the final in-game appearance.

The updated Horizon app is staged under `build/login-start-cycle-fix/app-only`.
Its embedded graphics bootstrap is still the published r62 package; the local
renderer trial is installed separately and must not be represented as published.
After Horizon was closed, application-only installation updated the app and its
ownership record to the tested 0.7.6 build without running graphics setup. The
installed app hash, previous-app backup and preserved AO renderer hash were
verified. `build/login-start-cycle-fix/APP-INSTALL.json` records the app and
backup locations. Preferences were preserved.
