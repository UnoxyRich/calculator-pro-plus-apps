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
    "More tools",
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

static int read_piecewise_guided(MathPiecewise *out, char *error,
                                 size_t error_capacity) {
    MathFraction branch_count;
    uint8_t index;

    memset(out, 0, sizeof(*out));
    if (!read_fraction("Branches (1-4)=", &branch_count)
            || branch_count.den != 1
            || branch_count.num < 1
            || branch_count.num > MATH_PIECEWISE_BRANCHES) {
        snprintf(error, error_capacity, "Choose 1, 2, 3, or 4 branches.");
        return 0;
    }
    out->count = (uint8_t) branch_count.num;
    for (index = 0; index < out->count; index++) {
        MathPiecewiseBranch *branch = &out->branch[index];
        char prompt[24];
        char expression_text[MATH_OUTPUT_WIDTH];
        MathStatus status;

        snprintf(prompt, sizeof(prompt), "Expr %u=", (unsigned) (index + 1));
        if (!read_expression(prompt, expression_text, sizeof(expression_text))) {
            snprintf(error, error_capacity, "Each branch needs an expression.");
            return 0;
        }
        status = math_parse_expression(expression_text, &branch->expression,
                                       error, error_capacity);
        if (status != MATH_OK) {
            return 0;
        }
        snprintf(prompt, sizeof(prompt), "Lower %u=", (unsigned) (index + 1));
        if (!read_fraction(prompt, &branch->lower)) {
            snprintf(error, error_capacity, "Bounds must be exact fractions.");
            return 0;
        }
        snprintf(prompt, sizeof(prompt), "Upper %u=", (unsigned) (index + 1));
        if (!read_fraction(prompt, &branch->upper)
                || math_fraction_compare(branch->lower, branch->upper) >= 0) {
            snprintf(error, error_capacity, "Each lower bound must be below its upper bound.");
            return 0;
        }
        snprintf(prompt, sizeof(prompt), "Lower closed? 0/1");
        if (!read_toggle(prompt, &branch->lower_closed)) {
            snprintf(error, error_capacity, "Endpoint choices must be 0 or 1.");
            return 0;
        }
        snprintf(prompt, sizeof(prompt), "Upper closed? 0/1");
        if (!read_toggle(prompt, &branch->upper_closed)) {
            snprintf(error, error_capacity, "Endpoint choices must be 0 or 1.");
            return 0;
        }
    }
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

static void solve_sides(const char *left_input, const char *right_input) {
    char error[MATH_ERROR_WIDTH];
    MathEquation equation;
    MathSolveResult result;
    MathStatus status;

    status = math_parse_expression(left_input, &equation.left,
                                   error, sizeof(error));
    if (status == MATH_OK) {
        status = math_parse_expression(right_input, &equation.right,
                                       error, sizeof(error));
    }
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
    char left_input[MATH_OUTPUT_WIDTH];
    char right_input[MATH_OUTPUT_WIDTH];
    os_ClrHomeFull();
    if (read_expression("Left side: ", left_input, sizeof(left_input))
            && read_expression("Right side: ", right_input,
                               sizeof(right_input))) {
        solve_sides(left_input, right_input);
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
            {"Vertex form: ", result.vertex_form},
            {"Vertex: ", result.vertex},
            {"x-int: ", result.x_intercepts},
            {"y-int: ", result.y_intercept}
        };
        show_result_pages("Transform result", fields,
                          sizeof(fields) / sizeof(fields[0]), &result.steps);
    }
}

static void run_piecewise_analysis(void) {
    char error[MATH_ERROR_WIDTH];
    MathPiecewise piecewise;
    MathAnalysis result;
    MathStatus status;

    os_ClrHomeFull();
    os_PutStrFull("Guided piecewise input");
    os_NewLine();
    if (!read_piecewise_guided(&piecewise, error, sizeof(error))) {
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

static void run_compose(void) {
    char outer_input[MATH_OUTPUT_WIDTH];
    char inner_input[MATH_OUTPUT_WIDTH];
    char error[MATH_ERROR_WIDTH];
    MathExpr outer;
    MathExpr inner;
    MathCompositionResult result;
    MathStatus status;

    os_ClrHomeFull();
    if (!read_expression("Outer f(x)=", outer_input, sizeof(outer_input))
            || !read_expression("Inner g(x)=", inner_input, sizeof(inner_input))) {
        return;
    }
    status = math_parse_expression(outer_input, &outer, error, sizeof(error));
    if (status != MATH_OK) {
        show_parse_error(error);
        return;
    }
    status = math_parse_expression(inner_input, &inner, error, sizeof(error));
    if (status != MATH_OK) {
        show_parse_error(error);
        return;
    }
    status = math_compose_functions(&outer, &inner, &result);
    if (status != MATH_OK) {
        show_parse_error(result.warning);
        return;
    }
    {
        const ResultField fields[] = {
            {"Expression: ", result.expression},
            {"Domain: ", result.domain},
            {"Range: ", result.range}
        };
        show_result_pages("Composition", fields,
                          sizeof(fields) / sizeof(fields[0]), &result.steps);
    }
}

static void run_average_rate(void) {
    char input[MATH_OUTPUT_WIDTH];
    char error[MATH_ERROR_WIDTH];
    MathExpr expression;
    MathPiecewise piecewise;
    MathAverageRateResult result;
    MathFraction first_x;
    MathFraction second_x;
    uint8_t piecewise_mode;
    MathStatus status;

    os_ClrHomeFull();
    if (!read_toggle("Piecewise? 0/1", &piecewise_mode)) {
        show_parse_error("Use 0/1 and exact values such as -2, 0, or 1/2.");
        return;
    }
    if (piecewise_mode != 0) {
        if (!read_piecewise_guided(&piecewise, error, sizeof(error))) {
            show_parse_error(error);
            return;
        }
    } else if (!read_expression("f(x)=", input, sizeof(input))) {
        return;
    }
    if (!read_fraction("x1=", &first_x)
            || !read_fraction("x2=", &second_x)) {
        show_parse_error("Use exact x-values such as -2, 0, or 1/2.");
        return;
    }
    if (piecewise_mode != 0) {
        status = math_average_rate_piecewise(&piecewise, first_x, second_x,
                                             &result);
    } else {
        status = math_parse_expression(input, &expression, error, sizeof(error));
        if (status == MATH_OK) {
            status = math_average_rate(&expression, first_x, second_x, &result);
        }
    }
    if (status != MATH_OK) {
        show_parse_error(status == MATH_ERR_SYNTAX ? error : result.warning);
        return;
    }
    {
        const ResultField fields[] = {{"Average rate: ", result.answer}};
        show_result_pages("Average rate", fields, 1, &result.steps);
    }
}

static void run_compose_three(void) {
    char outer_input[MATH_OUTPUT_WIDTH];
    char middle_input[MATH_OUTPUT_WIDTH];
    char inner_input[MATH_OUTPUT_WIDTH];
    char error[MATH_ERROR_WIDTH];
    MathExpr outer;
    MathExpr middle;
    MathExpr inner;
    MathCompositionResult result;
    MathStatus status;

    os_ClrHomeFull();
    if (!read_expression("Outer f(x)=", outer_input, sizeof(outer_input))
            || !read_expression("Middle g(x)=", middle_input, sizeof(middle_input))
            || !read_expression("Inner h(x)=", inner_input, sizeof(inner_input))) {
        return;
    }
    status = math_parse_expression(outer_input, &outer, error, sizeof(error));
    if (status == MATH_OK) {
        status = math_parse_expression(middle_input, &middle, error, sizeof(error));
    }
    if (status == MATH_OK) {
        status = math_parse_expression(inner_input, &inner, error, sizeof(error));
    }
    if (status != MATH_OK) {
        show_parse_error(error);
        return;
    }
    status = math_compose_three_functions(&outer, &middle, &inner, &result);
    if (status != MATH_OK) {
        show_parse_error(result.warning);
        return;
    }
    {
        const ResultField fields[] = {
            {"Expression: ", result.expression},
            {"Domain: ", result.domain},
            {"Range: ", result.range}
        };
        show_result_pages("Three-function composition", fields,
                          sizeof(fields) / sizeof(fields[0]), &result.steps);
    }
}

static void run_regression(void) {
    MathFraction x_values[3];
    MathFraction y_values[3];
    MathRegressionResult result;
    MathStatus status;
    uint8_t index;

    os_ClrHomeFull();
    for (index = 0; index < 3; index++) {
        char prompt[16];
        snprintf(prompt, sizeof(prompt), "x%u=", (unsigned) index);
        if (!read_fraction(prompt, &x_values[index])) {
            show_parse_error("Enter exact x-values.");
            return;
        }
        snprintf(prompt, sizeof(prompt), "y%u=", (unsigned) index);
        if (!read_fraction(prompt, &y_values[index])) {
            show_parse_error("Enter exact y-values.");
            return;
        }
    }
    status = math_quadratic_regression(x_values, y_values, 3, &result);
    if (status != MATH_OK) {
        show_parse_error(result.warning);
        return;
    }
    {
        const ResultField fields[] = {
            {"Model: ", result.equation},
            {"a=", result.coefficient_a},
            {"b=", result.coefficient_b},
            {"c=", result.coefficient_c}
        };
        show_result_pages("Quadratic fit", fields,
                          sizeof(fields) / sizeof(fields[0]), &result.steps);
    }
}

static void draw_more_tools(uint8_t selected) {
    static const char *items[] = {
        "Compose functions",
        "Compose three",
        "Average rate",
        "Quadratic fit",
        "Back"
    };
    uint8_t index;
    os_ClrHomeFull();
    os_PutStrFull("More tools");
    os_NewLine();
    for (index = 0; index < 5; index++) {
        os_PutStrFull(index == selected ? "> " : "  ");
        os_PutStrFull(items[index]);
        os_NewLine();
    }
    os_PutStrFull("Up/down select  Enter open");
}

static void run_more_tools(void) {
    uint8_t selected = 0;
    uint16_t key;
    for (;;) {
        draw_more_tools(selected);
        key = os_GetKey();
        if (key == sk_Up) {
            selected = selected == 0 ? 4 : selected - 1;
        } else if (key == sk_Down) {
            selected = selected == 4 ? 0 : selected + 1;
        } else if (key == sk_Enter) {
            if (selected == 0) {
                run_compose();
            } else if (selected == 1) {
                run_compose_three();
            } else if (selected == 2) {
                run_average_rate();
            } else if (selected == 3) {
                run_regression();
            } else {
                return;
            }
        } else if (key == sk_Clear) {
            return;
        }
    }
}

static void run_automatic(void) {
    char input[MATH_OUTPUT_WIDTH];
    MathPiecewise piecewise;
    MathAnalysis analysis;
    char error[MATH_ERROR_WIDTH];
    uint8_t piecewise_mode;
    MathStatus status;

    os_ClrHomeFull();
    if (!read_toggle("Piecewise? 0/1", &piecewise_mode)) {
        show_parse_error("Choose 0 for normal input or 1 for guided piecewise input.");
        return;
    }
    if (piecewise_mode != 0) {
        if (!read_piecewise_guided(&piecewise, error, sizeof(error))) {
            show_parse_error(error);
            return;
        }
        status = math_analyze_piecewise(&piecewise, &analysis);
        if (status != MATH_OK) {
            show_parse_error("The piecewise function could not be analyzed.");
            return;
        }
        show_analysis(&analysis);
        return;
    }
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
    print_wrapped("Exact interval rules cover polynomial degree 0 through 3 and selected rational, radical, and composition domains.");
    print_wrapped("More tools includes composition, average rate, and quadratic fit.");
    os_PutStrFull("Press any key");
    wait_for_key();
}

static void draw_menu(uint8_t selected) {
    uint8_t index;
    os_ClrHomeFull();
    os_PutStrFull("MATHAPP");
    os_NewLine();
    {
        uint8_t first = selected >= 6 ? selected - 5 : 0;
        uint8_t last = first + 6;
        if (last > MENU_COUNT) {
            last = MENU_COUNT;
        }
        for (index = first; index < last; index++) {
            os_PutStrFull(index == selected ? "> " : "  ");
            os_PutStrFull(menu_items[index]);
            os_NewLine();
        }
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
            } else if (selected == 5) {
                run_more_tools();
            } else {
                show_about();
            }
        } else if (key == sk_Clear) {
            return 0;
        }
    }
}
