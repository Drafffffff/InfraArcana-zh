"""Idempotent post-catalog fixes for Unicode name entry and Steam text-input UI."""
from pathlib import Path
R=Path(__file__).resolve().parents[1]/'source'
def replace(rel,old,new):
 p=R/rel;s=p.read_text()
 if new in s:return
 if old not in s:raise RuntimeError(f'Patch context missing: {rel}: {old[:60]}')
 p.write_text(s.replace(old,new))
def apply():
 replace('include/io.hpp','        int key {-1};','        int key {-1};\n        std::string text; // Complete UTF-8 SDL text event, for name entry.')
 replace('src/io_input.cpp','#include "io.hpp"','#include "io.hpp"\n#include "utf8.hpp"')
 replace('src/io_input.cpp','        const auto c = s_sdl_event.text.text[0];','''        const std::string entered = s_sdl_event.text.text;
        if (utf8::has_unicode(entered)) {
                for (size_t p = 0; p < entered.size();) {
                        const auto g = utf8::next(entered, p);
                        if (g.cp == 0xfffd || g.cp < 32) return;
                        p = g.end;
                }
                io::clear_input();
                s_input.key = -1;
                s_input.text = entered;
                s_is_done_reading_input = true;
                return;
        }
        const auto c = s_sdl_event.text.text[0];''')
 replace('src/create_character.cpp','        if (m_current_str.size() < g_player_name_max_len) {','''        if (!input.text.empty()) {
                for (size_t p = 0; p < input.text.size();) {
                        const auto g = utf8::next(input.text, p);
                        const auto part = input.text.substr(p, g.end - p);
                        if (g.cp != 0xfffd && utf8::columns(g.cp) > 0 &&
                            utf8::width(m_current_str) + utf8::columns(g.cp) <= g_player_name_max_len)
                                m_current_str += part;
                        p = g.end;
                }
                return;
        }
        if (utf8::width(m_current_str) < g_player_name_max_len) {''')
 replace('src/create_character.cpp','m_current_str.erase(m_current_str.end() - 1);','utf8::pop_back(m_current_str);')
 replace('src/create_character.cpp','if ((m_current_str.size() < g_player_name_max_len) &&','if ((utf8::width(m_current_str) < g_player_name_max_len) &&')
if __name__=='__main__':apply()
