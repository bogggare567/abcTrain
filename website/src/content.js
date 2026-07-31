// Everything the page states as fact.
//
// Kept in one file on purpose: docs/website-brief.md has a "what you must
// not claim" section, and a page that scatters its claims through markup
// is a page nobody can audit against it. Every string below is checkable
// against the repository — exercise names come from GameManager.cpp, the
// groupings from categoryForGame() in Source/PluginEditor.cpp, the module
// counts from CompressorModules.h / ReverbModules.h.
//
// No user counts, no star counts, no download counts: the product is
// pre-release and any number here would be invented.

import facts from './generated/plugin-facts.json';

export const RELEASE = {
  // Generated from `git describe` against the plugin's own tags, so a
  // release updates these links by being tagged rather than by somebody
  // remembering to edit this file. See tools/sync-from-plugin.mjs.
  version: facts.version,
  repo: 'https://github.com/bogggare567/abcTrain',
  releases: 'https://github.com/bogggare567/abcTrain/releases',
  site: 'https://soundkorb.ru',
  telegram: 'https://t.me/vstabc',
  vendor: 'soundkorb',
};

const dl = (file) =>
  `${RELEASE.repo}/releases/download/v${RELEASE.version}/${file}`;

export const DOWNLOADS = [
  {
    os: 'macOS',
    file: `abcTrain-macOS-${RELEASE.version}.dmg`,
    href: dl(`abcTrain-macOS-${RELEASE.version}.dmg`),
    formats: 'VST3 · AU · Standalone',
  },
  {
    os: 'Windows',
    file: `abcTrain-Windows-${RELEASE.version}-setup.exe`,
    href: dl(`abcTrain-Windows-${RELEASE.version}-setup.exe`),
    formats: 'VST3 · Standalone',
  },
  {
    os: 'Linux',
    file: `abcTrain-Linux-${RELEASE.version}.tar.gz`,
    href: dl(`abcTrain-Linux-${RELEASE.version}.tar.gz`),
    formats: 'VST3 · Standalone',
  },
];

// The four skill families are the product's real spine: nine exercises
// grouped by what they train, each with its own colour in
// AbcTrainTheme::accentFor(Family). Counts are 2 / 2 / 4 / 1 — verified
// against categoryForGame().
export const FAMILIES = [
  {
    key: 'freq',
    name: 'Frequency',
    what: 'Where a sound sits in the spectrum, and what a boost or a cut there does to it.',
    exercises: ['Guess the Band', 'Name the Range'],
  },
  {
    key: 'dyn',
    name: 'Dynamics',
    what: 'How hard something is being squeezed, and how big a level change really is.',
    exercises: ['Guess the Compression', 'Guess the Gain Change'],
  },
  {
    key: 'space',
    name: 'Space & stereo',
    what: 'The room around a sound, where it sits between the speakers, and how far back it is.',
    exercises: [
      'Guess the Reverb',
      'Guess the Pan Position',
      'Guess the Delay Time',
      'Guess the Stereo Width',
    ],
  },
  {
    key: 'char',
    name: 'Character',
    what: 'What kind of distortion is on it — the difference between warmth and damage.',
    exercises: ['Guess the Distortion'],
  },
];

// The three teaching plugins. Module counts from the per-plugin module
// files; Learner EQ deliberately has no knob modules — see ADR 027 — and
// four lessons instead.
export const PLUGINS = [
  {
    key: 'eq',
    name: 'ABC Learner EQ',
    body: 'A graphical EQ on your own audio: eight free bands of any type — bells, shelves, high-pass, low-pass, notch — added and moved on the curve itself, over a spectrum labelled in sensations as well as numbers.',
    modules: '4 guided lessons',
  },
  {
    key: 'comp',
    name: 'ABC Learner Comp',
    body: 'A real compressor with a gain-reduction meter that fills downward, four teaching presets, and per-knob training you answer by turning the plugin’s own knob.',
    modules: '7 knob modules · 4 lessons',
  },
  {
    key: 'verb',
    name: 'ABC Learner Verb',
    body: 'Room, hall, plate and a spring tank built from resonant allpass filters, with the same spectrum, meters and per-knob training.',
    modules: '7 knob modules · 4 lessons',
  },
];

// Straight from docs/website-brief.md's "what you must not claim". Putting
// these on the page rather than hiding them is the whole point: an
// instrument that tells you what it cannot do yet is one you can trust
// about what it can.
export const LIMITS = [
  {
    head: 'The builds are not signed',
    body: 'macOS Gatekeeper and Windows SmartScreen will both warn on first launch. Signing needs a certificate bought against a legal identity; it has not been done.',
  },
  {
    head: 'The importer does not separate stems',
    body: 'It sorts your audio by measurable character — percussive, where the energy sits, how wide it is. It cannot tell a vocal from a mix, and a heuristic pretending to would be confidently wrong.',
  },
  {
    head: 'Stereo width still trains on pink noise',
    body: 'Imported clips are downmixed to mono, and mono has no side signal to widen. Compression and delay are also harder to hear on a dense mix than on a synthesized stand.',
  },
  {
    head: 'Updating is not fully automatic',
    body: 'The button fetches and launches the right installer for your system. No program can replace a plugin the host already has loaded, so it always ends with “restart your DAW”.',
  },
];

export const FACTS = [
  ['Exercises', '9'],
  ['Plugins', '4'],
  ['Formats', 'VST3 · AU · Standalone'],
  ['Languages', '12'],
  ['Licence', 'Source available'],
];
