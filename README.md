# ctos-x11-lock

X11 lock screen for the [ctOS greeter](https://github.com/TSM-061/ctOS). The upstream locker is a Wayland session lock. This project puts that same page on an X11 session, grabs the keyboard on quickshell's own X connection, and falls back to i3lock if the page never comes up.

The page, artwork, and greeter are TSM-061/ctOS, copyright 2026 TSM-061, GPL-3.0-or-later. This repository is the X11 adapter only, also GPL-3.0-or-later.

ctOS, the ctOS name, and the related branding are trademarks or copyrights of Ubisoft Entertainment. This project is not affiliated with, endorsed by, or supported by Ubisoft.

## What you need

- An installed ctOS greeter (the `greeter/` tree quickshell can import)
- quickshell
- `libxcb` headers to build the grab library
- i3lock, used only when the ctOS page fails to start
- A PAM service named `login` for the account that owns the session

## Build and install

```sh
make
sudo make install PREFIX=$HOME/.local CTOS_ROOT=/opt/ctos
```

`make install` copies:

- `ctos-lock` to `$PREFIX/bin`
- `libctos-x11-lock-grab.so` to `$PREFIX/lib`
- `x11lock.qml` to `$CTOS_ROOT/x11lock.qml`, next to `greeter/`

Apply the account-name fix once, from the ctOS tree:

```sh
patch -p1 -N < /path/to/ctos-x11-lock/patches/lockd-user.patch
```

Upstream `LockdHandler` reads `AuthManager.user`, which is not a property. The patch uses `USER` from the environment instead, and does not start PAM until that name is set.

## i3

```
bindsym $mod+Shift+x exec --no-startup-id "$HOME/.local/bin/ctos-lock"
exec --no-startup-id xss-lock --transfer-sleep-lock --ignore-xss -- "$HOME/.local/bin/ctos-lock"
```

`CTOS_MODE=lockd` selects the settled page. The preload library marks the password window `ctos-lock`, makes it override-redirect, and grabs the keyboard and pointer. Other screens get a cover window.

If the grab is not ready within a few seconds, `ctos-lock` closes quickshell and runs `i3lock --nofork -c 0E0E0E`. A log is written to `~/.cache/ctos-lock.log`.

## License

GNU General Public License v3.0 or later. See `LICENSE`.
