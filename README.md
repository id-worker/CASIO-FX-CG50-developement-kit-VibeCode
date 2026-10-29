# Casio fx-CG50 Add-in Suite

A collection of native `.g3a` programs and developer resources for the
**CASIO fx-CG50**. Suite release **v1.01** collects the five programs below;
the release number is separate from each application's own version.
The interface of the calculator programs is in English; detailed guides are
mostly in Russian.

> **Testing status:** these new builds passed computer-side checks. GraphLab
> and Wireworld also have synthetic SuperH/QEMU tests, and ObjView's modifier
> regression was checked with the actual gint key code under QEMU. Full testing
> of these exact new files on physical hardware has not been confirmed. Earlier
> versions were tested on a real fx-CG50; that does not prove these new builds.
> Keep backups of working add-ins and data before updating.

## Applications in v1.01

| Application | Version | Description |
|---|---:|---|
| [GraphLab](GraphLab/README.md) | 1.09 | 2D/3D graphs, implicit equations and fractals; longer formula entry, monochrome UI and approximate 3D point preview. |
| [Wireworld](Wireworld/README.md) | 1.65 | A wrapping Wireworld field, patterns and editing tools; monochrome interface with the three simulation colors retained. |
| [PyCEdit](PyCEdit/README_RU.md) | 1.12 | Editor for `.py`, `.c` and `.txt`; monochrome panels and retained syntax highlighting. |
| [Translator](Translator/README.md) | 1.11 | Offline Russian ↔ English translation, mostly white UI; requires `TDIC.DAT`. |
| [ObjView](ObjView/README_RU.md) | 2.23 | Wavefront `.obj` viewer; monochrome panels, preserved model shading, and ALPHA/F3 filename-mode switching. |

The [`releases/`](releases/) folder contains the current `.g3a` files. The
[v1.01 GitHub release](https://github.com/id-worker/CASIO-FX-CG50-developement-kit-VibeCode/releases/tag/v1.01)
provides a small installation bundle and individual files. The older v1.0
release is retained. [`Wireworld_1.64/`](Wireworld_1.64/) is historical and
is not the new version.

## Installation

1. Back up your existing `.g3a` files and important data, including GraphLab
   `GLAB*.DAT` / `GLQ8*.DAT`, `Wireworld.ww`, code files and 3D models.
2. Copy the desired `.g3a` files to the root of the calculator's USB storage.
   For Translator, also copy [`TDIC.DAT`](Translator/TDIC.DAT) to the root; it
   is unchanged from 1.10. Eject the calculator safely.
3. In ObjView, copy `ObjView/models/torus.obj` to the root and enter `torus`
   without the extension. The filename input supports Latin letters, digits,
   `.`, `-`, `_`, and `/` as a path separator; spaces, parentheses and Cyrillic
   are not available. ALPHA/F3 cycle input modes. ALPHA in the model viewer
   controls slow **movement**, not rotation speed.
4. GraphLab 1.09 imports older saved settings into its separate `GLAB4_A/B.DAT`
   journal. Old versions do not read GLAB4; see its guide before switching.

## Дополнительно по-русски

**v1.01** — версия всего набора, а не номер каждого аддина. Пять программ:
GraphLab 1.09, Wireworld 1.65, PyCEdit 1.12, Translator 1.11 и ObjView 2.23.
До установки новых сборок сохрани прежние `.g3a` и важные файлы на ПК. Полная
проверка именно новых файлов на калькуляторе не подтверждена; проверка в QEMU
не заменяет устройство. Инструкции и ограничения приведены по ссылкам в
таблице выше. Для Translator нужен `TDIC.DAT` в корне накопителя.

Также в репозитории остаются [`win_gint/`](win_gint/) для Windows Python и
[`docs/`](docs/) с русскими материалами по PicoC. Они не заменяют отдельные
репозитории [PythonExtra](https://github.com/id-worker/PythonExtra-4.0-beta-FX-CG50)
и [Utilities](https://github.com/id-worker/Utilities).

## Building from source

- GraphLab and ObjView: install fxSDK/gint, enter the corresponding project
  folder and run `fxsdk build-cg`.
- Wireworld and PyCEdit: install a compatible libfxcg/PrizmSDK toolchain and
  `mkg3a`; set `FXCGSDK`, then run `make` in the project folder.
- Translator: see [`source/README_BUILD.txt`](Translator/source/README_BUILD.txt)
  for its libfxcg build. Individual project READMEs cover tests and caveats.

## Licenses and provenance

Original code is offered under the [MIT License](LICENSE), including the
`Alex Work` copyright credit. Third-party components and data retain their
own terms: see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the
project-specific notices and licenses. The Translator dictionary's precise
attribution/license provenance and a menu-return code fragment in some
projects remain unresolved; warnings do not themselves grant rights.

PythonExtra and the GPL Utilities fork are maintained in their separate
repositories and are not included in this release bundle. CASIO, PRIZM and
fx-CG50 are trademarks of CASIO COMPUTER CO., LTD.; this is unofficial
third-party software.
