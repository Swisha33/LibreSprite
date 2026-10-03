# LibreSprite for PS Vita

An experimental port of LibreSprite to the PlayStation Vita, using the
existing SDL2 backend.

## Installing

1. Copy `LibreSprite.vpk` to the Vita and install it with VitaShell.
2. Launch **LibreSprite** from the LiveArea.

The app needs a homebrew-enabled Vita (HENkaku/Ensō). No plugins are
required.

Files are stored in `ux0:data/LibreSprite/`:

| Path | Contents |
| --- | --- |
| `sprites/` | Suggested folder for your own sprites |
| `config/libresprite/` | Preferences, sessions and backups |
| `log.txt` | Console output of the last run (attach it to bug reports) |

## Controls

| Input | Action |
| --- | --- |
| Touch screen | Left mouse button (draw, click, drag) |
| Hold **Circle** + touch | Right mouse button (secondary color, context menus) |
| Hold **Square** + drag | Pan the canvas (Space) |
| Hold **Cross** + touch | Alt (eyedropper with drawing tools) |
| **L** / **R** | Undo / Redo |
| D-pad | Arrow keys |
| Left stick | Move an on-screen pointer for pixel-precise work |
| **Triangle** | Click at the stick pointer (hold to drag) |
| Right stick up/down | Zoom (mouse wheel) |
| **Start** / **Select** | Enter / Esc |

Text fields open the system keyboard. A USB/Bluetooth keyboard also
works on models that support it.

On first launch the UI uses 1x scaling, which fits the most on the
960x544 screen. If it is too small, raise it under
*Edit > Preferences > General > UI Elements Scaling*.

## Limitations

- No networking (scripts that fetch URLs will fail).
- The clipboard is internal to the app; there is no system clipboard.
- Opening links or folders in another app does nothing.
- The rear touchpad is ignored.

## Building

You need [VitaSDK](https://vitasdk.org) and a host C++ compiler.

1. Install the dependencies. Most are available through vdpm:

   ```sh
   vdpm zlib libpng libjpeg-turbo freetype pixman TinyXML2 libarchive sdl2 sdl2_image
   ```

   giflib is not packaged for the Vita. `vita/build-deps.sh` builds it
   (and can build every other dependency from source too):

   ```sh
   ONLY="giflib" VITASDK=$VITASDK vita/build-deps.sh /path/to/sources
   ```

   If you use the vdpm SDL2_image, make sure it was built without QOI
   support, otherwise its `qoi_encode` clashes with LibreSprite's.

2. Build the `gen` code generator for your host:

   ```sh
   cmake -S . -B build-gen -DGEN_ONLY=ON -DUSE_SDL2_BACKEND=OFF
   cmake --build build-gen --target gen
   ```

3. Cross-compile and package:

   ```sh
   cmake -S . -B build-vita \
     -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake \
     -DCMAKE_BUILD_TYPE=Release \
     -DHOST_GEN_EXECUTABLE=$PWD/build-gen/bin/gen
   cmake --build build-vita
   ```

   The package is written to `build-vita/src/LibreSprite.vpk`.

## Porting notes

All Vita-specific code is guarded with `__vita__` or `if(VITA)`:

- `src/she/sdl2/she.cpp`: button/stick mapping, heap and stack sizes,
  writable directories, CPU clocks.
- `src/she/sdl2/sdl2_display.cpp`: fixed 960x544 window, software cursor.
- `src/base/`: app path (`app0:`), no `dlopen`, `kill` or external launcher.
- `src/net/http_request.cpp`: networking stub when libcurl is absent.
- `third_party/quickjs-amalgam/quickjs-libc-min.c`: the few quickjs-libc
  helpers the script engine needs, without the POSIX-only std/os modules.
