#!/usr/bin/env python3
"""Создаёт публичную доску abcTrain на GitHub одной командой.

    python3 tools/board/create_board.py            # сделать
    python3 tools/board/create_board.py --dry-run  # показать, что будет сделано
    python3 tools/board/create_board.py --sync-stages  # ещё и переставить уже
        # существующие карточки по этапам из cards.json (перезапишет то, что
        # двигали руками на доске)

Скрипт можно запускать повторно. Уже существующие метки, карточки (по
заголовку) и доска не дублируются, добавляется только новое из cards.json.

Нужен GitHub CLI с правом на проекты:
    brew install gh
    gh auth login
    gh auth refresh -s project
"""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

REPO = "bogggare567/abcTrain"
OWNER = "bogggare567"
PROJECT_TITLE = "abcTrain"
STAGE_FIELD = "Этап"
STAGES = ["Идеи", "Обсуждаем", "Готово к работе", "В работе", "Проверяют люди", "Выпущено"]

LABELS = {
    "trainer": ("1f6feb", "ABC Ear Trainer"),
    "learner-eq": ("0e8a16", "ABC Learner EQ"),
    "learner-comp": ("d93f0b", "ABC Learner Comp"),
    "learner-verb": ("5319e7", "ABC Learner Verb"),
    "site": ("c5def5", "Сайт, демо, видео"),
    "education": ("fbca04", "Школы, преподаватели, Live"),
    "infra": ("bfdadc", "Сборка, CI, выпуск, подпись"),
    "docs": ("d4c5f9", "Документация"),
    "library": ("0052cc", "Библиотека звуков и пакеты"),
    "idea": ("a2eeef", "Идея"),
    "feedback": ("f9d0c4", "Отзыв живого человека"),
    "research": ("006b75", "Опирается на литературу, см. docs/research/"),
    "needs-source": ("e4e669", "Нужна опора в литературе или практике"),
}

DRY = False


def gh(*args, parse=False):
    cmd = ["gh", *args]
    mutating = {"create", "edit", "item-add", "item-edit", "field-create", "link", "comment"}
    if DRY and len(args) > 1 and args[1] in mutating:
        print("  [dry-run]", " ".join(cmd)[:160])
        return {} if parse else ""
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"\nОшибка: {' '.join(cmd)[:200]}\n{out.stderr.strip()}")
    return json.loads(out.stdout or "{}") if parse else out.stdout


def check_tools():
    if shutil.which("gh") is None:
        sys.exit("Нет GitHub CLI. Установите: brew install gh && gh auth login && gh auth refresh -s project")
    status = subprocess.run(["gh", "auth", "status"], capture_output=True, text=True)
    text = status.stdout + status.stderr
    if status.returncode != 0:
        sys.exit("gh не авторизован. Выполните: gh auth login")
    if "project" not in text:
        sys.exit("У токена нет права на проекты. Выполните: gh auth refresh -s project")


def ensure_labels():
    existing = {l["name"] for l in gh("label", "list", "-R", REPO, "--limit", "200", "--json", "name", parse=True) or []}
    for name, (color, desc) in LABELS.items():
        if name not in existing:
            print(f"+ метка {name}")
            gh("label", "create", name, "-R", REPO, "--color", color, "--description", desc)


def ensure_issues(cards):
    existing = {
        i["title"]: i["url"]
        for i in gh("issue", "list", "-R", REPO, "--state", "all", "--limit", "500", "--json", "title,url", parse=True) or []
    }
    urls = {}
    for card in cards:
        if card["title"] in existing:
            urls[card["title"]] = existing[card["title"]]
            continue
        print(f"+ карточка: {card['title'][:70]}")
        args = ["issue", "create", "-R", REPO, "--title", card["title"], "--body", card["body"]]
        for label in card["labels"]:
            args += ["--label", label]
        urls[card["title"]] = gh(*args).strip()
    return urls


def ensure_project():
    projects = gh("project", "list", "--owner", OWNER, "--format", "json", parse=True).get("projects", [])
    for p in projects:
        if p["title"] == PROJECT_TITLE:
            return p["number"], p["id"]
    print(f"+ доска {PROJECT_TITLE}")
    p = gh("project", "create", "--owner", OWNER, "--title", PROJECT_TITLE, "--format", "json", parse=True)
    if DRY:
        return None, None
    gh("project", "edit", str(p["number"]), "--owner", OWNER, "--visibility", "PUBLIC",
       "--description", "Задачи abcTrain: от идеи до выпуска. Процесс — docs/process.md")
    gh("project", "link", str(p["number"]), "--owner", OWNER, "--repo", REPO)
    return p["number"], p["id"]


def ensure_stage_field(number):
    fields = gh("project", "field-list", str(number), "--owner", OWNER, "--format", "json", parse=True).get("fields", [])
    for f in fields:
        if f["name"] == STAGE_FIELD:
            return f
    print(f"+ поле «{STAGE_FIELD}»")
    gh("project", "field-create", str(number), "--owner", OWNER, "--name", STAGE_FIELD,
       "--data-type", "SINGLE_SELECT", "--single-select-options", ",".join(STAGES))
    fields = gh("project", "field-list", str(number), "--owner", OWNER, "--format", "json", parse=True)["fields"]
    return next(f for f in fields if f["name"] == STAGE_FIELD)


def ensure_items(number, project_id, field, cards, urls, sync_stages):
    items = gh("project", "item-list", str(number), "--owner", OWNER, "--limit", "500", "--format", "json", parse=True)
    on_board = {i.get("content", {}).get("url"): i for i in items.get("items", [])}
    options = {o["name"]: o["id"] for o in field["options"]}
    for card in cards:
        url = urls[card["title"]]
        if url in on_board:
            item = on_board[url]
            current = item.get(STAGE_FIELD.lower()) or item.get(STAGE_FIELD)
            if sync_stages and current != card["stage"]:
                gh("project", "item-edit", "--id", item["id"], "--project-id", project_id,
                   "--field-id", field["id"], "--single-select-option-id", options[card["stage"]])
                print(f"  ⇢ {card['stage']}: {card['title'][:60]}")
            continue
        item = gh("project", "item-add", str(number), "--owner", OWNER, "--url", url, "--format", "json", parse=True)
        gh("project", "item-edit", "--id", item["id"], "--project-id", project_id,
           "--field-id", field["id"], "--single-select-option-id", options[card["stage"]])
        print(f"  → {card['stage']}: {card['title'][:60]}")


DONE_MARKER = "<!-- abctrain-done -->"


def comment_done(cards, urls, release_tag):
    """Один комментарий «сделано» на карточку с полем done. Повторный запуск
    не дублирует: ищет свой маркер в уже оставленных комментариях."""
    for card in cards:
        done = card.get("done")
        if not done:
            continue

        url = urls[card["title"]]
        number = url.rstrip("/").split("/")[-1]
        existing = gh("issue", "view", number, "-R", REPO, "--json", "comments", parse=True) or {}
        if any(DONE_MARKER in (c.get("body") or "") for c in existing.get("comments", [])):
            continue

        where = (f"в коммите https://github.com/{REPO}/commit/{done}" if done != "release"
                 else "в этом выпуске")
        body = (f"{DONE_MARKER}\n✅ Сделано {where}.\n\n"
                f"Входит в **{release_tag}**: https://github.com/{REPO}/releases/tag/{release_tag}\n"
                f"Что проверить: https://github.com/{REPO}/blob/main/BETA_TESTING.md\n\n"
                "Карточка остаётся открытой в «Проверяют люди», пока тестировщики не подтвердят.")
        print(f"  ✎ {card['title'][:60]}")
        gh("issue", "comment", number, "-R", REPO, "--body", body)


def main():
    global DRY
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--sync-stages", action="store_true")
    parser.add_argument("--release", metavar="TAG", help="отметить сделанное комментарием со ссылкой на выпуск")
    args = parser.parse_args()
    DRY = args.dry_run

    cards = json.loads((Path(__file__).parent / "cards.json").read_text(encoding="utf-8"))
    for c in cards:
        assert c["stage"] in STAGES, f"неизвестный этап: {c['stage']}"
        assert all(l in LABELS for l in c["labels"]), f"неизвестная метка в «{c['title']}»"

    check_tools()
    ensure_labels()
    urls = ensure_issues(cards)
    number, project_id = ensure_project()
    if DRY:
        print("\nСухой прогон окончен.")
        return
    field = ensure_stage_field(number)
    ensure_items(number, project_id, field, cards, urls, args.sync_stages)

    if args.release:
        comment_done(cards, urls, args.release)

    print(f"\nГотово: https://github.com/users/{OWNER}/projects/{number}")
    print(f"Осталось один раз в браузере: вид Board → Group by «{STAGE_FIELD}».")


if __name__ == "__main__":
    main()
