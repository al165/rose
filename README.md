# rose

A small collection of useful pd objects.

Requires **Pd 0.55 or newer**.

## List of objects

- `[display-trace]`: displays message bursts (multi-line) with timeout before resetting.

## Layout

```
myui/
├── Makefile            # pd-lib-builder wrapper
├── *.c                 # the externals
├── *-help.pd           # help files
├── README.md
├── rose_test_patch.pd  # testing ground for objects
└── pd-lib-builder/     # git submodule (build system)
```

## Build

```bash
git clone https://github.com/pure-data/pd-lib-builder.git
make
```

If `m_pd.h` isn't on the default search path, point the build at it:

```bash
make PDINCLUDEDIR=/usr/include/pd            # distro headers
make PDDIR=/path/to/pure-data                # a Pd source tree
```

To generate `compile_commands.json`, runs `$ bear -- make`

## Install

```bash
make install objectsdir=~/.local/lib/pd/extra
```

Then in Pd: _Preferences → Path_ should include `~/.local/lib/pd/extra`, and
`~/.local/lib/pd/extra/myui` will be found automatically.

To test without installing, just open a patch saved in this directory — Pd
searches the patch's own folder first.

## License

Do as you like. The Pd headers themselves are BSD-3-Clause.
