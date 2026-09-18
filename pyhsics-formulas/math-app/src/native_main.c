#include "math_core.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ti/getcsc.h>
#include <ti/screen.h>
#include <tice.h>

static const char *menu_items[] = {
    "Automatic input",
    "Solve equation",
    "Analyze function",
    "Transform function",
    "Piecewise analyze",
    "About"
};

#define MENU_COUNT ((uint8_t) (sizeof(menu_items) / sizeof(menu_items[0])))

static uint8_t print_wrapped(const char *text) {
    char line[39];
    size_t length = strlen(text);
    size_t offset = 0;
    size_t chunk;
    uint8_t lines = 0;

    while (offset < length) {
        chunk = length - offset;
        if (chunk >= sizeof(line)) {
            chunk = sizeof(line) - 1;
        }
        memcpy(line, text + offset, chunk);
        line[chunk] = '\0';
        os_PutStrFull(line);
        os_NewLine();
        offset += chunk;
        lines++;
    }
    if (length == 0) {
        os_NewLine();
        lines = 1;
    }
    return lines;
}

static uint8_t print_label_value(const char *label, const char *value) {
    uint8_t lines;

    /* Keep the value intact even when an exact interval is longer than the
     * screen width.  The previous fixed 160-byte combined buffer could clip
     * a valid result before print_wrapped had a chance to paginate it. */
    os_PutStrFull(label);
    os_NewLine();
    lines = print_wrapped(value);
    return (uint8_t) (lines + 1);
}

static void wait_for_key(void) {
    os_GetKey();
}

static void show_steps(const MathSteps *steps) {
    uint8_t index;
    if (steps->count == 0) {
        os_ClrHomeFull();
        os_PutStrFull("Steps");
        os_NewLine();
        os_PutStrFull("No recorded steps.");
        os_NewLine();
        os_PutStrFull("Press any key");
        wait_for_key();
        return;
    }
    for (index = 0; index < steps->count; index++) {
        char progress[32];
        os_ClrHomeFull();
        os_PutStrFull("Steps");
        os_NewLine();
        print_wrapped(steps->lines[index]);
        snprintf(progress, sizeof(progress), "%u/%u  Next: any key",
                 (unsigned) (index + 1), (unsigned) steps->count);
        os_PutStrFull(progress);
        wait_for_key();
    }
}

typedef struct {
    const char *label;
    const char *value;
} ResultField;

static void show_result_pages(const char *title, const ResultField *fields,
                              size_t field_count, const MathSteps *steps) {
    size_t index;
    for (index = 0; index < field_count; index++) {
        uint16_t key;
        os_ClrHomeFull();
        os_PutStrFull(title);
        os_NewLine();
        print_label_value(fields[index].label, fields[index].value);
        os_PutStrFull("Any: next  Enter: steps");
        key = os_GetKey();
        if (key == sk_Enter) {
            show_steps(steps);
            return;
        }
        if (key == sk_Clear) {
            return;
        }
    }
}

static void show_analysis(const MathAnalysis *analysis) {
    ResultField fields[8];
    size_t count = 7;
    fields[0] = (ResultField) {"Domain: ", analysis->domain};
    fields[1] = (ResultField) {"Range: ", analysis->range};
    fields[2] = (ResultField) {"Parity: ", analysis->parity};
    fields[3] = (ResultField) {"Increasing: ", analysis->increasing};
    fields[4] = (ResultField) {"Decreasing: ", analysis->decreasing};
    fields[5] = (ResultField) {"Concave up: ", analysis->concave_up};
    fields[6] = (ResultField) {"Concave down: ", analysis->concave_down};
    if (analysis->warning[0] != '\0') {
        fields[count++] = (ResultField) {"Note: ", analysis->warning};
    }
    show_result_pages("Analysis", fields, count, &analysis->steps);
}

static void show_solution(const MathSolveResult *solution) {
    ResultField fields[3];
    size_t count = 2;
    fields[0] = (ResultField) {"Method: ", solution->method};
    fields[1] = (ResultField) {"Answer: ", solution->answer};
    if (solution->warning[0] != '\0') {
        fields[count++] = (ResultField) {"Note: ", solution->warning};
    }
    show_result_pages("Equation result", fields, count, &solution->steps);
}

static int read_expression(const char *prompt, char *buffer, size_t capacity) {
    buffer[0] = '\0';
    os_GetStringInput(prompt, buffer, capacity - 1);
    return buffer[0] != '\0';
}

static int read_fraction(const char *prompt, MathFraction *out) {
    char buffer[32];
    if (!read_expression(prompt, buffer, sizeof(buffer))) {
        return 0;
    }
    return math_fraction_parse(buffer, strlen(buffer), out);
}

static int read_toggle(const char *prompt, uint8_t *out) {
    MathFraction value;
    if (!read_fraction(prompt, &value)
            || value.den != 1
            || (value.num != 0 && value.num != 1)) {
        return 0;
    }
    *out = (uint8_t) value.num;
    return 1;
}

static void show_parse_error(const char *error) {
    os_ClrHomeFull();
    os_PutStrFull("Input error");
    os_NewLine();
    print_wrapped(error[0] == '\0' ? "The input could not be parsed." : error);
    os_PutStrFull("Press any key");
    wait_for_key();
}

static void solve_text(const char *input) {
    char error[MATH_ERROR_WIDTH];
    MathEquation equation;
    MathSolveResult result;
    MathStatus status;

    status = math_parse_equation(input, &equation, error, sizeof(error));
    if (status != MATH_OK) {
        show_parse_error(error);
        return;
    }
    status = math_solve_equation(&equation, &result);
    if (status == MATH_ERR_INPUT || result.status == MATH_ERR_LIMIT) {
        show_parse_error("The exact result exceeded the app storage limit.");
        return;
    }
    show_solution(&result);
}

static void analyze_text(const char *input) {
    char error[MATH_ERROR_WIDTH];
    MathExpr expression;
    MathAnalysis result;
    MathStatus status;

    status = math_parse_expression(input, &expression, error, sizeof(error));
    if (status != MATH_OK) {
        show_parse_error(error);
        return;
    }
    status = math_analyze_function(&expression, &result);
    if (status != MATH_OK) {
        show_parse_error("The function could not be analyzed.");
        return;
    }
    show_analysis(&result);
}

static void run_solver(void) {
    char input[MATH_OUTPUT_WIDTH];
    os_ClrHomeFull();
    if (read_expression("Equation: ", input, sizeof(input))) {
        solve_text(input);
    }
}

static void run_analysis(void) {
    char input[MATH_OUTPUT_WIDTH];
    os_ClrHomeFull();
    if (read_expression("f(x)=", input, sizeof(input))) {
        analyze_text(input);
    }
}

static void run_transform(void) {
    char input[MATH_OUTPUT_WIDTH];
    char error[MATH_ERROR_WIDTH];
    MathExpr expression;
    MathTransformSpec spec;
    MathTransformResult result;
    MathStatus status;

    os_ClrHomeFull();
    if (!read_expression("Function: ", input, sizeof(input))) {
        return;
    }
    status = math_parse_expression(input, &expression, error, sizeof(error));
    if (status != MATH_OK) {
        show_parse_error(error);
        return;
    }
    if (!read_fraction("Horizontal shift h=", &spec.horizontal_shift)
            || !read_fraction("Vertical shift k=", &spec.vertical_shift)
            || !read_fraction("Horizontal scale s=", &spec.horizontal_scale)
            || !read_fraction("Vertical scale a=", &spec.vertical_scale)) {
        show_parse_error("Use exact fractions such as -3, 1/2, or 4.");
        return;
    }
    if (!read_toggle("Reflect x-axis? 0/1", &spec.reflect_x_axis)
            || !read_toggle("Reflect y-axis? 0/1", &spec.reflect_y_axis)) {
        show_parse_error("Reflection choices must be 0 or 1.");
        return;
    }
    status = math_transform_quadratic(&expression, &spec, &result);
    if (status != MATH_OK) {
        show_parse_error(result.warning);
        return;
    }
    {
        const ResultField fields[] = {
            {"g(x)=", result.formula},
            {"Vertex: ", result.vertex},
            {"x-int: ", result.x_intercepts},
            {"y-int: ", result.y_intercept}
        };
        show_result_pages("Transform result", fields,
                          sizeof(fields) / sizeof(fields[0]), &result.steps);
    }
}

static void run_piecewise_analysis(void) {
    char input[MATH_OUTPUT_WIDTH];
    char error[MATH_ERROR_WIDTH];
    MathPiecewise piecewise;
    MathAnalysis result;
    MathStatus status;

    os_ClrHomeFull();
    os_PutStrFull("Syntax: piecewise(expr|[a,b); more branches");
    os_NewLine();
    if (!read_expression("Input: ", input, sizeof(input))) {
        return;
    }
    status = math_parse_piecewise(input, &piecewise, error, sizeof(error));
    if (status != MATH_OK) {
        show_parse_error(error);
        return;
    }
    status = math_analyze_piecewise(&piecewise, &result);
    if (status != MATH_OK) {
        show_parse_error("The piecewise function could not be analyzed.");
        return;
    }
    show_analysis(&result);
}

static void run_automatic(void) {
    char input[MATH_OUTPUT_WIDTH];
    MathPiecewise piecewise;
    MathAnalysis analysis;
    char error[MATH_ERROR_WIDTH];
    MathStatus status;

    os_ClrHomeFull();
    if (!read_expression("Input: ", input, sizeof(input))) {
        return;
    }
    if (math_classify_input(input) == MATH_INPUT_PIECEWISE) {
        status = math_parse_piecewise(input, &piecewise, error, sizeof(error));
        if (status != MATH_OK) {
            show_parse_error(error);
            return;
        }
        status = math_analyze_piecewise(&piecewise, &analysis);
        if (status != MATH_OK) {
            show_parse_error("The piecewise function could not be analyzed.");
            return;
        }
        show_analysis(&analysis);
    } else if (math_classify_input(input) == MATH_INPUT_EQUATION) {
        solve_text(input);
    } else {
        analyze_text(input);
    }
}

static void show_about(void) {
    os_ClrHomeFull();
    os_PutStrFull("MATHAPP for TI-84 Plus CE");
    os_NewLine();
    print_wrapped("Exact fractions, radicals, complex quadratic roots, and real function analysis.");
    print_wrapped("The app preserves denominator exclusions and records the operations used for each result.");
    print_wrapped("Current exact interval rules cover polynomial degree 0 through 2 and selected rational or radical domains.");
    os_PutStrFull("Press any key");
    wait_for_key();
}

static void draw_menu(uint8_t selected) {
    uint8_t index;
    os_ClrHomeFull();
    os_PutStrFull("MATHAPP");
    os_NewLine();
    for (index = 0; index < MENU_COUNT; index++) {
        os_PutStrFull(index == selected ? "> " : "  ");
        os_PutStrFull(menu_items[index]);
        os_NewLine();
    }
    os_PutStrFull("Up/down select  Enter open");
}

int main(void) {
    uint8_t selected = 0;
    uint16_t key;

    for (;;) {
        draw_menu(selected);
        key = os_GetKey();
        if (key == sk_Up) {
            selected = selected == 0 ? MENU_COUNT - 1 : selected - 1;
        } else if (key == sk_Down) {
            selected = selected + 1 == MENU_COUNT ? 0 : selected + 1;
        } else if (key == sk_Enter) {
            if (selected == 0) {
                run_automatic();
            } else if (selected == 1) {
                run_solver();
            } else if (selected == 2) {
                run_analysis();
            } else if (selected == 3) {
                run_transform();
            } else if (selected == 4) {
                run_piecewise_analysis();
            } else {
                show_about();
            }
        } else if (key == sk_Clear) {
            return 0;
        }
    }
}
