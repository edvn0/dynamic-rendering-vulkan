#pragma once

#include <concepts>
#include <functional>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

enum class KeyCode : std::int32_t
{
  Unknown = -1,
  Space = 32,
  A = 65,
  B = 66,
  C = 67,
  D = 68,
  E = 69,
  F = 70,
  G = 71,
  H = 72,
  I = 73,
  J = 74,
  K = 75,
  L = 76,
  M = 77,
  N = 78,
  O = 79,
  P = 80,
  Q = 81,
  R = 82,
  S = 83,
  T = 84,
  U = 85,
  V = 86,
  W = 87,
  X = 88,
  Y = 89,
  Z = 90,
  _0 = 48,
  _1 = 49,
  _2 = 50,
  _3 = 51,
  _4 = 52,
  _5 = 53,
  _6 = 54,
  _7 = 55,
  _8 = 56,
  _9 = 57,
  Escape = 256,
  Enter = 257,
  Tab = 258,
  Backspace = 259,
  Insert = 260,
  Delete = 261,
  Right = 262,
  Left = 263,
  Down = 264,
  Up = 265,
  PageUp = 266,
  PageDown = 267,
  Home = 268,
  End = 269,
  F1 = 290,
  F2 = 291,
  F3 = 292,
  F4 = 293,
  F5 = 294,
  F6 = 295,
  F7 = 296,
  F8 = 297,
  F9 = 298,
  F10 = 299,
  F11 = 300,
  F12 = 301,
  F13 = 302,
  F14 = 303,
  F15 = 304,
  F16 = 305,
  F17 = 306,
  F18 = 307,
  F19 = 308,
  F20 = 309,
  F21 = 310,
  F22 = 311,
  F23 = 312,
  F24 = 313,
  F25 = 314,
  KP0 = 320,
  KP1 = 321,
  KP2 = 322,
  KP3 = 323,
  KP4 = 324,
  KP5 = 325,
  KP6 = 326,
  KP7 = 327,
  KP8 = 328,
  KP9 = 329,
  KPDecimal = 330,
  KPDivide = 331,
  KPMultiply = 332,
  KPSubtract = 333,
  KPAdd = 334,
  KPEnter = 335,
  KPEqual = 336,
  LeftShift = 340,
  LeftControl = 341,
  LeftAlt = 342,
  LeftSuper = 343,
  RightShift = 344,
  RightControl = 345,
  RightAlt = 346,
  RightSuper = 347,
  Menu = 348,
  Last = 348
};

enum class MouseCode : std::uint8_t
{
  MouseButton1 = 0,
  MouseButton2 = 1,
  MouseButton3 = 2,
  MouseButton4 = 3,
  MouseButton5 = 4,
  MouseButton6 = 5,
  MouseButton7 = 6,
  MouseButton8 = 7,
  MouseButtonLast = 7,
  MouseButtonLeft = 0,
  MouseButtonRight = 1,
  MouseButtonMiddle = 2
};
auto
operator<<(std::ostream& os, KeyCode key) -> std::ostream&;

enum class EventType : std::int8_t
{
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

static constexpr auto
bit(std::integral auto x) -> auto
{
  return 1 << x;
}

enum class EventCategory : std::int32_t
{
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
  static auto get_static_type() -> EventType                                   \
  {                                                                            \
    return EventType::type;                                                    \
  }                                                                            \
  virtual auto get_event_type() const -> EventType override                    \
  {                                                                            \
    return get_static_type();                                                  \
  }                                                                            \
  virtual auto get_name() const -> std::string_view override                   \
  {                                                                            \
    return std::string_view{ #type };                                          \
  }

#define EVENT_CLASS_CATEGORY(category)                                         \
  virtual auto get_category_flags() const -> std::int32_t override             \
  {                                                                            \
    return std::to_underlying(EventCategory::category);                        \
  }

class Event
{
public:
  bool handled{ false };
  virtual auto get_event_type() const -> EventType = 0;
  virtual auto get_name() const -> std::string_view = 0;
  virtual auto get_category_flags() const -> std::int32_t = 0;
  virtual ~Event() = default;
};

template<typename T>
concept EventTypeConcept = std::derived_from<T, Event>;

class EventDispatcher
{
public:
  explicit EventDispatcher(Event& e)
    : event(e)
  {
  }

  template<EventTypeConcept Evt, typename Fn>
    requires std::invocable<Fn, Evt&>
  auto dispatch(Fn&& fn) -> bool
  {
    if (event.get_event_type() == Evt::get_static_type() && !event.handled) {
      auto& concrete = static_cast<Evt&>(event);
      event.handled = fn(concrete);
      return true;
    }
    return false;
  }

private:
  Event& event;
};

class WindowCloseEvent : public Event
{
public:
  EVENT_CLASS_TYPE(WindowClose)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowMinimizeEvent : public Event
{
public:
  EVENT_CLASS_TYPE(WindowMinimize)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowResizeEvent : public Event
{
public:
  WindowResizeEvent(std::int32_t w, std::int32_t h)
    : width(w)
    , height(h)
  {
  }
  std::int32_t width;
  std::int32_t height;
  EVENT_CLASS_TYPE(WindowResize)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowFocusEvent : public Event
{
public:
  EVENT_CLASS_TYPE(WindowFocus)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowLostFocusEvent : public Event
{
public:
  EVENT_CLASS_TYPE(WindowLostFocus)
  EVENT_CLASS_CATEGORY(Application)
};

class WindowMovedEvent : public Event
{
public:
  WindowMovedEvent(std::int32_t x, std::int32_t y)
    : x_pos(x)
    , y_pos(y)
  {
  }
  std::int32_t x_pos;
  std::int32_t y_pos;
  EVENT_CLASS_TYPE(WindowMoved)
  EVENT_CLASS_CATEGORY(Application)
};

class KeyPressedEvent : public Event
{
public:
  explicit KeyPressedEvent(KeyCode keyCode, std::int32_t repeat = 0)
    : key(keyCode)
    , repeat_count(repeat)
  {
  }
  KeyCode key;
  std::int32_t repeat_count;
  EVENT_CLASS_TYPE(KeyPressed)
  EVENT_CLASS_CATEGORY(Keyboard)
};

class KeyReleasedEvent : public Event
{
public:
  explicit KeyReleasedEvent(KeyCode keyCode)
    : key(keyCode)
  {
  }
  KeyCode key;
  EVENT_CLASS_TYPE(KeyReleased)
  EVENT_CLASS_CATEGORY(Keyboard)
};

class KeyTypedEvent : public Event
{
public:
  explicit KeyTypedEvent(KeyCode key_code)
    : key(key_code)
  {
  }
  KeyCode key;
  EVENT_CLASS_TYPE(KeyTyped)
  EVENT_CLASS_CATEGORY(Keyboard)
};

class MouseButtonPressedEvent : public Event
{
public:
  explicit MouseButtonPressedEvent(std::int32_t button)
    : button(button)
  {
  }
  std::int32_t button;
  EVENT_CLASS_TYPE(MouseButtonPressed)
  EVENT_CLASS_CATEGORY(MouseButton)
};

class MouseButtonReleasedEvent : public Event
{
public:
  explicit MouseButtonReleasedEvent(std::int32_t button)
    : button(button)
  {
  }
  std::int32_t button;
  EVENT_CLASS_TYPE(MouseButtonReleased)
  EVENT_CLASS_CATEGORY(MouseButton)
};

class MouseMovedEvent : public Event
{
public:
  MouseMovedEvent(double x, double y)
    : x(x)
    , y(y)
  {
  }
  double x;
  double y;
  EVENT_CLASS_TYPE(MouseMoved)
  EVENT_CLASS_CATEGORY(Mouse)
};

class MouseScrolledEvent : public Event
{
public:
  MouseScrolledEvent(double xOffset, double yOffset)
    : x_offset(xOffset)
    , y_offset(yOffset)
  {
  }
  double x_offset;
  double y_offset;
  EVENT_CLASS_TYPE(MouseScrolled)
  EVENT_CLASS_CATEGORY(Mouse)
};

class ScenePreStartEvent : public Event
{
  EVENT_CLASS_TYPE(ScenePreStart) EVENT_CLASS_CATEGORY(Scene)
};
class ScenePostStartEvent : public Event
{
  EVENT_CLASS_TYPE(ScenePostStart) EVENT_CLASS_CATEGORY(Scene)
};
class ScenePreStopEvent : public Event
{
  EVENT_CLASS_TYPE(ScenePreStop) EVENT_CLASS_CATEGORY(Scene)
};
class ScenePostStopEvent : public Event
{
  EVENT_CLASS_TYPE(ScenePostStop) EVENT_CLASS_CATEGORY(Scene)
};

class EditorExitPlayModeEvent : public Event
{
  EVENT_CLASS_TYPE(EditorExitPlayMode) EVENT_CLASS_CATEGORY(Editor)
};
class SelectionChangedEvent : public Event
{
  EVENT_CLASS_TYPE(SelectionChanged) EVENT_CLASS_CATEGORY(Editor)
};