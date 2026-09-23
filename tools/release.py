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
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TAG_PATTERN = re.compile(r"^v\d+\.\d+\.\d+(-(alpha|beta|rc)\.\d+)?$")


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

    branch = run("git", "rev-parse", "--abbrev-ref", "HEAD")
    if branch != "main":
        sys.exit(f"Нужно быть на main, сейчас {branch}")

    if run("git", "status", "--porcelain"):
        sys.exit("В репозитории есть незакоммиченные изменения — сначала закоммитьте или уберите их")

    run("git", "fetch", "--tags", "origin")
    if run("git", "tag", "-l", tag):
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
