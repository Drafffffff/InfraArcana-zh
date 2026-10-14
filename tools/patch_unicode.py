from pathlib import Path
r=Path(__file__).resolve().parents[1]/'source'
def edit(path, changes):
 p=r/path;s=p.read_text()
 for a,b in changes:
  assert a in s,(path,a)
  s=s.replace(a,b)
 p.write_text(s)
edit('src/text.cpp',[
 ('#include "text.hpp"','#include "text.hpp"\n#include "utf8.hpp"'),
 ('current_action.str.size()','utf8::width(current_action.str)'),
 ('fwd_action.str.size()','utf8::width(fwd_action.str)'),
 ('action.str.size()','utf8::width(action.str)'),
 ('if (current_action.str != " ") {','if (current_action.str != " " && !utf8::has_unicode(current_action.str)) {'),
 ('if (fwd_action.id == TextActionId::write_str) {','if (utf8::has_unicode(fwd_action.str)) break;\n                        if (fwd_action.id == TextActionId::write_str) {'),
 ("        const auto starting_c = m_raw_str[pos];","        const auto starting_c = m_raw_str[pos];\n\n        if (static_cast<unsigned char>(starting_c) >= 0x80) {\n                const auto g = utf8::next(m_raw_str, pos);\n                return {m_raw_str.substr(pos, g.end-pos), g.end};\n        }"),
 ("if ((next_c == ' ') || (next_c == '\\n') || (next_c == '{')) {","if ((next_c == ' ') || (next_c == '\\n') || (next_c == '{') ||\n                    (static_cast<unsigned char>(next_c) >= 0x80 && starting_c != '{')) {")
])
edit('src/text_format.cpp',[
 ('#include "text_format.hpp"','#include "text_format.hpp"\n#include "utf8.hpp"'),
 ('current_string.size() + word_to_fit.size()', 'utf8::width(current_string) + utf8::width(word_to_fit)'),
 ('tot_w > str.size()', 'tot_w > utf8::width(str)'),
 ('tot_w - str.size()', 'tot_w - utf8::width(str)'),
 ('tolower(result[0])','static_cast<unsigned char>(result[0]) < 128 ? tolower(result[0]) : result[0]'),
 ('toupper(result[0])','static_cast<unsigned char>(result[0]) < 128 ? toupper(result[0]) : result[0]'),
 ('::toupper);','[](unsigned char c) { return c < 128 ? std::toupper(c) : c; });'),
 ('::isspace);','[](unsigned char c) { return c < 128 && std::isspace(c); });'),
 ('::isspace).base();','[](unsigned char c) { return c < 128 && std::isspace(c); }).base();'),
 ('        if (line.empty()) {\n                return {};\n        }\n\n        std::string current_word', '''        if (utf8::has_unicode(line)) {
                std::vector<std::string> out;
                std::string row;
                size_t w = 0;
                for (size_t p = 0; p < line.size();) {
                        const auto g = utf8::next(line, p);
                        if (g.cp == '\\n') { out.push_back(row); row.clear(); w = 0; p = g.end; continue; }
                        if (w + utf8::columns(g.cp) > static_cast<size_t>(std::max(1, max_w))) {
                                out.push_back(row); row.clear(); w = 0;
                                if (g.cp == ' ') { p = g.end; continue; }
                        }
                        row += line.substr(p, g.end-p); w += utf8::columns(g.cp); p = g.end;
                }
                if (!row.empty()) out.push_back(row);
                return out;
        }
        if (line.empty()) {
                return {};
        }

        std::string current_word''')
])
for name,exprs in {
 'msg_log.cpp':['msg_log::g_more_str','str','text'],
 'popup.cpp':['msg_lines[0]','m_title','common_text::g_confirm_hint','common_text::g_cancel_hint','choice_str'],
 'create_character.cpp':['key_str','label','prereq_title.str'],
 'main_menu.cpp':['last_line'],
 'game_over_summary.cpp':['ev_str'],
}.items():
 p=r/'src'/name;s=p.read_text().replace('#include <cstddef>','#include <cstddef>\n#include "utf8.hpp"')
 if '#include "utf8.hpp"' not in s:s='#include "utf8.hpp"\n'+s
 for expr in exprs:
  for fn in ['size','length']:s=s.replace(expr+'.'+fn+'()',f'utf8::width({expr})')
 p.write_text(s)
