# Third-Party Licenses & Community Attributions

Original portions of **ObjView**, **PyCEdit**, **Wireworld**, **obj_viewer.py**, **win_gint**, and the documentation in `docs/` are offered under the **MIT License** (see [`LICENSE`](LICENSE)). Third-party code, libraries and assets retain their own terms; the `set_menu_return_flag()` implementation and the exact source of one sample mesh require additional review (see sections 5 and 6).

> Two related projects ship in **companion repositories** under their own upstream licenses: **Utilities** (modified fork of `gbl08ma/utilities`, GNU GPL v2 → repo [`Utilities`](https://github.com/id-worker/Utilities)) and **PythonExtra** (prebuilt MIT binaries of PythonExtra + MicroPython → repo [`PythonExtra-4.0-beta-FX-CG50`](https://github.com/id-worker/PythonExtra-4.0-beta-FX-CG50)). Their notices travel with those repositories.

Prebuilt `.g3a` add-in binaries in this repository statically link against open-source Casio Prizm / fx-CG50 libraries and toolchains listed below. Identified notices and license terms are recorded below; see the unresolved review items before further redistribution.

> **Trademark Disclaimer:** CASIO, PRIZM, and fx-CG50 are trademarks or registered trademarks of CASIO COMPUTER CO., LTD. This repository is an independent, unofficial community project and is neither affiliated with nor endorsed by CASIO. No proprietary CASIO SDK or ROM code is included.

---

## 1. gint, fxSDK, fxlibc & OpenLibm (used when building `ObjView.g3a`)

- **gint** unikernel by **Lephenixnoir** & contributors  
  Upstream: <https://git.planet-casio.com/Lephenixnoir/gint> (mirrors: `Insoft-UK/gint`, `cheparukhin/gint`)
  > *"This is free software: you may use it for any purpose, share it, modify it, and share your changes. Credit is not required, but please let me know!"*

- **fxSDK** (`fxsdk` / `fxgxa`) — **MIT License**
  ```text
  Copyright (C) 2015-2022 gint/fxSDK contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
  ```

- **fxlibc** (`Vhex-Kernel-Core/fxlibc`) — SuperH C standard library by Vhex-Kernel-Core / Planète Casio contributors.

- **OpenLibm** (`Lephenixnoir/OpenLibm`, derived from OpenLibm / FreeBSD `msun` / Sun Microsystems `fdlibm`) — **MIT / 2-Clause BSD / ISC Licenses**:
  ```text
  Copyright (c) 2011-2021 OpenLibm Developers (Viral B. Shah, Jameson Nash, et al.)
  Copyright (c) 1992-2011 The FreeBSD Project. All rights reserved.
  Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.
  ```

---

## 2. libfxcg (used when building `PyCEdit.g3a` & `Wireworld.g3a`)

Upstream: <https://github.com/Jonimoose/libfxcg> — **3-Clause BSD License**:

```text
Copyright (c) 2014, The libfxcg Project Contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the project nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## 3. mkg3a (build-time `.g3a` packager)

Upstream: <https://gitlab.com/taricorp/mkg3a> — **zlib License**:

```text
Copyright (c) 2011-2020 Peter Marheine <peter@taricorp.net>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
```

---

## 4. GCC Runtime Library (`libgcc`)

Compiled SuperH (`sh3eb-elf` / `sh-elf`) binaries link minimal integer helper routines from GNU `libgcc`, distributed under **GPLv3 with the GCC Runtime Library Exception v3.1**, which explicitly permits linking into and distributing binaries under any license (including MIT).

---

## 5. Community Hardware & Syscall Documentation Credits

- **Dr-Carlos** (Cemetech, [topic #18944](https://www.cemetech.net/forum/viewtopic.php?t=18944)) & **Lephenixnoir** (`gint`): reverse-engineering of Casio OS syscall `0x1E58` (`SwitchToMainMenu` -> `SaveAndOpenMainMenu`) used for instant, flicker-free Main Menu transitions in `PyCEdit` and `Wireworld`.
- **Simon Lothar** & **gbl08ma**: documentation of Casio Prizm syscall `0x1E99` (`GetGetkeyToMainFunctionReturnFlag`) and R61524 VRAM layout.
- **TheRainbowPhoenix** & **OffCamera-Civilman** (`PythonExtra`): public `gint` / `utime` Python API design replicated in clean-room desktop form by `win_gint` for Windows Python IDLE compatibility.

---

### Pending: menu-return implementation

`set_menu_return_flag()` in the existing PyCEdit/Wireworld code resembles the
published routine by Simon Lothar and code in GPL Utilities. Attribution alone
is not permission to license a particular implementation under MIT. Confirm
permission / provenance, or independently replace and test it, before further
redistribution. The corresponding concern for Translator is documented in its
own `PUBLICATION_REVIEW_RU.md`.

---

## 6. Sample 3D Meshes (`ObjView/models/`)

Status of the bundled `.obj` files in `ObjView/models/`:
- `torus.obj` — Procedurally generated mathematical torus mesh (**CC0 1.0 Public Domain**).
- `spot.obj` — *"Spot"* cow mesh by **Keenan Crane** (Carnegie Mellon University), released into the **Public Domain (CC0)** (<https://www.cs.cmu.edu/~kmcrane/Projects/ModelRepository/>).
- `teapot.obj` — Classic **Utah Teapot** dataset by **Martin Newell** (1975, University of Utah). The exact provenance and CC0/public-domain status of this particular triangulated file have not been established. Utah Graphics Lab states that its Utah Teapot files are freely available for any use and requests preserving the name/origin: <https://graphics.cs.utah.edu/teapot/>. This is not, by itself, proof that this particular file is CC0. Review its source or replace it with an individually verified CC0 model before claiming that all bundled models are CC0.

---
