# Third Party Components

Seluruh komponen di folder ini di-vendor agar program dapat dikompilasi tanpa instalasi library tambahan. Tidak ada komponen yang mengimplementasikan RDR.

| Komponen | Versi | Lisensi | Sumber | Kegunaan |
|---|---|---|---|---|
| Nuklear (`nuklear/nuklear.h`) | 4.13.3 | MIT atau Public Domain (`nuklear/LICENSE`) | https://github.com/Immediate-Mode-UI/Nuklear | Toolkit GUI immediate mode |
| GLFW (`glfw/`) | 3.4 | zlib (`glfw/LICENSE.md`) | https://github.com/glfw/glfw/releases/tag/3.4 | Window, konteks OpenGL, dan input |
| Google Sans (`fonts/GoogleSans*.ttf`) | Google Fonts | Sesuai lisensi Google Sans | https://fonts.google.com/specimen/Google+Sans | Font utama GUI |
| DejaVu Sans Mono (`fonts/`) | 2.37 | Bitstream Vera dan Public Domain (`fonts/LICENSE-DejaVu.txt`) | https://dejavu-fonts.github.io | Font cadangan dan karakter box drawing |

Catatan:
- Dari GLFW hanya diambil header publik serta source untuk platform X11 dan Win32. Arsip rilis `glfw-3.4.zip` memiliki SHA-256 `b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9`.
- Backend Nuklear untuk GLFW dan OpenGL ditulis sendiri di `src/gui/gui_backend.c`.
