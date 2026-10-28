# Casio fx-CG50 Add-in Suite (`ObjView`, `PyCEdit`, `Wireworld`, `win_gint`)

A collection of native `.g3a` add-ins and developer tools for the **CASIO fx-CG50** graphing calculator, with C/Python source code. Several earlier builds were tested on real hardware with **OS 3.81 / `03.81.0202`**; see each project for the status of newer versions.

> **Русское описание находится ниже / Russian description below.**

---

## Included Projects & Ready-to-Install `.g3a` Binaries (`releases/` + per-project folders)

| File / Folder | Version | SDK | Description |
| :--- | :---: | :---: | :--- |
| **`ObjView/`** (`releases/ObjView.g3a`) | `v2.21` | `gint` + `fxSDK` | High-speed artifact-free **3D Wavefront `.obj` viewer** with Q16.16 ILRAM Z-buffer, Sutherland–Hodgman near-plane & 2D viewport clipping, Half-Lambert lighting, horizon-locked 6-DOF camera, `[SLW]`/`[FST]` speed modes, and optional `OPTN` 116 MHz turbo toggle. Includes `obj_viewer.py` for **PythonExtra** & **Windows IDLE** plus `.obj` test meshes (`torus.obj`, `spot.obj`, `teapot.obj`); see the notices for each model’s provenance. |
| **`PyCEdit/`** (`releases/PyCEdit.g3a`) | `v1.11` | `libfxcg` | On-calculator **Python (`.py`) & PicoC (`.c`) code editor** styled after `PythonExtra`, featuring syntax highlighting, selection, clipboard copy/cut/paste, auto-indent, instant file extension switcher (`.py` / `.c` / `.txt`), and instant flicker-free OS Main Menu switching. |
| **`Wireworld/`** (`releases/Wireworld.g3a`) | `v1.63` | `libfxcg` | Full-screen **Wireworld cellular automaton** (384×216 RGB565) on a wrapping torus up to `512×256` cells with dirty-rect + active-head sparse stepping, built-in logic gate circuits (Clock, Diode, OR, XOR, Memory), region Copy/Cut/Paste with 90° rotation & mirror flip, persistent `.ww` save/load, and instant OS Main Menu switching. |
| **`win_gint/`** | `v1.1` | Python 3 (`ctypes` Win32 GDI32) | Zero-dependency **Windows Python IDLE compatibility layer** implementing `gint` (`384×216` RGB565 window + 16-bit Z-buffer `dtri_zbuf`) and `time`/`utime` so fx-CG50 `PythonExtra` scripts run directly on a Windows PC at 60+ FPS without changes. |
| **`docs/`** | — | — | Comprehensive Russian guide & 544-function reference for **PicoC** on fx-CG50 (`picoc_guide.txt`) plus a compact ASCII pocket cheat-sheet (`picoc_pocket_en.txt`) readable directly inside `PyCEdit`. |

### More projects / Другие проекты

- **[GraphLab 1.06](GraphLab/README.md)** — 2D/3D graphing and fractals. Hardware testing of this version has not been confirmed / проверка этой версии на калькуляторе не подтверждена.
- **[Translator 1.10](Translator/README.md)** — offline RU↔EN translation; tested on hardware. Dictionary attribution and the menu-return code require further review.
- **[Wireworld 1.64](Wireworld_1.64/README.md)** — sources and experimental build kept separately from the hardware-tested 1.63. Version 1.64 has not yet been tested on a physical calculator.

The licensing and provenance review notes accompany each project; a warning is not a substitute for permission. See also [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

> **Two companion repositories** (kept separate because they carry different upstream licenses):
> - **PythonExtra** (prebuilt `pe-0.4.0-beta` binaries; upstream PythonExtra/MicroPython use MIT) → see sibling repository [`PythonExtra-4.0-beta-FX-CG50`](https://github.com/id-worker/PythonExtra-4.0-beta-FX-CG50)
> - **Utilities** (modified fork of [gbl08ma/utilities](https://github.com/gbl08ma/utilities), **GNU GPL v2**) → see sibling repository [`Utilities`](https://github.com/id-worker/Utilities)

---

## Quick Installation on Casio fx-CG50

1. Connect the calculator to your PC via USB and press **`F1`** (*USB Flash*).
2. Copy `ObjView.g3a`, `PyCEdit.g3a`, and/or `Wireworld.g3a` from `releases/` into the root directory of the calculator drive. Utilities and PythonExtra are available in their separate companion repositories linked above; sample `.obj` files are in `ObjView/models/`.
3. Safely eject the USB drive — the icons will appear in the Casio **Main Menu**.

---

## Набор аддинов и утилит для Casio fx-CG50 (RU)

Подборка нативных `.g3a`-аддинов и инструментов для **CASIO fx-CG50**. Некоторые предыдущие сборки проверены на реальном калькуляторе с **OS 3.81**; статус новых версий указан отдельно:

1. **`ObjView` (`releases/ObjView.g3a`, `v2.21`)** — 3D-просмотрщик моделей и карт Wavefront `.obj` без визуальных артефактов (16-битный Z-буфер в быстрой памяти ILRAM, точное отсечение ближней плоскостью и границами экрана, освещение Half-Lambert, 6-DOF камера с ровным горизонтом, точный шаг `[SLW]` на `ALPHA` без замедления поворота камеры, разгон `OPTN` 58/116 МГц и чистый выход в главное меню). В папке также лежит порт `obj_viewer.py` для `PythonExtra` / ПК и тестовые 3D-модели (`torus.obj`, `spot.obj`, `teapot.obj`); происхождение и права отдельных моделей описаны в `THIRD_PARTY_NOTICES.md`.
2. **`PyCEdit` (`releases/PyCEdit.g3a`, `v1.11`)** — текстовый редактор кода `.py` / `.c` / `.txt` в визуальном стиле `PythonExtra` с подсветкой синтаксиса, автоотступами, буфером обмена, быстрой сменой расширений файлов и мгновенным бесшовным выходом в меню ОС без мигания экрана.
3. **`Wireworld` (`releases/Wireworld.g3a`, `v1.63`)** — клеточный автомат Wireworld на весь экран (`384×216`) с бесшовным тороидальным полем до `512×256`, встроенными логическими схемами, копированием/вставкой участков с поворотом и зеркалированием, сохранением/загрузкой `Wireworld.ww` и мгновенным выходом в меню ОС.
4. **`win_gint/`** — библиотека совместимости `gint` + `utime` на чистом Win32 GDI32 (`ctypes`) для запуска графических и 3D-скриптов `PythonExtra` прямо из **Windows Python IDLE** без сторонних пакетов.
5. **`docs/`** — подробный учебник и справочник по языку **PicoC** на калькуляторе (`picoc_guide.txt`) и компактная английская шпаргалка (`picoc_pocket_en.txt`) для чтения с экрана самого калькулятора.

> **Две отдельные репозитории-спутника** (вынесены, потому что у них свои лицензии апстримов):
> - **PythonExtra** (готовые бинарники `pe-0.4.0-beta`; апстрим PythonExtra/MicroPython — MIT) → репозиторий [`PythonExtra-4.0-beta-FX-CG50`](https://github.com/id-worker/PythonExtra-4.0-beta-FX-CG50)
> - **Utilities** (патченый форк [gbl08ma/utilities](https://github.com/gbl08ma/utilities), **GNU GPL v2**) → репозиторий [`Utilities`](https://github.com/id-worker/Utilities)

---

## Building from Source

- **`ObjView`** (`gint` + `fxSDK` + `sh-elf-gcc`):
  ```bash
  cd ObjView
  fxsdk build-cg
  ```
- **`PyCEdit`** & **`Wireworld`** (`libfxcg` + `sh3eb-elf-gcc` + `mkg3a`):
  ```bash
  export FXCGSDK=/path/to/libfxcg
  make -C PyCEdit
  make -C Wireworld
  ```

---

## License & Credits

- Original portions of the code in this repository are offered under the **MIT License** — see [`LICENSE`](LICENSE). Third-party components retain their terms; unresolved provenance is described in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
- **Companion repositories with their own upstream licenses** (not included here):
  - **[`Utilities`](https://github.com/id-worker/Utilities)** — modified fork of [`gbl08ma/utilities`](https://github.com/gbl08ma/utilities) under **GNU GPL v2** (ships its own `LICENSE`, source files, and `PATCHNOTES.md` change notice; correspondence to a particular binary remains to be verified).
  - **[`PythonExtra-4.0-beta-FX-CG50`](https://github.com/id-worker/PythonExtra-4.0-beta-FX-CG50)** — prebuilt PythonExtra and MicroPython binaries (upstream MIT, with separate terms potentially applying to linked components). See its repository for provenance and notices.
- Compiled `.g3a` binaries link against open-source community SDKs (**`gint`**, **`fxSDK`**, **`OpenLibm`**, **`fxlibc`**, **`libfxcg`**, **`mkg3a`**, and GCC `libgcc` with its Runtime Library Exception). Identified copyright notices, licenses, attributions, and unresolved review items are documented in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
- *CASIO, PRIZM, and fx-CG50 are trademarks of CASIO COMPUTER CO., LTD. Unofficial third-party software.*
