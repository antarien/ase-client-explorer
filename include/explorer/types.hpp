#pragma once

/**
 * ASE MODULE TYPES (SSOT)
 *
 * @file        types.hpp
 * @brief       Single Source of Truth for ase-client-explorer constants
 * @description Compile-time constants only - runtime state lives in the window and view
 *              objects. Drei Werte: die Fenstergeometrie beim Start und der Pfad, mit dem
 *              der Explorer oeffnet, wenn ihm keiner mitgegeben wird.
 *
 *              WARUM DIESE DATEI EINEN KOPF BEKOMMT UND SONST NICHTS: sie erfuellte die
 *              SACHE der Checkliste schon vorher — keine Structs, kein enum class, jede
 *              Konstante mit Inline-Kommentar, keine Magic Number im Code. Es fehlte
 *              ausschliesslich die FORM. Am 2026-08-22 waren das 17 der 64 Befunde des
 *              Moduls, und keiner davon hat einen Wert, einen Namen oder ein Verhalten
 *              beruehrt.
 *
 *              DEFAULT_ROOT IST EIN ABSOLUTER PFAD AUF DIESE MASCHINE, und das ist eine
 *              bewusste Grenze, kein Versehen: der Explorer ist das Werkzeug des
 *              Betreibers auf seinem eigenen Baum. Wer ihn woanders startet, gibt den
 *              Wurzelpfad als Argument mit — dafuer ist der Vorgabewert der Rueckfall,
 *              nicht die einzige Quelle.
 *
 * @module      ase-client-explorer
 * @layer       5 (Client)
 * @created     2026-04-12
 * @modified    2026-08-22
 * @version     1.0.0
 *
 * ECS TYPES COMPLIANCE
 *
 * [ ] All constants defined (no magic numbers in code)
 * [ ] Every constant has inline comment (English, explains purpose)
 * [ ] NO enum class (only constexpr uint8_t for enumeration values)
 * [ ] Type aliases defined
 * [ ] InvalidEntityId = UINT32_MAX defined (if needed)
 * [ ] Abbreviations documented
 * [ ] NO structs (structs belong in Components)
 *
 * ABBREVIATIONS
 *
 * EXP = Explorer (client prefix)
 */

#include <cstdint>

namespace ase::explorer {

/// Default window dimensions (narrow portrait, like IDE sidebar)
constexpr int DEFAULT_WIDTH  = 380;
constexpr int DEFAULT_HEIGHT = 900;

/// Default root path when no argument given
constexpr const char* DEFAULT_ROOT = "/mnt/code/SRC/GITHUB/ase";

}  // namespace ase::explorer
