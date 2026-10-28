# ICC return crash investigation

The available Windows crash dump is from 2026-10-26 at 20:54 local time, process
25676. It loaded the F: renderer from Preview13: PE timestamp `6adfad19`, image
size `f15000`. Both match the saved Preview13 DLL and symbol map. The installed
Preview14 DLL has a different timestamp (`6adfe92c`) and image size (`f1a000`).

Windows reports `c0000374`, heap corruption detected in ntdll. The matching stack
places the render worker in BeginFrame with NVIDIA driver frames beneath the heap
failure. This is the detection site; the dump does not identify the earlier write
that damaged the heap. It does not demonstrate an out-of-memory allocation.
The game thread was waiting for the render worker to finish its frame.

Preview13 also contained the separately reproduced expired MeshInfo borrow
described in `tree-pebbles-validation.md`; Preview14 corrected that ownership
defect and worker-failure waits. This dump alone cannot prove that it shares the
same origin. No additional speculative ownership fix is attributed to this dump.

At investigation time, the latest available Windows crash event/dump was the
Preview13 one. The F: log had later Preview14 sessions and no newer recorded
fault. Absence of a report is not proof that every zoning fault is resolved.
Evidence is preserved privately under `build/icc-return-crash/` with a matching
symbol-map analysis. No private client log, player capture or dump is published.

The existing ownership regression is rerun with concurrent rendering, repeated
grass regeneration, graphics-mode changes and zoning/cleanup. Successful memory
instrumentation and worker-error tests are regression evidence, not a claim of
live ICC → another zone → ICC visual or long-session verification. Public release
remains withheld.
