#!/usr/bin/env python3
"""Витрина репозитория на GitHub одной командой: блок About и вики.

    python3 tools/github_profile.py            # About + вики
    python3 tools/github_profile.py --no-wiki  # только About

About — описание, сайт и темы справа на странице репозитория. Вики — копия
docs/wiki/ на вкладке Wiki: в репозитории она правится вместе с кодом, а
на GitHub попадает только этой командой.

Нужны git и gh (gh auth login).
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = "bogggare567/abcTrain"

DESCRIPTION = ("Ear training for sound engineers — practise alone, learn in class, compete live. "
               "Nine listening exercises on a psychoacoustic staircase and three teaching plugins "
               "(EQ · Comp · Verb). Free, VST3 · AU · Standalone.")
HOMEPAGE = "https://bogggare567.github.io/abcTrain/"
TOPICS = ["ear-training", "audio-engineering", "critical-listening", "psychoacoustics", "mixing",
          "music-education", "juce", "vst3", "audio-unit", "audio-plugin", "equalizer",
          "compressor", "reverb"]


def run(*cmd, cwd=ROOT):
    result = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if result.returncode != 0:
        sys.exit(f"Ошибка: {' '.join(cmd)}\n{(result.stderr or result.stdout).strip()}")
    return result.stdout.strip()


def about():
    args = ["gh", "repo", "edit", REPO, "--description", DESCRIPTION, "--homepage", HOMEPAGE,
            "--enable-discussions", "--enable-wiki", "--enable-projects"]
    for topic in TOPICS:
        args += ["--add-topic", topic]
    run(*args)
    print("About обновлён: описание, сайт, темы; Discussions, Wiki и Projects включены.")


def wiki():
    source = ROOT / "docs/wiki"
    with tempfile.TemporaryDirectory() as tmp:
        clone = Path(tmp) / "wiki"
        result = subprocess.run(["git", "clone", "-q", f"https://github.com/{REPO}.wiki.git", str(clone)],
                                text=True, capture_output=True)
        if result.returncode != 0:
            sys.exit("Вики ещё не создана на GitHub: откройте вкладку Wiki, создайте любую первую "
                     "страницу в браузере и запустите команду снова.")

        for page in source.glob("*.md"):
            shutil.copy(page, clone / page.name)

        run("git", "add", "-A", cwd=clone)
        if not run("git", "status", "--porcelain", cwd=clone):
            print("Вики уже совпадает с docs/wiki.")
            return

        run("git", "commit", "-q", "-m", "Синхронизация с docs/wiki", cwd=clone)
        run("git", "push", "-q", cwd=clone)
        print("Вики обновлена из docs/wiki.")


def main():
    parser = argparse.ArgumentParser(description="About и вики abcTrain на GitHub")
    parser.add_argument("--no-wiki", action="store_true")
    args = parser.parse_args()

    if shutil.which("gh") is None:
        sys.exit("Нужен GitHub CLI: brew install gh && gh auth login")

    about()
    if not args.no_wiki:
        wiki()


if __name__ == "__main__":
    main()
