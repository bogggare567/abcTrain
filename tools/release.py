#!/usr/bin/env python3
"""Выпуск abcTrain одной командой.

    python3 tools/release.py v1.8.0-beta.1        # пре-релиз для тестировщиков
    python3 tools/release.py v1.8.0               # обычный выпуск
    python3 tools/release.py v1.8.0-beta.1 --dry-run

Что делает:
  1. Проверяет, что вы на main, дерево чистое и совпадает с GitHub.
  2. Ставит аннотированный тег и отправляет main и тег. Отправка тега и есть
     выпуск: CI собирает установщики под три системы, гоняет тесты и
     публикует релиз с текстом из .github/release-body.md. Тег с суффиксом
     (-beta.1) публикуется как пре-релиз: обычная проверка обновлений его не
     предлагает, бета-канал предлагает, сайт продолжает ссылаться на
     последнюю стабильную версию.
  3. Обновляет доску: карточки по этапам из tools/board/cards.json и по
     одному комментарию «сделано» со ссылкой на выпуск.

Нужны git и gh (brew install gh && gh auth login && gh auth refresh -s project).
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TAG_PATTERN = re.compile(r"^v\d+\.\d+\.\d+(-(alpha|beta|rc)\.\d+)?$")

# Each part's own version (ADR 069) and where its code lives: a part whose
# files changed since the last release but whose number did not is named
# before anything is sent. shared/ is everyone's, so a change there is
# listed for each plugin too - whether it changes what the plugin does is a
# judgement, not a rule.
PARTS = {
    "learner-eq": ["LearnerEQ/", "shared/dsp/", "shared/learning/", "shared/analysis/"],
    "learner-comp": ["LearnerComp/", "shared/dsp/", "shared/learning/", "shared/analysis/"],
    "learner-verb": ["LearnerVerb/", "shared/dsp/", "shared/learning/", "shared/analysis/"],
    "lessons": ["lessons/"],
}


def check_versions(tag):
    versions = json.loads((ROOT / "versions.json").read_text(encoding="utf-8"))
    core = tag[1:].split("-")[0]
    if versions.get("app") != core:
        sys.exit(f"versions.json: app = {versions.get('app')}, а тег {tag} — поправьте \"app\" на {core}")

    previous = run("git", "describe", "--tags", "--abbrev=0", "HEAD", check=False)
    print("Версии частей (versions.json):")
    for part, value in versions.items():
        print(f"  {part:13} {value}")

    if not previous:
        return
    try:
        before = json.loads(run("git", "show", f"{previous}:versions.json", check=False) or "{}")
    except json.JSONDecodeError:
        before = {}

    stale = []
    for part, paths in PARTS.items():
        changed = run("git", "diff", "--name-only", f"{previous}..HEAD", "--", *paths, check=False)
        if changed and before.get(part) == versions.get(part):
            files = changed.splitlines()
            stale.append(f"  {part}: {len(files)} файл(ов) изменилось с {previous}, а версия та же ({versions.get(part)}): "
                         + ", ".join(files[:3]) + (" …" if len(files) > 3 else ""))
    if stale:
        print("Внимание — изменилось, но версия не поднята:")
        print("\n".join(stale))
        print("Если изменение касается этой части, поднимите её номер в versions.json и закоммитьте.")


def run(*cmd, check=True, capture=True):
    result = subprocess.run(cmd, cwd=ROOT, text=True, capture_output=capture)
    if check and result.returncode != 0:
        sys.exit(f"Ошибка: {' '.join(cmd)}\n{(result.stderr or result.stdout).strip()}")
    return (result.stdout or "").strip()


def main():
    parser = argparse.ArgumentParser(description="Выпуск abcTrain")
    parser.add_argument("tag", help="например v1.8.0-beta.1 или v1.8.0")
    parser.add_argument("--dry-run", action="store_true", help="только проверить и показать")
    args = parser.parse_args()
    tag = args.tag

    if not TAG_PATTERN.match(tag):
        sys.exit(f"Тег «{tag}» не похож на vX.Y.Z или vX.Y.Z-beta.N")

    check_versions(tag)

    branch = run("git", "rev-parse", "--abbrev-ref", "HEAD")
    if branch != "main":
        sys.exit(f"Нужно быть на main, сейчас {branch}")

    if run("git", "status", "--porcelain"):
        sys.exit("В репозитории есть незакоммиченные изменения — сначала закоммитьте или уберите их")

    # Не --tags: локальный тег, отличающийся от одноимённого на GitHub,
    # роняет fetch целиком ("would clobber existing tag"). Сам новый тег
    # проверяем и здесь, и на GitHub.
    run("git", "fetch", "origin", "main")
    if run("git", "tag", "-l", tag) or run("git", "ls-remote", "--tags", "origin", f"refs/tags/{tag}"):
        sys.exit(f"Тег {tag} уже есть")

    behind = run("git", "rev-list", "--count", "HEAD..origin/main")
    if behind != "0":
        sys.exit(f"main отстаёт от GitHub на {behind} коммит(ов): git pull --ff-only")

    ahead = run("git", "rev-list", "--count", "origin/main..HEAD")
    head = run("git", "log", "--oneline", "-1")
    kind = "пре-релиз" if "-" in tag else "выпуск"

    print(f"{kind} {tag} на {head}")
    print(f"  неотправленных коммитов в main: {ahead}")

    if args.dry_run:
        print("Сухой прогон: ничего не отправлено.")
        return

    if ahead != "0":
        run("git", "push", "origin", "main", capture=False)

    run("git", "tag", "-a", tag, "-m", f"abcTrain {tag}")
    run("git", "push", "origin", tag, capture=False)

    print("\nДоска:")
    subprocess.run([sys.executable, str(ROOT / "tools/board/create_board.py"), "--sync-stages", "--release", tag],
                   cwd=ROOT, check=False)

    print(f"\nГотово. Сборка идёт ~30 минут: https://github.com/bogggare567/abcTrain/actions")
    print(f"Страница выпуска появится здесь: https://github.com/bogggare567/abcTrain/releases/tag/{tag}")


if __name__ == "__main__":
    main()
