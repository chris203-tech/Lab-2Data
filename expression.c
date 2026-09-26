#include "expression.h"
#include "stack.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ExprResult result(ExprStatus status, long value, size_t position,
                         const char *message) {
    ExprResult r = {.status = status, .value = value,
                    .position = position, .message = ""};

    if (message != NULL)
        (void)snprintf(r.message, sizeof r.message, "%s", message);

    return r;
}

static bool is_opener(char c) {
    return c == '(' || c == '[' || c == '{';
}

static bool is_closer(char c) {
    return c == ')' || c == ']' || c == '}';
}

static char expected_opener(char closer) {
    switch (closer) {
    case ')':
        return '(';
    case ']':
        return '[';
    case '}':
        return '{';
    default:
        return '\0';
    }
}

static bool is_operator_token(const char *token) {
    return token[0] != '\0' &&
           token[1] == '\0' &&
           strchr("+-*/%", token[0]) != NULL;
}

static bool parse_long_token(const char *token, long *out) {
    if (token == NULL || token[0] == '\0')
        return false;

    char *end = NULL;
    long value = strtol(token, &end, 10);

    if (*end != '\0')
        return false;

    *out = value;
    return true;
}

static bool apply_operator(char op, long left, long right, long *out) {
    switch (op) {
    case '+':
        *out = left + right;
        break;

    case '-':
        *out = left - right;
        break;

    case '*':
        *out = left * right;
        break;

    case '/':
        if (right == 0)
            return false;
        *out = left / right;
        break;

    case '%':
        if (right == 0)
            return false;
        *out = left % right;
        break;

    default:
        return false;
    }

    return true;
}

const char *expr_status_name(ExprStatus status) {
    switch (status) {
    case EXPR_OK:
        return "OK";
    case EXPR_UNMATCHED_CLOSER:
        return "UNMATCHED_CLOSER";
    case EXPR_UNCLOSED_OPENER:
        return "UNCLOSED_OPENER";
    case EXPR_MISMATCHED_DELIMITER:
        return "MISMATCHED_DELIMITER";
    case EXPR_INVALID_TOKEN:
        return "INVALID_TOKEN";
    case EXPR_TOO_FEW_OPERANDS:
        return "TOO_FEW_OPERANDS";
    case EXPR_TOO_MANY_OPERANDS:
        return "TOO_MANY_OPERANDS";
    case EXPR_DIVIDE_BY_ZERO:
        return "DIVIDE_BY_ZERO";
    case EXPR_OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    }

    return "UNKNOWN";
}

ExprResult check_delimiters(const char *text) {
    Stack stack = stack_create();

    for (size_t i = 0; text[i] != '\0'; i++) {
        char current = text[i];

        if (is_opener(current)) {
            if (!stack_push(&stack, (long)current)) {
                stack_destroy(&stack);
                return result(EXPR_OUT_OF_MEMORY, 0, i,
                              "could not create delimiter stack node");
            }
        }
        else if (is_closer(current)) {
            long openChar = 0;

            if (!stack_pop(&stack, &openChar)) {
                stack_destroy(&stack);
                return result(EXPR_UNMATCHED_CLOSER, 0, i,
                              "no matching opening delimiter");
            }

            if ((char)openChar != expected_opener(current)) {
                stack_destroy(&stack);
                return result(EXPR_MISMATCHED_DELIMITER, 0, i,
                              "opening and closing delimiters do not match");
            }
        }
    }

    if (!stack_is_empty(&stack)) {
        stack_destroy(&stack);
        return result(EXPR_UNCLOSED_OPENER, 0, 0,
                      "an opening delimiter was not closed");
    }

    stack_destroy(&stack);

    return result(EXPR_OK, 0, 0, "balanced");
}

ExprResult eval_postfix(const char *expression, bool trace) {
    Stack operands = stack_create();

    char *copy = malloc(strlen(expression) + 1);

    if (copy == NULL)
        return result(EXPR_OUT_OF_MEMORY, 0, 0,
                      "could not create expression copy");

    strcpy(copy, expression);

    size_t token_number = 0;

    for (char *token = strtok(copy, " \t\r\n");
         token != NULL;
         token = strtok(NULL, " \t\r\n")) {

        token_number++;

        long number = 0;

        if (parse_long_token(token, &number)) {
            if (!stack_push(&operands, number)) {
                free(copy);
                stack_destroy(&operands);

                return result(EXPR_OUT_OF_MEMORY, 0, token_number,
                              "could not create operand stack node");
            }
        }
        else if (is_operator_token(token)) {
            long rightSide = 0;
            long leftSide = 0;
            long answer = 0;

            if (!stack_pop(&operands, &rightSide)) {
                free(copy);
                stack_destroy(&operands);

                return result(EXPR_TOO_FEW_OPERANDS, 0, token_number,
                              "operator requires two operands");
            }

            if (!stack_pop(&operands, &leftSide)) {
                free(copy);
                stack_destroy(&operands);

                return result(EXPR_TOO_FEW_OPERANDS, 0, token_number,
                              "operator requires two operands");
            }

            if (!apply_operator(token[0], leftSide, rightSide, &answer)) {
                free(copy);
                stack_destroy(&operands);

                return result(EXPR_DIVIDE_BY_ZERO, 0, token_number,
                              "cannot divide or take modulo by zero");
            }

            if (!stack_push(&operands, answer)) {
                free(copy);
                stack_destroy(&operands);

                return result(EXPR_OUT_OF_MEMORY, 0, token_number,
                              "could not store calculated result");
            }
        }
        else {
            char message[160];

            (void)snprintf(message, sizeof message,
                           "'%s' is not a valid number or operator",
                           token);

            free(copy);
            stack_destroy(&operands);

            return result(EXPR_INVALID_TOKEN, 0, token_number, message);
        }

        if (trace) {
            printf("%-10s ", token);
            stack_print(&operands, stdout);
            putchar('\n');
        }
    }

    free(copy);

    if (stack_size(&operands) == 0) {
        stack_destroy(&operands);

        return result(EXPR_TOO_FEW_OPERANDS, 0, token_number,
                      "expression produced no result");
    }

    if (stack_size(&operands) > 1) {
        stack_destroy(&operands);

        return result(EXPR_TOO_MANY_OPERANDS, 0, token_number,
                      "extra operands remain");
    }

    long finalValue = 0;

    stack_pop(&operands, &finalValue);
    stack_destroy(&operands);

    return result(EXPR_OK, finalValue, token_number,
                  "evaluation successful");
}