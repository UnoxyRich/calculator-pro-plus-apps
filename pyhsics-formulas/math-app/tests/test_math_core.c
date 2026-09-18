#include "math_core.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static MathExpr parse_expression(const char *input) {
    MathExpr expression;
    char error[MATH_ERROR_WIDTH];
    MathStatus status = math_parse_expression(input, &expression, error, sizeof(error));
    if (status != MATH_OK) {
        fprintf(stderr, "Could not parse %s: %s\n", input, error);
    }
    assert(status == MATH_OK);
    return expression;
}

static MathEquation parse_equation(const char *input) {
    MathEquation equation;
    char error[MATH_ERROR_WIDTH];
    MathStatus status = math_parse_equation(input, &equation, error, sizeof(error));
    if (status != MATH_OK) {
        fprintf(stderr, "Could not parse %s: %s\n", input, error);
    }
    assert(status == MATH_OK);
    return equation;
}

static int steps_contain(const MathSteps *steps, const char *text) {
    uint8_t index;
    for (index = 0; index < steps->count; index++) {
        if (strstr(steps->lines[index], text) != NULL) {
            return 1;
        }
    }
    return 0;
}

static void test_fractions_and_parser(void) {
    MathFraction value;
    MathExpr expression = parse_expression("3(2x+3)^2+4x+6");
    char text[256];

    assert(math_fraction_parse("6/8", 3, &value));
    assert(value.num == 3 && value.den == 4);
    assert(math_expression_to_text(&expression, text, sizeof(text)) == MATH_OK);
    assert(strstr(text, "x") != NULL);
    assert(strstr(text, "^2") != NULL);
}

static void test_automatic_input_classification(void) {
    assert(math_classify_input("x^2+1") == MATH_INPUT_FUNCTION);
    assert(math_classify_input("  x^2+1=5  ") == MATH_INPUT_EQUATION);
    assert(math_classify_input(
               " piecewise(x|-2<x<0;-x|0<x<2) ") == MATH_INPUT_PIECEWISE);
}

static void test_complex_quadratic(void) {
    MathSolveResult result;
    MathEquation equation = parse_equation("x^2+2x+3=0");

    assert(math_solve_equation(&equation, &result) == MATH_OK);
    assert(strcmp(result.method, "Quadratic formula") == 0);
    assert(strcmp(result.answer, "x=-1+/-i*sqrt(2)") == 0);
    assert(result.steps.count >= 5);
}

static void test_rational_restrictions(void) {
    MathAnalysis analysis;
    MathSolveResult result;
    MathExpr expression = parse_expression("(x^2-1)/(x-1)");
    MathExpr constant_with_hole = parse_expression("(x-1)/(x-1)");
    MathEquation forbidden = parse_equation("x/(x-1)=1/(x-1)");
    MathEquation hole_root = parse_equation("(x^2-1)/(x-1)=0");
    MathEquation identity_with_hole = parse_equation("(x-1)/(x-1)=1");

    assert(math_analyze_function(&expression, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,1) U (1,infinity)") == 0);
    assert(strcmp(analysis.range, "(-infinity,2) U (2,infinity)") == 0);
    assert(strcmp(analysis.parity, "Neither (domain is not symmetric)") == 0);
    assert(math_analyze_function(&constant_with_hole, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,1) U (1,infinity)") == 0);
    assert(strcmp(analysis.range, "{1}") == 0);
    assert(math_solve_equation(&forbidden, &result) == MATH_OK);
    assert(strcmp(result.answer, "No solutions") == 0);
    assert(math_solve_equation(&hole_root, &result) == MATH_OK);
    assert(strcmp(result.answer, "x=-1") == 0);
    assert(math_solve_equation(&identity_with_hole, &result) == MATH_OK);
    assert(strcmp(result.answer, "x in (-infinity,1) U (1,infinity)") == 0);
}

static void test_function_analysis(void) {
    MathAnalysis analysis;
    MathExpr even_function = parse_expression("x^3/x");
    MathExpr plain_even = parse_expression("x^2+1");
    MathExpr plain_odd = parse_expression("x^3");
    MathExpr plain_neither = parse_expression("x^2+x");
    MathExpr radical = parse_expression("sqrt(x^2)");
    MathExpr shifted_radical = parse_expression("sqrt(x^2+1)");
    MathExpr quadratic = parse_expression("x^2-4x+1");

    assert(math_analyze_function(&even_function, &analysis) == MATH_OK);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(math_analyze_function(&radical, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,infinity)") == 0);
    assert(strcmp(analysis.range, "[0,infinity)") == 0);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(strcmp(analysis.increasing, "(0,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "(-infinity,0)") == 0);
    assert(math_analyze_function(&shifted_radical, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,infinity)") == 0);
    assert(strcmp(analysis.range, "[1,infinity)") == 0);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(strcmp(analysis.increasing, "(0,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "(-infinity,0)") == 0);
    assert(strcmp(analysis.concave_up, "(-infinity,infinity)") == 0);
    {
        MathExpr radical_domain_gap = parse_expression("sqrt(x^2-1)");
        assert(math_analyze_function(&radical_domain_gap, &analysis) == MATH_OK);
        assert(strcmp(analysis.domain, "(-infinity,-1] U [1,infinity)") == 0);
        assert(strcmp(analysis.range, "[0,infinity)") == 0);
        assert(strcmp(analysis.increasing, "(1,infinity)") == 0);
        assert(strcmp(analysis.decreasing, "(-infinity,-1)") == 0);
        assert(strcmp(analysis.concave_down,
                      "(-infinity,-1) U (1,infinity)") == 0);
        assert(strcmp(analysis.parity, "Even") == 0);
    }
    {
        MathExpr irrational_radical_gap = parse_expression("sqrt(x^2-2)");
        assert(math_analyze_function(&irrational_radical_gap, &analysis) == MATH_OK);
        assert(strcmp(analysis.domain,
                      "(-infinity,-sqrt(2)] U [sqrt(2),infinity)") == 0);
        assert(strcmp(analysis.increasing, "(sqrt(2),infinity)") == 0);
        assert(strcmp(analysis.decreasing, "(-infinity,-sqrt(2))") == 0);
    }
    assert(math_analyze_function(&quadratic, &analysis) == MATH_OK);
    assert(strcmp(analysis.increasing, "(2,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "(-infinity,2)") == 0);
    assert(strcmp(analysis.concave_up, "(-infinity,infinity)") == 0);
    assert(math_analyze_function(&plain_even, &analysis) == MATH_OK);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(math_analyze_function(&plain_odd, &analysis) == MATH_OK);
    assert(strcmp(analysis.parity, "Odd") == 0);
    assert(math_analyze_function(&plain_neither, &analysis) == MATH_OK);
    assert(strcmp(analysis.parity, "Neither") == 0);
}

static void test_special_interval_analysis(void) {
    MathAnalysis analysis;
    MathExpr absolute = parse_expression("abs(x-2)");
    MathExpr shifted_absolute = parse_expression("abs(x-2)+3");
    MathExpr absolute_pair = parse_expression("abs(x+2)+abs(x-2)");
    MathExpr radical = parse_expression("sqrt(x+1)");
    MathExpr shifted_sqrt = parse_expression("sqrt(x+1)+2");
    MathExpr nested_radical = parse_expression("sqrt(x)+1");
    MathExpr rational = parse_expression("1/(x-2)");

    assert(math_analyze_function(&absolute, &analysis) == MATH_OK);
    assert(strcmp(analysis.parity, "Neither") == 0);
    assert(strcmp(analysis.increasing, "(2,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "(-infinity,2)") == 0);
    assert(strcmp(analysis.concave_up, "none") == 0);
    assert(strcmp(analysis.concave_down, "none") == 0);
    assert(steps_contain(&analysis.steps, "Parity"));

    assert(math_analyze_function(&shifted_absolute, &analysis) == MATH_OK);
    assert(strcmp(analysis.range, "[3,infinity)") == 0);
    assert(strcmp(analysis.parity, "Neither") == 0);
    assert(strcmp(analysis.increasing, "(2,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "(-infinity,2)") == 0);

    assert(math_analyze_function(&shifted_sqrt, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "[-1,infinity)") == 0);
    assert(strcmp(analysis.range, "[2,infinity)") == 0);
    assert(strcmp(analysis.increasing, "(-1,infinity)") == 0);
    assert(strcmp(analysis.concave_down, "(-1,infinity)") == 0);
    assert(strcmp(analysis.parity, "Neither (domain is not symmetric)") == 0);

    {
        MathExpr lowered_sqrt = parse_expression("sqrt(x+1)-2");
        MathExpr reflected_sqrt = parse_expression("2-sqrt(x+1)");
        MathExpr lowered_absolute = parse_expression("abs(x-2)-3");
        MathExpr reflected_absolute = parse_expression("-abs(x-2)");

        assert(math_analyze_function(&lowered_sqrt, &analysis) == MATH_OK);
        assert(strcmp(analysis.range, "[-2,infinity)") == 0);
        assert(strcmp(analysis.increasing, "(-1,infinity)") == 0);
        assert(strcmp(analysis.concave_down, "(-1,infinity)") == 0);
        assert(steps_contain(&analysis.steps, "vertical shift"));

        assert(math_analyze_function(&reflected_sqrt, &analysis) == MATH_OK);
        assert(strcmp(analysis.range, "(-infinity,2]") == 0);
        assert(strcmp(analysis.decreasing, "(-1,infinity)") == 0);
        assert(strcmp(analysis.concave_up, "(-1,infinity)") == 0);

        assert(math_analyze_function(&lowered_absolute, &analysis) == MATH_OK);
        assert(strcmp(analysis.range, "[-3,infinity)") == 0);
        assert(strcmp(analysis.parity, "Neither") == 0);

        assert(math_analyze_function(&reflected_absolute, &analysis) == MATH_OK);
        assert(strcmp(analysis.range, "(-infinity,0]") == 0);
        assert(strcmp(analysis.increasing, "(-infinity,2)") == 0);
        assert(strcmp(analysis.decreasing, "(2,infinity)") == 0);
    }

    assert(math_analyze_function(&absolute_pair, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,infinity)") == 0);
    assert(strcmp(analysis.range, "[4,infinity)") == 0);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(strcmp(analysis.increasing, "(2,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "(-infinity,-2)") == 0);
    assert(steps_contain(&analysis.steps, "Parity"));

    assert(math_analyze_function(&radical, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "[-1,infinity)") == 0);
    assert(strcmp(analysis.increasing, "(-1,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "none") == 0);
    assert(strcmp(analysis.concave_down, "(-1,infinity)") == 0);

    assert(math_analyze_function(&nested_radical, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "[0,infinity)") == 0);
    assert(strcmp(analysis.range, "[1,infinity)") == 0);
    assert(strcmp(analysis.increasing, "(0,infinity)") == 0);

    assert(math_analyze_function(&rational, &analysis) == MATH_OK);
    assert(strcmp(analysis.increasing, "none") == 0);
    assert(strcmp(analysis.decreasing, "(-infinity,2) U (2,infinity)") == 0);
    assert(strcmp(analysis.concave_up, "(2,infinity)") == 0);
    assert(strcmp(analysis.concave_down, "(-infinity,2)") == 0);
}

static void test_transform(void) {
    MathTransformSpec spec;
    MathTransformResult result;
    MathExpr expression = parse_expression("x^2-3x-4");

    spec.horizontal_shift = math_fraction(0, 1);
    spec.vertical_shift = math_fraction(0, 1);
    spec.horizontal_scale = math_fraction(1, 2);
    spec.vertical_scale = math_fraction(1, 2);
    spec.reflect_x_axis = 0;
    spec.reflect_y_axis = 0;
    assert(math_transform_quadratic(&expression, &spec, &result) == MATH_OK);
    assert(strcmp(result.formula, "2*x^2-3*x-2") == 0);
    assert(strcmp(result.vertex, "(3/4,-25/8)") == 0);
    assert(strcmp(result.x_intercepts, "(-1/2,0),(2,0)") == 0);
    assert(strcmp(result.y_intercept, "(0,-2)") == 0);

    spec.horizontal_shift = math_fraction(0, 1);
    spec.vertical_shift = math_fraction(0, 1);
    spec.horizontal_scale = math_fraction(1, 1);
    spec.vertical_scale = math_fraction(1, 1);
    spec.reflect_x_axis = 1;
    spec.reflect_y_axis = 0;
    assert(math_transform_quadratic(&expression, &spec, &result) == MATH_OK);
    assert(strcmp(result.formula, "-x^2+3*x+4") == 0);

    expression = parse_expression("x^2-3x-4");
    spec.horizontal_shift = math_fraction(0, 1);
    spec.vertical_shift = math_fraction(0, 1);
    spec.horizontal_scale = math_fraction(1, 1);
    spec.vertical_scale = math_fraction(1, 1);
    spec.reflect_x_axis = 0;
    spec.reflect_y_axis = 1;
    assert(math_transform_quadratic(&expression, &spec, &result) == MATH_OK);
    assert(strcmp(result.formula, "x^2+3*x-4") == 0);

    expression = parse_expression("x^2");
    spec.horizontal_shift = math_fraction(2, 1);
    spec.vertical_shift = math_fraction(3, 1);
    spec.horizontal_scale = math_fraction(1, 1);
    spec.vertical_scale = math_fraction(1, 1);
    spec.reflect_x_axis = 0;
    spec.reflect_y_axis = 0;
    assert(math_transform_quadratic(&expression, &spec, &result) == MATH_OK);
    assert(strcmp(result.formula, "x^2-4*x+7") == 0);
    assert(strcmp(result.vertex, "(2,3)") == 0);
    assert(strcmp(result.y_intercept, "(0,7)") == 0);

    expression = parse_expression("2x+1");
    spec.horizontal_shift = math_fraction(1, 1);
    spec.vertical_shift = math_fraction(2, 1);
    spec.horizontal_scale = math_fraction(2, 1);
    spec.vertical_scale = math_fraction(3, 1);
    assert(math_transform_quadratic(&expression, &spec, &result) == MATH_OK);
    assert(strcmp(result.formula, "3*x+2") == 0);
    assert(strcmp(result.vertex, "none (not quadratic)") == 0);
    assert(strcmp(result.x_intercepts, "(-2/3,0)") == 0);
    assert(strcmp(result.y_intercept, "(0,2)") == 0);

    expression = parse_expression("5");
    spec.horizontal_shift = math_fraction(-1, 1);
    spec.vertical_shift = math_fraction(2, 1);
    spec.horizontal_scale = math_fraction(3, 1);
    spec.vertical_scale = math_fraction(-2, 1);
    spec.reflect_x_axis = 0;
    spec.reflect_y_axis = 0;
    assert(math_transform_quadratic(&expression, &spec, &result) == MATH_OK);
    assert(strcmp(result.formula, "-8") == 0);
    assert(strcmp(result.x_intercepts, "none") == 0);
}

static void test_piecewise_analysis(void) {
    MathPiecewise piecewise;
    MathPiecewise condition_piecewise;
    MathAnalysis analysis;
    char error[MATH_ERROR_WIDTH];
    MathStatus status = math_parse_piecewise(
        "piecewise(x|(-2,0);-x|(0,2))", &piecewise, error, sizeof(error));

    if (status != MATH_OK) {
        fprintf(stderr, "Could not parse piecewise input: %s\n", error);
    }
    assert(status == MATH_OK);
    assert(piecewise.count == 2);
    assert(math_analyze_piecewise(&piecewise, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-2,0) U (0,2)") == 0);
    assert(strcmp(analysis.range, "(-2,0)") == 0);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(strcmp(analysis.increasing, "(-2,0)") == 0);
    assert(strcmp(analysis.decreasing, "(0,2)") == 0);
    assert(math_parse_piecewise(
               "piecewise(x|-2<x<0;-x|0<x<2)",
               &condition_piecewise, error, sizeof(error)) == MATH_OK);
    assert(math_analyze_piecewise(&condition_piecewise, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-2,0) U (0,2)") == 0);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(math_parse_piecewise(
               "piecewise(x|-2<=x<0;-x|0<=x<2)",
               &condition_piecewise, error, sizeof(error)) == MATH_OK);
    assert(math_analyze_piecewise(&condition_piecewise, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "[-2,0) U [0,2)") == 0);
}

static void test_special_equations(void) {
    MathSolveResult result;
    MathEquation linear = parse_equation("2x+3=7");
    MathEquation identity = parse_equation("2x+1=2x+1");
    MathEquation contradiction = parse_equation("2x=2x+1");
    MathEquation radical = parse_equation("sqrt(2x-3)=1");
    MathEquation radical_with_linear = parse_equation("sqrt(2x-3)+x=1");
    MathEquation absolute = parse_equation("abs(x-3)=2");

    assert(math_solve_equation(&linear, &result) == MATH_OK);
    assert(strcmp(result.method, "Linear isolation") == 0);
    assert(strcmp(result.answer, "x=2") == 0);
    assert(math_solve_equation(&identity, &result) == MATH_OK);
    assert(strcmp(result.answer, "All real numbers") == 0);
    assert(math_solve_equation(&contradiction, &result) == MATH_OK);
    assert(strcmp(result.answer, "No solutions") == 0);
    assert(math_solve_equation(&radical, &result) == MATH_OK);
    assert(strcmp(result.method, "Square and verify") == 0);
    assert(strcmp(result.answer, "x=2") == 0);
    assert(math_solve_equation(&radical_with_linear, &result) == MATH_OK);
    assert(strcmp(result.answer, "No solutions") == 0);
    assert(math_solve_equation(&absolute, &result) == MATH_OK);
    assert(strcmp(result.method, "Absolute-value cases") == 0);
    assert(strcmp(result.answer, "x=1 or x=5") == 0);
}

static void test_radical_formatting_and_edge_cases(void) {
    MathSolveResult result;
    MathEquation rational_radical = parse_equation("x^2-1/2=0");
    MathEquation complex_rational = parse_equation("x^2+1/2=0");
    MathEquation repeated_root = parse_equation("x^2-2x+1=0");

    assert(math_solve_equation(&rational_radical, &result) == MATH_OK);
    assert(strstr(result.answer, "sqrt(2)/2") != NULL);
    assert(math_solve_equation(&complex_rational, &result) == MATH_OK);
    assert(strcmp(result.answer, "x=+/-i*sqrt(2)/2") == 0);
    assert(math_solve_equation(&repeated_root, &result) == MATH_OK);
    assert(strcmp(result.answer, "x=1") == 0);
}

static void test_more_analysis_and_piecewise_validation(void) {
    MathAnalysis analysis;
    MathExpr linear = parse_expression("2x+1");
    MathExpr absolute = parse_expression("abs(x-2)");
    MathPiecewise piecewise;
    char error[MATH_ERROR_WIDTH];

    assert(math_analyze_function(&linear, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,infinity)") == 0);
    assert(strcmp(analysis.range, "(-infinity,infinity)") == 0);
    assert(strcmp(analysis.increasing, "(-infinity,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "none") == 0);
    assert(math_analyze_function(&absolute, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,infinity)") == 0);
    assert(strcmp(analysis.range, "[0,infinity)") == 0);

    assert(math_parse_piecewise("piecewise(x|[-2,0);-x|(0,2])",
                                &piecewise, error, sizeof(error)) == MATH_OK);
    assert(math_analyze_piecewise(&piecewise, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "[-2,0) U (0,2]") == 0);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(math_parse_piecewise("piecewise(x|[0,1];x|[1,2])",
                                &piecewise, error, sizeof(error)) == MATH_ERR_SYNTAX);
    assert(math_parse_piecewise("piecewise(x|[0,1])",
                                &piecewise, error, sizeof(error)) == MATH_OK);
    assert(math_analyze_piecewise(&piecewise, &analysis) == MATH_OK);
    assert(strcmp(analysis.parity, "Neither (domain is not symmetric)") == 0);
}

int main(void) {
    test_fractions_and_parser();
    test_automatic_input_classification();
    test_complex_quadratic();
    test_rational_restrictions();
    test_function_analysis();
    test_special_interval_analysis();
    test_transform();
    test_piecewise_analysis();
    test_special_equations();
    test_radical_formatting_and_edge_cases();
    test_more_analysis_and_piecewise_validation();
    puts("math_core tests passed");
    return 0;
}
