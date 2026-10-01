#!/bin/bash
# Проверка всего abcTrain на маке перед пушем и деплоем — двойной щелчок в Finder.
#
#   1. Собирает приложение, тесты и BattleSmoke в build-check/ (отдельно от
#      build/, чтобы не упираться в права на старый .app).
#   2. Гоняет тесты приложения и тесты сервера батлов сайта, проверку типов сайта.
#   3. Поднимает локальный API сайта на 127.0.0.1:8931 с временной базой
#      (не data/app.db, не .env — почта никуда не уходит), прогоняет на нём
#      BattleSmoke (два «приложения» играют настоящий батл через сервер)
#      и минуту виртуальных игроков в комнатах — чтобы в рейтинге было что смотреть.
#   4. Открывает страницу рейтинга (vite на 5173 → API 8931) и приложение,
#      подключённое к этому же локальному серверу.
#
# Enter в этом окне — всё останавливается. Ничего не пушит и не деплоит.
#
# Папка сайта по умолчанию ~/Desktop/bogdankorablev; другая: SITE_DIR=… ./check-local.command

set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SITE="${SITE_DIR:-$HOME/Desktop/bogdankorablev}"
BUILD="$REPO/build-check"
API_PORT=8931
TMP="$(mktemp -d -t abctrain-check)"
PIDS=()
FAILED=()

say()  { printf '\n\033[1m== %s\033[0m\n' "$*"; }
ok()   { printf '   \033[32mok\033[0m  %s\n' "$*"; }
bad()  { printf '   \033[31mFAIL\033[0m %s\n' "$*"; FAILED+=("$*"); }

cleanup() {
  for p in ${PIDS[@]+"${PIDS[@]}"}; do kill "$p" 2>/dev/null; done
  rm -rf "$TMP"
}
trap cleanup EXIT

say "Приложение: сборка ($BUILD)"
GEN=()
command -v ninja >/dev/null && GEN=(-G Ninja)
if cmake -S "$REPO" -B "$BUILD" ${GEN[@]+"${GEN[@]}"} -DCMAKE_BUILD_TYPE=Release >"$TMP/cmake.log" 2>&1 \
   && cmake --build "$BUILD" --config Release --target EarTrainer_Standalone EarTrainerTests BattleSmoke -j 8 >"$TMP/build.log" 2>&1; then
  ok "собрано"
else
  bad "сборка — хвост лога ниже"; tail -30 "$TMP/build.log" "$TMP/cmake.log" 2>/dev/null
  echo; read -r -p "Enter — выйти"; exit 1
fi

TESTS="$(find "$BUILD/EarTrainerTests_artefacts" -type f -perm -u+x -name 'EarTrainer*Tests' | head -1)"
SMOKE="$(find "$BUILD/BattleSmoke_artefacts" -type f -perm -u+x -name 'BattleSmoke' | head -1)"
APP="$(find "$BUILD/EarTrainer_artefacts" -maxdepth 4 -name '*.app' -type d | head -1)"

say "Приложение: тесты"
if "$TESTS" >"$TMP/tests.log" 2>&1; then ok "$(grep -c 'Starting test' "$TMP/tests.log") тестов"; else bad "тесты приложения"; grep -B2 -A6 -i "fail" "$TMP/tests.log" | head -40; fi

say "Сайт: зависимости, типы, тесты сервера батлов ($SITE)"
cd "$SITE" || { bad "нет папки сайта $SITE"; read -r -p "Enter — выйти"; exit 1; }
[ -d node_modules ] || npm ci >"$TMP/npm.log" 2>&1 || bad "npm ci"
if npm run -s typecheck >"$TMP/tsc.log" 2>&1; then ok "typecheck"; else bad "typecheck"; head -30 "$TMP/tsc.log"; fi
if node scripts/abctrain-battles-test.mjs >"$TMP/srv.log" 2>&1; then ok "$(tail -1 "$TMP/srv.log")"; else bad "тесты сервера батлов"; tail -30 "$TMP/srv.log"; fi

say "Локальный API на 127.0.0.1:$API_PORT (временная база)"
(
  cd "$TMP" && exec env -u SMTP_HOST -u SMTP_USER -u SMTP_PASS -u MAIL_FROM -u MAIL_TO \
    PORT=$API_PORT SQLITE_PATH="$TMP/check.db" NODE_ENV=test ABCTRAIN_TEST_HOOKS=1 \
    ABCTRAIN_PEPPER=check NOTIFICATION_CHANNEL=none ALLOWED_ORIGINS="http://127.0.0.1:5173,http://localhost:5173" \
    ABCTRAIN_BATTLE_ACCEPT_MS=8000 ABCTRAIN_BATTLE_COUNTDOWN_MS=1500 \
    ABCTRAIN_BATTLE_REVEAL_MS=1500 ABCTRAIN_BATTLE_ROUND_MS=8000 \
    node "$SITE/server/index.js"
) >"$TMP/api.log" 2>&1 &
PIDS+=($!)
for _ in $(seq 50); do curl -fs "http://127.0.0.1:$API_PORT/api/abctrain/health" >/dev/null && break; sleep 0.2; done
if curl -fs "http://127.0.0.1:$API_PORT/api/abctrain/health" >/dev/null; then
  ok "отвечает"
else
  bad "API не поднялся"; tail -20 "$TMP/api.log"
  echo; read -r -p "Enter — выйти"; exit 1
fi

say "BattleSmoke: два клиента играют батл через локальный сервер"
if "$SMOKE" "http://127.0.0.1:$API_PORT" >"$TMP/smoke.log" 2>&1; then ok "$(tail -1 "$TMP/smoke.log")"; else bad "BattleSmoke"; tail -30 "$TMP/smoke.log"; fi

say "Комнаты: 12 виртуальных игроков, 90 секунд (наполняет рейтинг)"
node "$SITE/scripts/abctrain-battles-load.mjs" --url "http://127.0.0.1:$API_PORT" --steps 12 --seconds 90 2>&1 | tail -3

say "Страница рейтинга и приложение"
( cd "$SITE" && API_PORT=$API_PORT exec node_modules/.bin/vite --host 127.0.0.1 --port 5173 --strictPort ) >"$TMP/vite.log" 2>&1 &
PIDS+=($!)
for _ in $(seq 50); do curl -fs "http://127.0.0.1:5173/" >/dev/null && break; sleep 0.3; done
open "http://127.0.0.1:5173/abctrain/rating"
if [ -n "$APP" ]; then
  BIN="$(find "$APP/Contents/MacOS" -type f -perm -u+x | head -1)"
  ABCTRAIN_LIVE_URL="http://127.0.0.1:$API_PORT" "$BIN" >"$TMP/app.log" 2>&1 &
  PIDS+=($!)
  ok "приложение запущено и смотрит в локальный сервер: Live → Батл"
fi

say "Итог"
if [ ${#FAILED[@]} -eq 0 ]; then
  printf '\033[32mВсё зелёное.\033[0m Посмотрите рейтинг в браузере и экраны в приложении; если всё так — можно пушить.\n'
else
  printf '\033[31mНе прошло:\033[0m\n'; printf '  - %s\n' ${FAILED[@]+"${FAILED[@]}"}
fi
echo
read -r -p "Enter — остановить локальный сервер, страницу и приложение"
