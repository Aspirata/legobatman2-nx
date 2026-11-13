# Pre-Run 1 — исполнение

Цель пользователя: запускаемая LEGO Batman 2 на оригинальном Switch без требований
к производительности. Минимальная проверка: запуск настоящего guest entrypoint,
различимое меню и отклик контроллера на устройстве. Пустой NRO не засчитывается.
Первый уровень, звук и сохранение остаются последующими проверками P3 исходного
`first-working-build.md`, если они не блокируют загрузку меню.

Работа в `codex/pre-run-1`, в текущей папке с локальным ISO. Исходные исследования
сохранены отдельным коммитом 32058c5. Игровые данные и generated code вне Git.
Разрешены только локальные переносимые инструменты: WSL/системные установки
запрещены пользователем. Switch доступен с компьютера, способ подключения ещё
предстоит установить. Каждый этап заканчивается проверкой и отдельным коммитом.

## Последовательность

- [x] 1. Импорт: `tools/disc.py`, `tools/prepare-game.py`, синтетические тесты,
  `config/game-build.json`. Проверить ISO, извлечь потоково с хешами и защитой
  путей, сверить XEX с исследованием. Команда: `python -m unittest discover -s tests -v`.
- [ ] 2. Локальная среда: проверить доступные portable host tools и возможность
  devkitA64 без Linux; выбрать согласованный generator/runtime и лицензии.
  Доказательство: CLI help/version, host sample, отдельно sample NRO и uam.
- [ ] 3. Паспорт PE: дешифровка/LZX проверенным loader, секции, TLS, импорты,
  карта VA/RVA. Без этого нельзя запускать codegen.
- [ ] 4. Кодген: manifest закреплённого SDK, консервативная трансляция,
  доказательства каждого override, воспроизводимый список функций.
- [ ] 5. Host launcher: настоящий entry, память/aliases, потоки, файлы, профиль,
  ввод; clean build и журнал первого исполнения.
- [ ] 6. Ресурсы и графика: по первым блокирующим обращениям guest, оригинальные
  DAT, shader corpus и PM4 coverage, эталон меню на ПК.
- [ ] 7. Switch: Horizon runtime, deko3d/shader package, контроллер и файловая
  система, приложение NRO. Проверить на доступной консоли и записать журнал.
- [ ] 8. Приёмка Pre-Run 1: холодный запуск и меню с управлением, хеш NRO,
  версии окружения, известные ошибки. Только после этого ставить релизный тег.

## Связи и решения

1 → 3: manifest импорта определяет единственную сборку XEX; сверка обязательна.
2 → 3–7: upstream SDK и Horizon fork не взаимозаменяемы; pin устанавливается
после проверки исходников, не по совпадению номера версии.
3 → 4 → 5: адреса из loaded PE, generated ABI из выбранного runtime.
6 → 7: ключи/байты шейдеров должны совпадать на extractor/translator/runtime.
Отсутствие Linux не компенсировать выдуманной успешной сборкой.

## Журнал

- База: 13 синтетических тестов проходят; 274 GiB свободно на S:.
- Среда: CMake/Ninja/Clang/ReXGlue/devkitA64/uam/контейнеры отсутствуют в PATH;
  `wsl --list --quiet` подтверждает, что WSL не установлен.
- Этап 1: 19 тестов проходят. Реальный импорт извлёк 38 файлов, 4 487 085 430
  байт; ISO и XEX SHA-256 совпали с исследованием. 7 файлов runtime, остальные
  — системное обновление Xbox (сохранены в инвентаре, исключены из runtime).
  Повторить: `python tools/prepare-game.py "<путь к ISO>"`. Полный manifest
  локально в `work/analysis/import.json`. `git check-ignore` подтвердил исключение
  XEX/DAT/manifest. PE ещё не загружен, игра не запускалась.
- Host-проверка: переносимый официальный SDK v0.10.0 запускает CLI и loader.
  Диагностический codegen без overrides нашёл 3 unresolved tail branches.
  Исследование загруженного PE подтвердило одну отсутствующую границу функции;
  `port/overrides.toml` добавляет только её. Codegen exit 0: 46 271 mappings,
  159 recomp units, 325 файлов, 175 095 633 байт. Повтор с `--ignore-stamp`:
  325 unchanged. Команда с проверкой хешей: `python tools/codegen.py --regenerate`.
  21 синтетический тест проходит. Это промежуточный результат этапов 3–4;
  полнота callbacks/ABI ещё не проверена, чекбоксы этих этапов остаются открыты.
- Решение по порядку: пока настраивается portable compiler, официальный CLI
  использован для диагностического анализа. Host-ready не объявлен до sample
  compile/link. Официальный generator используется только со своим host runtime;
  перед Horizon нужно проверить API и регенерировать тем же форком.
- Найден Clang 20.1.3 в Windhawk. Его LLVM-MinGW libc++ не реализует требуемую
  SDK `std::chrono::clock_time_conversion`: реальный compile probe остановился
  в `rex/chrono/chrono.h`. SDK не патчился ради обхода. CMake 3.31.6 и Ninja
  1.11.1.4 установлены только в `work/toolchain/python`. Следующий эксперимент:
  совместимые MSVC headers/libs из официальных архивов, без установщика.
- Этап 2, host-ready: официальный MSVC/Windows SDK извлечён локально,
  `runtime_sample.cpp` реально скомпилирован, слинкован и запущен: Setup=0,
  process exit=0. Нужна app-local rexruntime.dll. Воспроизводимая загрузка
  пакетов — tools/bootstrap-host.py, SHA-256 в config/host-msvc-packages.json.
  24 теста проходят. Switch-ready остаётся открытым; devkitA64/deko3d/uam
  ещё не получены. Подробности и лицензии в docs/research/dependencies.md.
- Консоль: пользователь подключил SD через Hekate USB Tools, диск E:.
  Read-only проверка подтвердила atmosphere/bootloader/switch и 26,2 GB
  свободного места. Никаких записей на SD не выполнялось. Hekate mass storage
  не даёт автоматически запустить NRO: запуск потребуется проверить на консоли.
- Этап 5, host launcher: `CMakeLists.txt`, `src/main.cpp`, SHA-256 guard до
  ConstructRuntime, явный xenos plugin, строгое имя Default.xex, запрет TU.
  Команда `./tools/build-host.ps1` завершилась успешной компиляцией/линковкой;
  CTest host_identity 1/1 (known vector, mismatch, missing, неизменность файла).
  Windows host guest code использует SSE4.1, /Od, без fast-math и FP contraction.
  Все 24 Python-теста проходят. EXE: build/host/legobatman2.exe (106 969 088 байт
  до последующих изменений; точную версию определять хешем конкретного файла).
- Первый host-run без GPU выявил runtime target 0x82B071A8; после исследования
  добавлена граница 0xF4. Новая сборка с GPU прошла этот вызов, инициализировала
  interrupt callback, audio endpoint 48 kHz stereo и обратилась к архивам.
  Журнал: work/analysis/host-with-gpu.log. Exit 0xC0000409 от намеренного fatal
  dispatcher: следующий незарегистрированный метод 0x82B08598. Меню не достигнуто.
  Отсутствующие PATCH.DAT/GAME3..GAME10 не подменяются фиктивными файлами;
  сначала нужно отличить enumeration дополнительных архивов от обязательного чтения.
  Следующий эксперимент: таблица указателей и control flow 0x82B08598.
- Review host milestone: исправлены два Important замечания — app-local
  MSVC runtime closure (4 DLL) и pin SHA-256 compiler/DLL/builtin headers.
  Проверены bootstrap, повторная сборка, 25 Python tests, CTest 1/1; helper-test
  проходит с PATH только Windows/System32. Minor deferred: автоматическая
  проверка rejection именно launcher (пока CTest покрывает helper, успешный
  hash guard и запуск runtime проверены реальным запуском).
