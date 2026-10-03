// SHE library
// Copyright (C) 2021-2026 LibreSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "she/she.h"

#include "base/concurrent_queue.h"
#include "base/exception.h"
#include "base/string.h"
#include "she/sdl2/sdl2_display.h"
#include "she/sdl2/sdl2_surface.h"
#include "she/common/system.h"
#include "she/logger.h"

#undef HAVE_STDINT_H
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>
#else
#include <SDL.h>
#include <SDL_hints.h>
#include <SDL_image.h>
#include <SDL_syswm.h>
#endif

#include <iostream>
#include <cassert>
#include <list>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <cstdio>

#define SDL_HINT_WINDOWS_DPI_AWARENESS "SDL_WINDOWS_DPI_AWARENESS"

float penPressure = 0;

namespace ui {
  float get_pen_pressure() {
    return penPressure;
  }
}

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
extern "C" {
    void onPointerEvent(int isPen, float pressure) {
	if (isPen) {
	    penPressure = pressure > 0 ? pressure : 0.0001;
	} else {
	    penPressure = 0;
	}
    }
}
extern bool cfginit();
#endif

static she::System* g_instance = nullptr;
static std::unordered_map<int, she::Event::MouseButton> mouseButtonMapping = {
  {SDL_BUTTON_LEFT, she::Event::LeftButton},
  {SDL_BUTTON_MIDDLE, she::Event::MiddleButton},
  {SDL_BUTTON_RIGHT, she::Event::RightButton}
};
static she::KeyScancode lastScancode;
static int lastScancodeSDL;
struct Modifier {
  const int sheModifier;
  int ascii;
  bool isPressed = false;
  Modifier(int sheModifier) : sheModifier(sheModifier) {}
};

static std::unordered_map<int, Modifier*> reverseKeyCodeMapping;

static std::unordered_map<SDL_Keycode, Modifier> keyCodeMapping = {
  {SDLK_UNKNOWN, she::kKeyNil},
  {SDL_Keycode(13), she::kKeyEnter},
  {SDLK_PERIOD, she::kKeyStop},
  {SDLK_a, she::kKeyA},
  {SDLK_b, she::kKeyB},
  {SDLK_c, she::kKeyC},
  {SDLK_d, she::kKeyD},
  {SDLK_e, she::kKeyE},
  {SDLK_f, she::kKeyF},
  {SDLK_g, she::kKeyG},
  {SDLK_h, she::kKeyH},
  {SDLK_i, she::kKeyI},
  {SDLK_j, she::kKeyJ},
  {SDLK_k, she::kKeyK},
  {SDLK_l, she::kKeyL},
  {SDLK_m, she::kKeyM},
  {SDLK_n, she::kKeyN},
  {SDLK_o, she::kKeyO},
  {SDLK_p, she::kKeyP},
  {SDLK_q, she::kKeyQ},
  {SDLK_r, she::kKeyR},
  {SDLK_s, she::kKeyS},
  {SDLK_t, she::kKeyT},
  {SDLK_u, she::kKeyU},
  {SDLK_v, she::kKeyV},
  {SDLK_w, she::kKeyW},
  {SDLK_x, she::kKeyX},
  {SDLK_y, she::kKeyY},
  {SDLK_z, she::kKeyZ},
  {SDLK_0, she::kKey0},
  {SDLK_1, she::kKey1},
  {SDLK_2, she::kKey2},
  {SDLK_3, she::kKey3},
  {SDLK_4, she::kKey4},
  {SDLK_5, she::kKey5},
  {SDLK_6, she::kKey6},
  {SDLK_7, she::kKey7},
  {SDLK_8, she::kKey8},
  {SDLK_9, she::kKey9},
  {SDLK_KP_0, she::kKey0Pad},
  {SDLK_KP_1, she::kKey1Pad},
  {SDLK_KP_2, she::kKey2Pad},
  {SDLK_KP_3, she::kKey3Pad},
  {SDLK_KP_4, she::kKey4Pad},
  {SDLK_KP_5, she::kKey5Pad},
  {SDLK_KP_6, she::kKey6Pad},
  {SDLK_KP_7, she::kKey7Pad},
  {SDLK_KP_8, she::kKey8Pad},
  {SDLK_KP_9, she::kKey9Pad},
  {SDLK_F1, she::kKeyF1},
  {SDLK_F2, she::kKeyF2},
  {SDLK_F3, she::kKeyF3},
  {SDLK_F4, she::kKeyF4},
  {SDLK_F5, she::kKeyF5},
  {SDLK_F6, she::kKeyF6},
  {SDLK_F7, she::kKeyF7},
  {SDLK_F8, she::kKeyF8},
  {SDLK_F9, she::kKeyF9},
  {SDLK_F10, she::kKeyF10},
  {SDLK_F11, she::kKeyF11},
  {SDLK_F12, she::kKeyF12},
  {SDLK_ESCAPE, she::kKeyEsc},
  {SDLK_QUOTE, she::kKeyTilde},
  {SDLK_MINUS, she::kKeyMinus},
  {SDLK_EQUALS, she::kKeyEquals},
  {SDLK_BACKSPACE, she::kKeyBackspace},
  {SDLK_TAB, she::kKeyTab},
  {SDLK_LEFTBRACKET, she::kKeyOpenbrace},
  {SDLK_RIGHTBRACKET, she::kKeyClosebrace},
  {SDLK_KP_ENTER, she::kKeyEnter},
  {SDLK_COLON, she::kKeyColon},
  {SDLK_QUOTE, she::kKeyQuote},
  {SDLK_BACKSLASH, she::kKeyBackslash},
  // {SDLK_BACKSLASH2, she::kKeyBackslash2},
  {SDLK_COMMA, she::kKeyComma},
  {SDLK_STOP, she::kKeyStop},
  {SDLK_SLASH, she::kKeySlash},
  {SDLK_SPACE, she::kKeySpace},
  {SDLK_INSERT, she::kKeyInsert},
  {SDLK_DELETE, she::kKeyDel},
  {SDLK_HOME, she::kKeyHome},
  {SDLK_END, she::kKeyEnd},
  {SDLK_PAGEUP, she::kKeyPageUp},
  {SDLK_PAGEDOWN, she::kKeyPageDown},
  {SDLK_LEFT, she::kKeyLeft},
  {SDLK_RIGHT, she::kKeyRight},
  {SDLK_UP, she::kKeyUp},
  {SDLK_DOWN, she::kKeyDown},
  {SDLK_KP_DIVIDE, she::kKeySlashPad},
  {SDLK_ASTERISK, she::kKeyAsterisk},
  {SDLK_KP_MINUS, she::kKeyMinusPad},
  {SDLK_KP_PLUS, she::kKeyPlusPad},
  // {SDLK_KP_DEL, she::kKeyDelPad},
  {SDLK_KP_PERIOD, she::kKeyDelPad},
  {SDLK_KP_ENTER, she::kKeyEnterPad},
  {SDLK_PRINTSCREEN, she::kKeyPrtscr},
  {SDLK_PAUSE, she::kKeyPause},
  // {SDLK_ABNTC1, she::kKeyAbntC1},
  // {SDLK_YEN, she::kKeyYen},
  // {SDLK_KANA, she::kKeyKana},
  // {SDLK_CONVERT, she::kKeyConvert},
  // {SDLK_NOCONVERT, she::kKeyNoconvert},
  {SDLK_AT, she::kKeyAt},
  // {SDLK_CIRCUMFLEX, she::kKeyCircumflex},
  // {SDLK_COLON2, she::kKeyColon2},
  // {SDLK_KANJI, she::kKeyKanji},
  {SDLK_KP_EQUALS, she::kKeyEqualsPad},
  {SDLK_BACKQUOTE, she::kKeyBackquote},
  {SDLK_SEMICOLON, she::kKeySemicolon},
  // {SDLK_COMMAND, she::kKeyCommand},
  // {SDLK_UNKNOWN1, she::kKeyUnknown1},
  // {SDLK_UNKNOWN2, she::kKeyUnknown2},
  // {SDLK_UNKNOWN3, she::kKeyUnknown3},
  // {SDLK_UNKNOWN4, she::kKeyUnknown4},
  // {SDLK_UNKNOWN5, she::kKeyUnknown5},
  // {SDLK_UNKNOWN6, she::kKeyUnknown6},
  // {SDLK_UNKNOWN7, she::kKeyUnknown7},
  // {SDLK_UNKNOWN8, she::kKeyUnknown8},
  {SDLK_LSHIFT, she::kKeyLShift},
  {SDLK_RSHIFT, she::kKeyRShift},
  {SDLK_LCTRL, she::kKeyLControl},
  {SDLK_RCTRL, she::kKeyRControl},
  {SDLK_LALT, she::kKeyAlt},
  {SDLK_RALT, she::kKeyAltGr},
  {SDL_Keycode(1073742051), she::kKeyLWin},
  // {SDLK_RWIN, she::kKeyRWin},
  {SDLK_MENU, she::kKeyMenu},
  {SDLK_SCROLLLOCK, she::kKeyScrLock},
  {SDLK_NUMLOCKCLEAR, she::kKeyNumLock},
  {SDLK_CAPSLOCK, she::kKeyCapsLock},
};

std::unordered_map<SDL_Keycode, Modifier> modifiers = {
  {SDLK_SPACE, she::kKeySpaceModifier},

  {SDLK_LALT, she::kKeyAltModifier},
  {SDLK_RALT, she::kKeyAltModifier},

  {SDLK_LCTRL, she::kKeyCtrlModifier},
  {SDLK_RCTRL, she::kKeyCtrlModifier},

  {SDLK_LGUI, she::kKeyCmdModifier},
  {SDLK_RGUI, she::kKeyCmdModifier},

  {SDLK_LSHIFT, she::kKeyShiftModifier},
  {SDLK_RSHIFT, she::kKeyShiftModifier}
};

she::KeyModifiers getSheModifiers() {
  int mod = 0;
  for (auto& entry : modifiers) {
    if (entry.second.isPressed)
      mod |= entry.second.sheModifier;
  }
  return (she::KeyModifiers) mod;
}

#ifdef __EMSCRIPTEN__
EM_JS(int, get_canvas_width, (), { return canvas.clientWidth; });
EM_JS(int, get_canvas_height, (), { return canvas.clientHeight; });
static int oldWidth, oldHeight;

static void addEventListener(const std::string& name, void (*function)(void*), void* data = nullptr) {
  EM_ASM({
    canvas.addEventListener(UTF8ToString($0), (event) => {
      window.event = event;
      dynCall('vi', $1, [$2]);
    });
  }, name.c_str(), function, data);
}

static void cancelEvent(void*) {
  EM_ASM({
    event.stopPropagation();
    event.preventDefault();
  });
}

static bool wrapped;
static void patchEventListeners() {
  if (wrapped)
    return;
  wrapped = EM_ASM_INT({
    let handle = 0;
    JSEvents.eventHandlers.forEach(handler => {
	if (handler.eventTypeString == 'keydown' || handler.eventTypeString == 'keyup') {
          handle = handler.callbackfunc;
	  let tmp = getWasmTableEntry(handle);
	  function wrapper(...args) {
	    let ret = tmp(...args);
	    let keyCode = GROWABLE_HEAP_I32()[(args[1] >> 2) + 9];
	    return ret && keyCode != 86;
	  }
	  if (tmp.name != wrapper.name)
	    wasmTableMirror[handle] = wrapper;
	}
    });
    return !!handle;
  });
}

#endif

static std::deque<she::Event> keybuffer;
static bool display_has_mouse = false;

#ifdef __vita__
// PS Vita: the built-in buttons and sticks arrive as an SDL game
// controller. They are translated here into the keyboard/mouse events
// LibreSprite already understands, without going through SDL's text
// input (which would pop up the system IME on every press).
namespace vita_input {
  static SDL_GameController* controller = nullptr;
  static bool rightClickHeld = false;      // Circle: touches act as right button
  static she::Event::MouseButton touchButton = she::Event::LeftButton;
  static float pointerX = 480, pointerY = 272; // virtual pointer (screen pixels)
  static bool pointerButtonDown = false;   // Triangle pressed at virtual pointer
  static std::chrono::steady_clock::time_point lastPoll = std::chrono::steady_clock::now();
  static std::chrono::steady_clock::time_point lastWheel = std::chrono::steady_clock::now();

  // Mirrors the SDL_KEYDOWN/SDL_KEYUP branch of the event loop for a
  // synthetic key, so held keys (Space, Alt...) are visible to
  // she::is_key_pressed() just like a physical keyboard.
  static void key(SDL_Keycode sym, bool down) {
    auto modifierIt = modifiers.find(sym);
    if (modifierIt != modifiers.end())
      modifierIt->second.isPressed = down;
    auto it = keyCodeMapping.find(sym);
    if (it == keyCodeMapping.end())
      return;
    it->second.isPressed = down;
    she::Event event;
    event.setType(down ? she::Event::KeyDown : she::Event::KeyUp);
    event.setModifiers(getSheModifiers());
    event.setScancode(static_cast<she::KeyScancode>(it->second.sheModifier));
    keybuffer.push_back(event);
  }

  static void shortcut(SDL_Keycode modifier, SDL_Keycode sym) {
    key(modifier, true);
    key(sym, true);
    key(sym, false);
    key(modifier, false);
  }

  static void open() {
    if (controller)
      return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
      if (SDL_IsGameController(i)) {
        controller = SDL_GameControllerOpen(i);
        if (controller)
          break;
      }
    }
  }

  static void button(Uint8 button, bool down) {
    std::printf("vita: button %d %s at %d,%d\n", int(button),
                down ? "down" : "up", int(pointerX), int(pointerY));
    switch (button) {
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  if (down) shortcut(SDLK_LCTRL, SDLK_z); break; // L: undo
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: if (down) shortcut(SDLK_LCTRL, SDLK_y); break; // R: redo
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    key(SDLK_UP, down); break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  key(SDLK_DOWN, down); break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  key(SDLK_LEFT, down); break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key(SDLK_RIGHT, down); break;
    case SDL_CONTROLLER_BUTTON_X:     key(SDLK_SPACE, down); break; // Square: hold to pan
    case SDL_CONTROLLER_BUTTON_A:     key(SDLK_LALT, down); break;  // Cross: hold for Alt (eyedropper)
    case SDL_CONTROLLER_BUTTON_B:     rightClickHeld = down; break; // Circle: hold for right click
    case SDL_CONTROLLER_BUTTON_START: key(SDLK_RETURN, down); break;
    case SDL_CONTROLLER_BUTTON_BACK:  key(SDLK_ESCAPE, down); break; // Select
    case SDL_CONTROLLER_BUTTON_Y: {   // Triangle: click at the virtual pointer
      she::Event event;
      event.setType(down ? she::Event::MouseDown : she::Event::MouseUp);
      event.setButton(rightClickHeld ? she::Event::RightButton : she::Event::LeftButton);
      event.setModifiers(getSheModifiers());
      event.setPosition({int(pointerX) / she::unique_display->scale(),
                         int(pointerY) / she::unique_display->scale()});
      event.setPressure(down ? 1.0f : 0.0f);
      event.setPointerType(she::PointerType::Mouse);
      pointerButtonDown = down;
      keybuffer.push_back(event);
      break;
    }
    default:
      break;
    }
  }

  // Analog sticks: left moves the virtual pointer, right (vertical) zooms.
  // Called on every event-queue read. The UI drains the queue in a loop
  // until it is empty, so this must not produce an event on every call:
  // sample the sticks at most once per frame and only report real moves.
  static void poll() {
    using namespace std::chrono_literals;
    auto now = std::chrono::steady_clock::now();
    if (now - lastPoll < 16ms)
      return;
    float dt = std::chrono::duration<float>(now - lastPoll).count();
    lastPoll = now;
    if (!controller || !she::unique_display)
      return;
    if (dt > 0.1f)
      dt = 0.1f;

    const int deadzone = 6000;
    int lx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    int ly = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    if (std::abs(lx) > deadzone || std::abs(ly) > deadzone) {
      // Quadratic response: slow near the center for pixel-precise moves.
      auto curve = [](int v) {
        float f = (std::abs(v) < 6000) ? 0.0f : v / 32767.0f;
        return f * std::abs(f);
      };
      const float speed = 600.0f; // pixels per second at full tilt
      const int oldX = int(pointerX) / she::unique_display->scale();
      const int oldY = int(pointerY) / she::unique_display->scale();
      pointerX = std::clamp(pointerX + curve(lx) * speed * dt, 0.0f, 959.0f);
      pointerY = std::clamp(pointerY + curve(ly) * speed * dt, 0.0f, 543.0f);
      const bool moved =
        (int(pointerX) / she::unique_display->scale() != oldX ||
         int(pointerY) / she::unique_display->scale() != oldY);
      if (moved) {
      she::Event event;
      event.setType(she::Event::MouseMove);
      event.setModifiers(getSheModifiers());
      event.setPosition({int(pointerX) / she::unique_display->scale(),
                         int(pointerY) / she::unique_display->scale()});
      event.setPressure(pointerButtonDown ? 1.0f : 0.0f);
      event.setPointerType(she::PointerType::Mouse);
      keybuffer.push_back(event);
      }
    }

    int ry = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);
    if (std::abs(ry) > 16000 && now - lastWheel > 150ms) {
      lastWheel = now;
      she::Event event;
      event.setType(she::Event::MouseWheel);
      event.setModifiers(getSheModifiers());
      event.setWheelDelta({0, ry > 0 ? 1 : -1});
      event.setPosition({int(pointerX) / she::unique_display->scale(),
                         int(pointerY) / she::unique_display->scale()});
      keybuffer.push_back(event);
    }
  }
}
#endif
namespace she {
  void log(const std::string& text) {
#if defined(ANDROID)
    SDL_Log("%s", text.c_str());
#endif
  }

  namespace sdl {
    bool isMaximized;
    bool isMinimized;
    extern std::unordered_map<int, SDL2Display*> windowIdToDisplay;
  }

  class SDL2EventQueue : public EventQueue {
  public:
    PointerType pointerType = PointerType::Mouse;
    std::chrono::steady_clock::time_point lastUpTime = std::chrono::steady_clock::now();

    SDL2EventQueue() {
#if defined(__EMSCRIPTEN__)
        EM_ASM(
            const onPointerEvent = Module.cwrap("onPointerEvent", "", ["number", "number"]);
	    const listener = event => {
		onPointerEvent((event.pointerType == "pen")|0, event.pressure);
	    };
            Module.canvas.addEventListener("pointerdown", listener);
	    Module.canvas.addEventListener("pointermove", listener);
	    Module.canvas.addEventListener("pointerup", listener);
            );
#endif
      if (reverseKeyCodeMapping.empty()) {
        for (auto& entry : keyCodeMapping) {
          reverseKeyCodeMapping[entry.second.sheModifier] = &entry.second;
          entry.second.ascii = entry.first;
        }
      }
    }

    void forceFlip() {
      for (auto& entry : sdl::windowIdToDisplay) {
        entry.second->flip({
            0,
            0,
            entry.second->width(),
            entry.second->height()
          });
        entry.second->present();
      }
    }

    void refresh() {
      if (!m_events.empty())
	return;
      Event event;
      while (true) {
	event.setType(Event::None);
	getEventInternal(event, false);
	if (event.type() == Event::None) {
	  return;
	}
	m_events.push(event);
      }
    }

    void getEvent(Event& event, bool) override {
      event.setType(Event::None);
      if (m_events.try_pop(event))
        return;
      if (she::instance()->isGfxThread())
	getEventInternal(event, false);
    }

    void getEventInternal(Event& event, bool) {
      SDL_Event sdlEvent;
#ifdef __vita__
      vita_input::poll();
#endif
      while (SDL_PollEvent(&sdlEvent)) {
        switch (sdlEvent.type) {
#ifdef __vita__
        case SDL_CONTROLLERDEVICEADDED:
          vita_input::open();
          continue;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
          vita_input::button(sdlEvent.cbutton.button,
                             sdlEvent.type == SDL_CONTROLLERBUTTONDOWN);
          continue;
        case SDL_CONTROLLERAXISMOTION:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_CONTROLLERDEVICEREMAPPED:
        case SDL_JOYAXISMOTION:
        case SDL_JOYBALLMOTION:
        case SDL_JOYHATMOTION:
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
        case SDL_JOYDEVICEADDED:
        case SDL_JOYDEVICEREMOVED:
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
          continue;
#endif
        case SDL_APP_DIDENTERFOREGROUND:
          SDL2Surface::textureGen++;
          forceFlip();
          continue;

	case SDL_SYSWMEVENT:
#if defined(EASYTAB_H)
#if defined(_WIN32)
	  {
	    auto& win = sdlEvent.syswm.msg->msg.win;
	    if (EasyTab_HandleEvent(win.hwnd, win.msg, win.lParam, win.wParam) == EASYTAB_OK) {
		penPressure = std::max<>(EasyTab->Pressure, 0.0001f);
	    }
	  }
#endif
#if defined(__linux__)
	    if (EasyTab_HandleEvent(&sdlEvent.syswm.msg->msg.x11.event) == EASYTAB_OK) {
		penPressure = std::max(EasyTab->Pressure, 0.0001f);
	    }
#endif
#endif
	    continue;

        case SDL_WINDOWEVENT:
          switch (sdlEvent.window.event) {
          case SDL_WINDOWEVENT_EXPOSED:
            forceFlip();
            continue;
          case SDL_WINDOWEVENT_SIZE_CHANGED:
            continue;

          case SDL_WINDOWEVENT_MAXIMIZED:
            sdl::isMaximized = true;
            sdl::isMinimized = false;
            std::cout << "Maximized" << std::endl;
            continue;

          case SDL_WINDOWEVENT_MINIMIZED:
            sdl::isMaximized = false;
            sdl::isMinimized = true;
            std::cout << "Minimized" << std::endl;
            continue;

          case SDL_WINDOWEVENT_RESTORED:
            sdl::isMaximized = false;
            sdl::isMinimized = false;
            std::cout << "Restored" << std::endl;
            continue;

          case SDL_WINDOWEVENT_RESIZED: {
	    #ifdef __EMSCRIPTEN__
	    continue;
	    #else
            auto display = sdl::windowIdToDisplay[sdlEvent.window.windowID];
            display->setWidth(sdlEvent.window.data1);
            display->setHeight(sdlEvent.window.data2);
            display->recreateSurface();
            event.setType(Event::ResizeDisplay);
            event.setDisplay(display);
            return;
	    #endif
          }

          case SDL_WINDOWEVENT_LEAVE: {
            if (display_has_mouse) {
              display_has_mouse = false;

              Event ev;
              ev.setType(Event::MouseLeave);
              m_events.push(ev);
              break;
            }
          }
          //silence 'Unknown windowevent' console spam for common SDL window events
          case SDL_WINDOWEVENT_SHOWN:
          case SDL_WINDOWEVENT_HIDDEN:
          case SDL_WINDOWEVENT_MOVED:
          case SDL_WINDOWEVENT_ENTER:
          case SDL_WINDOWEVENT_FOCUS_GAINED:
          case SDL_WINDOWEVENT_FOCUS_LOST:
          case SDL_WINDOWEVENT_CLOSE: 
          //closing the app is handled elsewhere so we can ignore it here
            continue;

          default:
            std::cout << "Unknown windowevent: " << (int) sdlEvent.window.event << std::endl;
            continue;
          }
          continue;

        case SDL_MOUSEMOTION:
          if (!display_has_mouse) {
            display_has_mouse = true;
            Event ev;
            ev.setType(Event::MouseEnter);
            m_events.push(ev);
          }

          event.setType(Event::MouseMove);
          event.setModifiers(getSheModifiers());
          event.setPosition({
              sdlEvent.motion.x / unique_display->scale(),
              sdlEvent.motion.y / unique_display->scale()
            });
#ifdef __vita__
          // Keep the stick-driven pointer where the finger last was.
          vita_input::pointerX = sdlEvent.motion.x;
          vita_input::pointerY = sdlEvent.motion.y;
          // The touch panel's force readings are not a pen pressure.
          event.setPressure(0);
          event.setPointerType(pointerType);
          return;
#endif

	  {
	      int hasFingerEvent = SDL_PeepEvents(&sdlEvent, 1, SDL_PEEKEVENT, SDL_FINGERMOTION, SDL_FINGERMOTION);
	      if (hasFingerEvent) {
		  penPressure = std::max<>(sdlEvent.tfinger.pressure, 0.0001f);
	      }
	  }


	  event.setPressure(penPressure);
	  event.setPointerType(pointerType);
          return;

        case SDL_FINGERMOTION:
#ifndef __vita__
          penPressure = std::max<>(sdlEvent.tfinger.pressure, 0.0001f);
#endif
          continue;

        case SDL_MOUSEWHEEL:
          event.setType(Event::MouseWheel);
          event.setModifiers(getSheModifiers());
          event.setWheelDelta({-sdlEvent.wheel.x, -sdlEvent.wheel.y});
          int x, y;
          SDL_GetMouseState(&x, &y);
          event.setPosition({
              x / unique_display->scale(),
              y / unique_display->scale()
            });
          return;

        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN: {
          auto type = sdlEvent.type == SDL_MOUSEBUTTONDOWN ? Event::MouseDown : Event::MouseUp;
          event.setType(type);
          event.setPosition({
              sdlEvent.button.x / unique_display->scale(),
              sdlEvent.button.y / unique_display->scale()
            });
          event.setButton(mouseButtonMapping[sdlEvent.button.button]);
          event.setModifiers(getSheModifiers());
#ifdef __vita__
          // Holding Circle turns a touch into a right click. The button
          // chosen on touch-down is reused for the matching touch-up.
          if (sdlEvent.type == SDL_MOUSEBUTTONDOWN)
            vita_input::touchButton = vita_input::rightClickHeld
              ? Event::RightButton : Event::LeftButton;
          event.setButton(vita_input::touchButton);
          std::printf("vita: touch %s at %d,%d\n",
                      sdlEvent.type == SDL_MOUSEBUTTONDOWN ? "down" : "up",
                      sdlEvent.button.x, sdlEvent.button.y);
#endif

	  if (penPressure > 0.0f) {
	    pointerType = PointerType::Pen;
	    event.setPressure(penPressure);
	    event.setPointerType(pointerType);
	  } else {
	    event.setPressure(sdlEvent.type == SDL_MOUSEBUTTONDOWN ? 1.0f : 0.0f);
	    event.setPointerType(pointerType);
	    pointerType = PointerType::Mouse;
	  }

	  auto now = std::chrono::steady_clock::now();
	  auto delta = now - lastUpTime;
          if (sdlEvent.type == SDL_MOUSEBUTTONUP) {
	    using namespace std::chrono_literals;
	    if (delta < 200ms) {
	      m_events.push(event);
	      event.setType(Event::MouseDoubleClick);
	      event.setPosition(event.position());
	      event.setButton(event.button());
	    }
	    lastUpTime = now;
          }

          return;
        }

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
          Event event;
          bool isPressed = sdlEvent.type == SDL_KEYDOWN;
          auto modifierIt = modifiers.find((SDL_Keycode) sdlEvent.key.keysym.sym);
          if (modifierIt != modifiers.end()) {
            modifierIt->second.isPressed = sdlEvent.type == SDL_KEYDOWN;
          }

          auto it = keyCodeMapping.find((SDL_Keycode) sdlEvent.key.keysym.sym);

          if (it == keyCodeMapping.end()) {
            std::cout << "Unknown scancode: " << sdlEvent.key.keysym.sym << std::endl;
            continue;
          }

          event.setType(isPressed ? Event::KeyDown : Event::KeyUp);
          auto modifiers = getSheModifiers();
          event.setModifiers(modifiers);
          it->second.isPressed = isPressed;
          auto scancode = static_cast<she::KeyScancode>(it->second.sheModifier);
          event.setScancode(scancode);
          if (isPressed) {
            lastScancode = scancode;
            lastScancodeSDL = sdlEvent.key.keysym.scancode;
          }
          if (sdlEvent.key.repeat) {
            event.setRepeat(sdlEvent.key.repeat);
          }
          keybuffer.push_back(event);
          if (modifiers & (she::kKeyCtrlModifier | she::kKeyCmdModifier)) {
            SDL_StopTextInput();
            break;
          } else if (!SDL_IsTextInputActive()) {
            SDL_StartTextInput();
          }
          continue;
        }

        case SDL_DROPFILE: {
          std::string file(sdlEvent.drop.file);
          event.setType(Event::DropFiles);
          event.setFiles({file});
          SDL_free(sdlEvent.drop.file);
          return;
        }

          // CloseDisplay,
          // ResizeDisplay,
          // MouseEnter,
          // MouseLeave,
          // TouchMagnify,
        case SDL_QUIT:
          event.setType(Event::CloseDisplay);
          return;

        case SDL_TEXTEDITING:
          continue;

        case SDL_TEXTINPUT: {
          keybuffer.clear();
          std::string textString = sdlEvent.text.text;
          base::utf8_const_iterator begin{textString.begin()};
          base::utf8_const_iterator end{textString.end()};
          Event event;
          event.setModifiers(getSheModifiers());
          for (auto it = begin; it != end; ++it) {
            event.setType(Event::KeyDown);
            event.setUnicodeChar(*it);
            if (lastScancodeSDL > SDL_SCANCODE_UNKNOWN && lastScancodeSDL < SDL_SCANCODE_RETURN) {
              event.setScancode(lastScancode);
              lastScancodeSDL = SDL_SCANCODE_UNKNOWN;
            }
            keybuffer.push_back(event);
            event.setType(Event::KeyUp);
            keybuffer.push_back(event);
          }

          break;
        }

        case SDL_KEYMAPCHANGED:
          continue;

        default:
          std::cout << "Unknown event: " << sdlEvent.type << std::endl;
          continue;
        }
      }

      if (!keybuffer.empty()) {
        event = keybuffer.front();
        keybuffer.pop_front();
        return;
      }
    }

    void queueEvent(const Event& event) override {
      m_events.push(event);
    }

  private:
    base::concurrent_queue<Event> m_events;
  };

  EventQueue* EventQueue::instance() {
    static SDL2EventQueue g_queue;
    return &g_queue;
  }

  class SDL2System : public CommonSystem {
  public:
    SDL2System() {
      g_instance = this;
    }

    ~SDL2System() {
      shutdown = true;
      sleeping = false;
      if (mainThread.joinable())
	mainThread.join();
      IMG_Quit();
      SDL_Quit();
      g_instance = nullptr;
    }

    bool shutdown{false};
    std::thread mainThread;
    std::thread::id mainThreadId;
    std::thread::id gfxThreadId;
    std::vector<std::function<void()>> gfxQueue;
    std::atomic<bool> sleeping{false};

    bool isGfxThread() override {
      return std::this_thread::get_id() == gfxThreadId;
    }

    bool isMainThread() override {
      return std::this_thread::get_id() == mainThreadId;
    }

    void gfx(std::function<void()>&& func, bool sleep) override {
      if (isGfxThread()) {
	func();
	return;
      }
      gfxQueue.emplace_back(std::move(func));
      if (sleep)
	this->sleep();
    }

    using Timestamp = std::chrono::high_resolution_clock::time_point;
    Timestamp start = std::chrono::high_resolution_clock::now();

    void sleep() override {
      using namespace std::chrono_literals;
      if (shutdown)
	return;

      if (mainThreadId == gfxThreadId) {
	refresh();
	auto now = std::chrono::high_resolution_clock::now();

	// If the dispatching of messages was faster than 10 milliseconds,
	// it means that the process is not using a lot of CPU, so we can
	// wait the difference to cover those 10 milliseconds
	// sleeping. With this code we can avoid 100% CPU usage.
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
	start = now;

	if (elapsed < 15ms)
	  std::this_thread::sleep_for(15ms - elapsed);
      } else if (isMainThread()) {
	sleeping = true;
	while (sleeping) {
	  using namespace std::chrono_literals;
	  std::this_thread::sleep_for(10ms);
	}
      }
    }

    std::function<int()> m_func;
    int run(std::function<int()>&& func) override {
      gfxThreadId = std::this_thread::get_id();
      #ifndef EMSCRIPTEN
      mainThreadId = gfxThreadId;
      return func();
      #elif defined(EMSCRIPTEN) && !defined(__EMSCRIPTEN__)
      m_func = std::move(func);
      // like emscripten, but not really
      mainThread = std::thread{[this]{
        mainThreadId = std::this_thread::get_id();
	m_func();
      }};
      while (!shutdown)(
	refresh();
      }
      #else
      m_func = std::move(func);

      addEventListener("dragenter", cancelEvent);
      addEventListener("dragover", cancelEvent);
      addEventListener("drop", [](void*){
	EM_ASM({
	  event.stopPropagation();
	  event.preventDefault();
	  let files = event.dataTransfer?.files;
	  if (files?.length) {
	    for (let i = 0; i < files.length; ++i) {
	      let fr = new FileReader();
	      fr.onload = ((fr, name)=>{
		  try{ FS.mkdir('/tmp'); }catch(ex){}
		  FS.writeFile('/tmp/' + name, new Uint8Array(fr.result));
		  canvas.dispatchEvent(new CustomEvent("readFile", {detail:{path:'/tmp/' + name}}));
		}).bind(null, fr, files[i].name);
	      fr.readAsArrayBuffer(files[i]);
	    }
	  }
        });
      });

      addEventListener("readFile", [](void*){
	auto str = (char*) EM_ASM_PTR({return stringToNewUTF8(event.detail.path)});
	std::string path = str;
	free(str);
	Event event;
	event.setType(Event::DropFiles);
	event.setFiles({path});
	static_cast<SDL2EventQueue*>(EventQueue::instance())->queueEvent(event);
      });

      emscripten_set_main_loop([]{
	  if (!cfginit())
	      return;
	  auto sys = static_cast<SDL2System*>(g_instance);
	  sys->mainThread = std::thread{[]{
	      auto sys = static_cast<SDL2System*>(g_instance);
	      sys->mainThreadId = std::this_thread::get_id();
	      sys->m_func();
	  }};
	  emscripten_cancel_main_loop();
	  emscripten_set_main_loop([]{
	      patchEventListeners();
	      static_cast<SDL2System*>(g_instance)->refresh();
	  }, 0, true);
      }, 0, true);
      #endif
      return 0;
    }

    void refresh() {
      if (!sleeping) {
	static_cast<SDL2EventQueue*>(EventQueue::instance())->refresh();
	return;
      }
      #ifdef __EMSCRIPTEN__
      auto width = get_canvas_width();
      auto height = get_canvas_height();
      if (width && height && (oldWidth != width || oldHeight != height) && !sdl::windowIdToDisplay.empty()) {
	oldWidth = width;
	oldHeight = height;
	for (auto& entry : sdl::windowIdToDisplay) {
	  auto display = entry.second;
	  SDL_SetWindowSize(display->m_window, width, height);
	  display->setWidth(width);
	  display->setHeight(height);
	  display->recreateSurface();
	  Event event;
	  event.setType(Event::ResizeDisplay);
	  event.setDisplay(display);
	  static_cast<SDL2EventQueue*>(EventQueue::instance())->queueEvent(event);
	}
      }
      #endif
      int frames = 5;
      do {
	for (auto it = gfxQueue.begin(); it != gfxQueue.end(); ++it) {
	  (*it)();
	}
	gfxQueue.clear();
	sleeping = false;
	for (auto& entry : sdl::windowIdToDisplay)
	  entry.second->present();
	static_cast<SDL2EventQueue*>(EventQueue::instance())->refresh();
      } while (sleeping && --frames);
    }

    void activateApp() override {
      // Do nothing
    }

    void finishLaunching() override {
      // Do nothing
    }

    Capabilities capabilities() const override {
      return (Capabilities)(int(Capabilities::CanResizeDisplay) | int(Capabilities::GpuAccelerationSwitch));
    }

    EventQueue* eventQueue() override { // TODO remove this function
      return EventQueue::instance();
    }

    bool gpuAcceleration() const override {
      return SDL2Display::gpu;
    }

    void setGpuAcceleration(bool state) override {
      if (!unique_display)
        SDL2Display::gpu = state;
    }

    gfx::Size defaultNewDisplaySize() override {
      return gfx::Size(0, 0);
    }

    gfx::Size desktopSize() override {
      // Reports the primary display's resolution in pixels (or in points on
      // platforms where the window is not created high-DPI aware, e.g. macOS
      // without SDL_WINDOW_ALLOW_HIGHDPI). Returns (0, 0) when SDL can't
      // determine it or the video subsystem isn't up yet.
      if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
        return gfx::Size(0, 0);
      SDL_DisplayMode mode;
      if (SDL_GetDesktopDisplayMode(0, &mode) != 0)
        return gfx::Size(0, 0);
      return gfx::Size(mode.w, mode.h);
    }

    Display* defaultDisplay() override {
      return unique_display;
    }

    Display* createDisplay(int width, int height, int scale) override {
      //LOG("Creating display %dx%d (scale = %d)\n", width, height, scale);
      return new SDL2Display(width, height, scale);
    }

    Surface* createSurface(int width, int height) override {
      return new SDL2Surface(width, height, SDL2Surface::DeleteAndDestroy);
    }

    Surface* createRgbaSurface(int width, int height) override {
      return new SDL2Surface(width, height, 32, SDL2Surface::DeleteAndDestroy);
    }

    std::vector<uint8_t> encodeSurfaceAsPNG(Surface* s) override {
      auto surface = static_cast<SDL2Surface*>(s);
      std::vector<uint8_t> data;
      data.resize(surface->width() * surface->height() * 4 + 1024);
      std::shared_ptr<SDL_RWops> rops{
        SDL_RWFromMem(data.data(), data.size()),
        [](auto *rops){ rops->close(rops); }
      };
      if (IMG_SavePNG_RW(static_cast<SDL_Surface*>(surface->nativeHandle()), rops.get(), 0) != 0)
        return {};
      data.resize(SDL_RWtell(rops.get()));
      return data;
    }

    Surface* loadSurface(const char* filename) override {
      SDL_Surface* bmp = IMG_Load(filename);
      if (!bmp)
	throw std::runtime_error(std::string{"Error loading image "} + filename);
      return new SDL2Surface(bmp, SDL2Surface::DeleteAndDestroy);
    }

    Surface* loadRgbaSurface(const char* filename) override {
      SDL_Surface* bmp = IMG_Load(filename);
      if (!bmp)
	throw std::runtime_error(std::string{"Error loading image "} + filename);
      if (bmp->format->BitsPerPixel < 32) {
        auto copy = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGBA8888, 0);
        SDL_FreeSurface(bmp);
        bmp = copy;
      }
      return new SDL2Surface(bmp, SDL2Surface::DeleteAndDestroy);
    }

  };

  System* create_system() {
    return new SDL2System();
  }

  System* instance()
  {
    return g_instance;
  }

  void error_message(const char* msg)
  {
    if (g_instance && g_instance->logger())
      g_instance->logger()->logError(msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, PACKAGE, msg, nullptr);
  }

  int scancode_to_ascii(KeyScancode scancode) {
    auto it = reverseKeyCodeMapping.find(scancode);
    if (it == reverseKeyCodeMapping.end())
      return 0;
    return it->second->ascii;
  }

  bool is_key_pressed(KeyScancode scancode) {
    auto it = reverseKeyCodeMapping.find(scancode);
    if (it != reverseKeyCodeMapping.end()) {
      return it->second->isPressed;
    }
    return false;
  }

  void set_input_rect(const gfx::Rect& rect) {
    if (rect.isEmpty()) {
      SDL_StopTextInput();
      return;
    }
    SDL_Rect sdlRect{
      rect.x,
      rect.y,
      rect.w,
      rect.h
    };
    SDL_SetTextInputRect(&sdlRect);
    SDL_StartTextInput();
  }

  void clear_keyboard_buffer() {
    keybuffer.clear();
  }

} // namespace she

// It must be defined by the user program code.
extern int app_main(int argc, char* argv[]);

#ifdef __vita__
#include <psp2/io/stat.h>
#include <psp2/power.h>
#include <cstdio>
#include <cstdlib>

extern "C" {
// Main-memory heap for newlib's malloc (the default is far too small for
// an image editor). Textures live in separate GPU memory blocks.
__attribute__((used)) int _newlib_heap_size_user = 192 * 1024 * 1024;
// Default stack for std::thread/pthreads (pthread-embedded's own default
// is only 4 KiB).
__attribute__((used)) unsigned int _pthread_stack_default_user = 1024 * 1024;
// Main thread stack, read by vita-elf-create into the process parameters.
__attribute__((used)) extern const unsigned int sceUserMainThreadStackSize = 4 * 1024 * 1024;
}

static void vita_setup_environment() {
  // Writable storage for preferences, sessions, logs and user files.
  static const char* dirs[] = {
    "ux0:data/LibreSprite",
    "ux0:data/LibreSprite/config",
    "ux0:data/LibreSprite/config/libresprite",
    "ux0:data/LibreSprite/tmp",
    "ux0:data/LibreSprite/sprites",
  };
  for (auto dir : dirs)
    sceIoMkdir(dir, 0777);

  setenv("HOME", "ux0:data/LibreSprite", 1);
  setenv("XDG_CONFIG_HOME", "ux0:data/LibreSprite/config", 1);
  setenv("XDG_DESKTOP_DIR", "ux0:data/LibreSprite/sprites", 1);
  setenv("TMPDIR", "ux0:data/LibreSprite/tmp", 1);

  // Console output is invisible on the Vita; keep it for bug reports.
  std::freopen("ux0:data/LibreSprite/log.txt", "w", stdout);
  std::freopen("ux0:data/LibreSprite/log.txt", "a", stderr);
  // Unbuffered, so the log is complete up to the moment of a crash.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);

  // Run at full clock speed: the editor redraws a lot on the CPU.
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);

  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
  // Only the front panel drives the pointer; ignore the rear touchpad so
  // fingers resting on the back of the console don't paint.
  SDL_SetHint(SDL_HINT_VITA_TOUCH_MOUSE_DEVICE, "1");
  setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);
}
#endif

int main(const int argc, char* argv[]) {
#ifdef __vita__
  vita_setup_environment();
#endif
  #ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
  #endif
  SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

  // If the requested DPI awareness is not available on the currently running OS,
  // SDL will try to request the best available match.
  // https://wiki.libsdl.org/SDL2/SDL_HINT_WINDOWS_DPI_AWARENESS
  SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");

#ifdef __vita__
  const Uint32 sdlInitFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER;
#else
  const Uint32 sdlInitFlags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
#endif
  if (SDL_Init(sdlInitFlags) != 0) {
    std::cerr << "Critical: Could not initialize SDL2. Aborting." << std::endl;
    return -1;
  }
  if (!IMG_Init(-1)) {
    std::cerr << "Critical: Could not initialize SDL2_image (" << IMG_GetError() << "). Aborting." << std::endl;
    return -2;
  }
  SDL_EventState(SDL_FINGERMOTION, SDL_ENABLE);
#ifdef __vita__
  vita_input::open();
#endif
  return app_main(argc, argv);
}
