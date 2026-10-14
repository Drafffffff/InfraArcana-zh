// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "config.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "audio.hpp"
#include "audio_data.hpp"
#include "browser.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config_options.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "hints.hpp"
#include "ini.h"
#include "io.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "paths.hpp"
#include "pos.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "terrain_data.hpp"
#include "text.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Owning container of all available options (globally).
static std::vector<std::unique_ptr<config::Option>> s_options {};

// Options shown in the current submenu.
static std::vector<config::Option*> s_current_options {};

static config::OptionSubmenuType s_current_submenu = config::OptionSubmenuType::END;

static std::vector<std::string> s_font_image_names;

static const int s_video_scale_factor_max = 4;

static InputMode s_input_mode = InputMode::standard;
static bool s_is_double_click_toggle_fullscreen = true;
static std::string s_font_name;
static bool s_always_center_view_on_player = false;
static bool s_is_tiles_mode = false;
static RendererType s_renderer_type = RendererType::auto_select;
static bool s_is_fullscreen = false;
static int s_video_scale_factor = 1;
static int s_brightness_pct = 100;
static bool s_text_mode_filled_walls = true;
static bool s_display_health_bars = true;
static bool s_use_trap_color_when_obscured = true;
static bool s_warn_on_throw_valuable = false;
static bool s_warn_on_light_explosive = false;
static bool s_warn_on_drink_malign_potion = false;
static bool s_warn_on_ranged_wpn_melee = false;
static bool s_is_medical_bag_auto_choice = false;
static bool s_is_ranged_wpn_auto_reload = false;
static bool s_is_intro_lvl_skipped = false;
static bool s_is_intro_popup_skipped = false;
static bool s_is_any_key_confirm_more = false;
static bool s_auto_select_menu = false;
static HintsMode s_hints_mode = HintsMode::once;
static bool s_has_seen_hint_global[(size_t)hints::Id::END];
static bool s_always_warn_new_mon = false;
static int s_delay_projectile_draw = -1;
static int s_delay_explosion = -1;
static std::string s_default_player_name;
static bool s_is_bot_playing = false;
static bool s_is_stress_test = false;
static bool s_is_gj_mode = false;
static int s_master_volume_pct_option = 100;
static int s_master_volume_pct_adjusted = 100;
static bool s_is_ambient_audio_enabled = false;
static bool s_is_ambient_audio_preloaded = false;
static int s_audio_buffer_size = 512;
static int s_window_px_w = -1;
static int s_window_px_h = -1;
static int s_gui_cell_px_w = -1;
static int s_gui_cell_px_h = -1;
static int s_map_cell_px_w = -1;
static int s_map_cell_px_h = -1;

static std::string submenu_name(config::OptionSubmenuType submenu_type)
{
        switch (submenu_type) {
        case config::OptionSubmenuType::video:
                return "视频";

        case config::OptionSubmenuType::audio:
                return "音频";

        case config::OptionSubmenuType::input:
                return "输入";

        case config::OptionSubmenuType::gameplay:
                return "游戏玩法";

        case config::OptionSubmenuType::END:
                break;
        }

        PANIC;

        return "";
}

static std::string submenu_name_with_key_shortcut(
        config::OptionSubmenuType submenu_type)
{
        switch (submenu_type) {
        case config::OptionSubmenuType::video:
                return "(V)视频";

        case config::OptionSubmenuType::audio:
                return "(A)音频";

        case config::OptionSubmenuType::input:
                return "(I)输入";

        case config::OptionSubmenuType::gameplay:
                return "(G)游戏玩法";

        case config::OptionSubmenuType::END:
                break;
        }

        PANIC;

        return "";
}

static void filter_current_options(config::OptionSubmenuType submenu_type)
{
        s_current_options.clear();

        for (const auto& option : s_options) {
                if (option->submenu_type() == submenu_type) {
                        s_current_options.push_back(option.get());
                }
        }
}

static P parse_dims_from_font_name(const std::string& font_name)
{
        std::regex pattern(R"(^(\d+)x(\d+))");

        std::smatch match;

        const bool is_name_ok = std::regex_search(font_name, match, pattern);

        if (!is_name_ok) {
                TRACE << "Invalid font name: '" << font_name << "'" << std::endl;

                return {-1, -1};
        }

        int w = std::stoi(match[1].str());
        int h = std::stoi(match[2].str());

        return {w, h};
}

static void find_fonts_from_filesystem()
{
        TRACE_FUNC_BEGIN;

        TRACE << "Finding font images at '" << paths::fonts_dir() << "'" << std::endl;

        for (const auto& entry : std::filesystem::directory_iterator(paths::fonts_dir())) {
                if (!entry.is_regular_file()) {
                        continue;
                }

                // TODO: Should more formats be supported?
                if (entry.path().extension() != ".png") {
                        continue;
                }

                const std::string filename = entry.path().filename().string();

                const P dims = parse_dims_from_font_name(filename);

                if (dims.x == -1) {
                        continue;
                }

                s_font_image_names.push_back(filename);
        }

        std::sort(
                std::begin(s_font_image_names),
                std::end(s_font_image_names),
                [](const std::string& s1, const std::string& s2) {
                        const P dims_1 = parse_dims_from_font_name(s1);
                        const P dims_2 = parse_dims_from_font_name(s2);

                        // Primarily sort by width, secondarily be height.
                        if (dims_1.x == dims_2.x) {
                                // Same width, sort by height.
                                return dims_1.y < dims_2.y;
                        }
                        else {
                                // Different width, sort by width.
                                return dims_1.x < dims_2.x;
                        }
                });

#ifndef NDEBUG
        for (const std::string& font_name : s_font_image_names) {
                const P dims = parse_dims_from_font_name(font_name);

                TRACE
                        << "Found valid font '" << font_name << "'"
                        << " widh dimensions '" << dims.x << "x" << dims.y << "'"
                        << std::endl;
        }
#endif  // NDEBUG

        TRACE_FUNC_END;
}

static void update_render_dims()
{
        TRACE_FUNC_BEGIN;

        if (s_is_tiles_mode) {
                const auto font_dims = parse_dims_from_font_name(s_font_name);

                s_gui_cell_px_w = font_dims.x;
                s_gui_cell_px_h = font_dims.y;
                s_map_cell_px_w = 20;
                s_map_cell_px_h = 20;
        }
        else {
                const auto font_dims = parse_dims_from_font_name(s_font_name);

                s_gui_cell_px_w = s_map_cell_px_w = font_dims.x;
                s_gui_cell_px_h = s_map_cell_px_h = font_dims.y;
        }

        TRACE << "GUI cell size: " << s_gui_cell_px_w << "x" << s_gui_cell_px_h << std::endl;

        TRACE << "Tile size: " << s_map_cell_px_w << "x" << s_map_cell_px_h << std::endl;

        TRACE_FUNC_END;
}

static int calc_default_video_scale_factor(const P& native_res)
{
        // Set the video scale factor based on the user's native resolution.
        // Examples:
        // 1920 x 1080  : 2
        // 2560 x 1440  : 3
        // 3840 x 2160  : 4
        // 7680 x 4320  : 4

        int f = (native_res.x + 500) / 1000;

        f = std::clamp(f, 1, s_video_scale_factor_max);

        TRACE
                << "Calculated a default video scale factor of "
                << "'" << f << "', "
                << "based on native resolution of "
                << "'" << native_res.x << "x" << native_res.y << "' "
                << "(f = (x_resolution + 500) / 1000, "
                << "limited to " << s_video_scale_factor_max << ")"
                << std::endl;

        return f;
}

static void set_default_variables()
{
        TRACE_FUNC_BEGIN;

        s_input_mode = InputMode::standard;

        s_is_double_click_toggle_fullscreen = true;

        // Use the smallest font
        s_font_name = "6x12_fusion.png";

        s_always_center_view_on_player = true;

        s_is_tiles_mode = true;

        update_render_dims();

        const P native_res = io::get_native_resolution();

        s_window_px_w = native_res.x;
        s_window_px_h = native_res.y;

        s_master_volume_pct_option = s_master_volume_pct_adjusted = 100;
        s_is_ambient_audio_enabled = true;
        s_is_ambient_audio_preloaded = false;
        s_audio_buffer_size = 512;
        s_renderer_type = RendererType::auto_select;
        s_is_fullscreen = true;
        s_video_scale_factor = calc_default_video_scale_factor(native_res);
        s_brightness_pct = 100;
        s_text_mode_filled_walls = true;
        s_display_health_bars = true;
        s_use_trap_color_when_obscured = false;
        s_is_intro_lvl_skipped = false;
        s_is_intro_popup_skipped = false;
        s_is_any_key_confirm_more = false;
        s_auto_select_menu = false;
        s_hints_mode = HintsMode::once;
        s_always_warn_new_mon = true;
        s_warn_on_throw_valuable = true;
        s_warn_on_light_explosive = true;
        s_warn_on_drink_malign_potion = true;
        s_warn_on_ranged_wpn_melee = true;
        s_is_medical_bag_auto_choice = false;
        s_is_ranged_wpn_auto_reload = false;
        s_delay_projectile_draw = 25;
        s_delay_explosion = 250;
        s_default_player_name = "";

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                s_has_seen_hint_global[i] = false;
        }

        TRACE_FUNC_END;
}

static bool read_config_file()
{
        TRACE_FUNC_BEGIN;

        mINI::INIFile file(paths::config_file_path() + ".ini");
        mINI::INIStructure ini;

        if (!file.read(ini)) {
                return false;
        }

        auto config = ini["config"];

        s_master_volume_pct_option = to_int(config["master_volume_pct_option"]);
        s_master_volume_pct_adjusted = to_int(config["master_volume_pct_adjusted"]);
        s_is_ambient_audio_enabled = config["is_ambient_audio_enabled"] == "1";
        s_is_ambient_audio_preloaded = config["is_ambient_audio_preloaded"] == "1";
        s_audio_buffer_size = to_int(config["audio_buffer_size"]);
        s_input_mode = (InputMode)to_int(config["input_mode"]);
        s_is_double_click_toggle_fullscreen = config["is_double_click_toggle_fullscreen"] == "1";
        s_window_px_w = to_int(config["window_px_w"]);
        s_window_px_h = to_int(config["window_px_h"]);
        s_is_fullscreen = config["is_fullscreen"] == "1";
        s_video_scale_factor = to_int(config["video_scale_factor"]);

        std::string video_scale_str = config["video_scale_factor"];
        TRACE << "Read video_scale_factor: '" << video_scale_str << "'" << std::endl;

        s_brightness_pct = to_int(config["brightness_pct"]);
        s_renderer_type = (RendererType)to_int(config["renderer_type"]);

        s_always_center_view_on_player = config["always_center_view_on_player"] == "1";
        s_is_tiles_mode = config["is_tiles_mode"] == "1";
        s_font_name = config["font_name"];

        if (
                std::find(
                        std::cbegin(s_font_image_names),
                        std::cend(s_font_image_names),
                        s_font_name) == std::cend(s_font_image_names)) {
                TRACE
                        << "Font name in config file not found in available fonts "
                           "(removed from disk?), reverting to smallest font"
                        << std::endl;

                s_font_name = "6x12_fusion.png";
        }

        update_render_dims();

        s_text_mode_filled_walls = config["text_mode_filled_walls"] == "1";
        s_display_health_bars = config["display_health_bars"] == "1";
        s_use_trap_color_when_obscured = config["use_trap_color_when_obscured"] == "1";
        s_is_intro_lvl_skipped = config["is_intro_lvl_skipped"] == "1";
        s_is_intro_popup_skipped = config["is_intro_popup_skipped"] == "1";
        s_is_any_key_confirm_more = config["is_any_key_confirm_more"] == "1";
        s_auto_select_menu = config["auto_select_menu"] == "1";
        s_hints_mode = (HintsMode)to_int(config["hints_mode"]);
        s_always_warn_new_mon = config["always_warn_new_mon"] == "1";
        s_warn_on_throw_valuable = config["warn_on_throw_valuable"] == "1";
        s_warn_on_light_explosive = config["warn_on_light_explosive"] == "1";
        s_warn_on_drink_malign_potion = config["warn_on_drink_malign_potion"] == "1";
        s_warn_on_ranged_wpn_melee = config["warn_on_ranged_wpn_melee"] == "1";
        s_is_medical_bag_auto_choice = config["is_medical_bag_auto_choice"] == "1";
        s_is_ranged_wpn_auto_reload = config["is_ranged_wpn_auto_reload"] == "1";
        s_delay_projectile_draw = to_int(config["delay_projectile_draw"]);
        s_delay_explosion = to_int(config["delay_explosion"]);

        s_default_player_name = "";

        if (config["default_player_name_set"] == "1") {
                s_default_player_name = config["default_player_name"];
        }

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                s_has_seen_hint_global[i] =
                        config["has_seen_hint_" + std::to_string(i)] == "1";
        }

        TRACE_FUNC_END;

        return true;
}

static void write_config_file()
{
        TRACE_FUNC_BEGIN;

        mINI::INIFile ini_file(paths::config_file_path() + ".ini");
        mINI::INIStructure ini;

        auto& config = ini["config"];

        config["master_volume_pct_option"] = std::to_string(s_master_volume_pct_option);
        config["master_volume_pct_adjusted"] = std::to_string(s_master_volume_pct_adjusted);
        config["is_ambient_audio_enabled"] = s_is_ambient_audio_enabled ? "1" : "0";
        config["is_ambient_audio_preloaded"] = s_is_ambient_audio_preloaded ? "1" : "0";
        config["audio_buffer_size"] = std::to_string(s_audio_buffer_size);
        config["input_mode"] = std::to_string((int)s_input_mode);
        config["s_is_double_click_toggle_fullscreen"] =
                s_is_double_click_toggle_fullscreen ? "1" : "0";
        config["window_px_w"] = std::to_string(s_window_px_w);
        config["window_px_h"] = std::to_string(s_window_px_h);
        config["is_fullscreen"] = s_is_fullscreen ? "1" : "0";
        config["video_scale_factor"] = std::to_string(s_video_scale_factor);
        config["brightness_pct"] = std::to_string(s_brightness_pct);
        config["renderer_type"] = std::to_string((int)s_renderer_type);
        config["always_center_view_on_player"] = s_always_center_view_on_player ? "1" : "0";
        config["is_tiles_mode"] = s_is_tiles_mode ? "1" : "0";
        config["font_name"] = s_font_name;
        config["text_mode_filled_walls"] = s_text_mode_filled_walls ? "1" : "0";
        config["display_health_bars"] = s_display_health_bars ? "1" : "0";
        config["use_trap_color_when_obscured"] = s_use_trap_color_when_obscured ? "1" : "0";
        config["is_intro_lvl_skipped"] = s_is_intro_lvl_skipped ? "1" : "0";
        config["is_intro_popup_skipped"] = s_is_intro_popup_skipped ? "1" : "0";
        config["is_any_key_confirm_more"] = s_is_any_key_confirm_more ? "1" : "0";
        config["auto_select_menu"] = s_auto_select_menu ? "1" : "0";
        config["hints_mode"] = std::to_string((int)s_hints_mode);
        config["always_warn_new_mon"] = s_always_warn_new_mon ? "1" : "0";
        config["warn_on_throw_valuable"] = s_warn_on_throw_valuable ? "1" : "0";
        config["warn_on_light_explosive"] = s_warn_on_light_explosive ? "1" : "0";
        config["warn_on_drink_malign_potion"] = s_warn_on_drink_malign_potion ? "1" : "0";
        config["warn_on_ranged_wpn_melee"] = s_warn_on_ranged_wpn_melee ? "1" : "0";
        config["is_medical_bag_auto_choice"] = s_is_medical_bag_auto_choice ? "1" : "0";
        config["is_ranged_wpn_auto_reload"] = s_is_ranged_wpn_auto_reload ? "1" : "0";
        config["delay_projectile_draw"] = std::to_string(s_delay_projectile_draw);
        config["delay_explosion"] = std::to_string(s_delay_explosion);

        config["default_player_name_set"] = s_default_player_name.empty() ? "0" : "1";

        if (!s_default_player_name.empty()) {
                config["default_player_name"] = s_default_player_name;
        }

        for (size_t i = 0; i < (size_t)hints::Id::END; ++i) {
                config["has_seen_hint_" + std::to_string(i)] =
                        s_has_seen_hint_global[i] ? "1" : "0";
        }

        ini_file.generate(ini);

        TRACE_FUNC_END;
}

static std::vector<std::string> make_user_data_info_lines()
{
        std::string str = "用户数据位置： " + paths::user_dir();

        return text_format::split(str, panels::w(Panel::screen) - 1);
}

static void draw_user_data_info()
{
        std::vector<std::string> lines = make_user_data_info_lines();

        int y = panels::y1(Panel::screen) - (int)lines.size();

        for (size_t i = 0; i < lines.size(); ++i) {
                const std::string& line = lines[i];

                io::draw_text(line, Panel::screen, {1, y}, colors::text());

                ++y;
        }
}

// -----------------------------------------------------------------------------
// Config
// -----------------------------------------------------------------------------
namespace config
{
void init()
{
        //
        // Register options here.
        //
        // NOTE: Options will appear in this order in the game!
        //

        // Video
        s_options.emplace_back(std::make_unique<TilesModeOption>());
        s_options.emplace_back(std::make_unique<FontOption>());
        s_options.emplace_back(std::make_unique<RendererTypeOption>());
        s_options.emplace_back(std::make_unique<FullscreenOption>());
        s_options.emplace_back(std::make_unique<VideoScaleOption>());
        s_options.emplace_back(std::make_unique<BrightnessOption>());
        s_options.emplace_back(std::make_unique<TextModeFilledWallsOption>());

        // Audio
        s_options.emplace_back(std::make_unique<MasterVolumeOption>());
        s_options.emplace_back(std::make_unique<AmbientAudioEnabledOption>());
        s_options.emplace_back(std::make_unique<PreloadAmbientAudioOption>());
        s_options.emplace_back(std::make_unique<AudioBufferSizeOption>());

        // Input
        s_options.emplace_back(std::make_unique<InputModeOption>());
        s_options.emplace_back(std::make_unique<DoubleClickTogglesFullscreenOption>());
        s_options.emplace_back(std::make_unique<AnyKeyConfirmMoreOption>());
        s_options.emplace_back(std::make_unique<AutoSelectMenuOption>());
        s_options.emplace_back(std::make_unique<MedicalBagAutoChoiceOption>());
        s_options.emplace_back(std::make_unique<AutoReloadOption>());

        // Gameplay
        s_options.emplace_back(std::make_unique<AlwaysCenterViewOption>());
        s_options.emplace_back(std::make_unique<SkipIntroLevelOption>());
        s_options.emplace_back(std::make_unique<SkipIntroPopupOption>());
        s_options.emplace_back(std::make_unique<DisplayHintsOption>());
        s_options.emplace_back(std::make_unique<UseTrapColorWhenObscuredOption>());
        s_options.emplace_back(std::make_unique<DisplayHealthBarsOption>());
        s_options.emplace_back(std::make_unique<AlwaysWarnMonsterOption>());
        s_options.emplace_back(std::make_unique<WarnThrowValuableOption>());
        s_options.emplace_back(std::make_unique<WarnLightExplosivesOption>());
        s_options.emplace_back(std::make_unique<WanDrinkMalignPotionOption>());
        s_options.emplace_back(std::make_unique<WarnRangedWeaponMeleeOption>());
        s_options.emplace_back(std::make_unique<ProjectileDelayOption>());
        s_options.emplace_back(std::make_unique<ExplosionDelayOption>());

        // Find available fonts
        find_fonts_from_filesystem();

        // Reset to defaults
        //
        // TODO: Consider how to handle this. Should there be such an option?
        //
        // s_options.emplace_back(std::make_unique<ResetDefaultsOption>());

        s_font_name = "";
        s_is_bot_playing = false;
        s_is_stress_test = false;
        s_is_gj_mode = false;

        set_default_variables();

        // Load config file, if it exists
        const bool did_read_config_file = read_config_file();

        if (!did_read_config_file) {
                // No previous config file exists, create one.
                write_config_file();
        }

        update_render_dims();
}

InputMode input_mode()
{
        return s_input_mode;
}

bool is_double_click_toggle_fullscreen()
{
        return s_is_double_click_toggle_fullscreen;
}

bool always_center_view_on_player()
{
        return s_always_center_view_on_player;
}

bool is_tiles_mode()
{
        return s_is_tiles_mode;
}

std::string font_name()
{
        return s_font_name;
}

RendererType renderer_type()
{
        return s_renderer_type;
}

bool is_fullscreen()
{
        return s_is_fullscreen;
}

int video_scale_factor()
{
        return s_video_scale_factor;
}

int brightness_pct()
{
        return s_brightness_pct;
}

void set_window_px_w(const int w)
{
        s_window_px_w = w;

        write_config_file();
}

void set_window_px_h(const int h)
{
        s_window_px_h = h;

        write_config_file();
}

int window_px_w()
{
        return s_window_px_w;
}

int window_px_h()
{
        return s_window_px_h;
}

int gui_cell_px_w()
{
        return s_gui_cell_px_w;
}

int gui_cell_px_h()
{
        return s_gui_cell_px_h;
}

int map_cell_px_w()
{
        return s_map_cell_px_w;
}

int map_cell_px_h()
{
        return s_map_cell_px_h;
}

bool text_mode_filled_walls()
{
        return s_text_mode_filled_walls;
}

bool display_health_bars()
{
        return s_display_health_bars;
}

bool use_trap_color_when_obscured()
{
        return s_use_trap_color_when_obscured;
}

int master_volume_pct()
{
        return s_master_volume_pct_adjusted;
}

bool is_ambient_audio_enabled()
{
        return s_is_ambient_audio_enabled;
}

bool is_ambient_audio_preloaded()
{
        return s_is_ambient_audio_preloaded;
}

int audio_buffer_size()
{
        return s_audio_buffer_size;
}

bool is_bot_playing()
{
        return s_is_bot_playing;
}

void enable_bot_playing()
{
        s_is_bot_playing = true;
}

void toggle_bot_playing()
{
        s_is_bot_playing = !s_is_bot_playing;
}

bool is_stress_test()
{
        return s_is_stress_test;
}

void enable_stress_test()
{
        s_is_stress_test = true;
}

bool is_gj_mode()
{
        return s_is_gj_mode;
}

void toggle_gj_mode()
{
        s_is_gj_mode = !s_is_gj_mode;
}

bool warn_on_throw_valuable()
{
        return s_warn_on_throw_valuable;
}

bool warn_on_light_explosive()
{
        return s_warn_on_light_explosive;
}

bool warn_on_drink_malign_potion()
{
        return s_warn_on_drink_malign_potion;
}

bool warn_on_ranged_wpn_melee()
{
        return s_warn_on_ranged_wpn_melee;
}

bool is_medical_bag_auto_choice()
{
        return s_is_medical_bag_auto_choice;
}

bool is_ranged_wpn_auto_reload()
{
        return s_is_ranged_wpn_auto_reload;
}

bool is_intro_lvl_skipped()
{
        return s_is_intro_lvl_skipped;
}

bool is_intro_popup_skipped()
{
        return s_is_intro_popup_skipped;
}

bool is_any_key_confirm_more()
{
        return s_is_any_key_confirm_more;
}

bool is_auto_select_menu()
{
        return s_auto_select_menu;
}

HintsMode hints_mode()
{
        return s_hints_mode;
}

bool has_seen_hint_global(const hints::Id id)
{
        if (id == hints::Id::END) {
                ASSERT(false);

                return false;
        }

        return s_has_seen_hint_global[(size_t)id];
}

void set_hint_seen_global(const hints::Id id)
{
        if (id == hints::Id::END) {
                ASSERT(false);
        }

        s_has_seen_hint_global[(size_t)id] = true;

        write_config_file();
}

bool always_warn_new_mon()
{
        return s_always_warn_new_mon;
}

int delay_projectile_draw()
{
        return s_delay_projectile_draw;
}

int delay_explosion()
{
        return s_delay_explosion;
}

void set_default_player_name(const std::string& name)
{
        s_default_player_name = name;

        write_config_file();
}

std::string default_player_name()
{
        return s_default_player_name;
}

void set_fullscreen(const bool value)
{
        s_is_fullscreen = value;

        write_config_file();
}

// -----------------------------------------------------------------------------
// User options
// -----------------------------------------------------------------------------
std::string MasterVolumeOption::name() const
{
        return "音量";
}

std::string MasterVolumeOption::descr() const
{
        return (
                "主音量控制（0-100%）。");
}

std::string MasterVolumeOption::value_str() const
{
        std::string str(11, '-');

        str[s_master_volume_pct_option / 10] = '|';

        str += " " + std::to_string(s_master_volume_pct_option) + "%";

        return str;
}

OptionSubmenuType MasterVolumeOption::submenu_type() const
{
        return OptionSubmenuType::audio;
}

void MasterVolumeOption::change(const OptionChangeCommand command) const
{
        audio::stop_ambient();

        const int step = 10;

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                s_master_volume_pct_option += step;
        }
        else {
                // Left
                s_master_volume_pct_option -= step;
        }

        s_master_volume_pct_option = std::clamp(s_master_volume_pct_option, 0, 100);

        const auto f = ((double)s_master_volume_pct_option) / 100.0;

        s_master_volume_pct_adjusted = (int)((f * f) * 100.0);

        s_master_volume_pct_adjusted = std::clamp(s_master_volume_pct_adjusted, 0, 100);

        TRACE
                << "Volume option: "
                << s_master_volume_pct_option
                << "%, adjusted: "
                << s_master_volume_pct_adjusted
                << "%"
                << std::endl;

        audio::set_music_volume(s_master_volume_pct_adjusted);

        audio::play(audio::SfxId::menu_select);
}

std::string AmbientAudioEnabledOption::name() const
{
        return "播放环境音";
}

std::string AmbientAudioEnabledOption::descr() const
{
        return (
                "开启后，偶尔会播放呼啸的风声、滴水声等环境音效。");
}

std::string AmbientAudioEnabledOption::value_str() const
{
        return s_is_ambient_audio_enabled ? "是" : "否";
}

OptionSubmenuType AmbientAudioEnabledOption::submenu_type() const
{
        return OptionSubmenuType::audio;
}

void AmbientAudioEnabledOption::change(const OptionChangeCommand command) const
{
        (void)command;

        s_is_ambient_audio_enabled = !s_is_ambient_audio_enabled;

        audio::stop_ambient();

        audio::play(audio::SfxId::menu_select);
}

std::string PreloadAmbientAudioOption::name() const
{
        return "预加载环境音";
}

std::string PreloadAmbientAudioOption::descr() const
{
        return (
                "游戏启动时加载所有环境音效，否则在首次播放每种音效时单独加载（可能会有短暂延迟）。");
}

std::string PreloadAmbientAudioOption::value_str() const
{
        return s_is_ambient_audio_preloaded ? "是" : "否";
}

OptionSubmenuType PreloadAmbientAudioOption::submenu_type() const
{
        return OptionSubmenuType::audio;
}

void PreloadAmbientAudioOption::change(const OptionChangeCommand command) const
{
        (void)command;

        s_is_ambient_audio_preloaded = !s_is_ambient_audio_preloaded;
}

std::string AudioBufferSizeOption::name() const
{
        return "音频缓冲区大小";
}

std::string AudioBufferSizeOption::descr() const
{
        return (
                "较低的数值可降低延迟，但在 CPU 较慢的设备上可能导致声音爆裂（音频缓冲区欠载）。\n增大数值可使声音更流畅，但会增加音频延迟。");
}

std::string AudioBufferSizeOption::value_str() const
{
        return std::to_string(s_audio_buffer_size);
}

OptionSubmenuType AudioBufferSizeOption::submenu_type() const
{
        return OptionSubmenuType::audio;
}

void AudioBufferSizeOption::change(const OptionChangeCommand command) const
{
        const Range allowed_range(512, 4096);

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                s_audio_buffer_size <<= 1;
        }
        else {
                // Left
                s_audio_buffer_size >>= 1;
        }

        s_audio_buffer_size =
                std::clamp(
                        s_audio_buffer_size,
                        allowed_range.min,
                        allowed_range.max);

        TRACE
                << "Audio buffer size: "
                << s_audio_buffer_size
                << std::endl;

        io::init_sdl_audio();
}

std::string InputModeOption::name() const
{
        return "输入模式";
}

std::string InputModeOption::descr() const
{
        return (
                "使用默认输入模式（数字键盘或方向键），或使用“Vi 键”。详情请参阅游戏手册。");
}

std::string InputModeOption::value_str() const
{
        switch (s_input_mode) {
        case InputMode::standard:
                return "默认";

        case InputMode::vi_keys:
                return "Vi 键";

        case InputMode::END:
                break;
        }

        return "";

        ASSERT(false);
}

OptionSubmenuType InputModeOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void InputModeOption::change(const OptionChangeCommand command) const
{
        auto input_mode_nr = (int)s_input_mode;

        const auto nr_input_modes = (int)InputMode::END;

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                if (input_mode_nr < (nr_input_modes - 1)) {
                        ++input_mode_nr;
                }
                else {
                        input_mode_nr = 0;
                }
        }
        else {
                // Left
                if (input_mode_nr > 0) {
                        --input_mode_nr;
                }
                else {
                        input_mode_nr = nr_input_modes - 1;
                }
        }

        s_input_mode = (InputMode)(input_mode_nr);
}

std::string DoubleClickTogglesFullscreenOption::name() const
{
        return "双击切换全屏";
}

std::string DoubleClickTogglesFullscreenOption::descr() const
{
        return "双击任意位置切换全屏？";
}

std::string DoubleClickTogglesFullscreenOption::value_str() const
{
        return s_is_double_click_toggle_fullscreen ? "是" : "否";
}

OptionSubmenuType DoubleClickTogglesFullscreenOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void DoubleClickTogglesFullscreenOption::change(const OptionChangeCommand command) const
{
        (void)command;

        s_is_double_click_toggle_fullscreen = !s_is_double_click_toggle_fullscreen;
}

std::string AlwaysCenterViewOption::name() const
{
        return "视角始终居中";
}

std::string AlwaysCenterViewOption::descr() const
{
        return (
                "让视角始终以玩家为中心；关闭时，只有玩家接近视野边缘才会居中。");
}

std::string AlwaysCenterViewOption::value_str() const
{
        return s_always_center_view_on_player ? "是" : "否";
}

OptionSubmenuType AlwaysCenterViewOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void AlwaysCenterViewOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_always_center_view_on_player = !s_always_center_view_on_player;
}

std::string TilesModeOption::name() const
{
        return "使用图块集";
}

std::string TilesModeOption::descr() const
{
        return (
                "使用图块图像表示游戏环境；关闭后仅使用文本符号。");
}

std::string TilesModeOption::value_str() const
{
        return s_is_tiles_mode ? "是" : "否";
}

OptionSubmenuType TilesModeOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void TilesModeOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_tiles_mode = !s_is_tiles_mode;

        update_render_dims();
        io::init_other();
}

std::string FontOption::name() const
{
        return "字体";
}

std::string FontOption::descr() const
{
        return (
                "文本使用的字体。");
}

std::string FontOption::value_str() const
{
        return s_font_name;
}

OptionSubmenuType FontOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void FontOption::change(OptionChangeCommand command) const
{
        // Find current font index
        size_t font_idx = 0;

        const size_t nr_fonts = std::size(s_font_image_names);

        for (; font_idx < nr_fonts; ++font_idx) {
                if (s_font_image_names[font_idx] == s_font_name) {
                        break;
                }
        }

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                if (font_idx < (nr_fonts - 1)) {
                        ++font_idx;
                }
                else {
                        font_idx = 0;
                }
        }
        else {
                // Left
                if (font_idx > 0) {
                        --font_idx;
                }
                else {
                        font_idx = nr_fonts - 1;
                }
        }

        s_font_name = s_font_image_names[font_idx];

        update_render_dims();
        io::init_other();
}

std::string FullscreenOption::name() const
{
        return "全屏";
}

std::string FullscreenOption::descr() const
{
        return (
                "以全屏模式（无边框全屏窗口）运行，或使用可调整大小的窗口。也可按 Alt-Enter 切换。");
}

std::string FullscreenOption::value_str() const
{
        return s_is_fullscreen ? "是" : "否";
}

OptionSubmenuType FullscreenOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void FullscreenOption::change(OptionChangeCommand command) const
{
        (void)command;

        config::set_fullscreen(!s_is_fullscreen);

        io::on_user_toggle_fullscreen();
}

std::string RendererTypeOption::name() const
{
        return "使用硬件加速";
}

std::string RendererTypeOption::descr() const
{
        return (
                "使用自动选择（优先尝试硬件加速，失败时回退到软件渲染），或强制使用软件渲染。如果遇到图像问题，将此选项设为“软件”可能会解决问题。");
}

std::string RendererTypeOption::value_str() const
{
        switch (s_renderer_type) {
        case RendererType::auto_select:
                return "自动选择";

        case RendererType::sw:
                return "软件";

        case RendererType::END:
                break;
        }

        ASSERT(false);

        return "";
}

OptionSubmenuType RendererTypeOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void RendererTypeOption::change(OptionChangeCommand command) const
{
        auto renderer_type_nr = (int)s_renderer_type;
        const auto nr_renderer_types = (int)RendererType::END;

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                if (renderer_type_nr < (nr_renderer_types - 1)) {
                        ++renderer_type_nr;
                }
                else {
                        renderer_type_nr = 0;
                }
        }
        else {
                // Left
                if (renderer_type_nr > 0) {
                        --renderer_type_nr;
                }
                else {
                        renderer_type_nr = nr_renderer_types - 1;
                }
        }

        s_renderer_type = (RendererType)(renderer_type_nr);

        io::init_other();
}

std::string VideoScaleOption::name() const
{
        return "视频缩放系数";
}

std::string VideoScaleOption::descr() const
{
        return "按此系数缩放所有图像。";
}

std::string VideoScaleOption::value_str() const
{
        return std::to_string(s_video_scale_factor) + "x";
}

OptionSubmenuType VideoScaleOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void VideoScaleOption::change(OptionChangeCommand command) const
{
        const int scale_factor_before = s_video_scale_factor;

        switch (command) {
        case OptionChangeCommand::enter: {
                if (s_video_scale_factor < s_video_scale_factor_max) {
                        ++s_video_scale_factor;
                }
                else {
                        s_video_scale_factor = 1;
                }
        } break;

        case OptionChangeCommand::right: {
                if (s_video_scale_factor < s_video_scale_factor_max) {
                        ++s_video_scale_factor;
                }
        } break;

        case OptionChangeCommand::left: {
                if (s_video_scale_factor > 1) {
                        --s_video_scale_factor;
                }
        } break;
        }

        if (s_video_scale_factor != scale_factor_before) {
                io::on_user_toggle_scaling();
        }
}

std::string BrightnessOption::name() const
{
        return "亮度";
}

std::string BrightnessOption::descr() const
{
        return ("调整颜色亮度。");
}

std::string BrightnessOption::value_str() const
{
        std::string str(11, '-');

        str[(s_brightness_pct - 20) / 20] = '|';

        str += " " + std::to_string(s_brightness_pct) + "%";

        return str;
}

OptionSubmenuType BrightnessOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void BrightnessOption::change(const OptionChangeCommand command) const
{
        const int step = 20;

        if ((command == OptionChangeCommand::enter) ||
            (command == OptionChangeCommand::right)) {
                // Enter or right
                s_brightness_pct += step;
        }
        else {
                // Left
                s_brightness_pct -= step;
        }

        s_brightness_pct = std::clamp(s_brightness_pct, 20, 220);

        colors::init();
}

std::string TextModeFilledWallsOption::name() const
{
        return "文本模式墙壁符号";
}

std::string TextModeFilledWallsOption::descr() const
{
        return (
                "文本模式（不使用图块时）中用于表示墙壁的符号。");
}

std::string TextModeFilledWallsOption::value_str() const
{
        return s_text_mode_filled_walls ? "实心矩形" : "井号";
}

OptionSubmenuType TextModeFilledWallsOption::submenu_type() const
{
        return OptionSubmenuType::video;
}

void TextModeFilledWallsOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_text_mode_filled_walls = !s_text_mode_filled_walls;

        // Redefine the terrain data list.
        terrain::init();
}

std::string DisplayHealthBarsOption::name() const
{
        return "显示生命值条";
}

std::string DisplayHealthBarsOption::descr() const
{
        return "生物生命值未满时，在其下方显示生命值条。";
}

std::string DisplayHealthBarsOption::value_str() const
{
        return s_display_health_bars ? "是" : "否";
}

OptionSubmenuType DisplayHealthBarsOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void DisplayHealthBarsOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_display_health_bars = !s_display_health_bars;
}

std::string UseTrapColorWhenObscuredOption::name() const
{
        return "遮挡时使用陷阱颜色";
}

std::string UseTrapColorWhenObscuredOption::descr() const
{
        return (
                "如果陷阱被其他物体（如物品）遮挡，则以该陷阱类型的颜色作为背景色，表示物体下方有陷阱；否则无论陷阱类型如何，都使用相同的背景色。");
}

std::string UseTrapColorWhenObscuredOption::value_str() const
{
        return s_use_trap_color_when_obscured ? "是" : "否";
}

OptionSubmenuType UseTrapColorWhenObscuredOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void UseTrapColorWhenObscuredOption::change(
        OptionChangeCommand command) const
{
        (void)command;

        s_use_trap_color_when_obscured = !s_use_trap_color_when_obscured;
}

std::string SkipIntroLevelOption::name() const
{
        return "跳过开场关卡";
}

std::string SkipIntroLevelOption::descr() const
{
        return (
                "跳过序章关卡。此关卡仅用于交代背景故事和营造氛围；游玩不会获得物品或经验值等收益。");
}

std::string SkipIntroLevelOption::value_str() const
{
        return s_is_intro_lvl_skipped ? "是" : "否";
}

OptionSubmenuType SkipIntroLevelOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void SkipIntroLevelOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_intro_lvl_skipped = !s_is_intro_lvl_skipped;
}

std::string SkipIntroPopupOption::name() const
{
        return "跳过开场弹窗";
}

std::string SkipIntroPopupOption::descr() const
{
        return (
                "跳过序章关卡中的故事弹窗。");
}

std::string SkipIntroPopupOption::value_str() const
{
        return s_is_intro_popup_skipped ? "是" : "否";
}

OptionSubmenuType SkipIntroPopupOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void SkipIntroPopupOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_intro_popup_skipped = !s_is_intro_popup_skipped;
}

std::string AnyKeyConfirmMoreOption::name() const
{
        return "按任意键继续";
}

std::string AnyKeyConfirmMoreOption::descr() const
{
        return (
                "任意键确认“" +
                msg_log::g_more_str +
                "如果在消息记录中启用了“提示”（例如怪物出现时向玩家发出警告），则任何键都可以确认这些提示；否则只有空格（和少数其他按键）可以确认。关闭此选项更安全。");
}

std::string AnyKeyConfirmMoreOption::value_str() const
{
        return s_is_any_key_confirm_more ? "是" : "否";
}

OptionSubmenuType AnyKeyConfirmMoreOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void AnyKeyConfirmMoreOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_any_key_confirm_more = !s_is_any_key_confirm_more;
}

std::string AutoSelectMenuOption::name() const
{
        return "自动选择菜单项";
}

std::string AutoSelectMenuOption::descr() const
{
        return (
                "关闭此选项时，在菜单中按字母键只会跳到对应位置，还需再次按键（或按回车）才能选中（仅适用于有说明的菜单，例如特质选择菜单）。开启后，按字母会立即选中该项。");
}

std::string AutoSelectMenuOption::value_str() const
{
        return s_auto_select_menu ? "是" : "否";
}

OptionSubmenuType AutoSelectMenuOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void AutoSelectMenuOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_auto_select_menu = !s_auto_select_menu;
}

std::string DisplayHintsOption::name() const
{
        return "显示提示";
}

std::string DisplayHintsOption::descr() const
{
        return (
                "控制游戏提示的显示时机。 \n\n{COLOR_LIGHT_WHITE}每局一次：{color_reset}每次游戏中都显示所有提示（其他游戏中也会再次显示）。 \n\n{COLOR_LIGHT_WHITE}仅一次：{color_reset}所有游戏中每条提示只显示一次。 \n\n{COLOR_LIGHT_WHITE}从不：{color_reset}完全关闭提示。");
}

std::string DisplayHintsOption::value_str() const
{
        switch (s_hints_mode) {
        case HintsMode::once_per_game:
                return "每局一次";

        case HintsMode::once:
                return "仅一次";

        case HintsMode::never:
                return "从不";

        case HintsMode::END:
                break;
        }

        ASSERT(false);

        return "";
}

OptionSubmenuType DisplayHintsOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void DisplayHintsOption::change(OptionChangeCommand command) const
{
        (void)command;

        const auto current_idx = (int)s_hints_mode;
        const auto nr_modes = (int)HintsMode::END;

        s_hints_mode = (HintsMode)((current_idx + 1) % nr_modes);

        hints::init();
}

std::string AlwaysWarnMonsterOption::name() const
{
        return "始终提示怪物出现";
}

std::string AlwaysWarnMonsterOption::descr() const
{
        return (
                "视野中出现怪物时始终发出提示（且当前没有看到其他怪物）；否则仅在治疗创伤等长时间行动时提示。");
}

std::string AlwaysWarnMonsterOption::value_str() const
{
        return s_always_warn_new_mon ? "是" : "否";
}

OptionSubmenuType AlwaysWarnMonsterOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void AlwaysWarnMonsterOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_always_warn_new_mon = !s_always_warn_new_mon;
}

std::string WarnThrowValuableOption::name() const
{
        return "投掷贵重物品时警告";
}

std::string WarnThrowValuableOption::descr() const
{
        return (
                "投掷药水等“贵重”物品前发出警告。");
}

std::string WarnThrowValuableOption::value_str() const
{
        return s_warn_on_throw_valuable ? "是" : "否";
}

OptionSubmenuType WarnThrowValuableOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void WarnThrowValuableOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_warn_on_throw_valuable = !s_warn_on_throw_valuable;
}

std::string WarnLightExplosivesOption::name() const
{
        return "点燃爆炸物时警告";
}

std::string WarnLightExplosivesOption::descr() const
{
        return (
                "点燃爆炸物前发出警告");
}

std::string WarnLightExplosivesOption::value_str() const
{
        return s_warn_on_light_explosive ? "是" : "否";
}

OptionSubmenuType WarnLightExplosivesOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void WarnLightExplosivesOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_warn_on_light_explosive = !s_warn_on_light_explosive;
}

std::string WanDrinkMalignPotionOption::name() const
{
        return "饮用有害药水时警告";
}

std::string WanDrinkMalignPotionOption::descr() const
{
        return (
                "饮用已知有害的药水前发出警告。");
}

std::string WanDrinkMalignPotionOption::value_str() const
{
        return s_warn_on_drink_malign_potion ? "是" : "否";
}

OptionSubmenuType WanDrinkMalignPotionOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void WanDrinkMalignPotionOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_warn_on_drink_malign_potion = !s_warn_on_drink_malign_potion;
}

std::string WarnRangedWeaponMeleeOption::name() const
{
        return "用远程武器进行近战时警告";
}

std::string WarnRangedWeaponMeleeOption::descr() const
{
        return (
                "尝试用远程武器（如手枪）进行近战攻击时发出警告；虽然可以这样做，但可能并非本意。");
}

std::string WarnRangedWeaponMeleeOption::value_str() const
{
        return s_warn_on_ranged_wpn_melee ? "是" : "否";
}

OptionSubmenuType WarnRangedWeaponMeleeOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void WarnRangedWeaponMeleeOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_warn_on_ranged_wpn_melee = !s_warn_on_ranged_wpn_melee;
}

std::string MedicalBagAutoChoiceOption::name() const
{
        return "自动选择医疗包操作";
}

std::string MedicalBagAutoChoiceOption::descr() const
{
        return (
                "根据以下优先级自动选择医疗包操作： \n1）治疗感染\n2）治疗创伤\n3）快速包扎\n关闭此选项后，将在弹出菜单中选择操作。");
}

std::string MedicalBagAutoChoiceOption::value_str() const
{
        return s_is_medical_bag_auto_choice ? "是" : "否";
}

OptionSubmenuType MedicalBagAutoChoiceOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void MedicalBagAutoChoiceOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_medical_bag_auto_choice = !s_is_medical_bag_auto_choice;
}

std::string AutoReloadOption::name() const
{
        return "自动装填武器";
}

std::string AutoReloadOption::descr() const
{
        return (
                "尝试用没有装填弹药的远程武器开火时，自动改为装填。");
}

std::string AutoReloadOption::value_str() const
{
        return s_is_ranged_wpn_auto_reload ? "是" : "否";
}

OptionSubmenuType AutoReloadOption::submenu_type() const
{
        return OptionSubmenuType::input;
}

void AutoReloadOption::change(OptionChangeCommand command) const
{
        (void)command;

        s_is_ranged_wpn_auto_reload = !s_is_ranged_wpn_auto_reload;
}

std::string ProjectileDelayOption::name() const
{
        return "弹体延迟（毫秒）";
}

std::string ProjectileDelayOption::descr() const
{
        return (
                "弹体动画每一步的毫秒数。");
}

std::string ProjectileDelayOption::value_str() const
{
        return std::to_string(s_delay_projectile_draw);
}

OptionSubmenuType ProjectileDelayOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void ProjectileDelayOption::change(OptionChangeCommand command) const
{
        const Range allowed_range(0, 900);

        if (command == OptionChangeCommand::enter) {
                // Enter
                query::QueryNumberConfig query_config;

                query_config.allowed_range = allowed_range;
                query_config.default_value = s_delay_projectile_draw;
                query_config.cancel_returns_default = true;

                const int nr =
                        query::number(
                                query_config,
                                "弹体延迟");

                if (nr != -1) {
                        s_delay_projectile_draw = nr;
                }
        }
        else if (command == OptionChangeCommand::left) {
                // Left
                s_delay_projectile_draw -= 10;
        }
        else {
                // Right
                s_delay_projectile_draw += 10;
        }

        s_delay_projectile_draw =
                std::clamp(
                        s_delay_projectile_draw,
                        allowed_range.min,
                        allowed_range.max);
}

std::string ExplosionDelayOption::name() const
{
        return "爆炸延迟（毫秒）";
}

std::string ExplosionDelayOption::descr() const
{
        return (
                "爆炸动画时长（毫秒）。");
}

std::string ExplosionDelayOption::value_str() const
{
        return std::to_string(s_delay_explosion);
}

OptionSubmenuType ExplosionDelayOption::submenu_type() const
{
        return OptionSubmenuType::gameplay;
}

void ExplosionDelayOption::change(OptionChangeCommand command) const
{
        const Range allowed_range(0, 900);

        if (command == OptionChangeCommand::enter) {
                // Enter
                query::QueryNumberConfig query_config;

                query_config.allowed_range = allowed_range;
                query_config.default_value = s_delay_explosion;
                query_config.cancel_returns_default = true;

                const int nr =
                        query::number(
                                query_config,
                                "爆炸延迟");

                if (nr != -1) {
                        s_delay_explosion = nr;
                }
        }
        else if (command == OptionChangeCommand::left) {
                // Left
                s_delay_explosion -= 10;
        }
        else {
                // Right
                s_delay_explosion += 10;
        }

        s_delay_explosion =
                std::clamp(
                        s_delay_explosion,
                        allowed_range.min,
                        allowed_range.max);
}

// std::string ResetDefaultsOption::name() const
// {
//         return "Reset to defaults";
// }

// std::string ResetDefaultsOption::descr() const
// {
//         return (
//                 "");
// }

// std::string ResetDefaultsOption::value_str() const
// {
//         return "";
// }

// OptionSubmenuType ResetDefaultsOption::submenu_type() const
// {
//         return OptionSubmenuType::gameplay;
// }

// void ResetDefaultsOption::change(OptionChangeCommand command) const
// {
//         if (command == OptionChangeCommand::enter) {
//                 set_default_variables();
//                 update_render_dims();
//                 io::init_other();
//                 audio::init();
//                 states::draw();
//                 io::update_screen();
//         }
// }

}  // namespace config

// -----------------------------------------------------------------------------
// Options state
// -----------------------------------------------------------------------------
OptionsState::OptionsState() :
        m_browser((int)config::OptionSubmenuType::END)
{
        m_browser.set_custom_menu_keys({'v', 'a', 'i', 'g'});
}

StateId OptionsState::id() const
{
        return StateId::options;
}

void OptionsState::update()
{
        const io::InputData input = io::read_input();

        const MenuAction action =
                m_browser.read(
                        input,
                        MenuInputMode::scrolling_and_letters,
                        ForceAutoSelect::yes);

        switch (action) {
        case MenuAction::esc:
        case MenuAction::space: {
                states::pop();
                return;
        } break;

        case MenuAction::selected: {
                const auto submenu = (config::OptionSubmenuType)m_browser.y();

                filter_current_options(submenu);

                s_current_submenu = submenu;

                states::push(std::make_unique<OptionsSubmenuState>());
        } break;

        default: {
        } break;
        }
}

void OptionsState::draw()
{
        draw_box(panels::area(Panel::screen));

        io::draw_text_center(
                " 选项 ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        // io::draw_text_center(
        //         std::string(
        //                 " " +
        //                 common_text::g_set_option_hint +
        //                 " " +
        //                 common_text::g_screen_exit_hint +
        //                 " "),
        //         Panel::screen,
        //         {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
        //         colors::title(),
        //         io::DrawBg::yes,
        //         colors::black(),
        //         true);  // Allow pixel-level adjustment

        const auto nr_submenus = (int)config::OptionSubmenuType::END;

        const int x = panels::center_x(Panel::screen) - 4;

        int y = panels::center_y(Panel::screen) - (nr_submenus / 2) - 1;

        for (int i = 0; i < (int)config::OptionSubmenuType::END; ++i) {
                auto submenu_type = config::OptionSubmenuType(i);

                const std::string name =
                        submenu_name_with_key_shortcut(submenu_type);

                const Color& color =
                        (m_browser.is_at_idx((int)i))
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(name, Panel::screen, {x, y}, color);

                ++y;
        }

        draw_user_data_info();
}

// -----------------------------------------------------------------------------
// Options submenu state
// -----------------------------------------------------------------------------
OptionsSubmenuState::OptionsSubmenuState() :
        m_browser((int)s_current_options.size())
{
        m_browser.enable_left_right_keys();
}

StateId OptionsSubmenuState::id() const
{
        return StateId::options_submenu;
}

void OptionsSubmenuState::update()
{
        // Some options play a custom selection audio, so they must disable the
        // one played by the menu browser.
        const bool enable_selection_audio =
                s_current_options[m_browser.y()]
                        ->allow_browser_selection_audio();

        m_browser.set_selection_audio_enabled(enable_selection_audio);

        const io::InputData input = io::read_input();

        const MenuAction action = m_browser.read(input, MenuInputMode::scrolling);

        const config::Option* const option = s_current_options[m_browser.y()];

        bool did_set_option = false;

        switch (action) {
        case MenuAction::esc:
        case MenuAction::space: {
                states::pop();
                return;
        } break;

        case MenuAction::selected: {
                option->change(config::OptionChangeCommand::enter);
                did_set_option = true;
        } break;

        case MenuAction::left: {
                option->change(config::OptionChangeCommand::left);
                did_set_option = true;
        } break;

        case MenuAction::right: {
                option->change(config::OptionChangeCommand::right);
                did_set_option = true;
        } break;

        default: {
        } break;
        }

        if (did_set_option) {
                write_config_file();

                io::clear_input();
        }
}

void OptionsSubmenuState::draw()
{
        draw_box(panels::area(Panel::screen));

        io::draw_text_center(
                " " + submenu_name(s_current_submenu) + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        io::draw_text_center(
                std::string(
                        " " +
                        common_text::g_set_option_hint +
                        " " +
                        common_text::g_screen_exit_hint +
                        " "),
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        int y = 0;

        const int nr_shown = m_browser.nr_items_shown();

        for (int i = 0; i < nr_shown; ++i) {
                const config::Option* const option = s_current_options[i];

                const std::string name = option->name();

                const Color& color =
                        (m_browser.is_at_idx((int)i))
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(
                        name,
                        Panel::options,
                        {0, y},
                        color);

                const std::string value_str = option->value_str();

                if (!value_str.empty()) {
                        // io::draw_text(
                        //         ":",
                        //         Panel::options_values,
                        //         {s_opt_values_x_pos - 2, y},
                        //         color);

                        io::draw_text(
                                value_str,
                                Panel::options_values,
                                {0, y},
                                color);
                }

                ++y;
        }

        const config::Option* const selected_option =
                s_current_options[m_browser.y()];

        const std::string descr_str = selected_option->descr();

        if (!descr_str.empty()) {
                Text text(descr_str);

                text.set_w(panels::w(Panel::options_descr));

                io::draw_text(
                        text,
                        Panel::options_descr,
                        {0, 0},
                        colors::text());
        }
}
