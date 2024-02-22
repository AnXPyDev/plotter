#include <stdio.h>
#include <malloc.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "mathconfig.h"

typedef unsigned int uint;

enum ExpressionType {
    TYPE_VALUE, TYPE_VARIABLE, TYPE_CALL
};

typedef struct {
    enum ExpressionType type;
    void *object;
} Expression;

typedef struct {
    Value value;
    char *error;
} Result;

typedef unsigned char VarIndex;

typedef struct {
    int occupied;
    Value value;
} VarSlot;

typedef struct {
    VarSlot vars[UCHAR_MAX];
} State;

struct IExpression {
    Result (*evaluate)(void *this, State *state);
    void (*destroy)(void *this);
    void (*print)(void *this, FILE *fp);
};

Result Expression_evaluate(Expression this, State *state);
void Expression_destroy(Expression this);
void Expression_free(Expression this);
void Expression_print(Expression this, FILE *fp);

struct Builtin;

typedef Result (*fn_builtin)(const struct Builtin *this, const Value *args, uint argc);
typedef struct Builtin {
    const char *token;
    fn_builtin function;
    void *payload;
} Builtin;

typedef struct {
    const Builtin *builtin;
    Expression *args;
    uint argc; 
} CallExpression;

#define this ((CallExpression*)vthis)

Result CallExpression_evaluate(void *vthis, State *state) {
    Value *args = alloca(this->argc * sizeof(Value));
    for (uint i = 0; i < this->argc; i++) {
        Result res = Expression_evaluate(this->args[i], state);
        if (res.error) {
            return res;
        }
        args[i] = res.value;
    }
    return this->builtin->function(this->builtin, args, this->argc);
}

void CallExpression_destroy(void *vthis) {
    for (uint i = 0; i < this->argc; i++) {
        Expression_destroy(this->args[i]);
        Expression_free(this->args[i]);
    }
    free(this->args);
}

void CallExpression_print(void *vthis, FILE *fp) {
    fprintf(fp, "(%s", this->builtin->token);
    for (uint i = 0; i < this->argc; i++) {
        fprintf(fp, " ");
        Expression_print(this->args[i], fp);
    }
    fprintf(fp, ")");
}

#undef this

const struct IExpression ICallExpression = {
    &CallExpression_evaluate,
    &CallExpression_destroy,
    &CallExpression_print
};

typedef struct {
    Value value;
} ValueExpression;

#define this ((ValueExpression*)vthis)

Result ValueExpression_evaluate(void *vthis, State *state) {
    Result result = { this->value, NULL };
    return result;
}

void ValueExpression_destroy(void *vthis) {}

void ValueExpression_print(void *vthis, FILE *fp) {
    fprintf(fp, "%lf", this->value);
}

#undef this

const struct IExpression IValueExpression = {
    &ValueExpression_evaluate,
    &ValueExpression_destroy,
    &ValueExpression_print
};

typedef struct {
    VarIndex index;
} VariableExpression;

#define this ((VariableExpression*)vthis)

Result VariableExpression_evaluate(void *vthis, State *state) {
    VarSlot *slot = &state->vars[this->index];
    Result result = { slot->value, NULL };
    if (!slot->occupied) {
        result.error = malloc(64);
        sprintf(result.error, "Variable is undefined: %c (%d)", this->index, (int)this->index);
    }

    return result;
}

void VariableExpression_destroy(void *vthis) {}

void VariableExpression_print(void *vthis, FILE *fp) {
    fprintf(fp, "%c", this->index);
}

#undef this

const struct IExpression IVariableExpression = {
    &VariableExpression_evaluate,
    &VariableExpression_destroy,
    &VariableExpression_print
};

Result Expression_evaluate(Expression this, State *state) {
    switch (this.type) {
        case TYPE_VALUE:
            return ValueExpression_evaluate(this.object, state);
        case TYPE_VARIABLE:
            return VariableExpression_evaluate(this.object, state);
        case TYPE_CALL:
            return CallExpression_evaluate(this.object, state);
    }
}

void Expression_destroy(Expression this) {
    switch (this.type) {
        case TYPE_VALUE:
            return ValueExpression_destroy(this.object);
        case TYPE_VARIABLE:
            return VariableExpression_destroy(this.object);
        case TYPE_CALL:
            return CallExpression_destroy(this.object);
    }
}

void Expression_free(Expression this) {
    return free(this.object);
}

void Expression_print(Expression this, FILE *fp) {
    switch (this.type) {
        case TYPE_VALUE:
            return ValueExpression_print(this.object, fp);
        case TYPE_VARIABLE:
            return VariableExpression_print(this.object, fp);
        case TYPE_CALL:
            return CallExpression_print(this.object, fp);
    }
}


// Builtins

Result builtin_add(const Builtin *this, const Value *args, uint argc) {
    Result result = { 0, NULL };
    for (uint i = 0; i < argc; i++) {
        result.value += args[i];
    }
    return result;
}

Result builtin_neg(const Builtin *this, const Value *args, uint argc) {
    Result result = { 0, NULL };
    for (uint i = 0; i < argc; i++) {
        result.value -= args[i];
    }
    return result;
}

Result builtin_sub(const Builtin *this, const Value *args, uint argc) {
    Result result = { 0, NULL };

    if (argc == 0) {
        return result;
    }

    if (argc == 1) {
        result.value -= args[0];
        return result;
    }

    result.value = args[0];

    for (uint i = 1; i < argc; i++) {
        result.value -= args[i];
    }
    return result;
}

Result builtin_mul(const Builtin *this, const Value *args, uint argc) {
    Result result = { 1, NULL };
    for (uint i = 0; i < argc; i++) {
        result.value *= args[i];
    }
    return result;
}

Result builtin_inv(const Builtin *this, const Value *args, uint argc) {
    Result result = { 1, NULL };
    for (uint i = 0; i < argc; i++) {
        result.value /= args[i];
    }
    return result;
}

Result builtin_div(const Builtin *this, const Value *args, uint argc) {
    Result result = { 1, NULL };

    if (argc == 0) {
        return result;
    }

    if (argc == 1) {
        result.value /= args[0];
        return result;
    }

    result.value = args[0];

    for (uint i = 1; i < argc; i++) {
        result.value /= args[i];
    }
    return result;
}

Result builtin_unary(const Builtin *this, const Value *args, uint argc) {
    Result result = { 0, NULL };
    if (argc != 1) {
        result.error = malloc(64);
        sprintf(result.error, "Invalid number of arguments (%u) for unary function '%s'", argc, this->token);
        return result;
    }

    Value (*function)(Value) = this->payload;
    result.value = function(args[0]);
    return result;
}

Result builtin_binary(const Builtin *this, const Value *args, uint argc) {
    Result result = { 0, NULL };
    if (argc != 2) {
        result.error = malloc(64);
        sprintf(result.error, "Invalid number of arguments (%u) for binary function '%s'", argc, this->token);
        return result;
    }

    Value (*function)(Value, Value) = this->payload;
    result.value = function(args[0], args[1]);
    return result;
}

Result builtin_max(const Builtin *this, const Value *args, uint argc) {
    Result result = { 0, NULL };

    if (argc == 0) {
        return result;
    }

    result.value = args[0];
    for (uint i = 1; i < argc; i++) {
        if (result.value < args[i]) {
            result.value = args[i];
        }
    }
    return result;
}

Result builtin_min(const Builtin *this, const Value *args, uint argc) {
    Result result = { 0, NULL };

    if (argc == 0) {
        return result;
    }

    result.value = args[0];
    for (uint i = 1; i < argc; i++) {
        if (result.value > args[i]) {
            result.value = args[i];
        }
    }
    return result;
}

Result builtin_avg(const Builtin *this, const Value *args, uint argc) {
    Result result = { 0, NULL };

    if (argc == 0) {
        return result;
    }

    for (uint i = 0; i < argc; i++) {
        result.value += args[i];
    }

    result.value /= (Value)argc;

    return result;
}

Value round(Value x) {
    return floor(x + 0.5);
}

Value logn(Value x, Value b) {
    return log(x) / log(b);
}

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

const Builtin builtins[] = {
    // basic
    { "add", builtin_add, NULL },
    { "neg", builtin_neg, NULL },
    { "sub", builtin_sub, NULL },
    { "mul", builtin_mul, NULL },
    { "inv", builtin_inv, NULL },
    { "div", builtin_div, NULL },
    { "pow", builtin_binary, &pow },
    { "mod", builtin_binary, &fmod },
    { "sqrt", builtin_unary, &sqrt },
    
    { "loge", builtin_unary, &log },
    { "log10", builtin_unary, &log10 },
    { "log", builtin_binary, &logn },

    { "ceil", builtin_unary, &ceil },
    { "floor", builtin_unary, &floor },
    { "round", builtin_unary, &round },
    { "abs", builtin_unary, &fabs },

    { "max", builtin_max, NULL },
    { "min", builtin_min, NULL },
    { "avg", builtin_avg, NULL },

    // trig
    { "sin", builtin_unary, &sin },
    { "cos", builtin_unary, &cos },
    { "tan", builtin_unary, &tan },
    { "sinh", builtin_unary, &sinh },
    { "cosh", builtin_unary, &cosh },
    { "tanh", builtin_unary, &tanh },
    { "asin", builtin_unary, &asin },
    { "acos", builtin_unary, &acos },
    { "atan", builtin_unary, &atan },
    { "atan2", builtin_binary, &atan2 },
};

typedef struct {
    Expression expression;
    char *error;
} ParserResult;


ParserResult parseExpression(char**);

ParserResult parseCall(char **input) {
    ParserResult result;
    result.error = NULL;

    char fname[16 + 1];
    char *bp = fname;

    char c = **input;
    if (c == ' ' || c == '\0' || c == ')') {
        result.error = malloc(64);
        sprintf(result.error, "EOF while parsing call");
        return result;
    }

    for (uint i = 0; i < 16; i++) {
        c = **input;
        if (c == ' ') {
            goto next;
        }

        if (c == ')' || c == '\0') {
            result.error = malloc(64);
            sprintf(result.error, "Invalid call syntax / no arguments provided");
            return result;
        }

        *bp = c;
        bp++;
        (*input)++;
    }

    result.error = malloc(64);
    sprintf(result.error, "Call function name too long");
    return result;

    next:;

    const Builtin *builtin;

    *bp = '\0'; 
    for (uint i = 0; i < ARRLEN(builtins); i++) {
        if (strcmp(builtins[i].token, fname) == 0) {
            builtin = &builtins[i];
            goto loadargs; 
        }
    }
    
    result.error = malloc(64);
    sprintf(result.error, "Function '%s' not defined", fname);
    return result;

    loadargs:;

    Expression args[32];
    uint argc = 0;

    for (uint i = 0; i < 32; i++) {
        while (1) {
            char c = **input;
            if (c == ' ') {
                (*input)++;
                continue;
            }
            if (c == ')') {
                (*input)++;
                goto finish;
            }
            if (c == '\0') {
                result.error = malloc(64);
                sprintf(result.error, "EOF while parsing call");
                return result;
            }

            ParserResult r = parseExpression(input);
            if (r.error) {
                return r;
            }
            args[i] = r.expression;
            argc++;
            break;
        }
    }

    result.error = malloc(64);
    sprintf(result.error, "Too many arguments, max is 32");
    return result;

    finish:;

    CallExpression *ce = malloc(sizeof(CallExpression));
    ce->builtin = builtin; 
    ce->args = malloc(sizeof(Expression) * argc);
    memcpy(ce->args, args, sizeof(Expression) * argc);
    ce->argc = argc;

    //result.expression.interface = &ICallExpression;
    result.expression.type = TYPE_CALL;
    result.expression.object = ce;

    return result;
}

ParserResult parseNumber(char **input) {
    ParserResult result;
    result.error = NULL;

    char buf[32 + 1];
    char *bp = buf;

    for (uint i = 0; i < 32; i++) {
        char c = **input;
        if (c >= '0' && c <= '9' || c == '.' || c == '-') {
            *bp = c;
            bp++;
            (*input)++;
        } else if (c == ')' || c == ' ' || c == '\0') {
            goto end;
        } else {
            result.error = malloc(64);
            strcpy(result.error, "Failed to parse number");            
            return result;
        }
    }

    result.error = malloc(64);
    strcpy(result.error, "Number too long");
    return result;

    end:;
    Value value;
    *bp = '\0';
    int r = sscanf(buf, "%lf", &value);

    if (r != 1) {
        result.error = malloc(64);
        sprintf(result.error, "Failed to parse '%s' as a number", buf);
        return result;
    }
    
    ValueExpression *ve = malloc(sizeof(ValueExpression));
    ve->value = value;

    //result.expression.interface = &IValueExpression;
    result.expression.type = TYPE_VALUE;
    result.expression.object = ve;

    return result;
}

ParserResult parseVar(char **input) {
    ParserResult result;
    result.error = NULL;

    char c = **input;
    VarIndex index = c;
    (*input)++;
    c = **input;
    if (c != ' ' && c != ')' && c != '\0') {
        result.error = malloc(64);
        strcpy(result.error, "Failed to parse variable");
        return result;
    }

    VariableExpression *ve = malloc(sizeof(VariableExpression));
    ve->index = index;

    //result.expression.interface = &IVariableExpression;
    result.expression.type = TYPE_VARIABLE;
    result.expression.object = ve;

    return result;
}

ParserResult parseExpression(char **input) {
    char c = **input;
    if (c == '(') {
        (*input)++;
        return parseCall(input);        
    } else if (c >= '0' && c <= '9' || c == '-') {
        return parseNumber(input);
    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return parseVar(input);
    }

    ParserResult result;
    result.error = malloc(64);
    strcpy(result.error, "Failed to parse expression");
    return result;
}

void State_init(State *state) {
    for (uint i = 0; i < UCHAR_MAX; i++) {
        state->vars[i].occupied = 0;
    }

    state->vars['P'].occupied = 1;
    state->vars['P'].value = M_PI;
    state->vars['E'].occupied = 1;
    state->vars['E'].value = M_E;
}


int main(int argc, const char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Provide expression\n");
        return 1;
    }

    char in[256];
    strcpy(in, argv[1]);
    char *cursor = in;
    ParserResult result = parseExpression(&cursor);
    if (result.error) {
        fprintf(stderr, "Parser error: %s\n", result.error);
        free(result.error);
        return 1;
    }

    fprintf(stderr, "Expression: ");
    Expression_print(result.expression, stderr);
    fprintf(stderr, "\n");

    State state;
    State_init(&state);

    Result res = Expression_evaluate(result.expression, &state);
    if (res.error) {
        fprintf(stderr, "Evaluation error: %s\n", res.error);
    } else {
        fprintf(stderr, "Evaluated: %lf\n", res.value);
    }

    Expression_destroy(result.expression);
    Expression_free(result.expression);
}