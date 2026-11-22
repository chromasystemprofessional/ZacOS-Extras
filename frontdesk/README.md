# Front Desk

A shared-computer launcher for ZacOS 9, in the spirit of the classic
tabbed classroom shells. Front Desk is a **session shell**: for the accounts
an administrator chooses, it starts instead of the Sniffer and shows only the
programs and documents that account may use.

Front Desk is not another account system. ZacOS's real Unix accounts are the
users, the login window is the welcome screen, and the existing **Use only
chosen programs** allowlist is the list of programs on the panel.

## The model

| Front Desk | ZacOS |
|---|---|
| User | A Unix account, selected at the login window |
| Welcome screen | The Front Desk-skinned login window |
| Front Desk Items | The Applications folder's user-installed programs, chosen in the account's malcontent allowlist |
| User documents | `~/Documents` |
| “Go To Front Desk Setup” | `zacos9-frontdesk-setup`, also **Control Panels ▸ Front Desk Setup** |
| Quit | Log Out, back to the login window |
| Restricted Finder | Puppy Mode |
| Guest | The existing Guest account, emptied at each login |

There is no Front Desk password, user database or account creation. The
Front Desk Setup edits the same allowlist the Users panel uses, through
accountsservice; it authenticates with the administrator's password through
polkit.

## What is in the extra

- `zacos9-frontdesk` — the panel shell.
- `zacos9-frontdesk-setup` — the administrator's setup window, with the At
  Ease Setup layout: users list, per-user programs, menus, speech, save
  location, Finder access and screen captures.
- **Front Desk Setup in Control Panels** — the setup can be opened from an
  ordinary session, without Front Desk running, from Control Panels, the Zac
  menu's Control Panels, or the panel's Special menu.
- Original pixel art in the same layout as the reference: a 575×400 panel,
  folder tabs, 48 px buttons, labels, page arrows, and a ZacOS Platinum/teal
  palette. No Apple artwork, names or sounds.
- Original ZacOS sounds when the tabs, page arrows and icons are clicked:
  the woodblock click for tabs and pages, the chirp for opening an item, and
  the alert chime for an action that couldn't be done. **Special ▸ Turn Sound
  Off** silences them.

## Session behaviour

For an account with Front Desk enabled, `zacos9-session` sets
`ZACOS9_SHELL=frontdesk` and `ZACOS9_MENUBAR_MODE=frontdesk`:

- the menu bar starts with a minimal Zac menu;
- `zacos9-frontdesk` starts instead of the Sniffer, and The Collar stays
  visible (and its hot key works);
- the Front Desk panel fills the usable screen area behind the menu bar;
- an app launches on a single click; the panel waits in the Application
  menu and ⌘-Tab, and only the program in front draws, as on a shared Mac;
- closing the last program returns to the panel;
- Quit logs out, Restart and Shut Down use the normal power paths.

The login window shows the Front Desk welcome for Front Desk accounts, with an
original illustration, the user list and a Start button.

## Settings

`zacos9-accounts-helper desktop USER frontdesk|puppy|finder` chooses the
startup mode. `frontdesk USER on|off` enables or disables Front Desk and
creates `/etc/zacos9/frontdesk/USER`. The file holds:

```
menus=minimal|restricted|all
save=anywhere|user
save-removable=on|off
finder=on|off
speech=on|off
capture=on|off
```

`zacos9-accounts-helper frontdesk-settings USER KEY=VALUE…` writes validated
settings. Only an administrator can change them. The program list is the
account's existing malcontent allowlist, not a Front Desk file.

## Build and test

```sh
scripts/build.sh                 # from the extras repository root
meson test -C build frontdesk
```

The panel can be previewed from a normal ZacOS session with
`zacos9-frontdesk --window`; that path is a normal 575×400 window and does not
replace the session.
