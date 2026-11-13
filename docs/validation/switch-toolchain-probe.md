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
