# Kompilasi

Seluruh library pihak ketiga (Nuklear, GLFW, font) sudah disertakan di `third_party/` dan dikompilasi bersama program. Hasil build berupa executable tunggal yang dapat langsung dijalankan.

## 1. Linux dan WSL2

### Kebutuhan
- gcc atau clang dengan dukungan C99, dan `make`
- Header X11 dan OpenGL untuk mengompilasi GLFW

```bash
sudo apt install build-essential libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
```

Saat dijalankan, program hanya membutuhkan libX11 dan libGL yang tersedia di desktop Linux dan WSLg. Pada WSL2, GUI tampil lewat WSLg (Windows 11 atau Windows 10 dengan WSLg).

### Perintah

```bash
make                    # build/linux/rdr-expert dan build/linux/rdr-cli
make test               # unit test dengan AddressSanitizer dan UndefinedBehaviorSanitizer
make e2e                # end-to-end test CLI dan GUI
make clean              # menghapus folder build
scripts/build.sh        # alternatif make dengan pemeriksaan tool
```

Opsi tambahan: `DEBUG=1` untuk build debug, `TESTHOOK=1` untuk GUI dengan dukungan skrip pengujian.

## 2. Windows (native)

### Kebutuhan
gcc dari MinGW-w64, misalnya [w64devkit](https://github.com/skeeto/w64devkit) (cukup ekstrak dan jalankan `w64devkit.exe`) atau MSYS2 dengan paket `mingw-w64-x86_64-gcc`. Make tidak dibutuhkan.

### Perintah

```bat
scripts\build.bat           :: build\windows\rdr-expert.exe dan build\windows\rdr-cli.exe
scripts\build.bat clean
```

Executable di-link secara statis dan hanya bergantung pada DLL bawaan Windows (`GDI32`, `KERNEL32`, `msvcrt`, `OPENGL32`, `SHELL32`, `USER32`). Double click `rdr-expert.exe` untuk menjalankan, atau seret file `*.rdr.json` ke atas exe untuk langsung membukanya.

## 3. Cross-compile Windows dari Linux atau WSL2

```bash
sudo apt install mingw-w64
make windows            # build/windows/rdr-expert.exe dan build/windows/rdr-cli.exe
scripts/build.sh windows
```

Dari WSL2, executable Windows dapat langsung dijalankan, misalnya `./build/windows/rdr-expert.exe`.

## 4. Struktur Output

```
build/linux/      executable Linux beserta object dan resource hasil embed
build/windows/    executable Windows
build/hook/       build GUI dengan test hook (dipakai make e2e)
build/test/       unit test
build/e2e/        hasil end-to-end test
```

## 5. Catatan Teknis

- Standar bahasa C99 dengan flag `-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes`. Build Linux dan Windows tidak menghasilkan warning.
- Font dan teks Help di-embed ke dalam executable oleh `tools/embed.c` saat build.
- Path file ditangani sebagai UTF-8 di semua platform. Pada Windows, path dikonversi ke UTF-16 sebelum memanggil API sistem.
