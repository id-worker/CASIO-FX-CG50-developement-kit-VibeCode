#!/usr/bin/env python3
"""Build and verify the small fx-CG50 v1.02 installation release.

The output directory receives one ZIP, five individual G3As, TDIC.DAT and an
external SHA256SUMS.txt. Complete sources and further guides stay in the Git
repository; no SDK, user photos, or private backups are packaged.
"""
from pathlib import Path, PurePosixPath
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
import argparse
import hashlib

ROOT = Path(__file__).resolve().parents[1]
APPS = ('GraphLab', 'Wireworld', 'PyCEdit', 'Translator', 'ObjView')
ARCHIVE = 'CG50_Kit_v1.02.zip'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_name(name):
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts:
        raise ValueError(f'Unsafe archive path: {name}')
    for part in path.parts:
        if any(c in part for c in '<>:"\\|?*') or part.endswith(('.', ' ')):
            raise ValueError(f'Not Windows-safe: {name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path,
                        help='Directory outside the source repository')
    args = parser.parse_args()
    out = args.output.resolve()
    if out == ROOT or ROOT in out.parents:
        raise SystemExit('Choose an output directory outside the repository.')
    out.mkdir(parents=True, exist_ok=True)

    files = {}
    individual = {}
    for app in APPS:
        filename = app + '.g3a'
        data = (ROOT / 'releases' / filename).read_bytes()
        if data != (ROOT / app / filename).read_bytes():
            raise SystemExit(f'Project and releases/{filename} differ')
        files[filename] = data
        individual[filename] = data

    dictionary = (ROOT / 'Translator' / 'TDIC.DAT').read_bytes()
    if sha(dictionary) != 'e7f6ab4a090c4f4998cbc5641a7cb06b44c5ef38a5c4c1c9d33f2e303a98cef2':
        raise SystemExit('Translator dictionary does not match expected SHA-256')
    files['TDIC.DAT'] = individual['TDIC.DAT'] = dictionary
    files['torus.obj'] = (ROOT / 'ObjView/models/torus.obj').read_bytes()
    files['LICENSE.txt'] = (ROOT / 'LICENSE').read_bytes()
    files['THIRD_PARTY_NOTICES.md'] = (ROOT / 'THIRD_PARTY_NOTICES.md').read_bytes()

    for app in APPS:
        for name in ('LICENSE', 'LICENSE_DATA.txt', 'THIRD_PARTY_NOTICES.md',
                     'THIRD_PARTY_NOTICES.txt', 'PUBLICATION_REVIEW_RU.md'):
            p = ROOT / app / name
            if p.is_file():
                files[f'notices/{app}/{name}'] = p.read_bytes()
        for dirname in ('third_party', 'licenses'):
            base = ROOT / app / dirname
            if base.is_dir():
                for p in sorted(base.rglob('*')):
                    if p.is_file():
                        files[f'notices/{app}/{dirname}/{p.relative_to(base).as_posix()}'] = p.read_bytes()

    for name in ('PYTEST.py', 'CTEST.c', 'COLORPY.py', 'COLORC.c'):
        files[f'Examples/{name}'] = (ROOT / 'PyCEdit/Examples' / name).read_bytes()

    files['README_RU.txt'] = '''Casio fx-CG50 Add-in Suite — v1.02
===================================

Версии: GraphLab 1.09, Wireworld 1.65, PyCEdit 1.15,
Translator 1.11 и ObjView 2.23. Номер набора и версии программ разные.

ВАЖНО: точные новые файлы прошли компьютерные проверки; их полная проверка
на настоящем fx-CG50 не подтверждена. Видимость новых цветов и выделенных
иконок на LCD необходимо проверить отдельно. Сохрани прежние .g3a, файлы
GLAB*.DAT / GLQ8*.DAT, Wireworld.ww, свои .py/.c и модели перед обновлением.

Скопируй нужные .g3a в корень памяти калькулятора. Для Translator туда же
скопируй TDIC.DAT. Для ObjView можно скопировать torus.obj и ввести имя torus
без расширения; в имени поддерживаются латинские буквы, цифры, . - _ и /.

Если раньше был установлен PyCLite.g3a, после резервного копирования удали
только этот старый аддин и скопируй PyCEdit.g3a. Если PyCEdit.g3a уже есть,
замени его после создания копии. Файлы .py и .c не удаляй. В меню имя PyCEdit.
Цветные функции синие, строки красные. В папке Examples лежат примеры, в том
числе COLORPY.py и COLORC.c; для просмотра подсветки запускать их не нужно.

PyCEdit: F3 SYM -> F5 PYMOD вставляет текст Python import; F3 SYM -> F4 LIBS
вставляет имена C-заголовков. Список не устанавливает модули; [FILE] требует
отдельный файл, а [SDK] не означает наличие заголовка в Utilities PicoC.

GraphLab 1.09 импортирует старые настройки в отдельный GLAB4_A/B.DAT;
старые версии не читают этот журнал. Сначала сохрани исходные данные.

Исходники, справка и подробные проверки:
https://github.com/id-worker/CASIO-FX-CG50-developement-kit-VibeCode/tree/v1.02

Собственный код — MIT (LICENSE.txt, Alex Work). Сторонние компоненты, шрифты,
данные словаря и модели имеют свои условия: THIRD_PARTY_NOTICES.md и notices/.
Атрибуция словаря Translator и права на часть кода выхода остаются вопросами
для проверки; предупреждение само по себе не является разрешением. PythonExtra
и GPL Utilities распространяются в отдельных репозиториях и сюда не включены.
'''.encode('utf-8')

    for name in files:
        safe_name(name)
    if any(p.suffix.lower() in {'.jpg', '.jpeg'} for p in map(PurePosixPath, files)):
        raise SystemExit('Unexpected photo in archive')
    files['SHA256SUMS.txt'] = ''.join(f'{sha(data)}  {name}\n' for name, data in sorted(files.items())).encode()

    archive = out / ARCHIVE
    with ZipFile(archive, 'w', compression=ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(files.items()):
            info = ZipInfo(name, (2026, 10, 29, 12, 0, 0))
            info.compress_type = ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, data, compress_type=ZIP_DEFLATED, compresslevel=9)
    with ZipFile(archive) as z:
        assert z.testzip() is None
        assert len(z.namelist()) == len(files) == len(set(z.namelist()))
        assert set(z.namelist()) == set(files)
        for name, data in files.items():
            assert z.read(name) == data, name

    individual[ARCHIVE] = archive.read_bytes()
    for name, data in individual.items():
        if name != ARCHIVE:
            (out / name).write_bytes(data)
    (out / 'SHA256SUMS.txt').write_text(
        ''.join(f'{sha(data)}  {name}\n' for name, data in sorted(individual.items())),
        encoding='utf-8')
    print(f'{archive}: {len(files)} files, {archive.stat().st_size} bytes')
    print(f'{sha(individual[ARCHIVE])}  {ARCHIVE}')
    print('ZIP CRC and every packaged file verified; assets and external SHA256SUMS created.')


if __name__ == '__main__':
    main()
