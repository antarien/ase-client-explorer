#pragma once

/**
 * @file        theme.hpp
 * @brief       Dark-mode CSS generation for the explorer window
 * @description Builds the CSS that styles the window, header bar, list view,
 *              popovers, buttons and entries. Colours come from the generated
 *              ase::colors SSOT (sha-web-styles/src/colors.ts) so the desktop
 *              theme matches the web console 1:1. The CSS is installed for the
 *              default display at application startup.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <string>

namespace ase::explorer::theme {

/** Builds the full CSS stylesheet for the explorer's dark theme. */
std::string generate_css();

}  // namespace ase::explorer::theme
