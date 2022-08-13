/**
 * \author SPDX-FileCopyrightText: 2022 Peter Poeschl <pp+ujt2208@nest-ai.de>
 *
 * \copyright SPDX-License-Identifier: GPL-2.0-or-later
 *
 * \file jamexp_nongen.c
 * \brief Unit test program for non-generated stapl/jamexp.c
 *
 * Test idea:
 * * Assume jamexp.c is correct
 * * Create tests for urj_jam_evaluate_expression() to exercise all productions
 * * From the output of an instrumented jamexp.c it should be possible to
 *   reverse-engineer a GNU bison grammar file jamexp.y.
 */

#include "jamexp_shrd.h"
#include "tap/basic.h"

int main(void)
{
   plan(PLAN_TESTS);

   check__urj_jam_evaluate_expression();

}
