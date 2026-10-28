# Install and use

This locks an X11 session with the ctOS greeter page. The greeter itself comes from [TSM-061/ctOS](https://github.com/TSM-061/ctOS). Commands below assume a Debian-family system and i3. The shell expands `$HOME` before `sudo`, so the program lands in your home directory, not in `/root`.

## 1. Install the greeter

Clone ctOS and run its installer so the greeter tree is on disk. The default location this adapter expects is `/opt/ctos`, with `greeter/` inside it.

```sh
git clone https://github.com/TSM-061/ctOS.git
cd ctOS
./install.sh
```

A lock screen does not need greetd. Skip the display-manager setup unless you also want the ctOS login page at boot.

The page looks for JetBrains Mono. On Debian:

```sh
sudo apt install fonts-jetbrains-mono
```

## 2. Install build and lock packages

```sh
sudo apt install build-essential libxcb1-dev quickshell i3lock xss-lock
```

On Debian 13, quickshell is in `trixie-backports`. If apt cannot find it, enable that suite and install it from there. You need a quickshell build that can open X11 windows and talk to PAM.

## 3. Build and install this adapter

```sh
git clone https://github.com/Kikyo987/ctos-x11-lock.git
cd ctos-x11-lock
make
sudo make install PREFIX="$HOME/.local" CTOS_ROOT=/opt/ctos
```

That copies:

| File | Destination |
| --- | --- |
| `ctos-lock` | `$HOME/.local/bin/ctos-lock` |
| `libctos-x11-lock-grab.so` | `$HOME/.local/lib/libctos-x11-lock-grab.so` |
| `x11lock.qml` | `/opt/ctos/x11lock.qml` |

`x11lock.qml` has to sit next to `greeter/`. If your greeter lives somewhere else, pass that directory as `CTOS_ROOT`.

Put `$HOME/.local/bin` on `PATH` if it is not already. i3 can still call the script by its full path.

## 4. Patch the account name

Upstream `LockdHandler` reads `AuthManager.user`. That property does not exist, so PAM would start with an empty account and the page would never ask for a password. Apply the patch once, to the tree quickshell actually loads:

```sh
sudo make patch-lockd CTOS_ROOT=/opt/ctos
```

The patch is `patches/lockd-user.patch`. It uses `USER` from the environment and waits until that name is set. If the file was already patched, `patch` skips it.

## 5. Bind it in i3

Add this to `~/.config/i3/config`. `$mod` is Mod4 on a normal i3 setup, so the key is Super+Shift+x.

```
bindsym $mod+Shift+x exec --no-startup-id "$HOME/.local/bin/ctos-lock"
exec --no-startup-id xss-lock --transfer-sleep-lock --ignore-xss -- "$HOME/.local/bin/ctos-lock"
```

Reload the config so the key binding is picked up:

```sh
i3-msg reload
```

`reload` does not start programs that were added with `exec`. For the current session, start the suspend hook once:

```sh
xss-lock --transfer-sleep-lock --ignore-xss -- "$HOME/.local/bin/ctos-lock" &
```

The next login starts it from the i3 config. This project does not lock the screen on idle.

## 6. Lock and unlock

Press Super+Shift+x, or run:

```sh
"$HOME/.local/bin/ctos-lock"
```

Type the password of the account that owns the session, then confirm. That is the same password PAM's `login` service accepts. A wrong password stays on the page.

Closing the lid or suspending the machine runs the same locker through `xss-lock` before sleep. On more than one monitor, the password page is on the greeter's chosen screen, or the first screen. The others show the cover image.

A log is appended to `~/.cache/ctos-lock.log`.

## If the page does not appear

The script falls back to `i3lock` with the color `0E0E0E` when the grab is not ready in about five seconds, or when quickshell exits with an error. The desktop is not left unlocked.

Check the log first:

```sh
tail -n 40 ~/.cache/ctos-lock.log
```

Typical causes:

- `x11lock.qml` is not next to `greeter/`, or `CTOS_ROOT` points at the wrong directory.
- The account-name patch was applied to a copy of ctOS that quickshell is not loading.
- `libctos-x11-lock-grab.so` is missing. Rebuild with `make` and install again.
- `USER` is empty in the environment that starts the locker.

## If you cannot get back to the desktop

Switch to a text console with Ctrl+Alt+F2 and sign in there. Stop the lock process, not only the wrapper. Stopping the wrapper alone can leave quickshell holding the keyboard.

```sh
pkill -f 'quickshell --path .*/x11lock.qml'
pkill -f 'bin/ctos-lock'
pkill -x i3lock
```

Then return to the graphical session with Ctrl+Alt+F7, or whichever VT your X server is on.

## Paths you can override

| Variable | Default | Role |
| --- | --- | --- |
| `CTOS_ROOT` | `/opt/ctos` | Greeter tree. `x11lock.qml` is placed here. |
| `PREFIX` | `$HOME/.local` | Install prefix for the script and the library. |
| `CTOS_LOCK_LIB` | `$HOME/.local/lib/libctos-x11-lock-grab.so` | Grab library used at runtime. |
| `QUICKSHELL` | `/usr/bin/quickshell` | quickshell binary. |
| `I3LOCK` | `/usr/bin/i3lock` | Fallback locker. |
