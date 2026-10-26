# A turntable in The Collar

`zacos9-collar-turntable` plays a USB turntable, such as Audio-Technica's
AT-LP120-USB, through ZacOS's sound output. You switch it on and off from a
**Turntable** module in The Collar, ZacOS's control strip.

The module's menu has:
- **On / Off**, with the current state checked.
- **Input**: the turntable's USB input. With more than one USB input
  plugged in (a webcam's microphone, say), a list to choose it from.
- **Output**: where it plays, which is the output chosen in the Sound
  control panel. It follows that output when you change it.

Its icon is a record seen from above. While it plays, the label is red and
the tone arm is on the record. When it's off, the arm is at rest. The icon
is grey while the turntable isn't plugged in. The tooltip gives the state,
such as "Turntable: on".

Left on, it plays again whenever the turntable appears: after logging in,
after the sound server restarts, or when you plug the turntable back in.
Left off, it stays off.

## Setting up

1. Install it from **Software** (Apple menu > New Tricks > Extras), or with
   `scripts/install-user.sh turntable` from this tree.
2. Plug the turntable in with its USB cable. A **Turntable** module appears
   in The Collar.
3. Choose **On**. Start the record and turn the volume up in the Sound control
   panel or The Collar's Sound module.

## Details

- It plays through a PipeWire loopback, made with `pactl` (in the
  `pulseaudio-utils` package):
  `pactl load-module module-loopback source=INPUT latency_msec=100
  source_dont_move=true`, with the stream named "Turntable". It names no
  output, so the sound server plays it through the current one.
- `source_dont_move=true` ends the loopback when the turntable is unplugged.
  Without it, PipeWire moves the loopback to the next input, which is the
  built-in microphone, and the room plays through the speakers.
- The input: the one you chose. Until you choose one, it's the only USB
  input, or else the only TI/Burr-Brown PCM29xx codec (USB vendor 0x08bb,
  product 0x29xx), which most USB turntables use. It never plays a different
  input in its place.
- A loopback from the same input that you started by hand counts as on, and
  Off stops it too.
- Settings: `~/.config/zacos9/turntable.conf`, `[turntable]`: `input`,
  `listening`, and `latency` in milliseconds (default 100; set it by hand).
- Quitting (logging out, or removing the package) stops the record.
- It follows the sound server through `pactl subscribe`, and checks again
  every 2 s while the server is down.
- D-Bus: owns `org.zacos9.CollarExtension.Turntable`. See "Extensions" in
  `~/ZacOS/docs/collar.md`.

## Tests

`meson test -C build turntable` runs `tests/test_turntable.py` on a private
session bus, with `tests/fake_pactl.py` first on `PATH`. The stand-in sound
server is modelled on the iMac this was written on: HDMI to a Sony TV, the
built-in microphone, the turntable's PCM2900 codec, and a loopback started
by hand. The test covers:
- the module, its menu and icon pixels, on and off;
- the loopback's arguments, and the settings remembered;
- the sound output changing;
- unplugging the turntable and plugging it back in;
- the sound server stopping and coming back;
- choosing between two USB inputs, and never playing the other one in the
  turntable's place;
- a loopback that can't be made, and one that ended on its own;
- quitting, which stops the record.
