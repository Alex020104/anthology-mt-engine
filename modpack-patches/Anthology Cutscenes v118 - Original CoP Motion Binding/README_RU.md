# Anthology Cutscenes v118 — Original CoP Motion Binding

Узкий патч поверх v117 для двух сюжетных сцен Call of Pripyat: спуска в Путепровод (`jup_b219`) и прибытия в Припять (`pri_a15`). Камера, трава/details, звук, диалоги, тайминги и наборы анимаций не меняются.

## Реальная причина полётов NPC

`state_mgr` хранит логическое состояние сцены, например `pri_a15_idle_none`, но в `add_anim()` передаёт уже выбранный физический motion — обычно `chest_0_idle_0`. v117 ошибочно проверял именно physical motion на префикс `pri_a15_` или `jup_b219_`. Поэтому первый клип почти всех участников уходил в базовый Anomaly-код с `local_animation = true`, прежняя walker/idle-матрица становилась началом root-motion цепочки, а персонажи получали неверную высоту, скользили и поворачивались не в авторскую сторону. В журнале v117 перехватил только поздний повторный anchor Зулуса — это и подтвердило дефект.

v118 восстанавливает контракт оригинального Call of Pripyat:

- первый root-moving physical motion авторского `animation_position` + `animation_direction` запускается абсолютно (`local_animation = false`), независимо от имени motion;
- если перед ним встретится действительно non-moving клип, он не потребляет anchor — это повторяет порядок retail CoP;
- следующие moving-клипы при неизменном anchor остаются локальными (`local_animation = true`) и продолжают штатную root-motion траекторию;
- новый явный authored transform после штатного сброса снова передаётся как абсолютный запрос; жизненный цикл movement controller остаётся движковым;
- область действия ограничена точным уровнем и точными spawn-секциями участников `jup_b219`/`pri_a15`;
- NPC не телепортируются через `set_position`; общий движковый контроллер анимационного движения, камера и физика не подменяются.

Для `jup_b219` также исправлен вызов luabind patrol point: объект пути и `:point(0)` теперь читаются внутри одного защищённого вызова. Раздельный вызов метода в v117 возвращал `:path-point` при существующем пути из `all.spawn`, из-за чего сцена всегда доходила до жёсткого 30-секундного fallback.

Оригинальный CoP `state_mgr_animation.script` также использует `local_animation = false` для первого клипа с авторской позицией и направлением. C++-цепочка `add_animation`/`animation_movement_controller` сверена с OpenXRay CoP: менять глобальную физику движка для этой ошибки не требуется. Это важно: более поздний CoC/Anomaly как раз изменил эту ветку на локальную, и v118 не называет такое поведение оригинальным CoP.

## Проверка

Новая игра и очистка shader cache не нужны. Нужны сохранения перед спуском в Путепровод и перед переходом в Припять.

В логе ожидаются строки:

```text
[anthology/cutscenes-v118] original CoP motion-binding patch active
[anthology/cutscenes-v118] original CoP root anchor npc=... motion=chest_0_idle_0 state=pri_a15_idle_none ... local=false
[anthology/cutscenes-v118] JUP NPC XFORM settled
[anthology/cutscenes-v118] PRI NPC XFORM settled
```

Ключевая проверка: `root anchor` должен появиться для каждого animpoint-участника сцены, а не только для Зулуса. В кадре NPC должны идти анимацией по земле без бокового скольжения, скачков по Y и разворота из старой walker-позы.
