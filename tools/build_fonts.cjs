'use strict';
// Regenerate after adding or editing Chinese strings in main/ui_text.h.
const fs = require('node:fs');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const root = path.resolve(__dirname, '..');
const fontDir = path.join(root, 'managed_components/lvgl__lvgl/scripts/built_in_font');
const source = path.join(fontDir, 'SourceHanSansSC-Normal.otf');
const converter = path.join(__dirname, 'font-tool/node_modules/lv_font_conv/lv_font_conv.js');
const header = fs.readFileSync(path.join(root, 'main/ui_text.h'), 'utf8');
const strings = [...header.matchAll(/"([^"\n]*)"/g)].map(match => match[1]).join('');
const allCharacters = [...new Set(strings)].sort().join('');
for (const [size, symbols] of [[20, allCharacters], [28, '兰洛']]) {
  const output = path.join(root, `main/pet_font_zh_${size}.c`);
  execFileSync(process.execPath, [converter, '--size', String(size), '--bpp', '4',
    '--format', 'lvgl', '--font', source, '--symbols', symbols, '--no-compress',
    '--no-kerning', '--lv-include', 'lvgl.h', '-o', output], {stdio: 'inherit'});
  const generated = fs.readFileSync(output, 'utf8');
  const glyphs = new Set([...generated.matchAll(/U\+([0-9a-f]+)/gi)].map(match => parseInt(match[1], 16)));
  for (const character of symbols) {
    if (!glyphs.has(character.codePointAt(0))) throw new Error(`Missing glyph: ${character}`);
  }
  console.log(`${size}px: ${[...symbols].length} glyphs verified`);
}
const licenses = path.join(root, 'assets/fonts');
fs.mkdirSync(licenses, {recursive: true});
fs.copyFileSync(path.join(fontDir, 'font_license/SourceHanSansSC/LICENSE.txt'), path.join(licenses, 'SourceHanSansSC-LICENSE.txt'));
