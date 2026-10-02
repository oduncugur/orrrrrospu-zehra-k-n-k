# ZEHRA KINIK - Cince / Japonca: ceviri TSV'sinden src/app/LangCJK.inc, GNU Unifont .hex'ten (OFL 1.1 / GPL + font
# istisnasi) yalniz kullanilan karakterlerin 16x16 gliflerini src/app/CjkGlyphs.inc uretir.
# Kullanim: python tools/gen_cjk_font.py unifont-16.0.01.hex [ceviri.tsv]   (TSV: TURKCE<TAB>CINCE<TAB>JAPONCA)
import sys, os
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
hexf = sys.argv[1]
if len(sys.argv) > 2:
    rows = []
    for line in open(sys.argv[2], encoding='utf-8'):
        f = line.rstrip('\r\n').split('\t')
        if not f[0]: continue
        rows.append((f[0], f[1] if len(f) > 1 else '', f[2] if len(f) > 2 else ''))
    with open(os.path.join(root, 'src/app/LangCJK.inc'), 'w', encoding='utf-8', newline='\n') as o:
        o.write('// ZEHRA KINIK - Cince (basitlestirilmis) ve Japonca kelime tablosu (tools/gen_cjk_font.py ile uretilir).\n')
        o.write('// Bos karsilik: kelime atlanir. Glifler CjkGlyphs.inc (GNU Unifont). Lang.cpp icinde dahil edilir.\n')
        o.write('struct Multi3 { const char* tr; const char* zh; const char* ja; };\nconst Multi3 kMulti3[] = {\n')
        for t, z, j in rows: o.write('    {"%s", "%s", "%s"},\n' % (t, z, j))
        o.write('};\n')
cps = set()
for f in ['src/app/LangCJK.inc', 'src/app/Lang.cpp']:
    for ch in open(os.path.join(root, f), encoding='utf-8').read():
        if ord(ch) >= 0x2E80: cps.add(ord(ch))
glyph = {}
for line in open(hexf, encoding='ascii'):
    k, v = line.strip().split(':')
    cp = int(k, 16)
    if cp in cps and len(v) == 64: glyph[cp] = [int(v[i:i + 4], 16) for i in range(0, 64, 4)]
miss = sorted(cps - set(glyph))
if miss: print('glif yok:', ' '.join('%04X' % c for c in miss))
with open(os.path.join(root, 'src/app/CjkGlyphs.inc'), 'w', encoding='ascii', newline='\n') as o:
    o.write('// ZEHRA KINIK - GNU Unifont 16x16 glif alt kumesi (tools/gen_cjk_font.py ile uretilir; OFL 1.1). Renderer.cpp icinde.\n')
    o.write('const CjkGlyph kCjkGlyphs[] = {\n')
    for cp in sorted(glyph): o.write('    {0x%04X, {%s}},\n' % (cp, ','.join('0x%04X' % r for r in glyph[cp])))
    o.write('};\n')
print(len(glyph), 'glif')
