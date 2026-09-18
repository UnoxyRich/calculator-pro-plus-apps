#include "math_core.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *cursor;
    MathExpr *expression;
    char *error;
    size_t error_capacity;
} MathParser;

typedef struct {
    int degree;
    MathFraction coefficient[MATH_POLY_CAP];
} MathPoly;

typedef struct {
    MathPoly numerator;
    MathPoly denominator;
    int valid;
} MathRational;

typedef struct {
    MathFraction real;
    MathFraction imaginary;
} MathComplexFraction;

typedef struct {
    int degree;
    MathComplexFraction coefficient[MATH_POLY_CAP];
} MathComplexPoly;

static int64_t absolute_i64(int64_t value) {
    return value < 0 ? -value : value;
}

static int64_t greatest_common_divisor(int64_t left, int64_t right) {
    left = absolute_i64(left);
    right = absolute_i64(right);
    while (right != 0) {
        int64_t remainder = left % right;
        left = right;
        right = remainder;
    }
    return left == 0 ? 1 : left;
}

static MathFraction fraction_from_i64(int64_t numerator, int64_t denominator) {
    MathFraction result;
    int64_t divisor;

    if (denominator == 0) {
        result.num = 0;
        result.den = 0;
        return result;
    }
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    divisor = greatest_common_divisor(numerator, denominator);
    numerator /= divisor;
    denominator /= divisor;
    if (numerator > INT32_MAX || numerator < INT32_MIN || denominator > INT32_MAX) {
        result.num = 0;
        result.den = 0;
        return result;
    }
    result.num = (int32_t) numerator;
    result.den = (int32_t) denominator;
    return result;
}

MathFraction math_fraction(int32_t numerator, int32_t denominator) {
    return fraction_from_i64(numerator, denominator);
}

MathFraction math_fraction_zero(void) {
    return fraction_from_i64(0, 1);
}

MathFraction math_fraction_one(void) {
    return fraction_from_i64(1, 1);
}

static MathFraction fraction_negate(MathFraction value) {
    return fraction_from_i64(-(int64_t) value.num, value.den);
}

static MathFraction fraction_add(MathFraction left, MathFraction right) {
    return fraction_from_i64((int64_t) left.num * right.den
                             + (int64_t) right.num * left.den,
                             (int64_t) left.den * right.den);
}

static MathFraction fraction_subtract(MathFraction left, MathFraction right) {
    return fraction_from_i64((int64_t) left.num * right.den
                             - (int64_t) right.num * left.den,
                             (int64_t) left.den * right.den);
}

static MathFraction fraction_multiply(MathFraction left, MathFraction right) {
    return fraction_from_i64((int64_t) left.num * right.num,
                             (int64_t) left.den * right.den);
}

static MathFraction fraction_divide(MathFraction left, MathFraction right) {
    return fraction_from_i64((int64_t) left.num * right.den,
                             (int64_t) left.den * right.num);
}

static MathComplexFraction complex_fraction(MathFraction real,
                                            MathFraction imaginary) {
    MathComplexFraction result;
    result.real = real;
    result.imaginary = imaginary;
    return result;
}

static MathComplexFraction complex_zero(void) {
    return complex_fraction(math_fraction_zero(), math_fraction_zero());
}

static MathComplexFraction complex_one(void) {
    return complex_fraction(math_fraction_one(), math_fraction_zero());
}

static int complex_is_zero(MathComplexFraction value) {
    return math_fraction_is_zero(value.real)
        && math_fraction_is_zero(value.imaginary);
}

static int complex_is_real(MathComplexFraction value) {
    return math_fraction_is_zero(value.imaginary);
}

static MathComplexFraction complex_negate(MathComplexFraction value) {
    return complex_fraction(fraction_negate(value.real),
                            fraction_negate(value.imaginary));
}

static MathComplexFraction complex_add(MathComplexFraction left,
                                       MathComplexFraction right) {
    return complex_fraction(fraction_add(left.real, right.real),
                            fraction_add(left.imaginary, right.imaginary));
}

static MathComplexFraction complex_subtract(MathComplexFraction left,
                                            MathComplexFraction right) {
    return complex_fraction(fraction_subtract(left.real, right.real),
                            fraction_subtract(left.imaginary, right.imaginary));
}

static MathComplexFraction complex_multiply(MathComplexFraction left,
                                            MathComplexFraction right) {
    MathFraction real = fraction_subtract(
        fraction_multiply(left.real, right.real),
        fraction_multiply(left.imaginary, right.imaginary));
    MathFraction imaginary = fraction_add(
        fraction_multiply(left.real, right.imaginary),
        fraction_multiply(left.imaginary, right.real));
    return complex_fraction(real, imaginary);
}

static MathComplexFraction complex_divide(MathComplexFraction left,
                                          MathComplexFraction right) {
    MathFraction denominator = fraction_add(
        fraction_multiply(right.real, right.real),
        fraction_multiply(right.imaginary, right.imaginary));
    MathFraction real;
    MathFraction imaginary;
    if (math_fraction_is_zero(denominator)) {
        return complex_fraction(math_fraction(0, 0), math_fraction(0, 0));
    }
    real = fraction_divide(
        fraction_add(fraction_multiply(left.real, right.real),
                     fraction_multiply(left.imaginary, right.imaginary)),
        denominator);
    imaginary = fraction_divide(
        fraction_subtract(fraction_multiply(left.imaginary, right.real),
                          fraction_multiply(left.real, right.imaginary)),
        denominator);
    return complex_fraction(real, imaginary);
}

int math_fraction_is_zero(MathFraction value) {
    return value.num == 0 && value.den != 0;
}

int math_fraction_equal(MathFraction left, MathFraction right) {
    if (left.den == 0 || right.den == 0) {
        return 0;
    }
    return left.num == right.num && left.den == right.den;
}

int math_fraction_compare(MathFraction left, MathFraction right) {
    int64_t difference = (int64_t) left.num * right.den
                       - (int64_t) right.num * left.den;
    return difference < 0 ? -1 : difference > 0 ? 1 : 0;
}

static MathFraction fraction_absolute(MathFraction value) {
    return value.num < 0 ? fraction_negate(value) : value;
}

static int append_text(char *out, size_t capacity, size_t *used,
                       const char *format, ...) {
    va_list arguments;
    int written;

    if (*used >= capacity) {
        return 0;
    }
    va_start(arguments, format);
    written = vsnprintf(out + *used, capacity - *used, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t) written >= capacity - *used) {
        if (capacity > 0) {
            out[capacity - 1] = '\0';
        }
        return 0;
    }
    *used += (size_t) written;
    return 1;
}

int math_fraction_to_text(MathFraction value, char *out, size_t capacity) {
    if (capacity == 0 || value.den == 0) {
        return 0;
    }
    if (value.den == 1) {
        return snprintf(out, capacity, "%ld", (long) value.num) >= 0
            && strlen(out) < capacity;
    }
    return snprintf(out, capacity, "%ld/%ld", (long) value.num,
                    (long) value.den) >= 0 && strlen(out) < capacity;
}

static int parse_integer_segment(const char *text, size_t length,
                                 int64_t *value, size_t *used) {
    size_t index = 0;
    int sign = 1;
    int64_t result = 0;

    while (index < length && isspace((unsigned char) text[index])) {
        index++;
    }
    if (index < length && (text[index] == '+' || text[index] == '-')) {
        if (text[index] == '-') {
            sign = -1;
        }
        index++;
    }
    if (index == length || !isdigit((unsigned char) text[index])) {
        return 0;
    }
    while (index < length && isdigit((unsigned char) text[index])) {
        result = result * 10 + (text[index] - '0');
        index++;
    }
    while (index < length && isspace((unsigned char) text[index])) {
        index++;
    }
    *value = sign * result;
    *used = index;
    return 1;
}

int math_fraction_parse(const char *text, size_t length, MathFraction *out) {
    int64_t numerator;
    int64_t denominator;
    size_t used;
    size_t slash;

    if (text == NULL || out == NULL) {
        return 0;
    }
    slash = 0;
    while (slash < length && text[slash] != '/') {
        slash++;
    }
    if (slash < length) {
        size_t denominator_used;
        if (!parse_integer_segment(text, slash, &numerator, &used)
                || used != slash
                || !parse_integer_segment(text + slash + 1, length - slash - 1,
                                           &denominator, &denominator_used)
                || denominator_used != length - slash - 1) {
            return 0;
        }
        *out = fraction_from_i64(numerator, denominator);
        return out->den != 0;
    }
    if (parse_integer_segment(text, length, &numerator, &used) && used == length) {
        *out = fraction_from_i64(numerator, 1);
        return out->den != 0;
    }
    return 0;
}

static void parser_error(MathParser *parser, const char *message) {
    if (parser->error != NULL && parser->error_capacity > 0
            && parser->error[0] == '\0') {
        snprintf(parser->error, parser->error_capacity, "%s", message);
    }
}

static void parser_skip_spaces(MathParser *parser) {
    while (isspace((unsigned char) *parser->cursor)) {
        parser->cursor++;
    }
}

static int parser_add_node(MathParser *parser, MathNode node) {
    if (parser->expression->count >= MATH_EXPR_CAP) {
        parser_error(parser, "Expression is too large");
        return -1;
    }
    parser->expression->nodes[parser->expression->count] = node;
    return parser->expression->count++;
}

static int parser_make_unary(MathParser *parser, MathNodeType type, int child) {
    MathNode node;
    node.type = type;
    node.left = (int16_t) child;
    node.right = -1;
    node.exponent = 0;
    node.value = math_fraction_zero();
    return parser_add_node(parser, node);
}

static int parser_make_binary(MathParser *parser, MathNodeType type,
                              int left, int right) {
    MathNode node;
    node.type = type;
    node.left = (int16_t) left;
    node.right = (int16_t) right;
    node.exponent = 0;
    node.value = math_fraction_zero();
    return parser_add_node(parser, node);
}

static int parser_make_number(MathParser *parser, MathFraction value) {
    MathNode node;
    node.type = MATH_NODE_NUMBER;
    node.left = -1;
    node.right = -1;
    node.exponent = 0;
    node.value = value;
    return parser_add_node(parser, node);
}

static int parser_make_variable(MathParser *parser) {
    MathNode node;
    node.type = MATH_NODE_VARIABLE;
    node.left = -1;
    node.right = -1;
    node.exponent = 0;
    node.value = math_fraction_zero();
    return parser_add_node(parser, node);
}

static int parser_make_imaginary(MathParser *parser) {
    MathNode node;
    node.type = MATH_NODE_IMAGINARY;
    node.left = -1;
    node.right = -1;
    node.exponent = 0;
    node.value = math_fraction_zero();
    return parser_add_node(parser, node);
}

static int parser_parse_add(MathParser *parser);

static int parser_parse_number(MathParser *parser) {
    int64_t whole = 0;
    int64_t decimal = 0;
    int64_t scale = 1;
    int sign = 1;
    int had_digit = 0;
    MathFraction value;

    if (*parser->cursor == '.') {
        parser->cursor++;
    } else {
        while (isdigit((unsigned char) *parser->cursor)) {
            whole = whole * 10 + (*parser->cursor - '0');
            parser->cursor++;
            had_digit = 1;
        }
        if (*parser->cursor == '.') {
            parser->cursor++;
        }
    }
    while (isdigit((unsigned char) *parser->cursor)) {
        had_digit = 1;
        if (scale < 1000000) {
            decimal = decimal * 10 + (*parser->cursor - '0');
            scale *= 10;
        }
        parser->cursor++;
    }
    if (!had_digit) {
        parser_error(parser, "Expected a number");
        return -1;
    }
    value = fraction_from_i64(sign * (whole * scale + decimal), scale);
    if (value.den == 0) {
        parser_error(parser, "Number is outside exact range");
        return -1;
    }
    return parser_make_number(parser, value);
}

static int is_identifier_character(char character) {
    return isalpha((unsigned char) character) || character == '_';
}

static int parser_parse_primary(MathParser *parser) {
    char identifier[8];
    size_t length = 0;
    int child;

    parser_skip_spaces(parser);
    if (*parser->cursor == '(') {
        parser->cursor++;
        child = parser_parse_add(parser);
        parser_skip_spaces(parser);
        if (*parser->cursor != ')') {
            parser_error(parser, "Missing closing parenthesis");
            return -1;
        }
        parser->cursor++;
        return child;
    }
    if (*parser->cursor == '|') {
        parser->cursor++;
        child = parser_parse_add(parser);
        parser_skip_spaces(parser);
        if (*parser->cursor != '|') {
            parser_error(parser, "Missing closing absolute-value bar");
            return -1;
        }
        parser->cursor++;
        return parser_make_unary(parser, MATH_NODE_ABS, child);
    }
    if (isdigit((unsigned char) *parser->cursor) || *parser->cursor == '.') {
        return parser_parse_number(parser);
    }
    if (!is_identifier_character(*parser->cursor)) {
        parser_error(parser, "Expected a number, x, or function");
        return -1;
    }
    while (is_identifier_character(*parser->cursor)) {
        if (length + 1 < sizeof(identifier)) {
            identifier[length++] = *parser->cursor;
        }
        parser->cursor++;
    }
    identifier[length] = '\0';
    if (strcmp(identifier, "x") == 0 || strcmp(identifier, "X") == 0) {
        return parser_make_variable(parser);
    }
    if (strcmp(identifier, "i") == 0 || strcmp(identifier, "I") == 0) {
        return parser_make_imaginary(parser);
    }
    if (strcmp(identifier, "sqrt") == 0 || strcmp(identifier, "abs") == 0) {
        MathNodeType type = strcmp(identifier, "sqrt") == 0
                          ? MATH_NODE_SQRT : MATH_NODE_ABS;
        parser_skip_spaces(parser);
        if (*parser->cursor != '(') {
            parser_error(parser, "Function needs parentheses");
            return -1;
        }
        parser->cursor++;
        child = parser_parse_add(parser);
        parser_skip_spaces(parser);
        if (*parser->cursor != ')') {
            parser_error(parser, "Missing function closing parenthesis");
            return -1;
        }
        parser->cursor++;
        return parser_make_unary(parser, type, child);
    }
    parser_error(parser, "Unknown name; use x, i, sqrt, or abs");
    return -1;
}

static int parser_parse_power(MathParser *parser) {
    int base;
    int sign = 1;
    int exponent = 0;
    MathNode node;

    base = parser_parse_primary(parser);
    if (base < 0) {
        return -1;
    }
    parser_skip_spaces(parser);
    if (*parser->cursor != '^') {
        return base;
    }
    parser->cursor++;
    parser_skip_spaces(parser);
    if (*parser->cursor == '+' || *parser->cursor == '-') {
        if (*parser->cursor == '-') {
            sign = -1;
        }
        parser->cursor++;
    }
    if (!isdigit((unsigned char) *parser->cursor)) {
        parser_error(parser, "Exponent must be an integer");
        return -1;
    }
    while (isdigit((unsigned char) *parser->cursor)) {
        exponent = exponent * 10 + (*parser->cursor - '0');
        parser->cursor++;
        if (exponent > 8) {
            parser_error(parser, "Exponent is outside the supported range");
            return -1;
        }
    }
    node.type = MATH_NODE_POWER;
    node.left = (int16_t) base;
    node.right = -1;
    node.exponent = (int16_t) (sign * exponent);
    node.value = math_fraction_zero();
    return parser_add_node(parser, node);
}

static int parser_parse_unary(MathParser *parser) {
    int child;
    parser_skip_spaces(parser);
    if (*parser->cursor == '-') {
        parser->cursor++;
        child = parser_parse_unary(parser);
        return child < 0 ? -1 : parser_make_unary(parser, MATH_NODE_NEGATE, child);
    }
    if (*parser->cursor == '+') {
        parser->cursor++;
        return parser_parse_unary(parser);
    }
    return parser_parse_power(parser);
}

static int parser_can_start_primary(char character) {
    return isdigit((unsigned char) character) || character == '.'
        || character == '(' || is_identifier_character(character);
}

/* os_GetStringInput returns the TI-ASCII display form of calculator keys.
 * These codes are characters, not the token values in ti/tokens.h; for
 * example, 0x10 is the displayed square-root glyph and 0xD7 is italic i. */
int math_normalize_input(const char *input, char *out, size_t capacity) {
    size_t input_index = 0;
    size_t output_index = 0;

    if (input == NULL || out == NULL || capacity == 0) {
        return 0;
    }
    while (input[input_index] != '\0') {
        const char *replacement = NULL;
        unsigned char character = (unsigned char) input[input_index++];
        switch (character) {
        case 0x10: /* square-root glyph */
            replacement = "sqrt";
            break;
        case 0x11: /* reciprocal glyph */
            replacement = "^-1";
            break;
        case 0x12: /* square glyph */
            replacement = "^2";
            break;
        case 0x0E: /* cube glyph */
            replacement = "^3";
            break;
        case 0x1A: /* TI negation glyph */
            replacement = "-";
            break;
        case 0xD7: /* TI italic imaginary-unit glyph */
            replacement = "i";
            break;
        default:
            break;
        }
        if (replacement != NULL) {
            size_t length = strlen(replacement);
            if (output_index + length + 1 > capacity) {
                out[0] = '\0';
                return 0;
            }
            memcpy(out + output_index, replacement, length);
            output_index += length;
        } else {
            if (output_index + 2 > capacity) {
                out[0] = '\0';
                return 0;
            }
            out[output_index++] = (char) character;
        }
    }
    out[output_index] = '\0';
    return 1;
}

static int parser_parse_multiply(MathParser *parser) {
    int left = parser_parse_unary(parser);
    if (left < 0) {
        return -1;
    }
    for (;;) {
        MathNodeType type;
        int right;
        parser_skip_spaces(parser);
        if (*parser->cursor == '*') {
            type = MATH_NODE_MULTIPLY;
            parser->cursor++;
        } else if (*parser->cursor == '/') {
            type = MATH_NODE_DIVIDE;
            parser->cursor++;
        } else if (parser_can_start_primary(*parser->cursor)) {
            type = MATH_NODE_MULTIPLY;
        } else {
            break;
        }
        right = parser_parse_unary(parser);
        if (right < 0) {
            return -1;
        }
        left = parser_make_binary(parser, type, left, right);
        if (left < 0) {
            return -1;
        }
    }
    return left;
}

static int parser_parse_add(MathParser *parser) {
    int left = parser_parse_multiply(parser);
    if (left < 0) {
        return -1;
    }
    for (;;) {
        MathNodeType type;
        int right;
        parser_skip_spaces(parser);
        if (*parser->cursor == '+') {
            type = MATH_NODE_ADD;
            parser->cursor++;
        } else if (*parser->cursor == '-') {
            type = MATH_NODE_SUBTRACT;
            parser->cursor++;
        } else {
            break;
        }
        right = parser_parse_multiply(parser);
        if (right < 0) {
            return -1;
        }
        left = parser_make_binary(parser, type, left, right);
        if (left < 0) {
            return -1;
        }
    }
    return left;
}

MathStatus math_parse_expression(const char *input, MathExpr *out,
                                 char *error, size_t error_capacity) {
    MathParser parser;
    int root;

    if (input == NULL || out == NULL) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    out->root = -1;
    if (error != NULL && error_capacity > 0) {
        error[0] = '\0';
    }
    parser.cursor = input;
    parser.expression = out;
    parser.error = error;
    parser.error_capacity = error_capacity;
    root = parser_parse_add(&parser);
    parser_skip_spaces(&parser);
    if (root < 0) {
        return out->count >= MATH_EXPR_CAP ? MATH_ERR_LIMIT : MATH_ERR_SYNTAX;
    }
    if (*parser.cursor != '\0') {
        parser_error(&parser, "Unexpected characters after expression");
        return MATH_ERR_SYNTAX;
    }
    out->root = (int16_t) root;
    return MATH_OK;
}

MathStatus math_parse_equation(const char *input, MathEquation *out,
                               char *error, size_t error_capacity) {
    size_t length;
    size_t index;
    size_t equal_position = 0;
    int depth = 0;
    char left[MATH_OUTPUT_WIDTH];
    char right[MATH_OUTPUT_WIDTH];
    MathStatus status;

    if (input == NULL || out == NULL) {
        return MATH_ERR_INPUT;
    }
    length = strlen(input);
    for (index = 0; index < length; index++) {
        if (input[index] == '(') {
            depth++;
        } else if (input[index] == ')') {
            depth--;
            if (depth < 0) {
                if (error != NULL && error_capacity > 0) {
                    snprintf(error, error_capacity, "Unexpected closing parenthesis");
                }
                return MATH_ERR_SYNTAX;
            }
        } else if (input[index] == '=' && depth == 0) {
            if (equal_position != 0) {
                if (error != NULL && error_capacity > 0) {
                    snprintf(error, error_capacity, "Use one equals sign");
                }
                return MATH_ERR_SYNTAX;
            }
            equal_position = index + 1;
        }
    }
    if (depth != 0 || equal_position == 0 || equal_position - 1 >= sizeof(left)
            || length - equal_position >= sizeof(right)) {
        if (error != NULL && error_capacity > 0) {
            snprintf(error, error_capacity, "Enter an equation such as x^2+1=5");
        }
        return MATH_ERR_SYNTAX;
    }
    memcpy(left, input, equal_position - 1);
    left[equal_position - 1] = '\0';
    memcpy(right, input + equal_position, length - equal_position);
    right[length - equal_position] = '\0';
    status = math_parse_expression(left, out == NULL ? NULL : &out->left,
                                   error, error_capacity);
    if (status != MATH_OK) {
        return status;
    }
    return math_parse_expression(right, &out->right, error, error_capacity);
}

static void trim_span(const char *input, size_t *start, size_t *end) {
    while (*start < *end && isspace((unsigned char) input[*start])) {
        (*start)++;
    }
    while (*end > *start && isspace((unsigned char) input[*end - 1])) {
        (*end)--;
    }
}

MathInputKind math_classify_input(const char *input) {
    size_t length;
    size_t start = 0;
    size_t end;
    size_t index;

    if (input == NULL) {
        return MATH_INPUT_FUNCTION;
    }
    length = strlen(input);
    end = length;
    trim_span(input, &start, &end);
    if (end - start >= 10 && strncmp(input + start, "piecewise(", 10) == 0) {
        return MATH_INPUT_PIECEWISE;
    }
    for (index = start; index < end; index++) {
        if (input[index] == '=') {
            return MATH_INPUT_EQUATION;
        }
    }
    return MATH_INPUT_FUNCTION;
}

static size_t find_top_level_character(const char *input, size_t start,
                                       size_t end, char target) {
    int depth = 0;
    size_t index;
    for (index = start; index < end; index++) {
        if (input[index] == '(') {
            depth++;
        } else if (input[index] == ')') {
            /* An open interval may end with ')' without opening a grouping
             * parenthesis.  Do not let that interval delimiter hide the next
             * top-level piecewise separator. */
            if (depth > 0) {
                depth--;
            }
        } else if (input[index] == target && depth == 0) {
            return index;
        }
    }
    return end;
}

static MathStatus parse_piecewise_condition(const char *input, size_t start,
                                            size_t end, MathFraction *lower,
                                            MathFraction *upper,
                                            uint8_t *lower_closed,
                                            uint8_t *upper_closed,
                                            char *error, size_t error_capacity) {
    char normalized[MATH_ERROR_WIDTH];
    size_t normalized_length = 0;
    size_t index;
    size_t comma;

    trim_span(input, &start, &end);
    if (end - start >= 5
            && (input[start] == '(' || input[start] == '[')
            && (input[end - 1] == ')' || input[end - 1] == ']')) {
        comma = find_top_level_character(input, start + 1, end - 1, ',');
        if (comma == end - 1) {
            if (error != NULL && error_capacity > 0) {
                snprintf(error, error_capacity, "Piecewise bounds need a comma");
            }
            return MATH_ERR_SYNTAX;
        }
        {
            size_t lower_start = start + 1;
            size_t lower_end = comma;
            size_t upper_start = comma + 1;
            size_t upper_end = end - 1;
            trim_span(input, &lower_start, &lower_end);
            trim_span(input, &upper_start, &upper_end);
            if (!math_fraction_parse(input + lower_start, lower_end - lower_start,
                                     lower)
                    || !math_fraction_parse(input + upper_start,
                                            upper_end - upper_start, upper)
                    || math_fraction_compare(*lower, *upper) >= 0) {
                if (error != NULL && error_capacity > 0) {
                    snprintf(error, error_capacity,
                             "Piecewise bounds must be exact and increasing");
                }
                return MATH_ERR_SYNTAX;
            }
        }
        *lower_closed = input[start] == '[';
        *upper_closed = input[end - 1] == ']';
        return MATH_OK;
    }

    for (index = start; index < end; index++) {
        if (!isspace((unsigned char) input[index])) {
            if (normalized_length + 1 >= sizeof(normalized)) {
                if (error != NULL && error_capacity > 0) {
                    snprintf(error, error_capacity, "Piecewise condition is too long");
                }
                return MATH_ERR_LIMIT;
            }
            normalized[normalized_length++] = input[index];
        }
    }
    normalized[normalized_length] = '\0';
    {
        char *variable = strchr(normalized, 'x');
        size_t variable_index;
        size_t left_operator_length;
        size_t right_operator_length;
        size_t lower_length;
        size_t upper_start;
        char lower_text[MATH_ERROR_WIDTH];
        char upper_text[MATH_ERROR_WIDTH];

        if (variable == NULL || strchr(variable + 1, 'x') != NULL) {
            if (error != NULL && error_capacity > 0) {
                snprintf(error, error_capacity,
                         "Use an interval or a bounded condition such as -2<x<0");
            }
            return MATH_ERR_SYNTAX;
        }
        variable_index = (size_t) (variable - normalized);
        if (variable_index == 0 || variable_index + 1 >= normalized_length
                || (normalized[variable_index - 1] != '<'
                    && !(variable_index >= 2
                         && normalized[variable_index - 2] == '<'
                         && normalized[variable_index - 1] == '='))
                || normalized[variable_index + 1] != '<') {
            if (error != NULL && error_capacity > 0) {
                snprintf(error, error_capacity,
                         "Use a bounded condition such as -2<x<0 or 0<=x<=2");
            }
            return MATH_ERR_SYNTAX;
        }
        left_operator_length = variable_index >= 2
            && normalized[variable_index - 2] == '<'
            && normalized[variable_index - 1] == '=' ? 2 : 1;
        right_operator_length = variable_index + 2 < normalized_length
            && normalized[variable_index + 2] == '=' ? 2 : 1;
        lower_length = variable_index - left_operator_length;
        upper_start = variable_index + 1 + right_operator_length;
        if (lower_length == 0 || upper_start >= normalized_length
                || lower_length >= sizeof(lower_text)
                || normalized_length - upper_start >= sizeof(upper_text)) {
            if (error != NULL && error_capacity > 0) {
                snprintf(error, error_capacity,
                         "Piecewise conditions need two exact bounds");
            }
            return MATH_ERR_SYNTAX;
        }
        memcpy(lower_text, normalized, lower_length);
        lower_text[lower_length] = '\0';
        memcpy(upper_text, normalized + upper_start,
               normalized_length - upper_start);
        upper_text[normalized_length - upper_start] = '\0';
        if (!math_fraction_parse(lower_text, lower_length, lower)
                || !math_fraction_parse(upper_text,
                                        normalized_length - upper_start, upper)
                || math_fraction_compare(*lower, *upper) >= 0) {
            if (error != NULL && error_capacity > 0) {
                snprintf(error, error_capacity,
                         "Piecewise bounds must be exact and increasing");
            }
            return MATH_ERR_SYNTAX;
        }
        *lower_closed = left_operator_length == 2;
        *upper_closed = right_operator_length == 2;
    }
    return MATH_OK;
}

MathStatus math_parse_piecewise(const char *input, MathPiecewise *out,
                                char *error, size_t error_capacity) {
    size_t length;
    size_t start = 0;
    size_t end;
    size_t branch_start;
    size_t branch_end;
    size_t branch_index = 0;

    if (input == NULL || out == NULL) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    if (error != NULL && error_capacity > 0) {
        error[0] = '\0';
    }
    length = strlen(input);
    end = length;
    trim_span(input, &start, &end);
    if (end - start >= 11 && strncmp(input + start, "piecewise(", 10) == 0
            && input[end - 1] == ')') {
        start += 10;
        end--;
    }
    branch_start = start;
    while (branch_start < end) {
        size_t separator = find_top_level_character(input, branch_start, end, ';');
        size_t bar;
        size_t expression_start;
        size_t expression_end;
        size_t condition_start;
        size_t condition_end;
        MathFraction lower;
        MathFraction upper;
        uint8_t lower_closed;
        uint8_t upper_closed;
        char expression_text[MATH_OUTPUT_WIDTH];
        size_t expression_length;
        MathStatus status;

        branch_end = separator == end ? end : separator;
        trim_span(input, &branch_start, &branch_end);
        if (branch_index >= MATH_PIECEWISE_BRANCHES) {
            if (error != NULL && error_capacity > 0) {
                snprintf(error, error_capacity, "At most four piecewise branches are supported");
            }
            return MATH_ERR_LIMIT;
        }
        bar = find_top_level_character(input, branch_start, branch_end, '|');
        if (bar == branch_end) {
            if (error != NULL && error_capacity > 0) {
                snprintf(error, error_capacity, "Each branch needs expression|[lower,upper]");
            }
            return MATH_ERR_SYNTAX;
        }
        expression_start = branch_start;
        expression_end = bar;
        condition_start = bar + 1;
        condition_end = branch_end;
        trim_span(input, &expression_start, &expression_end);
        trim_span(input, &condition_start, &condition_end);
        status = parse_piecewise_condition(input, condition_start, condition_end,
                                           &lower, &upper, &lower_closed,
                                           &upper_closed, error, error_capacity);
        if (status != MATH_OK) {
            return status;
        }
        expression_length = expression_end - expression_start;
        if (expression_length == 0 || expression_length >= sizeof(expression_text)) {
            if (error != NULL && error_capacity > 0) {
                snprintf(error, error_capacity, "Piecewise expression is empty or too long");
            }
            return MATH_ERR_SYNTAX;
        }
        memcpy(expression_text, input + expression_start, expression_length);
        expression_text[expression_length] = '\0';
        status = math_parse_expression(expression_text,
                                       &out->branch[branch_index].expression,
                                       error, error_capacity);
        if (status != MATH_OK) {
            return status;
        }
        out->branch[branch_index].lower = lower;
        out->branch[branch_index].upper = upper;
        out->branch[branch_index].lower_closed = lower_closed;
        out->branch[branch_index].upper_closed = upper_closed;
        if (branch_index > 0) {
            MathPiecewiseBranch *previous = &out->branch[branch_index - 1];
            MathPiecewiseBranch *current = &out->branch[branch_index];
            int comparison = math_fraction_compare(previous->upper, current->lower);
            if (comparison > 0 || (comparison == 0
                    && previous->upper_closed && current->lower_closed)) {
                if (error != NULL && error_capacity > 0) {
                    snprintf(error, error_capacity, "Piecewise branches overlap");
                }
                return MATH_ERR_SYNTAX;
            }
        }
        branch_index++;
        if (separator == end) {
            break;
        }
        branch_start = separator + 1;
    }
    if (branch_index == 0) {
        if (error != NULL && error_capacity > 0) {
            snprintf(error, error_capacity, "Enter at least one piecewise branch");
        }
        return MATH_ERR_SYNTAX;
    }
    out->count = (uint8_t) branch_index;
    return MATH_OK;
}

static int node_precedence(MathNodeType type) {
    switch (type) {
        case MATH_NODE_ADD:
        case MATH_NODE_SUBTRACT:
            return 1;
        case MATH_NODE_MULTIPLY:
        case MATH_NODE_DIVIDE:
            return 2;
        case MATH_NODE_POWER:
            return 3;
        case MATH_NODE_NEGATE:
        case MATH_NODE_SQRT:
        case MATH_NODE_ABS:
            return 4;
        case MATH_NODE_IMAGINARY:
        default:
            return 5;
    }
}

static int format_expression_node(const MathExpr *expression, int index,
                                  int parent_precedence, char *out,
                                  size_t capacity, size_t *used) {
    const MathNode *node = &expression->nodes[index];
    int precedence = node_precedence(node->type);
    int parenthesized = precedence < parent_precedence;
    char fraction[48];
    int child_precedence;

    if (parenthesized && !append_text(out, capacity, used, "(")) {
        return 0;
    }
    switch (node->type) {
        case MATH_NODE_NUMBER:
            if (!math_fraction_to_text(node->value, fraction, sizeof(fraction))
                    || !append_text(out, capacity, used, "%s", fraction)) {
                return 0;
            }
            break;
        case MATH_NODE_VARIABLE:
            if (!append_text(out, capacity, used, "x")) {
                return 0;
            }
            break;
        case MATH_NODE_IMAGINARY:
            if (!append_text(out, capacity, used, "i")) {
                return 0;
            }
            break;
        case MATH_NODE_NEGATE:
            if (!append_text(out, capacity, used, "-")) {
                return 0;
            }
            if (!format_expression_node(expression, node->left, precedence,
                                        out, capacity, used)) {
                return 0;
            }
            break;
        case MATH_NODE_SQRT:
            if (!append_text(out, capacity, used, "sqrt(")
                    || !format_expression_node(expression, node->left, 0,
                                                out, capacity, used)
                    || !append_text(out, capacity, used, ")")) {
                return 0;
            }
            break;
        case MATH_NODE_ABS:
            if (!append_text(out, capacity, used, "abs(")
                    || !format_expression_node(expression, node->left, 0,
                                                out, capacity, used)
                    || !append_text(out, capacity, used, ")")) {
                return 0;
            }
            break;
        case MATH_NODE_POWER:
            if (!format_expression_node(expression, node->left, precedence,
                                        out, capacity, used)
                    || !append_text(out, capacity, used, "^%d", node->exponent)) {
                return 0;
            }
            break;
        case MATH_NODE_ADD:
        case MATH_NODE_SUBTRACT:
        case MATH_NODE_MULTIPLY:
        case MATH_NODE_DIVIDE:
            child_precedence = precedence + (node->type == MATH_NODE_SUBTRACT
                                           || node->type == MATH_NODE_DIVIDE);
            if (!format_expression_node(expression, node->left, precedence,
                                        out, capacity, used)) {
                return 0;
            }
            if (!append_text(out, capacity, used, "%s",
                             node->type == MATH_NODE_ADD ? "+"
                             : node->type == MATH_NODE_SUBTRACT ? "-"
                             : node->type == MATH_NODE_MULTIPLY ? "*" : "/")) {
                return 0;
            }
            if (!format_expression_node(expression, node->right, child_precedence,
                                        out, capacity, used)) {
                return 0;
            }
            break;
    }
    if (parenthesized && !append_text(out, capacity, used, ")")) {
        return 0;
    }
    return 1;
}

MathStatus math_expression_to_text(const MathExpr *expr, char *out,
                                    size_t capacity) {
    size_t used = 0;
    if (expr == NULL || out == NULL || capacity == 0 || expr->root < 0) {
        return MATH_ERR_INPUT;
    }
    out[0] = '\0';
    return format_expression_node(expr, expr->root, 0, out, capacity, &used)
        ? MATH_OK : MATH_ERR_LIMIT;
}

static void poly_zero(MathPoly *poly) {
    int index;
    poly->degree = 0;
    for (index = 0; index < MATH_POLY_CAP; index++) {
        poly->coefficient[index] = math_fraction_zero();
    }
}

static void poly_trim(MathPoly *poly) {
    while (poly->degree > 0 && math_fraction_is_zero(poly->coefficient[poly->degree])) {
        poly->degree--;
    }
}

static int poly_is_zero(const MathPoly *poly) {
    return poly->degree == 0 && math_fraction_is_zero(poly->coefficient[0]);
}

static int poly_add(const MathPoly *left, const MathPoly *right, MathPoly *out) {
    int index;
    poly_zero(out);
    out->degree = left->degree > right->degree ? left->degree : right->degree;
    if (out->degree >= MATH_POLY_CAP) {
        return 0;
    }
    for (index = 0; index <= out->degree; index++) {
        out->coefficient[index] = fraction_add(left->coefficient[index],
                                               right->coefficient[index]);
    }
    poly_trim(out);
    return 1;
}

static int poly_subtract(const MathPoly *left, const MathPoly *right,
                         MathPoly *out) {
    int index;
    poly_zero(out);
    out->degree = left->degree > right->degree ? left->degree : right->degree;
    if (out->degree >= MATH_POLY_CAP) {
        return 0;
    }
    for (index = 0; index <= out->degree; index++) {
        out->coefficient[index] = fraction_subtract(left->coefficient[index],
                                                    right->coefficient[index]);
    }
    poly_trim(out);
    return 1;
}

static int poly_multiply(const MathPoly *left, const MathPoly *right,
                         MathPoly *out) {
    int left_index;
    int right_index;
    poly_zero(out);
    if (left->degree + right->degree >= MATH_POLY_CAP) {
        return 0;
    }
    out->degree = left->degree + right->degree;
    for (left_index = 0; left_index <= left->degree; left_index++) {
        for (right_index = 0; right_index <= right->degree; right_index++) {
            out->coefficient[left_index + right_index]
                = fraction_add(out->coefficient[left_index + right_index],
                               fraction_multiply(left->coefficient[left_index],
                                                  right->coefficient[right_index]));
        }
    }
    poly_trim(out);
    return 1;
}

static int poly_scale(const MathPoly *input, MathFraction scale, MathPoly *out) {
    int index;
    *out = *input;
    for (index = 0; index <= input->degree; index++) {
        out->coefficient[index] = fraction_multiply(input->coefficient[index], scale);
    }
    poly_trim(out);
    return out->coefficient[out->degree].den != 0;
}

static void complex_poly_zero(MathComplexPoly *poly) {
    int index;
    poly->degree = 0;
    for (index = 0; index < MATH_POLY_CAP; index++) {
        poly->coefficient[index] = complex_zero();
    }
}

static void complex_poly_trim(MathComplexPoly *poly) {
    while (poly->degree > 0
            && complex_is_zero(poly->coefficient[poly->degree])) {
        poly->degree--;
    }
}

static int complex_poly_add(const MathComplexPoly *left,
                            const MathComplexPoly *right,
                            MathComplexPoly *out) {
    int index;
    complex_poly_zero(out);
    out->degree = left->degree > right->degree ? left->degree : right->degree;
    if (out->degree >= MATH_POLY_CAP) {
        return 0;
    }
    for (index = 0; index <= out->degree; index++) {
        out->coefficient[index] = complex_add(left->coefficient[index],
                                              right->coefficient[index]);
    }
    complex_poly_trim(out);
    return 1;
}

static int complex_poly_subtract(const MathComplexPoly *left,
                                 const MathComplexPoly *right,
                                 MathComplexPoly *out) {
    int index;
    complex_poly_zero(out);
    out->degree = left->degree > right->degree ? left->degree : right->degree;
    if (out->degree >= MATH_POLY_CAP) {
        return 0;
    }
    for (index = 0; index <= out->degree; index++) {
        out->coefficient[index] = complex_subtract(left->coefficient[index],
                                                   right->coefficient[index]);
    }
    complex_poly_trim(out);
    return 1;
}

static int complex_poly_multiply(const MathComplexPoly *left,
                                 const MathComplexPoly *right,
                                 MathComplexPoly *out) {
    int left_index;
    int right_index;
    complex_poly_zero(out);
    if (left->degree + right->degree >= MATH_POLY_CAP) {
        return 0;
    }
    out->degree = left->degree + right->degree;
    for (left_index = 0; left_index <= left->degree; left_index++) {
        for (right_index = 0; right_index <= right->degree; right_index++) {
            out->coefficient[left_index + right_index] = complex_add(
                out->coefficient[left_index + right_index],
                complex_multiply(left->coefficient[left_index],
                                 right->coefficient[right_index]));
        }
    }
    complex_poly_trim(out);
    return 1;
}

static int complex_poly_divide_constant(const MathComplexPoly *input,
                                        MathComplexFraction divisor,
                                        MathComplexPoly *out) {
    int index;
    if (complex_is_zero(divisor)) {
        return 0;
    }
    complex_poly_zero(out);
    out->degree = input->degree;
    for (index = 0; index <= input->degree; index++) {
        out->coefficient[index] = complex_divide(input->coefficient[index],
                                                 divisor);
        if (out->coefficient[index].real.den == 0
                || out->coefficient[index].imaginary.den == 0) {
            return 0;
        }
    }
    complex_poly_trim(out);
    return 1;
}

static int complex_polynomial_from_node(const MathExpr *expression,
                                        int index, MathComplexPoly *out) {
    const MathNode *node = &expression->nodes[index];
    MathComplexPoly left;
    MathComplexPoly right;
    MathComplexPoly power;
    MathComplexFraction divisor;
    int exponent;

    switch (node->type) {
        case MATH_NODE_NUMBER:
            complex_poly_zero(out);
            out->coefficient[0] = complex_fraction(node->value,
                                                   math_fraction_zero());
            return 1;
        case MATH_NODE_IMAGINARY:
            complex_poly_zero(out);
            out->coefficient[0] = complex_fraction(math_fraction_zero(),
                                                   math_fraction_one());
            return 1;
        case MATH_NODE_VARIABLE:
            complex_poly_zero(out);
            out->degree = 1;
            out->coefficient[1] = complex_one();
            return 1;
        case MATH_NODE_NEGATE:
            if (!complex_polynomial_from_node(expression, node->left, &left)) {
                return 0;
            }
            complex_poly_zero(out);
            out->degree = left.degree;
            for (exponent = 0; exponent <= left.degree; exponent++) {
                out->coefficient[exponent]
                    = complex_negate(left.coefficient[exponent]);
            }
            return 1;
        case MATH_NODE_ADD:
            return complex_polynomial_from_node(expression, node->left, &left)
                && complex_polynomial_from_node(expression, node->right, &right)
                && complex_poly_add(&left, &right, out);
        case MATH_NODE_SUBTRACT:
            return complex_polynomial_from_node(expression, node->left, &left)
                && complex_polynomial_from_node(expression, node->right, &right)
                && complex_poly_subtract(&left, &right, out);
        case MATH_NODE_MULTIPLY:
            return complex_polynomial_from_node(expression, node->left, &left)
                && complex_polynomial_from_node(expression, node->right, &right)
                && complex_poly_multiply(&left, &right, out);
        case MATH_NODE_DIVIDE:
            if (!complex_polynomial_from_node(expression, node->left, &left)
                    || !complex_polynomial_from_node(expression, node->right, &right)
                    || right.degree != 0) {
                return 0;
            }
            divisor = right.coefficient[0];
            return complex_poly_divide_constant(&left, divisor, out);
        case MATH_NODE_POWER:
            if (node->exponent < 0 || node->exponent > 4
                    || !complex_polynomial_from_node(expression, node->left,
                                                     &left)) {
                return 0;
            }
            complex_poly_zero(&power);
            power.coefficient[0] = complex_one();
            for (exponent = 0; exponent < node->exponent; exponent++) {
                MathComplexPoly next;
                if (!complex_poly_multiply(&power, &left, &next)) {
                    return 0;
                }
                power = next;
            }
            *out = power;
            return 1;
        case MATH_NODE_SQRT:
        case MATH_NODE_ABS:
            return 0;
    }
    return 0;
}

static int polynomial_from_node(const MathExpr *expression, int index,
                                MathPoly *out) {
    const MathNode *node = &expression->nodes[index];
    MathPoly left;
    MathPoly right;
    MathPoly power;
    int exponent;

    switch (node->type) {
        case MATH_NODE_NUMBER:
            poly_zero(out);
            out->coefficient[0] = node->value;
            return 1;
        case MATH_NODE_VARIABLE:
            poly_zero(out);
            out->degree = 1;
            out->coefficient[1] = math_fraction_one();
            return 1;
        case MATH_NODE_IMAGINARY:
            return 0;
        case MATH_NODE_NEGATE:
            if (!polynomial_from_node(expression, node->left, &left)) {
                return 0;
            }
            return poly_scale(&left, math_fraction(-1, 1), out);
        case MATH_NODE_ADD:
            return polynomial_from_node(expression, node->left, &left)
                && polynomial_from_node(expression, node->right, &right)
                && poly_add(&left, &right, out);
        case MATH_NODE_SUBTRACT:
            return polynomial_from_node(expression, node->left, &left)
                && polynomial_from_node(expression, node->right, &right)
                && poly_subtract(&left, &right, out);
        case MATH_NODE_MULTIPLY:
            return polynomial_from_node(expression, node->left, &left)
                && polynomial_from_node(expression, node->right, &right)
                && poly_multiply(&left, &right, out);
        case MATH_NODE_DIVIDE:
            if (!polynomial_from_node(expression, node->left, &left)
                    || !polynomial_from_node(expression, node->right, &right)
                    || right.degree != 0 || math_fraction_is_zero(right.coefficient[0])) {
                return 0;
            }
            return poly_scale(&left, fraction_divide(math_fraction_one(),
                                                     right.coefficient[0]), out);
        case MATH_NODE_POWER:
            if (node->exponent < 0 || node->exponent > 4
                    || !polynomial_from_node(expression, node->left, &left)) {
                return 0;
            }
            poly_zero(&power);
            power.coefficient[0] = math_fraction_one();
            for (exponent = 0; exponent < node->exponent; exponent++) {
                MathPoly next;
                if (!poly_multiply(&power, &left, &next)) {
                    return 0;
                }
                power = next;
            }
            *out = power;
            return 1;
        case MATH_NODE_SQRT:
        case MATH_NODE_ABS:
            return 0;
    }
    return 0;
}

MathStatus math_expression_to_polynomial(const MathExpr *expression,
                                         MathPolynomial *out) {
    MathPoly poly;
    if (expression == NULL || out == NULL || expression->root < 0) {
        return MATH_ERR_INPUT;
    }
    if (!polynomial_from_node(expression, expression->root, &poly)) {
        return MATH_ERR_UNSUPPORTED;
    }
    out->degree = poly.degree;
    memcpy(out->coefficient, poly.coefficient, sizeof(out->coefficient));
    return MATH_OK;
}

static int rational_from_node(const MathExpr *expression, int index,
                              MathRational *out) {
    const MathNode *node = &expression->nodes[index];
    MathRational left;
    MathRational right;
    MathPoly numerator;
    MathPoly denominator;

    out->valid = 0;
    switch (node->type) {
        case MATH_NODE_NUMBER:
            poly_zero(&out->numerator);
            poly_zero(&out->denominator);
            out->numerator.coefficient[0] = node->value;
            out->denominator.coefficient[0] = math_fraction_one();
            out->valid = 1;
            return 1;
        case MATH_NODE_VARIABLE:
            poly_zero(&out->numerator);
            poly_zero(&out->denominator);
            out->numerator.degree = 1;
            out->numerator.coefficient[1] = math_fraction_one();
            out->denominator.coefficient[0] = math_fraction_one();
            out->valid = 1;
            return 1;
        case MATH_NODE_IMAGINARY:
            return 0;
        case MATH_NODE_NEGATE:
            if (!rational_from_node(expression, node->left, &left)) {
                return 0;
            }
            out->numerator = left.numerator;
            out->denominator = left.denominator;
            out->valid = poly_scale(&out->numerator, math_fraction(-1, 1),
                                    &out->numerator);
            return out->valid;
        case MATH_NODE_ADD:
        case MATH_NODE_SUBTRACT:
        case MATH_NODE_MULTIPLY:
        case MATH_NODE_DIVIDE:
            if (!rational_from_node(expression, node->left, &left)
                    || !rational_from_node(expression, node->right, &right)) {
                return 0;
            }
            if (node->type == MATH_NODE_ADD || node->type == MATH_NODE_SUBTRACT) {
                MathPoly left_product;
                MathPoly right_product;
                if (!poly_multiply(&left.numerator, &right.denominator, &left_product)
                        || !poly_multiply(&right.numerator, &left.denominator, &right_product)
                        || (node->type == MATH_NODE_ADD
                            ? !poly_add(&left_product, &right_product, &numerator)
                            : !poly_subtract(&left_product, &right_product, &numerator))
                        || !poly_multiply(&left.denominator, &right.denominator,
                                          &denominator)) {
                    return 0;
                }
            } else if (node->type == MATH_NODE_MULTIPLY) {
                if (!poly_multiply(&left.numerator, &right.numerator, &numerator)
                        || !poly_multiply(&left.denominator, &right.denominator,
                                          &denominator)) {
                    return 0;
                }
            } else {
                if (poly_is_zero(&right.numerator)
                        || !poly_multiply(&left.numerator, &right.denominator,
                                          &numerator)
                        || !poly_multiply(&left.denominator, &right.numerator,
                                          &denominator)) {
                    return 0;
                }
            }
            if (poly_is_zero(&denominator)) {
                return 0;
            }
            out->numerator = numerator;
            out->denominator = denominator;
            out->valid = 1;
            return 1;
        case MATH_NODE_POWER:
            {
                int exponent = node->exponent;
            if (node->exponent < 0) {
                return 0;
            }
            if (!rational_from_node(expression, node->left, &left)) {
                return 0;
            }
            poly_zero(&numerator);
            poly_zero(&denominator);
            numerator.coefficient[0] = math_fraction_one();
            denominator.coefficient[0] = math_fraction_one();
            while (exponent-- > 0) {
                MathPoly next_numerator;
                MathPoly next_denominator;
                if (!poly_multiply(&numerator, &left.numerator, &next_numerator)
                        || !poly_multiply(&denominator, &left.denominator,
                                          &next_denominator)) {
                    return 0;
                }
                numerator = next_numerator;
                denominator = next_denominator;
            }
            out->numerator = numerator;
            out->denominator = denominator;
            out->valid = 1;
            return 1;
            }
        case MATH_NODE_SQRT:
        case MATH_NODE_ABS:
            return 0;
    }
    return 0;
}

static int poly_equal(const MathPoly *left, const MathPoly *right) {
    int index;
    if (left->degree != right->degree) {
        return 0;
    }
    for (index = 0; index <= left->degree; index++) {
        if (!math_fraction_equal(left->coefficient[index], right->coefficient[index])) {
            return 0;
        }
    }
    return 1;
}

static MathFraction polynomial_evaluate_fraction(const MathPoly *poly,
                                                 MathFraction x) {
    MathFraction value = math_fraction_zero();
    int index;
    for (index = poly->degree; index >= 0; index--) {
        value = fraction_add(fraction_multiply(value, x), poly->coefficient[index]);
    }
    return value;
}

static void steps_reset(MathSteps *steps) {
    memset(steps, 0, sizeof(*steps));
}

static void step_add(MathSteps *steps, const char *format, ...) {
    va_list arguments;
    if (steps->count >= MATH_MAX_STEPS) {
        return;
    }
    va_start(arguments, format);
    vsnprintf(steps->lines[steps->count], MATH_STEP_WIDTH, format, arguments);
    va_end(arguments);
    steps->count++;
}

static int expression_append_node(MathExpr *expression, MathNode node) {
    if (expression->count >= MATH_EXPR_CAP) {
        return -1;
    }
    expression->nodes[expression->count] = node;
    expression->count++;
    return (int) expression->count - 1;
}

static int clone_expression_node(const MathExpr *source_expression, int index,
                                 MathExpr *out) {
    const MathNode *source = &source_expression->nodes[index];
    MathNode node = *source;
    int left;
    int right;

    if (source->left >= 0) {
        left = clone_expression_node(source_expression, source->left, out);
        if (left < 0) {
            return -1;
        }
        node.left = (int16_t) left;
    }
    if (source->right >= 0) {
        right = clone_expression_node(source_expression, source->right, out);
        if (right < 0) {
            return -1;
        }
        node.right = (int16_t) right;
    }
    return expression_append_node(out, node);
}

static int clone_composed_node(const MathExpr *outer, int index,
                               const MathExpr *inner, MathExpr *out) {
    const MathNode *source = &outer->nodes[index];
    if (source->type == MATH_NODE_VARIABLE) {
        return clone_expression_node(inner, inner->root, out);
    }
    {
        MathNode node = *source;
        int left;
        int right;
        if (source->left >= 0) {
            left = clone_composed_node(outer, source->left, inner, out);
            if (left < 0) {
                return -1;
            }
            node.left = (int16_t) left;
        }
        if (source->right >= 0) {
            right = clone_composed_node(outer, source->right, inner, out);
            if (right < 0) {
                return -1;
            }
            node.right = (int16_t) right;
        }
        return expression_append_node(out, node);
    }
}

static MathStatus build_composed_expression(const MathExpr *outer,
                                            const MathExpr *inner,
                                            MathExpr *out) {
    int root;
    if (outer == NULL || inner == NULL || out == NULL
            || outer->root < 0 || inner->root < 0) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    out->root = -1;
    root = clone_composed_node(outer, outer->root, inner, out);
    if (root < 0) {
        return MATH_ERR_LIMIT;
    }
    out->root = (int16_t) root;
    return MATH_OK;
}

MathStatus math_compose_functions(const MathExpr *outer,
                                  const MathExpr *inner,
                                  MathCompositionResult *out) {
    MathAnalysis analysis;
    MathExpr composed;
    MathStatus status;

    if (outer == NULL || inner == NULL || out == NULL
            || outer->root < 0 || inner->root < 0) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    out->expression[0] = '\0';
    out->domain[0] = '\0';
    out->range[0] = '\0';
    out->status = MATH_OK;
    status = build_composed_expression(outer, inner, &composed);
    if (status != MATH_OK) {
        snprintf(out->warning, sizeof(out->warning),
                 "The composed expression exceeded the app storage limit.");
        out->status = status;
        return out->status;
    }
    if (math_expression_to_text(&composed, out->expression,
                                sizeof(out->expression)) != MATH_OK) {
        out->status = MATH_ERR_LIMIT;
        return out->status;
    }
    math_analyze_function(&composed, &analysis);
    snprintf(out->domain, sizeof(out->domain), "%s", analysis.domain);
    snprintf(out->range, sizeof(out->range), "%s", analysis.range);
    snprintf(out->warning, sizeof(out->warning), "%s", analysis.warning);
    steps_reset(&out->steps);
    step_add(&out->steps, "1. Substitute the inner function into the outer function.");
    step_add(&out->steps, "2. Compose: %s.", out->expression);
    step_add(&out->steps, "3. Preserve the composed domain: %s.", out->domain);
    step_add(&out->steps, "4. Determine the exact range when supported: %s.",
             out->range);
    return out->status;
}

MathStatus math_compose_three_functions(const MathExpr *outer,
                                        const MathExpr *middle,
                                        const MathExpr *inner,
                                        MathCompositionResult *out) {
    MathExpr middle_composed;
    MathExpr composed;
    MathAnalysis analysis;
    MathStatus status;

    if (out == NULL) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    status = build_composed_expression(middle, inner, &middle_composed);
    if (status == MATH_OK) {
        status = build_composed_expression(outer, &middle_composed, &composed);
    }
    if (status != MATH_OK
            || math_expression_to_text(&composed, out->expression,
                                       sizeof(out->expression)) != MATH_OK) {
        snprintf(out->warning, sizeof(out->warning),
                 "The three-function composition exceeded the app limits.");
        out->status = status == MATH_OK ? MATH_ERR_LIMIT : status;
        return out->status;
    }
    math_analyze_function(&composed, &analysis);
    snprintf(out->domain, sizeof(out->domain), "%s", analysis.domain);
    snprintf(out->range, sizeof(out->range), "%s", analysis.range);
    snprintf(out->warning, sizeof(out->warning), "%s", analysis.warning);
    steps_reset(&out->steps);
    step_add(&out->steps, "1. Substitute the inner function into the middle function.");
    step_add(&out->steps, "2. Substitute that result into the outer function.");
    step_add(&out->steps, "3. Compose: %s.", out->expression);
    step_add(&out->steps, "4. Domain: %s; range: %s.", out->domain, out->range);
    out->status = MATH_OK;
    return out->status;
}

static int integer_square_root(int64_t value, int64_t *root) {
    int64_t low = 0;
    int64_t high = value < 1000000000 ? value : 1000000000;
    while (low <= high) {
        int64_t middle = low + (high - low) / 2;
        int64_t square = middle * middle;
        if (square == value) {
            *root = middle;
            return 1;
        }
        if (square < value) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return 0;
}

static int fraction_is_perfect_square(MathFraction value, MathFraction *root) {
    int64_t numerator_root;
    int64_t denominator_root;
    if (value.den <= 0 || value.num < 0
            || !integer_square_root(value.num, &numerator_root)
            || !integer_square_root(value.den, &denominator_root)) {
        return 0;
    }
    *root = fraction_from_i64(numerator_root, denominator_root);
    return root->den != 0;
}

static void factor_square_part(int64_t value, int64_t *outside,
                               int64_t *inside) {
    int64_t factor;
    int64_t square_part = 1;
    int64_t square_free = value;

    for (factor = 2; factor <= square_free / factor; factor++) {
        int count = 0;
        while (square_free % (factor * factor) == 0) {
            square_free /= factor * factor;
            count++;
        }
        while (count-- > 0) {
            square_part *= factor;
        }
    }
    *outside = square_part;
    *inside = square_free;
}

static int square_root_components(MathFraction value, MathFraction *coefficient,
                                  int64_t *radicand) {
    int64_t numerator_outside;
    int64_t numerator_inside;
    int64_t denominator_outside;
    int64_t denominator_inside;

    if (value.den <= 0 || value.num < 0) {
        return 0;
    }
    if (value.num == 0) {
        *coefficient = math_fraction_zero();
        *radicand = 1;
        return 1;
    }
    factor_square_part(value.num, &numerator_outside, &numerator_inside);
    factor_square_part(value.den, &denominator_outside, &denominator_inside);
    if (denominator_inside > INT64_MAX / numerator_inside) {
        return 0;
    }
    *radicand = numerator_inside * denominator_inside;
    *coefficient = fraction_from_i64(numerator_outside,
                                     denominator_outside * denominator_inside);
    return coefficient->den != 0;
}

static int format_scaled_square_root(MathFraction scale, int64_t radicand,
                                     char *out, size_t capacity) {
    char scale_text[64];

    if (capacity == 0 || scale.den == 0 || radicand <= 0) {
        return 0;
    }
    if (radicand == 1) {
        return math_fraction_to_text(scale, out, capacity);
    }
    if (math_fraction_is_zero(scale)) {
        return snprintf(out, capacity, "0") >= 0 && strlen(out) < capacity;
    }
    if (scale.num == 1) {
        if (scale.den == 1) {
            return snprintf(out, capacity, "sqrt(%ld)", (long) radicand) >= 0
                && strlen(out) < capacity;
        }
        return snprintf(out, capacity, "sqrt(%ld)/%ld", (long) radicand,
                        (long) scale.den) >= 0 && strlen(out) < capacity;
    }
    if (scale.num == -1) {
        if (scale.den == 1) {
            return snprintf(out, capacity, "-sqrt(%ld)", (long) radicand) >= 0
                && strlen(out) < capacity;
        }
        return snprintf(out, capacity, "-sqrt(%ld)/%ld", (long) radicand,
                        (long) scale.den) >= 0 && strlen(out) < capacity;
    }
    if (!math_fraction_to_text(scale, scale_text, sizeof(scale_text))) {
        return 0;
    }
    if (scale.den == 1) {
        return snprintf(out, capacity, "%s*sqrt(%ld)", scale_text,
                        (long) radicand) >= 0 && strlen(out) < capacity;
    }
    return snprintf(out, capacity, "%ld*sqrt(%ld)/%ld", (long) scale.num,
                    (long) radicand, (long) scale.den) >= 0
        && strlen(out) < capacity;
}

static int format_square_root(MathFraction value, char *out, size_t capacity) {
    int64_t radicand;
    MathFraction coefficient;

    if (value.den == 0 || value.num < 0 || capacity == 0) {
        return 0;
    }
    if (!square_root_components(value, &coefficient, &radicand)) {
        return snprintf(out, capacity, "sqrt(%ld/%ld)", (long) value.num,
                        (long) value.den) >= 0 && strlen(out) < capacity;
    }
    return format_scaled_square_root(coefficient, radicand, out, capacity);
}

static int format_complex_root_pair(MathFraction a, MathFraction b,
                                    MathFraction discriminant, char *out,
                                    size_t capacity) {
    MathFraction denominator = fraction_multiply(math_fraction(2, 1), a);
    MathFraction real_part = fraction_divide(fraction_negate(b), denominator);
    MathFraction positive = fraction_negate(discriminant);
    MathFraction radical_coefficient;
    MathFraction imaginary_scale;
    int64_t radicand;
    char real_text[64];
    char imaginary_text[96];

    if (!math_fraction_to_text(real_part, real_text, sizeof(real_text))) {
        return 0;
    }
    if (!square_root_components(positive, &radical_coefficient, &radicand)) {
        return 0;
    }
    imaginary_scale = fraction_divide(radical_coefficient, denominator);
    if (!format_scaled_square_root(imaginary_scale, radicand, imaginary_text,
                                   sizeof(imaginary_text))) {
        return 0;
    }
    if (math_fraction_is_zero(real_part)) {
        return snprintf(out, capacity, "x=+/-i*%s", imaginary_text) >= 0
            && strlen(out) < capacity;
    }
    return snprintf(out, capacity, "x=%s+/-i*%s", real_text, imaginary_text) >= 0
        && strlen(out) < capacity;
}

static int format_absolute_fraction(MathFraction value, char *out, size_t capacity) {
    return math_fraction_to_text(fraction_absolute(value), out, capacity);
}

static int format_polynomial(const MathPoly *poly, char *out, size_t capacity) {
    size_t used = 0;
    int degree;
    int wrote_term = 0;
    char coefficient[48];
    MathFraction absolute;

    if (capacity == 0) {
        return 0;
    }
    out[0] = '\0';
    for (degree = poly->degree; degree >= 0; degree--) {
        MathFraction value = poly->coefficient[degree];
        if (math_fraction_is_zero(value)) {
            continue;
        }
        absolute = fraction_absolute(value);
        if (wrote_term) {
            if (!append_text(out, capacity, &used, value.num < 0 ? "-" : "+")) {
                return 0;
            }
        } else if (value.num < 0) {
            if (!append_text(out, capacity, &used, "-")) {
                return 0;
            }
        }
        if (degree == 0) {
            if (!format_absolute_fraction(absolute, coefficient, sizeof(coefficient))
                    || !append_text(out, capacity, &used, "%s", coefficient)) {
                return 0;
            }
        } else {
            if (!math_fraction_equal(absolute, math_fraction_one())) {
                if (!format_absolute_fraction(absolute, coefficient, sizeof(coefficient))
                        || !append_text(out, capacity, &used, "%s*", coefficient)) {
                    return 0;
                }
            }
            if (!append_text(out, capacity, &used, degree == 1 ? "x" : "x^%d", degree)) {
                return 0;
            }
        }
        wrote_term = 1;
    }
    if (!wrote_term) {
        return append_text(out, capacity, &used, "0");
    }
    return 1;
}

static int format_root_pair(MathFraction a, MathFraction b, MathFraction discriminant,
                            char *out, size_t capacity) {
    MathFraction denominator = fraction_multiply(math_fraction(2, 1), a);
    MathFraction negative_b = fraction_negate(b);
    MathFraction root_discriminant;
    char numerator[128];
    char radical[96];
    char denominator_text[64];

    if (!math_fraction_to_text(denominator, denominator_text,
                               sizeof(denominator_text))) {
        return 0;
    }

    if (discriminant.num == 0) {
        char root[64];
        MathFraction result = fraction_divide(negative_b, denominator);
        if (!math_fraction_to_text(result, root, sizeof(root))) {
            return 0;
        }
        return snprintf(out, capacity, "x=%s", root) >= 0 && strlen(out) < capacity;
    }
    if (discriminant.num > 0 && fraction_is_perfect_square(discriminant, &root_discriminant)) {
        MathFraction first = fraction_divide(fraction_add(negative_b, root_discriminant),
                                             denominator);
        MathFraction second = fraction_divide(fraction_subtract(negative_b, root_discriminant),
                                              denominator);
        char first_text[64];
        char second_text[64];
        if (!math_fraction_to_text(first, first_text, sizeof(first_text))
                || !math_fraction_to_text(second, second_text, sizeof(second_text))) {
            return 0;
        }
        return snprintf(out, capacity, "x=%s or x=%s", first_text, second_text) >= 0
            && strlen(out) < capacity;
    }
    if (discriminant.num < 0) {
        return format_complex_root_pair(a, b, discriminant, out, capacity);
    }
    if (math_fraction_is_zero(b)) {
        MathFraction radical_coefficient;
        MathFraction scale;
        int64_t radicand;
        char first_text[96];
        char second_text[96];
        if (square_root_components(discriminant, &radical_coefficient, &radicand)
                && radicand != 1) {
            scale = fraction_divide(radical_coefficient, denominator);
            if (!format_scaled_square_root(scale, radicand, first_text,
                                           sizeof(first_text))
                    || !format_scaled_square_root(fraction_negate(scale), radicand,
                                                  second_text, sizeof(second_text))) {
                return 0;
            }
            return snprintf(out, capacity, "x=%s or x=%s", first_text,
                            second_text) >= 0 && strlen(out) < capacity;
        }
    }
    if (!format_square_root(discriminant, radical, sizeof(radical))
            || !math_fraction_to_text(negative_b, numerator, sizeof(numerator))) {
        return 0;
    }
    return snprintf(out, capacity, "x=(%s+%s)/%s or x=(%s-%s)/%s",
                    numerator, radical, denominator_text, numerator, radical,
                    denominator_text) >= 0
        && strlen(out) < capacity;
}

static int format_complex_fraction(MathComplexFraction value, char *out,
                                   size_t capacity) {
    char real_text[64];
    char imaginary_text[64];
    MathFraction imaginary_absolute = fraction_absolute(value.imaginary);

    if (math_fraction_is_zero(value.imaginary)) {
        return math_fraction_to_text(value.real, out, capacity);
    }
    if (!math_fraction_to_text(value.real, real_text, sizeof(real_text))
            || !math_fraction_to_text(imaginary_absolute, imaginary_text,
                                      sizeof(imaginary_text))) {
        return 0;
    }
    if (math_fraction_is_zero(value.real)) {
        if (math_fraction_equal(value.imaginary, math_fraction_one())) {
            return snprintf(out, capacity, "i") >= 0 && strlen(out) < capacity;
        }
        if (math_fraction_equal(value.imaginary, math_fraction(-1, 1))) {
            return snprintf(out, capacity, "-i") >= 0 && strlen(out) < capacity;
        }
        if (value.imaginary.num < 0) {
            return snprintf(out, capacity, "-%s*i", imaginary_text) >= 0
                && strlen(out) < capacity;
        }
        return snprintf(out, capacity, "%s*i", imaginary_text) >= 0
            && strlen(out) < capacity;
    }
    if (value.imaginary.num < 0) {
        if (math_fraction_equal(imaginary_absolute, math_fraction_one())) {
            return snprintf(out, capacity, "%s-i", real_text) >= 0
                && strlen(out) < capacity;
        }
        return snprintf(out, capacity, "%s-%s*i", real_text,
                        imaginary_text) >= 0 && strlen(out) < capacity;
    }
    if (math_fraction_equal(imaginary_absolute, math_fraction_one())) {
        return snprintf(out, capacity, "%s+i", real_text) >= 0
            && strlen(out) < capacity;
    }
    return snprintf(out, capacity, "%s+%s*i", real_text,
                    imaginary_text) >= 0 && strlen(out) < capacity;
}

static int expression_contains_imaginary(const MathExpr *expression, int index) {
    const MathNode *node = &expression->nodes[index];
    if (node->type == MATH_NODE_IMAGINARY) {
        return 1;
    }
    if (node->left >= 0
            && expression_contains_imaginary(expression, node->left)) {
        return 1;
    }
    return node->right >= 0
        && expression_contains_imaginary(expression, node->right);
}

static MathStatus try_complex_equation(const MathEquation *equation,
                                       MathSolveResult *out, int *handled) {
    MathComplexPoly left;
    MathComplexPoly right;
    MathComplexPoly cross;
    char left_text[160];
    char right_text[160];
    int index;

    *handled = 0;
    if (!expression_contains_imaginary(&equation->left,
                                       equation->left.root)
            && !expression_contains_imaginary(&equation->right,
                                              equation->right.root)) {
        return MATH_OK;
    }
    *handled = 1;
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    math_expression_to_text(&equation->left, left_text, sizeof(left_text));
    math_expression_to_text(&equation->right, right_text, sizeof(right_text));
    step_add(&out->steps, "1. Read: %s=%s.", left_text, right_text);
    if (!complex_polynomial_from_node(&equation->left, equation->left.root,
                                      &left)
            || !complex_polynomial_from_node(&equation->right,
                                             equation->right.root, &right)
            || !complex_poly_subtract(&left, &right, &cross)) {
        snprintf(out->method, sizeof(out->method), "Unsupported complex pattern");
        snprintf(out->answer, sizeof(out->answer),
                 "This complex expression is outside the exact solver");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    if (cross.degree == 0) {
        snprintf(out->method, sizeof(out->method), "Complex identity check");
        if (complex_is_zero(cross.coefficient[0])) {
            snprintf(out->answer, sizeof(out->answer), "All complex numbers");
        } else {
            snprintf(out->answer, sizeof(out->answer), "No solutions");
        }
        step_add(&out->steps, "2. Compare the real and imaginary parts.");
        out->status = MATH_OK;
        return out->status;
    }
    if (cross.degree == 1) {
        MathComplexFraction root = complex_divide(
            complex_negate(cross.coefficient[0]), cross.coefficient[1]);
        char root_text[128];
        if (root.real.den == 0 || root.imaginary.den == 0
                || !format_complex_fraction(root, root_text, sizeof(root_text))) {
            snprintf(out->method, sizeof(out->method), "Unsupported complex pattern");
            snprintf(out->answer, sizeof(out->answer),
                     "The complex exact result exceeded the app limits");
            out->status = MATH_ERR_LIMIT;
            return out->status;
        }
        snprintf(out->method, sizeof(out->method), "Complex linear isolation");
        snprintf(out->answer, sizeof(out->answer), "x=%s", root_text);
        step_add(&out->steps, "2. Collect complex coefficients and isolate x.");
        step_add(&out->steps, "3. Multiply by the complex conjugate when needed.");
        step_add(&out->steps, "4. x=%s.", root_text);
        out->status = MATH_OK;
        return out->status;
    }
    if (cross.degree == 2) {
        MathFraction real_coefficient[3];
        MathFraction discriminant;
        char answer[256];
        for (index = 0; index <= 2; index++) {
            if (!complex_is_real(cross.coefficient[index])) {
                snprintf(out->method, sizeof(out->method),
                         "Unsupported complex quadratic pattern");
                snprintf(out->answer, sizeof(out->answer),
                         "Complex quadratic coefficients are not supported yet");
                out->status = MATH_ERR_UNSUPPORTED;
                return out->status;
            }
            real_coefficient[index] = cross.coefficient[index].real;
        }
        discriminant = fraction_subtract(
            fraction_multiply(real_coefficient[1], real_coefficient[1]),
            fraction_multiply(math_fraction(4, 1),
                              fraction_multiply(real_coefficient[2],
                                                real_coefficient[0])));
        if (!format_root_pair(real_coefficient[2], real_coefficient[1],
                              discriminant, answer, sizeof(answer))) {
            snprintf(out->method, sizeof(out->method), "Unsupported complex quadratic pattern");
            snprintf(out->answer, sizeof(out->answer),
                     "The quadratic exact result exceeded the app limits");
            out->status = MATH_ERR_LIMIT;
            return out->status;
        }
        snprintf(out->method, sizeof(out->method), "Quadratic formula over complex constants");
        snprintf(out->answer, sizeof(out->answer), "%s", answer);
        step_add(&out->steps, "2. The imaginary parts cancel, leaving a real quadratic.");
        step_add(&out->steps, "3. Apply the quadratic formula and keep exact roots.");
        out->status = MATH_OK;
        return out->status;
    }
    snprintf(out->method, sizeof(out->method), "Unsupported complex pattern");
    snprintf(out->answer, sizeof(out->answer),
             "Complex equations above degree 2 are not supported");
    out->status = MATH_ERR_UNSUPPORTED;
    return out->status;
}

static int format_positive_quadratic_roots(MathFraction a, MathFraction b,
                                           MathFraction discriminant,
                                           char *lower, size_t lower_capacity,
                                           char *upper, size_t upper_capacity) {
    MathFraction denominator = fraction_multiply(math_fraction(2, 1), a);
    MathFraction negative_b = fraction_negate(b);
    MathFraction root_discriminant;
    char denominator_text[64];

    if (a.num <= 0
            || !math_fraction_to_text(denominator, denominator_text,
                                      sizeof(denominator_text))) {
        return 0;
    }
    if (discriminant.num == 0) {
        char root[64];
        MathFraction result = fraction_divide(negative_b, denominator);
        if (!math_fraction_to_text(result, root, sizeof(root))) {
            return 0;
        }
        return snprintf(lower, lower_capacity, "%s", root) >= 0
            && strlen(lower) < lower_capacity
            && snprintf(upper, upper_capacity, "%s", root) >= 0
            && strlen(upper) < upper_capacity;
    }
    if (fraction_is_perfect_square(discriminant, &root_discriminant)) {
        MathFraction first = fraction_divide(
            fraction_subtract(negative_b, root_discriminant), denominator);
        MathFraction second = fraction_divide(
            fraction_add(negative_b, root_discriminant), denominator);
        return math_fraction_to_text(first, lower, lower_capacity)
            && math_fraction_to_text(second, upper, upper_capacity);
    }
    if (math_fraction_is_zero(b)) {
        MathFraction radical_coefficient;
        MathFraction scale;
        int64_t radicand;
        if (square_root_components(discriminant, &radical_coefficient, &radicand)
                && radicand != 1) {
            scale = fraction_divide(radical_coefficient, denominator);
            return format_scaled_square_root(fraction_negate(scale), radicand,
                                             lower, lower_capacity)
                && format_scaled_square_root(scale, radicand,
                                             upper, upper_capacity);
        }
    }
    {
        char negative_b_text[64];
        char radical_text[96];
        if (!math_fraction_to_text(negative_b, negative_b_text,
                                   sizeof(negative_b_text))
                || !format_square_root(discriminant, radical_text,
                                       sizeof(radical_text))) {
            return 0;
        }
        return snprintf(lower, lower_capacity, "(%s-%s)/%s", negative_b_text,
                        radical_text, denominator_text) >= 0
            && strlen(lower) < lower_capacity
            && snprintf(upper, upper_capacity, "(%s+%s)/%s", negative_b_text,
                        radical_text, denominator_text) >= 0
            && strlen(upper) < upper_capacity;
    }
}

static int candidate_allowed(MathFraction candidate, const MathPoly *left_denominator,
                             const MathPoly *right_denominator) {
    MathFraction left_value = polynomial_evaluate_fraction(left_denominator, candidate);
    MathFraction right_value = polynomial_evaluate_fraction(right_denominator, candidate);
    return !math_fraction_is_zero(left_value) && !math_fraction_is_zero(right_value);
}

static int polynomial_has_symmetric_zero_set(const MathPoly *poly) {
    int index;
    int has_even_power = 0;
    int has_odd_power = 0;
    for (index = 0; index <= poly->degree; index++) {
        if (math_fraction_is_zero(poly->coefficient[index])) {
            continue;
        }
        if (index % 2 == 0) {
            has_even_power = 1;
        } else {
            has_odd_power = 1;
        }
    }
    return !(has_even_power && has_odd_power);
}

static int rational_parity(const MathRational *rational, const char **result) {
    MathPoly reflected_numerator = rational->numerator;
    MathPoly reflected_denominator = rational->denominator;
    MathPoly left_product;
    MathPoly right_product;
    int index;
    int even;
    int odd;

    if (!polynomial_has_symmetric_zero_set(&rational->denominator)) {
        *result = "Neither (domain is not symmetric)";
        return 1;
    }
    for (index = 1; index <= reflected_numerator.degree; index += 2) {
        reflected_numerator.coefficient[index]
            = fraction_negate(reflected_numerator.coefficient[index]);
    }
    for (index = 1; index <= reflected_denominator.degree; index += 2) {
        reflected_denominator.coefficient[index]
            = fraction_negate(reflected_denominator.coefficient[index]);
    }
    if (!poly_multiply(&rational->numerator, &reflected_denominator, &left_product)
            || !poly_multiply(&reflected_numerator, &rational->denominator,
                              &right_product)) {
        return 0;
    }
    even = poly_equal(&left_product, &right_product);
    if (!poly_scale(&right_product, math_fraction(-1, 1), &right_product)) {
        return 0;
    }
    odd = poly_equal(&left_product, &right_product);
    if (even && odd) {
        *result = "Both even and odd (zero function)";
    } else if (even) {
        *result = "Even";
    } else if (odd) {
        *result = "Odd";
    } else {
        *result = "Neither";
    }
    return 1;
}

static int format_exclusion_domain(const MathPoly *denominator,
                                   char *out, size_t capacity) {
    MathFraction root_one;
    MathFraction root_two;
    MathFraction discriminant;
    MathFraction a;
    MathFraction b;
    MathFraction c;
    char one[64];
    char two[64];

    if (denominator->degree == 0) {
        return snprintf(out, capacity, "(-infinity,infinity)") >= 0
            && strlen(out) < capacity;
    }
    if (denominator->degree == 1) {
        root_one = fraction_divide(fraction_negate(denominator->coefficient[0]),
                                   denominator->coefficient[1]);
        if (!math_fraction_to_text(root_one, one, sizeof(one))) {
            return 0;
        }
        return snprintf(out, capacity, "(-infinity,%s) U (%s,infinity)", one, one) >= 0
            && strlen(out) < capacity;
    }
    if (denominator->degree != 2) {
        return snprintf(out, capacity, "Real values except denominator zeros") >= 0
            && strlen(out) < capacity;
    }
    a = denominator->coefficient[2];
    b = denominator->coefficient[1];
    c = denominator->coefficient[0];
    discriminant = fraction_subtract(fraction_multiply(b, b),
                                     fraction_multiply(math_fraction(4, 1),
                                                       fraction_multiply(a, c)));
    if (discriminant.num < 0 || !fraction_is_perfect_square(discriminant, &root_one)) {
        return snprintf(out, capacity, "Real values except denominator zeros") >= 0
            && strlen(out) < capacity;
    }
    root_one = fraction_divide(fraction_add(fraction_negate(b), root_one),
                               fraction_multiply(math_fraction(2, 1), a));
    fraction_is_perfect_square(discriminant, &root_two);
    root_two = fraction_divide(fraction_subtract(fraction_negate(b), root_two),
                               fraction_multiply(math_fraction(2, 1), a));
    if (math_fraction_compare(root_one, root_two) > 0) {
        MathFraction swap = root_one;
        root_one = root_two;
        root_two = swap;
    }
    if (!math_fraction_to_text(root_one, one, sizeof(one))
            || !math_fraction_to_text(root_two, two, sizeof(two))) {
        return 0;
    }
    if (math_fraction_equal(root_one, root_two)) {
        return snprintf(out, capacity, "(-infinity,%s) U (%s,infinity)",
                        one, one) >= 0 && strlen(out) < capacity;
    }
    return snprintf(out, capacity,
                    "(-infinity,%s) U (%s,%s) U (%s,infinity)",
                    one, one, two, two) >= 0 && strlen(out) < capacity;
}

static int format_equation_domain(const MathPoly *left_denominator,
                                  const MathPoly *right_denominator,
                                  char *out, size_t capacity) {
    MathPoly combined;

    if (left_denominator->degree == 0) {
        return format_exclusion_domain(right_denominator, out, capacity);
    }
    if (right_denominator->degree == 0) {
        return format_exclusion_domain(left_denominator, out, capacity);
    }
    if (!poly_multiply(left_denominator, right_denominator, &combined)) {
        return snprintf(out, capacity,
                        "Real values except original denominator zeros") >= 0
            && strlen(out) < capacity;
    }
    return format_exclusion_domain(&combined, out, capacity);
}

static int format_quadratic_range(const MathPolynomial *poly, char *out, size_t capacity) {
    MathPoly internal;
    MathFraction vertex_x;
    MathFraction vertex_y;
    char text[64];

    internal.degree = poly->degree;
    memcpy(internal.coefficient, poly->coefficient, sizeof(internal.coefficient));
    vertex_x = fraction_divide(fraction_negate(internal.coefficient[1]),
                               fraction_multiply(math_fraction(2, 1),
                                                 internal.coefficient[2]));
    vertex_y = polynomial_evaluate_fraction(&internal, vertex_x);
    if (!math_fraction_to_text(vertex_y, text, sizeof(text))) {
        return 0;
    }
    if (internal.coefficient[2].num > 0) {
        return snprintf(out, capacity, "[%s,infinity)", text) >= 0
            && strlen(out) < capacity;
    }
    return snprintf(out, capacity, "(-infinity,%s]", text) >= 0
        && strlen(out) < capacity;
}

static int format_linear_intervals(const MathFraction slope, char *increasing,
                                   char *decreasing, char *concave_up,
                                   char *concave_down, size_t capacity) {
    const char *all = "(-infinity,infinity)";
    const char *none = "none";
    snprintf(concave_up, capacity, "%s", none);
    snprintf(concave_down, capacity, "%s", none);
    if (slope.num > 0) {
        snprintf(increasing, capacity, "%s", all);
        snprintf(decreasing, capacity, "%s", none);
    } else if (slope.num < 0) {
        snprintf(increasing, capacity, "%s", none);
        snprintf(decreasing, capacity, "%s", all);
    } else {
        snprintf(increasing, capacity, "%s", none);
        snprintf(decreasing, capacity, "%s", none);
    }
    return 1;
}

static int expression_constant_value(const MathExpr *expression,
                                     MathFraction *value) {
    MathRational rational;
    if (!rational_from_node(expression, expression->root, &rational)
            || rational.numerator.degree != 0
            || rational.denominator.degree != 0
            || math_fraction_is_zero(rational.denominator.coefficient[0])) {
        return 0;
    }
    *value = fraction_divide(rational.numerator.coefficient[0],
                             rational.denominator.coefficient[0]);
    return value->den != 0;
}

static int format_candidate_pair(MathFraction first, MathFraction second,
                                 char *out, size_t capacity) {
    char first_text[64];
    char second_text[64];
    if (!math_fraction_to_text(first, first_text, sizeof(first_text))
            || !math_fraction_to_text(second, second_text, sizeof(second_text))) {
        return 0;
    }
    if (math_fraction_equal(first, second)) {
        return snprintf(out, capacity, "x=%s", first_text) >= 0
            && strlen(out) < capacity;
    }
    if (math_fraction_compare(first, second) > 0) {
        char swap[64];
        snprintf(swap, sizeof(swap), "%s", first_text);
        snprintf(first_text, sizeof(first_text), "%s", second_text);
        snprintf(second_text, sizeof(second_text), "%s", swap);
    }
    return snprintf(out, capacity, "x=%s or x=%s", first_text, second_text) >= 0
        && strlen(out) < capacity;
}

static int extract_linear_radical(const MathExpr *expression,
                                  MathPoly *linear_part, MathPoly *radicand,
                                  int *radical_sign) {
    const MathNode *root = &expression->nodes[expression->root];
    const MathNode *left = root->left >= 0 ? &expression->nodes[root->left] : NULL;
    const MathNode *right = root->right >= 0 ? &expression->nodes[root->right] : NULL;
    MathPoly other;

    if ((root->type != MATH_NODE_ADD && root->type != MATH_NODE_SUBTRACT)
            || left == NULL || right == NULL) {
        return 0;
    }
    if (left->type == MATH_NODE_SQRT
            && polynomial_from_node(expression, root->right, &other)
            && other.degree <= 1
            && polynomial_from_node(expression, left->left, radicand)
            && radicand->degree == 1) {
        *radical_sign = 1;
        if (root->type == MATH_NODE_SUBTRACT) {
            return poly_scale(&other, math_fraction(-1, 1), linear_part);
        }
        *linear_part = other;
        return 1;
    }
    if (right->type == MATH_NODE_SQRT
            && polynomial_from_node(expression, root->left, &other)
            && other.degree <= 1
            && polynomial_from_node(expression, right->left, radicand)
            && radicand->degree == 1) {
        if (root->type == MATH_NODE_SUBTRACT) {
            *radical_sign = -1;
        } else {
            *radical_sign = 1;
        }
        *linear_part = other;
        return 1;
    }
    return 0;
}

static int candidate_satisfies_radical(MathFraction candidate,
                                       const MathPoly *radicand,
                                       const MathPoly *right_side) {
    MathFraction radicand_value = polynomial_evaluate_fraction(radicand, candidate);
    MathFraction right_value = polynomial_evaluate_fraction(right_side, candidate);
    MathFraction right_squared = fraction_multiply(right_value, right_value);
    return math_fraction_compare(radicand_value, math_fraction_zero()) >= 0
        && math_fraction_compare(right_value, math_fraction_zero()) >= 0
        && math_fraction_equal(radicand_value, right_squared);
}

static int extract_two_linear_radicals(const MathExpr *expression,
                                       MathPoly *first, MathPoly *second) {
    const MathNode *root = &expression->nodes[expression->root];
    if (root->type != MATH_NODE_ADD
            || expression->nodes[root->left].type != MATH_NODE_SQRT
            || expression->nodes[root->right].type != MATH_NODE_SQRT
            || !polynomial_from_node(expression,
                                     expression->nodes[root->left].left, first)
            || !polynomial_from_node(expression,
                                     expression->nodes[root->right].left, second)
            || first->degree != 1 || second->degree != 1) {
        return 0;
    }
    return !math_fraction_is_zero(first->coefficient[1])
        && !math_fraction_is_zero(second->coefficient[1]);
}

static int candidate_satisfies_two_radicals(MathFraction candidate,
                                            const MathPoly *first,
                                            const MathPoly *second,
                                            MathFraction target) {
    MathFraction first_value = polynomial_evaluate_fraction(first, candidate);
    MathFraction second_value = polynomial_evaluate_fraction(second, candidate);
    MathFraction first_root;
    MathFraction second_root;

    if (first_value.num < 0 || second_value.num < 0
            || !fraction_is_perfect_square(first_value, &first_root)
            || !fraction_is_perfect_square(second_value, &second_root)) {
        return 0;
    }
    return math_fraction_equal(fraction_add(first_root, second_root), target);
}

static MathStatus try_two_radical_equation(const MathEquation *equation,
                                           MathSolveResult *out, int *handled) {
    const MathExpr *special = NULL;
    MathFraction target;
    MathPoly first;
    MathPoly second;
    MathPoly target_squared;
    MathPoly sum;
    MathPoly isolated;
    MathPoly squared;
    MathPoly equation_poly;
    MathFraction discriminant;
    MathFraction radical_root;
    MathFraction first_candidate;
    MathFraction second_candidate;
    int first_allowed;
    int second_allowed;
    char left_text[160];
    char right_text[160];
    char first_text[64];
    char second_text[64];

    *handled = 0;
    if (expression_constant_value(&equation->right, &target)
            && extract_two_linear_radicals(&equation->left, &first, &second)) {
        special = &equation->left;
    } else if (expression_constant_value(&equation->left, &target)
            && extract_two_linear_radicals(&equation->right, &first, &second)) {
        special = &equation->right;
    } else {
        return MATH_OK;
    }
    (void) special;
    *handled = 1;
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    math_expression_to_text(&equation->left, left_text, sizeof(left_text));
    math_expression_to_text(&equation->right, right_text, sizeof(right_text));
    step_add(&out->steps, "1. Read: %s=%s.", left_text, right_text);
    snprintf(out->method, sizeof(out->method),
             "Isolate radicals, square twice, and verify");
    if (target.num < 0 || math_fraction_is_zero(target)) {
        snprintf(out->answer, sizeof(out->answer), "No solutions");
        step_add(&out->steps, "2. A sum of real square roots cannot equal %s.",
                 target.num < 0 ? "a negative value" : "zero here");
        out->status = MATH_OK;
        return out->status;
    }
    poly_zero(&target_squared);
    target_squared.coefficient[0] = fraction_multiply(target, target);
    if (!poly_add(&target_squared, &first, &sum)
            || !poly_subtract(&sum, &second, &isolated)
            || !poly_scale(&isolated,
                           fraction_divide(math_fraction_one(),
                                           fraction_multiply(math_fraction(2, 1),
                                                             target)),
                           &isolated)
            || !poly_multiply(&isolated, &isolated, &squared)
            || !poly_subtract(&squared, &first, &equation_poly)
            || equation_poly.degree > 2) {
        snprintf(out->answer, sizeof(out->answer),
                 "The squared equation is outside degree 2");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    step_add(&out->steps, "2. Isolate one radical and square both sides.");
    step_add(&out->steps, "3. Square again, then solve the resulting polynomial.");
    if (equation_poly.degree == 0) {
        snprintf(out->answer, sizeof(out->answer),
                 math_fraction_is_zero(equation_poly.coefficient[0])
                     ? "Candidates require exact verification" : "No solutions");
        out->status = math_fraction_is_zero(equation_poly.coefficient[0])
            ? MATH_ERR_UNSUPPORTED : MATH_OK;
        return out->status;
    }
    if (equation_poly.degree == 1) {
        first_candidate = fraction_divide(
            fraction_negate(equation_poly.coefficient[0]),
            equation_poly.coefficient[1]);
        first_allowed = candidate_satisfies_two_radicals(first_candidate,
                                                          &first, &second,
                                                          target);
        if (first_allowed) {
            math_fraction_to_text(first_candidate, first_text, sizeof(first_text));
            snprintf(out->answer, sizeof(out->answer), "x=%s", first_text);
        } else {
            snprintf(out->answer, sizeof(out->answer), "No solutions");
        }
        step_add(&out->steps, "4. Substitute the candidate into both original radicals.");
        out->status = MATH_OK;
        return out->status;
    }
    discriminant = fraction_subtract(
        fraction_multiply(equation_poly.coefficient[1],
                          equation_poly.coefficient[1]),
        fraction_multiply(math_fraction(4, 1),
                          fraction_multiply(equation_poly.coefficient[2],
                                            equation_poly.coefficient[0])));
    if (discriminant.num < 0
            || !fraction_is_perfect_square(discriminant, &radical_root)) {
        snprintf(out->answer, sizeof(out->answer),
                 "Exact radical candidates are not supported for this case");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    first_candidate = fraction_divide(
        fraction_add(fraction_negate(equation_poly.coefficient[1]),
                     radical_root),
        fraction_multiply(math_fraction(2, 1), equation_poly.coefficient[2]));
    second_candidate = fraction_divide(
        fraction_subtract(fraction_negate(equation_poly.coefficient[1]),
                          radical_root),
        fraction_multiply(math_fraction(2, 1), equation_poly.coefficient[2]));
    first_allowed = candidate_satisfies_two_radicals(first_candidate,
                                                      &first, &second, target);
    second_allowed = candidate_satisfies_two_radicals(second_candidate,
                                                       &first, &second, target);
    if (first_allowed && second_allowed) {
        format_candidate_pair(first_candidate, second_candidate,
                              out->answer, sizeof(out->answer));
    } else if (first_allowed) {
        math_fraction_to_text(first_candidate, first_text, sizeof(first_text));
        snprintf(out->answer, sizeof(out->answer), "x=%s", first_text);
    } else if (second_allowed) {
        math_fraction_to_text(second_candidate, second_text, sizeof(second_text));
        snprintf(out->answer, sizeof(out->answer), "x=%s", second_text);
    } else {
        snprintf(out->answer, sizeof(out->answer), "No solutions");
    }
    step_add(&out->steps, "4. Reject any candidate that fails the original sign restrictions.");
    out->status = MATH_OK;
    return out->status;
}

static MathStatus try_linear_radical_equation(const MathEquation *equation,
                                              MathSolveResult *out,
                                              int *handled) {
    const MathExpr *special = NULL;
    const MathExpr *constant = NULL;
    MathFraction constant_value;
    MathPoly linear_part;
    MathPoly radicand;
    MathPoly constant_poly;
    MathPoly right_side;
    MathPoly squared_right;
    MathPoly squared_equation;
    MathFraction a;
    MathFraction b;
    MathFraction c;
    MathFraction discriminant;
    MathFraction radical_root;
    MathFraction first;
    MathFraction second;
    int radical_sign;
    char left_text[160];
    char right_text[160];
    char first_text[64];
    char second_text[64];
    int first_allowed;
    int second_allowed;

    *handled = 0;
    if (equation->left.root >= 0
            && expression_constant_value(&equation->right, &constant_value)) {
        special = &equation->left;
        constant = &equation->right;
    } else if (equation->right.root >= 0
            && expression_constant_value(&equation->left, &constant_value)) {
        special = &equation->right;
        constant = &equation->left;
    } else {
        return MATH_OK;
    }
    (void) constant;
    if (!extract_linear_radical(special, &linear_part, &radicand, &radical_sign)) {
        return MATH_OK;
    }
    *handled = 1;
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    math_expression_to_text(&equation->left, left_text, sizeof(left_text));
    math_expression_to_text(&equation->right, right_text, sizeof(right_text));
    step_add(&out->steps, "1. Read: %s=%s.", left_text, right_text);
    poly_zero(&constant_poly);
    constant_poly.coefficient[0] = constant_value;
    if (!poly_subtract(&constant_poly, &linear_part, &right_side)) {
        out->status = MATH_ERR_LIMIT;
        return out->status;
    }
    if (radical_sign < 0
            && !poly_scale(&right_side, math_fraction(-1, 1), &right_side)) {
        out->status = MATH_ERR_LIMIT;
        return out->status;
    }
    if (!poly_multiply(&right_side, &right_side, &squared_right)
            || !poly_subtract(&squared_right, &radicand, &squared_equation)
            || squared_equation.degree > 2) {
        snprintf(out->method, sizeof(out->method), "Unsupported radical pattern");
        snprintf(out->answer, sizeof(out->answer), "The squared equation is outside degree 2");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    step_add(&out->steps, "2. Isolate the square root on one side.");
    step_add(&out->steps, "3. Square both sides and keep the original sign condition.");
    if (squared_equation.degree == 0) {
        snprintf(out->method, sizeof(out->method), "Isolate radical, square, and verify");
        snprintf(out->answer, sizeof(out->answer),
                 math_fraction_is_zero(squared_equation.coefficient[0])
                 ? "All real candidates need sign checking" : "No solutions");
        out->status = math_fraction_is_zero(squared_equation.coefficient[0])
                    ? MATH_ERR_UNSUPPORTED : MATH_OK;
        return out->status;
    }
    if (squared_equation.degree == 1) {
        first = fraction_divide(fraction_negate(squared_equation.coefficient[0]),
                                squared_equation.coefficient[1]);
        first_allowed = candidate_satisfies_radical(first, &radicand, &right_side);
        snprintf(out->method, sizeof(out->method), "Isolate radical, square, and verify");
        if (first_allowed) {
            math_fraction_to_text(first, first_text, sizeof(first_text));
            snprintf(out->answer, sizeof(out->answer), "x=%s", first_text);
        } else {
            snprintf(out->answer, sizeof(out->answer), "No solutions");
        }
        step_add(&out->steps, "4. Check the candidate in the unsquared equation.");
        out->status = MATH_OK;
        return out->status;
    }
    a = squared_equation.coefficient[2];
    b = squared_equation.coefficient[1];
    c = squared_equation.coefficient[0];
    discriminant = fraction_subtract(fraction_multiply(b, b),
                                     fraction_multiply(math_fraction(4, 1),
                                                       fraction_multiply(a, c)));
    snprintf(out->method, sizeof(out->method), "Isolate radical, square, and verify");
    if (discriminant.num < 0) {
        snprintf(out->answer, sizeof(out->answer), "No solutions");
        step_add(&out->steps, "4. The squared equation has no real candidates.");
        out->status = MATH_OK;
        return out->status;
    }
    if (!fraction_is_perfect_square(discriminant, &radical_root)) {
        snprintf(out->answer, sizeof(out->answer),
                 "Exact candidates require a sign proof after squaring");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    first = fraction_divide(fraction_add(fraction_negate(b), radical_root),
                            fraction_multiply(math_fraction(2, 1), a));
    second = fraction_divide(fraction_subtract(fraction_negate(b), radical_root),
                             fraction_multiply(math_fraction(2, 1), a));
    first_allowed = candidate_satisfies_radical(first, &radicand, &right_side);
    second_allowed = candidate_satisfies_radical(second, &radicand, &right_side);
    if (first_allowed && second_allowed) {
        format_candidate_pair(first, second, out->answer, sizeof(out->answer));
    } else if (first_allowed) {
        math_fraction_to_text(first, first_text, sizeof(first_text));
        snprintf(out->answer, sizeof(out->answer), "x=%s", first_text);
    } else if (second_allowed) {
        math_fraction_to_text(second, second_text, sizeof(second_text));
        snprintf(out->answer, sizeof(out->answer), "x=%s", second_text);
    } else {
        snprintf(out->answer, sizeof(out->answer), "No solutions");
    }
    step_add(&out->steps, "4. Substitute every squared candidate into the original equation.");
    out->status = MATH_OK;
    return out->status;
}

static MathStatus try_special_equation(const MathEquation *equation,
                                       MathSolveResult *out, int *handled) {
    const MathExpr *special = NULL;
    const MathExpr *constant = NULL;
    const MathNode *special_node;
    MathPoly inside;
    MathFraction right_value;
    MathFraction first;
    MathFraction second;
    MathFraction squared;
    MathFraction inside_at_first;
    MathFraction inside_at_second;
    char right_text[64];
    char first_text[64];
    char second_text[64];
    char original_left[160];
    char original_right[160];
    int is_absolute;

    *handled = 0;
    if (equation->left.root >= 0
            && (equation->left.nodes[equation->left.root].type == MATH_NODE_SQRT
                || equation->left.nodes[equation->left.root].type == MATH_NODE_ABS)
            && expression_constant_value(&equation->right, &right_value)) {
        special = &equation->left;
        constant = &equation->right;
    } else if (equation->right.root >= 0
            && (equation->right.nodes[equation->right.root].type == MATH_NODE_SQRT
                || equation->right.nodes[equation->right.root].type == MATH_NODE_ABS)
            && expression_constant_value(&equation->left, &right_value)) {
        special = &equation->right;
        constant = &equation->left;
    } else {
        return MATH_OK;
    }
    (void) constant;
    *handled = 1;
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    math_expression_to_text(&equation->left, original_left, sizeof(original_left));
    math_expression_to_text(&equation->right, original_right, sizeof(original_right));
    special_node = &special->nodes[special->root];
    is_absolute = special_node->type == MATH_NODE_ABS;
    if (!polynomial_from_node(special, special_node->left, &inside)) {
        snprintf(out->method, sizeof(out->method), "Unsupported radical or absolute pattern");
        snprintf(out->answer, sizeof(out->answer),
                 "Isolate the %s before solving this equation",
                 is_absolute ? "absolute value" : "radical");
        step_add(&out->steps, "1. Read: %s=%s.", original_left, original_right);
        step_add(&out->steps, "2. The current exact special-case solver needs one isolated function.");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    if (inside.degree != 1 || math_fraction_is_zero(inside.coefficient[1])) {
        snprintf(out->method, sizeof(out->method), "Unsupported special pattern");
        snprintf(out->answer, sizeof(out->answer),
                 "The inside expression must be linear");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    math_fraction_to_text(right_value, right_text, sizeof(right_text));
    step_add(&out->steps, "1. Read: %s=%s.", original_left, original_right);
    if (!is_absolute && right_value.num < 0) {
        snprintf(out->method, sizeof(out->method), "Radical sign check");
        snprintf(out->answer, sizeof(out->answer), "No solutions");
        step_add(&out->steps, "2. A real square root cannot equal %s.", right_text);
        out->status = MATH_OK;
        return out->status;
    }
    if (is_absolute && right_value.num < 0) {
        snprintf(out->method, sizeof(out->method), "Absolute-value cases");
        snprintf(out->answer, sizeof(out->answer), "No solutions");
        step_add(&out->steps, "2. An absolute value cannot equal %s.", right_text);
        out->status = MATH_OK;
        return out->status;
    }
    squared = fraction_multiply(right_value, right_value);
    if (is_absolute) {
        first = fraction_divide(fraction_subtract(right_value,
                                                  inside.coefficient[0]),
                                inside.coefficient[1]);
        second = fraction_divide(fraction_subtract(fraction_negate(right_value),
                                                   inside.coefficient[0]),
                                 inside.coefficient[1]);
    } else {
        first = fraction_divide(fraction_subtract(squared, inside.coefficient[0]),
                                inside.coefficient[1]);
        second = first;
    }
    if (first.den == 0 || second.den == 0) {
        out->status = MATH_ERR_LIMIT;
        return out->status;
    }
    inside_at_first = polynomial_evaluate_fraction(&inside, first);
    inside_at_second = polynomial_evaluate_fraction(&inside, second);
    if ((!is_absolute && !math_fraction_equal(inside_at_first, squared))
            || (is_absolute && (!math_fraction_equal(inside_at_first, right_value)
                                && !math_fraction_equal(inside_at_first,
                                                         fraction_negate(right_value))))) {
        snprintf(out->method, sizeof(out->method), "Special-case verification");
        snprintf(out->answer, sizeof(out->answer), "No solutions");
        step_add(&out->steps, "2. The candidate fails the original restriction.");
        out->status = MATH_OK;
        return out->status;
    }
    (void) inside_at_second;
    math_fraction_to_text(first, first_text, sizeof(first_text));
    math_fraction_to_text(second, second_text, sizeof(second_text));
    snprintf(out->method, sizeof(out->method), "%s",
             is_absolute ? "Absolute-value cases" : "Square and verify");
    if (!format_candidate_pair(first, second, out->answer, sizeof(out->answer))) {
        out->status = MATH_ERR_LIMIT;
        return out->status;
    }
    step_add(&out->steps, "2. Isolate and solve for the candidate value.");
    step_add(&out->steps, "3. Candidate values: %s and %s.", first_text, second_text);
    step_add(&out->steps, "4. Substitute candidates into the original expression.");
    out->status = MATH_OK;
    return out->status;
}

MathStatus math_solve_equation(const MathEquation *equation,
                               MathSolveResult *out) {
    MathRational left;
    MathRational right;
    MathPoly cross_left;
    MathPoly cross_right;
    MathPoly cross;
    char left_text[160];
    char right_text[160];
    char coefficient[64];
    char discriminant_text[64];
    MathFraction a;
    MathFraction b;
    MathFraction c;
    MathFraction discriminant;
    MathFraction root;
    char root_text[64];
    char allowed_domain[MATH_OUTPUT_WIDTH];
    int special_handled;
    MathStatus special_status;
    int radical_linear_handled;
    MathStatus radical_linear_status;
    int two_radical_handled;
    MathStatus two_radical_status;
    int complex_handled;
    MathStatus complex_status;

    if (equation == NULL || out == NULL) {
        return MATH_ERR_INPUT;
    }
    complex_status = try_complex_equation(equation, out, &complex_handled);
    if (complex_handled) {
        return complex_status;
    }
    two_radical_status = try_two_radical_equation(equation, out,
                                                  &two_radical_handled);
    if (two_radical_handled) {
        return two_radical_status;
    }
    radical_linear_status = try_linear_radical_equation(equation, out,
                                                         &radical_linear_handled);
    if (radical_linear_handled) {
        return radical_linear_status;
    }
    special_status = try_special_equation(equation, out, &special_handled);
    if (special_handled) {
        return special_status;
    }
    memset(out, 0, sizeof(*out));
    out->status = MATH_OK;
    steps_reset(&out->steps);
    math_expression_to_text(&equation->left, left_text, sizeof(left_text));
    math_expression_to_text(&equation->right, right_text, sizeof(right_text));
    step_add(&out->steps, "1. Read: %s=%s", left_text, right_text);
    if (!rational_from_node(&equation->left, equation->left.root, &left)
            || !rational_from_node(&equation->right, equation->right.root, &right)) {
        snprintf(out->method, sizeof(out->method), "Unsupported exact pattern");
        snprintf(out->answer, sizeof(out->answer),
                 "Exact solver supports linear, quadratic, and rational-polynomial equations");
        step_add(&out->steps, "2. This expression needs a method outside the current exact core.");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    if (poly_equal(&left.denominator, &right.denominator)) {
        cross_left = left.numerator;
        cross_right = right.numerator;
    } else if (!poly_multiply(&left.numerator, &right.denominator, &cross_left)
            || !poly_multiply(&right.numerator, &left.denominator, &cross_right)) {
        snprintf(out->method, sizeof(out->method), "Unsupported exact pattern");
        snprintf(out->answer, sizeof(out->answer),
                 "The rational denominators exceed the exact solver limit");
        step_add(&out->steps, "2. The rational equation could not be combined exactly.");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    if (!poly_subtract(&cross_left, &cross_right, &cross)
            || cross.degree > 2) {
        snprintf(out->method, sizeof(out->method), "Unsupported exact pattern");
        snprintf(out->answer, sizeof(out->answer),
                 "Exact solver supports linear, quadratic, and rational-polynomial equations");
        step_add(&out->steps, "2. This expression needs a method outside the current exact core.");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    snprintf(coefficient, sizeof(coefficient), "%s",
             cross.degree == 2 ? "quadratic" : cross.degree == 1 ? "linear" : "constant");
    step_add(&out->steps, "2. Move terms to one side: %s=0 (%s).",
             coefficient, coefficient);
    if (left.denominator.degree > 0 || right.denominator.degree > 0) {
        snprintf(out->warning, sizeof(out->warning),
                 "Original denominator restrictions were preserved and checked.");
        step_add(&out->steps, "3. Record denominator restrictions before clearing fractions.");
    }
    if (cross.degree == 0) {
        if (math_fraction_is_zero(cross.coefficient[0])) {
            snprintf(out->method, sizeof(out->method), "Identity check");
            if (left.denominator.degree > 0 || right.denominator.degree > 0) {
                if (!format_equation_domain(&left.denominator, &right.denominator,
                                            allowed_domain, sizeof(allowed_domain))) {
                    snprintf(allowed_domain, sizeof(allowed_domain),
                             "real values except denominator zeros");
                }
                snprintf(out->answer, sizeof(out->answer), "x in %s",
                         allowed_domain);
                step_add(&out->steps,
                         "4. The result is 0=0; keep the original domain %s.",
                         allowed_domain);
            } else {
                snprintf(out->answer, sizeof(out->answer), "All real numbers");
                step_add(&out->steps,
                         "4. The result is 0=0, so every real x works.");
            }
        } else {
            snprintf(out->method, sizeof(out->method), "Contradiction check");
            snprintf(out->answer, sizeof(out->answer), "No solutions");
            step_add(&out->steps, "4. The result is a nonzero constant=0.");
        }
        return MATH_OK;
    }
    if (cross.degree == 1) {
        a = cross.coefficient[1];
        b = cross.coefficient[0];
        root = fraction_divide(fraction_negate(b), a);
        if (!candidate_allowed(root, &left.denominator, &right.denominator)) {
            snprintf(out->method, sizeof(out->method), "Linear isolation");
            snprintf(out->answer, sizeof(out->answer), "No solutions");
            math_fraction_to_text(root, root_text, sizeof(root_text));
            step_add(&out->steps,
                     "4. x=%s is excluded by the original equation.",
                     root_text);
            return MATH_OK;
        }
        snprintf(out->method, sizeof(out->method), "Linear isolation");
        math_fraction_to_text(root, coefficient, sizeof(coefficient));
        snprintf(out->answer, sizeof(out->answer), "x=%s", coefficient);
        step_add(&out->steps, "4. Isolate x: x=%s.", coefficient);
        step_add(&out->steps, "5. Substitute x back into the original equation.");
        return MATH_OK;
    }
    a = cross.coefficient[2];
    b = cross.coefficient[1];
    c = cross.coefficient[0];
    discriminant = fraction_subtract(fraction_multiply(b, b),
                                     fraction_multiply(math_fraction(4, 1),
                                                       fraction_multiply(a, c)));
    math_fraction_to_text(discriminant, discriminant_text, sizeof(discriminant_text));
    if (discriminant.num > 0 && fraction_is_perfect_square(discriminant, &root)) {
        snprintf(out->method, sizeof(out->method), "Factor into linear factors");
    } else {
        snprintf(out->method, sizeof(out->method), "Quadratic formula");
    }
    step_add(&out->steps, "3. Discriminant: b^2-4ac=%s.", discriminant_text);
    if (discriminant.num < 0) {
        step_add(&out->steps, "4. The discriminant is negative, so keep the complex roots.");
    } else if (out->method[0] == 'F') {
        step_add(&out->steps, "4. The discriminant is a perfect square, so the quadratic factors.");
    } else {
        step_add(&out->steps, "4. Use x=(-b+/-sqrt(b^2-4ac))/(2a).");
    }
    if (!format_root_pair(a, b, discriminant, out->answer, sizeof(out->answer))) {
        out->status = MATH_ERR_LIMIT;
        return out->status;
    }
    if (discriminant.num >= 0) {
        MathFraction sqrt_discriminant;
        if (fraction_is_perfect_square(discriminant, &sqrt_discriminant)) {
            MathFraction first = fraction_divide(fraction_add(fraction_negate(b),
                                                              sqrt_discriminant),
                                                              fraction_multiply(math_fraction(2, 1), a));
            MathFraction second = fraction_divide(fraction_subtract(fraction_negate(b),
                                                                    sqrt_discriminant),
                                                               fraction_multiply(math_fraction(2, 1), a));
            int first_allowed = candidate_allowed(first, &left.denominator,
                                                  &right.denominator);
            int second_allowed = candidate_allowed(second, &left.denominator,
                                                   &right.denominator);
            char first_text[64];
            char second_text[64];
            math_fraction_to_text(first, first_text, sizeof(first_text));
            math_fraction_to_text(second, second_text, sizeof(second_text));
            if (!first_allowed && !second_allowed) {
                snprintf(out->answer, sizeof(out->answer), "No solutions");
            } else if (first_allowed && !second_allowed) {
                snprintf(out->answer, sizeof(out->answer), "x=%s", first_text);
            } else if (!first_allowed && second_allowed) {
                snprintf(out->answer, sizeof(out->answer), "x=%s", second_text);
            }
        }
    }
    step_add(&out->steps, "5. Check each candidate against the original equation.");
    return MATH_OK;
}

static int analyze_absolute_pair(const MathExpr *expression,
                                 MathAnalysis *out);
static int analyze_shifted_special(const MathExpr *expression,
                                   MathAnalysis *out);
static int analyze_squared_linear_radical(const MathExpr *expression,
                                          MathAnalysis *out);
static int analyze_linear_sqrt_fraction(const MathExpr *expression,
                                        MathAnalysis *out);
static int analyze_sqrt_rational_square(const MathExpr *expression,
                                        MathAnalysis *out);

MathStatus math_analyze_function(const MathExpr *expression,
                                 MathAnalysis *out) {
    MathPolynomial polynomial;
    MathRational rational;
    char expression_text[160];
    char denominator_domain[MATH_OUTPUT_WIDTH];
    int special_node;
    int absolute_pair_handled = 0;
    const char *parity_result;
    int sqrt_poly_ok = 0;
    MathPoly sqrt_poly;

    if (expression == NULL || out == NULL) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    out->status = MATH_OK;
    steps_reset(&out->steps);
    math_expression_to_text(expression, expression_text, sizeof(expression_text));
    step_add(&out->steps, "1. Analyze f(x)=%s.", expression_text);

    if (rational_from_node(expression, expression->root, &rational)) {
        format_exclusion_domain(&rational.denominator, denominator_domain,
                                sizeof(denominator_domain));
        snprintf(out->domain, sizeof(out->domain), "%s", denominator_domain);
        if (rational.denominator.degree == 0) {
            if (rational.numerator.degree == 0) {
                char value[64];
                math_fraction_to_text(rational.numerator.coefficient[0], value, sizeof(value));
                snprintf(out->range, sizeof(out->range), "{%s}", value);
            } else if (rational.numerator.degree == 1) {
                snprintf(out->range, sizeof(out->range), "(-infinity,infinity)");
            }
        } else if (rational.denominator.degree == 1
                && rational.numerator.degree <= 1) {
            MathFraction determinant = fraction_subtract(
                fraction_multiply(rational.numerator.coefficient[1],
                                  rational.denominator.coefficient[0]),
                fraction_multiply(rational.numerator.coefficient[0],
                                  rational.denominator.coefficient[1]));
            char value[64];
            if (math_fraction_is_zero(determinant)) {
                MathFraction constant = fraction_divide(
                    rational.numerator.coefficient[1],
                    rational.denominator.coefficient[1]);
                math_fraction_to_text(constant, value, sizeof(value));
                snprintf(out->range, sizeof(out->range), "{%s}", value);
            } else {
                MathFraction asymptote = rational.numerator.degree == 1
                    ? fraction_divide(rational.numerator.coefficient[1],
                                      rational.denominator.coefficient[1])
                    : math_fraction_zero();
                math_fraction_to_text(asymptote, value, sizeof(value));
                snprintf(out->range, sizeof(out->range),
                         "(-infinity,%s) U (%s,infinity)", value, value);
            }
        } else if (rational.denominator.degree == 1
                && rational.numerator.degree == 2) {
            MathFraction denominator_root = fraction_divide(
                fraction_negate(rational.denominator.coefficient[0]),
                rational.denominator.coefficient[1]);
            if (math_fraction_is_zero(polynomial_evaluate_fraction(
                    &rational.numerator, denominator_root))) {
                MathFraction quotient_slope = fraction_divide(
                    rational.numerator.coefficient[2],
                    rational.denominator.coefficient[1]);
                MathFraction quotient_intercept = fraction_divide(
                    fraction_subtract(rational.numerator.coefficient[1],
                                      fraction_multiply(rational.denominator.coefficient[0],
                                                        quotient_slope)),
                    rational.denominator.coefficient[1]);
                MathFraction hole_value = fraction_add(
                    fraction_multiply(quotient_slope, denominator_root),
                    quotient_intercept);
                char value[64];
                math_fraction_to_text(hole_value, value, sizeof(value));
                snprintf(out->range, sizeof(out->range),
                         "(-infinity,%s) U (%s,infinity)", value, value);
            }
        }
        if (rational_parity(&rational, &parity_result)) {
            snprintf(out->parity, sizeof(out->parity), "%s", parity_result);
        }
        step_add(&out->steps, "2. Domain: %s.", out->domain);
        step_add(&out->steps, "3. Parity: compare f(-x) with f(x) and -f(x).");
    } else {
        if (analyze_absolute_pair(expression, out)
                || analyze_shifted_special(expression, out)
                || analyze_squared_linear_radical(expression, out)
                || analyze_linear_sqrt_fraction(expression, out)
                || analyze_sqrt_rational_square(expression, out)) {
            absolute_pair_handled = 1;
        } else if (expression->nodes[expression->root].type == MATH_NODE_SQRT) {
            special_node = expression->root;
            sqrt_poly_ok = polynomial_from_node(expression,
                expression->nodes[special_node].left, &sqrt_poly);
        }
        if (!absolute_pair_handled && sqrt_poly_ok && sqrt_poly.degree == 1) {
            MathFraction boundary = fraction_divide(fraction_negate(sqrt_poly.coefficient[0]),
                                                    sqrt_poly.coefficient[1]);
            char value[64];
            math_fraction_to_text(boundary, value, sizeof(value));
            if (sqrt_poly.coefficient[1].num > 0) {
                snprintf(out->domain, sizeof(out->domain), "[%s,infinity)", value);
            } else {
                snprintf(out->domain, sizeof(out->domain), "(-infinity,%s]", value);
            }
            snprintf(out->range, sizeof(out->range), "[0,infinity)");
            snprintf(out->parity, sizeof(out->parity),
                     "Neither (domain is not symmetric)");
        } else if (!absolute_pair_handled && sqrt_poly_ok && sqrt_poly.degree == 2
                && sqrt_poly.coefficient[2].num > 0) {
            MathFraction radical_discriminant = fraction_subtract(
                fraction_multiply(sqrt_poly.coefficient[1], sqrt_poly.coefficient[1]),
                fraction_multiply(math_fraction(4, 1), fraction_multiply(
                    sqrt_poly.coefficient[2], sqrt_poly.coefficient[0])));
            char minimum_text[96];
            char lower_root[96];
            char upper_root[96];
            if (radical_discriminant.num > 0
                    && format_positive_quadratic_roots(
                        sqrt_poly.coefficient[2], sqrt_poly.coefficient[1],
                        radical_discriminant, lower_root, sizeof(lower_root),
                        upper_root, sizeof(upper_root))) {
                snprintf(out->domain, sizeof(out->domain),
                         "(-infinity,%s] U [%s,infinity)",
                         lower_root, upper_root);
            } else if (radical_discriminant.num > 0) {
                snprintf(out->domain, sizeof(out->domain),
                         "Exact domain not supported for this radical expression");
            } else {
                snprintf(out->domain, sizeof(out->domain),
                         "(-infinity,infinity)");
            }
            if (radical_discriminant.num < 0) {
                MathFraction minimum = fraction_divide(
                    fraction_negate(radical_discriminant),
                    fraction_multiply(math_fraction(4, 1),
                                      sqrt_poly.coefficient[2]));
                if (format_square_root(minimum, minimum_text,
                                       sizeof(minimum_text))) {
                    snprintf(out->range, sizeof(out->range),
                             "[%s,infinity)", minimum_text);
                } else {
                    snprintf(out->range, sizeof(out->range),
                             "Exact range not supported for this radical expression");
                }
            } else {
                snprintf(out->range, sizeof(out->range), "[0,infinity)");
            }
            if (sqrt_poly.coefficient[1].num == 0) {
                snprintf(out->parity, sizeof(out->parity), "Even");
            } else {
                snprintf(out->parity, sizeof(out->parity),
                         "Neither");
            }
        } else if (!absolute_pair_handled
                && expression->nodes[expression->root].type == MATH_NODE_ABS) {
            MathPoly absolute_poly;
            int absolute_poly_ok = polynomial_from_node(expression,
                expression->nodes[expression->root].left, &absolute_poly);
            snprintf(out->domain, sizeof(out->domain), "(-infinity,infinity)");
            if (absolute_poly_ok && absolute_poly.degree <= 1) {
                snprintf(out->range, sizeof(out->range), "[0,infinity)");
            } else {
                snprintf(out->range, sizeof(out->range),
                         "Exact range not supported for this absolute-value expression");
            }
            if (absolute_poly_ok && absolute_poly.degree == 0) {
                snprintf(out->parity, sizeof(out->parity),
                         math_fraction_is_zero(absolute_poly.coefficient[0])
                             ? "Both" : "Even");
            } else if (absolute_poly_ok && absolute_poly.degree == 1
                    && math_fraction_is_zero(absolute_poly.coefficient[0])) {
                snprintf(out->parity, sizeof(out->parity), "Even");
            } else if (absolute_poly_ok && absolute_poly.degree <= 1) {
                snprintf(out->parity, sizeof(out->parity), "Neither");
            } else {
                snprintf(out->parity, sizeof(out->parity),
                         "Not determined exactly for this absolute-value expression");
            }
        } else if (!absolute_pair_handled) {
            snprintf(out->domain, sizeof(out->domain),
                     "Exact domain not supported for this expression");
            snprintf(out->range, sizeof(out->range),
                     "Exact range not supported for this expression");
            snprintf(out->parity, sizeof(out->parity),
                     "Not determined exactly for this expression");
            snprintf(out->warning, sizeof(out->warning),
                     "The expression parsed, but one or more exact analysis rules do not apply.");
        }
        if (!absolute_pair_handled) {
            step_add(&out->steps, "2. Preserve real-domain restrictions from radicals or absolute values.");
            step_add(&out->steps, "3. Parity result: %s.", out->parity[0] == '\0'
                ? "not determined exactly" : out->parity);
        }
    }

    {
        int intervals_handled = absolute_pair_handled;
        MathPoly special_poly;
        int special_poly_ok;

        if (expression->nodes[expression->root].type == MATH_NODE_ABS) {
            special_poly_ok = polynomial_from_node(expression,
                expression->nodes[expression->root].left, &special_poly);
            if (special_poly_ok && special_poly.degree == 1
                    && !math_fraction_is_zero(special_poly.coefficient[1])) {
                MathFraction vertex_x = fraction_divide(
                    fraction_negate(special_poly.coefficient[0]),
                    special_poly.coefficient[1]);
                char vertex[64];
                math_fraction_to_text(vertex_x, vertex, sizeof(vertex));
                if (special_poly.coefficient[1].num > 0) {
                    snprintf(out->increasing, sizeof(out->increasing),
                             "(%s,infinity)", vertex);
                    snprintf(out->decreasing, sizeof(out->decreasing),
                             "(-infinity,%s)", vertex);
                } else {
                    snprintf(out->increasing, sizeof(out->increasing),
                             "(-infinity,%s)", vertex);
                    snprintf(out->decreasing, sizeof(out->decreasing),
                             "(%s,infinity)", vertex);
                }
                snprintf(out->concave_up, sizeof(out->concave_up), "none");
                snprintf(out->concave_down, sizeof(out->concave_down), "none");
                step_add(&out->steps,
                         "4. Split abs(linear) at x=%s; slope gives the two intervals.",
                         vertex);
                intervals_handled = 1;
            }
        } else if (expression->nodes[expression->root].type == MATH_NODE_SQRT) {
            special_poly_ok = polynomial_from_node(expression,
                expression->nodes[expression->root].left, &special_poly);
            if (special_poly_ok && special_poly.degree == 1
                    && !math_fraction_is_zero(special_poly.coefficient[1])) {
                MathFraction boundary = fraction_divide(
                    fraction_negate(special_poly.coefficient[0]),
                    special_poly.coefficient[1]);
                char boundary_text[64];
                math_fraction_to_text(boundary, boundary_text,
                                      sizeof(boundary_text));
                if (special_poly.coefficient[1].num > 0) {
                    snprintf(out->increasing, sizeof(out->increasing),
                             "(%s,infinity)", boundary_text);
                    snprintf(out->decreasing, sizeof(out->decreasing), "none");
                } else {
                    snprintf(out->increasing, sizeof(out->increasing), "none");
                    snprintf(out->decreasing, sizeof(out->decreasing),
                             "(-infinity,%s)", boundary_text);
                }
                snprintf(out->concave_up, sizeof(out->concave_up), "none");
                snprintf(out->concave_down, sizeof(out->concave_down),
                         special_poly.coefficient[1].num > 0
                             ? "(%s,infinity)" : "(-infinity,%s)",
                         boundary_text);
                step_add(&out->steps,
                         "4. Use the square-root boundary and derivative sign.");
                intervals_handled = 1;
            } else if (special_poly_ok && special_poly.degree == 2
                    && special_poly.coefficient[2].num > 0
                    && math_fraction_is_zero(special_poly.coefficient[1])
                    && math_fraction_is_zero(special_poly.coefficient[0])) {
                snprintf(out->increasing, sizeof(out->increasing), "(0,infinity)");
                snprintf(out->decreasing, sizeof(out->decreasing), "(-infinity,0)");
                snprintf(out->concave_up, sizeof(out->concave_up), "none");
                snprintf(out->concave_down, sizeof(out->concave_down), "none");
                step_add(&out->steps,
                         "4. sqrt(a*x^2) is a scaled absolute value; split at x=0.");
                intervals_handled = 1;
            } else if (special_poly_ok && special_poly.degree == 2
                    && special_poly.coefficient[2].num > 0) {
                MathFraction radical_discriminant = fraction_subtract(
                    fraction_multiply(special_poly.coefficient[1],
                                      special_poly.coefficient[1]),
                    fraction_multiply(math_fraction(4, 1), fraction_multiply(
                        special_poly.coefficient[2],
                        special_poly.coefficient[0])));
                MathFraction vertex_x = fraction_divide(
                    fraction_negate(special_poly.coefficient[1]),
                    fraction_multiply(math_fraction(2, 1),
                                      special_poly.coefficient[2]));
                char vertex_text[64];
                math_fraction_to_text(vertex_x, vertex_text, sizeof(vertex_text));
                if (radical_discriminant.num <= 0) {
                    snprintf(out->increasing, sizeof(out->increasing),
                             "(%s,infinity)", vertex_text);
                    snprintf(out->decreasing, sizeof(out->decreasing),
                             "(-infinity,%s)", vertex_text);
                    if (radical_discriminant.num < 0) {
                        snprintf(out->concave_up, sizeof(out->concave_up),
                                 "(-infinity,infinity)");
                    } else {
                        snprintf(out->concave_up, sizeof(out->concave_up), "none");
                    }
                    snprintf(out->concave_down, sizeof(out->concave_down), "none");
                    step_add(&out->steps,
                             "4. Use the quadratic-radicand vertex and derivative sign.");
                    intervals_handled = 1;
                } else {
                    char lower_root[96];
                    char upper_root[96];
                    if (format_positive_quadratic_roots(
                            special_poly.coefficient[2],
                            special_poly.coefficient[1], radical_discriminant,
                            lower_root, sizeof(lower_root), upper_root,
                            sizeof(upper_root))) {
                        snprintf(out->increasing, sizeof(out->increasing),
                                 "(%s,infinity)", upper_root);
                        snprintf(out->decreasing, sizeof(out->decreasing),
                                 "(-infinity,%s)", lower_root);
                        snprintf(out->concave_up, sizeof(out->concave_up), "none");
                        snprintf(out->concave_down, sizeof(out->concave_down),
                                 "(-infinity,%s) U (%s,infinity)",
                                 lower_root, upper_root);
                        step_add(&out->steps,
                                 "4. Restrict the derivative sign to the two radicand-domain branches.");
                        intervals_handled = 1;
                    }
                }
            }
        }

        if (!intervals_handled
                && rational_from_node(expression, expression->root, &rational)
                && rational.denominator.degree == 1
                && rational.numerator.degree <= 1) {
            MathFraction pole = fraction_divide(
                fraction_negate(rational.denominator.coefficient[0]),
                rational.denominator.coefficient[1]);
            MathFraction determinant = fraction_subtract(
                fraction_multiply(rational.numerator.coefficient[1],
                                  rational.denominator.coefficient[0]),
                fraction_multiply(rational.numerator.coefficient[0],
                                  rational.denominator.coefficient[1]));
            char pole_text[64];
            math_fraction_to_text(pole, pole_text, sizeof(pole_text));
            if (math_fraction_is_zero(determinant)) {
                snprintf(out->increasing, sizeof(out->increasing), "none");
                snprintf(out->decreasing, sizeof(out->decreasing), "none");
                snprintf(out->concave_up, sizeof(out->concave_up), "none");
                snprintf(out->concave_down, sizeof(out->concave_down), "none");
            } else if (determinant.num > 0) {
                snprintf(out->increasing, sizeof(out->increasing),
                         "(-infinity,%s) U (%s,infinity)", pole_text, pole_text);
                snprintf(out->decreasing, sizeof(out->decreasing), "none");
                snprintf(out->concave_up, sizeof(out->concave_up),
                         "(-infinity,%s)", pole_text);
                snprintf(out->concave_down, sizeof(out->concave_down),
                         "(%s,infinity)", pole_text);
            } else {
                snprintf(out->increasing, sizeof(out->increasing), "none");
                snprintf(out->decreasing, sizeof(out->decreasing),
                         "(-infinity,%s) U (%s,infinity)", pole_text, pole_text);
                snprintf(out->concave_up, sizeof(out->concave_up),
                         "(%s,infinity)", pole_text);
                snprintf(out->concave_down, sizeof(out->concave_down),
                         "(-infinity,%s)", pole_text);
            }
            step_add(&out->steps,
                     "4. Split at the denominator zero x=%s and inspect the exact derivative sign.",
                     pole_text);
            intervals_handled = 1;
        }

        if (!intervals_handled
                && math_expression_to_polynomial(expression, &polynomial) == MATH_OK
                && polynomial.degree <= 3) {
            char vertex[64];
            int has_even_terms = 0;
            int has_odd_terms = 0;
            int coefficient_index;

            if (out->domain[0] == '\0'
                    || strstr(out->domain, "not supported") != NULL) {
                snprintf(out->domain, sizeof(out->domain),
                         "(-infinity,infinity)");
            }

            for (coefficient_index = 0;
                 coefficient_index <= polynomial.degree; coefficient_index++) {
                if (math_fraction_is_zero(polynomial.coefficient[coefficient_index])) {
                    continue;
                }
                if (coefficient_index % 2 == 0) {
                    has_even_terms = 1;
                } else {
                    has_odd_terms = 1;
                }
            }
            if (out->parity[0] == '\0') {
                if (has_even_terms && has_odd_terms) {
                    snprintf(out->parity, sizeof(out->parity), "Neither");
                } else if (has_odd_terms) {
                    snprintf(out->parity, sizeof(out->parity), "Odd");
                } else if (has_even_terms) {
                    snprintf(out->parity, sizeof(out->parity), "Even");
                } else {
                    snprintf(out->parity, sizeof(out->parity), "Both");
                }
                step_add(&out->steps,
                         "3. Compare f(-x) with f(x) and -f(x): %s.",
                         out->parity);
            }
            if (polynomial.degree == 0) {
                format_linear_intervals(math_fraction_zero(), out->increasing,
                                        out->decreasing, out->concave_up,
                                        out->concave_down, sizeof(out->increasing));
            } else if (polynomial.degree == 1) {
                format_linear_intervals(polynomial.coefficient[1], out->increasing,
                                        out->decreasing, out->concave_up,
                                        out->concave_down, sizeof(out->increasing));
            } else if (polynomial.degree == 2) {
                MathFraction vertex_x = fraction_divide(
                    fraction_negate(polynomial.coefficient[1]),
                    fraction_multiply(math_fraction(2, 1),
                                      polynomial.coefficient[2]));
                math_fraction_to_text(vertex_x, vertex, sizeof(vertex));
                if (polynomial.coefficient[2].num > 0) {
                    snprintf(out->increasing, sizeof(out->increasing),
                             "(%s,infinity)", vertex);
                    snprintf(out->decreasing, sizeof(out->decreasing),
                             "(-infinity,%s)", vertex);
                    snprintf(out->concave_up, sizeof(out->concave_up),
                             "(-infinity,infinity)");
                    snprintf(out->concave_down, sizeof(out->concave_down), "none");
                } else {
                    snprintf(out->increasing, sizeof(out->increasing),
                             "(-infinity,%s)", vertex);
                    snprintf(out->decreasing, sizeof(out->decreasing),
                             "(%s,infinity)", vertex);
                    snprintf(out->concave_up, sizeof(out->concave_up), "none");
                    snprintf(out->concave_down, sizeof(out->concave_down),
                             "(-infinity,infinity)");
                }
            } else {
                MathFraction derivative_a = fraction_multiply(
                    math_fraction(3, 1), polynomial.coefficient[3]);
                MathFraction derivative_b = fraction_multiply(
                    math_fraction(2, 1), polynomial.coefficient[2]);
                MathFraction derivative_c = polynomial.coefficient[1];
                MathFraction derivative_discriminant = fraction_subtract(
                    fraction_multiply(derivative_b, derivative_b),
                    fraction_multiply(math_fraction(4, 1),
                                      fraction_multiply(derivative_a,
                                                        derivative_c)));
                MathFraction inflection = fraction_divide(
                    fraction_negate(polynomial.coefficient[2]),
                    fraction_multiply(math_fraction(3, 1),
                                      polynomial.coefficient[3]));
                char inflection_text[64];

                math_fraction_to_text(inflection, inflection_text,
                                      sizeof(inflection_text));
                if (derivative_discriminant.num <= 0) {
                    if (derivative_a.num > 0) {
                        snprintf(out->increasing, sizeof(out->increasing),
                                 "(-infinity,infinity)");
                        snprintf(out->decreasing, sizeof(out->decreasing),
                                 "none");
                    } else {
                        snprintf(out->increasing, sizeof(out->increasing),
                                 "none");
                        snprintf(out->decreasing, sizeof(out->decreasing),
                                 "(-infinity,infinity)");
                    }
                } else {
                    MathFraction derivative_root;
                    if (fraction_is_perfect_square(derivative_discriminant,
                                                    &derivative_root)) {
                        MathFraction first = fraction_divide(
                            fraction_add(fraction_negate(derivative_b),
                                         derivative_root),
                            fraction_multiply(math_fraction(2, 1),
                                              derivative_a));
                        MathFraction second = fraction_divide(
                            fraction_subtract(fraction_negate(derivative_b),
                                              derivative_root),
                            fraction_multiply(math_fraction(2, 1),
                                              derivative_a));
                        char first_text[64];
                        char second_text[64];
                        if (math_fraction_compare(first, second) > 0) {
                            MathFraction swap = first;
                            first = second;
                            second = swap;
                        }
                        math_fraction_to_text(first, first_text,
                                              sizeof(first_text));
                        math_fraction_to_text(second, second_text,
                                              sizeof(second_text));
                        if (derivative_a.num > 0) {
                            snprintf(out->increasing, sizeof(out->increasing),
                                     "(-infinity,%s) U (%s,infinity)",
                                     first_text, second_text);
                            snprintf(out->decreasing, sizeof(out->decreasing),
                                     "(%s,%s)", first_text, second_text);
                        } else {
                            snprintf(out->increasing, sizeof(out->increasing),
                                     "(%s,%s)", first_text, second_text);
                            snprintf(out->decreasing, sizeof(out->decreasing),
                                     "(-infinity,%s) U (%s,infinity)",
                                     first_text, second_text);
                        }
                    } else {
                        snprintf(out->increasing, sizeof(out->increasing),
                                 "Exact interval analysis not supported");
                        snprintf(out->decreasing, sizeof(out->decreasing),
                                 "Exact interval analysis not supported");
                    }
                }
                if (polynomial.coefficient[3].num > 0) {
                    snprintf(out->concave_up, sizeof(out->concave_up),
                             "(%s,infinity)", inflection_text);
                    snprintf(out->concave_down, sizeof(out->concave_down),
                             "(-infinity,%s)", inflection_text);
                } else {
                    snprintf(out->concave_up, sizeof(out->concave_up),
                             "(-infinity,%s)", inflection_text);
                    snprintf(out->concave_down, sizeof(out->concave_down),
                             "(%s,infinity)", inflection_text);
                }
            }
            if (out->range[0] == '\0') {
                if (polynomial.degree == 0) {
                    char value[64];
                    math_fraction_to_text(polynomial.coefficient[0], value, sizeof(value));
                    snprintf(out->range, sizeof(out->range), "{%s}", value);
                } else if (polynomial.degree == 1) {
                    snprintf(out->range, sizeof(out->range), "(-infinity,infinity)");
                } else if (polynomial.degree == 2) {
                    format_quadratic_range(&polynomial, out->range, sizeof(out->range));
                } else {
                    snprintf(out->range, sizeof(out->range),
                             "(-infinity,infinity)");
                }
            }
            step_add(&out->steps, "4. Use exact polynomial slope and vertex sign analysis.");
            intervals_handled = 1;
        }
        if (!intervals_handled) {
        snprintf(out->increasing, sizeof(out->increasing),
                 "Exact interval analysis not supported");
        snprintf(out->decreasing, sizeof(out->decreasing),
                 "Exact interval analysis not supported");
        snprintf(out->concave_up, sizeof(out->concave_up),
                 "Exact interval analysis not supported");
        snprintf(out->concave_down, sizeof(out->concave_down),
                 "Exact interval analysis not supported");
        if (out->warning[0] == '\0') {
            snprintf(out->warning, sizeof(out->warning),
                     "Intervals are exact for polynomial degree 0 through 2 in this release.");
        }
        }
    }
    if (out->range[0] == '\0') {
        snprintf(out->range, sizeof(out->range),
                 "Exact range not supported for this expression");
    }
    return MATH_OK;
}

typedef struct {
    MathFraction slope;
    MathFraction intercept;
    MathFraction root;
    int multiplier;
} MathAbsoluteTerm;

static int absolute_linear_term(const MathExpr *expression, int node_index,
                                int multiplier, MathAbsoluteTerm *out) {
    MathPoly inner;

    if (expression->nodes[node_index].type != MATH_NODE_ABS
            || !polynomial_from_node(expression,
                                     expression->nodes[node_index].left, &inner)
            || inner.degree != 1
            || math_fraction_is_zero(inner.coefficient[1])) {
        return 0;
    }
    out->slope = inner.coefficient[1];
    out->intercept = inner.coefficient[0];
    out->multiplier = multiplier;
    if (out->slope.num < 0) {
        out->slope = fraction_negate(out->slope);
        out->intercept = fraction_negate(out->intercept);
    }
    out->root = fraction_divide(fraction_negate(out->intercept), out->slope);
    return 1;
}

static void append_interval_text(char *out, size_t capacity,
                                 const char *interval) {
    size_t length = strlen(out);
    if (length == 0) {
        snprintf(out, capacity, "%s", interval);
    } else if (length < capacity) {
        snprintf(out + length, capacity - length, " U %s", interval);
    }
}

static MathFraction absolute_pair_value(const MathAbsoluteTerm *terms,
                                        int term_count, MathFraction x) {
    MathFraction value = math_fraction_zero();
    int index;

    for (index = 0; index < term_count; index++) {
        MathFraction inner = fraction_add(
            fraction_multiply(terms[index].slope, x), terms[index].intercept);
        MathFraction term_value = fraction_absolute(inner);
        if (terms[index].multiplier < 0) {
            term_value = fraction_negate(term_value);
        }
        value = fraction_add(value, term_value);
    }
    return value;
}

static int absolute_pair_matches(const MathAbsoluteTerm *left,
                                 const MathAbsoluteTerm *right,
                                 int right_multiplier_sign, int count) {
    int used[2] = {0, 0};
    int right_index;

    for (right_index = 0; right_index < count; right_index++) {
        int left_index;
        int expected_multiplier = right[right_index].multiplier
            * right_multiplier_sign;
        int found = 0;
        for (left_index = 0; left_index < count; left_index++) {
            if (!used[left_index]
                    && math_fraction_equal(left[left_index].slope,
                                           right[right_index].slope)
                    && math_fraction_equal(left[left_index].intercept,
                                           right[right_index].intercept)
                    && left[left_index].multiplier == expected_multiplier) {
                used[left_index] = 1;
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    return 1;
}

static int analyze_absolute_pair(const MathExpr *expression,
                                 MathAnalysis *out) {
    MathNodeType root_type = expression->nodes[expression->root].type;
    MathAbsoluteTerm terms[2];
    MathAbsoluteTerm reflected[2];
    MathFraction breakpoints[2];
    MathFraction values[2];
    MathFraction slopes[3];
    MathFraction intercepts[3];
    int term_count = 2;
    int breakpoint_count = 0;
    int segment_count;
    int index;
    int lower_unbounded = 0;
    int upper_unbounded = 0;
    int finite_value = 0;
    MathFraction minimum = math_fraction_zero();
    MathFraction maximum = math_fraction_zero();
    char text_one[64];
    char text_two[64];

    if (root_type != MATH_NODE_ADD && root_type != MATH_NODE_SUBTRACT) {
        return 0;
    }
    if (!absolute_linear_term(expression,
                              expression->nodes[expression->root].left,
                              1, &terms[0])
            || !absolute_linear_term(expression,
                                      expression->nodes[expression->root].right,
                                      root_type == MATH_NODE_ADD ? 1 : -1,
                                      &terms[1])) {
        return 0;
    }
    breakpoints[breakpoint_count++] = terms[0].root;
    if (!math_fraction_equal(terms[1].root, breakpoints[0])) {
        breakpoints[breakpoint_count++] = terms[1].root;
    }
    if (breakpoint_count == 2
            && math_fraction_compare(breakpoints[0], breakpoints[1]) > 0) {
        MathFraction swap = breakpoints[0];
        breakpoints[0] = breakpoints[1];
        breakpoints[1] = swap;
    }
    segment_count = breakpoint_count + 1;
    for (index = 0; index < segment_count; index++) {
        MathFraction sample;
        int term_index;

        if (index == 0) {
            sample = fraction_subtract(breakpoints[0], math_fraction_one());
        } else if (index == breakpoint_count) {
            sample = fraction_add(breakpoints[breakpoint_count - 1],
                                  math_fraction_one());
        } else {
            sample = fraction_divide(
                fraction_add(breakpoints[index - 1], breakpoints[index]),
                math_fraction(2, 1));
        }
        slopes[index] = math_fraction_zero();
        intercepts[index] = math_fraction_zero();
        for (term_index = 0; term_index < term_count; term_index++) {
            MathFraction inner = fraction_add(
                fraction_multiply(terms[term_index].slope, sample),
                terms[term_index].intercept);
            int sign = inner.num < 0 ? -1 : 1;
            MathFraction factor = math_fraction(
                terms[term_index].multiplier * sign, 1);
            slopes[index] = fraction_add(slopes[index],
                fraction_multiply(factor, terms[term_index].slope));
            intercepts[index] = fraction_add(intercepts[index],
                fraction_multiply(factor, terms[term_index].intercept));
        }
        if (slopes[index].num == 0) {
            MathFraction constant = intercepts[index];
            if (!finite_value) {
                minimum = constant;
                maximum = constant;
                finite_value = 1;
            } else {
                if (math_fraction_compare(constant, minimum) < 0) {
                    minimum = constant;
                }
                if (math_fraction_compare(constant, maximum) > 0) {
                    maximum = constant;
                }
            }
        }
    }
    for (index = 0; index < breakpoint_count; index++) {
        values[index] = absolute_pair_value(terms, term_count, breakpoints[index]);
        if (!finite_value) {
            minimum = values[index];
            maximum = values[index];
            finite_value = 1;
        } else {
            if (math_fraction_compare(values[index], minimum) < 0) {
                minimum = values[index];
            }
            if (math_fraction_compare(values[index], maximum) > 0) {
                maximum = values[index];
            }
        }
    }
    if (slopes[0].num > 0) {
        lower_unbounded = 1;
    } else if (slopes[0].num < 0) {
        upper_unbounded = 1;
    }
    if (slopes[segment_count - 1].num > 0) {
        upper_unbounded = 1;
    } else if (slopes[segment_count - 1].num < 0) {
        lower_unbounded = 1;
    }
    if (!finite_value) {
        return 0;
    }

    snprintf(out->domain, sizeof(out->domain), "(-infinity,infinity)");
    math_fraction_to_text(minimum, text_one, sizeof(text_one));
    math_fraction_to_text(maximum, text_two, sizeof(text_two));
    if (lower_unbounded && upper_unbounded) {
        snprintf(out->range, sizeof(out->range), "(-infinity,infinity)");
    } else if (lower_unbounded) {
        snprintf(out->range, sizeof(out->range), "(-infinity,%s]", text_two);
    } else if (upper_unbounded) {
        snprintf(out->range, sizeof(out->range), "[%s,infinity)", text_one);
    } else if (math_fraction_equal(minimum, maximum)) {
        snprintf(out->range, sizeof(out->range), "{%s}", text_one);
    } else {
        snprintf(out->range, sizeof(out->range), "[%s,%s]", text_one, text_two);
    }
    out->increasing[0] = '\0';
    out->decreasing[0] = '\0';
    for (index = 0; index < segment_count; index++) {
        char interval[MATH_OUTPUT_WIDTH];
        if (index == 0) {
            math_fraction_to_text(breakpoints[0], text_one, sizeof(text_one));
            snprintf(interval, sizeof(interval), "(-infinity,%s)", text_one);
        } else if (index == breakpoint_count) {
            math_fraction_to_text(breakpoints[breakpoint_count - 1],
                                  text_one, sizeof(text_one));
            snprintf(interval, sizeof(interval), "(%s,infinity)", text_one);
        } else {
            math_fraction_to_text(breakpoints[index - 1], text_one, sizeof(text_one));
            math_fraction_to_text(breakpoints[index], text_two, sizeof(text_two));
            snprintf(interval, sizeof(interval), "(%s,%s)", text_one, text_two);
        }
        if (slopes[index].num > 0) {
            append_interval_text(out->increasing, sizeof(out->increasing), interval);
        } else if (slopes[index].num < 0) {
            append_interval_text(out->decreasing, sizeof(out->decreasing), interval);
        }
    }
    if (out->increasing[0] == '\0') {
        snprintf(out->increasing, sizeof(out->increasing), "none");
    }
    if (out->decreasing[0] == '\0') {
        snprintf(out->decreasing, sizeof(out->decreasing), "none");
    }
    snprintf(out->concave_up, sizeof(out->concave_up), "none");
    snprintf(out->concave_down, sizeof(out->concave_down), "none");
    reflected[0] = terms[0];
    reflected[1] = terms[1];
    reflected[0].intercept = fraction_negate(reflected[0].intercept);
    reflected[1].intercept = fraction_negate(reflected[1].intercept);
    if (absolute_pair_matches(terms, reflected, 1, term_count)) {
        snprintf(out->parity, sizeof(out->parity), "Even");
    } else if (absolute_pair_matches(terms, reflected, -1, term_count)) {
        snprintf(out->parity, sizeof(out->parity), "Odd");
    } else {
        snprintf(out->parity, sizeof(out->parity), "Neither");
    }
    step_add(&out->steps, "2. Domain: (-infinity,infinity).");
    step_add(&out->steps, "3. Split at the absolute-value zeros and combine line segments.");
    step_add(&out->steps, "4. Range: %s.", out->range);
    step_add(&out->steps, "5. Parity: %s.", out->parity);
    out->status = MATH_OK;
    return 1;
}

static int constant_node_value(const MathExpr *expression, int node_index,
                               MathFraction *value) {
    MathPoly polynomial;
    if (!polynomial_from_node(expression, node_index, &polynomial)
            || polynomial.degree != 0) {
        return 0;
    }
    *value = polynomial.coefficient[0];
    return 1;
}

static int analyze_shifted_special(const MathExpr *expression,
                                   MathAnalysis *out) {
    const MathNode *root = &expression->nodes[expression->root];
    int special_index = -1;
    int special_sign = 1;
    MathFraction offset;
    MathPoly inner;
    char expression_text[160];
    char boundary_text[64];
    char offset_text[64];

    offset = math_fraction_zero();
    if (root->type == MATH_NODE_NEGATE
            && (expression->nodes[root->left].type == MATH_NODE_ABS
                || expression->nodes[root->left].type == MATH_NODE_SQRT)) {
        special_index = root->left;
        special_sign = -1;
    } else if (root->type == MATH_NODE_ADD
            && (expression->nodes[root->left].type == MATH_NODE_ABS
                || expression->nodes[root->left].type == MATH_NODE_SQRT)
            && constant_node_value(expression, root->right, &offset)) {
        special_index = root->left;
    } else if (root->type == MATH_NODE_ADD
            && (expression->nodes[root->right].type == MATH_NODE_ABS
                || expression->nodes[root->right].type == MATH_NODE_SQRT)
            && constant_node_value(expression, root->left, &offset)) {
        special_index = root->right;
    } else if (root->type == MATH_NODE_SUBTRACT
            && (expression->nodes[root->left].type == MATH_NODE_ABS
                || expression->nodes[root->left].type == MATH_NODE_SQRT)
            && constant_node_value(expression, root->right, &offset)) {
        special_index = root->left;
        offset = fraction_negate(offset);
    } else if (root->type == MATH_NODE_SUBTRACT
            && constant_node_value(expression, root->left, &offset)
            && (expression->nodes[root->right].type == MATH_NODE_ABS
                || expression->nodes[root->right].type == MATH_NODE_SQRT)) {
        special_index = root->right;
        special_sign = -1;
    }
    if (special_index < 0
            || !polynomial_from_node(expression,
                                     expression->nodes[special_index].left,
                                     &inner)) {
        return 0;
    }
    if (expression->nodes[special_index].type == MATH_NODE_ABS) {
        MathFraction vertex;
        if (inner.degree != 1 || math_fraction_is_zero(inner.coefficient[1])) {
            return 0;
        }
        vertex = fraction_divide(fraction_negate(inner.coefficient[0]),
                                 inner.coefficient[1]);
        math_fraction_to_text(vertex, boundary_text, sizeof(boundary_text));
        math_fraction_to_text(offset, offset_text, sizeof(offset_text));
        snprintf(out->domain, sizeof(out->domain), "(-infinity,infinity)");
        if (special_sign > 0) {
            snprintf(out->range, sizeof(out->range), "[%s,infinity)", offset_text);
        } else {
            snprintf(out->range, sizeof(out->range), "(-infinity,%s]", offset_text);
        }
        if ((inner.coefficient[1].num > 0 && special_sign > 0)
                || (inner.coefficient[1].num < 0 && special_sign < 0)) {
            snprintf(out->increasing, sizeof(out->increasing),
                     "(%s,infinity)", boundary_text);
            snprintf(out->decreasing, sizeof(out->decreasing),
                     "(-infinity,%s)", boundary_text);
        } else {
            snprintf(out->increasing, sizeof(out->increasing),
                     "(-infinity,%s)", boundary_text);
            snprintf(out->decreasing, sizeof(out->decreasing),
                     "(%s,infinity)", boundary_text);
        }
        snprintf(out->concave_up, sizeof(out->concave_up), "none");
        snprintf(out->concave_down, sizeof(out->concave_down), "none");
        snprintf(out->parity, sizeof(out->parity),
                 math_fraction_is_zero(inner.coefficient[0]) ? "Even" : "Neither");
    } else {
        MathFraction boundary;
        if (inner.degree != 1 || math_fraction_is_zero(inner.coefficient[1])) {
            return 0;
        }
        boundary = fraction_divide(fraction_negate(inner.coefficient[0]),
                                   inner.coefficient[1]);
        math_fraction_to_text(boundary, boundary_text, sizeof(boundary_text));
        math_fraction_to_text(offset, offset_text, sizeof(offset_text));
        if (special_sign > 0) {
            snprintf(out->range, sizeof(out->range), "[%s,infinity)", offset_text);
        } else {
            snprintf(out->range, sizeof(out->range), "(-infinity,%s]", offset_text);
        }
        if (inner.coefficient[1].num > 0) {
            snprintf(out->domain, sizeof(out->domain), "[%s,infinity)",
                     boundary_text);
        } else {
            snprintf(out->domain, sizeof(out->domain), "(-infinity,%s]",
                     boundary_text);
        }
        {
            char domain_interval[MATH_OUTPUT_WIDTH];
            if (inner.coefficient[1].num > 0) {
                snprintf(domain_interval, sizeof(domain_interval),
                         "(%s,infinity)", boundary_text);
            } else {
                snprintf(domain_interval, sizeof(domain_interval),
                         "(-infinity,%s)", boundary_text);
            }
            if ((inner.coefficient[1].num > 0 && special_sign > 0)
                    || (inner.coefficient[1].num < 0 && special_sign < 0)) {
                snprintf(out->increasing, sizeof(out->increasing), "%s",
                         domain_interval);
                snprintf(out->decreasing, sizeof(out->decreasing), "none");
            } else {
                snprintf(out->increasing, sizeof(out->increasing), "none");
                snprintf(out->decreasing, sizeof(out->decreasing), "%s",
                         domain_interval);
            }
            if (special_sign > 0) {
                snprintf(out->concave_up, sizeof(out->concave_up), "none");
                snprintf(out->concave_down, sizeof(out->concave_down), "%s",
                         domain_interval);
            } else {
                snprintf(out->concave_up, sizeof(out->concave_up), "%s",
                         domain_interval);
                snprintf(out->concave_down, sizeof(out->concave_down), "none");
            }
        }
        snprintf(out->parity, sizeof(out->parity),
                 "Neither (domain is not symmetric)");
    }
    out->warning[0] = '\0';
    math_expression_to_text(expression, expression_text, sizeof(expression_text));
    steps_reset(&out->steps);
    step_add(&out->steps, "1. Analyze f(x)=%s.", expression_text);
    step_add(&out->steps, "2. Preserve the exact vertical shift in the range.");
    step_add(&out->steps, "3. Split at the special-function boundary x=%s.",
             boundary_text);
    step_add(&out->steps, "4. Parity result: %s.", out->parity);
    out->status = MATH_OK;
    return 1;
}

static int squared_linear_radical_node(const MathExpr *expression,
                                       int node_index, MathPoly *radicand) {
    const MathNode *node = &expression->nodes[node_index];
    const MathNode *child;

    if (node->type != MATH_NODE_POWER || node->exponent != 2
            || node->left < 0) {
        return 0;
    }
    child = &expression->nodes[node->left];
    if (child->type != MATH_NODE_SQRT || child->left < 0
            || !polynomial_from_node(expression, child->left, radicand)
            || radicand->degree != 1) {
        return 0;
    }
    return !math_fraction_is_zero(radicand->coefficient[1]);
}

static int analyze_squared_linear_radical(const MathExpr *expression,
                                          MathAnalysis *out) {
    const MathNode *root = &expression->nodes[expression->root];
    MathPoly radicand;
    MathPoly other;
    MathPoly reduced;
    MathFraction boundary;
    MathFraction boundary_value;
    int special_sign = 1;
    int other_sign = 0;
    int special_index = -1;
    int other_index = -1;
    char expression_text[160];
    char boundary_text[64];
    char value_text[64];
    char interval[MATH_OUTPUT_WIDTH];

    poly_zero(&other);
    if (root->type == MATH_NODE_POWER
            && squared_linear_radical_node(expression, expression->root,
                                           &radicand)) {
        special_index = expression->root;
    } else if ((root->type == MATH_NODE_ADD
                || root->type == MATH_NODE_SUBTRACT)
            && squared_linear_radical_node(expression, root->left, &radicand)
            && polynomial_from_node(expression, root->right, &other)
            && other.degree <= 1) {
        special_index = root->left;
        other_index = root->right;
        other_sign = root->type == MATH_NODE_ADD ? 1 : -1;
    } else if (root->type == MATH_NODE_ADD
            && squared_linear_radical_node(expression, root->right, &radicand)
            && polynomial_from_node(expression, root->left, &other)
            && other.degree <= 1) {
        special_index = root->right;
        other_index = root->left;
        other_sign = 1;
    } else if (root->type == MATH_NODE_SUBTRACT
            && squared_linear_radical_node(expression, root->right, &radicand)
            && polynomial_from_node(expression, root->left, &other)
            && other.degree <= 1) {
        special_index = root->right;
        other_index = root->left;
        special_sign = -1;
        other_sign = 1;
    }
    if (special_index < 0) {
        return 0;
    }
    (void) other_index;
    if (other_sign == 0) {
        poly_zero(&other);
    } else if (special_sign > 0) {
        MathPoly signed_other;
        if (other_sign > 0) {
            signed_other = other;
        } else if (!poly_scale(&other, math_fraction(-1, 1), &signed_other)) {
            return 0;
        }
        if (!poly_add(&radicand, &signed_other, &reduced)) {
            return 0;
        }
    } else {
        MathPoly signed_radicand;
        if (!poly_scale(&radicand, math_fraction(-1, 1), &signed_radicand)
                || !poly_add(&signed_radicand, &other, &reduced)) {
            return 0;
        }
    }
    if (other_sign == 0) {
        reduced = radicand;
    }
    if (reduced.degree != 1) {
        return 0;
    }
    boundary = fraction_divide(fraction_negate(radicand.coefficient[0]),
                               radicand.coefficient[1]);
    boundary_value = polynomial_evaluate_fraction(&reduced, boundary);
    math_fraction_to_text(boundary, boundary_text, sizeof(boundary_text));
    math_fraction_to_text(boundary_value, value_text, sizeof(value_text));
    if (radicand.coefficient[1].num > 0) {
        snprintf(out->domain, sizeof(out->domain), "[%s,infinity)",
                 boundary_text);
        snprintf(interval, sizeof(interval), "(%s,infinity)", boundary_text);
    } else {
        snprintf(out->domain, sizeof(out->domain), "(-infinity,%s]",
                 boundary_text);
        snprintf(interval, sizeof(interval), "(-infinity,%s)", boundary_text);
    }
    if ((radicand.coefficient[1].num > 0 && reduced.coefficient[1].num > 0)
            || (radicand.coefficient[1].num < 0
                && reduced.coefficient[1].num < 0)) {
        snprintf(out->increasing, sizeof(out->increasing), "%s", interval);
        snprintf(out->decreasing, sizeof(out->decreasing), "none");
    } else {
        snprintf(out->increasing, sizeof(out->increasing), "none");
        snprintf(out->decreasing, sizeof(out->decreasing), "%s", interval);
    }
    if ((radicand.coefficient[1].num > 0 && reduced.coefficient[1].num > 0)
            || (radicand.coefficient[1].num < 0
                && reduced.coefficient[1].num < 0)) {
        snprintf(out->range, sizeof(out->range), "[%s,infinity)",
                 value_text);
    } else {
        snprintf(out->range, sizeof(out->range), "(-infinity,%s]",
                 value_text);
    }
    snprintf(out->concave_up, sizeof(out->concave_up), "none");
    snprintf(out->concave_down, sizeof(out->concave_down), "none");
    snprintf(out->parity, sizeof(out->parity),
             "Neither (domain is not symmetric)");
    out->warning[0] = '\0';
    math_expression_to_text(expression, expression_text, sizeof(expression_text));
    steps_reset(&out->steps);
    step_add(&out->steps, "1. Analyze f(x)=%s.", expression_text);
    step_add(&out->steps, "2. Use sqrt(u)^2=u while preserving u>=0.");
    step_add(&out->steps, "3. Domain begins at x=%s.", boundary_text);
    step_add(&out->steps, "4. Range: %s; parity: %s.", out->range, out->parity);
    out->status = MATH_OK;
    return 1;
}

static int extract_linear_sqrt_form(const MathExpr *expression, int node_index,
                                    MathPoly *radicand, MathFraction *alpha,
                                    MathFraction *beta) {
    const MathNode *node = &expression->nodes[node_index];
    MathPoly left_radicand;
    MathPoly right_radicand;
    MathFraction left_alpha;
    MathFraction left_beta;
    MathFraction right_alpha;
    MathFraction right_beta;
    MathFraction scalar;

    if (node->type == MATH_NODE_SQRT && node->left >= 0
            && polynomial_from_node(expression, node->left, radicand)
            && radicand->degree == 1
            && !math_fraction_is_zero(radicand->coefficient[1])) {
        *alpha = math_fraction_one();
        *beta = math_fraction_zero();
        return 1;
    }
    if (node->type == MATH_NODE_NUMBER) {
        poly_zero(radicand);
        *alpha = math_fraction_zero();
        *beta = node->value;
        return 1;
    }
    if (node->type == MATH_NODE_NEGATE
            && extract_linear_sqrt_form(expression, node->left, radicand,
                                        alpha, beta)) {
        *alpha = fraction_negate(*alpha);
        *beta = fraction_negate(*beta);
        return 1;
    }
    if (node->type == MATH_NODE_ADD || node->type == MATH_NODE_SUBTRACT) {
        if (!extract_linear_sqrt_form(expression, node->left, &left_radicand,
                                      &left_alpha, &left_beta)
                || !extract_linear_sqrt_form(expression, node->right,
                                              &right_radicand, &right_alpha,
                                              &right_beta)) {
            return 0;
        }
        if (!math_fraction_is_zero(left_alpha)
                && !math_fraction_is_zero(right_alpha)
                && !poly_equal(&left_radicand, &right_radicand)) {
            return 0;
        }
        if (math_fraction_is_zero(left_alpha)) {
            *radicand = right_radicand;
        } else {
            *radicand = left_radicand;
        }
        *alpha = node->type == MATH_NODE_ADD
            ? fraction_add(left_alpha, right_alpha)
            : fraction_subtract(left_alpha, right_alpha);
        *beta = node->type == MATH_NODE_ADD
            ? fraction_add(left_beta, right_beta)
            : fraction_subtract(left_beta, right_beta);
        return 1;
    }
    if (node->type == MATH_NODE_MULTIPLY || node->type == MATH_NODE_DIVIDE) {
        if (constant_node_value(expression, node->left, &scalar)
                && extract_linear_sqrt_form(expression, node->right,
                                            radicand, alpha, beta)) {
            if (node->type == MATH_NODE_DIVIDE) {
                return 0;
            }
            *alpha = fraction_multiply(*alpha, scalar);
            *beta = fraction_multiply(*beta, scalar);
            return 1;
        }
        if (node->type == MATH_NODE_MULTIPLY
                && constant_node_value(expression, node->right, &scalar)
                && extract_linear_sqrt_form(expression, node->left,
                                            radicand, alpha, beta)) {
            *alpha = fraction_multiply(*alpha, scalar);
            *beta = fraction_multiply(*beta, scalar);
            return 1;
        }
        if (node->type == MATH_NODE_DIVIDE
                && constant_node_value(expression, node->right, &scalar)
                && !math_fraction_is_zero(scalar)
                && extract_linear_sqrt_form(expression, node->left,
                                            radicand, alpha, beta)) {
            *alpha = fraction_divide(*alpha, scalar);
            *beta = fraction_divide(*beta, scalar);
            return alpha->den != 0 && beta->den != 0;
        }
    }
    return 0;
}

static int format_bounded_range(MathFraction first, int first_included,
                                MathFraction second, int second_included,
                                char *out, size_t capacity) {
    char first_text[64];
    char second_text[64];
    if (math_fraction_compare(first, second) <= 0) {
        if (!math_fraction_to_text(first, first_text, sizeof(first_text))
                || !math_fraction_to_text(second, second_text,
                                          sizeof(second_text))) {
            return 0;
        }
        return snprintf(out, capacity, "%c%s,%s%c",
                        first_included ? '[' : '(', first_text, second_text,
                        second_included ? ']' : ')') >= 0
            && strlen(out) < capacity;
    }
    return format_bounded_range(second, second_included, first, first_included,
                                out, capacity);
}

static int analyze_linear_sqrt_fraction(const MathExpr *expression,
                                        MathAnalysis *out) {
    const MathNode *root = &expression->nodes[expression->root];
    MathPoly numerator_radicand;
    MathPoly denominator_radicand;
    MathFraction numerator_alpha;
    MathFraction numerator_beta;
    MathFraction denominator_alpha;
    MathFraction denominator_beta;
    MathFraction boundary;
    MathFraction endpoint_value;
    MathFraction limit;
    MathFraction determinant;
    char boundary_text[64];
    char interval[MATH_OUTPUT_WIDTH];
    char expression_text[160];
    int denominator_at_zero_nonzero;

    if (root->type != MATH_NODE_DIVIDE
            || !extract_linear_sqrt_form(expression, root->left,
                                         &numerator_radicand,
                                         &numerator_alpha, &numerator_beta)
            || !extract_linear_sqrt_form(expression, root->right,
                                         &denominator_radicand,
                                         &denominator_alpha,
                                         &denominator_beta)
            || !poly_equal(&numerator_radicand, &denominator_radicand)
            || math_fraction_is_zero(denominator_beta)
            || (denominator_alpha.num != 0
                && denominator_alpha.num * denominator_beta.num < 0)) {
        return 0;
    }
    boundary = fraction_divide(
        fraction_negate(numerator_radicand.coefficient[0]),
        numerator_radicand.coefficient[1]);
    endpoint_value = fraction_divide(numerator_beta, denominator_beta);
    limit = fraction_divide(numerator_alpha, denominator_alpha);
    determinant = fraction_subtract(
        fraction_multiply(numerator_alpha, denominator_beta),
        fraction_multiply(numerator_beta, denominator_alpha));
    denominator_at_zero_nonzero = !math_fraction_is_zero(denominator_beta);
    if (!math_fraction_to_text(boundary, boundary_text, sizeof(boundary_text))) {
        return 0;
    }
    if (numerator_radicand.coefficient[1].num > 0) {
        snprintf(out->domain, sizeof(out->domain),
                 denominator_at_zero_nonzero ? "[%s,infinity)" : "(%s,infinity)",
                 boundary_text);
        snprintf(interval, sizeof(interval), "(%s,infinity)", boundary_text);
    } else {
        snprintf(out->domain, sizeof(out->domain),
                 denominator_at_zero_nonzero ? "(-infinity,%s]" : "(-infinity,%s)",
                 boundary_text);
        snprintf(interval, sizeof(interval), "(-infinity,%s)", boundary_text);
    }
    if (math_fraction_is_zero(determinant)) {
        char value_text[64];
        if (!math_fraction_to_text(endpoint_value, value_text,
                                   sizeof(value_text))) {
            return 0;
        }
        snprintf(out->range, sizeof(out->range), "{%s}", value_text);
        snprintf(out->increasing, sizeof(out->increasing), "none");
        snprintf(out->decreasing, sizeof(out->decreasing), "none");
    } else {
        int increases_with_x =
            (determinant.num > 0
             && numerator_radicand.coefficient[1].num > 0)
            || (determinant.num < 0
                && numerator_radicand.coefficient[1].num < 0);
        if (increases_with_x) {
            snprintf(out->increasing, sizeof(out->increasing), "%s", interval);
            snprintf(out->decreasing, sizeof(out->decreasing), "none");
        } else {
            snprintf(out->increasing, sizeof(out->increasing), "none");
            snprintf(out->decreasing, sizeof(out->decreasing), "%s", interval);
        }
        if (denominator_alpha.num == 0) {
            char endpoint_text[64];
            if (!math_fraction_to_text(endpoint_value, endpoint_text,
                                       sizeof(endpoint_text))) {
                return 0;
            }
            if ((numerator_alpha.num > 0
                 && numerator_radicand.coefficient[1].num > 0)
                    || (numerator_alpha.num < 0
                        && numerator_radicand.coefficient[1].num < 0)) {
                snprintf(out->range, sizeof(out->range), "[%s,infinity)",
                         endpoint_text);
            } else {
                snprintf(out->range, sizeof(out->range),
                         "(-infinity,%s]", endpoint_text);
            }
        } else {
            format_bounded_range(limit, 0, endpoint_value,
                                 denominator_at_zero_nonzero, out->range,
                                 sizeof(out->range));
        }
    }
    snprintf(out->concave_up, sizeof(out->concave_up),
             "Exact concavity analysis not supported for this composition");
    snprintf(out->concave_down, sizeof(out->concave_down),
             "Exact concavity analysis not supported for this composition");
    snprintf(out->parity, sizeof(out->parity),
             "Neither (domain is not symmetric)");
    out->warning[0] = '\0';
    math_expression_to_text(expression, expression_text, sizeof(expression_text));
    steps_reset(&out->steps);
    step_add(&out->steps, "1. Analyze f(x)=%s using t=sqrt(linear radicand).",
             expression_text);
    step_add(&out->steps, "2. Use t>=0 and inspect the exact fractional expression.");
    step_add(&out->steps, "3. Include the radicand boundary and denominator restriction.");
    step_add(&out->steps, "4. Range: %s.", out->range);
    out->status = MATH_OK;
    return 1;
}

static int polynomial_square_root_linear(const MathPoly *square,
                                         MathPoly *root) {
    MathFraction leading_root;
    MathFraction constant_root;
    MathFraction middle;

    if (square->degree == 0) {
        if (!fraction_is_perfect_square(square->coefficient[0],
                                        &constant_root)) {
            return 0;
        }
        poly_zero(root);
        root->coefficient[0] = constant_root;
        return 1;
    }
    if (square->degree != 2
            || !fraction_is_perfect_square(square->coefficient[2],
                                           &leading_root)
            || !fraction_is_perfect_square(square->coefficient[0],
                                           &constant_root)) {
        return 0;
    }
    poly_zero(root);
    root->degree = 1;
    root->coefficient[1] = leading_root;
    root->coefficient[0] = constant_root;
    middle = fraction_multiply(math_fraction(2, 1),
                               fraction_multiply(leading_root,
                                                 constant_root));
    if (math_fraction_equal(square->coefficient[1], middle)) {
        return 1;
    }
    root->coefficient[0] = fraction_negate(constant_root);
    middle = fraction_negate(middle);
    return math_fraction_equal(square->coefficient[1], middle);
}

static int fraction_sign(MathFraction value) {
    return value.num < 0 ? -1 : value.num > 0 ? 1 : 0;
}

static int analyze_sqrt_rational_square(const MathExpr *expression,
                                        MathAnalysis *out) {
    const MathNode *root = &expression->nodes[expression->root];
    MathRational rational;
    MathPoly numerator_root;
    MathPoly denominator_root;
    MathFraction determinant;
    MathFraction ratio;
    MathFraction numerator_slope;
    MathFraction numerator_intercept;
    MathFraction denominator_slope;
    MathFraction denominator_intercept;
    MathFraction breakpoints[2];
    int breakpoint_count;
    int index;
    char expression_text[160];
    char domain[MATH_OUTPUT_WIDTH];

    if (root->type != MATH_NODE_SQRT || root->left < 0) {
        return 0;
    }
    if (!rational_from_node(expression, root->left, &rational)
            || !polynomial_square_root_linear(&rational.numerator,
                                              &numerator_root)
            || !polynomial_square_root_linear(&rational.denominator,
                                              &denominator_root)
            || denominator_root.degree != 1
            || math_fraction_is_zero(denominator_root.coefficient[1])
            || !format_exclusion_domain(&rational.denominator, domain,
                                         sizeof(domain))) {
        return 0;
    }
    snprintf(out->domain, sizeof(out->domain), "%s", domain);
    numerator_slope = numerator_root.degree == 1
        ? numerator_root.coefficient[1] : math_fraction_zero();
    numerator_intercept = numerator_root.coefficient[0];
    denominator_slope = denominator_root.coefficient[1];
    denominator_intercept = denominator_root.coefficient[0];
    determinant = fraction_subtract(
        fraction_multiply(numerator_slope, denominator_intercept),
        fraction_multiply(numerator_intercept, denominator_slope));
    if (numerator_root.degree == 0) {
        char value[64];
        math_fraction_to_text(fraction_absolute(numerator_root.coefficient[0]),
                              value, sizeof(value));
        if (math_fraction_is_zero(numerator_root.coefficient[0])) {
            snprintf(out->range, sizeof(out->range), "{%s}", value);
        } else {
            snprintf(out->range, sizeof(out->range), "(0,infinity)");
        }
    } else if (math_fraction_is_zero(determinant)) {
        char value[64];
        ratio = fraction_divide(numerator_slope, denominator_slope);
        math_fraction_to_text(fraction_absolute(ratio), value, sizeof(value));
        snprintf(out->range, sizeof(out->range), "{%s}", value);
    } else {
        snprintf(out->range, sizeof(out->range), "[0,infinity)");
    }
    if (math_fraction_is_zero(numerator_intercept)
            && math_fraction_is_zero(numerator_slope)) {
        snprintf(out->increasing, sizeof(out->increasing), "none");
        snprintf(out->decreasing, sizeof(out->decreasing), "none");
        snprintf(out->concave_up, sizeof(out->concave_up), "none");
        snprintf(out->concave_down, sizeof(out->concave_down), "none");
    } else if (math_fraction_is_zero(determinant)) {
        snprintf(out->increasing, sizeof(out->increasing), "none");
        snprintf(out->decreasing, sizeof(out->decreasing), "none");
        snprintf(out->concave_up, sizeof(out->concave_up), "none");
        snprintf(out->concave_down, sizeof(out->concave_down), "none");
    } else {
        MathFraction numerator_root_value = math_fraction_zero();
        breakpoint_count = 1;
        breakpoints[0] = fraction_divide(
            fraction_negate(denominator_intercept), denominator_slope);
        if (numerator_root.degree == 1) {
            numerator_root_value = fraction_divide(
                fraction_negate(numerator_intercept), numerator_slope);
            if (!math_fraction_equal(numerator_root_value, breakpoints[0])) {
                breakpoints[breakpoint_count++] = numerator_root_value;
            }
        }
        if (breakpoint_count == 2
                && math_fraction_compare(breakpoints[0], breakpoints[1]) > 0) {
            MathFraction swap = breakpoints[0];
            breakpoints[0] = breakpoints[1];
            breakpoints[1] = swap;
        }
        out->increasing[0] = '\0';
        out->decreasing[0] = '\0';
        out->concave_up[0] = '\0';
        out->concave_down[0] = '\0';
        for (index = 0; index <= breakpoint_count; index++) {
            MathFraction sample;
            MathFraction numerator_value;
            MathFraction denominator_value;
            int derivative_sign;
            int concavity_sign;
            char interval[MATH_OUTPUT_WIDTH];
            char lower[64];
            char upper[64];

            if (index == 0) {
                sample = fraction_subtract(breakpoints[0], math_fraction_one());
                math_fraction_to_text(breakpoints[0], upper, sizeof(upper));
                snprintf(interval, sizeof(interval), "(-infinity,%s)", upper);
            } else if (index == breakpoint_count) {
                sample = fraction_add(breakpoints[breakpoint_count - 1],
                                      math_fraction_one());
                math_fraction_to_text(breakpoints[breakpoint_count - 1], lower,
                                      sizeof(lower));
                snprintf(interval, sizeof(interval), "(%s,infinity)", lower);
            } else {
                sample = fraction_divide(
                    fraction_add(breakpoints[index - 1], breakpoints[index]),
                    math_fraction(2, 1));
                math_fraction_to_text(breakpoints[index - 1], lower,
                                      sizeof(lower));
                math_fraction_to_text(breakpoints[index], upper, sizeof(upper));
                snprintf(interval, sizeof(interval), "(%s,%s)", lower, upper);
            }
            numerator_value = polynomial_evaluate_fraction(&numerator_root, sample);
            denominator_value = polynomial_evaluate_fraction(&denominator_root, sample);
            derivative_sign = fraction_sign(numerator_value)
                * fraction_sign(denominator_value) * fraction_sign(determinant);
            concavity_sign = fraction_sign(numerator_value)
                * -fraction_sign(determinant) * fraction_sign(denominator_slope);
            if (derivative_sign > 0) {
                append_interval_text(out->increasing, sizeof(out->increasing), interval);
            } else if (derivative_sign < 0) {
                append_interval_text(out->decreasing, sizeof(out->decreasing), interval);
            }
            if (concavity_sign > 0) {
                append_interval_text(out->concave_up, sizeof(out->concave_up), interval);
            } else if (concavity_sign < 0) {
                append_interval_text(out->concave_down, sizeof(out->concave_down), interval);
            }
        }
        if (out->increasing[0] == '\0') {
            snprintf(out->increasing, sizeof(out->increasing), "none");
        }
        if (out->decreasing[0] == '\0') {
            snprintf(out->decreasing, sizeof(out->decreasing), "none");
        }
        if (out->concave_up[0] == '\0') {
            snprintf(out->concave_up, sizeof(out->concave_up), "none");
        }
        if (out->concave_down[0] == '\0') {
            snprintf(out->concave_down, sizeof(out->concave_down), "none");
        }
    }
    if (math_fraction_is_zero(denominator_intercept)
            && (numerator_root.degree == 0
                || math_fraction_is_zero(determinant))) {
        snprintf(out->parity, sizeof(out->parity), "Even");
    } else if (!math_fraction_is_zero(denominator_intercept)) {
        snprintf(out->parity, sizeof(out->parity),
                 "Neither (domain is not symmetric)");
    } else {
        snprintf(out->parity, sizeof(out->parity), "Neither");
    }
    out->warning[0] = '\0';
    math_expression_to_text(expression, expression_text, sizeof(expression_text));
    steps_reset(&out->steps);
    step_add(&out->steps, "1. Analyze f(x)=%s.", expression_text);
    step_add(&out->steps, "2. Recognize sqrt((p(x)/q(x))^2)=|p(x)/q(x)|.");
    step_add(&out->steps, "3. Preserve denominator zeros in the domain.");
    step_add(&out->steps, "4. Range: %s.", out->range);
    step_add(&out->steps, "5. Intervals: increasing %s; decreasing %s.",
             out->increasing, out->decreasing);
    step_add(&out->steps, "6. Concavity: up %s; down %s; parity %s.",
             out->concave_up, out->concave_down, out->parity);
    out->status = MATH_OK;
    return 1;
}

static int format_piece_interval(const MathPiecewiseBranch *branch,
                                 char *out, size_t capacity) {
    char lower[64];
    char upper[64];
    if (!math_fraction_to_text(branch->lower, lower, sizeof(lower))
            || !math_fraction_to_text(branch->upper, upper, sizeof(upper))) {
        return 0;
    }
    return snprintf(out, capacity, "%c%s,%s%c",
                    branch->lower_closed ? '[' : '(', lower, upper,
                    branch->upper_closed ? ']' : ')') >= 0 && strlen(out) < capacity;
}

static int append_union_text(char *out, size_t capacity, const char *piece) {
    size_t length = strlen(out);
    const char *cursor = out;

    if (strcmp(out, piece) == 0) {
        return 1;
    }
    while (*cursor != '\0') {
        const char *separator = strstr(cursor, " U ");
        size_t token_length = separator == NULL
            ? strlen(cursor) : (size_t) (separator - cursor);
        if (strlen(piece) == token_length
                && strncmp(cursor, piece, token_length) == 0) {
            return 1;
        }
        if (separator == NULL) {
            break;
        }
        cursor = separator + 3;
    }
    if (length == 0) {
        return snprintf(out, capacity, "%s", piece) >= 0 && strlen(out) < capacity;
    }
    return snprintf(out + length, capacity - length, " U %s", piece) >= 0
        && strlen(out) < capacity;
}

typedef struct {
    MathFraction value;
    uint8_t included;
} MathRangePoint;

typedef struct {
    MathFraction lower;
    MathFraction upper;
    uint8_t lower_included;
    uint8_t upper_included;
    uint8_t valid;
} MathRangeInterval;

static void range_point_add(MathRangePoint *points, int *count,
                            MathFraction value, int included) {
    int index;
    for (index = 0; index < *count; index++) {
        if (math_fraction_equal(points[index].value, value)) {
            points[index].included = points[index].included || included;
            return;
        }
    }
    if (*count < 4) {
        points[*count].value = value;
        points[*count].included = (uint8_t) included;
        (*count)++;
    }
}

static int format_piece_range(const MathPiecewiseBranch *branch,
                              char *out, size_t capacity,
                              MathRangeInterval *range) {
    MathPolynomial public_poly;
    MathPoly poly;
    MathRangePoint points[4];
    MathFraction vertex;
    int point_count = 0;
    int index;
    MathRangePoint minimum;
    MathRangePoint maximum;
    char lower[64];
    char upper[64];

    if (math_expression_to_polynomial(&branch->expression, &public_poly) != MATH_OK
            || public_poly.degree > 2) {
        range->valid = 0;
        return snprintf(out, capacity, "unsupported branch range") >= 0
            && strlen(out) < capacity;
    }
    poly.degree = public_poly.degree;
    memcpy(poly.coefficient, public_poly.coefficient, sizeof(poly.coefficient));
    if (poly.degree == 0) {
        char value[64];
        range->lower = poly.coefficient[0];
        range->upper = poly.coefficient[0];
        range->lower_included = 1;
        range->upper_included = 1;
        range->valid = 1;
        math_fraction_to_text(poly.coefficient[0], value, sizeof(value));
        return snprintf(out, capacity, "{%s}", value) >= 0 && strlen(out) < capacity;
    }
    range_point_add(points, &point_count,
                    polynomial_evaluate_fraction(&poly, branch->lower),
                    branch->lower_closed);
    range_point_add(points, &point_count,
                    polynomial_evaluate_fraction(&poly, branch->upper),
                    branch->upper_closed);
    if (poly.degree == 2) {
        vertex = fraction_divide(fraction_negate(poly.coefficient[1]),
                                 fraction_multiply(math_fraction(2, 1),
                                                   poly.coefficient[2]));
        if (math_fraction_compare(vertex, branch->lower) > 0
                && math_fraction_compare(vertex, branch->upper) < 0) {
            range_point_add(points, &point_count,
                            polynomial_evaluate_fraction(&poly, vertex), 1);
        } else if (math_fraction_equal(vertex, branch->lower)
                && branch->lower_closed) {
            range_point_add(points, &point_count,
                            polynomial_evaluate_fraction(&poly, vertex), 1);
        } else if (math_fraction_equal(vertex, branch->upper)
                && branch->upper_closed) {
            range_point_add(points, &point_count,
                            polynomial_evaluate_fraction(&poly, vertex), 1);
        }
    }
    if (point_count == 0) {
        range->valid = 0;
        return snprintf(out, capacity, "unsupported empty range") >= 0
            && strlen(out) < capacity;
    }
    minimum = points[0];
    maximum = points[0];
    for (index = 1; index < point_count; index++) {
        int low_comparison = math_fraction_compare(points[index].value, minimum.value);
        int high_comparison = math_fraction_compare(points[index].value, maximum.value);
        if (low_comparison < 0) {
            minimum = points[index];
        } else if (low_comparison == 0) {
            minimum.included = minimum.included || points[index].included;
        }
        if (high_comparison > 0) {
            maximum = points[index];
        } else if (high_comparison == 0) {
            maximum.included = maximum.included || points[index].included;
        }
    }
    math_fraction_to_text(minimum.value, lower, sizeof(lower));
    math_fraction_to_text(maximum.value, upper, sizeof(upper));
    range->lower = minimum.value;
    range->upper = maximum.value;
    range->lower_included = minimum.included;
    range->upper_included = maximum.included;
    range->valid = 1;
    return snprintf(out, capacity, "%c%s,%s%c", minimum.included ? '[' : '(',
                    lower, upper, maximum.included ? ']' : ')') >= 0
        && strlen(out) < capacity;
}

static void append_behavior_interval(char *out, size_t capacity,
                                     const MathPiecewiseBranch *branch,
                                     MathFraction lower, MathFraction upper) {
    char interval[96];
    if (math_fraction_compare(lower, upper) >= 0) {
        return;
    }
    snprintf(interval, sizeof(interval), "(%ld/%ld,%ld/%ld)",
             (long) lower.num, (long) lower.den,
             (long) upper.num, (long) upper.den);
    if (lower.den == 1 && upper.den == 1) {
        snprintf(interval, sizeof(interval), "(%ld,%ld)",
                 (long) lower.num, (long) upper.num);
    }
    append_union_text(out, capacity, interval);
    (void) branch;
}

static void append_piece_behavior(const MathPiecewiseBranch *branch,
                                  char *increasing, char *decreasing,
                                  char *concave_up, char *concave_down,
                                  size_t capacity) {
    MathPolynomial public_poly;
    MathPoly poly;
    MathFraction vertex;

    if (math_expression_to_polynomial(&branch->expression, &public_poly) != MATH_OK
            || public_poly.degree > 2) {
        return;
    }
    poly.degree = public_poly.degree;
    memcpy(poly.coefficient, public_poly.coefficient, sizeof(poly.coefficient));
    if (poly.degree == 1) {
        if (poly.coefficient[1].num > 0) {
            append_behavior_interval(increasing, capacity, branch,
                                     branch->lower, branch->upper);
        } else if (poly.coefficient[1].num < 0) {
            append_behavior_interval(decreasing, capacity, branch,
                                     branch->lower, branch->upper);
        }
    } else if (poly.degree == 2) {
        char interval[96];
        vertex = fraction_divide(fraction_negate(poly.coefficient[1]),
                                 fraction_multiply(math_fraction(2, 1),
                                                   poly.coefficient[2]));
        format_piece_interval(branch, interval, sizeof(interval));
        if (poly.coefficient[2].num > 0) {
            append_union_text(concave_up, capacity, interval);
        } else {
            append_union_text(concave_down, capacity, interval);
        }
        if (poly.coefficient[2].num > 0) {
            append_behavior_interval(decreasing, capacity, branch,
                                     branch->lower,
                                     math_fraction_compare(vertex, branch->upper) < 0
                                     ? vertex : branch->upper);
            append_behavior_interval(increasing, capacity, branch,
                                     math_fraction_compare(vertex, branch->lower) > 0
                                     ? vertex : branch->lower,
                                     branch->upper);
        } else {
            append_behavior_interval(increasing, capacity, branch,
                                     branch->lower,
                                     math_fraction_compare(vertex, branch->upper) < 0
                                     ? vertex : branch->upper);
            append_behavior_interval(decreasing, capacity, branch,
                                     math_fraction_compare(vertex, branch->lower) > 0
                                     ? vertex : branch->lower,
                                     branch->upper);
        }
    }
}

static int piecewise_matching_branch(const MathPiecewise *piecewise,
                                     uint8_t source_index) {
    const MathPiecewiseBranch *source = &piecewise->branch[source_index];
    uint8_t index;
    MathFraction reflected_lower = fraction_negate(source->upper);
    MathFraction reflected_upper = fraction_negate(source->lower);
    for (index = 0; index < piecewise->count; index++) {
        const MathPiecewiseBranch *candidate = &piecewise->branch[index];
        if (math_fraction_equal(candidate->lower, reflected_lower)
                && math_fraction_equal(candidate->upper, reflected_upper)
                && candidate->lower_closed == source->upper_closed
                && candidate->upper_closed == source->lower_closed) {
            return index;
        }
    }
    return -1;
}

static int piecewise_parity(const MathPiecewise *piecewise, const char **result) {
    uint8_t index;
    int even = 1;
    int odd = 1;
    for (index = 0; index < piecewise->count; index++) {
        if (piecewise_matching_branch(piecewise, index) < 0) {
            *result = "Neither (domain is not symmetric)";
            return 1;
        }
    }
    for (index = 0; index < piecewise->count; index++) {
        int match = piecewise_matching_branch(piecewise, index);
        MathPolynomial source_public;
        MathPolynomial target_public;
        MathPoly reflected;
        MathPoly target;
        int coefficient;
        if (match < 0
                || math_expression_to_polynomial(&piecewise->branch[index].expression,
                                                  &source_public) != MATH_OK
                || math_expression_to_polynomial(&piecewise->branch[match].expression,
                                                  &target_public) != MATH_OK
                || source_public.degree != target_public.degree) {
            return 0;
        }
        reflected.degree = source_public.degree;
        target.degree = target_public.degree;
        memcpy(reflected.coefficient, source_public.coefficient,
               sizeof(reflected.coefficient));
        memcpy(target.coefficient, target_public.coefficient, sizeof(target.coefficient));
        for (coefficient = 1; coefficient <= reflected.degree; coefficient += 2) {
            reflected.coefficient[coefficient]
                = fraction_negate(reflected.coefficient[coefficient]);
        }
        if (!poly_equal(&reflected, &target)) {
            even = 0;
        }
        for (coefficient = 0; coefficient <= target.degree; coefficient++) {
            target.coefficient[coefficient]
                = fraction_negate(target.coefficient[coefficient]);
        }
        if (!poly_equal(&reflected, &target)) {
            odd = 0;
        }
    }
    if (even && odd) {
        *result = "Both even and odd (zero function)";
    } else if (even) {
        *result = "Even";
    } else if (odd) {
        *result = "Odd";
    } else {
        *result = "Neither";
    }
    return 1;
}

MathStatus math_analyze_piecewise(const MathPiecewise *piecewise,
                                  MathAnalysis *out) {
    uint8_t index;
    const char *parity_result;
    MathRangeInterval ranges[MATH_PIECEWISE_BRANCHES];
    uint8_t range_count = 0;
    int unsupported = 0;

    if (piecewise == NULL || out == NULL || piecewise->count == 0) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    out->status = MATH_OK;
    out->domain[0] = '\0';
    out->range[0] = '\0';
    out->increasing[0] = '\0';
    out->decreasing[0] = '\0';
    out->concave_up[0] = '\0';
    out->concave_down[0] = '\0';
    step_add(&out->steps, "1. Analyze %u piecewise branches.", piecewise->count);
    for (index = 0; index < piecewise->count; index++) {
        char interval[96];
        char branch_range[MATH_OUTPUT_WIDTH];
        if (!format_piece_interval(&piecewise->branch[index], interval,
                                   sizeof(interval))) {
            return MATH_ERR_LIMIT;
        }
        append_union_text(out->domain, sizeof(out->domain), interval);
        if (!format_piece_range(&piecewise->branch[index], branch_range,
                                sizeof(branch_range), &ranges[range_count])) {
            unsupported = 1;
        }
        if (ranges[range_count].valid) {
            range_count++;
        } else {
            unsupported = 1;
        }
        append_piece_behavior(&piecewise->branch[index], out->increasing,
                              out->decreasing, out->concave_up,
                              out->concave_down, sizeof(out->increasing));
        if (strstr(branch_range, "unsupported") != NULL) {
            unsupported = 1;
        }
    }
    if (range_count > 0) {
        uint8_t merge_index;
        for (index = 0; index + 1 < range_count; index++) {
            for (merge_index = (uint8_t) (index + 1);
                 merge_index < range_count; merge_index++) {
                if (math_fraction_compare(ranges[merge_index].lower,
                                          ranges[index].lower) < 0) {
                    MathRangeInterval swap = ranges[index];
                    ranges[index] = ranges[merge_index];
                    ranges[merge_index] = swap;
                }
            }
        }
        out->range[0] = '\0';
        for (index = 0; index < range_count; index++) {
            MathRangeInterval merged = ranges[index];
            char interval[128];
            while (index + 1 < range_count) {
                MathRangeInterval next = ranges[index + 1];
                int comparison = math_fraction_compare(next.lower,
                                                        merged.upper);
                if (comparison > 0
                        || (comparison == 0
                            && !merged.upper_included
                            && !next.lower_included)) {
                    break;
                }
                if (math_fraction_compare(next.upper, merged.upper) > 0) {
                    merged.upper = next.upper;
                    merged.upper_included = next.upper_included;
                } else if (math_fraction_equal(next.upper, merged.upper)) {
                    merged.upper_included = (uint8_t) (merged.upper_included
                                                       || next.upper_included);
                }
                index++;
            }
            if (math_fraction_equal(merged.lower, merged.upper)
                    && merged.lower_included && merged.upper_included) {
                char value[64];
                math_fraction_to_text(merged.lower, value, sizeof(value));
                snprintf(interval, sizeof(interval), "{%s}", value);
            } else {
                char lower[64];
                char upper[64];
                math_fraction_to_text(merged.lower, lower, sizeof(lower));
                math_fraction_to_text(merged.upper, upper, sizeof(upper));
                snprintf(interval, sizeof(interval), "%c%s,%s%c",
                         merged.lower_included ? '[' : '(', lower, upper,
                         merged.upper_included ? ']' : ')');
            }
            append_union_text(out->range, sizeof(out->range), interval);
        }
    }
    if (out->range[0] == '\0') {
        snprintf(out->range, sizeof(out->range),
                 "Exact range not supported for these branches");
    }
    if (piecewise_parity(piecewise, &parity_result)) {
        snprintf(out->parity, sizeof(out->parity), "%s", parity_result);
    } else {
        snprintf(out->parity, sizeof(out->parity),
                 "Not determined exactly for this piecewise input");
        unsupported = 1;
    }
    if (out->increasing[0] == '\0') {
        snprintf(out->increasing, sizeof(out->increasing), "none");
    }
    if (out->decreasing[0] == '\0') {
        snprintf(out->decreasing, sizeof(out->decreasing), "none");
    }
    if (out->concave_up[0] == '\0') {
        snprintf(out->concave_up, sizeof(out->concave_up), "none");
    }
    if (out->concave_down[0] == '\0') {
        snprintf(out->concave_down, sizeof(out->concave_down), "none");
    }
    step_add(&out->steps, "2. Domain is the union of the branch intervals.");
    step_add(&out->steps, "3. Range is computed branch by branch with endpoint inclusion.");
    step_add(&out->steps, "4. Slopes, vertices, and concavity are clipped to each branch.");
    if (unsupported) {
        snprintf(out->warning, sizeof(out->warning),
                 "All exact branch results require polynomial degree 0 through 2.");
    }
    return MATH_OK;
}

static int format_vertex_form(const MathPoly *poly, char *out, size_t capacity) {
    MathFraction vertex_x;
    MathFraction vertex_y;
    char coefficient_text[64];
    char vertex_x_text[64];
    char vertex_y_text[64];
    char inside[128];

    if (poly->degree != 2
            || !math_fraction_to_text(poly->coefficient[2], coefficient_text,
                                      sizeof(coefficient_text))) {
        return snprintf(out, capacity, "none (not quadratic)") >= 0
            && strlen(out) < capacity;
    }
    vertex_x = fraction_divide(fraction_negate(poly->coefficient[1]),
                               fraction_multiply(math_fraction(2, 1),
                                                 poly->coefficient[2]));
    vertex_y = polynomial_evaluate_fraction(poly, vertex_x);
    if (!math_fraction_to_text(vertex_x, vertex_x_text, sizeof(vertex_x_text))
            || !math_fraction_to_text(vertex_y, vertex_y_text,
                                      sizeof(vertex_y_text))) {
        return 0;
    }
    if (math_fraction_is_zero(vertex_x)) {
        snprintf(inside, sizeof(inside), "x");
    } else if (vertex_x.num > 0) {
        snprintf(inside, sizeof(inside), "x-%s", vertex_x_text);
    } else {
        snprintf(inside, sizeof(inside), "x+%s",
                 vertex_x_text[0] == '-' ? vertex_x_text + 1 : vertex_x_text);
    }
    if (math_fraction_equal(poly->coefficient[2], math_fraction_one())) {
        snprintf(out, capacity, "(%s)^2", inside);
    } else if (math_fraction_equal(poly->coefficient[2], math_fraction(-1, 1))) {
        snprintf(out, capacity, "-(%s)^2", inside);
    } else {
        snprintf(out, capacity, "%s*(%s)^2", coefficient_text, inside);
    }
    if (vertex_y.num > 0) {
        return snprintf(out + strlen(out), capacity - strlen(out), "+%s",
                        vertex_y_text) >= 0 && strlen(out) < capacity;
    }
    if (vertex_y.num < 0) {
        char absolute_y[64];
        if (!math_fraction_to_text(fraction_absolute(vertex_y), absolute_y,
                                   sizeof(absolute_y))) {
            return 0;
        }
        return snprintf(out + strlen(out), capacity - strlen(out), "-%s",
                        absolute_y) >= 0 && strlen(out) < capacity;
    }
    return 1;
}

MathStatus math_transform_quadratic(const MathExpr *expression,
                                    const MathTransformSpec *spec,
                                    MathTransformResult *out) {
    MathPolynomial source_public;
    MathPoly source;
    MathPoly transformed;
    MathFraction a;
    MathFraction b;
    MathFraction c;
    MathFraction horizontal;
    MathFraction vertical;
    MathFraction h;
    MathFraction k;
    MathFraction denominator;
    char source_text[160];
    char coefficient_text[160];
    char vertex_x[64];
    char vertex_y[64];
    char intercept[128];
    MathFraction vertex_fraction;
    MathFraction discriminant;
    MathFraction sqrt_discriminant;

    if (expression == NULL || spec == NULL || out == NULL) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    if (math_expression_to_polynomial(expression, &source_public) != MATH_OK
            || source_public.degree < 0 || source_public.degree > 2) {
        snprintf(out->warning, sizeof(out->warning),
                 "Transform output is exact for constant, linear, or quadratic input.");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    memcpy(source.coefficient, source_public.coefficient, sizeof(source.coefficient));
    source.degree = source_public.degree;
    h = spec->horizontal_shift;
    k = spec->vertical_shift;
    horizontal = spec->horizontal_scale;
    vertical = spec->vertical_scale;
    if (horizontal.den == 0 || vertical.den == 0 || math_fraction_is_zero(horizontal)) {
        snprintf(out->warning, sizeof(out->warning), "Scale factors must be nonzero.");
        out->status = MATH_ERR_INPUT;
        return out->status;
    }
    if (spec->reflect_y_axis) {
        horizontal = fraction_negate(horizontal);
    }
    if (spec->reflect_x_axis) {
        vertical = fraction_negate(vertical);
    }
    a = source.coefficient[2];
    b = source.coefficient[1];
    c = source.coefficient[0];
    poly_zero(&transformed);
    if (source.degree == 2) {
        denominator = fraction_multiply(horizontal, horizontal);
        transformed.degree = 2;
        transformed.coefficient[2] = fraction_multiply(vertical,
            fraction_divide(a, denominator));
        transformed.coefficient[1] = fraction_multiply(vertical,
            fraction_add(fraction_divide(fraction_negate(
                fraction_multiply(math_fraction(2, 1), fraction_multiply(a, h))),
                denominator), fraction_divide(b, horizontal)));
        transformed.coefficient[0] = fraction_add(k, fraction_multiply(vertical,
            fraction_add(fraction_add(fraction_divide(fraction_multiply(a,
                fraction_multiply(h, h)), denominator),
                fraction_divide(fraction_negate(fraction_multiply(b, h)), horizontal)), c)));
    } else if (source.degree == 1) {
        a = source.coefficient[1];
        b = source.coefficient[0];
        transformed.degree = 1;
        transformed.coefficient[1] = fraction_multiply(vertical,
            fraction_divide(a, horizontal));
        transformed.coefficient[0] = fraction_add(k, fraction_multiply(vertical,
            fraction_subtract(b, fraction_divide(fraction_multiply(a, h), horizontal))));
    } else {
        transformed.degree = 0;
        transformed.coefficient[0] = fraction_add(k,
            fraction_multiply(vertical, c));
    }
    poly_trim(&transformed);
    math_expression_to_text(expression, source_text, sizeof(source_text));
    format_polynomial(&transformed, coefficient_text, sizeof(coefficient_text));
    snprintf(out->formula, sizeof(out->formula), "%s", coefficient_text);
    format_vertex_form(&transformed, out->vertex_form, sizeof(out->vertex_form));
    if (transformed.degree == 2) {
        vertex_fraction = fraction_divide(
            fraction_negate(transformed.coefficient[1]),
            fraction_multiply(math_fraction(2, 1), transformed.coefficient[2]));
        math_fraction_to_text(vertex_fraction, vertex_x, sizeof(vertex_x));
        math_fraction_to_text(polynomial_evaluate_fraction(&transformed,
                                                           vertex_fraction),
                              vertex_y, sizeof(vertex_y));
        snprintf(out->vertex, sizeof(out->vertex), "(%s,%s)", vertex_x, vertex_y);
    } else {
        snprintf(out->vertex, sizeof(out->vertex), "none (not quadratic)");
    }
    math_fraction_to_text(transformed.coefficient[0], intercept, sizeof(intercept));
    snprintf(out->y_intercept, sizeof(out->y_intercept), "(0,%s)", intercept);
    if (transformed.degree == 0) {
        snprintf(out->x_intercepts, sizeof(out->x_intercepts),
                 math_fraction_is_zero(transformed.coefficient[0])
                     ? "all real x" : "none");
    } else if (transformed.degree == 1) {
        if (math_fraction_is_zero(transformed.coefficient[1])) {
            snprintf(out->x_intercepts, sizeof(out->x_intercepts),
                     math_fraction_is_zero(transformed.coefficient[0])
                         ? "all real x" : "none");
        } else {
            MathFraction intercept_x = fraction_divide(
                fraction_negate(transformed.coefficient[0]),
                transformed.coefficient[1]);
            math_fraction_to_text(intercept_x, intercept, sizeof(intercept));
            snprintf(out->x_intercepts, sizeof(out->x_intercepts), "(%s,0)", intercept);
        }
    } else {
        discriminant = fraction_subtract(
            fraction_multiply(transformed.coefficient[1], transformed.coefficient[1]),
            fraction_multiply(math_fraction(4, 1), fraction_multiply(
                transformed.coefficient[2], transformed.coefficient[0])));
        if (discriminant.num < 0) {
            snprintf(out->x_intercepts, sizeof(out->x_intercepts), "none (complex roots)");
        } else if (discriminant.num == 0) {
            MathFraction intercept_x = fraction_divide(
                fraction_negate(transformed.coefficient[1]),
                fraction_multiply(math_fraction(2, 1), transformed.coefficient[2]));
            math_fraction_to_text(intercept_x, intercept, sizeof(intercept));
            snprintf(out->x_intercepts, sizeof(out->x_intercepts), "(%s,0)", intercept);
        } else if (fraction_is_perfect_square(discriminant, &sqrt_discriminant)) {
            MathFraction first = fraction_divide(
                fraction_add(fraction_negate(transformed.coefficient[1]), sqrt_discriminant),
                fraction_multiply(math_fraction(2, 1), transformed.coefficient[2]));
            MathFraction second = fraction_divide(
                fraction_subtract(fraction_negate(transformed.coefficient[1]), sqrt_discriminant),
                fraction_multiply(math_fraction(2, 1), transformed.coefficient[2]));
            char first_text[64];
            char second_text[64];
            if (math_fraction_compare(first, second) > 0) {
                MathFraction swap = first;
                first = second;
                second = swap;
            }
            math_fraction_to_text(first, first_text, sizeof(first_text));
            math_fraction_to_text(second, second_text, sizeof(second_text));
            snprintf(out->x_intercepts, sizeof(out->x_intercepts),
                     "(%s,0),(%s,0)", first_text, second_text);
        } else {
            char radical[96];
            format_square_root(discriminant, radical, sizeof(radical));
            snprintf(out->x_intercepts, sizeof(out->x_intercepts),
                     "(-b+sqrt(D))/(2a),(-b-sqrt(D))/(2a), D=%s", radical);
        }
    }
    out->status = MATH_OK;
    step_add(&out->steps, "1. Start with f(x)=%s.", source_text);
    step_add(&out->steps, "2. Apply g(x)=a*f((x-h)/s)+k with exact scale factors.");
    step_add(&out->steps, "3. Simplify: g(x)=%s.", out->formula);
    step_add(&out->steps, "4. Vertex=%s; x-intercepts=%s; y-intercept=%s.",
             out->vertex, out->x_intercepts, out->y_intercept);
    return MATH_OK;
}

static int evaluate_rational_fraction(const MathExpr *expression,
                                      MathFraction x, MathFraction *value) {
    MathRational rational;
    MathFraction numerator;
    MathFraction denominator;
    if (!rational_from_node(expression, expression->root, &rational)) {
        return 0;
    }
    numerator = polynomial_evaluate_fraction(&rational.numerator, x);
    denominator = polynomial_evaluate_fraction(&rational.denominator, x);
    if (math_fraction_is_zero(denominator)) {
        return 0;
    }
    *value = fraction_divide(numerator, denominator);
    return value->den != 0;
}

static MathStatus average_rate_from_values(MathFraction first_x,
                                           MathFraction second_x,
                                           MathFraction first_value,
                                           MathFraction second_value,
                                           MathAverageRateResult *out) {
    MathFraction numerator;
    MathFraction denominator;
    char first_x_text[64];
    char second_x_text[64];
    char first_value_text[64];
    char second_value_text[64];

    if (math_fraction_equal(first_x, second_x)) {
        snprintf(out->warning, sizeof(out->warning),
                 "The two x-values must be different.");
        out->status = MATH_ERR_INPUT;
        return out->status;
    }
    numerator = fraction_subtract(second_value, first_value);
    denominator = fraction_subtract(second_x, first_x);
    if (!math_fraction_to_text(first_x, first_x_text, sizeof(first_x_text))
            || !math_fraction_to_text(second_x, second_x_text,
                                      sizeof(second_x_text))
            || !math_fraction_to_text(first_value, first_value_text,
                                      sizeof(first_value_text))
            || !math_fraction_to_text(second_value, second_value_text,
                                      sizeof(second_value_text))
            || !math_fraction_to_text(fraction_divide(numerator, denominator),
                                      out->answer, sizeof(out->answer))) {
        out->status = MATH_ERR_LIMIT;
        return out->status;
    }
    step_add(&out->steps, "1. Compute f(%s)=%s and f(%s)=%s.", first_x_text,
             first_value_text, second_x_text, second_value_text);
    step_add(&out->steps, "2. Average rate=(f(x2)-f(x1))/(x2-x1).");
    step_add(&out->steps, "3. Average rate=(%s-%s)/(%s-%s)=%s.",
             second_value_text, first_value_text, second_x_text, first_x_text,
             out->answer);
    out->status = MATH_OK;
    return out->status;
}

MathStatus math_average_rate(const MathExpr *expression,
                             MathFraction first_x, MathFraction second_x,
                             MathAverageRateResult *out) {
    MathFraction first_value;
    MathFraction second_value;

    if (expression == NULL || out == NULL) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    if (!evaluate_rational_fraction(expression, first_x, &first_value)
            || !evaluate_rational_fraction(expression, second_x, &second_value)) {
        snprintf(out->warning, sizeof(out->warning),
                 "The function cannot be evaluated exactly at both x-values.");
        out->status = MATH_ERR_UNSUPPORTED;
        return out->status;
    }
    return average_rate_from_values(first_x, second_x, first_value, second_value,
                                    out);
}

static int piecewise_branch_contains(const MathPiecewiseBranch *branch,
                                     MathFraction x) {
    int lower = math_fraction_compare(x, branch->lower);
    int upper = math_fraction_compare(x, branch->upper);
    return (lower > 0 || (lower == 0 && branch->lower_closed))
        && (upper < 0 || (upper == 0 && branch->upper_closed));
}

static int evaluate_piecewise_fraction(const MathPiecewise *piecewise,
                                       MathFraction x, MathFraction *value) {
    uint8_t index;
    for (index = 0; index < piecewise->count; index++) {
        if (piecewise_branch_contains(&piecewise->branch[index], x)
                && evaluate_rational_fraction(&piecewise->branch[index].expression,
                                              x, value)) {
            return 1;
        }
    }
    return 0;
}

MathStatus math_average_rate_piecewise(const MathPiecewise *piecewise,
                                        MathFraction first_x,
                                        MathFraction second_x,
                                        MathAverageRateResult *out) {
    MathFraction first_value;
    MathFraction second_value;

    if (piecewise == NULL || out == NULL || piecewise->count == 0) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    if (!evaluate_piecewise_fraction(piecewise, first_x, &first_value)
            || !evaluate_piecewise_fraction(piecewise, second_x, &second_value)) {
        snprintf(out->warning, sizeof(out->warning),
                 "Both x-values must lie in the defined piecewise domain.");
        out->status = MATH_ERR_INPUT;
        return out->status;
    }
    return average_rate_from_values(first_x, second_x, first_value, second_value,
                                    out);
}

MathStatus math_quadratic_regression(const MathFraction *x_values,
                                     const MathFraction *y_values,
                                     size_t count,
                                     MathRegressionResult *out) {
    MathFraction matrix[3][4];
    MathFraction coefficients[3];
    MathPoly polynomial;
    size_t row;
    size_t column;

    if (x_values == NULL || y_values == NULL || out == NULL || count != 3) {
        return MATH_ERR_INPUT;
    }
    memset(out, 0, sizeof(*out));
    steps_reset(&out->steps);
    for (row = 0; row < 3; row++) {
        MathFraction square = fraction_multiply(x_values[row], x_values[row]);
        matrix[row][0] = square;
        matrix[row][1] = x_values[row];
        matrix[row][2] = math_fraction_one();
        matrix[row][3] = y_values[row];
    }
    for (column = 0; column < 3; column++) {
        size_t pivot = column;
        size_t index;
        while (pivot < 3 && math_fraction_is_zero(matrix[pivot][column])) {
            pivot++;
        }
        if (pivot == 3) {
            snprintf(out->warning, sizeof(out->warning),
                     "The three data points do not determine one quadratic.");
            out->status = MATH_ERR_INPUT;
            return out->status;
        }
        if (pivot != column) {
            for (index = column; index < 4; index++) {
                MathFraction swap = matrix[column][index];
                matrix[column][index] = matrix[pivot][index];
                matrix[pivot][index] = swap;
            }
        }
        for (index = column + 1; index < 4; index++) {
            matrix[column][index] = fraction_divide(matrix[column][index],
                                                    matrix[column][column]);
        }
        matrix[column][column] = math_fraction_one();
        for (index = 0; index < 3; index++) {
            size_t eliminate;
            if (index == column) {
                continue;
            }
            {
                MathFraction factor = matrix[index][column];
                for (eliminate = column; eliminate < 4; eliminate++) {
                    matrix[index][eliminate] = fraction_subtract(
                        matrix[index][eliminate],
                        fraction_multiply(factor, matrix[column][eliminate]));
                }
            }
        }
    }
    coefficients[0] = matrix[0][3];
    coefficients[1] = matrix[1][3];
    coefficients[2] = matrix[2][3];
    polynomial.degree = 2;
    polynomial.coefficient[2] = coefficients[0];
    polynomial.coefficient[1] = coefficients[1];
    polynomial.coefficient[0] = coefficients[2];
    if (!format_polynomial(&polynomial, out->equation,
                           sizeof(out->equation))
            || !math_fraction_to_text(coefficients[0], out->coefficient_a,
                                      sizeof(out->coefficient_a))
            || !math_fraction_to_text(coefficients[1], out->coefficient_b,
                                      sizeof(out->coefficient_b))
            || !math_fraction_to_text(coefficients[2], out->coefficient_c,
                                      sizeof(out->coefficient_c))) {
        out->status = MATH_ERR_LIMIT;
        return out->status;
    }
    step_add(&out->steps, "1. Set y=ax^2+bx+c and substitute the three points.");
    step_add(&out->steps, "2. Solve the exact 3-by-3 linear system.");
    step_add(&out->steps, "3. a=%s, b=%s, c=%s.", out->coefficient_a,
             out->coefficient_b, out->coefficient_c);
    step_add(&out->steps, "4. Model: y=%s.", out->equation);
    out->status = MATH_OK;
    return out->status;
}
