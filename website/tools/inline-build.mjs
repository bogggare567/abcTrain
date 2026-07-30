// Folds `vite build`'s output into one self-contained HTML file.
//
// Why this exists: the shareable preview is served under a strict CSP that
// blocks every external host - no CDN scripts, no font files, no separate
// stylesheet. Inlining is the only way a preview can be the *real* build
// rather than a hand-written copy of it that drifts the moment anyone
// edits a component.
//
// Usage:  node tools/inline-build.mjs [outputFile]

import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { join, dirname, extname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');
const dist = join(root, 'dist');
const out = process.argv[2] ?? join(dist, 'abctrain-preview.html');

const asDataUri = (file, mime) =>
  `data:${mime};base64,${readFileSync(file).toString('base64')}`;

const assets = readdirSync(join(dist, 'assets'));
const jsFile = assets.find((f) => extname(f) === '.js');
const cssFile = assets.find((f) => extname(f) === '.css');

if (!jsFile || !cssFile) {
  console.error('No built JS/CSS found in dist/assets — run `npm run build` first.');
  process.exit(1);
}

let css = readFileSync(join(dist, 'assets', cssFile), 'utf8');

// Point every @font-face at an embedded copy. Source Code Pro ships under
// the SIL Open Font License 1.1, which permits embedding.
for (const face of ['SourceCodePro-Light', 'SourceCodePro-Medium']) {
  const uri = asDataUri(join(root, 'public', 'fonts', `${face}.ttf`), 'font/ttf');
  css = css.replaceAll(`/fonts/${face}.ttf`, uri);
}

const js = readFileSync(join(dist, 'assets', jsFile), 'utf8');
const title = 'abcTrain — ear training that loads in your DAW';

// The publish step wraps this in its own <!doctype>/<head>/<body>, so the
// file is written as page content only.
writeFileSync(
  out,
  `<title>${title}</title>
<style>
${css}
</style>
<div id="root"></div>
<script type="module">
${js}
</script>
`,
  'utf8',
);

const kb = (n) => `${Math.round(n / 1024)} KB`;
console.log(`Wrote ${out}`);
console.log(`  css ${kb(css.length)} (fonts embedded)   js ${kb(js.length)}`);
