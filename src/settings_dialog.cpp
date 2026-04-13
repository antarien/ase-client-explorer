/**
 * @file        settings_dialog.cpp
 * @brief       Implementation for settings_dialog.hpp
 * @description Composes the AdwWindow → ToolbarView → HeaderBar + ViewStack
 *              skeleton and populates three pages with switch / entry rows.
 *              The dialog is purely presentational today; persisting the
 *              selected values is a follow-up task tracked in the project.
 *
 * @module      ase-client-explorer
 * @layer       5
 */

#include <explorer/settings_dialog.hpp>

#include <ase/adw/adw.hpp>

namespace ase::explorer::settings_dialog {

void show(ase::gtk::ApplicationWindow& parent) {
    auto window = ase::adw::Window::create();
    window.set_title("Preferences");
    window.set_default_size(720, 600);
    window.set_transient_for(parent);
    window.set_modal(false);

    auto toolbar = ase::adw::ToolbarView::create();
    auto header  = ase::adw::HeaderBar::create();
    auto stack   = ase::adw::ViewStack::create();

    auto switcher = ase::adw::ViewSwitcher::create();
    switcher.set_stack(stack);
    switcher.set_policy(ase::adw::ViewSwitcherPolicy::Wide);
    header.set_title_widget(switcher);

    toolbar.add_top_bar(header);
    toolbar.set_content(stack);
    window.set_content(toolbar);

    // ── General ──
    auto page_general = ase::adw::PreferencesPage::create();
    page_general.set_title("General");
    page_general.set_icon_name("preferences-system-symbolic");

    auto group_general = ase::adw::PreferencesGroup::create();
    group_general.set_title("File Display");

    auto row_hidden = ase::adw::SwitchRow::create();
    row_hidden.set_title("Show Hidden Files");
    row_hidden.set_active(false);
    group_general.add_switch_row(row_hidden);

    auto row_gitignored = ase::adw::SwitchRow::create();
    row_gitignored.set_title("Show .gitignore'd Files");
    row_gitignored.set_active(false);
    group_general.add_switch_row(row_gitignored);

    page_general.add_group(group_general);
    stack.add_titled_with_icon(page_general, "general", "General", "preferences-system-symbolic");

    // ── Appearance ──
    auto page_appearance = ase::adw::PreferencesPage::create();
    page_appearance.set_title("Appearance");
    page_appearance.set_icon_name("applications-graphics-symbolic");

    auto group_theme = ase::adw::PreferencesGroup::create();
    group_theme.set_title("Theme");

    auto row_compact = ase::adw::SwitchRow::create();
    row_compact.set_title("Compact Mode");
    row_compact.set_active(false);
    group_theme.add_switch_row(row_compact);

    page_appearance.add_group(group_theme);
    stack.add_titled_with_icon(page_appearance, "appearance", "Appearance", "applications-graphics-symbolic");

    // ── Behavior ──
    auto page_behavior = ase::adw::PreferencesPage::create();
    page_behavior.set_title("Behavior");
    page_behavior.set_icon_name("preferences-other-symbolic");

    auto group_behavior = ase::adw::PreferencesGroup::create();
    group_behavior.set_title("Interaction");

    auto row_singleclick = ase::adw::SwitchRow::create();
    row_singleclick.set_title("Single-Click Open");
    row_singleclick.set_active(false);
    group_behavior.add_switch_row(row_singleclick);

    auto row_filewatch = ase::adw::SwitchRow::create();
    row_filewatch.set_title("Live File Watching");
    row_filewatch.set_active(true);
    group_behavior.add_switch_row(row_filewatch);

    auto row_terminal = ase::adw::EntryRow::create();
    row_terminal.set_title("Terminal Emulator");
    row_terminal.set_text("foot");
    group_behavior.add_entry_row(row_terminal);

    page_behavior.add_group(group_behavior);
    stack.add_titled_with_icon(page_behavior, "behavior", "Behavior", "preferences-other-symbolic");

    window.present();
}

}  // namespace ase::explorer::settings_dialog
