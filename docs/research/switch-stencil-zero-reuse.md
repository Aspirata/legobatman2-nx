# Повторное использование доказанного нулевого stencil

2026-11-17. Продолжение пункта Astra3 об актуальности stencil между проходами.
[047](switch-stencil-clear-cutout.md#аппаратный-результат-run047) уменьшил площадь
переносов на4,63%, но устойчивые FPS отличаются лишь на0,10%. Следующий контроль
убирает сами восемь stencil-bit draw в узкой доказанной последовательности.

## Доказательство и ограничения

Принятый full clear003062D0 полностью заменяет480×480 stencil всех4 samples
нулём в tiles[720,1440). Следующий material RTUpdate часто запрашивает
`003062D0 → 002062D0` того же диапазона: это960×960 при1x. Исходный путь уже
записывает полный native stencilzero, затем восемью bit passes переносит нули.
Native zero сам воспроизводит точный результат этого stencil import.

Трасса180:565→590,1163→1188,1218→1243,1277→1302,1351→1376.
Трасса280:855→880,918→943,996→1021,1073→1098,1240→1265,1863→1888.
Между каждой парой —24 elided/no-output операции без material draws/resolves.
Повтор production-кодом даёт5/6 групп и40/48 исключаемых stencil draws.
Это не прогноз FPS. Дополнительные SDK guards могут отклонить аппаратный случай.

Новый init-only `lb2_stencil_zero_reuse=false` по умолчанию. Для этой пробы
требуется включённый прежний full-cutout guard: новый флаг не включает его
самостоятельно. Проба основана на047, где оба full clear flags уже включены.

После **реального** полного draw-clear публикуется одно доказательство, связанное
с конкретным VkImage. Для этого проверяются прежние exact VS27/state/vertices,
zero stencil reference, PS=null и actual target key/scale/height. Публикация
возможна и без deferred imports: факт очистки важен отдельно от пропуска импорта.
PS-null async contract остаётся обязательным.

Каждый VulkanRTUpdate забирает и обнуляет token до базового Update, даже при
отказе/неиспользуемых RT. Только его текущий IssueDraw Scope может один раз
применить доказательство. Другой material draw без подходящего переноса теряет
token. Resolve entry обнуляет доказательство; destructor image удаляет его
из global/source/ожидающего публикации Context, исключая reuse старого handle.
При active query/visibility query новый Scope отключён.

Потребитель допускается только для указанной пары, диапазона720..1440,
одного full960×960 rectangle, scale1, единственной depth target, без
host-depth source, resolve/cutout или EXT_shader_stencil_export.
При неизвестном/несовпавшем состоянии исходные восемь passes сохраняются.

Перечисление j1 и full native stencilzero **сохраняются**. Затем вся stencil-bit
invocation пропускается до генерации geometry/fast-copy branch. J0 depth import,
barriers, source lifetime, transfer list и ownership model не меняются.
Отдельного rollback потребителя нет: nativezero уже восстановил тот же stencil,
независимо от дальнейшего успешного или пропущенного игрового draw.

Это одноразовое доказательство поколения, не общая lazy materialization и не
предположение, что stencil не нужен при выключенном stencil test. Разрешение,
MSAA, shaders, memexport и число ожиданий GPU сохраняются.

## Проверки и аппаратный контроль

Production unit tests проверяют отсутствие доказательства, flag OFF, реальные и
пропущенные clears, публикацию без deferred imports, один material consumer,
физический handle, resolve/destruction и изменённые transfer guards. Старые full/
partial clear проверки и actual SDK oracle сохранены. Oracle подтверждает, что
4x480×480 и1x960×960 описывают одни720 tiles/921600 samples. Causal replay
проверяет точные программы и порядок всех material Update в кадрах180/280.

Код приложения — `lb2_stencil_zero.h`, `stencil_zero.cpp` и малый publication hook
в `depth_clear.cpp`. SDK patch — `port/rexglue-horizon-stencil-zero-reuse.patch`.
Материалы: `work/analysis/stencil-zero-reuse/`. Публичные SDK headers не меняются.

Аппаратного результата пока нет. Настройки047 сохраняются и добавляется только
новый renderer flag; GPU query режимы выключены. Нужен тот же начальный пролёт
90 секунд без управления, затем закрытие игры и MTP; ожидается048. Проверить
`published`, `groups`, `avoided_draws`, уменьшение stencil draw count, FPS и
изображение. Исходный047 остаётся для точного отката.

## Сборка и установка

Финальная ARM64 сборка и все обязательные регрессии прошли (`build-final.log`).
Дополнительно проверены destruction до публикации, bind без full proof и
actual causal replay5/6 групп. Проверка кода завершена без существенных замечаний;
инкрементальный SDK patch обратно применим. Новая пара установлена, полностью
прочитана обратно и проверена по SHA256. Предыдущая047 сохранена в `preinstall/`.

| Файл | Размер | SHA256 |
| --- | ---: | --- |
| NRO | 70 441 157 | `7508fe4607dc27cb2db5475403967212e1ea087d2accc0de641086a061d7a29d` |
| TOML | 1 368 | `33009870e451d62404688b08f34bf652d5aa0a0be130f650606b6578702c783b` |

`preparation.json`, `build-verification.json`, `verification.json`, `readback/`
фиксируют идентичность. Единственное изменение renderer settings относительно047 —
`lb2_stencil_zero_reuse=true`; оба GPU query режима остаются OFF. Аппаратный
результат048 после этой установки ещё не получен, прирост FPS не заявляется.
