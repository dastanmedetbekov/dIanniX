# DIAMED v0.1 — Lightweight C-like Live Coding Language

## 1) Цели языка

Diamed создаётся как минимальный C-подобный язык для live coding в IanniX.

Ключевые цели:

1. **Простой синтаксис** (ближе к C, без Java-церемоний).
2. **Детерминированность** (одинаковый input/seed -> одинаковый результат).
3. **Безопасность в realtime** (ограничения на тяжелые операции в `on tick`).
4. **Совместимость** с текущими IanniX-командами (`run(...)` не ломается).
5. **Диагностика уровня IDE** (строка/колонка, код ошибки, человеко-понятное сообщение).

### 1.1 Философия IanniX

Diamed должен усиливать философию IanniX, а не заменять её:

1. **Сначала пространство и время**: код управляет траекториями/событиями, но не скрывает визуальную партитуру.
2. **Музыкальная предсказуемость**: одна и та же сцена должна вести себя одинаково на репетиции и концерте.
3. **Живое редактирование без паники**: ошибки локализуются диагностикой, а не аварийным поведением runtime.
4. **Мост с существующим патчем**: `legacy("run ...")` остаётся как эволюционный переходный слой.

---

## 2) Минимальный синтаксис v0.1

### 2.1 Типы

- `int`
- `float`
- `bool`
- `string`
- `vec2`
- `vec3`

### 2.2 Объявления

```c
int count = 32;
float speed = 1.25;
vec3 p = vec3(0.0, 1.0, 0.0);
```

### 2.3 Управляющие конструкции

```c
if (speed > 1.0) {
	speed = speed * 0.95;
} else {
	speed = 1.0;
}

for (int i = 0; i < 64; i = i + 1) {
	// ...
}

while (count > 0) {
	count = count - 1;
}
```

### 2.4 Функции

```c
fn float easeIn(float x) {
	return x * x;
}
```

### 2.5 События

```c
on start {
	// запуск сцены
}

on tick(dt) {
	// каждый тик
}

on beat(4) {
	// каждый 4-й beat
}
```

---

## 3) Семантика live coding

### 3.1 Разделение фаз

1. **Compile phase**: parsing, type check, bytecode/IR.
2. **Start phase**: `on start` выполняется 1 раз.
3. **Realtime phase**: `on tick`, `on beat`, `on event`.

### 3.2 Realtime-ограничения

Внутри `on tick` запрещаются/ограничиваются:

- неограниченные циклы,
- динамические аллокации в больших объёмах,
- потенциально блокирующий IO.

Runtime должен иметь:

- instruction budget на тик,
- watchdog timeout,
- защиту от рекурсивного runaway.

### 3.3 Live Stage Profile (обязательный для выступлений)

Профиль `LiveStage` включает дополнительные гарантии:

- лимит размера патча (line budget),
- запрет realtime-unsafe конструкций,
- предупреждения о недетерминированности (`random/rand`),
- только инкрементальные изменения между компиляциями.

---

## 4) Встроенный API v0.1 (черновой набор)

### 4.1 Scene/objects

- `addCurve() -> int`
- `addCursor() -> int`
- `addTrigger() -> int`
- `setPos(id, x, y, z)`
- `setColor(id, r, g, b, a)`
- `setLabel(id, text)`

### 4.2 Time/transport

- `time() -> float`
- `play()`
- `stop()`
- `goto(seconds)`

### 4.3 Messaging

- `sendOsc(path, ...args)`
- `sendMidi(port, status, d1, d2)`

### 4.4 Legacy escape hatch

- `legacy("run ...")`

---

## 5) Ошибки и диагностика

Формат ошибки:

```text
E0102 [line:12 col:8] expected ';' after expression
```

Диагностика должна включать:

- machine code (`E0102`),
- уровень (`error/warning/info`),
- позицию,
- fix hint (`did you mean ...`).

---

## 6) Грамматика (минимальный EBNF набросок)

```ebnf
program      = { declaration | functionDecl | eventDecl | statement } ;

declaration  = type identifier [ "=" expression ] ";" ;
type         = "int" | "float" | "bool" | "string" | "vec2" | "vec3" ;

functionDecl = "fn" type identifier "(" [ params ] ")" block ;
eventDecl    = "on" identifier [ "(" [ params ] ")" ] block ;

params       = param { "," param } ;
param        = [ type ] identifier ;

statement    = block
			 | ifStmt
			 | whileStmt
			 | forStmt
			 | returnStmt
			 | exprStmt ;

block        = "{" { statement } "}" ;
ifStmt       = "if" "(" expression ")" statement [ "else" statement ] ;
whileStmt    = "while" "(" expression ")" statement ;
forStmt      = "for" "(" [exprStmt] [expression] ";" [expression] ")" statement ;
returnStmt   = "return" [ expression ] ";" ;
exprStmt     = expression ";" ;
```

---

## 7) Стратегия внедрения в IanniX

1. Добавить модуль `scripting/diamed` (lexer/parser/IR/diagnostics).
2. Сделать CLI/внутренний API: `compileDiamed(source)`.
3. В редакторе добавить переключатель language mode: `Legacy` / `Diamed`.
4. При `Diamed`: `compile -> validate -> execute`.
5. Постепенно расширять API и оптимизировать runtime.

---

## 8) Что v0.1 НЕ включает

- классы/наследование,
- GC и сложные контейнеры,
- reflection,
- потоки пользователя.

Это сознательно: сначала надёжность live coding, потом расширение.

---

## 9) Rehearsal vs Stage

- `Studio` режим: мягче проверки, больше свободы для прототипирования.
- `LiveStage` режим: строгий профиль с realtime guardrails.
- Переход между режимами не меняет язык, меняет только policy компиляции.

