// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>

#include "SDL.h"
#include <cstdlib>
#include <fstream>
#include <unordered_set>
#include "colors.hpp"
#include "utf8.hpp"
#include "config.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "panel.hpp"
#include "pos.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void draw_text_at_px(
        const std::string& str,
        P px_pos,
        const Color& color,
        const io::DrawBg draw_bg,
        const Color& bg_color)
{
        static std::unordered_set<std::string> seen;
        if (const char* trace_path = std::getenv("IA_ZH_TRACE")) {
                if (seen.insert(str).second) {
                        std::ofstream trace(trace_path, std::ios::app);
                        for (char c : str) {
                                if (c == '\n') trace << "\\n";
                                else if (c == '\r') trace << "\\r";
                                else trace << c;
                        }
                        trace << '\n';
                }
        }
        if ((px_pos.y < 0) || (px_pos.y >= panel_px_h(Panel::screen))) {
                return;
        }

        const int cw = config::gui_cell_px_w();
        const int ch = config::gui_cell_px_h();
        const int sw = panel_px_w(Panel::screen);
        const int sf = config::video_scale_factor();
        const bool fits = px_pos.x + static_cast<int>(utf8::width(str))*cw <= sw;
        for (size_t p = 0; p < str.size();) {
                auto glyph = utf8::next(str, p); p = glyph.end;
                if (px_pos.x < 0 || px_pos.x >= sw) return;
                if (!fits && px_pos.x + 5*cw >= sw) {
                        for (int i=0; i<5 && px_pos.x < sw; ++i) {
                                draw_character_at_px("(...)"[i], px_pos, colors::gray(), draw_bg, bg_color);
                                px_pos.x += cw;
                        }
                        return;
                }
                const int cols = utf8::columns(glyph.cp);
                if (glyph.cp < 128) {
                        draw_character_at_px(static_cast<char>(glyph.cp), px_pos, color, draw_bg, bg_color);
                } else if (cols > 0) {
                        if (draw_bg == DrawBg::yes)
                                draw_rectangle_filled({px_pos, px_pos + P(cols*cw, ch)-1}, bg_color);
                        const uint32_t cp = glyph.cp <= 0xffff ? glyph.cp : 0xfffd;
                        SDL_Rect src {static_cast<int>(cp%256)*12, static_cast<int>(cp/256)*12, 12, 12};
                        P dest = px_pos.scaled_up(sf).with_offsets(g_rendering_px_offset);
                        // The atlas is square. A narrow non-ASCII glyph occupies one ASCII cell.
                        SDL_Rect dst {dest.x, dest.y, cols*cw*sf, ch*sf};
                        const auto adapted = color.with_brightness(config::brightness_pct());
                        SDL_SetTextureColorMod(g_unicode_texture, adapted.r(), adapted.g(), adapted.b());
                        SDL_RenderCopy(g_sdl_renderer, g_unicode_texture, &src, &dst);
                }
                px_pos.x += cols*cw;
        }
}

void draw_text(
        Text text,
        const Panel panel,
        P pos,
        Color color,
        const DrawBg draw_bg,
        const Color& bg_color)
{
        text.set_color(color);

        const P origin_pos = pos;

        for (const TextAction& action : text.actions()) {
                switch (action.id) {
                case TextActionId::write_str: {
                        const P px_pos = gui_to_px_coords(panel, pos);

                        draw_text_at_px(
                                action.str,
                                px_pos,
                                color,
                                draw_bg,
                                bg_color);

                        pos.x += (int)utf8::width(action.str);
                } break;

                case TextActionId::newline: {
                        ++pos.y;
                        pos.x = origin_pos.x;
                } break;

                case TextActionId::change_color: {
                        color = action.color;
                } break;

                case TextActionId::done: {
                        return;
                } break;
                }
        }
}

void draw_text_center(
        const std::string& str,
        const Panel panel,
        const P pos,
        const Color& color,
        const DrawBg draw_bg,
        const Color& bg_color,
        const bool is_pixel_pos_adj_allowed)
{
        const int len = (int)utf8::width(str);
        const int len_half = len / 2;
        const int x_pos_left = pos.x - len_half;

        P px_pos = gui_to_px_coords(panel, {x_pos_left, pos.y});

        if (is_pixel_pos_adj_allowed) {
                const int pixel_x_adj =
                        ((len_half * 2) == len)
                        ? (config::gui_cell_px_w() / 2)
                        : 0;

                px_pos += P(pixel_x_adj, 0);
        }

        draw_text_at_px(str, px_pos, color, draw_bg, bg_color);
}

void draw_text_right(
        const std::string& str,
        const Panel panel,
        const P pos,
        const Color& color,
        const DrawBg draw_bg,
        const Color& bg_color)
{
        const int x_pos_left = pos.x - (int)utf8::width(str) + 1;

        P px_pos = gui_to_px_coords(panel, {x_pos_left, pos.y});

        draw_text_at_px(str, px_pos, color, draw_bg, bg_color);
}

}  // namespace io
