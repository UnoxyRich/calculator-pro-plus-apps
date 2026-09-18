#include "native_ui_stubs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int mathapp_native_main(void);

#define INPUT_CAPACITY 64
#define INPUT_COUNT 64
#define KEY_CAPACITY 64
#define KEY_COUNT 32
#define SCREEN_CAPACITY 4096
#define HISTORY_CAPACITY 262144

static const char ti_sqrt_equation[] = {
    (char) 0x10, '(', '2', 'x', '-', '3', ')', '=', '1', '\0'
};
static const char ti_negative_four[] = {(char) 0x1A, '4', '\0'};

static const char *input_queue[INPUT_CAPACITY];
static size_t input_count;
static size_t input_index;
static uint16_t key_queue[KEY_CAPACITY];
static size_t key_count;
static size_t key_index;
static int result_prompt_consumed;
static char screen[SCREEN_CAPACITY];
static char history[HISTORY_CAPACITY];

static void append_text(char *destination, size_t capacity, const char *text) {
    size_t used = strlen(destination);
    size_t available = used < capacity ? capacity - used - 1 : 0;
    if (available == 0) {
        return;
    }
    strncat(destination, text, available);
}

static void queue_input(const char *value) {
    assert(input_count < INPUT_CAPACITY);
    input_queue[input_count++] = value;
}

static void queue_key(uint16_t key) {
    assert(key_count < KEY_CAPACITY);
    key_queue[key_count++] = key;
}

static int screen_has(const char *text) {
    return strstr(screen, text) != NULL;
}

void os_ClrHomeFull(void) {
    screen[0] = '\0';
    result_prompt_consumed = 0;
}

void os_NewLine(void) {
    append_text(screen, sizeof(screen), "\n");
    append_text(history, sizeof(history), "\n");
}

void os_PutStrFull(const char *text) {
    append_text(screen, sizeof(screen), text);
    append_text(history, sizeof(history), text);
}

void os_GetStringInput(const char *prompt, char *buffer, size_t capacity) {
    const char *value;
    (void) prompt;
    assert(input_index < input_count);
    value = input_queue[input_index++];
    if (capacity == 0) {
        return;
    }
    snprintf(buffer, capacity + 1, "%s", value);
}

uint16_t os_GetKey(void) {
    if (screen_has("Any: next  Enter: steps") && !result_prompt_consumed) {
        result_prompt_consumed = 1;
        if (screen_has("Analysis") && !screen_has("Concave up: ")) {
            return sk_Down;
        }
        if (screen_has("Transform result") && !screen_has("x-int: ")) {
            return sk_Down;
        }
        if (screen_has("Equation result") && !screen_has("Answer: ")) {
            return sk_Down;
        }
        return sk_Enter;
    }
    if (screen_has("Next: any key") || screen_has("Press any key")) {
        return sk_Clear;
    }
    if (key_index < key_count) {
        return key_queue[key_index++];
    }
    return sk_Clear;
}

static void queue_menu_navigation(void) {
    queue_key(sk_Enter); /* automatic normal */
    queue_key(sk_Enter); /* automatic radical equation */
    queue_key(sk_Enter); /* automatic guided piecewise */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* solve */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* analyze */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* transform */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* piecewise analyze */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* more tools */
    queue_key(sk_Enter); /* compose */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* compose three */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* average rate */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* quadratic fit */
    queue_key(sk_Clear); /* leave more tools */
    queue_key(sk_Down);
    queue_key(sk_Enter); /* About */
    queue_key(sk_Clear); /* exit from main */
}

static void queue_inputs(void) {
    static const char *piecewise_expr_one = "x";
    static const char *piecewise_expr_two = "-x";

    /* Automatic normal function. */
    queue_input("0");
    queue_input("x^3");

    /* Automatic equation using the TI-ASCII square-root glyph. */
    queue_input("0");
    queue_input(ti_sqrt_equation);

    /* Automatic guided piecewise function. */
    queue_input("1");
    queue_input("2");
    queue_input(piecewise_expr_one);
    queue_input("-2");
    queue_input("0");
    queue_input("0");
    queue_input("0");
    queue_input(piecewise_expr_two);
    queue_input("0");
    queue_input("2");
    queue_input("0");
    queue_input("0");

    /* Complex quadratic equation. */
    queue_input("x^2+2x+3");
    queue_input("0");

    /* Function analysis. */
    queue_input("sqrt(x^2-1)");

    /* Transformation with a TI-ASCII negation glyph as h. */
    queue_input("x^2-3x-4");
    queue_input(ti_negative_four);
    queue_input("4");
    queue_input("1");
    queue_input("1");
    queue_input("0");
    queue_input("0");

    /* Guided worksheet piecewise function. */
    queue_input("3");
    queue_input("(x+1)^2");
    queue_input("-2");
    queue_input("0");
    queue_input("0");
    queue_input("0");
    queue_input("1-x");
    queue_input("0");
    queue_input("2");
    queue_input("1");
    queue_input("1");
    queue_input("-3x^2+18x-25");
    queue_input("2");
    queue_input("4");
    queue_input("0");
    queue_input("0");

    /* Two-function composition. */
    queue_input("x^2-1");
    queue_input("x+1");

    /* Three-function composition. */
    queue_input("x^2-1");
    queue_input("x+1");
    queue_input("x-2");

    /* Average rate of change. */
    queue_input("0");
    queue_input("1-x");
    queue_input("0");
    queue_input("2");

    /* Three-point quadratic fitting. */
    queue_input("0");
    queue_input("14");
    queue_input("35");
    queue_input("57");
    queue_input("45");
    queue_input("46");

}

int main(void) {
    queue_menu_navigation();
    queue_inputs();
    assert(mathapp_native_main() == 0);
    assert(input_index == input_count);
    assert(key_index == key_count);
    assert(strstr(history, "Parity: ") != NULL);
    assert(strstr(history, "Odd") != NULL);
    assert(strstr(history, "(-2,0) U (0,2)") != NULL);
    assert(strstr(history, "x=-1+/-i*sqrt(2)") != NULL);
    assert(strstr(history, "(-infinity,-1] U [1,infinity)") != NULL);
    assert(strstr(history, "x=2") != NULL);
    assert(strstr(history, "Vertex: ") != NULL);
    assert(strstr(history, "x-int: ") != NULL);
    assert(strstr(history, "Concave up: ") != NULL);
    assert(strstr(history, "Composition") != NULL);
    assert(strstr(history, "Three-function composition") != NULL);
    assert(strstr(history, "Average rate: \n-1") != NULL);
    assert(strstr(history, "Quadratic fit") != NULL);
    assert(strstr(history, "Exact fractions, radicals") != NULL);
    puts("native UI smoke test passed");
    return 0;
}
