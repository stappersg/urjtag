/**
 * \author SPDX-FileCopyrightText: 2022 Peter Poeschl <pp+ujt2208@nest-ai.de>
 *
 * \copyright SPDX-License-Identifier: GPL-2.0-or-later
 *
 * \file jamexp_shrd.c
 * \brief Common test function to check urj_jam_evaluate_expression() results.
 *
 * Test idea:
 * * check all functions in jamexp.c directly or indirectly called by
 *   urj_jam_evaluate_expression().
 * * exercise all productions in the parser.
 * * check result values
 * * tests shared between drivers using generated of non-generated jamexp.c.
 */

#include "jamexp_shrd.h"
#include "jamdefs.h"
#include "jamexprt.h"
#include "jamsym.h"
#include "jamexp.h"
#include "jamheap.h"
#include "jamjtag.h"
#include "jamstack.h"

#include "tap/basic.h"

//============================================================================
// constants
//============================================================================


//============================================================================
// data types
//============================================================================

struct JAMS_HEAP_STRUCT2
{
    struct JAMS_HEAP_STRUCT *next;
    JAMS_SYMBOL_RECORD *symbol_record;
    JAME_BOOLEAN_REP rep;       /* data representation format */
    BOOL cached;                /* true if array data is cached */
    int32_t dimension;          /* number of elements in array */
    int32_t position;           /* position in file of initialization data */
    int32_t data[2];            /* first word of data (or cache buffer) */

};
struct JAMS_HEAP_STRUCT3
{
    struct JAMS_HEAP_STRUCT *next;
    JAMS_SYMBOL_RECORD *symbol_record;
    JAME_BOOLEAN_REP rep;       /* data representation format */
    BOOL cached;                /* true if array data is cached */
    int32_t dimension;          /* number of elements in array */
    int32_t position;           /* position in file of initialization data */
    int32_t data[3];            /* first word of data (or cache buffer) */

};

struct sEvalExpSpec {
   /// expression string to evaluate
   const char * expr;
   /// expected function return value
   JAM_RETURN_TYPE ret_x;
   /// expected expression result
   int32_t res_x;
   /** expected type of res_x
    * JAM_ILLEGAL_EXPR_TYPE = 0,
    * JAM_INTEGER_EXPR,
    * JAM_BOOLEAN_EXPR,
    * JAM_INT_OR_BOOL_EXPR,
    * JAM_ARRAY_REFERENCE,
    * JAM_EXPR_MAX
    */
   JAME_EXPRESSION_TYPE typ_x;
};

struct sInitSym {
   JAME_SYMBOL_TYPE type;
   char *name;
   intptr_t value;
};

static struct JAMS_HEAP_STRUCT2 BoolAffe_64 = {
   .cached = false,
   .dimension = 64, // bits
   .data = {0xaffe0000, 0xaffe0001},
};
static struct JAMS_HEAP_STRUCT BoolBaff_16 = {
   .cached = false,
   .dimension = 16, // bits
   .data = {0x0000baff},
};
static struct JAMS_HEAP_STRUCT2 IntA5A5_2 = {
   .cached = false,
   .dimension = 2, // uint32_t elems
   .data = {0xa5a50000, 0xa5a50001},
};
static struct JAMS_HEAP_STRUCT3 Int5A5A_3 = {
   .cached = false,
   .dimension = 3, // uint32_t elems
   .data = {0x5a5a0000, 0x5a5a0001, 0x5a5a0002},
};

static const struct sInitSym InitSymAry[INITSYMARY_NRELM] = {
   {JAM_BOOLEAN_SYMBOL, "BOOL0", 0},
   {JAM_BOOLEAN_SYMBOL, "BOOL1", 1},
   {JAM_INTEGER_SYMBOL, "INT0", 0},
   {JAM_INTEGER_SYMBOL, "INT1", 1},
   {JAM_INTEGER_SYMBOL, "INT23", 23},
   {JAM_INTEGER_SYMBOL, "S32MAX", 2147483647},   // 0x7fffffff
   {JAM_INTEGER_SYMBOL, "U32MAX", 4294967295},   // 0xffffffff
   {JAM_INTEGER_SYMBOL, "S32MIN", -2147483648},  // 0x80000000
   {JAM_BOOLEAN_ARRAY_INITIALIZED, "BOOLAFFE_", (intptr_t) &BoolAffe_64},
   {JAM_BOOLEAN_ARRAY_INITIALIZED, "BOOL_BAFF", (intptr_t) &BoolBaff_16},
   {JAM_INTEGER_ARRAY_INITIALIZED, "INTA5A5_",  (intptr_t) &IntA5A5_2},
   {JAM_INTEGER_ARRAY_INITIALIZED, "INT_5A5A",  (intptr_t) &Int5A5A_3},
};

struct sEvalExpSpec EvalSpecAry[EVAL_EXP_NRELM]
= {
   // [0]
   // P1: default START_SYMBOL accept production?
   // P2: bool literal param of INT function
   {.expr = "INT(#10001)",  .ret_x = JAMC_SUCCESS,       .res_x = 17,               .typ_x = JAM_INTEGER_EXPR,  },
   // P3: bool array ref param of INT function
   {.expr = "INT($BOOLAFFE_[7])", .ret_x = JAMC_SUCCESS,  .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,  },
   // P4: bool array range param of INT function
   {.expr = "INT(BOOLAFFE_[60..63])", .ret_x = JAMC_SUCCESS, .res_x = 5,            .typ_x = JAM_INTEGER_EXPR,  },
   //                     ???? Success from 64-bit bool array?
   {.expr = "INT(BOOLAFFE_[0..63])", .ret_x = JAMC_SUCCESS, .res_x = -2147450891,   .typ_x = JAM_INTEGER_EXPR,  },
   // P5: bool array param of INT function
   {.expr = "INT(BOOL_BAFF[])",.ret_x = JAMC_SUCCESS,     .res_x = 0x0000baff,      .typ_x = JAM_INTEGER_EXPR,  },
   // P6: good literals and identifiers
   // [5]
   {.expr = "42",          .ret_x = JAMC_SUCCESS,       .res_x = 42,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "0",           .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INT_OR_BOOL_EXPR,},
   {.expr = "1",           .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INT_OR_BOOL_EXPR,},
   {.expr = "10001",       .ret_x = JAMC_SUCCESS,       .res_x = 10001,           .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2147483647",  .ret_x = JAMC_SUCCESS,       .res_x = 2147483647,      .typ_x = JAM_INTEGER_EXPR,    },
   // [10]
#ifdef __code_model_32__
   {.expr = "2147483648",  .ret_x = JAMC_SUCCESS,       .res_x =  2147483647,     .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "4294967295",  .ret_x = JAMC_SUCCESS,       .res_x =  2147483647,     .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "4294967296",  .ret_x = JAMC_SUCCESS,       .res_x =  2147483647,     .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "4294967297",  .ret_x = JAMC_SUCCESS,       .res_x =  2147483647,     .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "4294967298",  .ret_x = JAMC_SUCCESS,       .res_x =  2147483647,     .typ_x = JAM_INTEGER_EXPR,    },
#else
   {.expr = "2147483648",  .ret_x = JAMC_SUCCESS,       .res_x = -2147483648,     .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "4294967295",  .ret_x = JAMC_SUCCESS,       .res_x = -1,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "4294967296",  .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INT_OR_BOOL_EXPR,},
   {.expr = "4294967297",  .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INT_OR_BOOL_EXPR,},
   {.expr = "4294967298",  .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,},
#endif
   // [15]
   {.expr = "BOOL0",       .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "INT23",       .ret_x = JAMC_SUCCESS,       .res_x = 23,              .typ_x = JAM_INTEGER_EXPR,    },
   // P7: parenthesized literals and identifiers
   {.expr = "(42)",        .ret_x = JAMC_SUCCESS,       .res_x = 42,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "(0)",         .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INT_OR_BOOL_EXPR,},
   {.expr = "(1)",         .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INT_OR_BOOL_EXPR,},
   // [20]
   {.expr = "(BOOL0)",     .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "(INT23)",     .ret_x = JAMC_SUCCESS,       .res_x = 23,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "(1||0)",      .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "(2+3)",       .ret_x = JAMC_SUCCESS,       .res_x = 5,               .typ_x = JAM_INTEGER_EXPR,    },
   // P8-P11: prec 1 unary ops +, -, !, ~
   {.expr = "+42",         .ret_x = JAMC_SUCCESS,       .res_x = 42,              .typ_x = JAM_INTEGER_EXPR,    },
   // [25]
   {.expr = "+0",          .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INT_OR_BOOL_EXPR,},
   {.expr = "+1",          .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INT_OR_BOOL_EXPR,},
   {.expr = "+BOOL0",      .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "+INT23",      .ret_x = JAMC_SUCCESS,       .res_x = 23,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "++INT23",     .ret_x = JAMC_SUCCESS,       .res_x = 23,              .typ_x = JAM_INTEGER_EXPR,    },
   // [30]
   {.expr = "-42",         .ret_x = JAMC_SUCCESS,       .res_x = -42,             .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "-0",          .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "-1",          .ret_x = JAMC_SUCCESS,       .res_x = -1,              .typ_x = JAM_INTEGER_EXPR,    },
#ifdef __code_model_32__
   {.expr = "-2147483648", .ret_x = JAMC_SUCCESS,       .res_x = -2147483647,     .typ_x = JAM_INTEGER_EXPR,    },
#else
   {.expr = "-2147483648", .ret_x = JAMC_SUCCESS,       .res_x = -2147483648,     .typ_x = JAM_INTEGER_EXPR,    },
#endif
   {.expr = "-BOOL0",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   // [35]
   {.expr = "-INT23",      .ret_x = JAMC_SUCCESS,       .res_x = -23,             .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "--INT23",     .ret_x = JAMC_SUCCESS,       .res_x =  23,             .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "-+INT23",     .ret_x = JAMC_SUCCESS,       .res_x = -23,             .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "+-INT23",     .ret_x = JAMC_SUCCESS,       .res_x = -23,             .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "!42",         .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   // [40]
   {.expr = "!0",          .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "!1",          .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "!BOOL0",      .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "!INT23",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 23 ^ 0xffffffff, .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "!!BOOL0",     .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [45]
   {.expr = "~42",         .ret_x = JAMC_SUCCESS,       .res_x = 42 ^ 0xffffffff, .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "~0",          .ret_x = JAMC_SUCCESS,       .res_x = 0xffffffff,      .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "~1",          .ret_x = JAMC_SUCCESS,       .res_x = 0xfffffffe,      .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "~BOOL0",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "~INT23",      .ret_x = JAMC_SUCCESS,       .res_x = 23 ^ 0xffffffff, .typ_x = JAM_INTEGER_EXPR,    },
   // [50]
   {.expr = "~~42",        .ret_x = JAMC_SUCCESS,       .res_x = 42,              .typ_x = JAM_INTEGER_EXPR,    },
   // P14-P16: prec 2  binary ops *, /, %
   {.expr = "2*3",         .ret_x = JAMC_SUCCESS,       .res_x = 6,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2*INT23",     .ret_x = JAMC_SUCCESS,       .res_x = 46,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "INT23*2",     .ret_x = JAMC_SUCCESS,       .res_x = 46,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "INT23*INT23", .ret_x = JAMC_SUCCESS,       .res_x = 529,             .typ_x = JAM_INTEGER_EXPR,    },
   // [55]
   {.expr = "-2*3",        .ret_x = JAMC_SUCCESS,       .res_x = -6,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2*-3",        .ret_x = JAMC_SUCCESS,       .res_x = -6,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1*BOOL0",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL0*2",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL0*BOOL1", .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   // [60]
   {.expr = "INT23*BOOL0", .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL1*INT23", .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "2*3*4",       .ret_x = JAMC_SUCCESS,       .res_x = 24,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "6/2",         .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "6/4",         .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   // [65]
   {.expr = "6/0",         .ret_x = JAMC_DIVIDE_BY_ZERO,.res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "6%2",         .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   // P12-P13: prec 3  binary ops +, -
   {.expr = "2+3",         .ret_x = JAMC_SUCCESS,       .res_x = 5,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2147483647+1",.ret_x = JAMC_INTEGER_OVERFLOW, .res_x = 0xdead,       .typ_x = 0xdead,              },
   {.expr = "2+INT23",     .ret_x = JAMC_SUCCESS,       .res_x = 25,              .typ_x = JAM_INTEGER_EXPR,    },
   // [70]
   {.expr = "INT23+2",     .ret_x = JAMC_SUCCESS,       .res_x = 25,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "INT23+INT23", .ret_x = JAMC_SUCCESS,       .res_x = 46,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2+3+4",       .ret_x = JAMC_SUCCESS,       .res_x = 9,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2*3+4",       .ret_x = JAMC_SUCCESS,       .res_x = 10,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2+3*4",       .ret_x = JAMC_SUCCESS,       .res_x = 14,              .typ_x = JAM_INTEGER_EXPR,    },
   // [75]
   {.expr = "2-3",         .ret_x = JAMC_SUCCESS,       .res_x = -1,              .typ_x = JAM_INTEGER_EXPR,    },
#ifdef __code_model_32__
   {.expr = "0-2147483648",.ret_x = JAMC_SUCCESS,       .res_x = -2147483647,     .typ_x = JAM_INTEGER_EXPR,    },
#else
   {.expr = "0-2147483648",.ret_x = JAMC_SUCCESS,       .res_x = -2147483648,     .typ_x = JAM_INTEGER_EXPR,    },
#endif
   {.expr = "S32MIN-1",    .ret_x = JAMC_INTEGER_OVERFLOW, .res_x = 0xdead,       .typ_x = 0xdead,              },
   // P22-P23: prec 4  binary ops <<, >>
   {.expr = "1<<0",        .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1<<30",       .ret_x = JAMC_SUCCESS,       .res_x = 1073741824,      .typ_x = JAM_INTEGER_EXPR,    },
   // [80]
   {.expr = "1<<31",       .ret_x = JAMC_SUCCESS,       .res_x = -2147483648,     .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1<<32",       .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1<<33",       .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1<<34",       .ret_x = JAMC_SUCCESS,       .res_x = 4,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1<<62",       .ret_x = JAMC_SUCCESS,       .res_x = 1073741824,      .typ_x = JAM_INTEGER_EXPR,    },
   // [85]
   {.expr = "1<<63",       .ret_x = JAMC_SUCCESS,       .res_x = -2147483648,     .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1<<1<<2",     .ret_x = JAMC_SUCCESS,       .res_x = 8,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1+1<<2",      .ret_x = JAMC_SUCCESS,       .res_x = 8,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1<<2+3",      .ret_x = JAMC_SUCCESS,       .res_x = 32,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2>>1",        .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   // [90]
   {.expr = "2>>2",        .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2>>4",        .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "S32MIN>>1",   .ret_x = JAMC_SUCCESS,       .res_x = -1073741824,/*?*/.typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "S32MIN>>2",   .ret_x = JAMC_SUCCESS,       .res_x = -536870912, /*?*/.typ_x = JAM_INTEGER_EXPR,    },
   // P26-P29: prec 5  binary ops >, <, >=, <=
   {.expr = "2<4",         .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [95]
   {.expr = "0<4",         .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "1<4",         .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "1<4<2",       .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "1<<1<4",      .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "1<1<<4",      .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [100]
   {.expr = "1<4",         .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "2<=4",        .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "2>4",         .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "2>=4",        .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // P24-P25: prec 6  binary ops ==, !=
   {.expr = "2==2",        .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [105]
   {.expr = "2==2==2",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "2<3==2",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "2==2<3",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "1<<1==2",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "2==1<<1",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [110]
   {.expr = "2!=2",        .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // P17: prec 7  binary ops &
   {.expr = "3&1",         .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "3&0",         .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1&3",         .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "0&3",         .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   // [115]
   {.expr = "3&BOOL1",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL1&3",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL1&BOOL0", .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "3&7==7",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "4==4&3",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   // [120]
   {.expr = "7&3&2",       .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1<<1&3",      .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "4&1<<2",      .ret_x = JAMC_SUCCESS,       .res_x = 4,               .typ_x = JAM_INTEGER_EXPR,    },
   // P19: prec 8  binary ops ^
   {.expr = "3^1",         .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "3^0",         .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   // [125]
   {.expr = "1^3",         .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "0^3",         .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "3^BOOL1",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL1^3",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL1^BOOL0", .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   // [130]
   {.expr = "7^3^12",      .ret_x = JAMC_SUCCESS,       .res_x = 8,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "7^4&12",      .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "7&3^12",      .ret_x = JAMC_SUCCESS,       .res_x = 15,              .typ_x = JAM_INTEGER_EXPR,    },
   // P18: prec 9  binary ops |
   {.expr = "1|2",         .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "0|2",         .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   // [135]
   {.expr = "2|1",         .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "2|0",         .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "3|BOOL1",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL1|3",     .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "BOOL1|BOOL0", .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   // [140]
   {.expr = "1|2|4",       .ret_x = JAMC_SUCCESS,       .res_x = 7,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1|2^4",       .ret_x = JAMC_SUCCESS,       .res_x = 7,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "1^2|4",       .ret_x = JAMC_SUCCESS,       .res_x = 7,               .typ_x = JAM_INTEGER_EXPR,    },
   // P20: prec 10 binary ops &&
   {.expr = "1&&1",        .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "1&&0",        .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [145]
   {.expr = "0&&1",        .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "0&&0",        .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "BOOL1&&BOOL1",.ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "BOOL1&&BOOL0",.ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "BOOL0&&BOOL1",.ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [150]
   {.expr = "BOOL0&&BOOL0",.ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "2&&1",        .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "1&&2",        .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "2&&2",        .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "1&&1&&1",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [155]
   {.expr = "1|1&&1",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "1&&1|1",      .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   // P21: prec 11 binary ops ||
   {.expr = "1||1",        .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "1||0",        .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "0||1",        .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [160]
   {.expr = "0||0",        .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "BOOL1||BOOL1",.ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "BOOL1||BOOL0",.ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "BOOL0||BOOL1",.ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "BOOL0||BOOL0",.ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [165]
   {.expr = "2||1",        .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "1||2",        .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "2||2",        .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "1||1||0",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "1&&1||0",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // [170]
   {.expr = "1||1&&0",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "1&&1==1",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   {.expr = "1==1&&1",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,    },
   // P30: ABS function
   {.expr = "ABS(3)",      .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "ABS(-3)",     .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   // [175]
   {.expr = "-ABS(3)",     .ret_x = JAMC_SUCCESS,       .res_x = -3,              .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "-ABS(3)+3",   .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   // P31: INT function
   {.expr = "INT($BOOL0)",  .ret_x = JAMC_SUCCESS,      .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "INT($BOOL1)",  .ret_x = JAMC_SUCCESS,      .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "INT($INT23)",  .ret_x = JAMC_SUCCESS,      .res_x = 23,              .typ_x = JAM_INTEGER_EXPR,    },
   // P32: LOG2 function
   // [180]
   {.expr = "LOG2(4)",     .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "LOG2(5)",     .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "LOG2(6)",     .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "LOG2(7)",     .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "LOG2(8)",     .ret_x = JAMC_SUCCESS,       .res_x = 3,               .typ_x = JAM_INTEGER_EXPR,    },
   // P33: SQRT function
   // [185]
   {.expr = "SQRT(0)",     .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "SQRT(1)",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "SQRT(2)",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "SQRT(3)",     .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "SQRT(4)",     .ret_x = JAMC_SUCCESS,       .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   // P34: CEIL function
   // [190]
   {.expr = "CEIL(5)",       .ret_x = JAMC_SUCCESS,     .res_x = 5,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "CEIL(6/4)",     .ret_x = JAMC_SUCCESS,     .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   {.expr = "CEIL(SQRT(3))", .ret_x = JAMC_SUCCESS,     .res_x = 2,               .typ_x = JAM_INTEGER_EXPR,    },
   // P35: FLOOR function
   {.expr = "FLOOR(5)",       .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,       .typ_x = 0xdead,              },
   {.expr = "FLOOR(LOG2(4))", .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,       .typ_x = 0xdead,              },
   // [195]
   {.expr = "FLOOR(LOG2(5))", .ret_x = JAMC_TYPE_MISMATCH, .res_x = 0xdead,       .typ_x = 0xdead,              },
   // P36: array
   {.expr = "BOOLAFFE_[0]",  .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,  },
   {.expr = "BOOLAFFE_[1]",  .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,  },
   {.expr = "BOOLAFFE_[62]", .ret_x = JAMC_SUCCESS,       .res_x = 0,               .typ_x = JAM_BOOLEAN_EXPR,  },
   {.expr = "BOOLAFFE_[63]", .ret_x = JAMC_SUCCESS,       .res_x = 1,               .typ_x = JAM_BOOLEAN_EXPR,  },
   // [200]
   {.expr = "INTA5A5_[0]",   .ret_x = JAMC_SUCCESS,       .res_x = 0xa5a50000,      .typ_x = JAM_INTEGER_EXPR,  },
   {.expr = "INTA5A5_[1]",   .ret_x = JAMC_SUCCESS,       .res_x = 0xa5a50001,      .typ_x = JAM_INTEGER_EXPR,  },
   {.expr = "BOOLAFFE_[-1]", .ret_x = JAMC_BOUNDS_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "BOOLAFFE_[64]", .ret_x = JAMC_BOUNDS_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "BOOL_BAFF[-1]", .ret_x = JAMC_BOUNDS_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,            },
   // [205]
   {.expr = "BOOL_BAFF[16]", .ret_x = JAMC_BOUNDS_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "INTA5A5_[-1]",  .ret_x = JAMC_BOUNDS_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "INTA5A5_[2]",   .ret_x = JAMC_BOUNDS_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "INT_5A5A[-1]",  .ret_x = JAMC_BOUNDS_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "INT_5A5A[3]",   .ret_x = JAMC_BOUNDS_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,            },
   // syntax errors seem to cause failures in subsequent good test steps, put them at the end of the test cases
   // [210]
   {.expr = "INTA5A5_[]",     .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "BOOLAFFE_[1..2]",.ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "BOOLAFFE_[]",    .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "INT(0)",         .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "INT(1)",         .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,          .typ_x = 0xdead,            },
   // [215]
   {.expr = "INT(42)",        .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "INT(23+42)",     .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,          .typ_x = 0xdead,            },
   {.expr = "INT($BOOLAFFE_[])",     .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,   .typ_x = 0xdead,            },
   {.expr = "INT($BOOLAFFE_[2..3])", .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,   .typ_x = 0xdead,            },
   // literal boolean array - bit string
   {.expr = "#10001",                .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,   .typ_x = 0xdead,            },
   // [220]
   {.expr = "INT(#10001[0])",        .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,   .typ_x = 0xdead,            },
   {.expr = "INT(#10001[0..2])",     .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,   .typ_x = 0xdead,            },
   {.expr = "INT(#10001[])",         .ret_x = JAMC_SYNTAX_ERROR, .res_x = 0xdead,   .typ_x = 0xdead,            },
   // array identifiers
   {.expr = "BOOLAFFE_",   .ret_x = JAMC_SYNTAX_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,              },
   {.expr = "INT_5A5A",    .ret_x = JAMC_SYNTAX_ERROR,  .res_x = 0xdead,          .typ_x = 0xdead,              },
   // [225]
#ifdef CORE_DUMP
   // literal boolean array - hex string
   {.expr = "$42ff",       .ret_x = JAMC_SYNTAX_ERROR,  .res_x = 10001,           .typ_x = JAM_INTEGER_EXPR,    },
#endif
#ifdef FP_EXCEPTION
   {.expr = "6%0",         .ret_x = JAMC_DIVIDE_BY_ZERO,.res_x = 0xdead,          .typ_x = 0xdead,              },
#endif
};
//============================================================================
// function prototypes
//============================================================================

//============================================================================
// local variables
//============================================================================

//============================================================================
// global variables
//============================================================================

//============================================================================
// helper functions
//============================================================================

//+++++ stapl.c replacement
int urj_jam_jtag_io (int tms, int tdi, int read_tdo)
{
   (void) tms;
   (void) tdi;
   (void) read_tdo;
   return 0; // JAMC_SUCCESS
}

int urj_jam_jtag_io_transfer (int count, char *tdi, char *tdo)
{
   (void) count;
   (void) tdi;
   (void) tdo;
   return 0; // JAMC_SUCCESS
}
void urj_jam_flush_and_delay (int32_t microseconds)
{
   (void) microseconds;
}
int urj_jam_seek (int32_t offset)
{
   (void) offset;
   return 0;
}
int urj_jam_getc (void)
{
   return 0;
}
void urj_jam_message (const char *message_text)
{
   printf("%s(%s)\n", __func__, message_text);
}
void urj_jam_export_integer (const char *key, int32_t value)
{
   (void) key;
   (void) value;
}
void urj_jam_export_boolean_array (
   char *key, unsigned char *data, int32_t count)
{
   (void) key;
   (void) data;
   (void) count;
}
//+++++ end stapl.c replacement

#define STR_INTLR(t) [t] = #t
static const char * typeStr(JAME_EXPRESSION_TYPE type)
{
   static char badType[20];
   static const char * typeAry[JAM_EXPR_MAX] = {
      STR_INTLR(JAM_ILLEGAL_EXPR_TYPE),
      STR_INTLR(JAM_INTEGER_EXPR),
      STR_INTLR(JAM_BOOLEAN_EXPR),
      STR_INTLR(JAM_INT_OR_BOOL_EXPR),
      STR_INTLR(JAM_ARRAY_REFERENCE),
   };
   if (type < JAM_EXPR_MAX)
   {
      return typeAry[type];
   }
   else
   {
      snprintf(badType, 20, "BAD type %d", type);
      return badType;
   }
}
static const char * retStr(JAM_RETURN_TYPE ret)
{
   static char badRet[20];
   static const char * retAry[25] = {
      STR_INTLR(JAMC_SUCCESS),
      STR_INTLR(JAMC_OUT_OF_MEMORY),
      STR_INTLR(JAMC_IO_ERROR),
      STR_INTLR(JAMC_SYNTAX_ERROR),
      STR_INTLR(JAMC_UNEXPECTED_END),
      STR_INTLR(JAMC_UNDEFINED_SYMBOL),
      STR_INTLR(JAMC_REDEFINED_SYMBOL),
      STR_INTLR(JAMC_INTEGER_OVERFLOW),
      STR_INTLR(JAMC_DIVIDE_BY_ZERO),
      STR_INTLR(JAMC_CRC_ERROR),
      STR_INTLR(JAMC_INTERNAL_ERROR),
      STR_INTLR(JAMC_BOUNDS_ERROR),
      STR_INTLR(JAMC_TYPE_MISMATCH),
      STR_INTLR(JAMC_ASSIGN_TO_CONST),
      STR_INTLR(JAMC_NEXT_UNEXPECTED),
      STR_INTLR(JAMC_POP_UNEXPECTED),
      STR_INTLR(JAMC_RETURN_UNEXPECTED),
      STR_INTLR(JAMC_ILLEGAL_SYMBOL),
      STR_INTLR(JAMC_VECTOR_MAP_FAILED),
      STR_INTLR(JAMC_USER_ABORT),
      STR_INTLR(JAMC_STACK_OVERFLOW),
      STR_INTLR(JAMC_ILLEGAL_OPCODE),
      STR_INTLR(JAMC_PHASE_ERROR),
      STR_INTLR(JAMC_SCOPE_ERROR),
      STR_INTLR(JAMC_ACTION_NOT_FOUND),
   };
   if (ret <= JAMC_ACTION_NOT_FOUND)
   {
      return retAry[ret];
   }
   else
   {
      snprintf(badRet, 20, "BAD ret %d", ret);
      return badRet;
   }
}
//============================================================================
// Test code
//============================================================================
static char *statement_buffer = NULL;
static void check_init_symtab_stack(void)
{
   //+++++ urj_jam_execute replacement
   JAM_RETURN_TYPE status = JAMC_SUCCESS;
   status = urj_jam_init_symbol_table ();

   if (status == JAMC_SUCCESS)
   {
      status = urj_jam_init_stack ();
   }

   if (status == JAMC_SUCCESS)
   {
      status = urj_jam_init_jtag ();
   }

   if (status == JAMC_SUCCESS)
   {
      status = urj_jam_init_heap ();
   }

   if (status == JAMC_SUCCESS)
   {
      status = urj_jam_seek (0L);
   }

   if (status == JAMC_SUCCESS)
   {
      statement_buffer = malloc (JAMC_MAX_STATEMENT_LENGTH + 1024);

      if (statement_buffer == NULL)
      {
         status = JAMC_OUT_OF_MEMORY;
      }
   }
   //+++++ end urj_jam_execute replacement
   is_int(status, JAMC_SUCCESS, "urj_jam_execute inits are JAMC_SUCCESS");

   for (int i = 0; i < INITSYMARY_NRELM; ++i)
   {
      const struct sInitSym *const pIS = &InitSymAry[i];
      JAM_RETURN_TYPE res = urj_jam_add_symbol(
         pIS->type, pIS->name, pIS->value, (int32_t) i * 10);
      is_int(res, JAMC_SUCCESS,
             "urj_jam_add_symbol(\"%s\") is JAMC_SUCCESS", pIS->name);
   }
}

void check__urj_jam_evaluate_expression(void)
{
   check_init_symtab_stack();
   for (int i = 0; i < EVAL_EXP_NRELM; ++ i)
   {
      const struct sEvalExpSpec *const pS = &EvalSpecAry[i];
      diag("[%d] urj_jam_evaluate_expression(\"%s\")", i, pS->expr);

      int32_t result = 0xDEADBEEF;
      JAME_EXPRESSION_TYPE result_type = JAM_EXPR_MAX;

      //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
      JAM_RETURN_TYPE res
         = urj_jam_evaluate_expression(
            (char *) pS->expr, // removing const is OK
            &result, &result_type);
      //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      is_int(res, pS->ret_x, "  return value is %s", retStr(pS->ret_x));
      if (pS->ret_x == JAMC_SUCCESS)
      {
         is_int(result_type, pS->typ_x,
                "  result_type is %s",
                typeStr(pS->typ_x));
         is_int(result, pS->res_x, "  result is %d", pS->res_x);
      }
   }
}
