# GraphLab 1.20 for Casio fx-CG50

GraphLab provides 2D/3D equations, inequalities, a structured formula editor, sums/products, an XY intersection map and custom fractals. The calculator interface is in English; the detailed guides are in Russian.

- **Ray is now the only 3D renderer.** The former Fast Ray keeps its polygon-free ray-casting approach; the separate VOXEL engine and renderer selector have been removed.
- 3D inequalities are also handled by Ray as approximate clipped volumes. The finite numerical search can miss small components, tangencies or difficult domain boundaries.
- Preview and initial Ray spacing remain independently adjustable from 1 to 32 pixels; Ray Q is 4–48. After movement stops, the view refines automatically to one pixel. The 2→1 pass can show completed rows and progress; this does not guarantee faster rendering on the calculator.
- 1.20 uses the same `GLAB8_A/B.DAT` journal as 1.18/1.19. Old renderer-selection bytes are retained for compatibility but ignored by 1.20. Back up `GraphLab.g3a`, `GLAB*.DAT` and `GLQ8*.DAT` before updating or reverting.
- **1.20 has not been tested on physical fx-CG50 hardware.** Host and SuperH/QEMU tests do not establish physical timing, available memory, LCD or keyboard behavior.

[Русская инструкция](README_RU.md) · [Быстрая проверка](БЫСТРАЯ_ПРОВЕРКА_RU.md) · [Настройки](НАСТРОЙКИ_RU.md) · [Проверки](ПРОВЕРКИ_1.20_RU.md)

Build with `fxsdk build-cg` and compatible fxSDK/gint 2.11. Third-party notices and license texts are in `THIRD_PARTY_NOTICES.md` and `third_party/`.
