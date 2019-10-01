/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "octo.h"
#include "octo_types.h"
#include "logical_plan.h"

/**
 * Generates a meta-plan for doing a KEY_<set operation>
 */
LogicalPlan *lp_join_plans(LogicalPlan *a, LogicalPlan *b, LPActionType type) {
	LogicalPlan	*set_operation, *set_option, *set_plans;

	if (NULL == a)
		return b;
	if (NULL == b)
		return a;
	MALLOC_LP_2ARGS(set_operation, LP_SET_OPERATION);
	MALLOC_LP(set_option, set_operation->v.operand[0], LP_SET_OPTION);
	MALLOC_LP_2ARGS(set_option->v.operand[0], type);
	MALLOC_LP(set_plans, set_operation->v.operand[1], LP_PLANS);
	set_plans->v.operand[0] = a;
	set_plans->v.operand[1] = b;
	return set_operation;
}
