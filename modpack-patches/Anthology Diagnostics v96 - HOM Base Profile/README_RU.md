# Anthology Diagnostics v96 — HOM Base Profile

Временный диагностический модуль после теста v95 без заметного изменения FPS.

Он включает:

- `r__hom_dynamic on`;
- `r__portal_traverse_stats 1` и `rs_stats on` для строки `dynamic HOM box: test/reject`;
- `mt_frame_profile 1` и `mt_frame_profile_detail 1` для записи в лог стоимости кадра, scheduler, online-объектов и Lua.

Нужно загрузить то же сохранение, постоять и походить на тяжёлой базе 30–40 секунд, затем штатно выйти. После анализа модуль должен быть отключён: подробная диагностика сама создаёт небольшой CPU overhead и не предназначена для обычной игры.

Маркер:

```text
* [anthology/diagnostics-v96] HOM and populated-base profile active
```
