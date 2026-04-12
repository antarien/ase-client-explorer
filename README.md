# ase-client-explorer

[![Layer](https://img.shields.io/badge/Layer-5%20Client-purple.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()
[![GTK4](https://img.shields.io/badge/UI-GTK4%2Fgtkmm-green.svg)]()
[![AUR](https://img.shields.io/badge/Package-AUR-blue.svg)]()

> Standalone hierarchical project explorer with NerdFont file icons and drag & drop

Part of [ASE - Antares Simulation Engine](../../..)

## Overview

Native GTK4 desktop application that displays the ASE project structure as a hierarchical
file tree with language-specific NerdFont icons, drag & drop support, and xdg-open integration.

## Features

- Hierarchical tree view of the entire ASE project structure
- NerdFont icons per file type (CLion/Atom quality)
- Drag & drop files to other apps (full absolute paths)
- Double-click opens files with associated application
- Right-click context menu (Copy Path, Open With, Open in Terminal)
- Live file watching (auto-refresh on filesystem changes)
- Fuzzy search / filter
- Submodule detection with layer badges
- VERSION status indicators per submodule
- ASE color scheme (dark theme, colors.hpp SSOT)

## Usage

```bash
ase exp -B -R                     # Build and run
ase exp -R                        # Run (default: ASE root)
ase exp -R /path/to/project/      # Run with custom root
ase-explorer                      # Direct (default: ASE root)
ase-explorer /path/to/project/    # Direct with path
```

## Build

```bash
cmake -B build -G Ninja && ninja -C build
# or
./build.sh
```

## Dependencies

- gtkmm-4.0 (GTK4 C++ bindings)
- libadwaita-1 (GNOME adaptive widgets)
- ttf-fira-code (monospace font)
- ttf-nerd-fonts-symbols-mono (file icons)

## AUR

```bash
cd packaging && makepkg -si
```
