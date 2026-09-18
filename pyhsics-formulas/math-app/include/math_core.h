#ifndef MATH_CORE_H
#define MATH_CORE_H

#include <stddef.h>
#include <stdint.h>

#define MATH_EXPR_CAP 192
#define MATH_POLY_CAP 5
#define MATH_MAX_STEPS 12
#define MATH_STEP_WIDTH 128
#define MATH_OUTPUT_WIDTH 256
#define MATH_ERROR_WIDTH 96
#define MATH_PIECEWISE_BRANCHES 4

typedef struct {
    int32_t num;
    int32_t den;
} MathFraction;

typedef enum {
    MATH_NODE_NUMBER,
    MATH_NODE_VARIABLE,
    MATH_NODE_IMAGINARY,
    MATH_NODE_ADD,
    MATH_NODE_SUBTRACT,
    MATH_NODE_MULTIPLY,
    MATH_NODE_DIVIDE,
    MATH_NODE_POWER,
    MATH_NODE_NEGATE,
    MATH_NODE_SQRT,
    MATH_NODE_ABS
} MathNodeType;

typedef struct {
    MathNodeType type;
    int16_t left;
    int16_t right;
    int16_t exponent;
    MathFraction value;
} MathNode;

typedef struct {
    MathNode nodes[MATH_EXPR_CAP];
    uint16_t count;
    int16_t root;
} MathExpr;

typedef struct {
    MathExpr left;
    MathExpr right;
} MathEquation;

typedef struct {
    MathExpr expression;
    MathFraction lower;
    MathFraction upper;
    uint8_t lower_closed;
    uint8_t upper_closed;
} MathPiecewiseBranch;

typedef struct {
    MathPiecewiseBranch branch[MATH_PIECEWISE_BRANCHES];
    uint8_t count;
} MathPiecewise;

typedef struct {
    char lines[MATH_MAX_STEPS][MATH_STEP_WIDTH];
    uint8_t count;
} MathSteps;

typedef enum {
    MATH_OK = 0,
    MATH_ERR_SYNTAX,
    MATH_ERR_LIMIT,
    MATH_ERR_UNSUPPORTED,
    MATH_ERR_DIVISION_BY_ZERO,
    MATH_ERR_NO_SOLUTION,
    MATH_ERR_INPUT
} MathStatus;

typedef enum {
    MATH_INPUT_FUNCTION,
    MATH_INPUT_EQUATION,
    MATH_INPUT_PIECEWISE
} MathInputKind;

typedef struct {
    char method[48];
    char answer[MATH_OUTPUT_WIDTH];
    char warning[128];
    MathSteps steps;
    MathStatus status;
} MathSolveResult;

typedef struct {
    char domain[MATH_OUTPUT_WIDTH];
    char range[MATH_OUTPUT_WIDTH];
    char parity[96];
    char increasing[MATH_OUTPUT_WIDTH];
    char decreasing[MATH_OUTPUT_WIDTH];
    char concave_up[MATH_OUTPUT_WIDTH];
    char concave_down[MATH_OUTPUT_WIDTH];
    char warning[128];
    MathSteps steps;
    MathStatus status;
} MathAnalysis;

typedef struct {
    MathFraction horizontal_shift;
    MathFraction vertical_shift;
    MathFraction horizontal_scale;
    MathFraction vertical_scale;
    uint8_t reflect_x_axis;
    uint8_t reflect_y_axis;
} MathTransformSpec;

typedef struct {
    char formula[MATH_OUTPUT_WIDTH];
    char vertex_form[MATH_OUTPUT_WIDTH];
    char vertex[96];
    char x_intercepts[MATH_OUTPUT_WIDTH];
    char y_intercept[96];
    char warning[128];
    MathSteps steps;
    MathStatus status;
} MathTransformResult;

typedef struct {
    char expression[MATH_OUTPUT_WIDTH];
    char domain[MATH_OUTPUT_WIDTH];
    char range[MATH_OUTPUT_WIDTH];
    char warning[128];
    MathSteps steps;
    MathStatus status;
} MathCompositionResult;

typedef struct {
    char answer[96];
    char warning[128];
    MathSteps steps;
    MathStatus status;
} MathAverageRateResult;

typedef struct {
    char equation[MATH_OUTPUT_WIDTH];
    char coefficient_a[64];
    char coefficient_b[64];
    char coefficient_c[64];
    char warning[128];
    MathSteps steps;
    MathStatus status;
} MathRegressionResult;

typedef struct {
    int degree;
    MathFraction coefficient[MATH_POLY_CAP];
} MathPolynomial;

MathFraction math_fraction(int32_t numerator, int32_t denominator);
MathFraction math_fraction_zero(void);
MathFraction math_fraction_one(void);
int math_fraction_is_zero(MathFraction value);
int math_fraction_equal(MathFraction left, MathFraction right);
int math_fraction_compare(MathFraction left, MathFraction right);
int math_fraction_to_text(MathFraction value, char *out, size_t capacity);
int math_fraction_parse(const char *text, size_t length, MathFraction *out);

MathStatus math_parse_expression(const char *input, MathExpr *out,
                                 char *error, size_t error_capacity);
int math_normalize_input(const char *input, char *out, size_t capacity);
MathStatus math_parse_equation(const char *input, MathEquation *out,
                               char *error, size_t error_capacity);
MathStatus math_parse_piecewise(const char *input, MathPiecewise *out,
                                char *error, size_t error_capacity);
MathInputKind math_classify_input(const char *input);
MathStatus math_expression_to_text(const MathExpr *expr, char *out,
                                   size_t capacity);

MathStatus math_solve_equation(const MathEquation *equation,
                               MathSolveResult *out);
MathStatus math_analyze_function(const MathExpr *expression,
                                 MathAnalysis *out);
MathStatus math_analyze_piecewise(const MathPiecewise *piecewise,
                                  MathAnalysis *out);
MathStatus math_transform_quadratic(const MathExpr *expression,
                                    const MathTransformSpec *spec,
                                    MathTransformResult *out);
MathStatus math_compose_functions(const MathExpr *outer,
                                  const MathExpr *inner,
                                  MathCompositionResult *out);
MathStatus math_compose_three_functions(const MathExpr *outer,
                                        const MathExpr *middle,
                                        const MathExpr *inner,
                                        MathCompositionResult *out);
MathStatus math_average_rate(const MathExpr *expression,
                             MathFraction first_x, MathFraction second_x,
                             MathAverageRateResult *out);
MathStatus math_average_rate_piecewise(const MathPiecewise *piecewise,
                                        MathFraction first_x,
                                        MathFraction second_x,
                                        MathAverageRateResult *out);
MathStatus math_quadratic_regression(const MathFraction *x_values,
                                     const MathFraction *y_values,
                                     size_t count,
                                     MathRegressionResult *out);

MathStatus math_expression_to_polynomial(const MathExpr *expression,
                                         MathPolynomial *out);

#endif
