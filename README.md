# ase-client-explorer

**Design:** DSGN_066 (Browser-App), DSGN_067 (Adaptive UXD)

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

## License

Copyright (c) Jan Ohlmann. All rights reserved.
Licensed to ANTARES SIMULACAO LTDA (CNPJ 69.363.366/0001-21) under ASE-JAN-LTDA-2026-10-16-1.0.

This repository is released under the Antares Simulation Engine and ADG License Agreement
**ASE-ADG-LIC-2026-10-16-1.0** ([LICENSE.md](LICENSE.md)) and the ADG Project Participation
Agreement **ASE-ADG-PJV-2026-10-16-1.0** ([PJV.md](PJV.md)). Contribution workflow: [CONTRIBUTING.md](CONTRIBUTING.md).

Translations: [DE LIC](license/de/LIC_ASE_ADG_DE.md) · [DE PJV](license/de/PJV_ASE_ADG_DE.md) ·
[PT LIC](license/pt/LIC_ASE_ADG_PT.md) · [PT PJV](license/pt/PJV_ASE_ADG_PT.md)

SPDX-License-Identifier: LicenseRef-ASE-LIC

Third-party components keep their own licenses; see [NOTICE](NOTICE).
