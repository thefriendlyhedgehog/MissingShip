"""The builder's window (MS-154): The Missing Link for SoH: Unbound, built from the player's own files with no command.

Started by build_mod.py when it is run with no arguments (double-clicked) or with --gui. The player picks Unbound, the
OoT USA 1.0 ROM and Kaze's patch (or a Missing Link ROM); the window runs the builder itself, as the command line would
(build_mod.py --unbound <game> --stock --oot-o2r <home>/oot.o2r --out <home>/mods/MissingLink_Unbound.o2r <files>),
and shows its progress. Tk only, so it works offline on Windows, macOS and Linux, everywhere Unbound builds for.

MS_GUI_AUTOTEST names a JSON file ({"unbound": ..., "rom": ..., "patch": ...}) for tests: the window fills those in,
builds at once, writes {"rc": ..., "status": ...} to <file>.result and closes.
"""
import glob, json, os, queue, subprocess, sys, threading, time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

TITLE = 'The Missing Link for SoH: Unbound'
PATCH_PAGE = 'https://www.romhacking.net/hacks/5334/'


def settings_path():
    if sys.platform == 'win32':
        base = os.environ.get('APPDATA') or os.path.expanduser('~')
    elif sys.platform == 'darwin':
        base = os.path.expanduser('~/Library/Application Support')
    else:
        base = os.environ.get('XDG_CONFIG_HOME') or os.path.expanduser('~/.config')
    return os.path.join(base, 'MissingLink Builder', 'settings.json')


def load_settings():
    try:
        with open(settings_path(), encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def save_settings(values):
    try:
        os.makedirs(os.path.dirname(settings_path()), exist_ok=True)
        with open(settings_path(), 'w', encoding='utf-8') as f:
            json.dump(values, f, indent=1)
    except OSError:
        pass                                            # remembering the paths is only a convenience


def mac_apps():
    """Unbound apps in the usual places: an app with SoH's binary and soh.o2r inside."""
    found = []
    for d in ('/Applications', os.path.expanduser('~/Applications')):
        for app in sorted(glob.glob(os.path.join(d, '*.app'))):
            if os.path.isfile(os.path.join(app, 'Contents', 'Resources', 'soh.o2r')) and \
                    os.path.isdir(os.path.join(app, 'Contents', 'MacOS')):
                found.append(app)
    return found


def appimages(folder):
    return sorted(glob.glob(os.path.join(folder, '*.appimage')) + glob.glob(os.path.join(folder, '*.AppImage')))


class Unbound:
    """What the builder needs from the Unbound the player picked: the game to run (--unbound), and the folder it keeps
    oot.o2r and mods/ in. `problem` says what is missing, `extract` names an AppImage still to unpack (Linux)."""

    def __init__(self, bm, path):
        self.path, self.game, self.home, self.problem, self.extract = path, None, None, None, None
        path = os.path.abspath(os.path.expanduser(path)) if path else ''
        if not path or not os.path.exists(path):
            self.problem = 'Choose your SoH: Unbound.'
            return
        if sys.platform == 'darwin':
            app = path
            while app != os.path.dirname(app) and not app.endswith('.app'):
                app = os.path.dirname(app)
            if app.endswith('.app'):
                self.game = app
            elif os.path.isfile(path) and os.path.isfile(os.path.join(os.path.dirname(path), 'soh.o2r')):
                self.game = path                        # a build folder's binary, with soh.o2r beside it
            else:
                self.problem = 'Choose the SoH: Unbound app (the .app).'
                return
            self.home = bm.unbound_home()
        else:
            folder = path if os.path.isdir(path) else os.path.dirname(path)
            if os.path.basename(folder) == 'bin' and folder.endswith(os.path.join('squashfs-root', 'usr', 'bin')):
                folder = os.path.dirname(os.path.dirname(os.path.dirname(folder)))
            self.home = os.environ.get('SHIP_HOME') or folder
            if sys.platform == 'win32':
                self.game = os.path.join(folder, 'soh.exe')
                if not os.path.isfile(self.game):
                    self.problem = 'That folder has no soh.exe: choose the folder SoH: Unbound is in.'
                    return
            else:
                self.game = os.path.join(folder, 'squashfs-root', 'usr', 'bin', 'soh.elf')
                if not os.path.isfile(self.game):
                    images = appimages(folder)
                    if not images:
                        self.problem = 'That folder has no SoH: Unbound AppImage: choose the folder it is in.'
                        return
                    self.extract = images[0]
        version = None if self.extract else bm.unbound_soh_version(self.game)
        if version is not None and version < bm.CODEMOD_MIN_SOH:
            self.problem = (f'This Unbound is built on SoH {".".join(map(str, version))}. The Missing Link needs '
                            'SoH: Unbound on SoH 9.3.0 or later with skijer\'s code mods (NEI Unbound 2.0).')
        elif not os.path.isfile(self.oot_o2r()):
            self.problem = ('Start SoH: Unbound once with your Ocarina of Time USA 1.0 ROM first, so it makes '
                            f'oot.o2r (in {self.home}), then come back.')

    def oot_o2r(self):
        return os.path.join(self.home, 'oot.o2r') if self.home else ''

    def mods(self):
        return os.path.join(self.home, 'mods')


class Window:
    def __init__(self, bm, root):
        self.bm, self.root = bm, root
        self.settings = load_settings()
        self.lines = queue.Queue()
        self.proc = None
        self.started = 0.0
        root.title(TITLE)
        root.minsize(620, 420)
        frame = ttk.Frame(root, padding=14)
        frame.grid(sticky='nsew')
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)
        frame.columnconfigure(1, weight=1)

        ttk.Label(frame, text=TITLE, font=('TkDefaultFont', 14, 'bold')).grid(row=0, column=0, columnspan=3, sticky='w')
        ttk.Label(frame, wraplength=580, justify='left',
                  text='Builds The Missing Link v2.0 as a mod for SoH: Unbound from your own files. Nothing from the '
                       'games comes with this builder. You need SoH: Unbound on SoH 9.3.0 or later with skijer\'s '
                       'code mods, started once with your OoT ROM, and Kaze\'s v2.0 patch from his page, '
                       f'{PATCH_PAGE} (the .zip as downloaded is fine).').grid(
            row=1, column=0, columnspan=3, sticky='w', pady=(4, 12))

        self.unbound = tk.StringVar(value=self.settings.get('unbound', ''))
        self.rom = tk.StringVar(value=self.settings.get('rom', ''))
        self.patch = tk.StringVar(value=self.settings.get('patch', ''))
        if not self.unbound.get() and sys.platform == 'darwin':
            apps = mac_apps()
            if apps:
                self.unbound.set(apps[0])
        rows = [('SoH: Unbound', self.unbound, self.pick_unbound),
                ('OoT USA 1.0 ROM\nor ML v2.0 ROM', self.rom, self.pick_rom),
                ('Kaze\'s patch\n(not with an ML ROM)', self.patch, self.pick_patch)]
        for i, (label, var, pick) in enumerate(rows, start=2):
            ttk.Label(frame, text=label, justify='left').grid(row=i, column=0, sticky='w', padx=(0, 8), pady=3)
            ttk.Entry(frame, textvariable=var).grid(row=i, column=1, sticky='ew', pady=3)
            ttk.Button(frame, text='Choose…', command=pick).grid(row=i, column=2, padx=(8, 0), pady=3)
            var.trace_add('write', lambda *_: self.refresh())

        self.check = ttk.Label(frame, wraplength=580, justify='left', foreground='#a33')
        self.check.grid(row=5, column=0, columnspan=3, sticky='w', pady=(8, 4))
        buttons = ttk.Frame(frame)
        buttons.grid(row=6, column=0, columnspan=3, sticky='w', pady=4)
        self.build_button = ttk.Button(buttons, text='Build The Missing Link', command=self.start)
        self.build_button.grid(row=0, column=0)
        self.open_button = ttk.Button(buttons, text='Open the mods folder', command=self.open_mods)
        self.progress = ttk.Progressbar(frame, mode='indeterminate')
        self.progress.grid(row=7, column=0, columnspan=3, sticky='ew', pady=(8, 2))
        self.status = ttk.Label(frame, text='', wraplength=580, justify='left')
        self.status.grid(row=8, column=0, columnspan=3, sticky='w')
        self.log = tk.Text(frame, height=8, wrap='word', state='disabled', font=('TkFixedFont', 9))
        self.log.grid(row=9, column=0, columnspan=3, sticky='nsew', pady=(6, 0))
        frame.rowconfigure(9, weight=1)
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.refresh()

    # --- choosing files

    def pick_unbound(self):
        start = self.unbound.get() or None
        if sys.platform == 'darwin':                     # the open panel takes an app bundle as a file
            path = filedialog.askopenfilename(title='Choose the SoH: Unbound app', initialdir='/Applications')
        elif sys.platform == 'win32':
            path = filedialog.askdirectory(title='Choose the folder SoH: Unbound is in (with soh.exe)',
                                           initialdir=start)
        else:
            path = filedialog.askdirectory(title='Choose the folder SoH: Unbound\'s AppImage is in', initialdir=start)
        if path:
            self.unbound.set(path)

    def pick_file(self, var, title, types):
        start = os.path.dirname(var.get()) if var.get() else None
        path = filedialog.askopenfilename(title=title, initialdir=start, filetypes=types)
        if path:
            var.set(path)

    def pick_rom(self):
        self.pick_file(self.rom, 'Choose your Ocarina of Time USA 1.0 ROM, or a Missing Link v2.0 ROM',
                       [('N64 ROMs', '*.z64 *.n64 *.v64 *.zip'), ('All files', '*')])

    def pick_patch(self):
        self.pick_file(self.patch, 'Choose Kaze\'s The Missing Link v2.0 patch',
                       [('The patch', '*.zip *.bps'), ('All files', '*')])

    # --- checks

    def current(self):
        return Unbound(self.bm, self.unbound.get())

    def refresh(self):
        if self.proc is not None:
            return
        problem = self.current().problem
        if not problem and not self.rom.get():
            problem = 'Choose your Ocarina of Time USA 1.0 ROM (or a Missing Link v2.0 ROM).'
        self.check.configure(text=problem or '')
        self.build_button.configure(state='disabled' if problem else 'normal')

    # --- building

    def start(self):
        unbound = self.current()
        files = [f for f in (self.rom.get(), self.patch.get()) if f]
        problem = unbound.problem
        if not problem:
            try:
                self.bm.ml_rom(files)                   # a wrong ROM or patch is said at once, not after a minute
            except self.bm.BuildError as e:
                problem = str(e)[:1].upper() + str(e)[1:] + '.'
        if problem:
            self.check.configure(text=problem)
            if os.environ.get('MS_GUI_AUTOTEST'):
                self.finish(2, problem)
            return
        if not os.environ.get('MS_GUI_AUTOTEST'):
            save_settings({'unbound': self.unbound.get(), 'rom': self.rom.get(), 'patch': self.patch.get()})
        self.build_button.configure(state='disabled')
        self.open_button.grid_forget()
        self.check.configure(text='')
        self.write_log('', clear=True)
        self.progress.start(12)
        self.started = time.time()
        threading.Thread(target=self.run, args=(unbound, files), daemon=True).start()
        self.poll()

    def run(self, unbound, files):
        try:
            if unbound.extract:
                self.lines.put(f'Unpacking {os.path.basename(unbound.extract)} (once)…')
                r = subprocess.run([unbound.extract, '--appimage-extract'], cwd=os.path.dirname(unbound.extract),
                                   stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
                if r.returncode != 0 or not os.path.isfile(unbound.game):
                    self.lines.put(('done', 1, 'Unpacking the AppImage failed.'))
                    return
            os.makedirs(unbound.mods(), exist_ok=True)
            args = ['--unbound', unbound.game, '--stock', '--oot-o2r', unbound.oot_o2r(),
                    '--out', os.path.join(unbound.mods(), 'MissingLink_Unbound.o2r'), '--verbose'] + files
            cmd = [sys.executable] + ([] if getattr(sys, 'frozen', False) else [self.bm.__file__]) + args
            flags = 0x08000000 if sys.platform == 'win32' else 0    # CREATE_NO_WINDOW: no console for the steps
            # stdin too: on Windows the freed console's handle is invalid, and inheriting it fails (WinError 6)
            self.proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                         text=True, encoding='utf-8', errors='replace', creationflags=flags,
                                         env=dict(os.environ, MS_LINE_BUFFERED='1'))
            last = ''
            for line in self.proc.stdout:
                line = line.rstrip()
                if line:
                    self.lines.put(line)
                    if line.startswith('MissingShip builder:'):
                        last = line.removeprefix('MissingShip builder:').strip()
            rc = self.proc.wait()
            self.lines.put(('done', rc, last))
        except OSError as e:
            self.lines.put(('done', 1, str(e)))

    def poll(self):
        done = None
        while True:
            try:
                item = self.lines.get_nowait()
            except queue.Empty:
                break
            if isinstance(item, tuple):
                done = item
            else:
                self.write_log(item)
                step = item.removeprefix('MissingShip builder: ').split(' {')[0].split(' (')[0].split(' [')[0]
                self.status.configure(text=f'Building ({int(time.time() - self.started)} s): {step[:100]}')
        if done:
            self.finish(*done[1:])
        else:
            self.root.after(150, self.poll)

    def finish(self, rc, last):
        self.proc = None
        self.progress.stop()
        took = int(time.time() - self.started) if self.started else 0
        if rc == 0:
            text = f'Done in {took} s. The Missing Link is in Unbound\'s mods folder: start SoH: Unbound to play.'
            self.open_button.grid(row=0, column=1, padx=(8, 0))
        else:
            text = f'The build failed: {last or "see the log below"}'
        self.status.configure(text=text)
        self.refresh()
        result = os.environ.get('MS_GUI_AUTOTEST')
        if result:
            with open(result + '.result', 'w', encoding='utf-8') as f:
                json.dump({'rc': rc, 'status': text}, f)
            self.root.after(1500, self.root.destroy)
        elif rc != 0:
            messagebox.showerror(TITLE, text)

    def write_log(self, line, clear=False):
        self.log.configure(state='normal')
        if clear:
            self.log.delete('1.0', 'end')
        if line:
            self.log.insert('end', line + '\n')
            self.log.see('end')
        self.log.configure(state='disabled')

    def open_mods(self):
        folder = self.current().mods()
        if sys.platform == 'win32':
            os.startfile(folder)                        # noqa: an Explorer window on the player's own folder
        else:
            subprocess.Popen(['open' if sys.platform == 'darwin' else 'xdg-open', folder])

    def close(self):
        if self.proc is not None and not messagebox.askyesno(TITLE, 'The build is still running. Stop it and quit?'):
            return
        if self.proc is not None:
            self.proc.kill()
        self.root.destroy()


def main(bm):
    """The window; bm is build_mod (the running module, frozen or not)."""
    if sys.platform == 'win32':                         # double-clicked: drop the console window Windows opened
        import ctypes
        console = sys.stdout is not None and sys.stdout.isatty()
        ctypes.windll.kernel32.FreeConsole()
        if console:                                     # its handles are gone with it: a print would now fail
            try:
                os.makedirs(os.path.dirname(settings_path()), exist_ok=True)
                sys.stdout = sys.stderr = open(os.path.join(os.path.dirname(settings_path()), 'builder.log'), 'w',
                                               encoding='utf-8', buffering=1)
            except OSError:
                sys.stdout = sys.stderr = open(os.devnull, 'w')
    root = tk.Tk()
    window = Window(bm, root)
    test = os.environ.get('MS_GUI_AUTOTEST')
    if test:
        with open(test, encoding='utf-8') as f:
            values = json.load(f)
        window.unbound.set(values.get('unbound', ''))
        window.rom.set(values.get('rom', ''))
        window.patch.set(values.get('patch', ''))
        root.after(500, window.start)
    root.mainloop()
