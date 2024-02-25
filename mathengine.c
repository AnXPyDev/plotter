#include <stdio.h>
#include <malloc.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "mathconfig.h"

typedef unsigned int uint;

typedef Value (*CET_fn_unary_t)(Value);
typedef Value (*CET_fn_binary_t)(Value, Value);
typedef Value (*CET_fn_ternary_t)(Value, Value, Value);
typedef Value (*CET_fn_variable_t)(Value* argv, uint argc);

typedef enum {
    CET_VALUE, CET_LOOKUP, CET_CALL, CET_BUILTIN
} ECompiledExpression_Type;

typedef enum {
    CET_CALL_UNARY, CET_CALL_BINARY, CET_CALL_TERNARY, CET_CALL_VARIABLE
} ECompiledExpression_Call;

typedef enum {
    CET_ADD, CET_SUB, CET_NEG, CET_MUL, CET_DIV, CET_INV, CET_MAX, CET_MIN, CET_AVG
} ECompiledExpression_Builtin;

typedef struct {
    uint size;
    ECompiledExpression_Type type;
    char expression[0];
} CompiledExpression;

typedef struct {
    Value value;
} CompiledExpression_Value;

typedef struct {
    Value *valuep;
} CompiledExpression_Lookup;

typedef union {
    CompiledExpression_Value value;
    CompiledExpression_Lookup lookup;
} CompiledExpression_VL;

typedef struct {
    ECompiledExpression_Call type;
    uint argc;
    void *function;
    char args[0];
} CompiledExpression_Call;

typedef struct {
    ECompiledExpression_Builtin type;
    uint argc;
    char args[0];
} CompiledExpression_Builtin;

// math interface

typedef unsigned char VarIndex;

typedef struct {
    int occupied;
    int constant;
    Value value;
} VarSlot;

typedef struct {
    VarSlot vars[MATH_MAX_VARS];
} State;

typedef struct {
    int used;
    Value *value;
} RegisterSlot;

typedef struct {
    RegisterSlot slots[MATH_MAX_VARS];
} Register;

typedef struct {
    uint size;
    Register reg;
    CompiledExpression *root;
    char data[0];
} Program;

typedef struct {
    State *state;
} CompilationContext;

struct VariableOffset {
    VariableIndex id;
    uint offset;
};

typedef struct {
    uint count;
    struct VariableOffset *offsets;
} VariableOffsets;

typedef struct {
    VariableOffsets offsets;  
    CompiledExpression *ce;
    char *error;
} CompilationResult;

typedef struct {
    const struct IExpression *interface;
    void *object;
} Expression;

typedef struct {
    Value value;
    char *error;
} Result;

struct IExpression {
    Result (*evaluate)(void *this, State *state);
    void (*destroy)(void *this);
    void (*print)(void *this, FILE *fp);

    // compilation 
    int (*isConstant)(void *this, State *state);
    Expression (*reduce)(void *this, State *state);
    CompilationResult (*compile)(void *this, CompilationContext ctx);
};

Result Expression_evaluate(Expression this, State *state) {
    return this.interface->evaluate(this.object, state);
}

void Expression_destroy(Expression this) {
    return this.interface->destroy(this.object);
}

void Expression_free(Expression this) {
    return free(this.object);
}

void Expression_print(Expression this, FILE *fp) {
    return this.interface->print(this.object, fp);
}

Expression Expression_reduce(Expression this, State *state) {
    return this.interface->reduce(this.object, state);
}

int Expression_isConstant(Expression this, State *state) {
    return this.interface->isConstant(this.object, state);
}

CompilationResult Expression_compile(Expression this, CompilationContext ctx) {
    return this.interface->compile(this.object, ctx);
};
void VariableOffsets_add(VariableOffsets this, uint offset) {
    for (uint i = 0; i < this.count; i++) {
        this.offsets[i].offset += offset;
    }
}

VariableOffsets VariableOffsets_join(VariableOffsets a, VariableOffsets b) {
    VariableOffsets result;
    result.count = a.count + b.count;

    if (result.count == 0) {
        result.offsets = NULL;
        return result;
    }

    result.offsets = malloc(sizeof(struct VariableOffset) * result.count);

    if (a.count != 0) {
        memcpy(result.offsets, a.offsets, a.count * sizeof(struct VariableOffset));
    }
    if (b.count != 0) {
        memcpy(result.offsets + a.count, b.offsets, b.count * sizeof(struct VariableOffset));
    }


    return result;
}

CompilationResult createCompiledConst(Value value) {
    CompilationResult result;
    result.error = NULL;
    const uint size = sizeof(CompiledExpression) + sizeof(CompiledExpression_Value);
    result.ce = malloc(size);
    result.ce->size = size;
    result.ce->type = CET_VALUE;
    CompiledExpression_Value *vl = (void*)&result.ce->expression;
    vl->value = value;
    result.offsets.count = 0;
    result.offsets.offsets = NULL;
    return result;
}

Program *Program_create(CompilationResult cr) {
    uint uses_per_var[MATH_MAX_VARS];
    memset(uses_per_var, 0, MATH_MAX_VARS * sizeof(uint));
    for (uint i = 0; i < cr.offsets.count; i++) {
        uses_per_var[cr.offsets.offsets[i].id]++;
    }

    uint var_count = 0; 

    for (uint i = 0; i < MATH_MAX_VARS; i++) {
        if (uses_per_var[i]) {
            var_count++;
        }
    }

    const uint prog_size = sizeof(Program) + sizeof(Value) * var_count + cr.ce->size;

    Program *prog = malloc(prog_size);
    memset(prog, 0, prog_size);
    prog->size = prog_size;
    prog->root = (void*)prog + (prog_size - cr.ce->size);
    
    memcpy(prog->root, cr.ce, cr.ce->size);

    Value *var = (void*)prog->data;
    
    for (uint i = 0; i < MATH_MAX_VARS; i++) {
        if (uses_per_var[i] >= 2) {
            prog->reg.slots[i].used = 1;
            prog->reg.slots[i].value = var;
            var++;
        }
    }

    for (uint i = 0; i < cr.offsets.count; i++) {
        struct VariableOffset offset = cr.offsets.offsets[i];
        CompiledExpression *ex = (void*)prog->root + offset.offset;
        if (uses_per_var[offset.id] > 1) {
            ((CompiledExpression_VL*)ex->expression)->lookup.valuep = prog->reg.slots[offset.id].value;
        } else {
            ex->type = CET_VALUE;
            prog->reg.slots[offset.id].used = 1;
            prog->reg.slots[offset.id].value = &((CompiledExpression_VL*)ex->expression)->value.value;
        }
    }

    return prog;
}



Value CompiledExpression_evaluate(CompiledExpression*);

Value CET_ADD_eval(uint argc, CompiledExpression *argsp) {
    Value result = 0;
    for (uint i = 0; i < argc; i++) {
        result += CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
    }
    return result;
}

Value CET_NEG_eval(uint argc, CompiledExpression *argsp) {
    Value result = 0;
    for (uint i = 0; i < argc; i++) {
        result -= CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
    }
    return result;
}

Value CET_SUB_eval(uint argc, CompiledExpression *argsp) {
    Value result = 0;
    if (argc == 0) {
        return result;
    } else if (argc == 1) {
        result -= CompiledExpression_evaluate(argsp);
        return result;
    }

    result = CompiledExpression_evaluate(argsp);

    for (uint i = 1; i < argc; i++) {
        result -= CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
    }
    return result;
}

Value CET_MUL_eval(uint argc, CompiledExpression *argsp) {
    Value result = 1;
    for (uint i = 0; i < argc; i++) {
        result *= CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
    }
    return result;
}

Value CET_INV_eval(uint argc, CompiledExpression *argsp) {
    Value result = 1;
    for (uint i = 0; i < argc; i++) {
        result /= CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
    }
    return result;
}

Value CET_DIV_eval(uint argc, CompiledExpression *argsp) {
    Value result = 1;
    if (argc == 0) {
        return result;
    } else if (argc == 1) {
        result /= CompiledExpression_evaluate(argsp);
        return result;
    }

    result = CompiledExpression_evaluate(argsp);
    argsp = (void*)argsp + argsp->size;

    for (uint i = 1; i < argc; i++) {
        result /= CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
    }
    return result;
}

Value CET_MAX_eval(uint argc, CompiledExpression *argsp) {
    Value result = 0;

    if (argc == 0) {
        return result;
    }

    result = CompiledExpression_evaluate(argsp);
    argsp = (void*)argsp + argsp->size;

    for (uint i = 1; i < argc; i++) {
        Value a = CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
        if (a > result) {
            result = a;
        }
    }
    return result;
}

Value CET_MIN_eval(uint argc, CompiledExpression *argsp) {
    Value result = 0;

    if (argc == 0) {
        return result;
    }

    result = CompiledExpression_evaluate(argsp);
    argsp = (void*)argsp + argsp->size;

    for (uint i = 1; i < argc; i++) {
        Value a = CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
        if (a < result) {
            result = a;
        }
    }
    return result;
}

Value CET_AVG_eval(uint argc, CompiledExpression *argsp) {
    Value sum = 0;

    if (argc == 0) {
        return sum;
    }

    for (uint i = 0; i < argc; i++) {
        sum += CompiledExpression_evaluate(argsp);
        argsp = (void*)argsp + argsp->size;
    }

    return sum / (Value)argc;
}


Value CompiledExpression_Builtin_evaluate(CompiledExpression_Builtin *this) {
    uint argc = this->argc;
    CompiledExpression *argsp = (void*)this + sizeof(CompiledExpression_Builtin);
    switch (this->type) {
        case CET_ADD:
            return CET_ADD_eval(argc, argsp);
        case CET_NEG:
            return CET_NEG_eval(argc, argsp);
        case CET_SUB:
            return CET_SUB_eval(argc, argsp);
        case CET_MUL:
            return CET_MUL_eval(argc, argsp);
        case CET_INV:
            return CET_INV_eval(argc, argsp);
        case CET_DIV:
            return CET_DIV_eval(argc, argsp);
        case CET_MAX:
            return CET_MAX_eval(argc, argsp);
        case CET_MIN:
            return CET_MAX_eval(argc, argsp);
        case CET_AVG:
            return CET_MAX_eval(argc, argsp);
        default:
            return 0;
    }
}

Value CompiledExpression_Call_evaluate(CompiledExpression_Call *this) {
    CompiledExpression *argsp = (void*)this + sizeof(CompiledExpression_Call);
    switch (this->type) {
        case CET_CALL_UNARY:
            Value arg = CompiledExpression_evaluate(argsp);
            Value (*ufn)(Value) = this->function;
            return ufn(arg);
        case CET_CALL_BINARY:
            Value arg1 = CompiledExpression_evaluate(argsp);
            Value arg2 = CompiledExpression_evaluate((void*)argsp + argsp->size);
            Value(*bfn)(Value, Value) = this->function;
            return bfn(arg1, arg2);
        default:
            return 0;
    }
}

#define CE_EXPRESSION(ce) ((void*)ce + sizeof(CompiledExpression))

Value CompiledExpression_evaluate(CompiledExpression *this) {
    switch (this->type) {
        case CET_VALUE:
            return ((CompiledExpression_Value*)CE_EXPRESSION(this))->value;
        case CET_LOOKUP:
            return *((CompiledExpression_Lookup*)CE_EXPRESSION(this))->valuep;
        case CET_BUILTIN:
            return CompiledExpression_Builtin_evaluate((CompiledExpression_Builtin*)CE_EXPRESSION(this));
        case CET_CALL:
            return CompiledExpression_Call_evaluate((CompiledExpression_Call*)CE_EXPRESSION(this));
        default:
            return 0;
    }
}

Value Program_execute(Program *program) {
    return CompiledExpression_evaluate(program->root);
}