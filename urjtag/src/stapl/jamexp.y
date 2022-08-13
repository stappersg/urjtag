// SPDX-FileCopyrightText: 1997 Altera Corporation
// SPDX-FileCopyrightText: 2022 Peter Poeschl <pp+ujt2208@nest-ai.de>
//
// SPDX-License-Identifier: GPL-2.0-or-later

%code top {
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "jamexprt.h"
#include "jamdefs.h"
#include "jamexp.h"
#include "jamsym.h"
#include "jamheap.h"
#include "jamarray.h"
#include "jamutil.h"
#include "jamytab.h"
}

%code {
/* ------------- LEXER DEFINITIONS -----------------------------------------*/
/****************************************************************************/
/*                                                                          */
/*  Operation of GET_FIRST_CH, GET_NEXT_CH, UNGET_CH, and DELETE_CH:        */
/*                                                                          */
/*  Call GET_FIRST_CH to read a character from mdl_lexer_fp and put it into */
/*  urj_jam_ch and urj_jam_token_buffer.                                    */
/*                                                                          */
/*      urj_jam_ch = first char                                             */
/*      urj_jam_token_buffer[0] = first char                                */
/*      urj_jam_token_buffer[1] = '\0';                                     */
/*      urj_jam_token_buffer[2] = ?                                         */
/*      urj_jam_token_buffer[3] = ?                                         */
/*                                                                          */
/*  Call GET_NEXT_CH to read a character from jam_lexer_fp, put it in       */
/*  urj_jam_ch, and append it to urj_jam_token_buffer.                      */
/*                                                                          */
/*      urj_jam_ch = second char                                            */
/*      urj_jam_token_buffer[0] = first char                                */
/*      urj_jam_token_buffer[1] = second char                               */
/*      urj_jam_token_buffer[2] = '\0';                                     */
/*      urj_jam_token_buffer[3] = ?                                         */
/*                                                                          */
/*  Call UNGET_CH remove the last character from the buffer but leave it in */
/*  urj_jam_ch and set a flag.  (The next call to GET_FIRST_CH will use     */
/*  urj_jam_ch as the first char of the token and clear the flag.)          */
/*                                                                          */
/*      urj_jam_ch = second char                                            */
/*      urj_jam_token_buffer[0] = first char                                */
/*      urj_jam_token_buffer[1] = '\0';                                     */
/*      urj_jam_token_buffer[2] = ?                                         */
/*      urj_jam_token_buffer[3] = ?                                         */
/*                                                                          */
/*  Call DELETE_CH to remove the last character from the buffer.  Use this  */
/*  macro to discard the quotes surrounding a string, for example.  Unlike  */
/*  UNGET_CH, the deleted character will not be reused.                     */
/*                                                                          */
/****************************************************************************/

#define MAX_BUFFER_LENGTH   1024
#define END_OF_STRING       -1



/****************************************************************************/
/*                                                                          */
/*  Operation of BEGIN_MACHINE, END_MACHINE, and ACCEPT:                    */
/*                                                                          */
/*  BEGIN_MACHINE and END_MACHINE should be at the beginning the end of an  */
/*  integer function.  Inside the function, define states of the machine    */
/*  with normal C labels, and jump to states with normal C goto statements. */
/*  Use ACCEPT(token) to return an integer value token to the calling       */
/*  routine.                                                                */
/*                                                                          */
/*      int foo (void)                                                      */
/*      {                                                                   */
/*          BEGIN_MACHINE;                                                  */
/*                                                                          */
/*          start:                                                          */
/*              if (whatever) goto next;                                    */
/*              else goto start;                                            */
/*                                                                          */
/*          next:                                                           */
/*              if (done) ACCEPT (a_token_id);                              */
/*              else goto start;                                            */
/*                                                                          */
/*          END_MACHINE;                                                    */
/*      }                                                                   */
/*                                                                          */
/*  Be sure that there is an ACCEPT() or goto at the end of every state.    */
/*  Otherwise, control will "flow" from one state to the next illegally.    */
/*                                                                          */
/****************************************************************************/

#define BEGIN_MACHINE   {int ret

#define ACCEPT(token)   {ret = (token); goto accept;}

#define END_MACHINE     accept: urj_jam_token = ret; \
                        }
// need defines, single-chars cannot be %token
#define GREATER_TOK '>'
#define LESS_TOK    '<'

struct
{
    char *string;
    int length;
    int token;
} static const jam_keyword_table[] =
{
    {"&&", 2, AND_TOK},
    {"||", 2, OR_TOK},
    {"==", 2, EQUALITY_TOK},
    {"!=", 2, INEQUALITY_TOK},
    {">", 2, GREATER_TOK},
    {"<", 2, LESS_TOK},
    {">=", 2, GREATER_EQ_TOK},
    {"<=", 2, LESS_OR_EQ_TOK},
    {"<<", 2, LEFT_SHIFT_TOK},
    {">>", 2, RIGHT_SHIFT_TOK},
    {"..", 2, DOT_DOT_TOK},
    {"OR", 2, OR_TOK},
    {"AND", 3, AND_TOK},
    {"ABS", 3, ABS_TOK},
    {"INT", 3, INT_TOK},
    {"LOG2", 4, LOG2_TOK},
    {"SQRT", 4, SQRT_TOK},
    {"CEIL", 4, CIEL_TOK},
    {"FLOOR", 5, FLOOR_TOK}
};

char urj_jam_ch = '\0';             /* next character from input file */
int urj_jam_strptr = 0;
int urj_jam_token = 0;
char urj_jam_token_buffer[MAX_BUFFER_LENGTH];
int urj_jam_token_buffer_index;
char urj_jam_parse_string[MAX_BUFFER_LENGTH];
int32_t urj_jam_parse_value = 0;
int urj_jam_expression_type = 0;
JAMS_SYMBOL_RECORD *urj_jam_array_symbol_rec = NULL;

#define YYMAXDEPTH 300          /* This fixes a stack depth problem on  */
                        /* all platforms.                       */

#define YYMAXTLIST 25           /* Max valid next tokens for any state. */
                        /* If there are more, error reporting   */
                        /* will be incomplete.                  */

}

%code requires {
enum OPERATOR_TYPE
{
    ADD = 0,
    SUB,
    UMINUS,
    MULT,
    DIV,
    MOD,
    NOT,
    AND,
    OR,
    BITWISE_NOT,
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR,
    LEFT_SHIFT,
    RIGHT_SHIFT,
    DOT_DOT,
    EQUALITY,
    INEQUALITY,
    GREATER_THAN,
    LESS_THAN,
    GREATER_OR_EQUAL,
    LESS_OR_EQUAL,
    ABS,
    INT,
    LOG2,
    SQRT,
    CIEL,
    FLOOR,
    ARRAY,
    POUND,
    DOLLAR,
    ARRAY_RANGE,
    ARRAY_ALL
};

typedef enum OPERATOR_TYPE OPERATOR_TYPE;

typedef struct EXP_STACK
{
    OPERATOR_TYPE child_otype;
    JAME_EXPRESSION_TYPE type;
    int32_t val;
    int32_t loper;              /* left and right operands for DIV */
    int32_t roper;              /* we save it for CEIL/FLOOR's use */
} EXPN_STACK;

#define URJ_JAM_YYSTYPE EXPN_STACK      /* must be a #define for yacc */
}

%code {
YYSTYPE urj_jam_null_expression = { 0, 0, 0, 0, 0 };

JAM_RETURN_TYPE urj_jam_return_code = JAMC_SUCCESS;

JAME_EXPRESSION_TYPE urj_jam_expr_type = JAM_ILLEGAL_EXPR_TYPE;

#define NULL_EXP urj_jam_null_expression    /* .. for 1 operand operators */

#define CALC(operator, lval, rval) urj_jam_exp_eval((operator), (lval), (rval))
}

%code provides {
/* --- FUNCTION PROTOTYPES -------------------------------------------- */

int urj_jam_yyparse (void);
int urj_jam_yylex (void);
}

%code {
#ifdef DELME_OLD_TOKEN_VALUES
#define AND_TOK 257
#define OR_TOK 258
#define EQUALITY_TOK 259
#define INEQUALITY_TOK 260
#define GREATER_TOK 261
#define LESS_TOK 262
#define GREATER_EQ_TOK 263
#define LESS_OR_EQ_TOK 264
#define LEFT_SHIFT_TOK 265
#define RIGHT_SHIFT_TOK 266
#define DOT_DOT_TOK 267
#define ABS_TOK 268
#define INT_TOK 269
#define LOG2_TOK 270
#define SQRT_TOK 271
#define CIEL_TOK 272
#define FLOOR_TOK 273
#define VALUE_TOK 274
#define IDENTIFIER_TOK 275
#define ARRAY_TOK 276
#define ERROR_TOK 277
#define UNARY_MINUS 278
#define UNARY_PLUS 279
#endif // DELME_OLD_TOKEN_VALUES

YYSTYPE urj_jam_yylval, urj_jam_yyval;
int32_t urj_jam_convert_bool_to_int (int32_t *data, int32_t msb, int32_t lsb);
EXPN_STACK urj_jam_exp_eval (OPERATOR_TYPE otype, EXPN_STACK op1, EXPN_STACK op2);
void urj_jam_exp_lexer (void);
bool urj_jam_constant_is_ok (const char *string);
bool urj_jam_binary_constant_is_ok (const char *string);
bool urj_jam_hex_constant_is_ok (const char *string);
int32_t urj_jam_atol_bin (char *string);
int32_t urj_jam_atol_hex (char *string);
int urj_jam_constant_value (char *string, int32_t *value);
void urj_jam_yyerror (char *msg);
int urj_jam_yylex (void);
int urj_jam_evaluate_expression (char *expression, int32_t *result,
                             JAME_EXPRESSION_TYPE *result_type);
int urj_jam_yyparse (void);

#define GET_FIRST_CH jam_get_first_ch()
#define GET_NEXT_CH jam_get_next_ch()
#define UNGET_CH jam_unget_ch()
#define CH jam_get_ch()
}

// Bison declarations
%language "C"
//always in current dir: %header "jamytab.h"
//fixed jamexp.h parallel to jamexp.c: %header
// ==> use --defines=$PATH_TO/jamytab.h
%define api.prefix {urj_jam_yy}
%define api.value.type {EXPN_STACK}
%start stapl_expr

%token AND_TOK         "&&"  // alias "AND"
%token OR_TOK          "||"  // alias "OR"
%token EQUALITY_TOK    "=="
%token INEQUALITY_TOK  "!="
// single-char cannot be %token
//%token GREATER_TOK   '>'
//%token LESS_TOK      '<'
%token GREATER_EQ_TOK  ">="
%token LESS_OR_EQ_TOK  "<="
%token LEFT_SHIFT_TOK  "<<"
%token RIGHT_SHIFT_TOK ">>"
%token DOT_DOT_TOK     ".."
%token ABS_TOK         "ABS"
%token INT_TOK         "INT"
%token LOG2_TOK        "LOG2"
%token SQRT_TOK        "SQRT"
%token CIEL_TOK        "CEIL"
%token FLOOR_TOK       "FLOOR"
%token VALUE_TOK       "VALUE"
%token IDENTIFIER_TOK  "IDENTIFIER"
%token ARRAY_TOK       "ARRAY"
%token ERROR_TOK       "ERROR"
%token UNARY_MINUS     "UNARY_MINUS"
%token UNARY_PLUS      "UNARY_PLUS"

%code {
static inline void jam_get_next_ch(void)
{
    urj_jam_ch = urj_jam_parse_string[urj_jam_strptr++];
    urj_jam_token_buffer [urj_jam_token_buffer_index++] = urj_jam_ch;
    if (urj_jam_token_buffer_index >= MAX_BUFFER_LENGTH)
    {
        --urj_jam_token_buffer_index;
        --urj_jam_strptr;
    }
    urj_jam_token_buffer [urj_jam_token_buffer_index] = '\0';
}

static inline void jam_get_first_ch(void)
{
    urj_jam_token_buffer_index = 0;
    jam_get_next_ch();
}

static inline void jam_unget_ch(void)
{
    urj_jam_strptr--;
    urj_jam_token_buffer[--urj_jam_token_buffer_index] = '\0';
}

static inline char jam_get_ch(void)
{
    return urj_jam_ch;
}

/*
*   Used by INT() function to convert Boolean array data to integer.  "msb"
*   is the index of the most significant bit of the array, and "lsb" is the
*   index of the least significant bit.  Typically msb > lsb, otherwise the
*   bit order will be reversed when converted into integer format.
*/
int32_t
urj_jam_convert_bool_to_int (int32_t *data, int32_t msb, int32_t lsb)
{
    int32_t i, increment = (msb > lsb) ? 1 : -1;
    int32_t mask = 1L, result = 0L;

    msb += increment;
    for (i = lsb; i != msb; i += increment)
    {
        if (data[i >> 5] & (1L << (i & 0x1f)))
            result |= mask;
        mask <<= 1;
    }

    return result;
}


/************************************************************************/
/*                                                                      */

YYSTYPE
urj_jam_exp_eval (OPERATOR_TYPE otype, YYSTYPE op1, YYSTYPE op2)
/*  Evaluate op1 OTYPE op2.  op1, op2 are operands, OTYPE is operator   */
/*                                                                      */
/*  Some sneaky things are done to implement CEIL and FLOOR.            */
/*                                                                      */
/*  We do CEIL of LOG2 by default, and FLOOR of a DIVIDE by default.    */
/*  Since we are lazy and we don't want to generate a parse tree,       */
/*  we use the parser's reduce actions to tell us when to perform       */
/*  an evaluation. But when CEIL and FLOOR are reduced, we know         */
/*  nothing about the expression tree beneath it (it's been reduced!)   */
/*                                                                      */
/*  We keep this information around so we can calculate the CEIL or     */
/*  FLOOR. We save value of the operand(s) or a divide in loper and     */
/*  roper, then when CEIL/FLOOR get reduced, we just look at their      */
/*  values.                                                             */
/*                                                                      */
{
    YYSTYPE rtn;
    int32_t tmp_val;
    JAMS_SYMBOL_RECORD *symbol_rec;
    JAMS_HEAP_RECORD *heap_rec;

    rtn.child_otype = 0;
    rtn.type = JAM_ILLEGAL_EXPR_TYPE;
    rtn.val = 0;
    rtn.loper = 0;
    rtn.roper = 0;

    switch (otype)
    {
    case UMINUS:
        if ((op1.type == JAM_INTEGER_EXPR)
            || (op1.type == JAM_INT_OR_BOOL_EXPR))
        {
            rtn.val = -1 * op1.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case ADD:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val + op2.val;
            rtn.type = JAM_INTEGER_EXPR;

            /* check for overflow */
            if (((op1.val > 0) && (op2.val > 0) && (rtn.val < 0)) ||
                ((op1.val < 0) && (op2.val < 0) && (rtn.val > 0)))
            {
                urj_jam_return_code = JAMC_INTEGER_OVERFLOW;
            }
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case SUB:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val - op2.val;
            rtn.type = JAM_INTEGER_EXPR;

            /* check for overflow */
            if (((op1.val > 0) && (op2.val < 0) && (rtn.val < 0)) ||
                ((op1.val < 0) && (op2.val > 0) && (rtn.val > 0)))
            {
                urj_jam_return_code = JAMC_INTEGER_OVERFLOW;
            }
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case MULT:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val * op2.val;
            rtn.type = JAM_INTEGER_EXPR;

            /* check for overflow */
            if ((op1.val != 0) && (op2.val != 0) &&
                (((rtn.val / op1.val) != op2.val) ||
                 ((rtn.val / op2.val) != op1.val)))
            {
                urj_jam_return_code = JAMC_INTEGER_OVERFLOW;
            }
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case DIV:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            if (op2.val != 0)
            {
                rtn.val = op1.val / op2.val;
                rtn.loper = op1.val;
                rtn.roper = op2.val;
                rtn.child_otype = DIV;  /* Save info needed by CEIL */
                rtn.type = JAM_INTEGER_EXPR;
            }
            else
            {
                urj_jam_return_code = JAMC_DIVIDE_BY_ZERO;
            }
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case MOD:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val % op2.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case NOT:
        if ((op1.type == JAM_BOOLEAN_EXPR)
            || (op1.type == JAM_INT_OR_BOOL_EXPR))
        {
            rtn.val = (op1.val == 0) ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case AND:
        if (((op1.type == JAM_BOOLEAN_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_BOOLEAN_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = (op1.val && op2.val) ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case OR:
        if (((op1.type == JAM_BOOLEAN_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_BOOLEAN_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = (op1.val || op2.val) ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case BITWISE_NOT:
        if ((op1.type == JAM_INTEGER_EXPR)
            || (op1.type == JAM_INT_OR_BOOL_EXPR))
        {
            rtn.val = ~(uint32_t) op1.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case BITWISE_AND:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val & op2.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case BITWISE_OR:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val | op2.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case BITWISE_XOR:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val ^ op2.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case LEFT_SHIFT:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val << op2.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case RIGHT_SHIFT:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = op1.val >> op2.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case EQUALITY:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = (op1.val == op2.val) ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else if (((op1.type == JAM_BOOLEAN_EXPR)
                  || (op1.type == JAM_INT_OR_BOOL_EXPR))
                 && ((op2.type == JAM_BOOLEAN_EXPR)
                     || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = ((op1.val && op2.val) || ((!op1.val) && (!op2.val)))
                ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case INEQUALITY:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = (op1.val == op2.val) ? 0 : 1;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else if (((op1.type == JAM_BOOLEAN_EXPR)
                  || (op1.type == JAM_INT_OR_BOOL_EXPR))
                 && ((op2.type == JAM_BOOLEAN_EXPR)
                     || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = ((op1.val && op2.val) || ((!op1.val) && (!op2.val)))
                ? 0 : 1;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case GREATER_THAN:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = (op1.val > op2.val) ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case LESS_THAN:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = (op1.val < op2.val) ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case GREATER_OR_EQUAL:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = (op1.val >= op2.val) ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case LESS_OR_EQUAL:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            rtn.val = (op1.val <= op2.val) ? 1 : 0;
            rtn.type = JAM_BOOLEAN_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case ABS:
        if ((op1.type == JAM_INTEGER_EXPR) ||
            (op1.type == JAM_INT_OR_BOOL_EXPR))
        {
            rtn.val = (op1.val < 0) ? (0 - op1.val) : op1.val;
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case INT:
        rtn.val = op1.val;
        rtn.type = JAM_INTEGER_EXPR;
        break;

    case LOG2:
        if ((op1.type == JAM_INTEGER_EXPR) ||
            (op1.type == JAM_INT_OR_BOOL_EXPR))
        {
            if (op1.val > 0)
            {
                rtn.child_otype = LOG2;
                rtn.type = JAM_INTEGER_EXPR;
                rtn.loper = op1.val;
                tmp_val = op1.val;
                rtn.val = 0;

                while (tmp_val != 1)    /* ret_val = log2(left_val) */
                {
                    tmp_val = tmp_val >> 1;
                    ++rtn.val;
                }

                /* if 2^(return_val) isn't the left_val, then the log */
                /* wasn't a perfect integer, so we increment it */
                if (pow (2, rtn.val) != op1.val)
                {
                    ++rtn.val;  /* Assume ceil of log2 */
                }
            }
            else
            {
                urj_jam_return_code = JAMC_INTEGER_OVERFLOW;
            }
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case SQRT:
        if ((op1.type == JAM_INTEGER_EXPR) ||
            (op1.type == JAM_INT_OR_BOOL_EXPR))
        {
            if (op1.val >= 0)
            {
                rtn.child_otype = SQRT;
                rtn.type = JAM_INTEGER_EXPR;
                rtn.loper = op1.val;
                rtn.val = sqrt (op1.val);
            }
            else
            {
                urj_jam_return_code = JAMC_INTEGER_OVERFLOW;
            }
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case CIEL:
        if ((op1.type == JAM_INTEGER_EXPR)
            || (op1.type == JAM_INT_OR_BOOL_EXPR))
        {
            if (op1.child_otype == DIV)
            {
                /* Below is true if wasn't perfect divide */
                if ((op1.loper * op1.roper) != op1.val)
                {
                    rtn.val = op1.val + 1;      /* add 1 to get CEIL */
                }
                else
                {
                    rtn.val = op1.val;
                }
            }
            else if (op1.child_otype == SQRT)
            {
                /* Below is true if wasn't perfect square-root */
                if ((op1.val * op1.val) < op1.loper)
                {
                    rtn.val = op1.val + 1;      /* add 1 to get CEIL */
                }
                else
                {
                    rtn.val = op1.val;
                }
            }
            else
            {
                rtn.val = op1.val;
            }
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case FLOOR:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            if (op1.child_otype == LOG2)
            {
                if (pow (2, op1.val) != op1.loper)
                {
                    rtn.val = op1.val - 1;
                }
                else
                {
                    rtn.val = op1.val;
                }
            }
            else
            {
                rtn.val = op1.val;
            }
            rtn.type = JAM_INTEGER_EXPR;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case ARRAY:
        if ((op1.type == JAM_ARRAY_REFERENCE) &&
            ((op2.type == JAM_INTEGER_EXPR)
             || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            symbol_rec = (JAMS_SYMBOL_RECORD *) op1.val;
            urj_jam_return_code =
                urj_jam_get_array_value (symbol_rec, op2.val, &rtn.val);

            if (urj_jam_return_code == JAMC_SUCCESS)
            {
                switch (symbol_rec->type)
                {
                case JAM_INTEGER_ARRAY_WRITABLE:
                case JAM_INTEGER_ARRAY_INITIALIZED:
                    rtn.type = JAM_INTEGER_EXPR;
                    break;

                case JAM_BOOLEAN_ARRAY_WRITABLE:
                case JAM_BOOLEAN_ARRAY_INITIALIZED:
                    rtn.type = JAM_BOOLEAN_EXPR;
                    break;

                default:
                    urj_jam_return_code = JAMC_INTERNAL_ERROR;
                    break;
                }
            }
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case POUND:
        rtn.val = op1.val;
        rtn.type = JAM_INTEGER_EXPR;
        break;

    case DOLLAR:
        rtn.val = op1.val;
        rtn.type = JAM_INTEGER_EXPR;
        break;

    case ARRAY_RANGE:
        if (((op1.type == JAM_INTEGER_EXPR)
             || (op1.type == JAM_INT_OR_BOOL_EXPR))
            && ((op2.type == JAM_INTEGER_EXPR)
                || (op2.type == JAM_INT_OR_BOOL_EXPR)))
        {
            symbol_rec = urj_jam_array_symbol_rec;

            if ((symbol_rec != NULL) &&
                ((symbol_rec->type == JAM_BOOLEAN_ARRAY_WRITABLE) ||
                 (symbol_rec->type == JAM_BOOLEAN_ARRAY_INITIALIZED)))
            {
                heap_rec = (JAMS_HEAP_RECORD *) symbol_rec->value;

                if (heap_rec != NULL)
                {
                    rtn.val = urj_jam_convert_bool_to_int (heap_rec->data,
                                                       op1.val, op2.val);
                }
                rtn.type = JAM_INTEGER_EXPR;
            }
            else
                urj_jam_return_code = JAMC_TYPE_MISMATCH;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    case ARRAY_ALL:
        if (op1.type == JAM_ARRAY_REFERENCE)
        {
            symbol_rec = (JAMS_SYMBOL_RECORD *) op1.val;

            if ((symbol_rec != NULL) &&
                ((symbol_rec->type == JAM_BOOLEAN_ARRAY_WRITABLE) ||
                 (symbol_rec->type == JAM_BOOLEAN_ARRAY_INITIALIZED)))
            {
                heap_rec = (JAMS_HEAP_RECORD *) symbol_rec->value;

                if (heap_rec != NULL)
                {
                    rtn.val = urj_jam_convert_bool_to_int (heap_rec->data,
                                                       heap_rec->dimension -
                                                       1, 0);
                }
                rtn.type = JAM_INTEGER_EXPR;
            }
            else
                urj_jam_return_code = JAMC_TYPE_MISMATCH;
        }
        else
            urj_jam_return_code = JAMC_TYPE_MISMATCH;
        break;

    default:
        urj_jam_return_code = JAMC_INTERNAL_ERROR;
        break;
    }

    return rtn;
}


/****************************************************************************/
/*                                                                          */

void
urj_jam_exp_lexer (void)
/*                                                                          */
/*  Description:    Lexical analyzer for expressions.                       */
/*                                                                          */
/*                  Results are returned in the global variables            */
/*                  urj_jam_token and urj_jam_token_buffer.                 */
/*                                                                          */
/*  References:     Compilers: Principles, Techniques and Tools by ASU      */
/*                  (the Green Dragon book), section 3.4, Recognition of    */
/*                  tokens.                                                 */
/*                                                                          */
/*  Returns:        Nothing                                                 */
/*                                                                          */
/****************************************************************************/
{
    BEGIN_MACHINE;

  start:
    GET_FIRST_CH;
    if (CH == '\0')
        ACCEPT (END_OF_STRING)  /* Fake an EOF! */
    else if (CH == ' ' || iscntrl (CH))
        goto start;             /* white space */
    else if (isalpha (CH))
        goto identifier;
    else if (isdigit (CH))
        goto number;
    else if (CH == '&')
    {
        GET_NEXT_CH;
        if (CH == '&')
            ACCEPT (AND_TOK)
        else
        {
            UNGET_CH;
            ACCEPT ('&')
        }
    }
    else if (CH == '|')
    {
        GET_NEXT_CH;
        if (CH == '|')
            ACCEPT (OR_TOK)
        else
        {
            UNGET_CH;
            ACCEPT ('|')
        }
    }
    else if (CH == '=')
    {
        GET_NEXT_CH;
        if (CH == '=')
            ACCEPT (EQUALITY_TOK)
        else
        {
            UNGET_CH;
            ACCEPT ('=')
        }
    }
    else if (CH == '!')
    {
        GET_NEXT_CH;
        if (CH == '=')
            ACCEPT (INEQUALITY_TOK)
        else
        {
            UNGET_CH;
            ACCEPT ('!')
        }
    }
    else if (CH == '>')
    {
        GET_NEXT_CH;
        if (CH == '=')
            ACCEPT (GREATER_EQ_TOK)
            else
        if (CH == '>')
            ACCEPT (RIGHT_SHIFT_TOK)
        else
        {
            UNGET_CH;
            ACCEPT (GREATER_TOK)
        }
    }
    else if (CH == '<')
    {
        GET_NEXT_CH;
        if (CH == '=')
            ACCEPT (LESS_OR_EQ_TOK)
            else
        if (CH == '<')
            ACCEPT (LEFT_SHIFT_TOK)
        else
        {
            UNGET_CH;
            ACCEPT (LESS_TOK)
        }
    }
    else if (CH == '.')
    {
        GET_NEXT_CH;
        if (CH == '.')
            ACCEPT (DOT_DOT_TOK)
        else
        {
            UNGET_CH;
            ACCEPT ('.')
        }
    }
    else
        ACCEPT (CH)             /* single-chararcter token */
      number:
        GET_NEXT_CH;
    if (isdigit (CH))
        goto number;
    else if (isalpha (CH) || CH == '_')
        goto identifier;
    else
    {
        UNGET_CH;
        ACCEPT (VALUE_TOK)
    }

  identifier:
    GET_NEXT_CH;
    if (jam_is_name_char(CH))
        goto identifier;
    else
    {
        UNGET_CH;
        ACCEPT (IDENTIFIER_TOK)
    }

    END_MACHINE;
}


/************************************************************************/
/*                                                                      */

bool
urj_jam_constant_is_ok (const char *string)
/*  This routine returns true if the value represented by string is     */
/*  a valid signed decimal number.                                      */
/*                                                                      */
{
    size_t ret = strspn (string, "-0123456789");
    return string[ret] == '\0' ? true : false;
}


/************************************************************************/
/*                                                                      */

bool
urj_jam_binary_constant_is_ok (const char *string)
/*  This routine returns true if the value represented by string is     */
/*  a valid binary number (containing only '0' and '1' characters).     */
/*                                                                      */
{
    size_t ret = strspn (string, "10");
    return string[ret] == '\0' ? true : false;
}


/************************************************************************/
/*                                                                      */

bool
urj_jam_hex_constant_is_ok (const char *string)
/*  This routine returns true if the value represented by string is     */
/*  a valid hexadecimal number.                                         */
/*                                                                      */
{
    size_t ret = strspn (string, "0123456789ABCDEFabcdef");
    return string[ret] == '\0' ? true : false;
}

int32_t
urj_jam_atol_bin (char *string)
{
    int32_t result = 0L;
    int index = 0;

    while ((string[index] == '0') || (string[index] == '1'))
    {
        result = (result << 1) + (string[index] - '0');
        ++index;
    }

    return result;
}

int32_t
urj_jam_atol_hex (char *string)
{
    int32_t result = 0L;

    sscanf (string, "%x", &result);

    return result;
}


/************************************************************************/
/*                                                                      */

int
urj_jam_constant_value (char *string, int32_t *value)
/*                                                                      */
/*      This routine converts a string constant into its binary         */
/*      value.                                                          */
/*                                                                      */
/*      Returns true for success, false if the string could not be      */
/*      converted.                                                      */
/*                                                                      */
{
    int status = false;

    if (urj_jam_expression_type == '#')
    {
        if (urj_jam_binary_constant_is_ok (string))
        {
            *value = urj_jam_atol_bin (string);
            urj_jam_expression_type = 0;
            status = true;
        }
    }
    else if (urj_jam_expression_type == '$')
    {
        if (urj_jam_hex_constant_is_ok (string))
        {
            *value = urj_jam_atol_hex (string);
            urj_jam_expression_type = 0;
            status = true;
        }
    }
    else if (urj_jam_constant_is_ok (string))
    {
        if (string[0] == '-')
        {
            *value = 0 - atol (&string[1]);
        }
        else
        {
            *value = atol (string);
        }
        status = true;
    }

    return status;
}


/************************************************************************/
/*                                                                      */

void
urj_jam_yyerror (char *msg)
/*                                                                      */
/*  WARNING: When compiling for YACC 5.0 using err_skel.c,              */
/*           this function needs to be modified to be:                  */
/*                                                                      */
/*           urj_jam_yyerror(char *ms1, char *msg2)                     */
/*                                                                      */
/*  urj_jam_yyerror() handles syntax error messages from the parser.    */
/*  Since we don't care about anything else but reporting the error,    */
/*  just flag the error in urj_jam_return_code.                         */
/*                                                                      */
{
    msg = msg;                  /* Avoid compiler warning about msg unused */

    if (urj_jam_return_code == JAMC_SUCCESS)
        urj_jam_return_code = JAMC_SYNTAX_ERROR;
}


/************************************************************************/
/*                                                                      */

int
urj_jam_yylex (void)
/*                                                                      */
/*  This is the lexer function called by urj_jam_yyparse(). It calls            */
/*  urj_jam_exp_lexer() to run as the DFA to return a token in urj_jam_token    */
/*                                                                      */
{
    JAMS_SYMBOL_RECORD *symbol_rec = NULL;
    int32_t val = 0L;
    JAME_EXPRESSION_TYPE type = JAM_ILLEGAL_EXPR_TYPE;
    int token_length;
    int i;

    urj_jam_exp_lexer ();

    token_length = strlen (urj_jam_token_buffer);

    if (token_length > 1)
    {
        for (i = 0; i < ARRAY_SIZE(jam_keyword_table); i++)
        {
            if (token_length == jam_keyword_table[i].length)
            {
                if (!strcmp
                    (urj_jam_token_buffer, jam_keyword_table[i].string))
                {
                    urj_jam_token = jam_keyword_table[i].token;
                }
            }
        }
    }

    if (urj_jam_token == VALUE_TOK)
    {
        if (urj_jam_constant_value (urj_jam_token_buffer, &val))
        {
            /* literal 0 and 1 may be interpreted as Integer or Boolean */
            if ((val == 0) || (val == 1))
            {
                type = JAM_INT_OR_BOOL_EXPR;
            }
            else
            {
                type = JAM_INTEGER_EXPR;
            }
        }
        else
        {
            urj_jam_return_code = JAMC_SYNTAX_ERROR;
        }
    }
    else if (urj_jam_token == IDENTIFIER_TOK)
    {
        urj_jam_return_code =
            urj_jam_get_symbol_record (urj_jam_token_buffer, &symbol_rec);

        if (urj_jam_return_code == JAMC_SUCCESS)
        {
            switch (symbol_rec->type)
            {
            case JAM_INTEGER_SYMBOL:
                /* Success, swap token to be a VALUE */
                urj_jam_token = VALUE_TOK;
                val = symbol_rec->value;
                type = JAM_INTEGER_EXPR;
                break;

            case JAM_BOOLEAN_SYMBOL:
                /* Success, swap token to be a VALUE */
                urj_jam_token = VALUE_TOK;
                val = symbol_rec->value ? 1 : 0;
                type = JAM_BOOLEAN_EXPR;
                break;

            case JAM_INTEGER_ARRAY_WRITABLE:
            case JAM_BOOLEAN_ARRAY_WRITABLE:
            case JAM_INTEGER_ARRAY_INITIALIZED:
            case JAM_BOOLEAN_ARRAY_INITIALIZED:
                /* Success, swap token to be an ARRAY_TOK, */
                /* save pointer to symbol record in value field */
                urj_jam_token = ARRAY_TOK;
                val = (int32_t) symbol_rec;
                type = JAM_ARRAY_REFERENCE;
                urj_jam_array_symbol_rec = symbol_rec;
                break;

            default:
                urj_jam_return_code = JAMC_SYNTAX_ERROR;
                break;
            }
        }
    }
    else if (urj_jam_token == '#')
    {
        urj_jam_expression_type = '#';
    }
    else if (urj_jam_token == '$')
    {
        urj_jam_expression_type = '$';
    }

    urj_jam_yylval.val = val;
    urj_jam_yylval.type = type;
    urj_jam_yylval.child_otype = 0;
    urj_jam_yylval.loper = 0;
    urj_jam_yylval.roper = 0;

    return urj_jam_token;
}


/************************************************************************/
/*                                                                      */

JAM_RETURN_TYPE urj_jam_evaluate_expression
    (char *expression, int32_t *result, JAME_EXPRESSION_TYPE *result_type)
/*                                                                      */
/*  THIS IS THE ENTRY POINT INTO THE EXPRESSION EVALUATOR.              */
/*                                                                      */
/*  s = a string representing the expression to be evaluated.           */
/*      (e.g. "2+2+PARAMETER")                                          */
/*                                                                      */
/*  status = for returning true if evaluation was successful.           */
/*           false if not.                                              */
/*                                                                      */
/*  This routine sets up the global variables and then calls            */
/*  urj_jam_yyparse() to do the parsing. The reduce actions of the      */
/*  parser evaluate the expression.                                     */
/*                                                                      */
/*  RETURNS: Value of the expression if success. 0 if FAIL.             */
/*                                                                      */
/*  Note: One should not rely on the return val to det.  success/fail   */
/*        since it is possible for, say, "2-2" to be success and        */
/*        return 0.                                                     */
/*                                                                      */
{
    strcpy (urj_jam_parse_string, expression);
    urj_jam_strptr = 0;
    urj_jam_token_buffer_index = 0;
    urj_jam_return_code = JAMC_SUCCESS;

    urj_jam_yyparse ();

    if (urj_jam_return_code == JAMC_SUCCESS)
    {
        if (result != 0)
            *result = urj_jam_parse_value;
        if (result_type != 0)
            *result_type = urj_jam_expr_type;
    }

    return urj_jam_return_code;
}
}

%% // Grammar rules

// grammar start symbol
stapl_expr: /*P1*/ logical_or_expr {
   urj_jam_parse_value = $1.val;
   urj_jam_expr_type = $1.type;
}
;
pound_expr: /*P2*/ '#' "VALUE"[right] {
   $$ = CALC (POUND, $right, NULL_EXP);
}
;
dollar_expr: /*P3*/ '$' "VALUE"[right] {
   $$ = CALC (DOLLAR, $right, NULL_EXP);
}
| '$' array_expr[right] {
   $$ = CALC (DOLLAR, $right, NULL_EXP);
}
;
array_range: /*P4*/ "ARRAY" '[' logical_or_expr[left] ".." logical_or_expr[right] ']' {
   // ??: ARRAY seems to have been cached somewhere
   $$ = CALC (ARRAY_RANGE, $left, $right);
}
;
array_all: /*P5*/ "ARRAY"[left] '[' ']' {
   $$ = CALC (ARRAY_ALL, $left, NULL_EXP);
}
;
primary_expr: /*P6*/ "VALUE"
| /*P7*/ '(' logical_or_expr[mid] ')' {
   $$ = $mid;
}
| array_expr
| abs_expr
| ceil_expr
| floor_expr
| int_expr
| log2_expr
| sqrt_expr
;
unary_expr: primary_expr
| /*P8*/ '+' unary_expr[right] {
   $$ = $right;
}
| /*P9*/ '-' unary_expr[right] {
   $$ = CALC (UMINUS, $right, NULL_EXP);
}
| /*P10*/ '!' unary_expr[right] {
   $$ = CALC (NOT, $right, NULL_EXP);
}
| /*P11*/ '~' unary_expr[right] {
   $$ = CALC (BITWISE_NOT, $right, NULL_EXP);
}
;
additive_expr: multiplicative_expr
| /*P12*/ additive_expr[left] '+' multiplicative_expr[right] {
   $$ = CALC (ADD, $left, $right);
}
| /*P13*/ additive_expr[left] '-' multiplicative_expr[right] {
   $$ = CALC (SUB, $left, $right);
}
;
multiplicative_expr: unary_expr
| /*P14*/ multiplicative_expr[left] '*' unary_expr[right] {
   $$ = CALC (MULT, $left, $right);
}
| /*P15*/ multiplicative_expr[left] '/' unary_expr[right] {
   $$ = CALC (DIV, $left, $right);
}
| /*P16*/ multiplicative_expr[left] '%' unary_expr[right] {
   $$ = CALC (MOD, $left, $right);
}
;
bitand_expr: equality_expr
| /*P17*/ bitand_expr[left] '&' equality_expr[right] {
   $$ = CALC (BITWISE_AND, $left, $right);
}
;
bitor_expr: bitxor_expr
| /*P18*/ bitor_expr[left] '|' bitxor_expr[right] {
   $$ = CALC (BITWISE_OR, $left, $right);
}
;
bitxor_expr: bitand_expr
| /*P19*/ bitxor_expr[left] '^' bitand_expr[right] {
   $$ = CALC (BITWISE_XOR, $left, $right);
}
;
logical_and_expr: bitor_expr
| /*P20*/ logical_and_expr[left] "&&" bitor_expr[right] {
   $$ = CALC (AND, $left, $right);
}
;
logical_or_expr: logical_and_expr
| /*P21*/ logical_or_expr[left] "||" logical_and_expr[right] {
   $$ = CALC (OR, $left, $right);
}
;
shift_expr: additive_expr
| /*P22*/ shift_expr[left] "<<" additive_expr[right] {
   $$ = CALC (LEFT_SHIFT, $left, $right);
}
| /*P23*/ shift_expr[left] ">>" additive_expr[right] {
   $$ = CALC (RIGHT_SHIFT, $left, $right);
}
;
equality_expr: relational_expr
| /*P24*/ equality_expr[left] "==" relational_expr[right] {
   $$ = CALC (EQUALITY, $left, $right);
}
| /*P25*/ equality_expr[left] "!=" relational_expr[right] {
   $$ = CALC (INEQUALITY, $left, $right);
}
;
relational_expr: shift_expr
| /*P26*/ relational_expr[left] '>' shift_expr[right] {
   $$ = CALC (GREATER_THAN, $left, $right);
}
| /*P27*/ relational_expr[left] '<' shift_expr[right] {
   $$ = CALC (LESS_THAN, $left, $right);
}
| /*28*/ relational_expr[left] ">=" shift_expr[right] {
   $$ = CALC (GREATER_OR_EQUAL, $left, $right);
}
| /*P29*/ relational_expr[left] "<=" shift_expr[right] {
   $$ = CALC (LESS_OR_EQUAL, $left, $right);
}
;
abs_expr: /*P30*/ "ABS" '(' logical_or_expr[mid] ')' {
   $$ = CALC (ABS, $mid, NULL_EXP);
}
;
int_expr: /*P31*/ "INT" '(' pound_expr[mid] ')' {
   $$ = CALC (INT, $mid, NULL_EXP);
}
| "INT" '(' dollar_expr[mid] ')' {
   $$ = CALC (INT, $mid, NULL_EXP);
}
| "INT" '(' array_range[mid] ')' {
   $$ = CALC (INT, $mid, NULL_EXP);
}
| "INT" '(' array_all[mid] ')' {
   $$ = CALC (INT, $mid, NULL_EXP);
}
;
log2_expr: /*P32*/ "LOG2" '(' logical_or_expr[mid] ')' {
   $$ = CALC (LOG2, $mid, NULL_EXP);
}
;
sqrt_expr: /*P33*/ "SQRT" '(' logical_or_expr[mid] ')' {
   $$ = CALC (SQRT, $mid, NULL_EXP);
}
;
ceil_expr: /*P34*/ "CEIL" '(' logical_or_expr[mid] ')' {
   $$ = CALC (CIEL, $mid, NULL_EXP);
}
;
floor_expr: /*P35*/ "FLOOR" '(' logical_or_expr[mid] ')' {
   $$ = CALC (FLOOR, $mid, NULL_EXP);
}
;
array_expr: /*P36*/ "ARRAY"[left] '[' logical_or_expr[right] ']' {
   $$ = CALC (ARRAY, $left, $right);
}
;
