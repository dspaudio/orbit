# ORBIT development

Read README.md and BUILDING.md before changing firmware. Source is derived from SLOOP commit d691ba7b2d922f1a1f41a3622cffe29ce41c5506 with upstream v2.4.1 (a1c5d68767ae10fafb6821dc63b9b1fc490342d2) integrated; retain GPL notices and asset licences.

Firmware uses one C translation unit and a small strip canvas. Do not allocate a full framebuffer or PCM tape without checking the target memory budget. Event tape is a view/edit layer over sequencer steps; do not describe it as audio recording.

Run python tools/orbit_check.py for firmware changes. Build tools/orbit_host.c through tools/orbit_preview.py before running python tests/orbit_preview_test.py. Firmware changes also need pi32v2 target compilation and tests/target_budget.py when the toolchain/SDK are available. Host tests do not establish target safety or timing.

Keep web preview sound sourced from the firmware C engine. Host preview project-save doubles are intentionally nonpersistent; clearly mark that limitation. Never claim a Dots deployment or hardware test that has not occurred.
