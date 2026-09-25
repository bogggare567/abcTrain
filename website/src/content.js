// Everything the page states as fact.
//
// Kept in one file on purpose: docs/website-brief.md has a "what you must
// not claim" section, and a page that scatters its claims through markup
// is a page nobody can audit against it. Every string below is checkable
// against the repository — exercise names come from GameManager.cpp, the
// groupings from categoryForGame() in Source/PluginEditor.cpp, the module
// counts from EQModules.h / CompressorModules.h / ReverbModules.h, the
// staircase from ADR 035, Beginner/Pro and hearing from ADR 036.
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
    formats: 'App · VST3 · AU',
  },
  {
    os: 'Windows',
    file: `abcTrain-Windows-${RELEASE.version}-setup.exe`,
    href: dl(`abcTrain-Windows-${RELEASE.version}-setup.exe`),
    formats: 'App · VST3',
  },
  {
    os: 'Linux',
    file: `abcTrain-Linux-${RELEASE.version}.tar.gz`,
    href: dl(`abcTrain-Linux-${RELEASE.version}.tar.gz`),
    formats: 'App · VST3',
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

// How the trainer judges you, in the terms ADR 035 and 036 use. Each line
// is something the app does now; none of it is a promise.
export const TRAINER_NOTES = [
  {
    head: 'A staircase, not points',
    body: 'Three right in a row makes an exercise one step harder, one wrong makes it one step easier — ten steps, and a record that never drops. Your level is shown as what it means: a threshold in the exercise’s own units, like ±0.35 oct or ±1.2 dB.',
  },
  {
    head: 'Beginner and Pro',
    body: 'Beginner keeps every training rule at its default. Pro opens them: how many in a row make it harder, the pause after an answer, hints, Survival and Blitz rules.',
  },
  {
    head: 'Hearing protection, optional',
    body: 'On by default. It suggests a break after an hour of sound, and — if you calibrate against any dB(A) meter — keeps a weekly dose estimate against the WHO / ITU-T H.870 safe-listening limit. It informs; it never blocks a round.',
  },
];

// The three teaching plugins. Module and lesson counts from EQModules.h,
// CompressorModules.h and ReverbModules.h (ADR 037).
export const PLUGINS = [
  {
    key: 'eq',
    name: 'ABC Learner EQ',
    body: 'A graphical EQ on your own audio: eight free bands of any type — bells, shelves, high-pass, low-pass, notch — added and moved on the curve itself, over a spectrum labelled in sensations as well as numbers.',
    modules: '4 knob modules · 4 lessons',
  },
  {
    key: 'comp',
    name: 'ABC Learner Comp',
    body: 'A real compressor with a gain-reduction meter that fills downward, a transfer curve drawn from the engine’s own gain computer, and four teaching presets.',
    modules: '7 knob modules · 4 lessons',
  },
  {
    key: 'verb',
    name: 'ABC Learner Verb',
    body: 'Room and hall from a feedback delay network, a Dattorro plate and two springs. Decay is measured RT60, and the display is an echogram — the plugin’s actual impulse response.',
    modules: '7 knob modules · 4 lessons',
  },
];

// What all three share, said once rather than three times.
export const PLUGINS_COMMON =
  'In all three, each knob is a training module: you match a hidden setting with the plugin’s own knob, inside an accept band in that knob’s units, on the same ten-step staircase as the trainer. A/B slots for comparing two settings, and the whole interface in 12 languages.';

// Live (ADR 045-047): what works today, without a server of anybody's.
export const LIVE = [
  {
    key: 'seminar',
    head: 'A seminar in a room, no internet',
    body: 'The presenter’s laptop plays the exercise into the hall and serves a page to the phones on the same Wi-Fi. Everybody scans one QR code, answers on the phone, and the projector window shows the question, how the room voted and the answer. A list of invitees prints as cards, each with its own code and QR.',
  },
  {
    key: 'bots',
    head: 'Battles against six bot listeners',
    body: 'Seven rounds on the same material for both, against a listener with its own hearing profile. Offline.',
  },
  {
    key: 'account',
    head: 'An account, only if you want one',
    body: 'Sign in on soundkorb.ru with an e-mail code, no password; the app is linked by a short code and gets a key you can revoke. Sync sends a summary of about 1 KB. Training never needs it.',
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
    head: 'The hearing dose is an estimate',
    body: 'Software sees dBFS, not what reaches your ears. The weekly figure holds only while the monitor level stays where you calibrated it, and it is not a certified H.870 measurement.',
  },
  {
    head: 'Seminars over the internet are not open yet',
    body: 'A seminar runs on the local network today. Online rooms and battles between people wait for the round server, which scores rounds on the server so a rating cannot be inflated.',
  },
  {
    head: 'Updating is not fully automatic',
    body: 'The button fetches and launches the right installer for your system. No program can replace a plugin the host already has loaded, so it always ends with “restart your DAW”.',
  },
];

export const FACTS = [
  ['Exercises', '9'],
  ['App + plugins', '1 + 3'],
  ['Formats', 'VST3 · AU · Standalone'],
  ['Languages', '12'],
  ['Licence', 'Source available'],
];
