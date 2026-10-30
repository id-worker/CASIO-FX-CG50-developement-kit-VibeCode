# GraphLab 1.20 — tests

These tests run the actual application code with synthetic input, display or filesystem stubs. They do **not** emulate Casio OS or prove physical fx-CG50 timing or memory availability.

- `sh tests/run_host_112.sh`: complete host suite, ASan/UBSan by default. Use `SANITIZE=0` only if the compiler lacks sanitizer support.
- `sh tests/run_target_112.sh`: SuperH/QEMU execution, synthetic display, storage, progress and keyboard; set `GINT_SOURCE` to a gint checkout for the optional real keydev tests.
- `sh tests/run_target_intersection112.sh`: additional SuperH intersection-map tests.
- `sh tests/run_target_volume120.sh`: analytic SuperH Ray-volume rays, clip faces, poles, domain boundaries, mixed-depth scenes and the 16 KiB stack.
- `sh tests/run_cross_119_120.sh`: both directions with the 1.19 source in `PREVIOUS_ROOT`, including both old mode-byte values, Q, formulas and no write on load. The script defaults to `/home/user/GraphLab_1.19/Source`.
- `sh tests/run_cross_previous.sh` / `sh tests/run_cross_quality.sh`: import and isolation against the bundled historical 1.08 storage fixture.
- `PREVIOUS_ROOT=/path/to/1.19/Source sh tests/run_compare_ray115.sh` and `run_compare_ray115_extra.sh`: exact equality-frame and statistics comparisons. The source directory of the old version is required.
- `sh tests/run_audit_screens120.sh /path/to/output`: synthetic host screenshots; `GINT_SOURCE` must include `font8x9.png`. They are not calculator photographs.

`test_ray_volume120.c` tests analytic rays and mixed equation/inequality visibility. `test_refine_ui118.c` exercises both equality and inequality scenes through the actual 1.20 UI, including interrupt/resume and exact final frames. `test_refine_memory118.c` covers a failed candidate-grid allocation and safe retry. Older historical reports and 1.19 source remain available in the separate 1.19 release.

The remaining old standalone test sources are historical; old release-specific comparison/benchmark runners are preserved in the separate 1.19 release rather than presented as current checks. `run_legacy_audit119.sh` and `run_110_target.sh` explicitly test retained mesh/section code that is **not** linked into 1.20.
