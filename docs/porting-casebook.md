# Руководство по портированию и сборке

> Это технический архив: записи ниже отражают состояние на момент отдельных исследований и могут описывать остановленные или неподтверждённые эксперименты. Актуальный общий статус и известные проблемы приведены в [README](../README.md).



---

<a id="section-porting-casebook-md"></a>

# Рекомпиляция LEGO Batman 2: технические заметки для портирования

На 16 ноября 2026 ПК-версия проходится с периодическими подлагиваниями; исходная производительность Switch — около 3 FPS при 720p и 848×480. После оптимизаций переносов и фильтрации GPU-лога сопоставимые первые шесть окон начального пролёта дают около 4,18 FPS в run039 против 3,90 в run038; это ещё не приемлемая производительность. Подробнее: [фильтрация GPU-лога](research/switch-gpu-log-control.md). Старые артефакты в меню визуально напоминают проблему глубины; их причина не установлена.

**Работа приостановлена по просьбе пользователя.** После следующей пробы упрощения адресов same-origin stencil-переносов пользователь сообщил: «Вроде добавились новые артефакты». Возможная регрессия зафиксирована, её связь с новой пробой и отличие от старых артефактов не доказаны; влияние этой пробы на FPS и зависания не подтверждено. После просьбы остановиться изменена только документация. На консоли остаётся последняя пробная сборка, откат не выполнялся. Точное состояние, хеши и сохранённая предыдущая версия: [отчёт последней пробы](research/switch-transfer-same-origin.md#user-report-and-pause). Продолжать диагностику или менять консоль следует только после возобновления работы пользователем.

Портирование полностью выполняется с помощью ChatGPT 6.1 Sol в режимах High и Extra High. Тестовый ПК: AMD Ryzen 7 5700X, NVIDIA GeForce RTX 5060 Ti с 8 ГБ видеопамяти и 16 ГБ оперативной памяти. Тестовая консоль — Nintendo Switch OLED в портативном режиме: CPU 2397 МГц, GPU 1305 МГц, RAM 2500 МГц. Частоты установлены с начала тестирования игры; Governor отключён, поэтому консоль постоянно работает на этих частотах.

В качестве примеров организации технической документации использованы [NFSMW-NX](https://github.com/StevensND/nfsmw-nx/blob/main/docs/performance-history.md), [SA2-NX](https://github.com/ChanseyIsTheBest/sa2-nx) и [LEGO Batman recomp](https://github.com/surblazer/legobatman-recomp/blob/main/PORTING_NOTES.md). Их архитектура и показатели не являются результатами этого проекта.

## Карта документации

| Задача | Материал |
| --- | --- |
| Хронология и результаты | [История разработки](#section-development-history-md) |
| Инструменты и ограничения восстановления | [Зависимости](#section-research-dependencies-md), [сборка Switch](#section-research-switch-game-build-md), [закреплённые версии](../toolchain-lock.json) |
| Функции PPC и косвенные вызовы | [Границы функций](#section-research-function-evidence-md), [vtable leaves](#section-research-vtable-leaves-md), [forwarders](#section-research-vtable-forwards-md) |
| ПК: изображение, видео и звук | [Чёрный экран RTV](#section-research-rtv-black-screen-md), [сцена и XMA](#section-research-scene-freeze-md), [начальная диагностика](#section-research-pc-freeze-md) |
| Switch: производительность и эксперименты | [480p и очередь](#section-research-switch-480p-md), [профиль переносов](#section-research-switch-intro-queue-md), [выборочные операции](#section-research-switch-fence-operation-samples-md) |
| Проверки и установка | [guest probe](#section-validation-switch-guest-probe-md), [toolchain probe](#section-validation-switch-toolchain-probe-md) |

Файлы `work/` и `build/` в ссылках старых отчётов — локальные артефакты, исключённые из Git. Пути к ним позволяют найти оригинальные логи на машине разработки, но не гарантируют их наличия у читателя. Здесь приведены ключевые результаты, сохранившиеся в публичных текстовых записях.

## Архитектура и границы применимости

Выбран базовый XEX с SHA-256 `f13277b2645a24132f11cb45068de5692cae35fa57077c4801d622623421b68a`, Title ID `57520817`, Media ID `1BF17B45`, image base `0x82000000`. Все адреса функций и точечные проверки ниже относятся к этому образу, а не к любому региону или обновлению. Идентичность зафиксирована в [game-build.json](../config/game-build.json); проверка не заменяет анализа другого издания.

ReXGlue переводит гостевой PowerPC в C++, после чего гостевые единицы компилируются под целевую архитектуру. Runtime обслуживает вызовы системы, память, ввод, звук и GPU. На Windows используется Xenos backend с D3D12; на Switch — Horizon-адаптация SDK из NFSMW-NX и путь Xenos → Vulkan → Mesa NVK. Он сохраняет перевод команд, EDRAM и memexport. Наличие deko3d в другом порте не означает, что его обработчик пригоден для LB2: изученный путь первой игры не обеспечивал необходимый здесь memexport.

Генератор и host runtime закреплены на ReXGlue `c94f5ebd` / v0.10.0; Horizon SDK — на `nfsmw-nx` commit `20f8dc12`, Mesa — `1a8c1a66`. Это разные компоненты и ABI нужно проверять при обновлении каждого из них. Реальные компиляционные флаги, список патчей и проверки находятся в скриптах и CMake, а не определяются данным текстом.

## От образа до исполняемого кода

1. `tools/prepare-game.py` проверяет выбранный образ и готовит локальные данные. Извлечение проверяет границы XDVDFS и пути; файлы игры и сгенерированный код не включены в Git.
2. `tools/codegen.py` и `port/legobatman2_manifest.toml` задают исходный образ и параметры генератора. При пропущенной цели не следует добавлять адрес как функцию только потому, что он похож на указатель: сначала установить вход, все достижимые ветви, конец тела и соглашение о вызовах.
3. Для виртуальных методов проверяли происхождение таблицы через конструктор и запись в объект, затем полный граф соответствующей функции. Отдельно учитывали хвостовые переходы в уже зарегистрированные блоки и сохранение регистров. См. [evidence](#section-research-function-evidence-md) и списки [verified_leaves](../port/verified_leaves.toml), [verified_forwards](../port/verified_forwards.toml).
4. Скомпилировать гостевые единицы для host и проверить запуск до изменений графики. Для Switch историческая сборка включала 159 единиц recomp и две единицы инициализации/регистрации. Кэш объектов проверяется по манифесту, включая входы и компилятор; `./tools/build-switch-game.ps1 -RecompileGuest` отключает его повторное использование.

Пример наблюдённой проблемы: нажатие Start завершалось на отсутствующем `0x823A1A60`. Дизассемблирование показало четырёхинструкционный virtual tail через vtable `+0x58`; зарегистрировали границу длиной `0x10`. Тест проверяет передачу аргументов, результата и состояние регистров. Аналогично были найдены отдельные переходы сохранения и загрузки. Успех синтетического теста не доказывает покрытие всех маршрутов игры.

## Сборка и практические ограничения

Проект рассчитан на локальную Windows-среду. `python tools/project.py doctor` показывает доступность инструментов, `python -m unittest discover -s tests -v` запускает Python-проверки. Host bootstrap получает закреплённые пакеты, но требует подходящего Clang; варианты и версии описаны в [зависимостях](#section-research-dependencies-md).

Switch-сборка требует не только `python tools/setup-switch.py`: этот скрипт проверяет хеши и распаковывает **уже полученные** архивы из `work/toolchain/switch-packages`. Дополнительно нужны Horizon SDK, подготовленные подмодули, локальные CMake/Ninja и отдельно собранный Mesa SDK с `lib/libvulkan.a`. `tools/build-switch-game.ps1` завершится с ошибкой при отсутствии последнего. Историческая конфигурация Mesa собирала Vulkan/NVK без Gallium/OpenGL; подробности и версии инструментов приведены в [журнале сборки](#section-research-switch-game-build-md). Часть точных архивных манифестов и подготовительных скриптов осталась в игнорируемом `work/`: полностью воспроизводимый процесс на чистой машине пока не доказан.

Скрипт сборки проверяет чувствительные патчи и регрессии, затем вызывает CMake и упаковщик. `-Jobs` задаёт параллелизм; `-MesaSdk` переопределяет путь к готовой Mesa. Результат `build/switch-game/sdcard` включает локальные данные игры: его нельзя считать свободным от игровых материалов. Для запуска нужен hbmenu в режиме приложения; логи и userdata находятся рядом с NRO. Наличие ELF/NRO и прохождение host-тестов не подтверждают работу драйвера на консоли.

## ПК: что сломалось и как различили причины

### Медленное видео выглядело как зависание

В видеодекодере номер кадра рос, но количество кадров, декодируемых для догоняния аудиочасов, увеличивалось `1 → 7 → 17 → 39 → 89`; при догонянии callback изображения подавлялся. Бесконечный цикл в одном блоке не подтвердился. Причиной оказался безусловный `/Od`, перекрывавший `/O2` для гостевого кода. После удаления `/Od` повторно собрали 159 единиц с `/O2`: диагностический прогон выдал 600 последовательных кадров, примерно 30 кадров/с на проверенном участке. Это не измерение всей игры. [Детали](#section-research-pc-freeze-md).

### Чёрный экран: искать всю цепочку resolve

ROV давал картинку и временно служил контрольным обходом. В RTV кадр присутствовал в host render target, но swap-source readback состоял из нулей. При clear depth surface с `base=1328`, pitch 16 tiles и 4× MSAA оценка высоты 1024 вместо 360 давала `length=2048`, перекрывая живой color buffer через wraparound EDRAM. Последний resolve считал владельцем depth вместо color.

При `execute_unclipped_draw_vs_on_cpu=true` оценка стала `height=360`, `length=720`, а владелец последнего resolve снова указывал на color `base=0`. Изображение восстановлено. Важно сравнивать исходный host target, EDRAM ownership, shared memory и итоговую swap texture; readback сам синхронизирует GPU и меняет наблюдаемые тайминги. Полная последовательность — в [разборе RTV](#section-research-rtv-black-screen-md).

### Сцена остановилась, хотя GPU продолжал работать

Счётчики гостя продолжали расти; отдельно снятые аудиопакеты декодировались, но полный проход через SDK давал mono-ошибки. В interleaved XMA packet 31 не содержал новых кадров и указывал skip на packet 33 того же потока; прежний последовательный increment попадал в packet 32 другого потока. Исправление следует `GetPacketSkipCount(packet) + 1`, учитывая пересечение буферов и специальный sentinel. Проверили синтетическую цепочку и реальный контекст, затем перенесли исправление в host и Horizon. [Данные и ограничения](#section-research-scene-freeze-md), [патч Horizon](../port/rexglue-horizon-xma-packet-traversal.patch).

`readback_memexport_fast=false` отдельно устранил растянутую геометрию. Вылет ввода при ускоренном запуске привёл к сериализации вызовов ввода; конкурентный доступ был рабочей гипотезой, а не доказанной crash dump причиной. Игра проходится на ПК; сохраняются периодические подлагивания.

## Horizon: запуск и память

При первом запуске `std::filesystem::absolute` на Horizon трактовал `sdmc:/...` как относительный POSIX-путь и добавлял текущий каталог, получая повторный префикс. Использование `/switch/legobatman2/...` для game, userdata и cache сохранило корневой путь, а libnx разрешает его на SD. Тест проверяет фактические назначения и POSIX-нормализацию: [разбор](#section-research-switch-startup-crash-md), [тест](../tests/test_switch_paths.py).

Другой запуск исчерпал ресурсы отображения памяти при множестве мелких guest heap commits. Патч [batched-memory-commit](../port/rexglue-horizon-batched-memory-commit.patch) группирует работу; его проверяет `tools/test-switch-memory-commit.py`. Не переносите изменение вслепую на другую версию SDK: сначала проверьте контракт commit/decommit и гранулярность. Игра позднее загрузилась на устройстве, но это не доказывает отсутствие других утечек или корректность всех путей памяти.

## Switch: как измеряли 3 FPS

### Разрешение — не размер экрана

Игра перезаписывала результат `XGetVideoMode`: `sub_824B0D70` назначала 1280×720, `sub_824B0C90` могла повторно брать размеры из globals, а `sub_824167D0` создавала пять полноэкранных targets с фиксированными размерами. [resolution.cpp](../src/platform/switch/resolution.cpp) сохраняет исходные функции, восстанавливает выбранный режим и изменяет размеры только пяти проверенных вызовов. Shadow maps и другие поверхности оставлены прежними. Регрессионный `tools/test-switch-resolution.py` проверяет режим, ABI и неизменность других вызовов.

`VdSwap` логирует внутренние размеры отдельно от физического выхода: дисплей 720p не доказывает рендер 720p. Лог подтвердил 848×480 для проверенного пути, но не для каждого промежуточного прохода. Частота оставалась около 3 FPS, поэтому дальнейшее снижение разрешения само по себе не установлено как решение. [Исследование](#section-research-switch-480p-md).

### Где происходит ожидание

На run013 четыре окна геймплея длиной около 10,1 с дали 31–34 кадра. Подготовка команд занимала 171–189 мс за окно, `vkEndCommandBuffer` 0,6–1,2 мс, захват очереди 0,09–0,19 мс и `vkQueueSubmit` 8,12–8,35 с. В другом измерении было примерно 4145 draw и 613 transfer draws на кадр. Вступление run020 показало около 500–600 переносов владения на кадр; примерно 85% внутренних transfer draws были stencil-bit passes. Площади прямоугольников — записанные области, не измеренные пиксели или GPU-время.

Это backpressure в графическом программном тракте. Оно не доказывает, что увеличение FIFO, отключение ожидания или Mesa сами по себе исправят FPS: GPU может исполнять необходимую работу. Вложенные CPU-интервалы пересекаются и не суммируются; данные разных сцен нельзя сравнивать как один benchmark. Уже существующие в NVK слияния соседних диапазонов и границы `NO_PREFETCH/SYNC` не следует без доказательства удалять. [Очередь](#section-research-switch-intro-queue-md), [исследование](#section-research-switch-480p-md).

### Эксперименты и их смысл

| Изменение | Что обнаружено | Состояние |
| --- | --- | --- |
| Dynamic UBO | Ожидание переместилось в memexport fence/readback, плавность ухудшилась | Откат; [запись](#section-research-switch-dynamic-constants-md) |
| Специализация memexport | Нативный шейдер 9005 → 1147 инструкций, без существенного прироста FPS | [Данные](#section-research-switch-memexport-format-md); размер не равен времени выполнения |
| GPU timestamps | Диагностический run016 потерял устройство | Откат; [запись](#section-research-switch-gpu-timestamps-md) |
| FSI | При специализации возникли долгие паузы компиляции и хрип звука | Откат на FBO, контроль ~3 FPS; [запись](#section-research-switch-intro-fsi-md) |
| 4×→1× stencil | Целевая замена активировалась, улучшение FPS не наблюдалось | Откат; [запись](#section-research-switch-stencil-copy-md) |
| Безрезультатные point draws | 36–40 тыс. вызовов за 10 с исключены при строгих проверках, FPS 3,087 → 3,150 | Неубедительный прирост; default off, [запись](#section-research-switch-guest-draws-md) |

В последнем опыте проверялись конкретные shader hashes, маски записи, отсутствие memexport и активных запросов; общая похожесть draw не делает его безопасным для удаления. Предыдущий skip-work эксперимент признан неконтролируемым из-за пропущенных побочных эффектов и состояния кэша.

### Выборочные операции: куда смотреть дальше

Run026 использовал ограниченные принудительные завершения до и после `IssueDraw`/`IssueCopy`: 256 попыток, 223 допустимые. Без transfer опубликованные draw samples занимали 0,063–1,832 мс; copy — 0,218–2,925 мс; draw с ownership — 1,814–17,700 мс. Наиболее длинный пример совпал с подготовкой pipeline. Повторявшиеся clear с ownership были около 3,8–4,7 мс; одна пара с переносами заняла 6,05 мс против 0,355 мс без них.

Это принудительная последовательная CPU+driver+GPU задержка с дополнительной синхронизацией, не чистый GPU timestamp и не слагаемое стоимости обычного кадра. Публиковались не все группы, поэтому отсутствие операции в отчёте не делает её бесплатной. Тем не менее transfer и resolve — конкретное направление проверки. `direct_host_resolve` уже включён, но в исследованной реализации preflight переходит к обычному `DumpRenderTargets`, после которого выполняется compute-преобразование в гостевой буфер; объединённый прямой путь ещё требуется реализовать и проверить. [Методика и результаты](#section-research-switch-fence-operation-samples-md).

## Как продолжить и не потерять доказательства

Используйте одинаковую сцену, состояние сохранения, конфигурацию, частоты, версию NRO и прогрев кэша. Записывайте их вместе с hash сборки, логами, числом кадров, временами и состоянием экспериментов. Начальные окна компиляции анализируйте отдельно. Меняйте один параметр, сохраняйте исходные NRO/TOML и сравнивайте не только FPS, но и изображение, звук, корректность memexport, сохранения и ошибки устройства. Для переносов EDRAM нужны проверки формата, MSAA, алиасов, порядка записи и последующих чтений. Диагностические fences и readback меняют расписание работы, а счётчики draw и длина шейдера не измеряют стоимость GPU.

Пока нет доказанного устранения 3 FPS. Следующий разработчик может исследовать границы transfer/resolve и сопоставить их с конкретными функциями и shader keys из журнала run026, прежде чем менять backend. Похожая на ошибку глубины проблема главного меню документируется отдельно и сейчас не блокирует исследование производительности.

---

<a id="section-porting-md"></a>

# Исследование и последовательность переноса

Проверено 2026-11-13. Факты ниже относятся к опубликованным исходникам; работоспособность
LEGO Batman 2 из них не следует. Файл игры пока не предоставлен.

## Два подхода к графике

| Подход | Что даёт | Что требует |
| --- | --- | --- |
| Обработчик PM4/Xenos + deko3d, как LEGO Batman | Полезный путь к первому изображению с меньшим числом игровых перехватов | Остаются затраты разбора команд, необходима точная обработка resolve, tiling, форматов и шейдеров |
| Перехват Direct3D + очередь нативных команд + deko3d, как SA2 | Возможность убрать формирование и повторный разбор PM4 | Найти функции конкретного XDK, поля устройства, fences, callbacks и все используемые пути рисования |

Рабочая рекомендация: сначала диагностический запуск на ПК и инвентаризация графики,
затем deko3d. Выбор первого Switch backend делать по фактическому покрытию вызовов LB2.
Прямой Direct3D backend — перспективное направление оптимизации, не готовая замена.
Не переносить адреса функций, offsets устройства, исключения codegen или skip_msr из других игр.

## Что найдено в проектах

- [LEGO Batman README](https://github.com/surblazer/legobatman-recomp/blob/main/README.md):
  ReXGlue через Horizon-порт nfsmw-nx, deko3d, извлечение шейдеров из архивов TT.
  Автор отмечает ограниченное тестирование и неподдерживаемые memexport-шейдеры.
- [CMake](https://github.com/surblazer/legobatman-recomp/blob/main/legobatman/CMakeLists.txt):
  C++23, отдельные правила для точности floating point и LTO; host OpenGL shim для проверки deko3d.
- [Codegen](https://github.com/surblazer/legobatman-recomp/blob/main/legobatman/codegen.sh):
  проверки чтения регистров до записи и нетипичных функций; direct-call rewrite должен сохранять hooks.
- [Porting notes](https://github.com/surblazer/legobatman-recomp/blob/main/PORTING_NOTES.md):
  проблемы границ функций, виртуальных вызовов, памяти, синхронизации и TT-графики.
  Это перечень гипотез для LB2, а не готовые патчи.
- [SA2 porting guide](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/porting-guide.md)
  и [deko3d guide](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/deko3d-renderer-guide.md):
  поэтапная проверка нативных замен, сравнение кадров, настройка под A57 и измерения на консоли.
- [deko3d](https://github.com/devkitPro/deko3d) и
  [uam](https://github.com/devkitPro/uam): низкоуровневая графика Switch и компилятор шейдеров.
- [XEX2 layout](https://github.com/xenia-project/xenia/blob/master/src/xenia/kernel/util/xex2_info.h):
  источник формата для нашего анализатора заголовка.


## Контрольные точки

1. **Идентификация.** SHA-256 XEX, Title/Media ID, версия, размер; список архивов,
   обновлений и DLC отдельно. Не смешивать разные версии. Затем извлечь сведения
   о XDK, секциях, импортах, pdata и таблицах переходов средствами выбранного SDK.
2. **ПК.** Закрепить host SDK и зависимости; получить кодген, проверить границы
   функций и ABI; добиться меню с журналом ошибок. Не подменять неизвестные вызовы
   безусловным успехом. Приложить команду, версии и журнал к результату.
3. **Графический инвентарь.** Найти D3D entrypoints и структуру устройства,
   извлечь shader containers из реальных архивов LB2, составить список форматов,
   resolve/tiling, memexport и изменяемых ресурсов. Сохранить эталонные кадры ПК.
4. **Switch.** Закрепить Horizon SDK, devkitA64, libnx, deko3d и uam; реализовать
   память, потоки, файловый доступ, звук и ввод через проверенные компоненты.
   Первое меню на устройстве с корректным кадром — отдельный результат от успешной сборки.
5. **Игра.** Первый уровень, открытый город, переходы сцен, ролики, сохранение/загрузка,
   повторный запуск, подключение второго игрока и split-screen. Проверить звук и темп игры.
6. **Производительность.** Одни и те же сцены и ввод, штатные частоты, отдельно handheld/docked.
   Замерять CPU game/render time, GPU time, p50/p95/p99 времени кадра, память,
   загрузки и компиляцию шейдеров. Прогон с холодным и прогретым кешем.

## Оптимизации после базовой корректности

- Кеш шейдеров и pipelines с ключом из хеша игры, версий переводчика и параметров;
  неизвестный shader сохраняется для диагностики, не заменяется молча пустым.
- Удаление повторных state updates и копирования ресурсов только при доказанной
  неизменности; учитывать записи CPU, GPU и алиасы гостевой памяти.
- Сокращение barriers и resolves с проверкой зависимостей; повторное использование
  буферов по fences, без перезаписи данных, ещё используемых GPU.
- LTO, затем PGO по реальным игровым сценам; NEON только для измеренных узких мест
  и с проверкой результата относительно исходной функции.
- Не включать глобальный fast-math: сохранять PowerPC rounding, NaN и FMA-семантику.
- Динамическое разрешение рассматривать при GPU-ограничении, UI оставить чётким;
  нижнюю границу выбрать по замерам, а не копировать из первой игры.
- Уменьшать spin-waits только после анализа сигналов и порядка исполнения.
  Frame pacing не должен менять скорость логики игры.
- Для открытого города проверить I/O и streaming; кэширование ограничить бюджетом памяти.

Каждая оптимизация получает переключатель, сравнение до/после и возможность отката.
Успешный запуск на ПК или эмуляторе не является доказательством FPS на Switch.

## Повторное использование исходников

Пока сторонний код только изучается в игнорируемой папке. Перед интеграцией записать
commit, применённые патчи и лицензии каждого компонента. SA2 сообщает отдельные условия
для reNut; разрешение, данное автору другого порта, не считать автоматически нашим.
У LEGO Batman GitHub не распознал корневую лицензию: проверять происхождение конкретных
файлов перед переносом. Лицензия всего будущего порта пока не выбрана.

---

<a id="section-setup-md"></a>

# Окружение и локальные файлы

> Исторический снимок подготовки: Switch toolchain и игровая сборка позже были
> подготовлены. Текущее состояние и ссылки см. в [README](../README.md) и
> [журнале разработки](#section-development-history-md).

## Сейчас

Текущее окружение и проверенные команды: [dependencies.md](#section-research-dependencies-md).
Host SDK/Clang/CMake/Ninja уже работают из локальной папки; системные установки
запрещены при проверке. Сборка: `./tools/build-host.ps1`. Switch toolchain ещё
не подготовлен. Ниже сохранён снимок первоначального исследования, до реализации.

На машине проверены Python 3.13 и Git. CMake, Ninja, Clang, ReXGlue, devkitA64,
uam, DXC, SPIRV-Cross, Docker и Podman не найдены в PATH; DEVKITPRO не задан.
`wsl --list --quiet` сообщает, что WSL не установлен. Это снимок 2026-11-13;
повторная проверка: `python tools/project.py doctor`.

## Выбранный порядок подготовки сборки

1. Host-инструменты анализа можно собирать на Windows. Точные требования брать
   из закреплённого SDK, а не смешивать инструкции разных форков.
2. Для повторения сборки LEGO Batman нужен Linux host (либо WSL2/отдельная Linux-машина)
   и контейнер devkitPro. Его README указывает около 30 GB места; это оценка
   чужого проекта, не измеренный бюджет LB2. Установка WSL меняет ОС и может требовать перезагрузки.
3. После выбора Linux-среды закрепить контейнер и версии devkitA64/libnx/deko3d,
   собрать SDK host и Horizon отдельно. Обычный upstream ReXGlue не заменяет
   автоматически SDK с патчами nfsmw-nx.
4. Отдельно проверить uam простым shader-примером и минимальный deko3d NRO на устройстве.
   Только после этого подключать кодген игры. Пустой пример не считать портом игры.

Эта стадия не устанавливает системные пакеты и не запускает чужие setup-скрипты.
Исходники трёх исследуемых проектов загружены в `third_party/`; это не готовая среда сборки.

## Как восстановить исследуемые исходники

URL и полные SHA находятся в `references.json`. Для каждого репозитория:

```text
git clone <url> third_party/<name>
git -C third_party/<name> checkout --detach <commit>
git -C third_party/<name> rev-parse HEAD
```

Не выполнять upstream setup/codegen без чтения скриптов: там есть пути, зависимости,
патчи и адреса другой игры. Не накладывать патч на другую ревизию с игнорированием ошибок.

## Когда появится игра

Дождаться завершения копирования. Указать путь к ISO или извлечённой папке.
Для анализатора нужен именно `default.xex`, ISO сначала извлекается отдельным инструментом.
Не переименовывать игровые архивы. Не объединять DLC или title update с базовой игрой
до фиксации её версии. Сохранять результаты анализа локально в `work/`.

Сгенерированный C++, shader packages, XEX, ISO, игровые архивы и дампы исключены из Git.
`.gitignore` — удобство, а не проверка содержимого: перед публикацией просмотреть список
добавляемых файлов. Текущий host manifest находится в `port/legobatman2_manifest.toml`;
его нельзя смешивать с другой версией runtime без проверки ABI и регенерации.

---

<a id="section-research-dependencies-md"></a>

# Проверка зависимостей Pre-Run 1

Системные установки запрещены при проверке; пакеты хранятся в `work/toolchain`.
Все перечисленные версии проверены локально 2026-11-13.

| Компонент | Закрепление и результат |
| --- | --- |
| Host ReXGlue | v0.10.0 / c94f5ebd, полный SHA и URL в toolchain-lock.json. CLI help/version и loader работают; sample с Runtime::Setup в tool_mode скомпилирован, слинкован и запущен, status 0. |
| Исследовательский upstream | 546a082a из references.json — другая ревизия. Не используется как runtime для release generator. |
| Horizon candidate | nfsmw-nx 20f8dc12. SDK BSD-3-Clause по sdk/LICENSE и THIRD_PARTY_NOTICES; приложение GPL-3.0. Исходники скачаны, не интегрированы. |
| LB1/SA2 | Только исследование. LB1 patches не применялись, renderer не копировался; условия конкретных файлов проверять до переноса. |
| Clang/LLD | 20.1.3 из установленного Windhawk LLVM-MinGW, binaries i686, target для host — x86_64-pc-windows-msvc. По умолчанию использует несовместимую libc++; нужны MSVC headers/libs. |
| Microsoft MSVC | Семейство 14.44.17.14, layout 14.44.35207. Точные версии каждого архива, URL Microsoft и SHA-256 в config/host-msvc-packages.json. Никакие VS installer/MSI не запускались. |
| Windows SDK | NuGet Microsoft.Windows.SDK.CPP + .x64 10.0.26100.9457, layout headers 10.0.26100.0, SHA в tools/bootstrap-host.py. Условия Microsoft; файлы только для локальной разработки. |
| CMake/Ninja | PyPI cmake 3.31.6 / ninja 1.11.1.4, `pip --target work/toolchain/python`. |
| Capstone | 5.0.6, только локальная диагностика PowerPC big-endian; не включён в приложение. |

Официальный SDK и его транзитивные лицензии лежат в `win-amd64/licenses`.
До распространения приложения нужна отдельная проверка фактически слинкованных
компонентов, FFmpeg configuration и graphics/audio dependencies Horizon.

## Проверенные команды

```
python -m pip install --target work/toolchain/python cmake==3.31.6 ninja==1.11.1.4 capstone==5.0.6
python tools/bootstrap-host.py
```

bootstrap сверяет каждый архив до извлечения и не заменяет отличающиеся файлы.
Для оболочки сборки: `. ./tools/host-env.ps1`. Найденный Clang использовался
с `--no-default-config --target=x86_64-pc-windows-msvc`.

bootstrap также создаёт локальный clang-cl.exe из найденного clang-20.exe,
копирует DLL-зависимости и builtin headers. Это MSVC command-line frontend того
же Clang 20.1.3. С ним CMake непосредственно запускает Microsoft linker;
нестандартные link rules больше не нужны. ABI/feature checks выполняются
обычным configure. Не задавать CMAKE_CXX_COMPILER_WORKS вместо реальной проверки.

Сборка приложения и тестов: `./tools/build-host.ps1 -Jobs 4`. Первое успешное
чистое построение из root проекта: clang-cl x64, Microsoft link, 169 шагов,
CTest 1/1. Windows CPU baseline SSE4.1 соответствует SDK. Без этого флага SIMDe
уходил в fallback __builtin_roundevenf, отсутствующий в Windows CRT; настройка
таргета исправлена без замены математики/guest функций. /Od задан для guest code,
производительность этой версии не оценивается.

Sample: `work/analysis/runtime_sample.cpp`, object/exe и журналы локальны.
Он создаёт Runtime, передаёт InitializeKernel, tool_mode=true, вызывает Setup,
печатает `Runtime Setup: 00000000` и завершает exit 0. rexruntime.dll должен быть
рядом с exe. Sample не выполняет гостевой код и не доказывает запуск игры.

## Switch tools: локальная сборка проверена, устройство ожидается

Восемь официальных пакетов закреплены в config/switch-packages.json:
devkita64-gcc 16.1.0-1, binutils 2.46.0-1, newlib 4.6.0.20260123-4,
rules 1.1.1-1, libnx 4.12.0-1, deko3d 0.5.0-1, switch-tools 1.13.1-1,
uam 1.1.0-2. Windows host tools реально выполнили version/help.

Основной сервер и зеркало отвечали Cloudflare403 из PowerShell/curl, в том
числе через предоставленный прокси http://127.0.0.1:10808. Скачивание тех же
точных URLs через in-app browser прошло. SHA-256 архивов совпали с индексами
dkp-windows.db/dkp-libs.db. Все восемь GPG-подписей дали VALIDSIG для ключа
BC26F752D25B92CE272E0F44F7FD5492264BB9D0 из официальной pacman-инструкции.
Индексы, подписи и журналы — локально в work/toolchain/switch-packages и work/analysis.

`python tools/setup-switch.py --archives <папка архивов>` проверяет весь набор
до извлечения; копирует только opt/devkitpro в work/toolchain/switch-root.
Никакие package-manager install scripts, Windows installer или registry edits
не выполняются. Переменные DEVKITPRO/DEVKITA64 задаются только в процессе сборки.

`./tools/build-switch-probe.ps1` собрал отдельный C++17 libnx/deko3d тест:
ARM64 PIE ELF → NACP → NRO, плюс свой минимальный GLSL → DKSH через uam.
Сценарий GPU/input описан в docs/validation/switch-toolchain-probe.md.
Физический запуск ещё не проверен, поэтому Switch-ready целиком не закрыт.
Стоковый uam проверяет инструменты; game shader translator и необходимые
scheduling patches ещё не интегрированы. Horizon runtime также не собран.

---

<a id="section-research-switch-game-build-md"></a>

# Full Switch game build — compiled, 2026-11-14


## Source and toolchain state

- Horizon SDK: `third_party/nfsmw-nx/sdk`, nfsmw-nx20f8dc12.
  Filled original vendored files from exact upstream SDKc94f5ebd and sixteen
  submodules; no existing fork files overwritten. Archive pins/checksums:
  `work/toolchain/horizon-deps/manifest.json`.
- Applied `port/rexglue-horizon-xma-packet-traversal.patch` to this SDK.
  It preserves UINT32_MAX end sentinel and fork-specific split-frame handling.
- Applied `port/rexglue-horizon-batched-memory-commit.patch` after console run
  007 exhausted mapping resources during small guest heap commits. The build
  script runs `tools/test-switch-memory-commit.py`; console retest is pending.
- Switch graphics also applies the GPU profile/stages patches. Keep
  `port/rexglue-horizon-dynamic-constants.patch` unapplied: console run014
  regressed and the experiment was rolled back. See
  `docs/research/switch-dynamic-constants.md` for evidence and status.
- Applied `port/rexglue-horizon-memexport-format.patch` for the exact LB2
  copy shader with guarded format specialization. The build checks both SDK
  translation units and runs the guard regression. Run015 shows no material FPS gain;
  see `docs/research/switch-memexport-format.md` for proof and validation.
- Run015 confirms native shader reduction but still about3FPS. Run016's GPU
  timestamp experiment ended in device loss; its patch is reversed and normal
  builds require it absent. See `docs/research/switch-gpu-timestamps.md`.
- The current console trial uses the restored baseline NRO plus the existing
  FSI render-target path with classic render passes. The temporary TOML is
  separate from normal defaults. See `docs/research/switch-fsi-trial.md`;
  hardware correctness and FPS are pending.
- The initial-cutscene follow-up applies the FSI specialization and pipeline
  CPU-profile patches, preserving classic FSI synchronization and using a
  separate FSI pipeline-description file. See `docs/research/switch-intro-fsi.md`.
  A successful host/ARM64 build does not establish console FPS.
- Full GPU path for first game build: SDK Xenos/Vulkan + Mesa NVK. LB1's
  deko3d renderer omits memexport, a demonstrated LB2 correctness dependency.
  This preserves the complete command processor/shader translation path first.
  Performance and native deko3d work follow a functional console baseline.
- Mesa source: `work/toolchain/mesa-source/mesa-switch-1a8c1a66d6fd8d65f10107c4627ffc3606ba5631`.
  Zip SHA256f0c75b335e1a62b574db5e27bc5c765080f810a73cef1ae7c74e5ee308e9278f.
  Applied nfsmw's exact Mesa patch after normalizing its CRLF to LF (all
  context then matched; original blob36cadd70… was checked). Also applied
  `port/mesa-host-clang-jump-buffer.patch` for the native Windows tool only.
- Portable MSYS2/MINGW64 in `work/toolchain/msys64`: GCC16.2, Clang/LLVM23.1.5,
  Meson1.12.1 plus dependencies. Downloaded with pacman **download only**;
  package signatures checked with task-local keyring. Extracted only mingw64
  and selected usr tool files; package install scripts not run. Pins/hashes:
  `work/toolchain/mesa-msys-packages.json` (77 archives).
- Rust1.100.0 GNU Windows host + aarch64-unknown-linux-gnu target in
  `work/toolchain/rustup`/`cargo`; bindgen0.72.1, cbindgen0.29.2.
  No Windows PATH change. All environment changes are per build process.

## Active builds and logs

1. All generated LB2 units -> ARM64 objects:
   `build/switch-game-guest`, CMake source `work/analysis/switch-game-build`.
   Generator is still pinned official0.10; Horizon headers used for all units.
   The GCC alias correction is force-included. PCH, O2, Cortex-A57, PIE,
   local-exec TLS, no FP contraction/strict aliasing. Jobs2.
   Log `work/analysis/switch-game-guest-build.log`; completed successfully,
   all161 objects (159 recomp units + init/register), 162/162 steps including PCH.
   Hash manifest `work/analysis/switch-game-guest-objects.json` includes sources
   and SDK headers. Switch CMake accepts it through LB2_PREBUILT_GUEST_MANIFEST,
  verifies every hash and object count, and links all imported objects directly.
  Cache validation also pins the compiler binary, toolchain and compile options,
  plus SDK CMake files/dependency manifest (756 input files in the final cache).
2. Mesa Vulkan SDK: `work/analysis/run-mesa.ps1`, log
   `work/analysis/mesa-unified-build.log`; completed successfully (943 steps).
   Native mesa_clc.exe and vtn_bindgen2.exe also built successfully.
   The full Gallium/OpenGL path required additional Switch-specific Nouveau
   headers and is not used by this title. Reconfigured Vulkan/NVK only:
   gallium-drivers empty, EGL/OpenGL/GLES disabled, Vulkan nouveau retained.
   No shader translation or memexport functionality was removed from NVK.
   Needs elevated execution for GNU/Rust linking: the restricted run gave
   empty linker errors, while the identical native Rust sanity program linked
   and ran successfully with elevation. No automatic approval rejection.
   Stage path is explicitly checked inside the Mesa source directory before
   upstream build script's recursive removal of its own install stage.

Installed Mesa SDK location:
`<mesa-source>/mesa-unified-install/opt/devkitpro/portlibs/switch`.
The local run script sets toolchain paths, Rust paths, SOURCE_DATE_EPOCH,
ALLOW_DIRTY and MSYSTEM. Source cross files were adapted from C:/msys64 to
workspace paths. Native build wrapper now retries Ninja incrementally before
upstream's full script, keeping completed native tools.

Zstd 1.5.7 is a genuine ARM64 static library in the portable devkitPro
portlibs directory, built from f8745da6ff1ad1e7bab384bd1f9d742439278e99
(archive SHA256e3b677f3edf60df600b1cc56f629e8cac93d5f2bd421d8f71c33d88abd3bff46).
Expat and zlib were built and staged by Mesa. None are placeholder archives.

## Full game target compiled and packaged

`switch/CMakeLists.txt` builds SDK/runtime, guest units and
`src/platform/switch/{main,game_identity,input_guard}.cpp`, packages
`legobatman2.nro`. All three title sources have compiled individually to
ARM64 (`work/analysis/switch-compat/entry-*.o`), followed by the complete link.

- Switch loader verifies XEX SHA256 with the SDK's portable SHA implementation.
- Data: `sdmc:/switch/legobatman2/game`, userdata/cache adjacent and separate.
- Speed1x, audio enabled, exact draw extents and current memexport readback.
- Input wrappers use static ELF --wrap, serializing original XamInput exports;
  no Windows GetProcAddress/VirtualProtect code enters Switch target.
- No game hooks or fake successful stubs added.

Configured `switch/` successfully with pinned devkitA64 toolchain,
DEVKITPRO and REXGLUE_SWITCH_NVK_SDK pointing at the stage above. Toolchain's
DEVKITPRO must also be supplied through the process environment for CMake's
try_compile subprojects (otherwise it falls back to /opt/devkitpro).
Full SDK/runtime/GPU/title build: `build/switch-game`, log
`work/analysis/switch-game-build.log`. Libmspack's source symlinks from the
verified archive were materialized as file copies on Windows; before that
the compiler saw the symlink target string as C code. The materializer checks
archive hashes, confines targets to each module, and preserves modified files.

Repeat with `tools/build-switch-game.ps1`. The final step prepares and hashes
`build/switch-game/sdcard/switch/legobatman2`, copying only the seven runtime
disc files, never Xbox system updates or PC saves.

Completed all535 SDK/title steps, static ARM64 PIE link and NRO packaging.
NRO size70,277,317 bytes; SHA256
31b5977b9572c7f3013c1cd51183d7069806a57fd6f2342ab12a4dc8d18b4c53.
The repeat build script also completed successfully, followed by hash-verified
SD packaging. NRO segment/asset bounds and the LEGO Batman 2 NACP title passed.
Complete SD manifest with every file's SHA256:
`build/switch-game/sdcard-manifest.json`. Readelf confirms AArch64 ELF64 PIE;
nm reports zero undefined symbols. Final ELF includes guest functions,
vkCreateInstance, XMA packet traversal and linked input wrappers.
The standalone portable Switch identity helper passed the same known-vector,
mismatch, missing-file and no-modification checks as the PC implementation.
Python26/26 and CTest6/6 passed. PC EXE SHA256 still matches the saved reference:
305d4ab2a52bb0802a17627a00c4adcea3222581b8c91cd33068d1d3a5d90d6d.

Real console boot/gameplay remain unverified. No Switch FPS has been claimed.

## Final review fixes

Review found that continuation scanning could discard a stream's overshoot
across the two input buffers, and treated the 0xFF full-packet sentinel as
a 256-packet jump. The final source patch carries the scanned index across
buffers and preserves Decode's one-packet handling of 0xFF, without changing
the SDK member API or the guest ABI.
`tools/test-switch-xma-chain.py` compiles the actual SDK scanner, advance and
swap function bodies with real packet-header helpers and synthetic buffers.
Before fixing: cross-buffer and sentinel cases fail, continuation-chain passes.
After fixing: all three pass. Logs: `switch-xma-chain-{red,green}.log` in
`work/analysis`. The build script runs this regression before cross-building.
Final source rebuild and SD verification: `work/analysis/switch-game-final-build.log`.
Delivery build (including automatic source regression):
`work/analysis/switch-game-delivery-build.log`. The hash above identifies this
last packaged NRO; build timestamps may change NRO hashes on later rebuilds.

## Native Mesa compiler fix evidence

Mesa used __builtin_setjmp on MinGW with a system jmp_buf, incompatible with
Clang's `void *[5]` builtin storage. The tiny failing compile is saved in
`mesa-jump-red.c/log`; `mesa-jump-green.c` compiled and completed a real
setjmp/longjmp round trip. The patch changes storage only for MinGW Clang
without UCRT, leaving Switch/GCC storage unchanged. ABI reference:
https://clang.llvm.org/doxygen/Sema_8h_source.html (BuiltinSetjmp/Longjmp).

---

<a id="section-research-function-evidence-md"></a>

# LB2 — границы функций

Все адреса относятся только к XEX SHA-256
`f13277b2645a24132f11cb45068de5692cae35fa57077c4801d622623421b68a`.
Loader: официальный ReXGlue v0.10.0, архив и exe закреплены в toolchain lock.
Это host baseline, не доказательство совместимости generated code с Horizon fork.

## 0x826B6AB8, размер 0x19C

Первый анализ без overrides завершился exit 1: UnresolvedCall из `0x826BBF90`,
`0x826BBFB8`, `0x826BBFE0`. Все три инструкции — прямой tail branch к
`0x826B6AB8`; wrappers передают объект через r3 (копируя r4), два предварительно
записывают float-поля. Это подтверждение назначения входа, не поиск похожего адреса.

Границы проверены Capstone 5.0.6 в big-endian PPC32 по памяти **собственного**
дочернего процесса SDK после загрузки XEX. ReadProcessMemory использован только
для диагностической копии mapped PE; это не raw PE и не стабильный формат пакета.
Скрипт исследования и полные инструкции — локально в `work/analysis/`.

Вход начинает чтением поля объекта, не требует stack frame, сохранённых
nonvolatile-регистров или состояния другого фрагмента. Предыдущая функция
заканчивается tail branch по `0x826B6AB4`. Единственная внутренняя ветвь по
`0x826B6AD8` ведёт к `0x826B6B94`. Оба пути заканчиваются `blr`:
`0x826B6B90` и `0x826B6C50`. Следующее слово `0x826B6C54` — padding;
по `0x826B6C58` начинается отдельный пролог с сохранением LR/регистров.
Размер вычислен по последнему достижимому return: `0x826B6C54 - 0x826B6AB8`.
Внутри нет вызовов, indirect branches и записей nonvolatile; share_registers
не включён. Регистр r3 остаётся гостевым адресом, native replacement не вводится.

Проверка: codegen с этой границей завершился exit 0 за 25,2 секунды,
устранив ровно три исходные ошибки без `--force`. Получено 46 271 mappings,
159 единиц recomp C++, 325 файлов. Успех анализа не доказывает runtime-поведение и полноту
функций, вызываемых только через ещё не найденные pointers.

## 0x82B071A8, размер 0xF4

Первое выполнение настоящего guest entrypoint остановилось на этом адресе:
журнал `work/analysis/first-host-run.log`, fatal invalid/unregistered function.
В загруженном образе ровно одна ссылка BE32: слот `.rdata` `0x8218BE1C`,
в окружении других адресов методов. Это наблюдаемый indirect-call target,
а не произвольное слово данных, объявленное указателем по совпадению.

Вход читает поля объекта из r3, использует volatile r3–r11 и CR, не имеет frame
или записей nonvolatile. Тело разветвляется по наличию объектов в полях;
каждый путь кончается tail `bctr` через метод по vtable offset 0x1C.
Последний tail branch — `0x82B07298`, затем padding по `0x82B0729C`;
следующий самостоятельный пролог начинается по `0x82B072A0`.
Полный диапазон `[0x82B071A8, 0x82B0729C)` равен 0xF4, внутренних вызовов
с возвратом нет. Сохраняется исходный PPC-код, native hook не добавляется.
Точный вызывающий guest PC пока не записан release runtime; источник цели
подтверждён runtime dispatch и слотом таблицы, а не независимой трассой caller.
Проверка: повторить codegen и host-run, этот адрес должен быть зарегистрирован.
Повторный host-run с xenos действительно прошёл его и дошёл до файлового ввода
и следующего отсутствующего метода 0x82B08598 (host-with-gpu.log).

## 0x82B08598, размер 0xFC

Наблюдался как fatal dispatcher target в host-with-gpu.log после инициализации
GPU/audio и файловых обращений. Ссылка в `.rdata` находится по `0x8218BEE4`,
рядом с методами `0x827B39D0`, `0x82B21938`, `0x82B21A68`, `0x8293D988`,
`0x82B07110`. Точное место вызывающего guest PC в release runtime не записано.

Полный CFG исследован по loaded PE. Вход сохраняет объект в volatile r11,
читает его поля, имеет условный `beqlr` по `0x82B08658` и tail-dispatch пути
через vtable slot 0x1C. Последний достижимый `bctr` — `0x82B08690`;
поэтому диапазон `[0x82B08598, 0x82B08694)` = 0xFC. Отдельный последующий
`blr` по 0x82B08694 в границу не включён; следующий frame начинается по
0x82B08698. Внутренних linked calls, frame и nonvolatile writes нет.
Добавлена только граница; регистры остаются в ctx. Проверка — повтор codegen,
линковка и отсутствие этого fatal target при следующем запуске.
Проверено: следующий run с PATH только Windows/System32 прошёл этот метод,
остановился на 0x82B1F160 (host-vtable-2.log). App-local CRT DLL загружены.

## 0x82B1F160, размер 0x18

Наблюдаемый dispatcher target в host-vtable-2.log. Единственная проверенная
ссылка `.rdata` по `0x8218CD1C`, рядом со sibling thunk 0x82B1F148 и другими
методами. Все шесть инструкций разобраны: чтение поля объекта 0x48(r3),
this adjustment к полю 0x40, чтение vtable slot 0x14, mtctr и tail bctr по
0x82B1F174. Следующий пролог начинается по 0x82B1F178. Это законченный
volatile-only thunk, без frame, linked calls или скрытых live nonvolatile.
Override 0x18 сохраняет оригинальную диспетчеризацию; native hook отсутствует.
Проверено: host-vtable-3.log проходит его; новый fatal — 0x82553C20 в другом
guest thread, до первого показанного кадра.

## 0x82553C20, размер 0x24, нестандартный вход r10

Runtime target подтверждён host-vtable-3.log. BE32-указателя в данных нет:
адрес создаётся кодом `lis r7,0x8255; addi r7,r7,0x3C20` по 0x825A3F14/24
и передаётся в sub_82553F20 по 0x825A3F30. Исследование registration function
показывает сохранение r7 через r26 в поле +0x0C записи по 0x82553F8C.
Это адрес зарегистрированного callback, не результат слепого сканирования данных.

Тело 0x82553C20..0x82553C40 состоит из 9 инструкций: читает контекст через
r10, переносит r3 в r4, задаёт r5=4/r6=3, получает virtual method offset 8
и завершает tail bctr. По 0x82553C44 padding, по 0x82553C48 другой пролог.
Вход r10 — существенная часть ABI; нельзя делать его обнуляемой локальной
переменной. Все register-local flags в текущем manifest отключены, исходный
ctx.r10 сохраняется. Native replacement и оптимизация direct calls не применяются.
Проверка: host-callback.log проходит этот target; новый callback target 0x825BE648.

## 0x825BE648, размер 0x28

Наблюдался в host-callback.log в worker guest thread. Указатель из `.rdata`
по 0x820DE130; тело полностью разобрано. По selector r4 выбирает условный
возврат или tail call к 0x826A1C28 (с this+0x174), либо к 0x825BE530.
Последняя достижимая инструкция — tail branch по 0x825BE66C, поэтому размер
0x28. Следующий отдельный blr по 0x825BE670 не включён; затем padding и
пролог следующей функции по 0x825BE678. Frame, linked calls и nonvolatile
writes отсутствуют. Изменена только зарегистрированная граница.
Host-callback-2.log прошёл его и остановился на следующем target 0x82242800.

## 0x82242800, размер 0x28

Runtime target в host-callback-2.log, ссылка из `.rdata` по 0x8200650C.
Тело проверяет глобальный указатель, условно возвращается через beqlr или
настраивает volatile аргументы r3–r6 и выполняет прямой tail call по
0x82242824 к 0x823EAE98. Размер по последней достижимой инструкции — 0x28;
отдельный blr 0x82242828 и padding не включены. Следующий вход 0x82242830.
Frame/nonvolatile writes/linked calls нет. Не объявляется типом RTTI по
соседним float-данным: доказательства здесь runtime target и разобранный CFG.
Проверено: host-virtual-early.log проходит этот target, следующий — 0x825CB588.

## 0x825CB588, размер 0xC

Наблюдаемый virtual target в host-virtual-early.log. Слот `.rdata` по
0x820E23A8 находится внутри таблицы методов. Три инструкции lis/addi/blr
возвращают адрес строки в `.rdata` 0x820E23C4 (имя Sound_Start).
После return 0x825CB590 идёт padding, следующий getter начинается по
0x825CB598. Это полностью разобранная leaf-функция, не копия native getter;
сохраняется оригинальная генерация C++.
Host-name-getter.log прошёл getter, следующий virtual target 0x82788928.

## 0x82788928, размер 0x28

Dispatcher target в host-name-getter.log, pointer slot `.rdata` 0x82151210.
Полный CFG: проверка поля 0x498(r4), затем маски поля 0xFC у найденного
объекта; возвращает r3=0/1. Условный return 0x82788944 или return 0x8278894C.
Внутренний переход направлен к 0x82788948, frame/calls/nonvolatile writes нет.
Следующий пролог начинается по 0x82788950, размер 0x28 определён по return.
Host-predicate.log проходит метод, следующий target — 0x82AA9AE8.

## 0x82AA9AE8, размер 0x18

Dispatcher target в host-predicate.log, `.rdata` slot 0x82185BE4. Все шесть
инструкций проверены: сохраняет байт аргумента r4 в поле 6(r3), получает
вложенный объект из 0x18(r3), загружает его virtual method offset 0x24 и
выполняет tail bctr по 0x82AA9AFC. Следующий пролог начинается по 0x82AA9B00.
Не трогает frame/nonvolatile registers; используется исходный guest-код.
Host-forward-setter.log прошёл этот метод, следующий target — 0x82AAA168.

## 0x82AAA168, размер 0x18

Наблюдаемый target в host-forward-setter.log, `.rdata` slot 0x82185C64.
Полностью разобранные 6 инструкций: byte store в 6(r3), переход к вложенному
объекту в 0x20(r3), чтение его vtable slot 0x24 и tail bctr по 0x82AAA17C.
Следующий отдельный метод начинается по 0x82AAA180. Nonvolatile/frame/calls нет.
Сходство с предыдущим wrapper не использовалось вместо проверки этих байтов.
Host-reviewed-leaves.log прошёл его, следующий target — 0x82AAA180.

## 0x82AAA180, размер 0x14

Наблюдаемый target в host-reviewed-leaves.log. Проверены все 5 инструкций:
переход к вложенному объекту из 0x20(r3), чтение его vtable slot 0x5C,
mtctr и tail bctr по 0x82AAA190; далее padding. Следующий вход 0x82AAA198.
Frame/nonvolatile writes/linked calls отсутствуют. Исходная диспетчеризация
сохранена, записывается только подтверждённая граница.
Pointer slot: 0x82185C9C. Host-forward-getter.log проходит метод, следующий
target — 0x82556980, один из отдельно отложенных prefix cases.

## 0x82556980: prefix 0x14, существующий shared return 0x82556994

Наблюдался в host-forward-getter.log; `.rdata` slot 0x820D821C. Полный
логический CFG занимает 0x1C: читает указатель из 0x18(r3), сравнивает с нулём,
для non-null возвращает слово 0x0C у записи; null-ветвь идёт к li r3,0/blr
по 0x82556994..0x82556998. Этот return-block уже зарегистрирован SDK отдельно.

Добавляется только непересекающийся prefix [0x82556980,0x82556994), а не
объявление всего CFG независимой leaf-функцией. Условный tail transfer к
существующему блоку сохраняется. Frame и nonvolatile writes отсутствуют;
полный ctx остаётся общим, LR при tail branch не меняется. CTest guest_prefix
проверяет null и non-null пути на синтетической big-endian guest memory,
результат r3, сохранность LR и r14. До добавления prefix тест не линковался
из-за отсутствующего __imp__sub_82556980; проверка после реализации обязательна.
После добавления: CTest guest_prefix прошёл оба пути; host-prefix-forwards.log
прошёл target, следующий — 0x827580D8.

## 0x827580D8: prefix 0x44, существующий tail block 0x8275811C

Observed target: host-prefix-forwards.log. Таблица/конструктор отдельно проверены
в work/analysis/review-held-leaves.json. Полный CFG — 0x48, но последняя
инструкция b 0x825F8D48 по 0x8275811C уже зарегистрирована как отдельный вход.
Включается только prefix [0x827580D8,0x8275811C), существующий block сохраняется.
Trailing blr по 0x82758120 недостижим и не включён.

Метод сравнивает bit15 поля u16 +0x5A с аргументом r4. При изменении использует
addic/subfe (XER carry), сохраняет остальные биты, проверяет объект из +0x5C,
затем tail-calls 0x825F8C68 или через существующий block 0x825F8D48.
Нет frame/nonvolatile writes. В guest_prefix_test тестовые strong hooks
двух tail-целей фиксируют маршрутизацию и guest pointer 0x500; они не входят
в target игры. Проверяются оба tail-пути, unchanged early return, BE16 bits,
carry, LR/r14. До реализации отсутствовал symbol __imp__sub_827580D8 (RED).

## Start/controller callback 0x823A1A60, size 0x10

Observed fatal target in `work/analysis/host-freeze-baseline.log` at 21:04:44.
Selected loaded image has an exact four-instruction virtual tail dispatch:
`lwz r12,0(r3); lwz r11,0x58(r12); mtctr r11; bctr`.
Next distinct forwarding entry begins at 0x823A1A70. Data pointer to this entry
is present at 0x82C6D40C. No stack frame, linked call or nonvolatile writes.
`guest_start_test` invokes the original generated entry on synthetic object,
vtable and dispatch memory, checking slot +0x58, arguments, result, LR and r14.
Before the override the test fails to link the missing generated entry.

## Save-selection callback 0x823A1AA0, size 0x10

Observed fatal target in `build/host/logs/legobatman2_006.log`, 21:24:11,
after the previously fixed Start route. Original instructions at 0x823A1AA0:
`lwz r12,0(r3); lwz r11,0x40(r12); mtctr r11; bctr`.
Data pointer at 0x82C6D3AC; next distinct thunk begins at 0x823A1AB0.
Same no-frame/no-nonvolatile-write ABI as the Start callback, different slot.
`guest_start_test` now tests both, clearing slot +0x58 before checking +0x40
to reject an accidental duplicate of the first dispatch. RED: missing symbol
`__imp__sub_823A1AA0` in `work/analysis/save-red.log`.

## Overwrite-confirmation callback 0x823A1AD0, size 0x10

Observed fatal in `work/analysis/render-baseline.log`, 21:43:41, when the
testing confirmed overwriting an existing save. Selected loaded image contains
`lwz r12,0(r3); lwz r11,0x54(r12); mtctr r11; bctr` at 0x823A1AD0–DC.
Data pointer at 0x82C6D3BC; next thunk starts at 0x823A1AE0. No stack frame,
linked call or nonvolatile writes. Only this observed entry is added.
`guest_start_test` clears the previous +0x40/+0x58 slots before invoking
the original generated entry through +0x54, checking arguments, return,
LR/r14 and CTR. RED: linker LNK2019 for `__imp__sub_823A1AD0` before the
override; GREEN: 3/3 CTest in `work/analysis/render-build.log`.
Testing confirmed overwrite succeeds in the rebuilt baseline; black screen
after entering the level persists and is a separate issue.

## End-of-cutscene callback 0x8226B460: prefix 0x88

Observed fatal in `work/analysis/scene-2x-xma-control.log`, 14 November
15:24:29, after the XMA packet traversal control let the scene pass Luthor.
Selected loaded image contains a data pointer at 0x8200DBBC. Inspected
all instructions from 0x8226B460 through the final return at 0x8226B5D8.
The SDK already registers shared blocks at 0x8226B4E8 and 0x8226B508.
Only the non-overlapping prefix [0x8226B460,0x8226B4E8) is added; its
branches retain both original registered tails. No frame, linked calls,
or nonvolatile register writes. Following distinct method starts B5E0.

The callback updates flags through r5 according to a nullable nested
record at r4+0x1008 and selector r6 (9/11/12). `guest_scene_test` exercises
null/non-null records, both feature flags, the global at 0x82C693E8,
shared tails, big-endian writes, and unchanged r3/LR/r14. RED: LNK2019
for `__imp__sub_8226B460` in `scene-callback-red.log`.
Runtime validation beyond the cutscene remains necessary.

## Load Game callback 0x823A1A80, size 0x10

Observed fatal in normal ×1 audio-enabled run, `build/host/logs/legobatman2_010.log`
at15:43:03. Original instructions: `lwz r12,0(r3); lwz r11,0x3C(r12);
mtctr r11; bctr`. Pointer0x82C6D39C; next distinct entry823A1A90.
Only the observed16-byte method is registered. Extended `guest_start_test`
clears the previous slots, tests +0x3C and preserves arguments/result/LR/r14/CTR.
RED: missing `__imp__sub_823A1A80` in `normal-menu-red.log`.
This route loads an existing save; New Game remains
available during the rebuild. The test invokes the original translated code.

---

<a id="section-research-vtable-leaves-md"></a>

# Constructor-backed leaf boundaries

All addresses apply only to XEX SHA-256
`f13277b2645a24132f11cb45068de5692cae35fa57077c4801d622623421b68a`.

This is a curated, reviewed exception list, not a blanket data-pointer scan.
For every row, the loaded image contains code constructing the table address
and storing it into an object; the indicated slot points to the entry. Each
selected store sequence was checked separately against the mapped PE bytes.
The nearest checked table proof is retained, without extending past another
known constructed table base. Object types are not inferred solely from stores.

All selected bodies were disassembled and checked for complete contiguous
reachable code ending in returns or direct tail transfers. No linked calls,
indirect tails, stack/nonvolatile writes or unknown instructions are accepted.
The original PPC instructions remain translated; no native replacement is used.
Register-local flags remain disabled. These functions have not all been observed
on the current runtime route; static evidence and runtime coverage are distinct.

A separate reviewer checked all 60 initial bodies (251 instructions) against the
image and the selected constructor sequences. Five entries were excluded because
they were prefixes transferring to existing registered interior blocks:
0x8226B460, 0x8275A180, 0x82556980, 0x826FD310, 0x82786C98.
They require independent full-CFG/ABI investigation. The remaining 55 had no
body/boundary issue identified by that bounded review. The exploratory scanner
is not a general dataflow proof and is not used for automatic override generation.
Local audit artifacts remain in work/analysis/leaf-audit.json and the mapped PE.

| Entry | Size | Installed table | Pointer slot | Construction / installation PC |
| --- | --- | --- | --- | --- |
| 0x82242480 | 0x8 | 0x8215f108 | 0x8215f35c | 0x828972a0 / 0x828972b8 |
| 0x822645D8 | 0x10 | 0x8200ec28 | 0x8200eccc | 0x8226fc28 / 0x8226fc50 |
| 0x822646D0 | 0x10 | 0x8200ec28 | 0x8200ed90 | 0x8226fc28 / 0x8226fc50 |
| 0x82264968 | 0x4 | 0x82006420 | 0x82006530 | 0x8234d218 / 0x8234d230 |
| 0x823BAC90 | 0x8 | 0x8203d7f8 | 0x8203d818 | 0x823baa88 / 0x823baa9c |
| 0x8256CB18 | 0x38 | 0x820cecf0 | 0x820ced40 | 0x8256c92c / 0x8256c940 |
| 0x82570D48 | 0xc | 0x820cf448 | 0x820cf4a4 | 0x82570cb8 / 0x82570cc8 |
| 0x82571298 | 0xc | 0x820cf4e8 | 0x820cf544 | 0x82571208 / 0x82571218 |
| 0x82571358 | 0xc | 0x820cf588 | 0x820cf5e4 | 0x825712d8 / 0x825712e8 |
| 0x825F65D8 | 0x10 | 0x820eb080 | 0x820eb094 | 0x825ffadc / 0x825ffaf8 |
| 0x82634FB0 | 0x8 | 0x820f2268 | 0x820f2268 | 0x82634ef4 / 0x82634f1c |
| 0x826357B8 | 0x8 | 0x820f2550 | 0x820f25f4 | 0x82635460 / 0x8263546c |
| 0x826DE490 | 0xc | 0x82112e68 | 0x82112fac | 0x826de308 / 0x826de310 |
| 0x826DE8C0 | 0xc | 0x82112e68 | 0x82112e84 | 0x826de308 / 0x826de310 |
| 0x826DEC20 | 0xc | 0x82112e68 | 0x82112e6c | 0x826de308 / 0x826de310 |
| 0x826DED78 | 0x8 | 0x82112e68 | 0x82112fd0 | 0x826de308 / 0x826de310 |
| 0x826F3810 | 0xc | 0x821166d8 | 0x821166f4 | 0x826f1cd8 / 0x826f1ce0 |
| 0x82703A78 | 0xc | 0x8211ce68 | 0x8211cec4 | 0x82703828 / 0x82703830 |
| 0x82703B38 | 0xc | 0x8211ced0 | 0x8211cf2c | 0x82703838 / 0x82703840 |
| 0x82711148 | 0x30 | 0x821252b8 | 0x82125438 | 0x8271f244 / 0x8271f254 |
| 0x82716A78 | 0x4c | 0x82014598 | 0x8201462c | 0x8234e03c / 0x8234e048 |
| 0x82737AC8 | 0xc | 0x820067d0 | 0x820068a8 | 0x82242e50 / 0x82242e70 |
| 0x82740120 | 0xc | 0x821319a8 | 0x82131a04 | 0x8273ffa0 / 0x8273ffa8 |
| 0x8274F5E8 | 0x38 | 0x82152ff8 | 0x821530b4 | 0x828108b8 / 0x828108c0 |
| 0x8275F8B8 | 0x8 | 0x82148f78 | 0x82148fd8 | 0x8275f8d4 / 0x8275f8e4 |
| 0x8276A6D0 | 0x2c | 0x8201a5e8 | 0x8201a6f0 | 0x822e6580 / 0x822e6598 |
| 0x827841C8 | 0x8 | 0x8200f060 | 0x8200f12c | 0x822b9884 / 0x822b9890 |
| 0x827AB9F0 | 0xc | 0x82163ff0 | 0x8216405c | 0x828ea0e0 / 0x828ea0f0 |
| 0x82814CD8 | 0x8 | 0x82153cf8 | 0x82153cf8 | 0x82814b30 / 0x82814b54 |
| 0x82AA6588 | 0x8 | 0x82005dd0 | 0x82005dec | 0x82240cfc / 0x82240d0c |
| 0x82AFB5D0 | 0x8 | 0x8218b714 | 0x8218b714 | 0x82afb3b8 / 0x82afb3dc |
| 0x82B01C88 | 0x8 | 0x8218ba40 | 0x8218ba8c | 0x82b01af0 / 0x82b01b18 |
| 0x82B01CE8 | 0x8 | 0x8218ba40 | 0x8218ba40 | 0x82b01af0 / 0x82b01b18 |
| 0x82B07160 | 0x8 | 0x8218ba40 | 0x8218ba50 | 0x82b01af0 / 0x82b01b18 |
| 0x82B07168 | 0x8 | 0x8218ba40 | 0x8218ba44 | 0x82b01af0 / 0x82b01b18 |
| 0x82B07170 | 0x8 | 0x8218ba40 | 0x8218ba68 | 0x82b01af0 / 0x82b01b18 |
| 0x82B07178 | 0x8 | 0x8218bdb0 | 0x8218bdfc | 0x82b070d0 / 0x82b070ec |
| 0x82B07188 | 0x8 | 0x8218bdb0 | 0x8218bde0 | 0x82b070d0 / 0x82b070ec |
| 0x82B07190 | 0x8 | 0x8218bdb0 | 0x8218bdb0 | 0x82b070d0 / 0x82b070ec |
| 0x82B07198 | 0x8 | 0x8218ba40 | 0x8218ba58 | 0x82b01af0 / 0x82b01b18 |
| 0x82B071A0 | 0x8 | 0x8218ba40 | 0x8218ba54 | 0x82b01af0 / 0x82b01b18 |
| 0x82B08550 | 0x8 | 0x8218be78 | 0x8218bec4 | 0x82b084c4 / 0x82b084e0 |
| 0x82B08560 | 0x8 | 0x8218ba40 | 0x8218ba74 | 0x82b01af0 / 0x82b01b18 |
| 0x82B08568 | 0x8 | 0x8218bdb0 | 0x8218bde8 | 0x82b070d0 / 0x82b070ec |
| 0x82B08570 | 0x8 | 0x8218ba40 | 0x8218ba48 | 0x82b01af0 / 0x82b01b18 |
| 0x82B08578 | 0x8 | 0x8218ba40 | 0x8218ba5c | 0x82b01af0 / 0x82b01b18 |
| 0x82B08580 | 0x8 | 0x8218ba40 | 0x8218ba60 | 0x82b01af0 / 0x82b01b18 |
| 0x82B08588 | 0x8 | 0x8218ba40 | 0x8218ba80 | 0x82b01af0 / 0x82b01b18 |
| 0x82B08590 | 0x8 | 0x8218ba40 | 0x8218ba84 | 0x82b01af0 / 0x82b01b18 |
| 0x82B1FF18 | 0x8 | 0x8218ccb4 | 0x8218ccb4 | 0x82b1fe24 / 0x82b1fe4c |
| 0x82B31C60 | 0x8 | 0x82191ed0 | 0x82191ef4 | 0x82b31cb0 / 0x82b31cc8 |
| 0x82B321E0 | 0x8 | 0x82191ed0 | 0x82191ed8 | 0x82b31cb0 / 0x82b31cc8 |
| 0x82B321F0 | 0x8 | 0x82191f2c | 0x82191f4c | 0x82b32288 / 0x82b3229c |
| 0x82B339D8 | 0x8 | 0x82191ed0 | 0x82191eec | 0x82b31cb0 / 0x82b31cc8 |
| 0x82B4D390 | 0x8 | 0x821ac950 | 0x821ac990 | 0x82b4dcd4 / 0x82b4dcec |

---

<a id="section-research-vtable-forwards-md"></a>

# Reviewed complete forwarding methods

Selected XEX: f13277b2645a24132f11cb45068de5692cae35fa57077c4801d622623421b68a.
Loaded image: 38d3eaaefea98e97cb57b2498358e12cc59c9c8c80e6d7ec5c6d6ed191214ead.

The separate bounded review manually followed every reachable path, decoded the
complete bodies, checked constructor/store provenance and table extents, and
rejected overlap with existing entries. 20 additional methods end in fixed-slot
virtual forwarding: the branch target comes from an object/vtable load, not an
indexed jump table. The remaining method 0x82756170 is a scalar predicate with
cntlzw. None has a stack frame, linked call or nonvolatile register write.
Dynamic target values remain runtime-dependent. Original PPC instructions are
translated with all register-local optimizations disabled.

Two entries already observed and added separately (0x82AAA168/0x82AAA180) are
not duplicated here. 0x827580D8 is held for explicit prefix/registered-block
treatment; 0x82AFF230 is held for review of its conditional divide guard trap.
Local complete review report: work/analysis/review-held-leaves.json. These are
statically reviewed additions; they have not all run on the current boot route.

| Entry | Complete size | Installed table | Pointer slot | Construction / store PC |
| --- | --- | --- | --- | --- |
| 0x82570D30 | 0x18 | 0x820CF448 | 0x820CF4A0 | 0x82570CB8 / 0x82570CC8 |
| 0x82571280 | 0x18 | 0x820CF4E8 | 0x820CF540 | 0x82571208 / 0x82571218 |
| 0x82571340 | 0x18 | 0x820CF588 | 0x820CF5E0 | 0x825712D8 / 0x825712E8 |
| 0x82675F50 | 0x1C | 0x820FE778 | 0x820FE77C | 0x82685404 / 0x82685414 |
| 0x826DE478 | 0x18 | 0x82112E68 | 0x82112F80 | 0x826DE308 / 0x826DE310 |
| 0x826E2650 | 0x14 | 0x821131A8 | 0x82113204 | 0x827CE238 / 0x827CE248 |
| 0x826F37F8 | 0x18 | 0x821166D8 | 0x821166EC | 0x826F1CD8 / 0x826F1CE0 |
| 0x82703A60 | 0x18 | 0x8211CE68 | 0x8211CEC0 | 0x82703828 / 0x82703830 |
| 0x82703B20 | 0x18 | 0x8211CED0 | 0x8211CF28 | 0x82703838 / 0x82703840 |
| 0x82740108 | 0x18 | 0x821319A8 | 0x82131A00 | 0x8273FFA0 / 0x8273FFA8 |
| 0x8274F5B0 | 0x14 | 0x82152FF8 | 0x82153080 | 0x828108B8 / 0x828108C0 |
| 0x82756170 | 0x60 | 0x8201B240 | 0x8201B438 | 0x8234D28C / 0x8234D29C |
| 0x82765A58 | 0x10 | 0x8200D6D0 | 0x8200D738 | 0x82264FDC / 0x82264FEC |
| 0x82765A78 | 0x10 | 0x8200D6D0 | 0x8200D744 | 0x82264FDC / 0x82264FEC |
| 0x82765A88 | 0x10 | 0x8200D6D0 | 0x8200D748 | 0x82264FDC / 0x82264FEC |
| 0x82765A98 | 0x10 | 0x8200D6D0 | 0x8200D74C | 0x82264FDC / 0x82264FEC |
| 0x82765AA8 | 0x10 | 0x8200D6D0 | 0x8200D73C | 0x82264FDC / 0x82264FEC |
| 0x82AA9F58 | 0x28 | 0x82185BC0 | 0x82185BD8 | 0x82AB87E8 / 0x82AB87F4 |
| 0x82AA9F80 | 0x1C | 0x82185BC0 | 0x82185BDC | 0x82AB87E8 / 0x82AB87F4 |
| 0x82AAA1B8 | 0x28 | 0x82185C40 | 0x82185C58 | 0x82AAA098 / 0x82AAA0A8 |
| 0x82AAA1E0 | 0x1C | 0x82185C40 | 0x82185C5C | 0x82AAA098 / 0x82AAA0A8 |

---

<a id="section-validation-switch-guest-probe-md"></a>

# ARM64 guest compatibility experiment — 2026-11-14

This is a local test of three original translated LB2 functions, not a game NRO.
It does not boot the title, emulate Xbox services, render the game or play sound.
The graphics/input test remains a separate `switch-toolchain-probe.md` check.

Build: `./tools/build-switch-guest-probe.ps1`.
Output: `build/switch-guest-probe/lb2-guest-probe.nro` (227525 bytes).
SHA-256: `3a79551295f08c39dd4f0377a329971e818c3a14ad95ae2771eae14afc85d23b`.

On Switch, launch from hbmenu. Expected: four PASS lines and RESULT: PASS;
press + to exit. Physical execution is pending; testing has not yet run
either hardware probe. Copy the NRO into its own folder under `sdmc:/switch/`.
It requires no game resources or saves. It contains a small amount of original
translated game code, stays local/ignored, and is not a public distribution.

## Evidence

- devkitA64 GCC16.1.0; ELF64 AArch64 ET_DYN; linker/startup from libnx.
- Native flags: Cortex-A57, ARMv8 CRC/crypto, soft thread pointer, local-exec
  TLS, PIE, O2, no FP contraction, no strict aliasing, no fast-math.
- Staging extracts entries 82556980, 82556994 and 825CB588 unchanged from the
  current generated code. Per-entry hashes: local `extraction.json`.
- Tests cover a null branch through an original shared tail, big-endian
  guest memory reads, a guest constant, and unchanged LR/nonvolatile r14.
- `guest_compiler_compat.h` preserves the weak override alias and original
  entry under GCC. Before: GCC ignored alias attributes (302 warnings in
  full unit44) and `nm` showed originals only. After: extracted functions
  compile with `-Werror=attributes`; final ELF has both W guest and T original
  symbols at matching addresses. Full-game GCC compilation is not yet verified.
- Repeated script build produced the same NRO hash. No Windows libraries
  or DLLs are linked into this test; SDK third-party headers are reused.

## Next integration issues

Existing Horizon candidate remains nfsmw-nx20f8dc12. Its CMake currently
requires a static NVK/Mesa SDK even when selecting an additional deko3d
backend. That SDK and most source dependencies are not installed under the
local Switch toolchain (currently devkitA64/libnx/tools only).

LB1's reference renderer does not support memexport, while LB2's geometry
requires current memexport results (confirmed by the PC debugging). That is
a correctness requirement for the Switch renderer, not an optimization to
disable. No LB1 game addresses or renderer code have been copied here.

The Horizon audio helper has the same interleaved-packet increment bug,
but a different end sentinel and split-frame handling than the PC SDK.
`port/rexglue-horizon-xma-packet-traversal.patch` adapts only the increment,
preserving those differences. Applying it and testing full streams against
the Horizon runtime remain pending. Windows method substitution/input
wrappers are not Switch platform implementations.

PC reference binaries were copied and hash-checked in
`build/pc-reference-2026-11-14`; manifest in local
`work/analysis/switch-compat/pc-reference-sha256.txt`.
No performance improvement or console frame rate has yet been measured.

---

<a id="section-validation-switch-toolchain-probe-md"></a>

# Проверка инструментов на Switch

Это отдельный тест libnx/deko3d и контроллера, **не игра и не Pre-Run 1**.
Он не читает игровые ресурсы, сохранения или другие папки на SD.

Сборка: `./tools/build-switch-probe.ps1`. Результат:
`build/switch-probe/lb2-toolchain-probe.nro`.

Проверено на ПК: компиляция и линковка devkitA64 GCC16.1.0, ELF AArch64 ET_DYN,
упаковка NRO0, компиляция собственного минимального vertex shader через uam
в DKSH. Шейдер пока не используется в тесте clear/present.
Фактический GPU и ввод ещё требуют запуска на устройстве.

Для проверки скопировать NRO в `sdmc:/switch/legobatman2-nx-probe/`.
После безопасного отключения USB-диска выйти из Hekate USB Tools, загрузить
Atmosphere и запустить **LB2 toolchain probe** из hbmenu.

Ожидается синий экран. Нажатие **A** переключает синий/оранжевый цвет.
**+** завершает тест. Приложение пишет только собственный журнал
`sdmc:/switch/legobatman2-nx-probe/probe.log`.

Успешный журнал: device=ready, swapchain=ready, present=first, present=120,
A=orange/A=blue после нажатий, plus=exit, graphics=shutdown, exit=0.
Если экран чёрный или приложение завершилось, нужен этот журнал;
сам факт сборки не подтверждает работу GPU на консоли.

Исходный NRO 2026-11-13: 268 485 байт, SHA-256
`1eae5d8a65cc6775cf716ffc87f76b57138be25492b85c2da2687a87baaf00d7`.

---

# История разработки и диагностики



---

<a id="section-development-history-md"></a>

# История разработки и решения проблем

Сводка составлена по Git и исследовательским записям проекта на 16 ноября 2026 года. Это история зафиксированных случаев, а не гарантия, что сохранился каждый разговор или эксперимент. Старые исследовательские документы сохраняют состояние на дату написания; локальные журналы в `work/` не входят в репозиторий.

Игра на ПК проходится, кроме периодических подлагиваний. Главная проблема Switch — около 3 FPS; в главном меню также наблюдается похожая на ошибку глубины ошибка отрисовки, её диагностика пока не приоритет. Подробный разбор механизмов, измерений и воспроизведения — в [техническом руководстве](#section-porting-casebook-md).

## Подготовка и рекомпиляция

1. Изучили ReXGlue и существующие порты, закрепили ревизии и выбрали статическую рекомпиляцию Xbox 360. Создали проверку окружения и потоковое извлечение собственного ISO с проверкой границ, путей и хешей. Игровые данные исключены из Git. Подробности: [первый план](#section-first-working-build-md), [перенос](#section-porting-md).
2. Генератор столкнулся с нераспознанными границами функций и косвенными вызовами. Для выбранного XEX вручную исследовали инструкции, таблицы и ABI, добавили подтверждённые границы и проверки, не принимая все указатели за функции. Статически проверенный код не обязательно выполнялся в игре. Доказательства: [границы функций](#section-research-function-evidence-md), [листья vtable](#section-research-vtable-leaves-md), [переадресации](#section-research-vtable-forwards-md).
3. На Windows отсутствовал готовый совместимый набор инструментов. Собрали переносимое окружение из закреплённых архивов, отдельно проверили компилятор, SDK и runtime. Ограничения и лицензии: [зависимости](#section-research-dependencies-md). Для компиляции Mesa исправили несовместимость хранения `setjmp` в MinGW Clang; область патча ограничена соответствующей конфигурацией: [сборка Switch](#section-research-switch-game-build-md).

## Рабочий запуск на Windows

- Вылет при нажатии Start был связан с отсутствующей границей виртуального перехода. Границу проверили по инструкциям, добавили в overrides и проверили на синтетическом объекте. [Диагностика](#section-research-pc-freeze-md).
- Чёрный экран в режиме RTV возникал из-за неверной оценки высоты depth surface и ошибочного владения EDRAM. Включили точную оценку геометрии; проверка подтвердила восстановление изображения. [Разбор](#section-research-rtv-black-screen-md).
- Растянутая геометрия исправилась отключением быстрого readback memexport. Застревание катсцены потребовало исправления обхода аудиопакетов XMA; затем отдельно восстановили пропущенную границу функции. Последующие запуски дошли до игрового процесса. [Диагностика сцены](#section-research-scene-freeze-md).

## Switch: от сборки к игре

- Подготовили ARM64 toolchain и Horizon SDK, интегрировали адаптацию runtime, ввод, звук и Vulkan/NVK. Сборка NRO сама по себе не считалась подтверждением запуска на устройстве. [Сборка](#section-research-switch-game-build-md), [проверка инструментов](#section-validation-switch-toolchain-probe-md).
- Первый запуск падал из-за удвоения `sdmc:` при преобразовании путей. Использовали абсолютные POSIX-пути для данных, кэша и сохранений; добавили проверку путей. [Разбор](#section-research-switch-startup-crash-md).
- Мелкие выделения гостевой памяти исчерпывали ресурсы отображений; добавили пакетное подтверждение памяти. Игра затем дошла до меню и игрового процесса, однако отдельные проблемы загрузки и стабильности остаются непроверенными на всех сценариях. [Сборка и патчи](#section-research-switch-game-build-md).
- Настройка 480p сначала не действовала: игра перезаписывала видеорежим и размеры поверхностей. Ограниченные Switch-перехваты сохранили выбранный режим; журнал подтвердил 848×480. Скорость осталась около 3 FPS. Сообщённые артефакты требуют отдельного исследования. [Разбор](#section-research-switch-480p-md).

## Производительность: результаты и неудачные эксперименты

На обычном игровом участке в 480p четыре окна по 10 секунд содержали 31–34 кадра, а ожидание внутри `vkQueueSubmit` составляло примерно 8,1–8,4 секунды на окно. Подсчёты выявили сотни переносов владения графическими ресурсами на кадр. Это указывает на давление в программном тракте рендеринга и очереди Mesa NVK, но не выделяет однозначно стоимость драйвера, синхронизации и работы GPU. Разрешение само по себе проблему не решило. [Замеры](#section-research-switch-480p-md), [анализ очереди](#section-research-switch-intro-queue-md).

| Эксперимент | Результат |
| --- | --- |
| Dynamic UBO для констант | Плавность ухудшилась, время переместилось в ожидание readback; изменение откатили. [Данные](#section-research-switch-dynamic-constants-md) |
| Специализация формата memexport | Размер наблюдаемого native shader сократился с 9005 до 1147 инструкций, но заметного прироста FPS не было. [Данные](#section-research-switch-memexport-format-md) |
| GPU timestamps | Диагностическая сборка потеряла GPU; откатили. Результаты нельзя считать безопасным исправлением. [Данные](#section-research-switch-gpu-timestamps-md) |
| FSI вместо FBO | Наблюдались длинные зависания и хрип звука; эксперимент откатили, контроль вернулся к исходным ~3 FPS. [Данные](#section-research-switch-intro-fsi-md) |
| Специализированное копирование stencil | Замена сработала, но заметного прироста FPS не дала; экспериментальную сборку откатили. [Данные](#section-research-switch-stencil-copy-md) |
| Исключение проверенных point draws | Удаление десятков тысяч вызовов за 10 секунд не устранило ~3 FPS. Для поиска стоимости отдельных операций подготовлена дополнительная диагностика; её результат не следует считать исправлением. [Подсчёт](#section-research-switch-guest-draws-md), [выборочные измерения](#section-research-switch-fence-operation-samples-md) |

Аналогичный начальный результат другого проекта не доказывает общую причину и не позволяет прогнозировать производительность этой игры. Возможные направления — исследование повторных draw, переносов EDRAM и синхронизации с сохранением корректности memexport и порядка операций. [Сравнение](#section-research-external-switch-performance-md).

## Что остаётся

Установить источник стоимости отправленной работы и безопасно уменьшить её, повторить замеры на одинаковых сценах и частотах. Ошибка отрисовки главного меню отложена до работы над производительностью. Затем нужны проверки переходов, игрового процесса, звука, сохранений и производительности на реальной консоли. Цель 30 FPS остаётся целью исследования, а не обещанием.

---

<a id="section-research-pc-freeze-md"></a>

# Диагностика ПК, 13 ноября 2026


В исходном запуске первые кадры относятся к автоматической демонстрации
после экрана Press START. Вход в игровой уровень пока не подтверждён.
Отдельное действие Start на геймпаде вызывает вылет.

## Вылет по Start

В `work/analysis/host-freeze-baseline.log`, 21:04:44, dispatcher завершает
процесс на отсутствующем адресе 0x823A1A60. Тело проверено по выбранному PE:
четыре инструкции виртуального tail-перехода через slot +0x58.
Добавлена граница 0x10 в overrides. CTest `guest_start` до исправления
не находил символ; после регенерации проверяет исходный перевод на
синтетическом объекте и таблице, передачу аргументов и результата, LR/r14.
Это исправляет обнаруженный пропуск; прохождение всего меню требует
повторного запуска с контроллером.

## Зависание автоматической демонстрации

Сняты два стека одного зависшего процесса (PID 27104):
`work/analysis/frozen-stacks.txt`, `frozen-stacks-2.txt`.
Главный поток продолжает занимать CPU; оба раза он внутри 0x824AEED0,
вызванной из 0x824B03F8. Это векторная обработка блоков изображения.
Другие кадры стека сохранены в `frozen-symbols.txt`. Графический поток
ждёт события; признаков компиляции pipeline в этот момент не обнаружено.
Стек сам по себе не доказывает бесконечный цикл или причину задержки.

В 0x824AEED0 нет длинного собственного цикла, кроме восьми prefetch-итераций.
Исследуется число повторных вызовов из вложенных циклов 0x824B03F8.
Пробная обёртка в `work/analysis/movie_trace.cpp` пишет счётчики блоков,
координаты и размеры, проверяет сохранение r28–r31 после исходного вызова.
Она включена только в отдельный `build/host/diagnostic.exe`, не в игровой
launcher. В основной сборке алгоритм видеодекодирования пока не менялся.

Последующий `movie-sync.log` сузил причину: пакеты FMV/FMA читаются успешно,
номер кадра растёт, но число кадров догоняющего декодирования в r27 растёт
1 → 7 → 17 → 39 → 89. В 0x824AA690 такой пакет кадров передаётся в
0x824B0998 с flag=1, подавляющим callback вывода в 0x824B0830.
Игра не успевает догнать аудиочасы и продолжает подавлять промежуточный вывод.
Замер блоков около 250–280 тысяч/с. Декодирование не зациклено на одном блоке,
размеры изображения корректны (80×45 макроблоков), r28–r31 сохраняются.

В CMake найден безусловный `/Od`, перекрывавший стандартный Release `/O2`
для всего гостевого кода. Он убран: оптимизация теперь определяется
конфигурацией CMake. Сохранены `/EHa`, SSE4.1, запрет FP contraction и
strict aliasing; настройки локализации гостевых регистров не менялись.
Повторный замер `work/analysis/movie-optimized.log` подтверждает исправление:
600 последовательных вызовов вывода для первых 600 кадров, skip=1/flag=0,
меняющиеся хеши пикселей. Кадры 30–600 разделяет примерно 19 секунд,
то есть 30 кадров/с на этом фрагменте. Проверка также подтвердила,
что катсцена воспроизводится нормально. Это проверка первых 20 секунд,
а не полного прохождения ролика или уровня.

Завершения `movie-sync.log` на кадре 120 и `movie-optimized.log` на кадре600
преднамеренные: только отдельная диагностическая обёртка вызывает exit
после записи. Основной EXE не содержит этой обёртки и ограничения.

Вылет при сохранении: следующий observed target 0x823A1AA0 в
`build/host/logs/legobatman2_006.log`. Добавлен исходный 16-байтовый переход
через slot +0x40; тест проверяет его отдельно от Start slot +0x58.

## Проверки и запуск

Основной EXE: `build/host/legobatman2.exe`. Звук по умолчанию выключен
в OnPreSetup в этом запуске; аудиосистема продолжает работать,
чтобы отключение устройства не меняло тайминги гостя. Явный аргумент
`--audio_mute=false` разрешает разовую проверку звука: звук включался для проверки. Обычный запуск остаётся тихим.
Сборка после override: 3/3 CTest, 26/26 Python tests.
После включения Release-оптимизации: полная пересборка всех 159 recomp units,
3/3 CTest; проверены команды компиляции (159 с /O2, ни одной с /Od,
FP contraction выключен). Тесты Python: 26/26. Переходы Start и Save slot
проверены синтетически; повторная проверка сохранения в игре ещё нужна.
Запускать только один экземпляр: общие файлы shader cache блокируются
при параллельных процессах. Прежние дубли закрыты.

## Чёрный экран уровня и перезапись сохранения, 13–14 ноября

Проверка подтвердила: звук и FMV работают, сохранение создаётся, но
после старта уровня первый кадр сменяется почти постоянным чёрным экраном.
При перезаписи обнаружен отдельный fatal target 0x823A1AD0 (21:43:41,
`work/analysis/render-baseline.log`). Добавлена проверенная граница 0x10
для исходного virtual tail +0x54; evidence в function-evidence.md.
До override тест не линковался, после — CTest 3/3. Повторная проверка
перезапись успешно; чёрный экран в baseline сохранился.

Контрольный запуск `render-rov.log` изменил только графическую настройку
`render_target_path_d3d12=rov` относительно baseline. Проверка дошла
до того же места и подтвердил: изображение видно. Таким образом выявлена
зависимость от host RTV path на RTX 5060 Ti; конкретный ошибочный draw/resolve
внутри RTV пока не локализован. ROV используется как проверенный обход,
без отключения эффектов игры. Артефакты пролёта отложены в этом запуске.

В src/main.cpp ROV выбран по умолчанию до настройки графического устройства.
Плагин загружается до установки флага: его CVars находятся в отдельной DLL,
их storage-функции не экспортируются для прямого REXCVAR_SET из EXE.
Используется registry API; явный command-line override сохраняется.
Проверки `render-default-probe.log` и `render-override-probe.log` подтверждают
выбор rov без параметра и rtv с явным override. Эти короткие startup probes
преднамеренно указывают отсутствующие данные и завершаются до загрузки XEX.
После финальной сборки CTest 3/3, Python 26/26. Полное прохождение уровня
и производительность ROV не проверены; итоговый EXE собран с тем же режимом,
который проверялся отдельным аргументом.

---

<a id="section-research-rtv-black-screen-md"></a>

# RTV: диагностика чёрного экрана, 14 ноября 2026

## Причина установлена и обход ROV заменён исправлением настройки RTV

В конце кадра игра очищает depth surface: base1328, pitch16 tiles,
4xMSAA, фактическая высота360. При execute_unclipped_draw_vs_on_cpu=false
оценка height=1024 и length=2048 захватывает всю EDRAM с wraparound,
включая живой color buffer base0. Последний resolve из-за ошибочного
ownership читает depth buffer вместо готовой картинки.

Доказательства: work/analysis/rtv-frame-501.txt: последний OWNER depth=1,
base1328,msaa2,height1024,length2048; DUMPOWNER последнего resolve depth=1.
Предыдущие полноэкранные resolve в `rtv-frame-301.txt` содержат изображение
и в EDRAM, и в shared memory; перед последним resolve первые720 EDRAM tiles
уже нулевые. Поэтому ранняя гипотеза про поломку typed UAV не подтвердилась.

Изменён только execute_unclipped_draw_vs_on_cpu=true в работающем процессе
(rtv-ownership.log,14:31:10). В `rtv-frame-502.txt` height=360,length=720,
последний DUMPOWNER depth=0,base0. Все четыре rtv-swap-502-*.bin содержат
изображение (около3.686MB ненулевых байтов). Проверка подтвердила:
«Картинка есть, заметно быстрее» относительно ROV.

В основном src/main.cpp теперь RTV + точная оценка по умолчанию, без
изменений GPU DLL. Явные CLI overrides сохранены. Сборка, CTest3/3,
Python26/26 прошли. Короткие startup probes подтверждают default rtv/true
и явный override rov/false. Основной EXE запущен с исправлением:
work/analysis/rtv-fixed-production.log.

Остановка катсцены на Люторе сохраняется по сообщению тестирования:
это отдельный открытый дефект. Полное прохождение не заявляется.

Ниже сохранён ход расследования; ранние гипотезы не являются текущим выводом.

## Что установлено

ROV сохраняет изображение, но была отмечена неприемлемую скорость.
Это временный обход, не окончательное исправление.

Наблюдение: в ROV катсцена останавливается на крупном плане
Лекса Лютора (белый костюм, женщина позади справа). Фоновые персонажи
продолжают двигаться и отдельные анимации проигрываются, но основные
персонажи и дальнейший ход сцены замирают. Состояние звука неизвестно:
звук не удалось проверить. Из этого нельзя заключить полную
остановку GPU или связать задержку с причиной чёрного экрана RTV.
Отдельно исследовать продвижение сценария/таймлайна катсцены и ожидаемые
события, включая аудиосинхронизацию как гипотезу. Не считать ROV проверенным
до конца катсцены или до управляемого игрового процесса.

Проверялась отдельная `build/rtv-diagnostic/legobatman2.exe` с собственным
`rexgpu-xenos.dll`. Основные DLL в build/host не заменялись. Плагин собран
из исходников **того же** commit c94f5ebdcb3c9d1a460ca48e04f9758448f8d518,
что и установленный SDK 0.10.0, с установленными runtime/import libraries.
Исходники: work/analysis/pinned-sdk. SHA256 ZIP:
9ce108d2fc111eed58ad90530af034c9547367efa1fd2165617a3eaa2ce06371.

- Несколько запусков дошли до чёрного экрана в RTV.
- Два swap-source readback в `rtv-baseline-capture/rtv-swap-1-*.bin`:
  1280×720 RGBA, 3 686 400 байт, **каждый байт равен нулю**.
  Это текстура ДО gamma/FXAA и presenter, а не скриншот окна.
- Трасса `work/analysis/rtv-frame-1.txt` (первая версия также сохранена в
  rtv-baseline-capture) содержит команды геометрии, MRT, освещения,
  уменьшения/фильтрации и финального resolve. GPU-команды не прекращаются.
- Контроль `direct_host_resolve=false` применён в работающем процессе:
  `rtv-source-diagnostic.log`, 08:49:34. Оба `rtv-swap-2-*.bin` также
  полностью нулевые. Отдельная direct/preflight-оптимизация не объясняет сбой.
- Контроль MRT clamp был отправлен, но завершённого захвата результата
  нет. Не считать гипотезу проверенной.
- В `rtv-frame-102.txt` сняты сами host render targets перед dump.
  `rtv-source-102-38.bin`: base=0, 1280×2048, 1 sample, stride=5120.
  В активной области 1280×720 находится полноценный кадр с репортёром
  Gotham City News, включая изображение сцены и титры.
  Просмотр: `work/analysis/rtv-source-102-38.png`.
  19/20 также содержат ненулевую картинку. Оба swap readback захвата102 — нули.

Вывод: RTV способен нарисовать и собрать изображение; исследовать нужно
цепочку host RT → EDRAM dump → resolve в shared memory → загрузка swap texture.
Это существенно уже, чем «эффекты камеры не работают». Пока НЕ доказано,
какое из трёх звеньев теряет данные. Readback host RT принудительно завершает
submission и синхронизирует очередь, поэтому учитывать влияние диагностики;
нужен дополнительный захват выходных буферов с минимальным вмешательством.

## Следующий точный эксперимент

В D3D12RenderTargetCache::Resolve снять EDRAM непосредственно после
DumpRenderTargets, затем written_address/written_length из shared memory
после resolve. Сопоставить с host RT и swap-source следующего кадра.
Так можно разделить ошибку dump shader, resolve shader и texture upload.
Проверка d3d12_bindless=false — отдельная гипотеза о привязке descriptors,
пока НЕ выполнялась. Не менять несколько параметров за один контроль.

Для сравнения сохранён официальный upstream Xenia render target cache:
work/analysis/xenia-render-target-cache.cc и compare-*.diff.
SDK изменил EDRAM dump с raw UAV на typed UAV; адрес в shader теперь
в элементах, а не байтах. Это участок для проверки, не доказанная причина.
Upstream: https://github.com/xenia-project/xenia/blob/master/src/xenia/gpu/d3d12/d3d12_render_target_cache.cc

## Как продолжить без повторной подготовки

- Диагностический CMake: work/analysis/gpu-diagnostic/CMakeLists.txt;
  сборка: build/gpu-diagnostic; изменения SDK: work/analysis/rtv-diagnostic.patch.
- Готовый отдельный launcher: build/rtv-diagnostic/legobatman2.exe.
  Аргументы RTV: `--render_target_path_d3d12=rtv --mnk_mode=true`
  и `--user_data_root=work/userdata`.
- Захват: создать work/analysis/rtv-trace.trigger. Плагин пишет два swap
  readback и следующие проходы; второй кадр включает host RT readback.
  Нумерация нового процесса начинается с101: перед запуском сохранить
  прошлые файлы отдельно, чтобы не затереть evidence.
- Файл rtv-control.txt принимает только direct_host_resolve,
  gamma_render_target_as_unorm16, mrt_edram_used_range_clamp_to_min с
  одним значением. Это диагностическая функция, не часть основной игры.
- Игра закрыта при проверке (rtv-buffer-diagnostic.log:08:56:36).
  Запущенных legobatman2 при последней проверке нет.
- Не продолжать попытки UI ввода через sky без нового наблюдения:
  ввод в этой сессии неоднократно блокировался как конкурентный ввод тестирования.

Основная игра всё ещё использует ROV по умолчанию из предыдущего этапа.
Исправление RTV не объявлено. Никакие случайные графические настройки
не внесены в основной launcher во время этой диагностики.

---

<a id="section-research-scene-freeze-md"></a>

# Катсцена и геометрия: диагностика, 14 ноября 2026


## Обновление: подтверждённые результаты

- `readback_memexport_fast=false` исправляет растянутую геометрию.
  Проверка подтвердила на том же эпизоде; теперь default в main.cpp.
- Контроль XMA packet traversal позволил пройти крупный план Лютора.
  Проверка подтвердила: «Сцена прошла дальше, ускорение есть» на ×2.
- Ближе к концу произошёл отдельный fatal0x8226B460, а не прежняя остановка.
  Добавлен проверенный prefix0x88 без перекрытия shared blocks B4E8/B508.
  `guest_scene_test` проверяет 11 вариантов, CTest4/4 и Python26/26 прошли.
- Следующий запуск в этом запуске — ×3 для часов и звука;
  обычный EXE остаётся ×1. Полный проход сцены пока НЕ подтверждён.

### Почему проверка перешла от GPU к аудио

Snapshots WAIT_REG_MEM сами по себе не доказывали deadlock. Лог ожидания
дольше2 секунд не сработал, а гостевые счётчики 0x82D9D49C продолжали
расти примерно30/сек. Рендер и кадры продолжались при остановке сюжета.

В остановившемся PID13556 контекст XMA3 stereo сохранял непрочитанные
данные: input valid1, read_bits525622, output valid0, read/write0/0,
capacity24. Контекст4 mono уже исчерпал input, read_bits32, current1,
output read/write4/0, capacity12. Музыка0–2 продолжала работу.
Таблица контекстов физическая0x1FCA7000. Данные: `xma-before.*`,
`xma-stuck1.*`/`stuck2.*`/`stuck3.*`, `xma-stuck1-3-input0.bin`.

Весь снятый3-канальный блок отдельно декодируется без ошибок FFmpeg8.
Тот же статический FFmpeg SDK также декодирует независимо извлечённые
кадры stereo/mono без ошибок, включая исходное битовое выравнивание.
Но полный повтор через оригинальный `XmaContext::Work` даёт ошибки
`Reserved bit` в mono и значительно сокращает его длину.

### Доказанный неверный переход XMA

Точный SDK commit `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`,
локально `work/analysis/pinned-sdk`; FFmpeg submodule
`0604b464c7cb4ebc94940cf1f324a3b26b87717c`, `ffmpeg-pinned`.

В `GetNextPacketReadOffset` mono packet30 указывает на31. Packet31
`03fff801` не содержит новых кадров (first32799), но skip1 указывает
на следующий packet этого же потока33. SDK делает простой increment,
попадает в packet32 `fc000106` другого, stereo потока и декодирует его
как mono. Контроль меняет этот increment на
`next_packet_index += uint32_t(xma::GetPacketSkipCount(next_packet)) + 1`.

Проверки (`work/analysis/scene-diagnostic`):

- `xma-packet-probe`: синтетическая interleaved таблица, оригинал exit1,
  контроль exit0. Также проверены immediate target и конец буфера.
- `xma-context-probe`: полный реальный блок через runtime; после
  подстановки helper ошибки mono исчезают и длина восстанавливается;
  stereo сохраняется. `xma-context-hook-mono.log`/`stereo.log`.
- Независимо пересобранный XmaContext с тем же единственным изменением
  подтверждает результат (`xma-context-fixed-probe`).

Равные указатели + output_valid0 при полном кольце — штатный сигнал
«выход заполнен». Ранняя гипотеза «ошибочно пустой» была отклонена,
резервирование дополнительного блока не применялось.

### Диагностический запуск и оставшаяся интеграция

`build/scene-diagnostic/scene-diagnostic.exe` использует отдельные часы
и SDL audio driver, `--lb2_diagnostic_speed=1` для обычного контроля.
×2 подтверждены журналом и при проверке. Теперь default/range повышен
до3, следующий лог `scene-3x-callback-control.log`, PID29300.

Первоначально аудиоисправление было только в диагностике. После успешного
прохода до управления оно перенесено в обычный EXE: `src/host/xma_packet_fix.cpp`
до гостевых потоков подставляет helper в память своего процесса по экспорту
pinned runtime, проверяя16 байт оригинального пролога. DLL на диске не
меняется; неподдерживаемый runtime отклоняется. Обычный EXE также содержит
исправления RTV/геометрии и guest prefix0x8226B460. Для будущего обновления
SDK предпочтителен исходный patch и воспроизводимая пересборка runtime.
Исходный patch сохранён в `port/rexglue-xma-packet-traversal.patch`;
он ещё не включён в штатную сборку SDK.

Первый запуск ×3 (PID29300) прошёл инициализацию, но вылетел15:31:20:
Windows Application event1000, access violation0xC0000005 в
rexruntime.dll+0x117F11, внутри `vector<DeviceInfo>::clear` (+0x61
от экспорта clear по0x117EB0). `InputSystem::RefreshDevices` очищает
общие devices_/device_owners_ при каждом getter без блокировки.
Конкурентный доступ — рабочая гипотеза, пока не доказанная crash dump.
В диагностическом EXE шесть XamInput wrappers теперь сериализуют вызовы
оригинальных SDK exports одним recursive_mutex, не меняя аргументы
и результаты. Отсутствие прямых input imports проверено dumpbin;
все оригинальные exports присутствуют. Добавлен unhandled minidump
`work/analysis/scene-crash-<PID>.dmp` для возможного повторения.
Повтор PID26524 (`scene-3x-guarded-control.log`) прошёл катсцену до управления;
проверка подтвердила игру и вибрацию. Завершён штатно через закрытие окна.
Те же wrappers перенесены в обычный EXE как `src/host/input_guard.cpp`.

### Обычный игровой тест на ПК

основная Release сборка теперь запускается на ×1,
с `audio_mute=false` по умолчанию и штатным SDL audio backend. Явный
command-line mute сохраняется. Диагностический audio driver, захват кадров,
crash dump hook и ускорение в основной EXE не включены. Обычные журналы
ошибок сохранены. Launcher: `launch-game.bat`.

Проверено: CTest5/5, Python26/26, отсутствие прямых XamInput imports,
неизменённые файлы DLL runtime/GPU по SHA-256. Новый `host_xma_test`
вызывает реальный SDK export после установки compatibility fix, проверяя
interleaved chains, конец буфера, immediate target и повторную установку.
Запуск PID33200, `build/host/logs/legobatman2_010.log`, выявил ещё один
missing callback0x823A1A80 при Load Game .
Добавлен его проверенный16-byte thunk через vtable+0x3C; расширен
`guest_start_test`, RED missing symbol → CTest5/5 GREEN.

Продолжение New Game в старом normal EXE (PID17260).
Чтобы сохранить эту сессию, обновлённый EXE собран отдельно в
`build/pc-play/legobatman2.exe` с теми же шестью runtime DLL.
`launch-game.bat` предпочитает этот путь; `tools/build-host.ps1` также
стадирует туда обычную сборку. Обновление автоматически не запускается
параллельно текущей игре. Успешная загрузка реального сохранения в новом
EXE и длительный геймплей пока требуют проверки.

## Архив: предварительные гипотезы до контрольных запусков

Нижеследующие предположения и ожидания результатов устарели; актуальные
выводы приведены выше.

RTV black screen исправлен точной оценкой unclipped draw extent:
см. rtv-black-screen.md. Этот документ относится к двум оставшимся
симптомам, которые рассматриваются совместно.

## Наблюдения

Катсцена останавливается на крупном плане Лютора. При проверке
фоновые анимации ещё двигаются, сюжетные персонажи не продвигаются.
Одновременно отдельные объекты растягиваются/перекрывают кадр крупными
неправильными треугольниками (скриншот 9feed777-f3f4-4217-9238-c0aca76ac328).
Происходит в ROV и в исправленном RTV. Звук проверить не удалось.

Основной процесс PID21032 остановлен для контрольного перезапуска.
Перед этим снят work/analysis/scene-stacks-1.txt и расшифрован
scene-symbols-1.txt по PPCFuncMappings из **того же** EXE (46362 entries),
без использования устаревшей diagnostic.map от /Od сборки.

- Main XThread5536 ждёт RtlEnterCriticalSection из guest8241BA00,
  вызванного82436FD0 →82437070 →823BCFC8.
- XThread6880 занимает CPU в82B65298: цикл ожидания обновления
  значения через указатель из объекта+11024 относительно объекта+11036.
  Вызван из82B6FD50, содержащего VdSwap/VdPersistDisplay.
- GPU Commands29152 в SleepEx из цепочки GPU command processor.
  По коду наиболее вероятно WAIT_REG_MEM; точные параметры ожидания
  пока не сняты, поэтому deadlock и его адрес ещё не доказаны.
- XMA Decoder и Audio Worker в snapshot ждут события. В журнале есть
  ошибки FFmpeg, но связь с зависанием пока не установлена.

## Проверяемая гипотеза

SDK по умолчанию readback_memexport=true/readback_memexport_fast=true.
D3D12CommandProcessor::IssueDraw_MemexportReadbackFastPath при готовом
предыдущем слоте копирует в guest memory результат предыдущего выполнения,
используя текущие memexport_ranges. Это даёт потенциально устаревшие
данные геометрии и маркеры синхронизации.

Контроль: основной EXE, исправленный RTV и точная оценка extents сохранены,
изменён только --readback_memexport_fast=false. Запуск:
work/analysis/scene-full-memexport.log, начат14:38:44. Проверяется
геометрию и продвижение сцены вместе. Результат ещё ожидается; не вносить
параметр в основной default без проверки.

## Если проблема останется

В отдельном графическом модуле подготовлен лог WAIT_REG_MEM после2 секунд
неуспешного ожидания: wait_info, poll address, value, reference, mask,
interval и состояние fast readback. Собрано: build/gpu-diagnostic/
lb2-gpu-diagnostic.dll, лог сборки gpu-wait-build.log. В текущем основном
запуске этот модуль не используется. Он будет нужен для следующего
контроля с сохранением RTV и той же сцены.

Основной src/main.cpp пока не содержит изменений для зависания/геометрии.

---

<a id="section-research-switch-startup-crash-md"></a>

# Switch startup path failure — 2026-11-14

The game was launched through hbmenu title takeover, with no visible error code.
Read the connected console through Windows MTP and preserved its logs in
`work/analysis/switch-console-2026-11-14/logs`.

All three crash entries report an uncaught filesystem_error for
`sdmc:/switch/legobatman2/sdmc:/switch/legobatman2/game`.
The last game log reaches Vulkan swapchain creation and guest arena setup.
Runtime::SetupVfs calls std::filesystem::absolute(game_data_root_): Horizon's
POSIX libstdc++ treats `sdmc:/...` as relative and prepends the working directory.
KernelState also calls absolute() for userdata, so both roots need correction.

The title now supplies rooted `/switch/legobatman2/...` paths for game data,
userdata, cache, metadata and config. libnx resolves them on the default SD
device; POSIX absolute() leaves them unchanged. No SDK patch is needed.

Regression `tests/test_switch_paths.py` checks the actual startup assignments
against POSIX resolution with the console's working directory. It failed on
all five original paths and passes after the change. Python suite: 27/27.
Full Switch rebuild and hash-verified SD packaging passed; log:
`work/analysis/switch-path-fix-build.log`.

Original console NRO preserved locally beside the copied logs, SHA256
31b5977b9572c7f3013c1cd51183d7069806a57fd6f2342ab12a4dc8d18b4c53.
Rebuilt NRO SHA256
39975668eaedca66323098732e60dea8c6e29ce56dc361963df28825f0e5c357.
Uploaded as `sd:/switch/legobatman2/legobatman2-fixed.nro` through MTP,
then copied back and verified SHA256 against the build. The original NRO
remains beside it. The fix still needs a console launch to verify progress
past VFS initialization.

Follow-up: console logs 004/005 still showed the original duplicated path.
The normal `legobatman2.nro` read back with the original hash: MTP CopyHere
did not overwrite the existing file. Renamed that file to
`legobatman2-original.nro.bak`, uploaded the fixed build under the normal
`legobatman2.nro` name, and read it back. SHA256 matches the fixed build above.
Console launch of the replaced main NRO
is still required.

## Black-screen hang after the path correction

Console run 007 (21:40:30 on 2026-11-14) passed VFS mounting, loaded the
guest image, started audio, read disc data and created Vulkan pipelines.
At 21:40:36.921, 4 KB guest commits started failing with 0xCE01. The log
records 542-543 MB of backing and 1255 MB mapped in guest views, then stops.
Testing showed a black screen and a console hang requiring a hard reboot.
There is no new exception entry in rex_crash.log. Logs preserved in
`work/analysis/switch-hang-20261114-214325/logs`.

The selected nfsmw SDK commits each small request as a separate heap allocation
and code alias. The pinned sa2-nx SDK already contains a batched commit fix for
the same reported 0xCE01 failure on small commits, at 329 MB backing. Ported
that change: round host backing to 2 MB granules, while retaining the guest
heap's page allocation state and lazy per-view mapping. Patch:
`port/rexglue-horizon-batched-memory-commit.patch`.

`tools/test-switch-memory-commit.py` compiles the actual RexGmCommit body
against a bounded mapping backend. Before: 1024 successive 4 KB commits exceed
the simulated mapping budget. After: two aliases and 4 MB backing suffice.
Repeated commits preserve contents, the final window page stays in bounds,
and a failed permission change releases both backing and alias. All four cases
pass. This verifies batching, not Horizon syscall behavior or console recovery.
The build script runs this regression before building. Python suite: 27/27.
Build log: `work/analysis/switch-memory-fix-build.log`.
New NRO SHA256:
de2d9a2434e78f531b24f0d7f5ad22eaa933262fc3d310de3ef12039d004c7d9.
Hardware verification of this memory fix is pending.

---

# Switch: производительность и графические эксперименты



---

<a id="section-research-switch-480p-md"></a>

# Switch internal resolution and level-load investigation — 2026-11-14

Testing showed approximately 3 FPS, GPU utilization at 100%, a crash during
the opening cutscene, and an indefinite load from a save made after gameplay
began. A different save preceding the opening cutscene loads. The initial
848x480 video-mode TOML was uploaded and read back correctly, but testing
reports rendering still at 720p. Configuration delivery alone was insufficient.

## Title code overriding the mode

Verified against the generated code for the selected, hash-checked base XEX:

- `sub_824B0D70` (`recomp.147.cpp`) calls XGetVideoMode, then overwrites width,
  height and HD flag with 1280, 720 and 1.
- `sub_824B0C90` (`recomp.9.cpp`) can replace the mode from engine globals at
  `0x82D75170 + 2156/2160`, accepting only legacy 640x480 and 1280x720.
- `sub_824167D0` (`recomp.158.cpp`) creates five full-screen color/depth
  surfaces using literal 1280x720. Return addresses are 824171FC, 82417228,
  82417264, 824172A0 and 824172D0. Helpers are `sub_824B0B08/824B0B70`.

Switch-only hooks in `src/platform/switch/resolution.cpp` retain the original
functions, restore the configured mode after the two game overrides, and pass
configured sizes into these five surface allocations. Other caller addresses,
shadow maps, half-size depth, and 1280x608 split targets retain their original
sizes/layout. This is a targeted first hardware test; other render passes may
still contain fixed dimensions. It does not establish that every pass is 480p.

VdSwap instrumentation logs the dimensions actually submitted by the game,
separately from the physical display size. Stack parameter slots 0x54/0x5C
follow the SDK's PPC argument marshaling. Native handheld output can remain
1280x720 while the internal frame is 848x480. Display resolution alone is not
evidence of the internal resolution.

## Validation

`tools/test-switch-resolution.py` extracts and executes the actual two generated
mode functions with a bounded test backend. The original functions failed the
480p checks. With the hooks, mode/HD/refresh fields, stack/LR/nonvolatile
registers, scoped full-screen allocation sizes, and unrelated caller/target
preservation pass (24 checks). The build script runs this check.
Python suite: 27/27. Full Switch NRO build and SD packaging pass. ELF symbols
show separate original implementations and strong hook definitions.
Review also identified raw-cvar parsing bypassing the SDK's effective-mode
validation/presets. The hooks now query VdQueryVideoMode directly; the regression
checks differing raw defaults and effective mode (failed before, passes now).

Build log: `work/analysis/switch-480p-build.log`. Console runtime verification
of internal dimensions, image correctness and performance remains required.

Final NRO SHA256:
`ca2a26fb0b90f4bc3a4e63e704343b02961dfa5fb7514c7a5048b2cc353ba17c`.
Replaced the console's main `legobatman2.nro` through MTP, read it back to
`work/analysis/switch-480p-save-load/updated-console`, and verified that hash.
Previous NRO is backed up locally and on SD as
`legobatman2-before-internal480.nro.bak`. The existing 848x480 config remains.

## Saved-game loading

Run 009 and a read-only backup of both saves are preserved under
`work/analysis/switch-480p-save-load`. Each GAME1 file is 58,888 bytes.
The config read back from this run has width848/height480, scale1.
The log reports successful XamContentCreate/open/close for both save containers
(result 00000000), followed by game archive reads. Late in the run there are
few reads and persistent NVK native submission-pressure retries (0xD5C).
This narrows investigation toward level entry/resource or GPU synchronization;
it does not prove that deserialization and checkpoint restore succeeded.
No save edits or resets were performed. The save-loading issue is unresolved.

Previous run 008 ends with repeated Protect failures (0xCE01) followed by an
unhandled physical write fault. The resolution hooks do not fix that memory
failure. Evidence: `work/analysis/switch-gpu-crash-20261114/logs`.

## Hardware run 010: internal 480p confirmed, GPU cost remains

Testing showed the progressed save now loads immediately and gameplay works,
but still 1-4 FPS with frequent visual stalls. This does not establish that the
earlier loading issue is permanently fixed. Evidence copied to
`work/analysis/switch-480p-hardware-test`, including nested `rex/rex_perfil.log`.

Run 010 logs effective mode848x480, four active full-scene allocations changed
to848x480, and submitted frames0-4,300,600 all848x480. Thus the game is sending
480p frames; physical screen output remains720p. The overlay separately keeps
its own default1280x720 in SDK switch_perf.cpp unless the title explicitly calls
RexSwitchPerfResolution. The title now publishes the effective mode there;
two new regression checks fail before the change and pass after it (26 total).

The log contains3097 native submission-pressure warnings. The last10-second
profile window reports2.8 guest FPS,4024 draws/frame,42.8 submits/frame,
36 resolves/frame and616.9 actual transfer draws/frame. NV fence waits cost
663ms per second; new NvMap allocation rate and sampler stalls are both zero.
Scissor-area counters are bounds, not actual shaded-fragment counts. Pressure
and fence waits locate a bottleneck but do not yet identify an optimizable
root cause; do not treat the retry warning itself as proof of a driver bug.

Added six lightweight calling-thread timers for draw handling, synchronous
memexport readback, submission, fence completion, resolves and texture requests.
The SDK changes are in `port/rexglue-horizon-gpu-profile.patch`; Switch CMake
requires the scopes and supplies the header. The executable aggregates and logs
every10seconds. Categories overlap, and interval counts/totals/maxima are
approximate because atomics are drained independently across producer threads.
These are wall-time measurements, not GPU timestamp queries. No draw/copy,
readback, barrier or synchronization commands are omitted or changed.
Performance is not claimed fixed. Next hardware run should spend30-60seconds
in the slow gameplay scene and return to hbmenu for log retrieval.

Diagnostic NRO built/packaged successfully and replaced the main console NRO;
readback SHA256 matches:
`685ad1e3e98e39506af7375588bc22b82e607ca231bcddaabbd783f6c1c68e83`.
Readback: `work/analysis/switch-gpu-profile-delivery/legobatman2.nro`.
Previous main preserved as `legobatman2-before-gpu-profile.nro.bak` on SD.
Logs: `switch-gpu-profile-final-build.log`, `switch-gpu-profile-python-tests.log`
under work/analysis. Mode/overlay regression26/26; Python suite27/27.
Code review found no upload blocker; approximate interval counters are labeled.
Hardware performance measurements from this diagnostic NRO are pending.

## Hardware run 011: submission backpressure dominates slow gameplay

Testing showed no perceptible improvement and constant99%GPU utilization.
Fresh logs preserved at `work/analysis/switch-gpu-profile-hardware`.
In the final steady gameplay windows:

- 10.04s/30frames: draw handling1317ms, memexport319ms, EndSubmission8441ms,
  fence completion230ms, resolve13ms, texture requests41ms.
- 10.17s/30frames: draw1435ms, memexport454ms, EndSubmission8480ms,
  fence completion277ms, resolve13ms, texture requests42ms.

Categories are inclusive. EndSubmission includes deferred command encoding,
vkEndCommandBuffer, queue acquisition and vkQueueSubmit. It cannot yet be
attributed wholly to vkQueueSubmit. Native profiler simultaneously reports
696ms/s inside blocking nvFenceWait, about4145draws/frame and613transfer draws.
No new pipeline creation in the final interval. Internal848x480 remains.

Reviewed `0xD5C`: Nvidia module348, description6 is Timeout, not insufficient
memory. Mesa waits for the oldest accepted fence and retries native submission;
near1988entries is a near-full queue batch. GPU99% and that wait establish
backpressure, not an unnecessary wait or a safe optimization. Adjacent command
range coalescing already exists in Mesa. No performance fix established.

The suggested L+R+ZL built-in rotation was withdrawn before relying on results.
Review found it skips memexport-producing guest draws and updates resource
validity despite omitted commands; queued work and stale cached contents carry
between modes. It is not a controlled correctness-preserving experiment for
LB2. The skip-all mode also retains barriers, copies and submissions. Do not
infer a driver-only bottleneck from unchanged FPS in that mode.

Safe next diagnostic: split EndSubmission timing into deferred Execute,
vkEndCommandBuffer, queue acquisition and vkQueueSubmit before another hardware
measurement. Do not ship speculative barrier/readback/draw omission as a fix.

## Follow-up diagnostic: separate encoding from queue submission

Implemented the four nested stages in
`port/rexglue-horizon-gpu-profile-stages.patch` (apply after the base profiling
patch). IDs6-9 are encode, end_command_buffer, queue_acquire, queue_submit.
The commands, arguments, ordering and error handling are unchanged. Ten-category
reports remain approximate/inclusive. Build and mode/overlay regression pass.

Testing completed the previously requested skip-work experiment in run012;
logs are at `work/analysis/switch-gpu-ab-hardware`. Baseline was2.1FPS before
rotation; omitted-work modes0.3-1.0FPS, and normal afterward0.4FPS. Because of
invalidated contents and cross-window queued work, do not use those numbers to
select a performance fix. 

Blocked the inherited shortcut with a link-time RexSwitchPerfToggleAb wrapper
that only logs a warning. Review and executable disassembly confirm the input
driver calls the wrapper and the original toggle is absent from linked symbols.

Final stages NRO SHA256:
`6f5624dd4597744e80c9e68bf32ca092ef105453a70626faeaad075111050e47`.
Replaced main console NRO and verified a complete readback at
`work/analysis/switch-gpu-stages-delivery/legobatman2.nro`. Previous main kept on
SD as `legobatman2-before-gpu-stages.nro.bak`. Build log:
`work/analysis/switch-gpu-stages-final-build.log`. Performance fix unresolved;
next measurement is30-60seconds normal gameplay after restarting the process,
without any shortcut or omitted work.

## Hardware run 013: native submission confirmed as the waiting site

Fresh run and nested native profiler preserved under
`work/analysis/switch-gpu-stages-hardware`. Also copied the shareable guest
shader/pipeline cache read-only into its shader-cache subdirectory for offline
investigation; no console cache was changed.

Four final ordinary gameplay windows show:

| interval | frames | encode ms | vkEnd ms | acquire ms | vkQueueSubmit ms |
|---|---:|---:|---:|---:|---:|
|10.07s|33|189.48|1.23|0.19|8124.63|
|10.07s|34|171.58|0.62|0.12|8354.87|
|10.21s|31|181.63|0.79|0.14|8233.31|
|10.15s|31|170.88|0.47|0.09|8320.02|

This isolates the dominant waiting site inside vkQueueSubmit, not CPU-side
command encoding, command-buffer finalization or queue mutex acquisition.
Approximately3FPS persists with848x480 and normal rendering. Prior native
fence/pressure evidence is consistent with native queue backpressure. It does
not prove an avoidable driver stall: the GPU may be consuming legitimate work.

Source inspection confirms adjacent command ranges already coalesce in
nvk_queue_try_merge_exec and nvk_cmd_buffer_flush_push_flags. NO_PREFETCH/SYNC
boundaries intentionally prevent merging and cannot safely be removed from
this evidence. NVK batches64 execs per enqueue then accumulates toward the
libnx2048-entry queue limit. Increasing queue capacity or suppressing waits is
not established as a throughput fix. No new binary/configuration was changed
during run013 analysis. Performance remains unresolved; no further resolution
experiment is warranted by the current evidence.

## Deferred image-correctness report — 2026-11-15

Testing showed depth-like rendering artifacts and, after reviewing older
recordings, places their first appearance around the480p trials. This is a
reported timeline, not an established cause. Testing explicitly defers
artifact diagnosis and keeps FPS as the current priority. Preserve this issue
when assessing later performance changes; do not attribute it to run020's
observational instrumentation. Photo evidence:
`work/analysis/switch-intro-queue-transfer-hardware/user-render-artifacts.png`.

---

<a id="section-research-switch-intro-queue-md"></a>

# FBO opening: native queue and transfer diagnosis — 2026-11-15

Status: diagnostic run020 collected. It identifies native resource-recovery
waiting and the dominance of stencil-bit transfer draws. A guarded4x-to1x copy
trial is now installed; see `switch-stencil-copy.md`. Acceptable FPS remains
unresolved; do not treat diagnostic counters or the new trial as a proven fix.

## Findings and external comparison

Run018's specialized FSI trial introduced long compilation freezes and audio
crackling. Reverted the exact NRO and original FBO TOML; control019 confirms
those added symptoms are gone. The original problem remains: about2.8–3.4FPS,
up to8.9seconds/10seconds in vkQueueSubmit. CPU draw preparation is much shorter.
Native FBO counts are about500–600ownership-transfer draws per frame in the
middle of the opening. Queue waiting is a measured site, not a root cause.

Read testing-supplied `external-switch-performance.md` and checked NFSMW's
primary performance history and Mesa notes on the web. NFSMW's initial SDK
GPU backend likewise ran at1–3FPS, but its authors identified CPU emulation
cost. Their specialized renderer still used Vulkan; later results are not a
forecast for LB2. The useful direction is reducing verified excess Xenos/EDRAM
work, not assuming the API or queue size is intrinsically the problem.

Prior dynamic UBO and FSI trials do not justify repeating their changes.
Control019 stderr already logs ZCULL backing allocations; this proves neither
that it covers every relevant image nor that enabling it is a new missing fix.
Local NAK explicitly lacks stencil export. Do not force the extension.
Potential repeated tiling, resolves and clears require LB2 traces and side-effect
validation, including memexport. A specialized native renderer remains a larger
architecture option, with differential PC validation, not an enabled next step.

Sources checked:
- https://github.com/StevensND/nfsmw-nx/blob/main/docs/performance-history.md
- https://github.com/FermataRest/nfsmw-nx/blob/main/docs/mesa.md

## Diagnostic changes

`native_queue_profile.cpp` wraps the exact pinned Horizon driver's exec and
submit APIs. It forwards all arguments, calls the original once, preserves the
result/fence output, then occasionally reads existing host counters. Up to16
channel identities are retained; at most one snapshot/channel/10seconds is
logged. There is no retained driver ownership or GPU query/wait/submission.
The first sample is labelled startup totals; later samples are interval deltas.
Inclusive times overlap and must not be summed.

`NVK_SWITCH_PERF_LOG=1` enables the driver's existing CPU counters before GPU
initialization. Reports separate resource recovery, proactive credit waiting,
native kickoff, locks, mapped-completion polling and report-lag waiting. They
also retain full-barrier and GPU-wait counts and current/peak FIFO occupancy.

`transfer_profile.cpp` records each original FBO transfer draw inside its
existing skip guard. It counts color, depth and stencil-bit operations, raw
shader/source/destination keys, rectangles and recorded pixel area. Area is
not measured fragment work or GPU time. Totals remain complete;96bins bound
metadata, top16retained bins are printed, and ungrouped draws are explicit.
Geometry/draw count, shader code, barriers, waits and readback are unchanged.
Apply `port/rexglue-horizon-transfer-profile.patch` after CPU profiling; when
mixed line endings require it, use git apply's `--ignore-space-change`.

The FSI specialization patch is reversed; CMake defaults itOFF and rejects an
accidentally applied patch. GPU timestamps and dynamic UBOs remain disabled.
The new native profiling uses headers from the exact linked Mesa source.

## Validation and installation

Production-counter, native-wrapper and transfer-accounting tests pass. Tests
cover forwarding/results, counter deltas/reset, startup labelling, grouping,
repeated stencil area, interval reset and bounded overflow. All existing normal
Switch regressions run successfully in the full build. Fresh bounded review
found no blocker. ARM64 disassembly confirms NVK calls reach both wrappers;
ELF includes transfer hooks and no GPU timestamp symbols.

Final build/package: `work/analysis/switch-intro-queue-transfer-build.log`.
Installed NRO:70,301,893bytes, SHA256
`5b1ce3e2e998de161a20c3eaa0b71f632b133e56671372d3e8a6b9113e1dce54`.
Full console readback matches build and manifest; TOML matches original FBO:
`work/analysis/switch-intro-queue-transfer-delivery/verification.json`.
Installation used IFileOperation CopyItem with all UI/confirmations suppressed.
Baseline backup remains `legobatman2-before-intro-fsi.nro.bak` on SD and is
fully readback verified. Failed FSI NRO/TOML remain preserved separately.

Next: collect the next numbered log plus nested rex logs. Check activation,
discard first native startup sample for interval comparisons, identify the
dominant wait type and transfer groups, and only then formulate a bounded
correctness-preserving optimization. Keep using the opening, not gameplay.

---

<a id="section-research-switch-dynamic-constants-md"></a>

# Switch GPU constant-buffer binding optimization — 2026-11-14

**REJECTED as a standalone performance fix.** Hardware run014 regressed logo,
menu and gameplay smoothness. The console main NRO has been restored to the
verified pre-experiment build, and the SDK patch has been unapplied. Default
builds must retain ordinary UBOs. The patch/helper remain as experimental
evidence, not an enabled optimization.

## Evidence and hypothesis

Ordinary run013 takes8.1-8.4seconds per10seconds inside vkQueueSubmit,
versus0.17-0.19seconds encoding commands, with848x480 frames and about3FPS.
Native batches approach the2048-entry FIFO limit and wait for accepted work.
This is a waiting site, not by itself proof of redundant waits.

The guest constant layout in Horizon SDK VulkanCommandProcessor uses five
ordinary VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER bindings. On the pinned Maxwell
NVK path, ordinary non-push UBO descriptors become NVK_CBUF_TYPE_UBO_DESC:
nvk_cmd_buffer_get_cbuf_addr returnsfalse, and nvk_cmd_draw emits an MME call
with an indirect descriptor push. Required no-prefetch boundaries prevent
coalescing across that lookup and may require pending MME synchronization.

Dynamic UBOs instead become NVK_CBUF_TYPE_DYNAMIC_UBO
(`nvk_nir_lower_descriptors.c`). NVK obtains their complete address/size from
its CPU-known root state and emits direct CBUF binding commands. Changing the
application's descriptor type is a Vulkan-supported way to reach that path;
removing driver ordering barriers is unnecessary.

## Implementation

`port/rexglue-horizon-dynamic-constants.patch` applies to the pinned Horizon SDK
after the GPU profiling/stages patches. It changes only Switch guest constants:

- Layout, allocation and descriptor writes consistently use dynamic UBOs.
- The existing VkDescriptorBufferInfo buffers, offsets and ranges are retained.
- The graphics bind loop supplies five zero dynamic offsets when its range
  includes constants set1, and no dynamic offsets for other ranges.
- Uniform pool capacity retains ordinary UBOs for transient compute operations
  and adds dynamic UBOs. The allocator copies the temporary pool-size array in
  its constructor. Per-layout free-lists and retirement stay unchanged.
- Checks the actual device dynamic-UBO limit before creating the layout and
  logs activation. Deferred recording copies the zero-offset array immediately.
- Other platforms retain ordinary guest UBOs and no dynamic offsets.

Helper: `src/platform/switch/lb2_vulkan_constants.h`. Shader code, memexport,
barriers, waits, draws and resolves are retained. Driver shader-cache keys
include descriptor-set layouts, so ordinary-UBO metadata is not reused for
the dynamic layout. Guest shader SPIR-V does not need a changed interface.

## Verification and delivery

`tools/test-switch-dynamic-constants.py` extracts the actual SDK binding loop,
descriptor-type assignments and pool-constructor arguments, then compiles and
executes them with a recording backend. Before the change, direct-path type,
mixed-pool and offset-count assertions failed. Afterward it passes all16 dirty
set combinations, verifies exactly five zero offsets for constants, original
effective buffer addresses, static transient UBO capacity and the unchanged
non-Switch branch. The build script runs this regression.

Existing mode/overlay regression passes; Python suite27/27. Full Switch build
and SD packaging passed (`work/analysis/switch-dynamic-constants-build.log`).
Code review found no blocker in descriptor ABI, limits, lifetimes or caching.

Replaced main console NRO, preserving the previous one as
`legobatman2-before-dynamic-constants.nro.bak`. Full readback verified at
`work/analysis/switch-dynamic-constants-delivery/legobatman2.nro`:
SHA256 `19359d7eb7c4d32cca8ba79526d2dbfad80ce39da913700b8cd1a71573dd53fa`.

## Hardware result and rollback

Run014 evidence: `work/analysis/switch-dynamic-constants-regression`, including
nested native profiler logs. Testing showed frozen-looking intro/menu and
worse gameplay, occasionally reaching the former3FPS. Native submission
pressure warnings disappear (count0); steady vkQueueSubmit totals drop to
33-53ms per10seconds. But synchronous memexport readback rises to8.8seconds,
and fence waits to8.4-8.5seconds per10seconds. Aggregate GPU execution cost did
not improve: waiting moved from submission backpressure to explicit completion.
Early shader/pipeline creation and multi-second stalls also occur; these do
not establish the sole cause of the reported visual regression.

This falsifies the assumption that eliminating the indirect binding path alone
would fix the throughput bottleneck. Correctness/ABI tests were insufficient
to establish performance. Do not remove readback or waits to hide this result.

Restored main console NRO from the verified prior artifact and read it back:
SHA256 `6f5624dd4597744e80c9e68bf32ca092ef105453a70626faeaad075111050e47`.
Readback: `work/analysis/switch-dynamic-constants-regression/rollback`.
Regressed NRO preserved on SD as
`legobatman2-dynamic-constants-regressed.nro.bak`. No save/cache files changed.

Reversed `port/rexglue-horizon-dynamic-constants.patch` in the SDK. The default
binding regression now requires ordinary UBOs, static pool capacity and zero
dynamic-offset count on both platforms. `--experimental` tests the rejected
variant only when explicitly reapplied. Both baseline branches pass.

---

<a id="section-research-switch-memexport-format-md"></a>

# LB2 memexport format specialization — 2026-11-15

Status: hardware run015 confirms specialization but no material FPS improvement.
This is a new experiment after rejecting dynamic constant-buffer descriptors.
Those descriptors remain ordinary UBOs; their rejected patch is unapplied.

## Evidence and hypothesis

Normal run013 stays near3FPS at848x480. Run014 with dynamic descriptors
removed queue-submit waiting but moved the time into memexport fence/readback;
it regressed visible pacing and was rolled back. This does not establish that
the waiting can safely be removed. Current work reduces the submitted shader
work, keeping synchronization and readback intact.

Copied the native Mesa cache read-only over USB into
`work/analysis/switch-native-shader-cache`. The NVK26.2.1/nvk_012b group contains
3332 decoded native shader records. The audit at
`work/analysis/native-shader-audit.json` shows no memory register spills;
the largest vertex variant contains9005 instructions and49011 static cycles.
Its constant-buffer usage matches the guest memexport helper, but a direct
native-key-to-guest-hash mapping has not been proved. Static cycles are not
measured GPU execution time.

Analyzed454 complete shaders in the copied guest cache
`work/analysis/switch-gpu-stages-hardware/shader-cache/57520817.xsh`.
The last record is incomplete; the original cache was preserved unchanged.
Only shader `9E10D1C39A24E18B` has memory exports. It is an84-dword vertex
helper with eight16_16_16_16 integer fetches and two groups of four exports.
Its two address writes are both `mad eA, r0.xxxx, c255.xyxx, c0`.
The generic translator emits packing paths for every runtime export format.
Hypothesis: specializing this helper's invariant format can reduce its cost
without dropping any guest work. FPS improvement remains unverified.

## Change and safety conditions

Apply `port/rexglue-horizon-memexport-format.patch` to the pinned Horizon SDK.
The shared guard is `src/platform/switch/lb2_memexport_format.h`.
Only the exact hash,84-dword length and ordinary host vertex mode qualify.
Specialization requires:

- A valid positive-normal `c0.z` metadata prefix0x4B0.
- `c255.x` equal to either signed zero.
- abs(c1.x),abs(c255.z) <=2^24 and abs(c255.w) <=2^16.
- Previously unused vertex modification bits40..60 clear.

The ordinary initial index is converted from uint32. These bounds keep both
computed r0 values finite, including the second export's add. Therefore both
MADs preserve every bit of c0.z whether fused or unfused. The complete low20
format bits become part of the64-bit translation/pipeline key (bits40..59,
enablebit60). The key survives persistent storage unchanged. Every effective
draw recomputes its guarded key, so constant changes choose another variant
or the existing generic path. Old generic cache entries remain compatible.

Only the format used by packing, numeric conversion, channel order and endian
logic becomes constant. Full dynamic eA validation, bounds and data masks,
fetches, packing operations, stores, barriers and synchronous readback remain.
Other shaders/platforms retain generic behavior. The first four selected and
fallback draws log `LB2 memexport format: specialized=...` with raw constants.
The unsafe old A/B shortcut remains disabled.

## Verification

`python tools/test-switch-memexport-format.py` passes2,109,773 checks:
guard failures, NaN/Inf, signed zero, all low20 format keys, key isolation,
and both export MADs at bounded coefficient/index extremes. The test failed
before the guard existed. It is run by the normal Switch build script.

The actual SDK translator is also built as a local Windows test executable:

```powershell
./tools/test-switch-memexport-spirv.ps1 -ShaderCache work/analysis/switch-gpu-stages-hardware/shader-cache/57520817.xsh
```

It reads testing's copied shader cache; game shader bytes are not bundled in
the test source. It checks actual pinned vertex bitfields, translates generic
and four valid-format variants, reloads each key in a fresh shader/translator
without registers, and requires byte-identical output. All SPIR-V modules
validate before and after optimization with Vulkan1.2 scalar-block layout,
which the console device enables. Optimized byte counts:

| variant | bytes |
|---|---:|
| generic |101192|
| unsigned integer16x4 |17264|
| signed integer16x4 |17804|
| unsigned integer16x4, R/B swap, endian2 |19088|
| float32x4 |17108|

An initial version retained constant BitFieldUExtract switch selectors, which
SPIR-V Tools did not fold. The size regression check failed. Emitting the
equivalent literal format/endian selectors made it pass. These sizes are
intermediate code measurements, not native instruction counts or FPS.

Switch build and packaging passed, including existing resolution, memory
commit, XMA-chain and ordinary-UBO binding checks. No SDK headers were changed;
the161 cached guest objects remain hash-validated. Build log:
`work/analysis/switch-memexport-format-final-build.log`.

The final configuration/packaging check also passed:
`work/analysis/switch-memexport-format-package-verify.log`.
Installed NRO SHA256:
`8258d42fab6524f626d0c43839c71449ea4d86538d51f06d48269f5dd36b0f8e`.
Complete console readback (70,289,605bytes) matches the local artifact and
package manifest. Evidence:
`work/analysis/switch-memexport-format-delivery/verification.json` and
`readback/legobatman2.nro` in the same directory. Previous main is preserved
on SD as `legobatman2-before-memexport-format.nro.bak`.

The installation shell shows full extensions. Earlier copies made with hidden
extensions had accidentally become `*.nro.bak.nro`; their five explicit names
were corrected to `*.nro.bak`, preserving contents and avoiding extra launchers.
The new main remains `legobatman2.nro`.

No save, configuration or console shader-cache file was changed. Hardware
correctness, guard activation and FPS improvement still require a fresh
ordinary30–60second run, followed by copying logs and the updated native cache.

## Hardware run015

Copied the latest log and nested native profiler read-only into
`work/analysis/switch-memexport-format-hardware`, along with the current
Mesa26.2.1/nvk_012b cache. The first four helper draws report
`specialized=true`, format`4B021A01`, c1.x0 and c255.x/z/w0/4/8.
The new cache exactly extends the old cache by7555bytes; it adds one vertex
shader, one geometry shader and one serialized pipeline. The new native vertex
record (`9fa926b95605db52eac98fbc016a9334b7921ae0`) has1147instructions,
11904static cycles and40GPR, versus9005/49011/64 for the prior large variant.
Thus code generation did shrink on the console; static code size was not the
dominant cost in this workload.

The last four CPU-profile intervals remain2.98/3.00/3.03/3.13FPS. Queue-submit
wall time remains7.91/8.18/8.23/8.50seconds in roughly10seconds. Memexport
readback falls within0.39–0.87seconds per interval, overlapping fence timings.
The native profiler ends at3.1FPS,3778draws/frame,36resolves/frame and no new
pipeline creation. These are different windows/scenes, not a controlled FPS
benchmark, but they provide no evidence that this solved the delays.

Machine-readable evidence: `summary.json` in the hardware snapshot, including
file hashes and decoded new shader metadata. The inherited rex_crash.log is
an old startup-path failure; it does not establish a new run015 crash.

Next step is actual GPU timestamp sampling, described in
`docs/research/switch-gpu-timestamps.md`. No barriers, synchronization or
memexport work have been removed based on CPU wait-site measurements.

---

<a id="section-research-switch-gpu-timestamps-md"></a>

# Sparse GPU timestamp diagnosis — 2026-11-15

Status: REJECTED for normal builds after run016 lost the GPU and crashed.
The prior NRO was restored and readback verified. The SDK timestamp patch is
unapplied; normal builds explicitly disable the experiment and reject an
accidentally applied patch. Host tests did not establish driver stability.
This build adds diagnosis, not an established FPS fix.

Run015 proves the copy-shader specialization is active and reduces native
instructions9005→1147, yet gameplay remains about3FPS. CPU measurements still
place most waiting inside vkQueueSubmit. The remaining question is which
submitted work is expensive on the GPU; shortening a large static shader did
not answer it. Keep ordinary UBO descriptors and correct synchronous readback.

## Implementation

`src/platform/switch/gpu_timestamps.cpp` owns all query resources. Apply
`port/rexglue-horizon-gpu-timestamps.patch` after the CPU-profile/stages and
memexport-format patches. It changes only three SDK.cpp files; SDK headers
and the161 validated guest objects remain unchanged.

- DeferredCommandBuffer::Execute starts outside render passes. A batch
  recorder resets one128-slot query page and records a TOP_OF_PIPE timestamp.
  After all deferred commands, it records a BOTTOM_OF_PIPE timestamp.
- Sparse operations use BOTTOM→BOTTOM pairs: draw/indexed approximately1/128,
  compute1/16, copy/clear1/32, plus first observations. A pseudorandom phase
  avoids synchronization with a fixed frame draw count. Query slots reserve
  the final batch timestamp; full pages stop sampling without a mid-pass reset.
- Draw labels retain the guest VS/PS hashes. Internal graphics and compute
  operations retain their Vulkan pipeline IDs. Labels are copied into samples,
  and erased before both guest-pipeline destruction paths, preventing handle
  reuse from changing an old sample's identity.
- At most64pages exist. A page remains pending until its submission fence
  completes and every written slot has availability. Reads request64-bit value
  and availability pairs with16-byte stride, only for the used prefix.
  VK_QUERY_RESULT_WAIT_BIT is never used. Unsupported/missing entry points or
  budget exhaustion skip profiling without omitting any guest GPU commands.
- TimestampPeriod and valid bits come from the actual device/queue family.
  Deltas handle both narrow counter wrap and64-bit wrap. Entry points absent
  from the SDK's function subset are loaded with vkGetDeviceProcAddr; the
  shared SDK Vulkan API header preserves its platform/beta feature defines.
- Failed command-buffer finalization, fence reset, queue submission or query
  result retrieval disables sampling and discards statistics. Pending query
  resources remain untouched until the existing idle shutdown path. This
  prevents abandoned recordings from being interpreted as completed samples.
- Shutdown destroys pools after the original AwaitAllQueueOperationsCompletion.
  No new CPU waits, queue flushes or readback policy changes are introduced.

## Interpretation limits

Timestamp commands themselves can flush pending writes; operation samples are
intrusive measurements. Whole-buffer spans are TOP→BOTTOM and may overlap
other pipeline work or include native scheduling gaps. Sparse BOTTOM→BOTTOM
pairs are approximate local costs, not an exact sum of frame costs. No barrier
cost is sampled: a bottom timestamp before a barrier would drain preceding
work and hide the dependency wait being investigated.

Logs explicitly prohibit summing or extrapolating these measurements. CPU
observed counts and completed query samples can cross report boundaries, so
their ratio and report-window totals are not direct throughput estimates.
This profiler also cannot by itself separate shader execution from MME/PBDMA
waiting. Read results alongside the existing CPU and native profiler logs.

## Verification and next hardware check

`python tools/test-switch-gpu-timestamps.py` compiles the production recorder
against a Vulkan recording backend.1002checks pass, covering absent timestamp
support, missing functions, nonblocking reads, pending/availability gating,
page reuse, used-slot prefixes, full pages,8-bit and64-bit rollover, captured
shader labels, destroyed-handle reuse, bounded resource exhaustion, shutdown,
and abandoned-submission cancellation. It failed before implementation; the
abandonment regression also failed before the error handling was added.

Normal Switch builds run this test and verify all three SDK hooks. Existing
format, resolution, memory-commit, XMA and ordinary-UBO regressions remain.
ARM64 build/packaging log:
`work/analysis/switch-gpu-timestamps-verified-build.log`.

Hardware needs one ordinary30–60second level run without button shortcuts,
followed by USB log collection. Activation should log
`LB2 GPU timestamps: enabled=true`, then `LB2 GPU sample ...` groups. Current
performance remains unresolved; do not present this diagnostic build as a fix.

## Installation

Final package configuration and regressions pass:
`work/analysis/switch-gpu-timestamps-package-check.log`.
Installed main NRO is70,305,989bytes, SHA256
`cfdd07754d4ca5dacdf22b3dc6b6fdab2ce508f83c3da78ad880aa7dd0f4830b`.
The complete MTP readback matches the built NRO and package manifest.
Evidence: `work/analysis/switch-gpu-timestamps-delivery/verification.json` and
`readback/legobatman2.nro` in the same directory. Previous main is preserved
on SD as `legobatman2-before-gpu-timestamps.nro.bak`.
No save, runtime configuration or console shader cache was modified.

## Run016 result and rollback

Testing showed a menu crash and unchanged~3FPS. Copied log016 and nested
profiler/stderr/crash files to `work/analysis/switch-gpu-timestamps-hardware`.
GPU timestamps activate with64valid bits and1ns period. Multiple completed
windows were collected with no exhausted query-page budget. Many largest
operation samples are internal graphics/compute pipelines, commonly0.6–2.2ms;
most guest VS/PS pairs are absent from the printed top16. Pipeline IDs lack
names, so individual internal operations cannot be identified exactly from
these records. They are coarse leads, not additive frame-time measurements.
The native profiler reports about500–600ownership-transfer draws/frame.

Stderr ends with `Graphics device lost`; rex_crash.log adds an abort record.
This is a new GPU failure, distinct from its preserved old startup-path errors.
The precise failure cause is not proven. Retired the new query experiment
rather than collecting further data with an unstable build.

Restored the exact previous main NRO, SHA256
`8258d42fab6524f626d0c43839c71449ea4d86538d51f06d48269f5dd36b0f8e`.
Full readback verification:
`work/analysis/switch-gpu-timestamps-rollback/verification.json`.
The failed NRO is preserved on SD as
`legobatman2-gpu-timestamps-regressed.nro.bak`.

Reversed `port/rexglue-horizon-gpu-timestamps.patch`; the normal Switch build
passes (`work/analysis/switch-gpu-timestamps-rollback-build.log`). Its ELF has
no Lb2GpuTimestamp symbols. CMake's experimental option defaultsOFF and the
normal build script explicitly passesOFF; opt-in reproduction requires the
patch. The standalone1002-check recorder test is retained as evidence, not
as a claim that GPU queries are reliable on this driver.

The next controlled trial uses the existing FSI path on the restored NRO,
described in `docs/research/switch-fsi-trial.md`.

---

<a id="section-research-switch-fsi-trial-md"></a>

# Switch FSI architecture trial — 2026-11-15

Status: FSI trial retired after run018's freezes/audio regression. Original FBO
configuration and prior NRO are restored and readback verified. Control019
confirms the regression is gone and the original~3FPS remains. See
`switch-intro-fsi.md`; further queue diagnosis stays on FBO.

Run015 reduced a guest memexport shader9005→1147native instructions without
material FPS gain. Run016's sparse GPU probes repeatedly rank internal
graphics/compute operations highly; native statistics show about500–600
ownership-transfer draws per frame. The query build then lost the GPU and was
rolled back. Do not re-enable it to judge this trial.

The ordinary FBO backend emulates guest EDRAM using host images. Changing
ownership/layout requires copies, including up to8extra stencil-bit draws
where shader stencil export is unavailable. Repeatedly adjusting resolution
or guest constants does not remove that architectural cost.

The existing FSI backend writes packed guest EDRAM with fragment interlock;
it avoids host-image ownership transfers and preserves the existing memexport
and synchronous readback implementation. This is a supported alternative in
the same Xenos/Vulkan/NVK renderer, not a deko3d integration.

## Capability and correctness checks

Run016 explicitly reports VK_EXT_fragment_shader_interlock, both pixel/sample
interlock features, fragmentStoresAndAtomics, sampleRateShading, standard
sample locations, maxStorageBufferRange UINT32_MAX and1048576storage slots.
These satisfy every FSI-path check in VulkanRenderTargetCache::Initialize
(one shared-memory binding is below the storage-buffer slot limit).

Mesa advertises interlock on MaxwellB through pre-Volta GPUs; the local
nak_nir_lower_interlock handlesSM52–69, including the consoleSM53. Its lowering
holds ordering over the whole fragment invocation, not only the translated
depth/blend section. This can serialize textures/shading under overdraw.
FSI may therefore improve or worsen performance; capability support alone
does not establish a speedup.

Use both settings from `config/switch-480p-fsi.toml`:

```toml
render_target_path_vulkan = "fsi"
vulkan_dynamic_rendering = false
```

Classic FSI render passes include explicit external/subpass BY_REGION fragment
read/write dependencies. The fork's dynamic-rendering path bypasses that pass
without an evident equivalent inter-pass dependency. Keep classic rendering
for this trial to use the existing FSI synchronization design.

Built the actual SDK translator as a Windows audit tool under
`work/analysis/fsi-shader-audit`, with interlock enabled. All454complete cached
guest shaders (220VS,234PS) translate without errors. Every output validates
as Vulkan1.2/SPIR-V1.5 with scalar-block layout; validation.json records zero
failures. The incomplete final guest-cache record is skipped and preserved.
This validates translation structure, not native-driver compilation or FPS.

## Delivery and hardware comparison

Restored main NRO remains the exact verified baseline:
`8258d42fab6524f626d0c43839c71449ea4d86538d51f06d48269f5dd36b0f8e`.
GPU query instrumentation and rejected dynamic UBOs are absent.

Copied the original runtime TOML read-only to
`work/analysis/switch-fsi-trial/before/legobatman2.toml` and preserved it on SD
as `legobatman2-before-fsi.toml.bak`. The new TOML changes only the two renderer
settings; a parsed comparison confirms the original848x480/scaling values.
Full readback matches the local trial file. Evidence:
`work/analysis/switch-fsi-trial/delivery/verification.json`.


The first start creates separate FSI pipeline storage (`57520817.fsi.vk.xpso`),
so allow shader compilation before judging steady gameplay. Fully restart,
load the same level and compare30–60seconds of gameplay, then collect logs.
Confirm path activation, ownership-transfer count, correct picture, stability,
and FPS; do not declare the architecture change successful from shader
validation alone. Preserve the FBO configuration for immediate rollback.

---

<a id="section-research-switch-intro-fsi-md"></a>

# Initial cutscene: FSI specialization trial — 2026-11-15

Status: REJECTED after hardware run018. Testing showed long freezes, crackling
audio and1–6FPS. Exact prior NRO and original FBO configuration were restored
and fully read back. Control run019 confirms the added regression is gone but
the original~3FPS remains. The FSI SDK patch is reversed; normal builds require
it absent. Retained tests and patch document the experiment, not an enabled fix.

## Evidence and scope

The initial cutscene replaces gameplay as the repeatable hardware workload.
Read-only USB collection in `work/analysis/switch-intro-baseline` contains
run017, profiler/stderr/crash logs, the actual runtime TOML and installed NRO.
There was no newer run on the console. Installed configuration was already
848x480, FSI, classic render passes. Baseline NRO SHA256:
`8258d42fab6524f626d0c43839c71449ea4d86538d51f06d48269f5dd36b0f8e`.

Run017 contains a 29,157.73ms maximum EndSubmission wall time, compared with
156.66ms maximum vkQueueSubmit in that reporting window. These are inclusive,
approximate CPU intervals, not additive GPU costs. PipelineCache::EndSubmission
helps compiler workers and waits for them before deferred command execution;
this is a distinct source of pauses from submission backpressure.

The experimental implementation specializes FSI color formats and
selected blend words. The underlying register values match those copied by
UpdateSystemConstantValues into the existing FSI constants. Replacement stays
inside the original write/keep-mask guards. Unsupported formats use the entire
generic path; unsupported blends retain dynamic blending. Depth/stencil,
interlock, barriers, draws, memexport and synchronous readback are preserved.

## Implementation and persistent storage

Apply `port/rexglue-horizon-fsi-specialization.patch` after the existing
memexport patch, then `port/rexglue-horizon-pipeline-profile.patch`.
The helper is `src/platform/switch/lb2_fsi_specialization.h`.

Unused pixel modification bits49..63 encode a self-contained format/blend
tuple. Zero keeps the original behavior. The shader's immutable written-target
mask determines the tuple radix; existing bits0..48 survive unchanged. Each
draw computes its key from the current registers. Fresh translators can reload
the same key without register state. The table order is persistent ABI.

Switch FSI pipeline descriptions use `57520817.fsi-lb2-v1.vk.xpso`. Reusing the
old `fsi.vk.xpso` would eagerly compile unused generic pipelines at startup,
even though supported new draws use different specialized keys. The old file
is preserved for rollback. FBO names, shared guest `.xsh` and Mesa caches retain
their existing paths. No console cache files need to be deleted.

Two CPU categories extend the existing profiler:

- `pipeline_prepare`: PipelineCache::EndSubmission, including processor-thread
  compilation and waiting for workers; nested in the existing `submit` scope.
- `pipeline_create`: time inside vkCreateGraphicsPipelines on all creation
  threads. Worker totals may overlap and cross report boundaries.

These distinguish compilation from `queue_submit` and `fence` waits. They do
not measure shader GPU execution. GPU timestamp queries remain disabled.

## Verification

- Production CPU counter test passes with deterministic time: added categories,
  accumulation, mean/maximum, out-of-range handling and interval reset.
- FSI key test passes1,479,252 checks, including all representable tuples,
  unsupported formats/blends, occupied bits, old keys and storage isolation.
- Actual SDK SPIR-V translation/reload test passes for27 variants from real
  one-, two- and three-target pixel shaders. Raw and optimized modules validate
  for Vulkan1.2/SPIR-V1.5 with scalar block layout. Optimized generic versus
  specialized no-blend sizes are84,524→19,784;159,644→31,812;
  248,284→57,192bytes. This is intermediate-code reduction, not a native GPU
  instruction count or measured speedup.
- Full Switch build runs the existing timestamp lifecycle, memexport, ordinary
  UBO, resolution, memory and XMA regressions. Both new SDK hooks are required
  by CMake. Independent code review identified the old-cache startup issue;
  the separate namespace addresses it. No other blocking issue was found.

Evidence: `work/analysis/switch-intro-fsi-spirv.log`,
`work/analysis/switch-intro-final-build.log` and the delivery readback manifest.

## Console delivery

Installed main `legobatman2.nro`:70,297,797bytes, SHA256
`0eb3f509250609429f4ec17c1638258ddf8dabce177961ba2d05f53c6562f24f`.
Full readback matches both built NRO and packaged manifest:
`work/analysis/switch-intro-delivery/verification.json`.
Runtime TOML readback is byte-identical to the baseline, so the comparison
retains848x480 and classic FSI. Rollback copies are preserved on SD as
`legobatman2-before-intro-fsi.nro.bak` and
`legobatman2-before-intro-main.nro.bak`; the uploaded backup was fully read back
and verified against the baseline hash. Saves and console caches were not
edited or deleted.

Shell upload/rename initially failed due to MTP state; a uniquely named staged
upload was fully verified, MTP was restarted, and IFileOperation completed the
final rename with silent/no-confirmation/no-error-UI flags. The final main
filename was checked before readback. Future Explorer-mediated operations
must suppress confirmation windows, as testing requested.

## Hardware procedure

Fully restart the game with the same settings and clocks. Watch the initial
logos and initial cutscene for60seconds without skipping it or loading a level.
If the new cache is still compiling, note whether animation resumes and give
the approximate longest pause. Exit normally if possible, reconnect MTP and
collect the next numbered game log plus nested `rex` logs. A second launch of
the same opening distinguishes cold compilation from warm steady performance.

Compare image correctness, hangs/crashes, guest frames per profiler window,
pipeline creation counts/time, pipeline preparation, queue submission and
fence waiting. Acceptable FPS has not been demonstrated. If performance or
correctness regresses, restore the backed-up NRO and retain both trials' logs.

## Run018 and FBO control019

Run018 evidence: `work/analysis/switch-intro-fsi-hardware`. Pipeline preparation
blocks up to33,536.92ms; native creation peaks at20,401.57ms. Later windows have
5.69/5.31guest FPS with7,542.97/7,293.86ms inside vkQueueSubmit and only2/5new
pipeline creations. Compilation is one problem, but does not explain steady
low throughput. Native submission pressure recurs1,968times. There is no new
device-loss error in stderr. Testing showed that the cutscene never finishes.

Restored NRO SHA256 `8258d42fab6524f626d0c43839c71449ea4d86538d51f06d48269f5dd36b0f8e`
and exact pre-FSI TOML; readbacks are under `work/analysis/switch-intro-rollback`.
The failed NRO and TOML are preserved on SD as
`legobatman2-intro-fsi-regressed.nro.bak` and
`legobatman2-intro-fsi-regressed.toml.bak`. Testing confirmed crackling and the
new regression are gone. Control019 evidence: `work/analysis/switch-intro-fbo-hardware`.
Steady windows yield2.8–3.4guest FPS,8.0–8.9seconds in vkQueueSubmit per10seconds,
and0.9–1.9seconds preparing guest draws. Native profiler counts about
500–600ownership-transfer draws/frame in the middle of the cutscene. Those
counts establish work volume, not the cost of individual operations.

Next diagnostic stays on FBO, preserving all draws and waits. It enables the
pinned driver's existing CPU counters (`NVK_SWITCH_PERF_LOG`) and samples them
after original native exec/submit calls, once per channel per10seconds. This
can distinguish resource-recovery waits, proactive credit waits, native kickoff,
channel locks and mapped completion polling. It adds no GPU queries and changes
no FIFO limits, scheduling, synchronization or memory-export behavior.

---

<a id="section-research-switch-stencil-copy-md"></a>

# Guarded4x-to1x stencil-copy trial — 2026-11-15

Status: run021 activated the replacement but gave no observed FPS improvement.
The exact diagnostic NRO and original FBO TOML were restored and readback
verified. A no-ZCULL/original-copy control is being prepared to separate the
trial's two performance changes. Acceptable FPS remains unresolved.

## Run020 evidence

`work/analysis/switch-intro-queue-transfer-hardware` contains the game/native
logs, parsed CPU windows, candidate groups and testing's photo. Testing
defers depth-like artifacts that they first noticed in older480p recordings;
that timeline is not a proven cause, and existing artifacts must not be
misclassified as new trial regressions.

Native reporting identifies7–8seconds/10seconds in resource-recovery waiting.
Proactive credit/throttle waits are zero; kickoff/locks/mapped polling are much
smaller. Warm FBO windows remain~3FPS despite almost no pipeline preparation.
Approximately85percent of internal transfer draws are stencil-bit passes.
Source00305EB6→destination00205EB6 and003062D0→002062D0 are matching base/pitch/
format depth4x→depth1x conversions. Their recorded area exceeds half of stencil
area in several opening windows. Draw/area counts do not establish exact GPU
cost, but identify a bounded optimization candidate.

## Replacement and fallback

Startup flag `lb2_stencil_copy` defaultsfalse. When enabled, same-base/pitch/
format depth4x→depth1x transfers at scale1 with standard sample locations and
one unmerged invocation may use compute packing plus stencil-only buffer-to-
image copying. Source coordinates are floor(destination/2); sample index is
`(x&1)|((y&1)<<1)`, matching the original SDK's depth mapping. Four stencil bytes
are packed per uint32, with zero right padding. Exact existing transfer
rectangles are retained, and padded local EDRAM tile indices>=2048 fall back.

Other formats/layouts/directions, scaled draws, merged invocations, bad bounds,
staging limits and preparation failures retain the original stencil draw path.
Original depth transfer, source data, target stencil values, guest memexport
and synchronous readback are preserved. No8-bit storage feature or stencil-
export extension is required. Buffer-to-image copies require a1x destination,
which is why the opposite MSAA direction is not changed.

Pipeline/layout are title-owned and destroyed on the existing idle shutdown
path. Existing transient descriptors and command-processor scratch allocation
handle buffer lifetime. Explicit scratch compute-write→transfer-read and
transfer-read→compute-write dependencies protect reuse. Source/destination
image barriers and render-pass end/re-entry are recorded around the copy.
No CPU wait or GPU query is added. SDK headers remain unchanged, retaining the
verified161guest-object cache.

The trial requestsTRANSFER_DST on1x depth images. NVK consequently disables
ZCULL for these images through its existing conservative rule. This is an
explicit performance tradeoff; the driver rule is not bypassed, and a gain
must be measured rather than inferred from fewer draws.

## Validation

-21CPU checks for fail-closed eligibility, origin/bounds, row padding, capacity,
  valid final tiles and rejection of padded EDRAM tile2048. The padded-tail
  regression failed before its guard was added.
- Actual production SPIR-V (3308bytes) passes spirv-val for Vulkan1.2.
- Headless GPU validation on NVIDIA RTX5060Ti/Vulkan1.4.341 passes14cases and
  131,712byte comparisons across D24_UNORM_S8_UINT and D32_SFLOAT_S8_UINT.
  Source images are filled through960real per-sample stencil draws per format.
  Cases verify odd origins/sizes, padded rows, output canaries, copied stencil,
  unchanged outside stencil and preserved depth.
- Fresh bounded final review finds no Critical/Important issue in mapping,
  aliasing, scratch/descriptor lifetime, barriers, pass restoration or fallback.
- Full ARM64 build/package and existing normal regressions pass. This validates
  compilation and host behavior, not the complete Switch/NVK integration.

Evidence: `work/analysis/stencil-copy-gpu/{results.txt,spirv-validation.log}`
and `work/analysis/stencil-copy/switch-build-package.log`.

## Installation

Installed NRO:70,314,181bytes, SHA256
`dfc9b935f2f6969b86b13a4336f40de054fb4a62b306e5538fd194d95106a90a`.
Readback matches built NRO and package manifest. Runtime TOML changes exactly
one parsed setting from control020: `lb2_stencil_copy=true`.
Proof: `work/analysis/stencil-copy/delivery/verification.json`.

Rollback NRO: `legobatman2-before-stencil-copy.nro.bak`, SHA256
`5b1ce3e2e998de161a20c3eaa0b71f632b133e56671372d3e8a6b9113e1dce54`.
Rollback TOML: `legobatman2-before-stencil-copy.toml.bak` (278bytes).
Both SD backups were fully read back. Installation used silent IFileOperation
CopyItem; no confirmation windows. Saves and console caches were not removed.

Check activation (`LB2 stencil copy: ... ready`), nonzerofast_stencil_groups,
stencil draw/area reduction, original transfer totals, native recovery time,
FPS and newly introduced defects. If the trial regresses, restore both backups
silently and verify full readback before the next experiment.

Vulkan constraints checked against primary references:
- https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBufferToImage.html
- https://docs.vulkan.org/refpages/latest/refpages/source/VkImageStencilUsageCreateInfo.html

## Hardware021 and interpretation

Activation logs `compute4x-to1x ready`; several windows replace374–495groups,
removing about3,000–4,000original stencil draws/10seconds. Recorded transfer
area drops from about4.6billion to2.1billionpixels in comparable opening
windows. Nonetheless guest FPS remains about2.8–3.3, with native recovery
still7–8seconds/10seconds. Testing showed unchanged FPS and the old menu
artifacts. No new device-loss error appears.

This does not prove stencil processing is free: the change also disabled
ZCULL on1x depth images and added compute/copy transitions. Parsed current
pipeline storage contains512checksum-valid records, including1361x pipelines
with depth test/write enabled. Stored states are not frequency measurements,
but prevent dismissing the ZCULL confound. The parser extracts the exact
PipelineDescription declarations, uses matching GCC bitfield packing and
verifies every XXH3 hash. Evidence and summary are under
`work/analysis/stencil-copy/hardware`.

Restored NRO SHA2565b1ce3e2e998de161a20c3eaa0b71f632b133e56671372d3e8a6b9113e1dce54
and original278-byte TOML. Proof: `work/analysis/stencil-copy/rollback/verification.json`.
Failed standalone trial preserved on SD as
`legobatman2-stencil-copy-no-gain.nro.bak` and
`legobatman2-stencil-copy-no-gain.toml.bak`.

Read-only queue review finds no proven fixed-delay/accounting bug. Accepted
oldest fences are used correctly; recovery5ms slices are not a programmed
50ms delay. Native counters overlap. The existing libnx profiler reports
milliseconds per SECOND, so its~0.8seconds/s of fence waiting agrees with
~8seconds/10seconds of native recovery; it is not a missing-time discrepancy.

Next control sets only `lb2_stencil_no_zcull_control=true`: ordinary stencil
draws remain, while1x depth images get the sameTRANSFER_DST usage/ZCULL
eligibility as021. Expanded existing counter snapshots expose recovery-call
counts, mapped native fallbacks/hits and lifetime maximum call duration.
No driver wait, FIFO limit, ordering or shader is changed for this control.

Control installed and fully read back:70,318,277bytes, SHA256
`c17d448c59334da3f5709e36338e50c47b2859ca1dee8c67c6bcf63fe57ef8c1`.
NRO matches the built package; parsed TOML differs from the baseline only by
`lb2_stencil_no_zcull_control=true`. Fast stencil copy remains disabled.
Proof: `work/analysis/stencil-copy/no-zcull-control/verification.json`.
Full build log: `work/analysis/stencil-copy/no-zcull-control-build.log`.
Awaiting the same60second opening run (expected log022); hardware FPS and
correctness remain unverified.

## Control hardware022/023

022 is a short interrupted start;023 contains the complete run. In opening
windows2–5, weighted guest FPS is3.108 versus3.073 in020 and3.029 in021.
Per-window023 FPS is3.20,3.03,3.33,2.88. These unreplicated approximate
windows do not establish a meaningful gain. Testing showed possibly fewer
freezes, unchanged FPS or at most0.5–1FPS more, and the existing artifacts.
Fast stencil groups remain zero. Stderr confirms1x depth images have no ZCULL
backing while other sample counts retain it.

Native recovery remains7.14–7.82seconds/10seconds. There are1523–1680
recovery calls/window, mean4.56–4.69ms/call and lifetime maximum5.27ms
over the complete run. Credit/throttle/report-lag time remains zero. This
rejects a large fixed recovery-sleep overshoot in this run, without proving
all driver synchronization is optimal. Pipeline preparation becomes negligible
in warmed windows while queue submission still dominates CPU wall time.

Evidence: `work/analysis/stencil-copy/no-zcull-control/hardware/comparison.json`
and logs022/023 with nested rex files. Original stencil processing with ZCULL
off is not substantially slower than020; ZCULL loss does not explain a hidden
large win from the021 copy trial. No tested variant reaches acceptable FPS.

Next investigation follows the actual emitted guest draws: shader pair, raw
surface/depth/color state, primitive type, memexport/rasterization flags,
scissor, draw counts and vertex/index counts. These remain workload proxies,
not measured GPU time. Preserve ordinary stencil, original FBO configuration,
all side effects and queue ordering; timestamps/skip-work remain disabled.

---

<a id="section-research-switch-guest-draws-md"></a>

# Opening guest draw census and no-effect point candidate

Run024: original FBO config, stencil and no-ZCULL trials disabled. Testing showed
unchanged behavior. Opening windows2–5 average3.087FPS; no group overflow in
those windows. Later scene changes exhaust the512-group census, reported
explicitly; those ranked groups are incomplete. Preserve that limitation.

Repeated point draws use VS B6C9863F710683EC, absent PS or PS A4A965C189287B99,
normalized depth-control0, normalized color-mask0, no analyzed memexport,
and16x16 scissor. The exact VS disassembly writes constant position(0,0,0,1)
and constant interpolator0; no fetch/export side effect. The PS copies its
interpolator to color0. Printed top groups alone give a substantial lower
bound on this workload:32.7–46.5percent of total emitted guest draws in
opening windows2–5. These groups do not reveal active guest queries or all
eligibility fields; this is not a count of safely removable commands.

Many other leading groups are real960x960 depth/stencil passes. Large
scissor bounds (including8192high clears) are not covered-pixel measurements;
framebuffer clipping and geometry matter. Do not discard these draws or
assume identical scissor/shader means redundant output.

Concrete safe candidate: eliminate only known auto-indexed normal point
draws with the exact VS and absent/exact PS, normalized depth-control0,
normalized color-mask0, no shader memexport, and no active host guest-query.
Everything else retains the original path. Guard comes after copy dispatch
and shader analysis, before BeginSubmission/primitive preparation.

Query handling audited: EVENT_WRITE_ZPD begin sets active query valid before
draws; end closes/submits/waits and synchronously writes results. Active
queries must retain the original point draws. Disabled/failed/nested queries
use draw-independent fallback results. Alpha discard/coverage cannot affect
any stored result with these zero writes and no active query. No stream-out
path exists here beyond explicitly excluded shader memexport. Pending work,
callbacks and queue fences keep their existing implementation.

Evidence: work/analysis/guest-draw-census/hardware/{legobatman2_024.log,census.json}
and work/analysis/shader-audit/ucode/{B6C9863F710683EC,A4A965C189287B99}.txt.
This identifies semantically unnecessary work, not its measured GPU cost or
a proven solution to3FPS. Startup flag defaults false; hardware trial required.

Implementation further restricts source to auto indices, implicit vertex path
and ordinary position export. It checks the bound PS identity even when the
SDK discarded the effective pixel stage, keeping unknown bound programs on
the original path. Runtime report separates candidate_calls, eligible_calls,
elided_calls, query_blocked and other_blocked; interval boundaries are
approximate under concurrent report/production. Call counts before primitive
preparation are not assumed equal to eventual emitted-draw counts.

Trial installed after93predicate/accounting checks and full normal ARM64
regressions/package:70,326,469bytes, SHA256
`5f5e97d869fdbe86c9ed7f07191c7412349217a02aeca9763e2616be396b65a6`.
Full main and rollback-file readback verified under
`work/analysis/no-effect-points/delivery/verification.json`.
Only parsed config change from024 is `lb2_elide_no_effect_points=true`.
Bounded review confirms copy handling, live queries, unknown bound shaders
and exports retain their original path. Hardware run025 pending.

## Hardware025

The guard activates: opening windows2–5 elide37,576/37,744/40,337/36,503
candidate calls, all eligible, with query_blocked=other_blocked=0. Actual
emitted guest draws fall from77k–99k/window to41k–66k. Weighted FPS is3.150
versus3.087 in024; queue-submit wall time remains7.88–9.11seconds/10seconds.
Testing showed unchanged behavior or perhaps0.5FPS improvement. No new
device-loss error. This is not a material or independently replicated gain.
The no-effect point path is retained opt-in/defaultfalse; disable it in the
next measurement to keep the original renderer as reference.

Evidence: work/analysis/no-effect-points/hardware/{legobatman2_025.log,comparison.json}.
Reducing command count did not remove the principal wait. Next diagnostic
uses bounded sparse full-operation completion waits to distinguish remaining
draw-plus-preparation and copy/resolve latency without Vulkan timestamp queries.

---

<a id="section-research-switch-fence-operation-samples-md"></a>

# Sparse operation completion diagnosis

Status: installed, hardware run026 pending. No FPS improvement claim.

025 eliminated36–40k empty point calls per10s yet opening FPS remained~3.15
and queue-submit waits~8–9s. Earlier stencil/ZCULL controls also remained~3FPS.
Command/area counts do not identify where GPU execution time is spent.

This trial sparsely isolates a complete IssueDraw (including texture/target
preparation and readback, excluding earlier shader analysis) or IssueCopy.
It first drains previous work using the SDK's existing completion path, then
starts timing. After successful operation and destruction of later scratch/
upload locals, it submits/waits again. Reports separate prior drain and CPU
preparation from full isolated completion latency. These are intrusive
CPU+driver+GPU wall times, not pure GPU timestamps or additive frame costs.

Warmup10s; random draw1/128,copy1/8; spacing>=125ms, lifetime<=256 attempts.
Unsafe/failed/cancelled attempts consume the budget. Active guest queries,
unsafe submission boundaries and lost devices are guarded at both drains.
Failed waits disable further samples. No new query objects, shader changes,
omitted draws/copies or driver FIFO-policy changes. The default startup flag
isfalse. Ordinary shader/UBO/readback semantics remain enabled.

TLS attributes emitted guest and ownership-transfer work only after previous
work drained and through final submission. Samples with/without ownership
transfers form distinct groups.128fixed groups, complete valid/overflow totals,
at most16group lines/10s. Group labels retain shader hashes and guest state;
copies label raw copy controls. High completion time with small CPU preparation
is a lead for GPU/driver execution, not proof of a particular shader cost.
Check pipeline compilation and new shader appearances before interpretation.

Validation: deterministic production tests cover disabled/warmup/spacing,
both drains, excluded prior work, cancellation/nested scopes/TLS, unsafe and
failed admission/completion, persistent disable,256-attempt limit,128-group
overflow and16-line reports. Full normal regressions and ARM64 package pass.
Bounded design/final reviews find no blocker. SDK public-header diff is empty;
ELF includes the recorder and no rejected timestamp instrumentation.

Installed NRO70,338,757bytes, SHA256
`d6752393104241d1b7b03d078f0e1634413b026fec5e49511c0861edefb876b5`.
Parsed TOML differs from original FBO only by `lb2_fence_operation_samples=true`.
Stencil copy, no-ZCULL and point-elision experiments are off.
Prior025 NRO/TOML saved on SD as
`legobatman2-before-fence-samples.nro.bak` and
`legobatman2-before-fence-samples.toml.bak`.
All main/backup bytes read back and verified; no confirmation windows.

Build: work/analysis/fence-operation-samples/build-verified.log.
Delivery: work/analysis/fence-operation-samples/delivery/verification.json.
Implementation plan: docs/superpowers/plans/2026-11-15-fence-operation-samples.md.

## Hardware026

256attempts reached the hard limit,223valid and33discarded, no wait failure
or device loss. Interval reports printed182of223samples (top16groups per
window); unsampled/unprinted operations cannot be treated as free. The pending
attempt across the first report explains interval count differences.

Printed no-transfer guest draws:140samples,0.063–1.832ms complete latency,
often0.1–0.2ms including submission/wait overhead. Some slow cases are almost
entirely CPU preparation. Printed copy groups:33samples,0.218–2.925ms.
Printed draws including ownership:9samples,1.814–17.700ms. The17.7ms case
occurs while pipeline creation/preparation is active; it is not established
as steady GPU cost. Repeated clear VS1E6883FCCDE1F688+ownership groups are
around3.8–4.7ms; one draw pair with transfers is6.05ms vs0.355ms without.
This identifies transfer/preparation and resolve work as a concrete lead,
without additive frame-cost or dominant-shader assertions.

Evidence: work/analysis/fence-operation-samples/hardware/{legobatman2_026.log,samples.json}
and nested rex files. Old appended crash log is unchanged, no new device loss
in current stderr. Restored original278-byte FBO config after collection;
diagnostic NRO remainsd6752393... with all experiments defaultfalse. Proof:
work/analysis/fence-operation-samples/disabled-readback/verification.json.

Code audit: direct_host_resolve is alreadytrue by default but its Vulkan
implementation is only preflight, then calls ordinary DumpRenderTargets.
Resolve subsequently executes its EDRAM-buffer→guest-buffer compute pass.
It does not currently fuse source-image sampling and final guest texture
tiling. This is a candidate for a new bounded implementation, not an already
available setting to enable.

---

<a id="section-research-switch-direct-color-resolve-md"></a>

# Direct RGBA8 resolve trial — 2026-11-16

Status: installed, hardware027 pending. Not an established FPS fix.

Run026 found copy completion latency commonly1–3ms and several ownership-
bearing draws slower than ordinary draws. Auditing direct_host_resolve found
a preflight-only stub: it still dumps source images into EDRAM, then the
ordinary resolve shader reads that buffer and writes the tiled guest texture.
The old direct-resolve success counter does not mean either GPU pass vanished.

The new opt-in path directly samples the authoritative source image and writes
the existing guest destination. Eligibility is fail-closed: FBO/scale1, plain
RGBA8 source/destination,1x samples, no exponent bias, sample0,2D destination,
supported32bit endian modes, complete same-owner tile span, matching pitches,
valid source/image/buffer bounds and no EDRAM wrap. Partial/mixed/unowned ranges
and all other modes retain the original dump/resolve path. No image usage or
ZCULL eligibility changes. Destination residency, synchronization, texture
invalidation, written-range outputs and post-copy clears remain common code.

Kernel matches SDK UNORM packing (component*255+0.5 then truncate), channel
swap followed by endian conversion and exact Xenos2D tiled byte addresses.
The existing destination descriptor is reused, so the shader base is relative
to the already-adjusted copy_dest_base. Shader resource lifetime follows the
existing idle render-cache shutdown. It does not add a query, CPU wait or
change queue policy.

Skipping the intermediate EDRAM snapshot is valid for the pinned FBO owner
model: ownership transfers sample host images, fallback resolves dump fresh,
host-depth scratch writes precede their own reads, and remaining EDRAM readers
are FSI-only. Owners persist across BeginFrame/ClearCache; the renderer path
is fixed at initialization. The fused path leaves EDRAM metadata unchanged.

Verification:73751CPU guard/address checks,96actual production-GPU cases/
301,989,888bytes and56actual SDK-reference cases/176,160,768bytes, including
output canaries, all4endian×2channel-swap modes,848x480, tile boundaries,
nonaligned bounds/pitches and adjusted EDRAM source bases. RTX5060Ti/
Vulkan1.4.341; production SPIR-V passes Vulkan1.2 validator. These validate
bytes, not Switch runtime transitions/FPS. Bounded integration review found
no Critical/Important issue; full normal ARM64 regressions/package pass.

Evidence: work/analysis/direct-color-gpu/results.txt,
work/analysis/direct-color-resolve/build.log and
docs/superpowers/plans/2026-11-16-direct-color-resolve.md.

Installed NRO70,351,045bytes, SHA256
`af4a98374e629b2ca4b55dbed2a37f0741ff3999b1902686a0c8bb7db9f4df5e`.
Full NRO/config/backup readback verified against build/package/original bytes:
work/analysis/direct-color-resolve/delivery/verification.json.
Only runtime config change is `lb2_direct_color_resolve=true`. Prior stencil,
no-ZCULL, point-elision and fence/timestamp experiments are disabled.
Rollback files on SD: `legobatman2-before-direct-color.nro.bak` and
`legobatman2-before-direct-color.toml.bak` (original278-byte FBO config).

Next: confirm nonzero direct dispatch count and eligible frequency, compare
same opening intervals/CPU prep/native recovery/FPS and new visual defects.
Zero eligibility is not a valid performance test. Do not infer a gain from
fewer passes alone; preserve testing-deferred old depth-like artifact issue.

---

<a id="section-research-external-switch-performance-md"></a>

# Аналоги проблемы производительности Switch: исследование для Sol

Дата: 15 ноября 2026. Исследование источников, не патч и не подтверждение исправления LB2.

## Вывод

**Да: наиболее близкий опубликованный случай — NFSMW-NX.** Первая версия с GPU backend SDK, переводом команд Xenos и эмуляцией EDRAM работала в гонке на **1–3 FPS**. Авторы заменили этот путь на специализированный native renderer и затем оптимизировали GPU и CPU; итоговые цифры порта — 32–35 FPS в гонках. Но авторы характеризуют исходное ограничение как стоимость эмуляции draw на CPU; это не доказательство, что причина нашего ожидания очереди та же. Источник: [история производительности NFSMW-NX](https://github.com/StevensND/nfsmw-nx/blob/main/docs/performance-history.md).

**Готового подтверждённого исправления именно для LB2, при котором очередь NVK перестаёт заполняться и сохраняются все эффекты memexport, не найдено.** Full queue / время в vkQueueSubmit показывают backpressure, но не устанавливают, является ли источник лишней работой GPU, драйвером или синхронизацией. Не следует считать размеры очереди причиной только потому, что ожидание наблюдается там.

На геймплее наблюдалось около 3 FPS, 4145 draws и 613 transfer draws на кадр, а также ожидание внутри `vkQueueSubmit`. Экспериментальный FSI вызвал продолжительную компиляцию, зависания и искажения звука; исходные NRO и FBO-конфигурация были восстановлены.

## 1. NFSMW-NX — самый близкий аналог

[История порта](https://github.com/StevensND/nfsmw-nx/blob/main/docs/performance-history.md): после 1–3 FPS на GPU backend SDK специализированный обработчик PM4 с Vulkan достиг 16,1 FPS медианы на штатных частотах. Первоначально GPU был занят примерно 60 мс/кадр: сцена 27,2, тени 13,6, копии 7,4, cubemap/blur 7,0 и постобработка 3,9 мс. Авторы снизили работу GPU примерно до 33 мс, а затем оптимизировали CPU и pacing. Это отдельная игра, отдельный renderer и другая нагрузка; цифры не прогноз для LB2.

Применимые направления для исследования, **не готовые безопасные патчи**:

- Xbox 360 tiling: NFSMW отказался от повторного рисования сцены по тайлам там, где это корректно, и рисовал её за один проход. Проверить в трассе LB2, сколько гостевых draw повторяются из-за tiling, включая predication, EDRAM и side effects. Нельзя просто выбросить дубликаты: memexport и порядок могут иметь наблюдаемый результат.
- Передача владения совместимым render target вместо реальной копии resolve дала NFSMW 2,36 мс GPU; нужен анализ алиасов, размера, формата, будущих записей, чтений и синхронизации, прежде чем переносить идею.
- Снижение стоимости шейдеров: объединение предикатных блоков и удаление лишних операций. Сравнивать не размер SPIR-V, а итоговый native shader, давление на регистры и время; холодная компиляция и steady state — разные вопросы.
- Dynamic UBO и устранение дублирующихся загрузок — порт применял их. В LB2 dynamic UBO уже изучены в `switch-dynamic-constants.md`; сначала проверить фактическую сборку и режим, не считать идею новой.
- В документе есть также удаление растительности из теней — это изменение качества и содержимого, а не безусловно корректная оптимизация. Не применять без отдельного решения.

## 2. LEGO Batman 1 — близкий движок и deko3d, но важное ограничение

[Заметки авторов LB1](https://github.com/surblazer/legobatman-recomp/blob/main/PORTING_NOTES.md): автоматический масштаб даёт около 30 FPS в обычной игре, тяжёлые сцены с более 2000 draw и трёхполосные заставки — 16–20 FPS. В их измерении GPU — ограничение в игре; при нативном разрешении около 33 мс GPU, из них около 4 мс command overhead и 5 мс resolves. Это пример того, что родственная TT-игра может иметь тяжёлые cutscene и GPU-нагрузку, но не описание той же NVK-очереди.

Критическое отличие: авторы прямо указывают **46 vertex shaders с memexport не поддержаны**, отсутствующий shader ничего не рисует. Наши локальные `switch-guest-probe.md` и `switch-memexport-format.md` фиксируют зависимость LB2 от memexport. Поэтому переход на renderer LB1 без реализации и проверки записей в память не сохраняет корректность. Заметки LB1 также сообщают, что попытка объединить трёхполосную раскладку в один проход не дала безопасного универсального решения; читать раздел predicated tiling перед экспериментом. Их упрощённая математика вызывала мерцание и была отвергнута.

## 3. Sonic Adventure 2 и другие порты

[SA2 README](https://github.com/ChanseyIsTheBest/sa2-nx) сообщает, что после перехода на Renut/deko3d при City Escape и примерно 1400 draw/frame время игрового потока уменьшилось с 19,2 до 15,5 мс, потока renderer — с 17,6 до 11,1 мс; GPU 7–9 мс при 307 МГц. Это CPU-ориентированное сравнение в другой игре, не доказательство, что замена NVK сама устранит GPU backpressure LB2.

[История renderer SA2](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/renut-renderer.md) документирует отслеживание resolve: две последовательные копии одного цветового target заменены условной передачей владения изображением, с fallback для последующих конфликтующих операций. Также отложено создание mip до фактического чтения и удалены повторные clear только при отслеживании записей и поколений ресурса. Авторы проверяли совпадение кадров на ПК. Это хороший образец для точечных гипотез, но семантика LB2, CPU readback, alias и memexport требуют собственной проверки.

[Руководство SA2 по deko3d](https://github.com/ChanseyIsTheBest/sa2-nx/blob/main/docs/deko3d-renderer-guide.md) рекомендует сначала запустить новый renderer на ПК рядом с эталоном, сопоставлять кадры и отдельно реализовать fence, callbacks и pacing. Устройство D3D и адреса зависят от игры. Это большой архитектурный путь, а не переключатель backend. Дополнительный пример переноса: [USF4 renderer notes](https://github.com/Thorhax/usf4-nx/blob/main/docs/deko3d-renderer-usf4.md).

## 4. Mesa / NVK / deko3d

[Mesa NVK documentation](https://docs.mesa3d.org/drivers/nvk.html) описывает NVK как Vulkan-драйвер NVIDIA; общая документация не является обещанием производительности конкретного Horizon-форка. [deko3d](https://github.com/devkitPro/deko3d) — низкоуровневый API Switch, а не совместимая замена Vulkan: нужен отдельный backend, shader translation и корректное управление ресурсами и синхронизацией.

[NFSMW Mesa notes](https://github.com/FermataRest/nfsmw-nx/blob/main/docs/mesa.md) описывают конкретные исправления mesa-switch на базе `1a8c1a66d6f`, той же базовой ревизии, что указана для LB2. В частности, ZCULL первоначально не работал из-за отсутствия размеров, привязки channel и несовместимого использования depth image; после исправлений сцена стала дешевле примерно на 2 мс GPU. Неинициализированный ZCULL может повесить GPU. LB2 уже использует патч NFSMW согласно `switch-game-build.md`: проверить фактический исходник/сборку и совместимость depth usage, а не накладывать патч повторно и не предполагать, что ZCULL активен для всех изображений.

Там же описаны изменения NAK scheduling, ветвлений и устранение ошибки clamp, кэши compiled shaders требуют версионирования; FP16 на SM50 через NAK недоступен так, как на новых GPU. Эти сведения важны для анализа получившихся шейдеров, но обновление драйвера или принудительное упрощение сами по себе не доказали ускорение LB2. NFSMW отмечает, что некоторые draw-оптимизации нужно включать настройками renderer; нельзя считать их автоматически активными. Их EDS-эксперимент оказался медленнее.

## 5. Что проверить в первую очередь

1. Сравнить один и тот же участок заставки на стабильном FBO: тёплый и холодный запуск, фактические настройки и частоты, распределение кадров, число и тип draw/transfer, очередь, время компиляции. Измерения временных меток GPU в LB2 ранее вызвали device lost (`switch-gpu-timestamps.md`), поэтому не включать их без анализа причины и безопасной проверки.
2. Инструментировать происхождение transfer draws и группировать по причинам, размеру и формату; отдельно установить долю stencil/EDRAM ownership и повторов тайлинга. В частности проверить возможность stencil export по возможностям именно установленного драйвера. Это гипотеза, а не установленная причина.
3. Для самых частых операций строить точечные, обратимые A/B с идентичным результатом: один resolve или гарантированно избыточный clear, сохраняя alias, barriers, порядок и memexport. Сопоставлять кадры и guest memory с контрольным путём; не использовать известный некорректный skip-work режим (`switch-480p.md`).
4. Проверить, активны ли оптимизации Mesa/NFSMW, включая ZCULL и dynamic UBO, в реальном runtime; выяснить, какие image usage и режим render target делают их неприменимыми. Проверить фактические shader stats и spills там, где их можно получить безопасно.
5. Если профилирование покажет, что общий Xenos/EDRAM путь генерирует фундаментально избыточную работу, рассмотреть отдельный native renderer по NFSMW/SA2 как долгосрочный проект с поэтапным сравнением на ПК и сохранением memexport. Ни один найденный источник не демонстрирует готовый перенос LB1/SA2 renderer на LB2 с этой функциональностью.

Источники описывают результаты авторов портов, а не независимое воспроизведение в LB2. Внешние страницы `main` могут изменяться; локальный патч и версии SDK нужно сопоставлять с конкретным собранным бинарником.

---

# Архив разработки и планов



---

<a id="section-first-working-build-md"></a>

# LEGO Batman 2: план первого рабочего билда для Nintendo Switch

Дата исследования: 2026-11-13. Документ предназначен следующему исполнителю: читать целиком до изменений. Это план исследования и реализации, а не отчёт о готовом порте.

**Цель:** получить локально собираемый NRO для оригинального Switch / Lite / OLED, который запускает предоставленную сборку LEGO Batman 2, показывает меню и позволяет играть в первый сюжетный уровень с управлением, звуком и сохранением.

**Архитектура:** PowerPC → C++ через ReXGlue, затем ARM64; гостевые сервисы Xbox 360 через runtime; графика сначала через существующий обработчик команд Xenos, затем deko3d на Switch. Диагностический запуск на ПК предшествует переносу на устройство. Прямой перехват D3D — возможная последующая оптимизация, не условие первой версии.

**Стек:** Python 3.11+ для подготовки, C++23 и закреплённый ReXGlue для исполнения, Horizon-совместимый форк runtime, devkitA64/libnx/deko3d/uam. Конкретные версии компиляторов и контейнера зафиксировать после проверки совместимости; сейчас рабочего toolchain нет.


## 1. Что действительно исследовано

Исходный ISO открыт только для чтения. Проверены дерево XDVDFS, диапазоны файлов, открытые заголовки XEX и сигнатуры во всех четырёх DAT; весь ISO прочитан при SHA-256. Размер и mtime до/после хеширования совпали. Это **не проверка подлинности диска**, не полная проверка содержимого архивов и не запуск игры. Расшифровка PE, дизассемблирование, кодген, декодирование архивов и выполнение на Switch на этой стадии не производились.

### Идентичность входа

| Поле | Измеренное значение |
| --- | --- |
| ISO | `LEGO Batman 2 - DC Super Heroes (USA) (En,Fr,De,Es,It,Nl,Pt,Da,Pl,Ru).iso` |
| Размер ISO | 8 738 846 720 байт |
| SHA-256 ISO | `3ae6046129958bee81ff0e3abf403ecc42185760674a91af089b3ced4d488ed0` |
| Начало игрового раздела | `0x02080000`, расположение XGD3 |
| XDVDFS | обе сигнатуры дескриптора совпадают; root sector 1 249 323, размер 2 048 |
| Дерево | 38 файлов; 2 каталога с учётом корня; сумма размеров файлов 4 487 085 430 байт |
| Исполняемый файл на диске | `Default.xex` — регистр имени существенен при переносе на Linux |
| Размер XEX | 5 869 568 байт |
| SHA-256 XEX | `f13277b2645a24132f11cb45068de5692cae35fa57077c4801d622623421b68a` |
| Title ID / Media ID | `57520817` / `1BF17B45` |
| Version / Base version, raw | `00000002` / `00000002`; это не доказательство наличия Title Update 2 |
| Диск | 1 из 1 |
| База / entry point | `0x82000000` / `0x82B577E0` |
| Заявленный размер загружаемого образа | 16 580 608 байт; не размер RAM, нужной игре |
| Имя исходного PE в метаданных | `SuperHeroes1.master.pe` |
| XEX file format | encryption=1 (normal), compression=2 (normal/LZX); PE нельзя дизассемблировать прямо из ISO |
| TLS | 64 слота, raw address 2197114880, data size 8, raw size 8 |
| Stack size в XEX | `0x10000`; не универсальный размер стека всех гостевых потоков |

Статические библиотеки `D3D9`, `D3DX9`, `XAUDIO2`, `XAPOBA`, `XAPOFX`, `XMCORE`, `XMP`, `XGRAPHC`, `XAPILIB`, `XBOXKRNL`, `XONLINE`, `XNET`: **2.0.21119**. `LIBCMT`: 2.0.11886, `LINK`: 10.0.11886, `C1`/`C2`: 16.0.11886. Это свидетельство набора библиотек, а не гарантия единственной версии всего SDK/компилятора.

Импортируются `xam.xex` и `xboxkrnl.exe`, версии raw `0x20527F00`. Таблицы содержат соответственно **116 и 266 записей**. Не называть сумму 382 количеством уникальных API: адресные записи и thunk-записи требуется разрешить по загруженному образу.

### Ресурсы

| Архив | Размер, байт | Найденные в сыром потоке строки `LZ2K` |
| --- | ---: | ---: |
| `GAME.DAT` | 1 776 752 451 | 31 204 |
| `GAME0.DAT` | 1 233 706 964 | 68 713 |
| `GAME1.DAT` | 1 196 982 245 | 65 979 |
| `GAME2.DAT` | 173 747 667 | 10 393 |

Все четыре содержат строку `PakDat v1.01` в начале. Общий размер DAT — **4 381 189 327 байт**, но каждый файл меньше лимита FAT32: объединять их в один файл не нужно. Сумма размеров не означает, что всё нужно держать в памяти.

В `GAME.DAT` по смещению 512 обнаружен заголовок `LZ2K` с LE32 unpacked size 32768 и packed size 1804; в `GAME0.DAT` первый такой заголовок по 108032, unpacked size тоже 32768. Это сильная зацепка для адаптации инструмента TT, **не доказанная распаковка**. Комментарий экстрактора LB1 про блоки 16 KiB не превращать в константу LB2.

В `GAME.DAT` по 255834624 обнаружен согласованный префикс `RIFF … WAVE fmt `, размер fmt 52 и format tag `0x0166` — кандидат XMA2. Полный RIFF ещё не провалидирован и не декодирован. Его следует проверить первым при аудио-инвентаризации. Сырые совпадения `BIKi`, `OggS`, `DFLT` и `102A1100/102A1101` могут быть случайными байтами сжатых данных; по ним нельзя объявлять наличие Bink/Ogg, полный набор кодеков или число шейдеров.

Кроме DAT и XEX, в корне есть `NxeArt` и `__DISC__` (3 байта), каталог `$SystemUpdate`. Последний содержит системные компоненты Xbox, **не DLC и не title update игры**. Не рекомпилировать найденные там XEX как игровые модули и не запускать обновления.

Локальные доказательства в игнорируемом `work/analysis/`: `iso.json`, `files.json`, `xex.json`, `xex-extra.json`, `archive-scan.json`, `default.xex`. Одноразовые исследовательские скрипты `inspect_disc.py` и `scan_resources.py` там же; они не являются готовыми безопасными импортерами. Публичный план содержит только метаданные, не игровые байты. При отсутствии этих локальных файлов воспроизвести исследование по задаче 1.

## 2. Перспективы и выбранный маршрут

**Вывод:** основания для экспериментального порта хорошие: обычный XEX2, один игровой XEX в корне, знакомое семейство архивов TT, библиотеки Xbox 360 и существующий близкий пример LB1. Но сейчас нет доказательства, что LB2 успешно проходит даже анализ ReXGlue. Первый рабочий билд — проект по адаптации runtime и графики, а не переименование чужого порта.

| Вариант | Решение и причина |
| --- | --- |
| PM4/Xenos → deko3d по примеру LB1 | Базовый выбор для первого Switch-билда: меньше завязки на адреса D3D конкретного XDK, можно сравнивать один поток гостевых команд на ПК и консоли. Покрытие LB2 ещё проверить. |
| D3D hooks → нативные команды по примеру SA2 | Резерв и оптимизация: потенциально убирает двойную обработку, но требует доказанного ABI D3D 21119, всех используемых entrypoints, callbacks, fences и device layout. Начать отдельный прототип только если трассы докажут блокер/цену базового пути. |
| Полная замена движка или общий эмулятор на Switch | Вне первого билда: меняет масштаб задачи и не использует выбранный путь статической рекомпиляции. |

Главные неизвестные в порядке снятия: (1) загрузка и анализ XEX, (2) полнота границ функций/косвенных вызовов, (3) покрытие runtime, (4) шейдеры/EDRAM/форматы, (5) память и streaming, (6) производительность на железе. Нельзя честно назначить процент готовности, срок или обещать 30 FPS, пока эти проверки не пройдены. 30 FPS на штатных частотах — цель оптимизации; 60 FPS не входит в первый билд.

Сверенные локальные ревизии совпадают с `references.json`: LB1 `dc4e3fa6312c72fbed18155dac3d8d9d1809611a`, ReXGlue `546a082a4356173635f6c40510a41ff53e518b5a`, SA2 `baf523b8e93a1b02d8b73d13aebf86780f8cc0c6`. Чужие отчёты о FPS и исправлениях относятся только к тем проектам.

## 3. Критерии готовности и правила работы

Различать четыре состояния:

1. **P0 — анализ:** воспроизводимо извлечены файлы и загружен PE, есть карта функций/импортов; ни меню, ни NRO ещё не обещаны.
2. **P1 — ПК:** от настоящего entry point до меню, затем первый уровень; ввод/звук/файловые операции наблюдаемы. Host Vulkan может быть диагностическим backend, не обязательной частью Switch.
3. **P2 — Switch boot:** реальный NRO показывает меню на устройстве и реагирует на контроллер; это ещё не первый игровой билд.
4. **P3 — первый рабочий билд:** холодный запуск, меню → новая игра → первый доступный игровой участок первого сюжетного уровня; 10 минут управления без crash/hang, различимая корректная сцена/HUD, работающие основные звуки и речь в проверенном участке, пауза/возврат, штатное сохранение на доступной контрольной точке и загрузка после перезапуска. Три повторения маршрута, с записью версии консоли/режима запуска, хешей, логов, памяти и кадров. Если checkpoint требует пройти дальше — расширить маршрут, не имитировать save. Известные дефекты явно перечислить.

P3 не означает полное прохождение, готовый открытый город, кооператив или стабильные 30 FPS. Город, split-screen, длинные ролики и все языки — следующая матрица регрессий. Однако любой из этих механизмов, необходимый для выбранного первого уровня, становится обязательным, а не исключается ради отчёта.

Глобальные ограничения:

- Не коммитить ISO, XEX, DAT, дампы, generated C++, производные игровые шейдеры или локальные настройки. Не выполнять `git add .`/`git add -A`. Исходное дерево уже содержит локальные изменения; не сбрасывать их.
- Не менять исходный ISO, не накладывать TU и не смешивать разные диски. Выбранная исходная версия определяется XEX SHA-256, не только Title ID.
- Игровые адреса, native replacements, исключения анализатора — только с доказательством из **этой** сборки. Ни один адрес LB1/SA2 не переносить по сходству названия функции.
- Не подменять неизвестные системные вызовы безусловным успехом. Для необязательной онлайн-службы допустим корректный документированный статус offline, если проверен вызывающий код.
- Нет фиктивных шейдеров/пропуска draw как «исправления». Диагностические заглушки допустимы под явным флагом, но исключают признание затронутого маршрута корректным.
- Установка WSL/контейнера и toolchain — отдельная операция настройки системы; сначала определить доступную среду, не запускать чужой setup вслепую.

Пять особенно важных проверок: неправильный/обрезанный вход (задача 1); регистр путей и короткое чтение (6); callback/необычный ABI (4); изменение ресурса и GPU alias (8–9); нехватка памяти/сохранение после перезапуска (10–11).

## 4. Карта будущих файлов и зависимостей

Это **предлагаемые** пути, сейчас большинства нет. Не создавать пустые модули ради галочки. Имена адаптировать один раз к реальному API закреплённого SDK и обновить все ссылки в плане.

| Путь | Ответственность |
| --- | --- |
| `tools/disc.py`, `tests/test_disc.py` | ограниченное чтение XDVDFS, безопасное извлечение, manifest файлов |
| `tools/project.py`, `tests/test_project.py` | расширение существующей инспекции открытых метаданных XEX |
| `config/game-build.json` | разрешённые метаданные выбранного XEX; без абсолютных путей и игровых байтов |
| `tools/prepare-game.py` | оркестрация импорта/хешей, никаких системных установок |
| `tools/codegen.*` | единый кодген, проверки, отчёт; оболочку выбрать по среде |
| `port/legobatman2_manifest.toml`, `port/overrides.toml` | настройки конкретной игры и только обоснованные overrides |
| `CMakeLists.txt`, `cmake/`, `src/main.cpp` | отдельные host/switch targets, создание runtime и запуск entry point |
| `src/platform/`, `src/hooks/` | платформенные сервисы и минимальные игровые перехваты |
| `src/gpu/`, `tools/shaders/` | выбранный backend и локальная подготовка шейдеров |
| `tests/fixtures/` | только синтетические тестовые данные |
| `docs/research/`, `docs/validation/` | объяснения адресов, покрытия и результатов без игровых дампов |
| `game/`, `work/`, `generated/`, `build/`, `out/` | игнорируемые оригиналы, промежуточные результаты и локальные сборки |

Основной порядок: `1 → 2(host-ready) → 3 → 4 → 5 → [6 ↔ 7 ↔ 8] → 9 → 10 → 11 → 12`. Квадратные скобки — цикл диагностики: первый host-запуск → первый блокирующий ресурс/API → соответствующее исследование 7/8 → повторный запуск. Не ждать готового меню, чтобы начать разбирать мешающий ему shader, аудио или видео. P1 отмечать только после разрешения этих блокеров. Исследование ресурсов 7 можно начать уже после 1. Часть 2(switch-ready), включая проверку sample на устройстве, обязательна перед P2, но отсутствие консоли не блокирует задачи 3–7 на ПК. Сначала добиваться наблюдаемого результата, затем оптимизировать.

## 5. Задачи реализации

### Задача 1. Воспроизводимый импорт и фиксация исходной игры

**Файлы:** `tools/disc.py`, `tools/prepare-game.py`, `tests/test_disc.py`, `config/game-build.json`; результаты в `game/` и `work/analysis/`.

- [ ] Перед записью проверить свободное место на томе. Помимо 8,74 GB ISO нужны извлечённые файлы, decoded image, generated C++, SDK и объектные файлы; полный бюджет измерить после пробной сборки, не выдавать чужие «30 GB» за гарантию.
- [ ] Написать синтетические тесты: XDVDFS с ненулевой базой раздела, левым/правым дочерним узлом, вложенным каталогом; обрезанный дескриптор, цикл дерева, выход extent за файл, нулевой файл, путь `../`, дубли/коллизии имён. Запустить и убедиться, что новые проверки сначала выявляют отсутствующую реализацию.
- [ ] Реализовать `read_directory(image: Path) -> list[DiscEntry]`, где DiscEntry содержит relative_path, relative_sector, size, is_directory; вся адресная арифметика минимум 64-bit. Absolute offset = partition_base + sector × 2048. LE у XDVDFS, BE у XEX. Проверять обе сигнатуры volume descriptor, границы таблиц, длину имени, циклы/лимиты; child offset в каталоге измеряется в 4-байтовых единицах.
- [ ] Реализовать `extract_game(image: Path, destination: Path) -> manifest`: только внутрь destination, потоковые чтения, временный файл → rename после хеширования. Не загружать ISO/DAT целиком в RAM. Проверять неизменность size/mtime и результаты чтения; не перезаписывать существующий отличный файл молча.
- [ ] Сохранить оригинальные имена `Default.xex`, `GAME*.DAT`, `NxeArt`, `__DISC__`; `$SystemUpdate` учитывать в инвентаре, но не включать в игровой runtime. Не объявлять все XEX диска зависимостями игры.
- [ ] Manifest: относительный путь, размер, SHA-256 каждого извлечённого файла, ISO hash, версия импортера. На уже извлечённом комплекте сверить хеши; имя файла ISO не критерий совместимости.
- [ ] `python -m unittest discover -s tests -v` проходит; XEX hash равен разделу 1; `git check-ignore` подтверждает исключение игры; просмотр staged списка не содержит бинарных данных. Зафиксировать только собственные исходники/метаданные, если делается коммит.

**Выход:** неизменный комплект игры с проверяемой идентичностью. Следующие задачи используют именно его.

### Задача 2. Toolchain, происхождение и лицензии зависимостей

**Файлы:** `docs/setup.md`, отдельный lock версий сборки, `patches/`, `docs/research/dependencies.md`.

- [ ] Повторить `python tools/project.py doctor`. На дату исследования CMake/Ninja/Clang/ReXGlue/devkitA64/uam/DXC/SPIRV-Cross/Docker/Podman не найдены в PATH, DEVKITPRO не задан. Наличие `wsl.exe` не доказывает установленный Linux. Проверить фактическую доступность выбранной среды до планирования команд.
- [ ] Для Windows host и Linux/Switch host описать отдельные пути сборки. Если выбирается WSL2, лучше хранить тяжёлую Linux-сборку в Linux FS, а не предполагать хорошую скорость сотен тысяч файлов на `/mnt/s`; перенос игры выполнять с хешами.
- [ ] Прочитать `third_party/legobatman-recomp/tools/setup.sh`, build scripts и patches. Выбрать согласованную пару generator/runtime из одного форка; upstream ReXGlue и Horizon-fork нельзя свободно смешивать. SDK `0.10.0` в TOML не заменяет pin commit.
- [ ] Зафиксировать commit каждого вложенного репозитория, checksum patches, compiler/linker versions, container digest и параметры сборки. Разделить host tools, host runtime и Horizon runtime; generated C++ должен соответствовать API runtime.
- [ ] Проверить лицензии конкретных копируемых файлов и транзитивных компонентов (включая FFmpeg/декодеры, renderer и shader translator). Не считать публичный GitHub разрешением на копирование; при неясных условиях использовать изучение/свою реализацию или разрешённый компонент, записать ограничение.
- [ ] **Host-ready:** собрать CLI, выполнить его help/version и минимальный host SDK sample. Сохранить команды и логи; после этой проверки разрешено продолжать задачи 3–7.
- [ ] **Switch-ready:** отдельно собрать/запустить deko3d sample NRO и проверить uam минимальным shader. Если устройства пока нет, оставить запуск непроверенным и продолжить host-работу; закрыть проверку до P2. Сборка sample доказывает только toolchain, не LB2.

**Выход:** воспроизводимая среда и exact команды, на которые могут ссылаться следующие задачи. Не угадывать CLI нового/старого SDK по статье в интернете.

### Задача 3. Загрузка PE и технический паспорт P0

**Файлы:** расширение `tools/project.py`, синтетические тесты, `docs/research/executable.md`; приватные PE/дампы в `work/`.

- [ ] Добавить разбор FILE_FORMAT_INFO `0x000003FF`, STATIC_LIBRARIES `0x000200FF`, TLS `0x00020104`, imports `0x000103FF`, original name `0x000183FF`, security image size. Проверить truncated/oversized variable blocks и неполные записи. **`0x000405FF` — Xbox logo, не static libraries.** Для library version approval byte и QFE — разные поля, не объединённая «версия».
- [ ] Через loader закреплённого SDK получить расшифрованный/распакованный образ. Проверить PE headers, machine/sections, entry point в исполняемой секции, VA↔RVA↔raw offset. XEX load image size не задаёт автоматически raw file layout. Не дизассемблировать ciphertext и не снимать «непонятные опкоды» overrides.
- [ ] Получить таблицу секций с permissions, pdata/unwind, TLS, imports/ordinals, exports, релокации при наличии. Разрешить адресные записи и thunk-пары; сопоставить с реализованными API runtime. Выделить непокрытые обязательные импорты.
- [ ] Проверить наличие других реально загружаемых игровых модулей и динамически создаваемого/распаковываемого кода во время будущей трассы: один XEX в корне не доказывает отсутствие таких механизмов.
- [ ] Сохранить паспорт: хеш входа/decoded image, версия инструмента, диапазоны, import coverage. При ошибке loader сначала проверить вход и совместимость SDK; не компенсировать её патчами игры.

**Выход P0:** валидированный загруженный образ и карта неопределённостей. Число функций и шейдеров заранее не назначено.

### Задача 4. Полный и проверяемый кодген

**Файлы:** `port/legobatman2_manifest.toml`, `port/overrides.toml`, `tools/codegen.*`, `docs/research/function-evidence.md`.

- [ ] Создать минимальный manifest по схеме **выбранного** SDK, с путём к `Default.xex`, базой/entry из паспорта, выходом в игнорируемую папку. Генерируемые data refs также хранить в игнорируемом месте, handwritten overrides отдельно.
- [ ] Первый кодген делать с консервативными настройками регистров, без direct-call rewrite, skip_msr и глобального fast-math. Все ошибки анализа сохранять, а не прятать через `--force` или лог-фильтр.
- [ ] Для unresolved branch/indirect call: проверить pdata, executable sections, ссылку из rdata/data, lis+addi/ori и jump tables, prologue/epilogue, вызывающий код. Учитывать signed low half у addi. Поиск 32-bit слова, похожего на адрес, сам по себе не доказывает function pointer.
- [ ] Не полагаться только на RTTI/vtables: близкий LB1 потребовал отдельного поиска callbacks без pdata. Не предполагать, что размер функции всегда расстояние до следующего адреса: бывают tail calls, внутренние labels и данные внутри диапазона.
- [ ] Каждый override документировать: game hash, VA, причина, откуда вызывается, краткое описание доказательства, проверка. Специально найти свои setjmp/longjmp, save/restore GPR/FPR/VMX и исключения. Адреса LB1 запрещены как значения по умолчанию.
- [ ] Добавить проверки чтения регистров до записи и непрологовых записей nonvolatile. `share_registers` включать точечно: массовое включение может разрушить ABI обычных функций. Начальная неоптимизированная версия нужна для дифференциального сравнения.
- [ ] После появления hooks генерировать список защищённых адресов из их реестра; direct-call rewrite не должен обходить weak alias/native hook. Тест: искусственный hooked call вызывает replacement и до, и после преобразования.
- [ ] Повторный чистый кодген теми же входами выдаёт эквивалентный список функций/настроек. Неисполняемые timestamps можно исключить из сравнения. Сохранить unresolved-call счётчик. Проверка clean build приложения выполняется в задаче 5 после создания launcher/CMake, а runtime crash diagnostics — после первого запуска.

**Выход:** сгенерированный гостевой код с объяснимыми исключениями и возможностью регенерации; его компиляция и линковка ещё требуют задачи 5.

### Задача 5. Runtime и первый диагностический запуск ПК

**Файлы:** `CMakeLists.txt`, `src/main.cpp`, `src/platform/`, журнал API coverage.

- [ ] Минимальный launcher: найти game root, сверить XEX hash, создать runtime/память/ядро по API SDK, зарегистрировать generated entrypoints, загрузить image/TLS, запустить настоящий entry. При несовпадении сборки отказ с понятной диагностикой до вызова кода.
- [ ] Создать host CMake target и выполнить clean configure/build/link приложения с generated units задачи 4 и совместимым runtime задачи 2. Зафиксировать команду и результат; ошибки compile/link устранять до попытки запуска. Чистая сборка не должна зависеть от оставшихся файлов чужого проекта.
- [ ] Логировать guest PC/LR, поток, last import, read/write address, last mounted path, текущую фазу загрузки. Добавить watchdog с дампом состояния, а не автоматическим убийством «медленного» первого shader compile.
- [ ] Проверить big-endian scalar loads/stores, VMX lane order, FPR/FPSCR/rounding, carry/CR/XER, atomics/reservations, TLS на нескольких потоках. Для native замен сделать синтетические проверки обычных/краевых входов; без доказательств не переписывать математические функции NEON.
- [ ] Host pointers не хранить в guest uint32; guest VA переводить через runtime. Физические и виртуальные views одной памяти должны разделять содержимое. Простая отдельная malloc-копия каждого диапазона ломает DMA, XMA и GPU.
- [ ] Проверить вызовы ожиданий, события, критические секции, interrupts/DPC и гостевые callbacks. Зависание может быть пропущенным сигналом GPU/аудио, а не «длинным циклом» игры. Не заменять wait немедленным success.
- [ ] Получить первый журнал от entry до загрузки ресурсов; каждый следующий crash чинить по первой причине. Разделять отсутствующий import, неверную функцию и плохую память — они требуют разных исправлений.

### Задача 6. Файлы, профиль, ввод и меню на ПК

**Файлы:** `src/platform/filesystem.*`, input/profile adapters в пределах SDK, `docs/validation/host.md`.

- [ ] Смонтировать guest game device на извлечённый комплект; пути guest отделить от host. Построить case-insensitive lookup с обнаружением коллизий; на Linux `Default.xex` и `default.xex` различаются. Не делать массовое переименование архивов.
- [ ] Проверить синхронное/асинхронное чтение, seek выше 2 GB в общих интерфейсах, короткое чтение/EOF, alignment и порядок completion. Тест на искусственном sparse-файле большого размера не требует включать игровые архивы в CI.
- [ ] Учитывать `__DISC__`, NxeArt и запросы media/disc status по фактическим обращениям. Не удалять их как «мусор» до трассы. `$SystemUpdate` не монтировать как обновление игры.
- [ ] Один локальный профиль и один контроллер; правильные состояния sign-in/storage enumeration и cancel. Отключённая сеть должна давать ожидаемые offline-коды, без бесконечного ожидания Xbox Live.
- [ ] Проверить sticks/deadzones/triggers, edge-triggered buttons, disconnect/reconnect. Повторяющийся удерживаемый Start не должен перескакивать несколько экранов.
- [ ] Достичь P1 меню, затем запуска уровня с host renderer. Сделать эталонные кадры и записать маршрут/ввод. Где доступно, сверить с настоящей Xbox 360 или независимым эмулятором: ПК с нашим же runtime не является независимой истиной.

### Задача 7. Архивы, аудио и шейдерный инвентарь

**Файлы:** `tools/shaders/`, тесты распаковки, `docs/research/assets.md`; извлечённое только в `work/`.

- [ ] Разобрать таблицу PakDat, границы логических файлов и кодеки по проверенным записям. Для запуска сначала оставлять DAT оригинальными: гостевой TT-код может читать их сам, полная распаковка всех ресурсов на SD не обязательна.
- [ ] Испытать LZ2K decoder на небольших ограниченных блоках этой версии и синтетических invalid/truncated streams. Проверить выходной размер, доступ за границы, лимиты памяти и прекращение при EOF. В LB1 `bs_get` возвращает нули после конца: переносить такое поведение как успешное чтение опасно.
- [ ] Не склеивать соседние `LZ2K` без доказательства принадлежности одному файлу: это может соединить разные shader containers. И наоборот, shader может пересекать границу блока; проверять декодирование целого логического ресурса.
- [ ] Инвентаризировать все четыре DAT, не только GAME.DAT: назначение частей/языков ещё неизвестно. Определить container version XDK 21119. LB1 scanner жёстко ищет `10 2A 11 00/01` (формат 2008); нулевой результат у LB2 означает возможную несовместимость scanner, а не отсутствие шейдеров.
- [ ] Собрать shader corpus: static extraction плюс runtime dumps на маршруте меню/уровень. Для каждого stage/hash записать provenance, размер, переводимые инструкции, memexport, vertex texture fetch, relative constants, fetch layout, bool/loop constants. Runtime может менять vfetch, поэтому код на диске и bound ucode различаются.
- [ ] Разобрать кандидат RIFF/XMA2 из раздела 1, перечислить реальные аудиоформаты и путь подачи samples/callbacks; проверить покрытие XMA decoder выбранного Horizon-runtime. Наличие XAUDIO2 в XEX не означает наличие host XAudio2 на Switch.
- [ ] Видео определять по распакованным ресурсам и вызовам decoder. Пропуск ролика разрешён как временная диагностика лишь с корректным событием завершения; не засчитывать его как нормальную поддержку первого маршрута.

**Выход:** таблица observed/translated/unsupported/untested, а не «все шейдеры найдены» по сканированию сигнатур.

### Задача 8. Выбор и проверка графического пути

**Файлы:** `src/gpu/`, `docs/research/graphics-coverage.md`; сначала host validation выбранного пути.

- [ ] Зафиксировать трассу используемых PM4 packets, texture/RT/depth formats, MSAA, resolves, predication/tiling, primitive types, shader instructions и sync/callbacks. Выбранный PM4 backend должен покрыть именно этот список.
- [ ] Если нужна смена на D3D hooks, сначала отдельный proof: опознать функции XDK 21119, аргументы/устройство, ring/fence semantics и показать небольшой корректный кадр. Не начинать одновременно два полных renderer и не смешивать их владельцев ресурсов без явного интерфейса.
- [ ] Написать синтетические сцены: textured quad, depth/stencil, alpha blend, DXT маленьких mip, cube, resolve → sample, predicated tiles, изменяемый vertex/index buffer. Где путь не используется LB2, пометить тест расширением, а не блокером первого билда.
- [ ] Сначала базовое разрешение и точное поведение. Проверить endian vertex/index data, swizzle/tiling, pitch/mips, vertex fetch, scissor/viewport, front-face и глубину. У deko3d и Xenos enum compare не обязан совпадать численно.
- [ ] Не переносить LB1 single-pass tiles: его two-tile оптимизация ломала three-strip post passes даже в той же игре. Для LB2 сначала воспроизвести все плитки/resolves и подтвердить их dependencies.
- [ ] Сравнивать промежуточные RT, depth/stencil и конечный кадр; «есть картинка» не исключает пропущенные тени, прозрачность или половину HUD. Golden кадры делать при одинаковой сцене/камере/времени, tolerances оговаривать из-за анимации и FP.

### Задача 9. Компиляция и поиск шейдеров для deko3d

- [ ] Согласовать формат ключа extractor → translator → packer → runtime. Сверить endian, длину и stage на одном известном shader. В LB1 код использует начальное значение hash **1469598103934665603**, хотя называет это FNV-1a 64; стандартное offset basis **14695981039346656037** отличается. Нельзя заменить его «правильной библиотекой» в одной половине pipeline. Либо сохранить совместимый контракт, либо мигрировать все стороны и version cache одновременно.
- [ ] Ключ compiled cache должен дополнительно учитывать shader translator/uam versions, параметры точности, fetch specialization, backend и game build. Коллизии хеша проверять размером/байтами или вторичным digest. Pipeline key включает render state, не только shader pair.
- [ ] Проверить Xenos FP: нули, NaN/Inf, reciprocals, насыщение, relative constants, signed comparisons. Пример LB1 показал, что llvmpipe может скрыть ошибку, проявляющуюся на Maxwell; host visual pass не заменяет железо.
- [ ] Собрать uam с обоснованным набором scheduling fixes; не считать любой установленный uam эквивалентным patched версии. Сохранить тестовый shader и снимок на Switch для воспроизводимости.
- [ ] Для missing shader писать stage/hash/guest PC/draw context и локальный dump. На первом маршруте требовать отсутствие непокрытых обязательных shader paths. Memexport, если он реально используется, — функциональный блокер, а не опция для удаления.
- [ ] Game-derived DKSH хранить во внешнем локальном пакете либо локально сгенерированном romfs. Не коммитить и не автоматически публиковать NRO с вложенными игровыми данными. Для первого билда достаточно локального результата; модель публичного распространения решается отдельно.

### Задача 10. Horizon runtime, память и запуск на Switch

**Файлы:** target CMake, `src/platform/switch/`, конфигурация приложения и бюджеты памяти.

- [ ] Настроить отдельный target и out dir; убедиться, что ARM64 links не подтягивают host библиотеки. Проверить FP flags на **generated code и launcher**, не только SDK; исходно `-ffp-contract=off`, без глобального fast-math, необходимые aliasing flags по runtime. Не запрещать явно моделируемую гостевую FMA через случайный native rewrite.
- [ ] Запускать в режиме приложения с доступным для порта объёмом памяти; applet-mode отдельно распознавать/документировать. В отчёте фиксировать фактически доступный heap и режим, не считать все физические 4 GB Switch свободными для игры.
- [ ] Описать guest address map и физические aliases, reservation/commit policy, CPU/GPU/XMA coherence. Не коммитить весь 32-bit VA как физическую память. Page-fault handler и signal/exception paths не должны выделять память рекурсивно или брать небезопасные locks.
- [ ] Снять baseline RSS/heap и отдельно guest committed, generated code, runtime, shader/pipeline cache, textures, RT, upload rings, audio и staging. Назначить лимиты после измерений; оставлять резерв под scene transition, когда старые и новые ресурсы сосуществуют.
- [ ] Реализовать bounded caches и освобождение по GPU fences. Не переписывать upload buffer, пока GPU его читает. GPU timestamps/readback требуют отдельной cache/coherency политики; «всё сделать cached» может повесить ожидания.
- [ ] CPU/GPU записи в один ресурс и resolve aliases должны инвалидировать правильный cache. Sampling hash может пропустить изменение между sampled words: сначала full validation/dirty tracking, затем измеренная оптимизация. В LB1 page watches на Horizon имели проблемы с фрагментацией; не считать их безусловно бесплатными.
- [ ] Ввод libnx, звук с непрерывными callbacks, файловая система SD и bounded async logging. Логи не должны блокировать GPU/audio; crash ring buffer сохранять даже при выключенном verbose. Sleep/wake, выход и повторный запуск протестировать без dangling worker threads.
- [ ] Пройти P2 на реальном Switch: menus, input, корректные кадры, рабочая диагностика. Эмулятор консоли и host OpenGL shim — дополнительные средства, не замена этой проверки.

### Задача 11. Первый игровой маршрут и сохранения P3

**Файлы:** `docs/validation/first-build.md`, regression route/input, минимальный README установки.

- [ ] Зафиксировать название первого уровня из настоящего меню, точный маршрут до checkpoint, используемый язык и контроллер. Не выдавать название/расположение уровня по памяти за проверенное.
- [ ] Довести 10-минутный участок до критериев P3: управление, бой/взаимодействие, камера, переходы, HUD, речь/звук, pause/resume. Проверить соотношение реального времени и игровой логики при низком FPS; убрать speedup/slowdown из-за неверного таймера.
- [ ] Сохранения хранить отдельно от read-only game data. Проверить успешную запись, загрузку после завершения процесса, повреждённый/отсутствующий save, отказ записи/недостаток места и отсутствие профиля. Atomic rename только если он совместим с гостевой последовательностью операций; не менять транзакционную семантику вслепую.
- [ ] Выполнить 3 прогона, включая холодный и прогретый cache, переход сцены туда/обратно; память после стабилизации не должна монотонно расти от повторения одного маршрута. Отдельно записать maximum committed и OOM diagnostics.
- [ ] Отчёт: exact game hash, source commits/patches, toolchain, build flags, устройство/режим/частоты, маршрут, известные дефекты, кадры и метрики. Если физической консоли нет, оставить P2/P3 непроверенными и передать NRO с инструкцией; не объявлять задачу окончательно выполненной.

### Задача 12. Оптимизация после корректного маршрута

- [ ] Измерить CPU game/render, GPU, I/O/decompression, audio, stalls на fences/locks, draw/resolve/upload counts и peak memory. Present cadence и game tick считать отдельно. Время кадра — не простая сумма CPU+GPU, которые перекрываются.
- [ ] Сравнить одни и те же сцены/ввод на штатных частотах; handheld и docked отдельно. Сохранять p50/p95/p99 frame time и hitch counts, а не только средний FPS. 30 FPS соответствует бюджету около 33,3 ms, но среднее ниже него не означает стабильность.
- [ ] При I/O/decompression bottleneck: bounded read-ahead по наблюдаемым запросам, объединение совместимых чтений, ограничение concurrent decompression; сохранять порядок callbacks и backpressure. Не распаковывать все DAT в RAM и не удалять языковые ресурсы без карты зависимостей.
- [ ] При CPU bottleneck: убрать повторные state updates, lookup/cache misses и лишние копии; затем проверенный direct-call rewrite, LTO, PGO на нескольких сценах. Guest atomics и MSR не удалять, пока не доказана семантическая эквивалентность runtime.
- [ ] При GPU bottleneck: уменьшить redundant resolves/barriers по dependency graph, проверить HW compression RT, затем render scale. UI, depth-dependent эффекты, pixel coordinates, post passes и pool reset при смене масштаба проверяются отдельно.
- [ ] Spin waits оптимизировать только после проверки producer/consumer и memory ordering. Чужие `sleep 100 µs`, размеры пулов и dynamic resolution thresholds не являются настройками LB2. Слишком частое sleep увеличивает latency/streaming stalls.
- [ ] NEON replacements и более слабые FP режимы только для профилированных функций: эталонные входы/выходы, NaN/Inf/denormals, alignment, overlap, aliasing, игровая регрессия, переключатель возврата.
- [ ] Каждая оптимизация: одна переменная, baseline/после, повторные прогоны, оценка шума и корректности. Удалить улучшение, которое ускоряет меню и ломает загрузку уровня. Не откладывать исправление пропущенного draw ради FPS.

## 6. Как продолжать, если результат отличается от ожиданий

| Симптом | Следующая проверка, до изменения игры |
| --- | --- |
| ISO выглядит пустым | База раздела XGD3, а не только sector 32 от начала всего файла |
| XEX не разбирается / мусорные инструкции | Хеш, ключ optional header, загрузка/дешифровка/LZX, VA/RVA/raw mapping |
| Незарегистрированный вызов | Callback/vtable/data ref, tail call, ошибочная граница, альтернативный модуль |
| Crash после register-local оптимизации | Nonvolatile writes, VMX inputs, share_registers и native ABI |
| Чёрный кадр при живом меню | Missing shaders, depth/stencil enum, RT/resolve, fences; не только projection matrix |
| Пропала речь / завис ролик | XMA completion, buffer lifetime, end-of-stream event, путь codec, частоты samples |
| На ПК работает, на Switch нет | ARM64 FP/aliasing, memory map, uam scheduling, GPU cache/coherence, case sensitivity |
| Город/переход зависает | Peak memory overlap, async I/O completion, dirty texture cache, producer fences |
| После codegen пропало исправление | Hook обойдён direct-call rewrite; postprocess не входит в единый скрипт |

Если гипотеза не подтвердилась, сохранять наблюдение и менять гипотезу; не складывать десятки необъяснённых патчей. После каждого этапа записывать: проверено, не проверено, точная команда воспроизведения, первый оставшийся blocker, следующий эксперимент. Новая версия SDK/game hash требует повторной проверки зависимых адресов, generated code и cache.

## 7. Источники и границы переноса знаний

Первичный источник фактов об игре — локальные байты и результаты раздела 1. Открытые реализации использовались для интерпретации форматов и выбора экспериментов:

- [XboxDev extract-xiso](https://github.com/XboxDev/extract-xiso/blob/master/extract-xiso.c): база раздела XGD3 и формат XDVDFS. Не требуется копировать весь инструмент ради импортера.
- [Закреплённый XEX2 layout ReXGlue](https://github.com/rexglue/rexglue-sdk/blob/546a082a4356173635f6c40510a41ff53e518b5a/include/rex/system/util/xex2_info.h): поля контейнера, static libraries, TLS, imports. Локально тот же путь в `third_party/rexglue-sdk/`.
- [ReXGlue codegen overview](https://github.com/rexglue/rexglue-sdk/wiki/Codegen-Pipeline-Overview): анализ и генерация C++ — разные стадии; команды проверять по pinned CLI.
- [LB1 porting notes](https://github.com/surblazer/legobatman-recomp/blob/dc4e3fa6312c72fbed18155dac3d8d9d1809611a/PORTING_NOTES.md): источник списка рисков ABI, памяти, deko3d и FP. Измерения LB1 не являются измерениями LB2.
- Локально `third_party/legobatman-recomp/tools/shader-extract/tt_shader_extract.c`, `tools/scripts/xenos2glsl.py`, `tools/switch-env/stage-shaders.sh`, `legobatman/codegen.sh`: читать реализацию, а не доверять только комментариям. В частности hash constant, container magic, grouping LZ2K и список исключений специфичны.
- [SA2 porting guide](https://github.com/ChanseyIsTheBest/sa2-nx/blob/baf523b8e93a1b02d8b73d13aebf86780f8cc0c6/docs/porting-guide.md): альтернативный подход и метод сравнения; offsets и native routines не переносить автоматически.

## 8. Короткое поручение следующей модели

Начни с задачи 1, не с renderer и не с копирования overrides LB1. Проверь локальный XEX hash и доступность файлов `work/analysis/`, прочитай паспорта и pinned SDK. Выполни задачи до первого доказанного blocker; для него сформулируй минимальный эксперимент. Не угадывай неизвестные адреса, API и форматы. Не называй sample NRO или меню готовой игрой. Сохраняй проверяемый базовый путь прежде, чем оптимизировать. Игру и производные данные держи вне Git. Этот документ — рабочая карта с критериями проверки, а не обещание, что все неизвестные уже решены.

---
