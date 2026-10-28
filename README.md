# ctos-x11-lock

An X11 lock screen that shows the [ctOS greeter](https://github.com/TSM-061/ctOS) page.

Upstream ctOS can already lock a Wayland session. Its locker is a Wayland session lock, so it does not run on X11. This project is the missing piece for an X11 desktop such as i3: the same page, a keyboard grab on quickshell's own X connection, and i3lock as a fallback if that page never appears.

Install and day-to-day use are in [INSTALL.md](INSTALL.md).

## What it is

Three small parts sit on top of an installed ctOS greeter:

- `qml/x11lock.qml` opens the greeter's lock page in an X11 window. The password screen is upstream `MainLayout`. Extra monitors get a cover.
- `src/grab.c` is preloaded into that process. Qt talks to X through libxcb on one connection, so the keyboard grab has to happen there. A second connection would take the grab and swallow keystrokes.
- `bin/ctos-lock` starts the page, waits until the grab is held, and stays in the foreground until the password is accepted. `xss-lock` and a key binding both run this one script.

`CTOS_MODE=lockd` selects the lock page. The account name comes from `USER`. Authentication uses the PAM service `login`. A wrong password stays on the page.

If the lock window is not ready within a few seconds, the script closes quickshell and runs `i3lock` with a plain dark screen, so a failed start does not leave the desktop open.

## What it is not

This repository does not contain the greeter, its artwork, or a display manager. Those stay in [TSM-061/ctOS](https://github.com/TSM-061/ctOS), copyright 2026 TSM-061, GPL-3.0-or-later. You install that first, then add this adapter.

It does not replace greetd, and it does not lock the machine when the session is idle. A key binding locks on demand. `xss-lock` locks before suspend.

ctOS, the ctOS name, and the related branding are trademarks or copyrights of Ubisoft Entertainment. This project is not affiliated with, endorsed by, or supported by Ubisoft.

## License

GNU General Public License v3.0 or later. See [LICENSE](LICENSE). The adapter and the greeter it builds on are both GPL-3.0-or-later.
