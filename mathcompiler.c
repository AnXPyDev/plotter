typedef Value (*CET_fn_unary_t)(Value);
typedef Value (*CET_fn_binary_t)(Value, Value);
typedef Value (*CET_fn_ternary_t)(Value, Value, Value);
typedef Value (*CET_fn_variable_t)(Value* argv, uint argc);

struct ExpressionDependency {
    uint8_t id;
    uint uses;
};

typedef struct {
    uint count;
    struct ExpressionDependency *deps;
} ExpressionDependency;

typedef enum {
    CET_VALUE, CET_LOOKUP, CET_CALL, CET_BUILTIN
} ECompiledExpression_Type;

typedef enum {
    CET_CALL_UNARY, CET_CALL_BINARY, CET_CALL_TERNARY, CET_CALL_VARIABLE
} ECompiledExpression_Call;

typedef enum {
    CET_ADD, CET_SUB, CET_NEG, CET_MUL, CET_DIV, CET_INV
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

