/**
 * \author SPDX-FileCopyrightText: 2022 Peter Poeschl <pp+ujt2208@nest-ai.de>
 *
 * \copyright SPDX-License-Identifier: GPL-2.0-or-later
 *
 * \file jamexp_shrd.h
 * \brief Common test function to check urj_jam_evaluate_expression() results.
 */

#ifndef JAMEXP_SHRD_H
#define JAMEXP_SHRD_H

/// Number of elements in InitSymAry.
#define INITSYMARY_NRELM 12
/// Number of tests in check_init_symtab_stack().
#define CHECK_INIT_SYMTAB_STACK \
   (0                           \
    + 1                         \
    + INITSYMARY_NRELM          \
      )
/// Number of EvalSpecAry elements with expected .ret_x == JAMC_SUCCESS,
#define EVAL_EXP_NRELM_GOOD 172
/// Number of EvalSpecAry elements with expected .ret_x != JAMC_SUCCESS.
#define EVAL_EXP_NRELM_BAD 61
/// Number of EvalSpecAry elements
#define EVAL_EXP_NRELM (EVAL_EXP_NRELM_GOOD + EVAL_EXP_NRELM_BAD)
/// Number of tests in EvalSpecAry element with .ret_x == JAMC_SUCCESS.
#define EVAL_EXP_NRCHK_GOOD 3
/// Number of tests in EvalSpecAry element with .ret_x != JAMC_SUCCESS.
#define EVAL_EXP_NRCHK_BAD 1
/// Number of tzests in check__urj_jam_evaluate_expression().
#define CHECK__URJ_JAM_EVALUATE_EXPRESSION              \
   ( 0                                                  \
     + (EVAL_EXP_NRELM_GOOD * EVAL_EXP_NRCHK_GOOD)      \
     + (EVAL_EXP_NRELM_BAD * EVAL_EXP_NRCHK_BAD)        \
      )
/// Number of planned tests.
#define PLAN_TESTS                              \
   ( 0                                          \
     + CHECK_INIT_SYMTAB_STACK                  \
     + CHECK__URJ_JAM_EVALUATE_EXPRESSION       \
      )

extern void check__urj_jam_evaluate_expression(void);
#endif // JAMEXP_SHRD_H
