# rose

A small collection of useful [PureData](https://puredata.info/) objects.

Requires **Pd 0.55 or newer**. Built with [pd-lib-builder](https://github.com/pure-data/pd-lib-builder).

## List of objects

- `[display-trace]`: displays message bursts (multi-line) with timeout before resetting.
- `[state]`: displays the latest messages of the form `[key values...(` and updates
  previous entries

## Layout

```directory
rose/
├── pd-lib-builder/     # git submodule (build system)
├── Makefile            # pd-lib-builder wrapper
├── *.c                 # the externals
├── *-help.pd           # help files
├── rose.c
├── rose_common.c       # common functions and defines
├── README.md
└── rose_test_patch.pd  # testing ground for objects
```

## Build

```bash
git clone --recursive https://github.com/al165/rose.git
cd rose
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
make install objectsdir=$HOME/.local/lib/pd/extra
```

Then in Pd: _Preferences -> Path_ should include `~/.local/lib/pd/extra`, and
`~/.local/lib/pd/extra/rose` will be found automatically.

To test without installing, just open a patch saved in this directory — Pd
searches the patch's own folder first.

## License

MIT (c) 2026 Arran Lyon
