#pragma once

/**
 * @file        exclude.hpp
 * @brief       Directory-entry exclusion rules for the tree view
 * @description Centralised list of filenames and prefix patterns that the tree
 *              view must hide: build directories, editor caches, VCS metadata,
 *              and OS junk files. The filter predicate is used by the adapter's
 *              FilterListModel around every DirectoryList in the tree.
 *
 * @module      ase-client-explorer
 * @layer       5
 *
 * Copyright (c) Jan Ohlmann. All rights reserved.
 * Licensed to ANTARES SIMULACAO LTDA. Use is governed by LICENSE.md and PJV.md in this repository.
 * SPDX-License-Identifier: LicenseRef-ASE-LIC
 */

#include <string>

namespace ase::explorer::exclude {

/** Returns true if a directory entry with this basename should be hidden. */
bool should_exclude(const std::string& name);

}  // namespace ase::explorer::exclude
