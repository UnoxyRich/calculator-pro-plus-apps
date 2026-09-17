#include "math_core.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ti/getcsc.h>
#include <ti/screen.h>
#include <tice.h>

static const char *menu_items[] = {
    "Solve equation",
    "Analyze function",
    "Transform quadratic",
    "Piecewise analyze",
    "About"
};

#define MENU_COUNT ((uint8_t) (sizeof(menu_items) / sizeof(menu_items[0])))

static void print_wrapped(const char *text) {
    char line[39];
    size_t length = strlen(text);
    size_t offset = 0;
    size_t chunk;

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
    }
    if (length == 0) {
        os_NewLine();
    }
}

static void print_label_value(const char *label, const char *value) {
    char line[160];
    snprintf(line, sizeof(line), "%s%s", label, value);
    print_wrapped(line);
}

static void wait_for_key(void) {
    os_GetKey();
}

static void show_steps(const MathSteps *steps) {
    uint8_t index;
    uint8_t page_count = 0;

    for (index = 0; index < steps->count; index++) {
        if (page_count == 0) {
            os_ClrHomeFull();
            os_PutStrFull("Steps");
            os_NewLine();
        }
        print_wrapped(steps->lines[index]);
        page_count++;
        if (page_count == 4 || index + 1 == steps->count) {
            os_PutStrFull("Press any key");
            wait_for_key();
            page_count = 0;
        }
    }
}

static void show_analysis(const MathAnalysis *analysis) {
    os_ClrHomeFull();
    os_PutStrFull("Analysis");
    os_NewLine();
    print_label_value("Domain: ", analysis->domain);
    print_label_value("Range: ", analysis->range);
    print_label_value("Parity: ", analysis->parity);
    print_label_value("Increasing: ", analysis->increasing);
    print_label_value("Decreasing: ", analysis->decreasing);
    print_label_value("Concave up: ", analysis->concave_up);
    print_label_value("Concave down: ", analysis->concave_down);
    if (analysis->warning[0] != '\0') {
        print_label_value("Note: ", analysis->warning);
    }
    os_PutStrFull("Enter: steps  Clear: menu");
    if (os_GetKey() == sk_Enter) {
        show_steps(&analysis->steps);
    }
}

static void show_solution(const MathSolveResult *solution) {
    os_ClrHomeFull();
    os_PutStrFull("Equation result");
    os_NewLine();
    print_label_value("Method: ", solution->method);
    print_label_value("Answer: ", solution->answer);
    if (solution->warning[0] != '\0') {
        print_label_value("Note: ", solution->warning);
    }
    os_PutStrFull("Enter: steps  Clear: menu");
    if (os_GetKey() == sk_Enter) {
        show_steps(&solution->steps);
    }
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

static void show_parse_error(const char *error) {
    os_ClrHomeFull();
    os_PutStrFull("Input error");
    os_NewLine();
    print_wrapped(error[0] == '\0' ? "The input could not be parsed." : error);
    os_PutStrFull("Press any key");
    wait_for_key();
}

static void run_solver(void) {
    char input[MATH_OUTPUT_WIDTH];
    char error[MATH_ERROR_WIDTH];
    MathEquation equation;
    MathSolveResult result;
    MathStatus status;

    os_ClrHomeFull();
    if (!read_expression("Equation: ", input, sizeof(input))) {
        return;
    }
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

static void run_analysis(void) {
    char input[MATH_OUTPUT_WIDTH];
    char error[MATH_ERROR_WIDTH];
    MathExpr expression;
    MathAnalysis result;
    MathStatus status;

    os_ClrHomeFull();
    if (!read_expression("f(x)=", input, sizeof(input))) {
        return;
    }
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

static void run_transform(void) {
    char input[MATH_OUTPUT_WIDTH];
    char error[MATH_ERROR_WIDTH];
    MathExpr expression;
    MathTransformSpec spec;
    MathTransformResult result;
    MathStatus status;

    os_ClrHomeFull();
    if (!read_expression("Quadratic: ", input, sizeof(input))) {
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
    spec.reflect_x_axis = 0;
    spec.reflect_y_axis = 0;
    status = math_transform_quadratic(&expression, &spec, &result);
    if (status != MATH_OK) {
        show_parse_error(result.warning);
        return;
    }
    os_ClrHomeFull();
    os_PutStrFull("Transform result");
    os_NewLine();
    print_label_value("g(x)=", result.formula);
    print_label_value("Vertex: ", result.vertex);
    print_label_value("x-int: ", result.x_intercepts);
    print_label_value("y-int: ", result.y_intercept);
    os_PutStrFull("Enter: steps  Clear: menu");
    if (os_GetKey() == sk_Enter) {
        show_steps(&result.steps);
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
    os_PutStrFull("Automatic exact math");
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
                run_solver();
            } else if (selected == 1) {
                run_analysis();
            } else if (selected == 2) {
                run_transform();
            } else if (selected == 3) {
                run_piecewise_analysis();
            } else {
                show_about();
            }
        } else if (key == sk_Clear) {
            return 0;
        }
    }
}
