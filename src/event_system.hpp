#pragma once

#include "GLFW/glfw3.h"

#include <concepts>
#include <functional>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

enum class KeyCode : std::int32_t {
  Unknown = -1,
  Space = GLFW_KEY_SPACE,
  A = GLFW_KEY_A,
  B = GLFW_KEY_B,
  C = GLFW_KEY_C,
  D = GLFW_KEY_D,
  E = GLFW_KEY_E,
  F = GLFW_KEY_F,
  G = GLFW_KEY_G,
  H = GLFW_KEY_H,
  I = GLFW_KEY_I,
  J = GLFW_KEY_J,
  K = GLFW_KEY_K,
  L = GLFW_KEY_L,
  M = GLFW_KEY_M,
  N = GLFW_KEY_N,
  O = GLFW_KEY_O,
  P = GLFW_KEY_P,
  Q = GLFW_KEY_Q,
  R = GLFW_KEY_R,
  S = GLFW_KEY_S,
  T = GLFW_KEY_T,
  U = GLFW_KEY_U,
  V = GLFW_KEY_V,
  W = GLFW_KEY_W,
  X = GLFW_KEY_X,
  Y = GLFW_KEY_Y,
  Z = GLFW_KEY_Z,
  _0 = GLFW_KEY_0,
  _1 = GLFW_KEY_1,
  _2 = GLFW_KEY_2,
  _3 = GLFW_KEY_3,
  _4 = GLFW_KEY_4,
  _5 = GLFW_KEY_5,
  _6 = GLFW_KEY_6,
  _7 = GLFW_KEY_7,
  _8 = GLFW_KEY_8,
  _9 = GLFW_KEY_9,
  Escape = GLFW_KEY_ESCAPE,
  Enter = GLFW_KEY_ENTER,
  Tab = GLFW_KEY_TAB,
  Backspace = GLFW_KEY_BACKSPACE,
  Insert = GLFW_KEY_INSERT,
  Delete = GLFW_KEY_DELETE,
  Right = GLFW_KEY_RIGHT,
  Left = GLFW_KEY_LEFT,
  Up = GLFW_KEY_UP,
  Down = GLFW_KEY_DOWN,
  PageUp = GLFW_KEY_PAGE_UP,
  PageDown = GLFW_KEY_PAGE_DOWN,
  Home = GLFW_KEY_HOME,
  End = GLFW_KEY_END,
  F1 = GLFW_KEY_F1,
  F2 = GLFW_KEY_F2,
  F3 = GLFW_KEY_F3,
  F4 = GLFW_KEY_F4,
  F5 = GLFW_KEY_F5,
  F6 = GLFW_KEY_F6,
  F7 = GLFW_KEY_F7,
  F8 = GLFW_KEY_F8,
  F9 = GLFW_KEY_F9,
  F10 = GLFW_KEY_F10,
  F11 = GLFW_KEY_F11,
  F12 = GLFW_KEY_F12,
  F13 = GLFW_KEY_F13,
  F14 = GLFW_KEY_F14,
  F15 = GLFW_KEY_F15,
  F16 = GLFW_KEY_F16,
  F17 = GLFW_KEY_F17,
  F18 = GLFW_KEY_F18,
  F19 = GLFW_KEY_F19,
  F20 = GLFW_KEY_F20,
  F21 = GLFW_KEY_F21,
  F22 = GLFW_KEY_F22,
  F23 = GLFW_KEY_F23,
  F24 = GLFW_KEY_F24,
  F25 = GLFW_KEY_F25,
  KP0 = GLFW_KEY_KP_0,
  KP1 = GLFW_KEY_KP_1,
  KP2 = GLFW_KEY_KP_2,
  KP3 = GLFW_KEY_KP_3,
  KP4 = GLFW_KEY_KP_4,
  KP5 = GLFW_KEY_KP_5,
  KP6 = GLFW_KEY_KP_6,
  KP7 = GLFW_KEY_KP_7,
  KP8 = GLFW_KEY_KP_8,
  KP9 = GLFW_KEY_KP_9,
  KPDecimal = GLFW_KEY_KP_DECIMAL,
  KPDivide = GLFW_KEY_KP_DIVIDE,
  KPMultiply = GLFW_KEY_KP_MULTIPLY,
  KPSubtract = GLFW_KEY_KP_SUBTRACT,
  KPAdd = GLFW_KEY_KP_ADD,
  KPEnter = GLFW_KEY_KP_ENTER,
  KPEqual = GLFW_KEY_KP_EQUAL,
  LeftShift = GLFW_KEY_LEFT_SHIFT,
  LeftControl = GLFW_KEY_LEFT_CONTROL,
  LeftAlt = GLFW_KEY_LEFT_ALT,
  LeftSuper = GLFW_KEY_LEFT_SUPER,
  RightShift = GLFW_KEY_RIGHT_SHIFT,
  RightControl = GLFW_KEY_RIGHT_CONTROL,
  RightAlt = GLFW_KEY_RIGHT_ALT,
  RightSuper = GLFW_KEY_RIGHT_SUPER,
  Menu = GLFW_KEY_MENU,
  Last = GLFW_KEY_LAST
};
inline auto operator<<(std::ostream &os, KeyCode key) -> std::ostream & {
  const auto key_name = glfwGetKeyName(std::to_underlying(key), 0);
  if (key_name) {
    os << key_name;
  } else {
    os << "Unknown Key";
  }
  return os;
}

enum class EventType : std::int8_t {
  None = 0,
  WindowClose,
  WindowMinimize,
  WindowResize,
  WindowFocus,
  WindowLostFocus,
  WindowMoved,
  WindowTitleBarHitTest,
  AppTick,
  AppUpdate,
  AppRender,
  KeyPressed,
  KeyReleased,
  KeyTyped,
  MouseButtonPressed,
  MouseButtonReleased,
  MouseMoved,
  MouseScrolled,
  ScenePreStart,
  ScenePostStart,
  ScenePreStop,
  ScenePostStop,
  EditorExitPlayMode,
  SelectionChanged
};

static constexpr auto bit(std::integral auto x) -> auto { return 1 << x; }

enum class EventCategory : std::int32_t {
  None = 0,
  Application = bit(0),
  Input = bit(1),
  Keyboard = bit(2),
  Mouse = bit(3),
  MouseButton = bit(4),
  Scene = bit(5),
  Editor = bit(6)
};

#define EVENT_CLASS_TYPE(type)                                                 \
  static auto get_static_type() -> EventType { return EventType::type; }       \
  virtual auto get_event_type() const -> EventType override {                  \
    return get_static_type();                                                  \
  }                                                                            \
  virtual auto get_name() const -> std::string_view override {                 \
    return std::string_view{#type};                                            \
  }

#define EVENT_CLASS_CATEGORY(category)                                         \
  virtual auto get_category_flags() const -> std::int32_t override {           \
    return std::to_underlying(EventCategory::category);                        \
  }

class Event {
public:
  bool handled{false};
  virtual auto get_event_type() const -> EventType = 0;
  virtual auto get_name() const -> std::string_view = 0;
  virtual auto get_category_flags() const -> std::int32_t = 0;
  virtual ~Event() = default;
};

template <typename T>
concept EventTypeConcept = std::derived_from<T, Event>;

class EventDispatcher {
public:
  explicit EventDispatcher(Event &e) : event(e) {}

  template <EventTypeConcept Evt, typename Fn>
    requires std::invocable<Fn, Evt &>
  auto dispatch(Fn &&fn) -> bool {
    // 1) Check that the runtime type of event matches the template Evt
    if (event.get_event_type() == Evt::get_static_type() && !event.handled) {
      // 2) Cast to the concrete event and invoke the handler
      auto &concrete = static_cast<Evt &>(event);
      event.handled = fn(concrete);
      return true;
    }
    return false;
  }

private:
  Event &event;
};

class WindowCloseEvent : public Event {
public:
  EVENT_CLASS_TYPE(WindowClose)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowMinimizeEvent : public Event {
public:
  EVENT_CLASS_TYPE(WindowMinimize)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowResizeEvent : public Event {
public:
  WindowResizeEvent(std::int32_t w, std::int32_t h) : width(w), height(h) {}
  std::int32_t width;
  std::int32_t height;
  EVENT_CLASS_TYPE(WindowResize)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowFocusEvent : public Event {
public:
  EVENT_CLASS_TYPE(WindowFocus)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowLostFocusEvent : public Event {
public:
  EVENT_CLASS_TYPE(WindowLostFocus)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowMovedEvent : public Event {
public:
  WindowMovedEvent(std::int32_t x, std::int32_t y) : x_pos(x), y_pos(y) {}
  std::int32_t x_pos;
  std::int32_t y_pos;
  EVENT_CLASS_TYPE(WindowMoved)
  EVENT_CLASS_CATEGORY(Application)
};

// Input events
class KeyPressedEvent : public Event {
public:
  explicit KeyPressedEvent(KeyCode keyCode, std::int32_t repeat = 0)
      : key(keyCode), repeat_count(repeat) {}
  KeyCode key;
  std::int32_t repeat_count;
  EVENT_CLASS_TYPE(KeyPressed)
  EVENT_CLASS_CATEGORY(Keyboard)
};

class KeyReleasedEvent : public Event {
public:
  explicit KeyReleasedEvent(KeyCode keyCode) : key(keyCode) {}
  KeyCode key;
  EVENT_CLASS_TYPE(KeyReleased)
  EVENT_CLASS_CATEGORY(Keyboard)
};

class KeyTypedEvent : public Event {
public:
  explicit KeyTypedEvent(KeyCode key_code) : key(key_code) {}
  KeyCode key;
  EVENT_CLASS_TYPE(KeyTyped)
  EVENT_CLASS_CATEGORY(Keyboard)
};

class MouseButtonPressedEvent : public Event {
public:
  explicit MouseButtonPressedEvent(std::int32_t button) : button(button) {}
  std::int32_t button;
  EVENT_CLASS_TYPE(MouseButtonPressed)
  EVENT_CLASS_CATEGORY(MouseButton)
};

class MouseButtonReleasedEvent : public Event {
public:
  explicit MouseButtonReleasedEvent(std::int32_t button) : button(button) {}
  std::int32_t button;
  EVENT_CLASS_TYPE(MouseButtonReleased)
  EVENT_CLASS_CATEGORY(MouseButton)
};

class MouseMovedEvent : public Event {
public:
  MouseMovedEvent(double x, double y) : x(x), y(y) {}
  double x;
  double y;
  EVENT_CLASS_TYPE(MouseMoved)
  EVENT_CLASS_CATEGORY(Mouse)
};

class MouseScrolledEvent : public Event {
public:
  MouseScrolledEvent(double xOffset, double yOffset)
      : x_offset(xOffset), y_offset(yOffset) {}
  double x_offset;
  double y_offset;
  EVENT_CLASS_TYPE(MouseScrolled)
  EVENT_CLASS_CATEGORY(Mouse)
};

class ScenePreStartEvent : public Event {
  EVENT_CLASS_TYPE(ScenePreStart) EVENT_CLASS_CATEGORY(Scene)
};
class ScenePostStartEvent : public Event {
  EVENT_CLASS_TYPE(ScenePostStart) EVENT_CLASS_CATEGORY(Scene)
};
class ScenePreStopEvent : public Event {
  EVENT_CLASS_TYPE(ScenePreStop) EVENT_CLASS_CATEGORY(Scene)
};
class ScenePostStopEvent : public Event {
  EVENT_CLASS_TYPE(ScenePostStop) EVENT_CLASS_CATEGORY(Scene)
};

class EditorExitPlayModeEvent : public Event {
  EVENT_CLASS_TYPE(EditorExitPlayMode) EVENT_CLASS_CATEGORY(Editor)
};
class SelectionChangedEvent : public Event {
  EVENT_CLASS_TYPE(SelectionChanged) EVENT_CLASS_CATEGORY(Editor)
};