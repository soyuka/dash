# Keypad input

dash runs without a touchscreen: a Renault 1657 keypad arrives as a keyboard
through `kbd-bridge` (uinput). Every key goes through one dispatcher,
`ActionEventFilter`:

1. a key bound to an **available** action fires it (press, release);
2. otherwise `KeypadNavigator` uses it to move around dash's own UI;
3. otherwise it reaches the focused widget as usual.

## Bindings

Bindings are stored by action id in `dash.conf`:

```ini
[Core]
Bindings\cycle_page=F5
Bindings\aa_enter=
```

An empty value unbinds the action. An action without a stored entry gets its
default, unless its default key is already bound to another action.

Upstream openDsh stored `Core/Action\<index>`, in an order that shifted when
the camera page was removed. On start, dash rewrites those entries to ids
using the upstream order (camera included) and deletes the `Core/Action`
group. The camera page binding is dropped.

| Keypad | Key    | Action id            | Behaviour                                         |
|--------|--------|----------------------|---------------------------------------------------|
| OK     | Return | `aa_enter`           | Android Auto in front, otherwise navigation       |
| BACK   | Esc    | `aa_back`            | idem                                              |
| joystick | Up/Down/Left/Right | `aa_up` `aa_down` `aa_left` `aa_right` | idem          |
| wheel - / + | PgUp / PgDown | `aa_scroll_up` / `aa_scroll_down` | AA rotary, otherwise scroll or change the focused value |
| TEL    | F1     | `aa_phone`           | always sent to Android Auto                       |
| MAP    | F2     | `aa_home`            | shows the Android Auto page, then acts as home    |
| ROUTE  | F3     | `aa_voice`           | always sent to Android Auto                       |
| INFO   | F4     | `aa_toggle_play`     | always sent to Android Auto                       |
| DEST   | F5     | `cycle_page`         |                                                   |
| LIGHT  | F6     | `toggle_dark_mode`   |                                                   |
| ZOOM   | F7     | `toggle_fullscreen`  |                                                   |
| SET    | F8     | `show_page_settings` |                                                   |

"Android Auto in front" means the Android Auto page is current, the
projection is running and no dialog is open. The wheel stays on scrolling
rather than volume: the Pi audio goes through the radio, whose own volume
control is the one in use.

Other ids: `aa_call_end`, `aa_play`, `aa_pause`, `aa_prev_track`,
`aa_next_track`, `decrease_brightness`, `increase_brightness`,
`decrease_volume`, `increase_volume`, `show_page_<page id>` where the page id
is its name in lower case with `_` for spaces (`android_auto`, `media`,
`vehicle`, `launcher`, `settings`).

## Navigation

Three zones: the rail (page buttons), the page content (everything else in
the window, control bar included) and, when open, the topmost dialog.

| Key        | Rail                         | Content                                  | Dialog          |
|------------|------------------------------|------------------------------------------|-----------------|
| Up/Down    | previous/next page button, wraps | nearest widget in that direction     | same as content |
| Left/Right | Right opens the page         | nearest widget; Left at the edge goes to the rail | same, no exit |
| Return     | opens the page, focuses its first widget | clicks a button, opens a combo box, else the widget gets it | same |
| Esc        | nothing                      | back to the rail (leaves fullscreen)     | closes it, focus returns to the opener |
| PgUp/PgDown | previous/next page button   | slider page step, spin box, combo box, tab, list row; else scrolls the enclosing scroll area | same |

The first key with nothing focused lands on the current page's rail button.
Lists and tab bars keep the arrows along their axis until their first or last
item, then the arrow moves focus out. The focused widget gets a 3 logical px
ring (scaled with `Layout/scale`) in the theme colour, drawn while the keypad
is in use and hidden on mouse or touch input or while Android Auto is in front.

## Page contract

A page gets keypad navigation without code when:

- every interactive control is a focusable widget (`focusPolicy` with
  `Qt::TabFocus`, the default for Qt buttons, sliders, combo boxes, lists and
  tab bars); decorative buttons use `Session::Forge::symbolize` (no focus);
- buttons act on `clicked` (Return calls `click()`), lists on `activated`;
- value widgets derive from `QAbstractSlider`, `QAbstractSpinBox` or
  `QComboBox`; the wheel moves a slider by its `pageStep`, so set it to the
  step one notch should make;
- dialogs are `Dialog` instances (focus moves in on `open()` and back on close).

A custom widget that needs more:

- Return and the wheel reach it as `Qt::Key_Return` and
  `Qt::Key_PageDown` ("+") / `Qt::Key_PageUp` ("-") key presses unless it is
  one of the types above;
- set the dynamic property `keypad_arrows` to `true` to receive the arrow
  keys itself (for instance a tuner stepping the frequency with Left/Right);
  it then has no arrow way out, so leave Esc to the navigator.
