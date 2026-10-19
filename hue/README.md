# Hue lights in The Collar

`zacos9-collar-hue` puts your Philips Hue lights in The Collar, ZacOS's
control strip: a **Hue** module, then one module for each room or zone you
choose.

The **Hue** module's menu has:
- **Turn Off All Lights** (the bridge's all-lights group).
- **Show in Control Strip**: every room and zone from the Hue app, together
  by name, checked if it has its own module. Choose one to add it; choose it
  again to remove it. None show until you choose them.

Its bulb glows with all the lit lights, and its tooltip counts them, such as
"Hue: 2 lights on".

A room's icon is a bulb lit with the room's current colour. The colour is the
average of its lit lights, and dims with brightness down to half strength.
The bulb is grey when the room is off. The tooltip gives the state, such as
"Living Room: on, 60%".

A room's menu has:
- **On / Off**, with the current state checked.
- **Brightness**: 25, 50, 75 and 100%. A level also turns the room on.
- **Scenes**: the room's scenes from the Hue app, with the active one checked.

Changes made anywhere else (the Hue app, switches, automations) appear right
away through the bridge's event stream.

## Setting up

1. Install it with `scripts/install-user.sh hue`. It starts now and at every login.
2. A **Hue** module appears in The Collar. It finds the bridge by mDNS
   (`_hue._tcp`, through `avahi-browse`), or else through Philips's discovery
   service.
3. Press the round link button on top of the bridge. Then, within 30 seconds,
   choose **Connect to Hue Bridge** from the Hue module's menu.
4. Choose the rooms you want from the Hue module's **Show in Control Strip**
   list. They appear after the Hue module, by name.

**Look for Bridges Again** forgets the bridge and searches again.

## Details

- Bridge API: CLIP v2 (`/clip/v2/resource`, `/eventstream/clip/v2`). It needs
  a v2 bridge (square, BSB002), with firmware from 2021 or later.
- Settings: `~/.config/zacos9/hue.conf`, `[bridge]` `address`, `id`, `key`. The
  key is the bridge's application key, so keep this file private. `[strip]`
  `rooms` lists the IDs of the rooms and zones you chose.
- TLS: the bridge signs its own certificate with its bridge ID as the subject
  CN. The extension accepts the certificate only when the CN matches the ID
  found by discovery.
- On/off, brightness and scenes use one command for the whole room.
- If the bridge stops answering, the module says so and retries every 5 s.
  Unpairing (401/403) returns to the Connect step.
- D-Bus: owns `org.zacos9.CollarExtension.Hue`. See "Extensions" in
  `~/ZacOS/docs/collar.md`.

## Tests

`meson test -C build hue` runs `tests/test_hue.py` on a private session bus.
It uses `tests/fake_bridge.py`, a fake bridge served over plain HTTP that has
two rooms, a zone, scenes and an event stream. The test covers:
- pairing, with and without the button;
- module order, icons and menus;
- choosing rooms and zones to show, and saving the choices;
- every command and the requests it sends;
- updates from the event stream;
- the bridge going away.
