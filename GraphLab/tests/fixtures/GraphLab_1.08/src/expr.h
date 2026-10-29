#ifndef GL_EXPR_H
#define GL_EXPR_H
#include <stdint.h>
#include <stddef.h>

/* Limits are deliberately small enough for the calculator. */
#define EXPR_MAX 160
#define EXPR_NODES 128
#define EXPR_CODE 128
#define EXPR_STACK 64
#define EXPR_DEPTH 32
/* Bounds renderer/compiler recursion on gint's 16 KiB stack, including
   long chains of postfix operators and left-associative expressions. */
#define EXPR_TREE_DEPTH 64

typedef enum {
    N_HOLE, N_NUM, N_VAR, N_CONST_I, N_RAW, N_GROUP, N_FUNC,
    N_NEG, N_ADD, N_SUB, N_MUL, N_DIV, N_POW, N_EQ,
    N_INF, N_FACT, N_SUM, N_PROD
} NodeKind;
typedef enum {
    V_X, V_Y, V_Z, V_A, V_B, V_C, V_T, V_N,
    V_D, V_E, V_F, V_G, V_H, V_I, V_J, V_K, V_L, V_M,
    V_O, V_P, V_Q, V_R, V_S, V_U, V_V, V_W, V_COUNT
} VarId;
typedef enum {
    F_SIN, F_COS, F_TAN, F_ASIN, F_ACOS, F_ATAN,
    F_SQRT, F_ABS, F_LN, F_LOG, F_EXP, F_FLOOR, F_CEIL,
    F_RE, F_IM, F_CONJ, F_MIN, F_MAX, F_ATAN2, F_COUNT
} FuncId;
typedef struct {
    uint8_t kind, aux;
    int16_t left, right, start, end, op_pos;
    /* SUM only: body node, and the source span of the bound variable. */
    int16_t extra, index_start, index_end;
    float value;
    /* Layout fields are filled by layout.c; do not affect evaluation. */
    int16_t width, height, baseline;
} ExprNode;
typedef struct {
    ExprNode nodes[EXPR_NODES];
    const char *src;
    /* Optional editor-only source ownership; never used by strict evaluation. */
    const int16_t *edit_mate, *edit_comma_owner;
    int pos, len, count, root, error_pos, depth, tolerant, error;
    uint32_t bound_mask;
    char message[64];
} Parsed;
/* For comparisons, the VM evaluates lhs-rhs; the relation describes the
   accepted side. A bare expression has REL_NONE and retains old semantics. */
typedef enum {REL_NONE=0, REL_EQ, REL_LT, REL_LE, REL_GT, REL_GE} Relation;
typedef enum {
    I_NUM, I_VAR, I_IMAG, I_NEG, I_ADD, I_SUB, I_MUL, I_DIV,
    I_POW, I_SQUARE, I_CUBE, I_FUNC, I_FACT, I_SUM, I_POWFACT, I_PROD
} OpCode;
typedef struct { uint8_t op, aux; float number; } Instr;
typedef struct {
    Instr code[EXPR_CODE];
    uint16_t size, max_stack;
    uint32_t variables;
    uint8_t has_equal; /* historical name: any top-level relation */
    uint8_t relation; /* Relation; the evaluated value is lhs-rhs */
} Program;
typedef struct { float re, im; } Complex;
typedef struct {
    float v[V_COUNT];
    Complex cz, cc;
    /* In complex evaluation, locally bound SUM variables are real, even
       when their names would ordinarily mean complex z or c. */
    uint32_t real_overrides;
} EvalVars;

/* tolerant=1 is for displaying an unfinished expression while typing.
 * It makes holes for missing operands; strict parsing is used to plot. */
int expr_parse(Parsed *p, char const *src, int tolerant);
/* Tolerant editor parse: a parent template's delimiter cannot be consumed
   by a manually opened but not-yet-closed inner group or function. */
int expr_parse_editor(Parsed *p,char const *src,const int16_t *mate,const int16_t *comma_owner);
int expr_compile(Parsed const *p, Program *out, char *error, size_t error_size);
float expr_eval_real(Program const *program, EvalVars const *vars);
Complex expr_eval_complex(Program const *program, EvalVars const *vars);
int expr_program_uses(Program const *program, VarId var);
/* Conservative whitelist: true only for expressions composed from globally
   continuous real operations. False means unknown, not necessarily a pole. */
int expr_program_continuous(Program const *program);
/* Prove a declared x period for a conservative subset of bytecode. A false
   result merely disables reuse; it never says the expression is nonperiodic. */
int expr_proves_x_period(Program const *program,EvalVars const *base,float period);
/* True only for finite residuals satisfying a strict/non-strict relation. */
int expr_relation_true(Relation relation,float residual);
/* Compile f(x) only when the whole input safely means y=f(x) or y REL f(x).
   A reversed comparison, such as f(x)<y, is normalized to y>f(x). */
int expr_compile_explicit_y(Parsed const *p,Program *out,Relation *normalized);
/* Compile the height function in z=f(x,y), f(x,y)=z, or a bare expression
   independent of z. Inequalities and equations still containing z are not
   accepted; they must use the general renderer. */
int expr_compile_explicit_z(Parsed const *p,Program *out);
/* For 3D surface search: safely replace an equality of the form g^k=0 or abs(g)=0
   (positive integer k) with g=0. This exposes sign changes that the finite
   ray sampler would otherwise miss. Returns 0 if no such rewrite applies. */
int expr_compile_zero_surface(Parsed const *p,Program *out);
/* Single-letter variable names, including e/i when locally bound by SUM. */
int expr_var_id(char letter);
/* Iteration and convergence limits are deliberately bounded for fx-CG50.
   Infinity is a heuristic numerical approximation, never a proof. */
#define EXPR_SUM_MAX_TERMS 128
#define EXPR_SUM_MAX_WORK 512
#define EXPR_SUM_MAX_DEPTH 4
#endif
