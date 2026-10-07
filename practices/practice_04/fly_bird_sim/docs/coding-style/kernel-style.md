# Linux Kernel Coding Style Guidelines

This document adapts the official Linux Kernel Coding Style (from `Documentation/process/coding-style.rst`) for C projects.

## 1. Indentation
- Use tabs for indentation. A tab is 8 characters wide.
- Do not put multiple statements on a single line unless you have something to hide.
- Switch statements: indent `switch` and `case` labels at the same column:
```c
switch (action) {
case ACTION_RUN:
	do_run();
	break;
default:
	break;
}
```

## 2. Breaking Long Lines and Strings
- Statements longer than 80 columns should be broken into sensible chunks, unless exceeding 80 columns significantly increases readability.
- Descendants should be placed substantially to the right.

## 3. Placing Braces and Spaces
- Opening brace goes on the same line as the statement:
```c
if (condition) {
	/* ... */
} else {
	/* ... */
}
```
- Exception: functions place the opening brace at the beginning of the next line:
```c
int my_function(int arg)
{
	/* ... */
	return 0;
}
```
- Do not use unnecessary braces for single-statement bodies:
```c
if (condition)
	action();
```

## 4. Naming Conventions
- No `CamelCase` or Hungarian notation (`dwFoo`).
- Use descriptive, lower-case names with underscores (`snake_case`): e.g., `count_active_users()`.
- Local variable names should be short and concise (e.g., `i`, `tmp`, `len`, `ret`).
- Global variables and functions require descriptive names.

## 5. Typedefs
- Avoid typedefs for structures and pointers. Keep `struct foo` explicit.
- Use typedefs only for:
  - Exact-width or platform-dependent types (`uint32_t`, `size_t`)
  - Opaque types where the caller should not touch struct members directly

## 6. Functions
- Functions should be short and do one thing well.
- Aim for functions that fit on one or two screens (24–40 lines).
- Return values: use `0` or positive on success, negative error codes (e.g., `-EINVAL`, `-ENOMEM`) on failure, or boolean where appropriate.

## 7. Centralized Exiting and Cleanup
- When a function needs cleanup upon failure across several stages, use `goto`:
```c
int init_resources(struct context *ctx)
{
	int ret = 0;

	ctx->buf = malloc(BUFFER_SIZE);
	if (!ctx->buf) {
		ret = -ENOMEM;
		goto out;
	}

	ret = setup_device(ctx);
	if (ret < 0)
		goto err_free_buf;

	return 0;

err_free_buf:
	free(ctx->buf);
	ctx->buf = NULL;
out:
	return ret;
}
```

## 8. Commenting
- Comments describe *what* and *why* the code does something, never *how* when the code itself is self-explanatory.
- Multi-line comment style:
```c
/*
 * This is the preferred style for multi-line
 * comments in the Linux kernel source code.
 */
```

## 9. Memory Allocation & Pointer Rules
- Always check the return value of allocation functions.
- Avoid cast on `void *` pointers (e.g. `malloc(sizeof(*ptr))` does not need `(struct foo *)`).
- Free memory in reverse order of allocation during cleanup.
