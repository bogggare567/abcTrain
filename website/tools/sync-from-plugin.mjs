// Derives the site's facts from the plugin's own source, so the two cannot
// drift.
//
// Before this, the palette in tokens.css and the tolerances in the demo
// were *typed out* from the C++ by hand. That works exactly until somebody
// changes a colour or widens an accept band, at which point the site
// quietly starts describing a product that no longer exists — and nothing
// fails, which is the worst kind of wrong.
//
// So: read the values out of the source of truth, write them to
// src/generated/plugin-facts.json, and let the build fail loudly if a
// value can no longer be found. A regex over C++ is a blunt instrument and
// it is used here only on lines that are deliberately simple — a named
// constant with a literal. If one of them stops matching, the extractor
// throws rather than emitting a default, because a default here is a lie
// with a plausible face.
//
//   node tools/sync-from-plugin.mjs [--check]
//
// --check exits non-zero if the generated file is stale, which is what CI
// runs so a pull request cannot land a mismatch.

import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { execSync } from 'node:child_process';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..', '..');
const out = join(here, '..', 'src', 'generated', 'plugin-facts.json');

const read = (relative) => readFileSync(join(root, relative), 'utf8');

// Every extraction goes through this, so a source change that breaks an
// assumption stops the build instead of silently producing a stale number.
function grab(source, label, pattern, transform = (m) => m[1]) {
  const match = source.match(pattern);

  if (!match) {
    throw new Error(
      `sync-from-plugin: could not find ${label}.\n` +
        `The pattern ${pattern} no longer matches. Someone changed the plugin; ` +
        `update this extractor rather than hand-editing the generated file.`,
    );
  }

  return transform(match);
}

// --- the four skill-family colours, from AbcTrainTheme::accentFor --------
const theme = read('shared/AbcTrainTheme.cpp');

const familyColour = (family) =>
  grab(
    theme,
    `the ${family} family colour`,
    new RegExp(`case Family::${family}:\\s*return juce::Colour \\(0xff([0-9a-f]{6})\\)`),
    (m) => `#${m[1]}`,
  );

// --- accept bands, level 1 and level 10, from each game's ramp ----------
// Since ADR 035 each ruler exercise has one `toleranceForLevel`, shared by
// setDifficulty and describeLevel, and it is a geometric ramp between two
// literals. Both ends are read, so the demo's staircase narrows the band
// exactly as the plugin's does.
const eq = read('Source/Games/EQGame.cpp');
const eqHeader = read('Source/Games/EQGame.h');
const db = read('Source/Games/DBGame.cpp');
const pan = read('Source/Games/PanGame.cpp');
const delay = read('Source/Games/DelayGame.cpp');

const ramp = (source, label, pattern) =>
  grab(source, label, pattern, (m) => ({ level1: Number(m[1]), level10: Number(m[2]) }));

const toleranceRamp = (source, game) =>
  ramp(
    source,
    `${game}'s toleranceForLevel ramp`,
    new RegExp(
      `${game}::toleranceForLevel \\(int level\\) noexcept\\s*\\{\\s*` +
        'return rampTolerance \\(level, ([0-9.]+)f, ([0-9.]+)f\\);',
    ),
  );

// --- the version the site should point at -------------------------------
// The same `git describe` the plugin builds its own version string from
// (ADR 012), so a tagged release updates the download links by existing
// rather than by somebody remembering.
const describe = execSync('git describe --tags --abbrev=0', { cwd: root })
  .toString()
  .trim();

const facts = {
  // Written into the file so it is obvious this is not hand-maintained.
  generatedBy: 'website/tools/sync-from-plugin.mjs',
  version: describe.replace(/^v/, ''),

  families: {
    frequency: familyColour('frequency'),
    dynamics: familyColour('dynamics'),
    space: familyColour('space'),
    character: familyColour('character'),
  },

  // Each ruler exercise's accept band at step 1 and at step 10, in its own
  // units: octaves, dB, a fraction of one side (pan), a fraction of the
  // time (delay). The steps between are DifficultyRamp::geometric.
  tolerances: {
    band: toleranceRamp(eq, 'EQGame'),
    gain: toleranceRamp(db, 'DBGame'),
    pan: toleranceRamp(pan, 'PanGame'),
    delay: toleranceRamp(delay, 'DelayGame'),
  },

  // Guess the Band's boost shrinks with the level too (EQGame::setDifficulty).
  bandBoostDb: ramp(
    eq,
    "EQGame's boost ramp",
    /gainDb = rampTolerance \(level, ([0-9.]+)f, ([0-9.]+)f\)/,
  ),

  // The ISO octave centres the trainer marks its axis with.
  bandTicks: grab(
    eq,
    "EQGame's grid frequencies",
    /31\.5f, 63\.0f, 125\.0f, 250\.0f, 500\.0f, 1000\.0f, 2000\.0f, 4000\.0f,\s*8000\.0f, 16000\.0f/,
    () => [31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000],
  ),

  // The ends of the frequency axis, read out rather than written down
  // twice - the demo had them as `100 / SQRT2` and `12800 * SQRT2`, which
  // silently became a different exercise the moment the plugin's range
  // changed.
  bandAxis: {
    lowHz: grab(
      eqHeader,
      "EQGame's axis low bound",
      /axisLowHz = ([0-9.]+)f/,
      (m) => Number(m[1]),
    ),
    highHz: grab(
      eqHeader,
      "EQGame's axis high bound",
      /axisHighHz = ([0-9.]+)f/,
      (m) => Number(m[1]),
    ),
  },

  // Narrower than the axis on purpose: the ruler shows everything you can
  // hear, the questions stay where a filter is actually audible. See
  // EQGame's targetLowHz/targetHighHz.
  bandTargets: {
    lowHz: grab(
      eqHeader,
      "EQGame's target low bound",
      /targetLowHz = ([0-9.]+)f/,
      (m) => Number(m[1]),
    ),
    highHz: grab(
      eqHeader,
      "EQGame's target high bound",
      /targetHighHz = ([0-9.]+)f/,
      (m) => Number(m[1]),
    ),
  },
};

const json = `${JSON.stringify(facts, null, 2)}\n`;

if (process.argv.includes('--check')) {
  const current = existsSync(out) ? readFileSync(out, 'utf8') : '';

  if (current !== json) {
    console.error(
      'sync-from-plugin: src/generated/plugin-facts.json is out of date.\n' +
        'The plugin changed and the site did not. Run:\n\n' +
        '    node tools/sync-from-plugin.mjs\n',
    );
    process.exit(1);
  }

  console.log('sync-from-plugin: up to date.');
  process.exit(0);
}

mkdirSync(dirname(out), { recursive: true });
writeFileSync(out, json, 'utf8');
console.log(`sync-from-plugin: wrote ${out}`);
console.log(`  version ${facts.version}`);
console.log(`  tolerances`, facts.tolerances);
