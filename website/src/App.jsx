import { useEffect, useState } from 'react';
import PlayableRound from './components/PlayableRound.jsx';
import { RELEASE, DOWNLOADS, FAMILIES, PLUGINS, LIMITS, FACTS } from './content.js';

function Masthead({ theme, onToggleTheme }) {
  return (
    <header className="masthead">
      <div className="wrap masthead__inner">
        <a className="wordmark" href="#top">
          <i>a</i><i>b</i><i>c</i>Train
        </a>
        <nav className="masthead__nav">
          <a className="masthead__link" href="#train">What it trains</a>
          <a className="masthead__link" href="#plugins">Plugins</a>
          <a className="masthead__link" href="#limits">Limits</a>
          <a className="masthead__link" href="#get">Download</a>
          <button
            type="button"
            className="themeToggle"
            onClick={onToggleTheme}
            aria-label={`Switch to ${theme === 'dark' ? 'light' : 'dark'} theme`}
          >
            {theme === 'dark' ? 'Light' : 'Dark'}
          </button>
        </nav>
      </div>
    </header>
  );
}

function Hero() {
  return (
    <section className="wrap hero" id="top">
      {/* The headline gets the full measure. A monospaced display face
          eats roughly twice the line a sans does, so squeezing it into a
          half-width column broke it onto one word per line. */}
      <p className="label" style={{ margin: 0 }}>
        Ear training · {RELEASE.vendor}
      </p>
      <h1>
        Learn to hear what<br />
        you’re <em>looking</em> at.
      </h1>

      <div className="hero__grid">
        <div className="stack">
          <p className="lede">
            Four plugins that load in your DAW. Nine exercises that play you a processed
            signal and ask one question about it. Three of them are real EQ, compressor
            and reverb units that teach while they process your own audio.
          </p>
          <div className="row">
            <a className="btn btn--primary" href="#get">Download v{RELEASE.version}</a>
            <a className="btn" href={RELEASE.repo}>Source on GitHub</a>
          </div>
          <div className="hero__meta">
            {FACTS.map(([k, v]) => (
              <span key={k}>
                {k} <b className="num" style={{ color: 'var(--dim)' }}>{v}</b>
              </span>
            ))}
          </div>
        </div>

        <PlayableRound />
      </div>
    </section>
  );
}

function Families() {
  return (
    <section className="wrap rack" id="train">
      <p className="label">What it trains</p>
      <div className="stack-5">
        <div className="prose stack-3">
          <h2>Four things an ear can be taught.</h2>
          <p className="lede">
            The nine exercises are grouped by the skill they build, not by the effect they
            use — and each group has its own colour everywhere in the product, so a run
            you started yesterday is recognisable at a glance.
          </p>
        </div>

        <div className="families">
          {FAMILIES.map((f) => (
            <article key={f.key} className={`family family--${f.key}`}>
              <div>
                <div className="family__name">{f.name}</div>
                <div className="family__count">
                  {f.exercises.length} exercise{f.exercises.length > 1 ? 's' : ''}
                </div>
              </div>
              <p className="family__what">{f.what}</p>
              <ul className="family__list">
                {f.exercises.map((e) => (
                  <li key={e}>{e}</li>
                ))}
              </ul>
            </article>
          ))}
        </div>
      </div>
    </section>
  );
}

function Plugins() {
  return (
    <section className="wrap rack" id="plugins">
      <p className="label">The teaching plugins</p>
      <div className="stack-5">
        <div className="prose stack-3">
          <h2>Three of them process your actual track.</h2>
          <p className="lede">
            Not simulations. They are host-automatable units you can leave on a channel —
            and while you turn a knob they tell you what it does, then hide a value and ask
            you to dial it back by ear.
          </p>
        </div>

        <div className="plugins">
          {PLUGINS.map((p) => (
            <article key={p.key} className={`plugin plugin--${p.key}`}>
              <div className="plugin__name">{p.name}</div>
              <p className="plugin__body">{p.body}</p>
              <div className="plugin__modules">{p.modules}</div>
            </article>
          ))}
        </div>
      </div>
    </section>
  );
}

function Limits() {
  return (
    <section className="wrap rack" id="limits">
      <p className="label">Known limits</p>
      <div className="limits">
        <div className="prose stack-3">
          <h2>What it doesn’t do yet.</h2>
          <p className="lede">
            You will find all of this out in the first ten minutes. Better here than there.
          </p>
        </div>
        <ul className="limits__list">
          {LIMITS.map((l) => (
            <li key={l.head}>
              <span className="limits__mark" aria-hidden="true">—</span>
              <span>
                <b>{l.head}.</b> {l.body}
              </span>
            </li>
          ))}
        </ul>
      </div>
    </section>
  );
}

function Get() {
  return (
    <section className="wrap rack" id="get">
      <p className="label">Download</p>
      <div className="stack-5">
        <div className="prose stack-3">
          <h2>Version {RELEASE.version}, built on every push.</h2>
          <p className="lede">
            One installer per system, containing all four plugins. Pick the formats you
            want during setup.
          </p>
        </div>

        <div className="downloads">
          {DOWNLOADS.map((d) => (
            <a className="dl" key={d.os} href={d.href}>
              <span className="dl__os">{d.os}</span>
              <span className="dl__formats">{d.formats}</span>
              <span className="dl__file">{d.file}</span>
            </a>
          ))}
        </div>

        <p className="round__note" style={{ maxWidth: '58ch' }}>
          The builds are unsigned — your system will warn you on first launch. On macOS,
          right-click the installer and choose Open; on Windows, More info → Run anyway.
        </p>
      </div>
    </section>
  );
}

function Footer() {
  return (
    <footer className="wrap footer">
      <div className="footer__cols">
        <div className="stack-3">
          <a className="wordmark" href="#top">
            <i>a</i><i>b</i><i>c</i>Train
          </a>
          <p style={{ maxWidth: '34ch' }}>
            ambiance · balance · clarity — by {RELEASE.vendor}
          </p>
        </div>
        <div className="stack-3">
          <a href={RELEASE.repo}>GitHub</a>
          <a href={RELEASE.releases}>All releases</a>
          <a href={RELEASE.telegram}>Telegram · @vstabc</a>
          <a href={RELEASE.site}>soundkorb.ru</a>
        </div>
      </div>
    </footer>
  );
}

export default function App() {
  // Defaults to whatever the visitor's OS says; the toggle then pins a
  // choice and remembers it.
  const [theme, setTheme] = useState(() => {
    const saved = typeof localStorage !== 'undefined' && localStorage.getItem('abctrain-theme');
    if (saved === 'light' || saved === 'dark') return saved;
    return typeof matchMedia !== 'undefined' && matchMedia('(prefers-color-scheme: light)').matches
      ? 'light'
      : 'dark';
  });

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    localStorage.setItem('abctrain-theme', theme);
  }, [theme]);

  return (
    <>
      <Masthead theme={theme} onToggleTheme={() => setTheme((t) => (t === 'dark' ? 'light' : 'dark'))} />
      <main>
        <Hero />
        <Families />
        <Plugins />
        <Limits />
        <Get />
      </main>
      <Footer />
    </>
  );
}
