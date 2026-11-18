# Stencil-only cutout400×240 с сохранением depth и границы

2026-11-17. После проверки других направлений Astra:
[046](switch-rt960-coarse-profile.md#аппаратный-результат-run046) полностью записал
64 выбранных CB, RT960 серии заняли173,697мс против1341,133мс остальных интервалов.
Это не доля времени всех кадров. Пул memexport по CPU замерам имеет малый
потенциал (allocation+release2,33мс/с). Следующий ограниченный эксперимент
устраняет импорт внутри доказанной области stencil-перезаписи.

## Основание

Полные причинные trace180/280 показывают4 и10 точных случаев соответственно:
восстанавливается440×240 stencil, затем гостевой draw безусловно заменяет
его левую область400×240. Это операции562/1160/1618/1708 в180 и
852/989/993/1918/1922/1926/1930/1934/2046/2186 в280. Ref=0, кроме1708/2186 сref=1.

Все имеют VS `1E6883FCCDE1F688` с проверкой всех27 dwords программы, PS=null,
DepthOnly5, normalized depth/stencil8701 (depth не записывается, stencil REPLACE),
полную write mask и один transfer `00205EB6 → 00305EB6`, [1718,2048) tiles.
Фактические интерпретированные позиции: (0,0,0,1), (400,0,0,1), (400,240,0,1).
Чужие state, queries, memexport, raster/primitive modes, размеры и masks
отклоняются. Повторного запуска VS interpreter для проверки нет.

## Что меняется

Новый init-only `lb2_stencil_clear_cutout=false` по умолчанию. При включении
допускаются только указанная программа/state/геометрия, key00305EB6, height240,
scale1, usedRT1, единственный source00205EB6 с точным диапазоном и без
host-depth source/других accumulated RT. Также требуется отсутствие
`EXT_shader_stencil_export`, чтобы depth и stencil переносились отдельными путями.

В нормальном пути сохраняются полный j0 depth import и исходная native
stencil zero-clear440×240. Только восемь stencil-bit meshes используют существующий
SDK GetRectangles с cutout400×240: остаётся правая полоса40×240. Исходный игровой
draw сохраняется и выполняет перезапись. Ownership model, MSAA, shaders,
адресация EDRAM и memexport не меняются.

Если настоящий гостевой draw не записан (placeholder, ошибка setup, пропуск),
Scope повторяет **полный исходный stencil import**, включая full nativezero,
с выключенным cutout. j0 depth и host-depth store при этом не повторяются.
Список исходных transfers и источник сохраняются до выхода из IssueDraw;
commit требует реального pipeline. Полный480×480 depth/stencil control044
сохраняет свои прежние guards, counters и fallback.

На кандидат площадь восьми stencil passes уменьшается с844800 до76800
pixel-passes, на90,9%. В trace180/280 исключаются3072000/7680000 pixel-passes.
**Восемь draw остаются восемью:** это не прогноз GPU времени или FPS. Проверка
покажет, насколько уменьшение площади помогает при прежнем числе команд.

## Проверки

Production proof проверяется на flag OFF, изменённые guard bits, PS/query/
memexport, неточную геометрию, NaN, неверные Z/W, другие RT/height/scale,
разделение full-depth и stencil-only видов, commit и fallback.
Важен также межкомпонентный контракт: оба вида требуют PS=null, а реальный
SDK async predicate допускает discard-placeholder только при ненулевом PS.
Отдельный тест извлекает и исполняет этот production predicate; PS-null clear
не может получить такой placeholder. Ошибка синхронной компиляции приводит
к выходу до гостевого draw и scoped replay. Сам mutable is_placeholder flag
недостаточен; гарантия этого узкого пути опирается на обязательный PS=null
и проверенное правило его компиляции.

Oracle использует фактические SDK GetRangeRectangles/AddRectangle. Помимо259560
старых partitions проверяется400×240 cutout внутри440×240. Независимое сравнение
с полным импортом проверяет depth и stencil на всех четырёх samples при ref0/1,
сохранение правой границы и полный stencil fallback.

Повтор фактических trace states production-кодом даёт:

| Кадр | Прежние полные clears044 | Stencil-only кандидаты |
| --- | ---: | ---: |
| 180 | 5 /72 исключаемых transfer draws | 4 |
| 280 | 6 /81 исключаемый transfer draw | 10 |

SDK дополнительно проверяет actual transfer list/rectangle и extensions;
эти числа не гарантируют столько же аппаратных commits. Материалы:
`work/analysis/stencil-clear-cutout/`; app `depth_clear.cpp`/`lb2_depth_clear.h`;
инкрементальный SDK patch `port/rexglue-horizon-stencil-clear-cutout.patch`.

## Аппаратная проверка

Результата нового механизма на Switch пока нет. Настройки для FPS контроля
берутся из принятого044 с единственным новым renderer flag. Оба GPU query
режима выключены; размеры/качество изображения сохранены. Нужен тот же начальный
пролёт90 секунд без управления, затем закрытие игры и MTP; ожидается047.
Проверить новые eligible/deferred/committed, отсутствие replay_failed/GPU errors,
изменение stencil pixel area и FPS сопоставимых окон, затем изображение.

## Сборка и установка

ARM64 сборка и обязательные регрессии прошли (`build.log`). Actual SDK geometry/
sample oracle, trace replay4/10 и PS-null async contract прошли; проверка кода
завершена без существенных замечаний. SDK patch обратно применим к текущим
исходникам. Публичные SDK headers не изменены.

Новая пара установлена и полностью прочитана обратно с проверкой SHA256.
Предыдущая046 сохранена в `preinstall/`, принятая044 остаётся в
`work/analysis/depth-only-alpha-clear/hardware/` для точного возврата:

| Файл | Размер | SHA256 |
| --- | ---: | --- |
| NRO | 70 437 061 | `27c0a3c1ddf4b5194d583b2cbf16301279f65adfd46cda089dc3e5561b726cbe` |
| TOML | 1 261 | `ca3f70eef8b14462630d0d9fe77b230b0fcbd8b09ca592765e0552469ee3e24f` |

`preparation.json`, `build-verification.json`, `verification.json`, `readback/`
фиксируют идентичность. TOML отличается от044 ровно одним renderer flag;
GPU timestamps и phase profile выключены. Установка относится к выполненному047;
его аппаратный результат приведён ниже.

## Аппаратный результат run047

NRO/TOML после запуска совпали с установленными SHA256. Журнал047:
`5f8984353927056d47233d084572b3cab80a5f2be364632c7cea3ea454edd9ae`.
`compare.py` проверяет identity и пишет `hardware-result.json`.

В первых шести устойчивых окнах:290 кадров/60,46с =4,7966 FPS против044
290/60,52с =4,7918 FPS, **+0,099%**. Это различие слишком мало, чтобы без
повторных запусков считать выигрыш установленным. Пользователь отметил небольшой
рост FPS и подтвердил: «Изображение как раньше».

Recorded transfer pixel area/кадр уменьшилась со107626774 до102638314,
на4,635%; draw count практически не изменился, как и предполагалось.
За весь сохранённый журнал2992 deferred=committed,507 ref_one_committed,
replay_attempts=0, replay_failed=0, GPU errors=0. Последнее CPU окно обрезано
при завершении записи; для сравнения взяты только первые шесть полных окон.

Изменение сохранено как уменьшение доказанно лишней работы при прежнем
изображении; заметное ускорение FPS этим запуском не доказано. Следующее
направление — [одноразовое повторное использование нулевого stencil](switch-stencil-zero-reuse.md),
которое убирает восемь draw в подходящей группе, а не только уменьшает их площадь.
