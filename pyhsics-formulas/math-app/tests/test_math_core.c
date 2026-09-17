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
    MathEquation forbidden = parse_equation("x/(x-1)=1/(x-1)");

    assert(math_analyze_function(&expression, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,1) U (1,infinity)") == 0);
    assert(strcmp(analysis.range, "(-infinity,2) U (2,infinity)") == 0);
    assert(strcmp(analysis.parity, "Neither (domain is not symmetric)") == 0);
    assert(math_solve_equation(&forbidden, &result) == MATH_OK);
    assert(strcmp(result.answer, "No solutions") == 0);
}

static void test_function_analysis(void) {
    MathAnalysis analysis;
    MathExpr even_function = parse_expression("x^3/x");
    MathExpr radical = parse_expression("sqrt(x^2)");
    MathExpr quadratic = parse_expression("x^2-4x+1");

    assert(math_analyze_function(&even_function, &analysis) == MATH_OK);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(math_analyze_function(&radical, &analysis) == MATH_OK);
    assert(strcmp(analysis.domain, "(-infinity,infinity)") == 0);
    assert(strcmp(analysis.range, "[0,infinity)") == 0);
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(math_analyze_function(&quadratic, &analysis) == MATH_OK);
    assert(strcmp(analysis.increasing, "(2,infinity)") == 0);
    assert(strcmp(analysis.decreasing, "(-infinity,2)") == 0);
    assert(strcmp(analysis.concave_up, "(-infinity,infinity)") == 0);
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
}

static void test_piecewise_analysis(void) {
    MathPiecewise piecewise;
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
    assert(strcmp(analysis.parity, "Even") == 0);
    assert(strcmp(analysis.increasing, "(-2,0)") == 0);
    assert(strcmp(analysis.decreasing, "(0,2)") == 0);
}

static void test_special_equations(void) {
    MathSolveResult result;
    MathEquation radical = parse_equation("sqrt(2x-3)=1");
    MathEquation radical_with_linear = parse_equation("sqrt(2x-3)+x=1");
    MathEquation absolute = parse_equation("abs(x-3)=2");

    assert(math_solve_equation(&radical, &result) == MATH_OK);
    assert(strcmp(result.method, "Square and verify") == 0);
    assert(strcmp(result.answer, "x=2") == 0);
    assert(math_solve_equation(&radical_with_linear, &result) == MATH_OK);
    assert(strcmp(result.answer, "No solutions") == 0);
    assert(math_solve_equation(&absolute, &result) == MATH_OK);
    assert(strcmp(result.method, "Absolute-value cases") == 0);
    assert(strcmp(result.answer, "x=1 or x=5") == 0);
}

int main(void) {
    test_fractions_and_parser();
    test_complex_quadratic();
    test_rational_restrictions();
    test_function_analysis();
    test_transform();
    test_piecewise_analysis();
    test_special_equations();
    puts("math_core tests passed");
    return 0;
}
