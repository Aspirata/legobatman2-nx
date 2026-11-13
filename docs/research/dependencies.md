# Проверка зависимостей Pre-Run 1

Системные установки запрещены пользователем; пакеты хранятся в `work/toolchain`.
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
