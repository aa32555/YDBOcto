/****************************************************************
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <assert.h>
#include <errno.h>
#include <limits.h>

#include "octo.h"
#include "octo_types.h"

#define IS_NUMBER(VAL) (('0' <= *(VAL)) && ('9' >= *(VAL)))
#define IS_SPACE(VAL)  ((' ' == *VAL) || ('\t' == *VAL) || ('\n' == *VAL))
#define RETURN_ERR_IF_NEXT_CHAR_NOT_SPACE_OR_NULL(CHAR_PTR)                                              \
	{                                                                                                \
		if (!((' ' == *(CHAR_PTR + 1)) || ('\t' == *(CHAR_PTR + 1)) || ('\n' == *(CHAR_PTR + 1)) \
		      || ('\0' == *(CHAR_PTR + 1)))) {                                                   \
			/* 'yearmonth' */                                                                \
			/* 'year1'  */                                                                   \
			/* 'yearsmonth' */                                                               \
			/* 'yeara'  */                                                                   \
			/* 'yearsa' */                                                                   \
			/* ERROR return */                                                               \
			RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);                                  \
		}                                                                                        \
	}

/* Following macro will increment CHAR_PTR by NON_PLURAL_LENGTH if the NON_PLURAL_LENGTH+1 character doesn't have 's'. If it does
 * then CHAR_PTR is incremented one more time.
 */
#define CHECK_FOR_PLURAL_AND_ADJUST_ITERATOR(CHAR_PTR, NON_PLURAL_LENGTH)                                         \
	{                                                                                                         \
		int   length_to_increment = NON_PLURAL_LENGTH; /* 1 more will be added at the end of iteration */ \
		char *ch_lcl = CHAR_PTR;                                                                          \
		if ('s' == *(ch_lcl + length_to_increment + 1)) {                                                 \
			/* plural */                                                                              \
			length_to_increment++; /* 1 more will be added at the end of iteration*/                  \
		}                                                                                                 \
		CHAR_PTR = CHAR_PTR + length_to_increment;                                                        \
	}

#define RETURN_INVALID_SYNTAX_ERROR(LIT, STMT)                \
	{                                                     \
		ERROR(ERR_INVALID_INTERVAL_SYNTAX, LIT);      \
		yyerror(NULL, NULL, &STMT, NULL, NULL, NULL); \
		return 1;                                     \
	}

#define STRTOI(STR, RET, LITERAL_STMT)                                        \
	{                                                                     \
		long val = strtol(STR, NULL, 10);                             \
		if ((INT_MAX < val) || (INT_MIN > val)) {                     \
			ERROR(ERR_INTERVAL_FIELD_OUT_OF_RANGE, STR);          \
			yyerror(NULL, NULL, &LITERAL_STMT, NULL, NULL, NULL); \
			return 1;                                             \
		}                                                             \
		RET = (int)val;                                               \
	}

#define GET_MICRO_SECONDS(CH_PTR, PREV, NUM, MICROSECOND)                                                   \
	{                                                                                                   \
		/* process microseconds */                                                                  \
		/* 1:1:1.11 */                                                                              \
		char *microsecond_ptr = CH_PTR + 1;                                                         \
		/* previous value was second */                                                             \
		CH_PTR++;                                                                                   \
		if (IS_NUMBER(microsecond_ptr)) {                                                           \
			/* traverse till we encounter a non-number */                                       \
			char micro[7] = {'0', '0', '0', '0', '0', '0', '\0'};                               \
			int  length = 0;                                                                    \
			while (IS_NUMBER(microsecond_ptr)) {                                                \
				if (6 != length) {                                                          \
					micro[length++] = *microsecond_ptr;                                 \
				} else if (5 < length) {                                                    \
					/* more than 6 precision */                                         \
					/* ignore the additional values for now */                          \
				}                                                                           \
				microsecond_ptr++;                                                          \
			}                                                                                   \
			/* value between second_ptr and microsecond_ptr is microseconds value extract it */ \
			STRTOI(micro, PREV, literal_stmt);                                                  \
			if (is_negative) {                                                                  \
				PREV = -PREV;                                                               \
			}                                                                                   \
			MICROSECOND = PREV;                                                                 \
			NUM = microsecond_ptr - 1;                                                          \
		} else {                                                                                    \
		}                                                                                           \
	}

/* Following functrion parses interval syntax values and forms the SqlInterval structure.
 * Literals are also validated for syntax correctness here.
 */
int qualify_interval(SqlStatement *literal_stmt, SqlStatement *interval_stmt) {
	SqlValue *value;
	UNPACK_SQL_STATEMENT(value, literal_stmt, value);

	SqlInterval *interval;
	UNPACK_SQL_STATEMENT(interval, interval_stmt, interval);

	SqlIntervalType interval_fields_spec = interval->type;
	/* Validate the literal. Intervals are allowed to be specified in verbose and short form syntax.
	 * Following is an example of the short form syntax:
	 * 	y-m d h:m:s.uuuuuu
	 * 	where y -> number of years
	 *	      m -> number of months
	 *	      d -> number of days
	 * 	      h -> number of hours
	 *	      m -> number of minutes
	 *	      s -> number of seconds
	 *	      u -> number of sub seconds
	 * Following is an example of the verbose syntax:
	 * 	1 year 1 month 1 day 1 hour 1 minute 1 second
	 * Literals with mixed notation is also possible:
	 * 	1 year 1 month 1:1:1
	 */
	char *lit = value->v.string_literal;
	/* Initializing here is important as we will not know which got set and which didn't and we would like to sync all of this
	 * to SqlInterval fields.
	 */
	int year = 0;
	int month = 0;
	int day = 0;
	int hour = 0;
	int minute = 0;
	int second = 0;
	int microsecond = 0;
	/* Valid values will look like the following:
	 * 	'1 year 1 month 1 day'
	 * 	'1-1 1 1:1:1.111111'
	 * 	'1 year 1 month 1:1:1'
	 */
	char	 *ch = lit;
	char	 *prev_start;
	char	 *prev_end;
	boolean_t is_prev_short_form = FALSE;
	boolean_t has_value = FALSE;
	boolean_t is_year_specified = FALSE;
	boolean_t is_month_specified = FALSE;
	boolean_t is_day_specified = FALSE;
	boolean_t is_hour_specified = FALSE;
	boolean_t is_minute_specified = FALSE;
	boolean_t is_second_specified = FALSE;
	boolean_t is_short_form_time_specified = FALSE;
	int	  prev = 0;
	char	 *prev_str;
	boolean_t is_positive = FALSE;
	boolean_t is_negative = FALSE;
	boolean_t is_prev_second_floating_point_value = FALSE;
	/* Note:
	 *  * Only year is allowed to be specified in both short form and verbose form.
	 *    Example: '1- 1 year'
	 *  * 1- is considered as 1 year 0 months. When this is specified '1- 1 month' is invalid but '1- 1 year' is okay.
	 *  * If a value is found to be left at the end of the literal without any qualifying unit it is considered to be seconds
	 *    value.
	 *    Example: '1' is 1 second, '1-1 1' is 1 year 1 month 1 second
	 *  * If a value without any qualifying unit occurs in the literal it either has to be a day value in short form '1 1:'
	 *    notation or it needs to be at the end of the literal making it depend on the field specification.
	 *    If it doesn't fall in either of the two categories an invalid syntax error is generated.
	 *    Example: '1 1' -> ERROR
	 * 	       '1 1:' -> valid
	 * 	       '1 hour 1'-> valid
	 *    * If its the second category the unit is consider the value of the lowest field in the field specification
		Example: '1: 1'hour to minute -> 1 hour 1 minute
			 '1: 1'hour to second -> 1 hour 1 second
	 *  * Any delimiter can exist between two valid tokens. A token can be a number(quantity), unit (year,month,..), short form
	 *    notation value (1-1,1:,..), positive or negative signs.
	 *    * After unit i.e. year,month,... space is required. Any delimiter is allowed other this scenario.
	 *  * Also later code ignores values which belong to fields after the least significant field specification
	 */
	while ('\0' != *ch) {
		switch (*ch) {
		case 'y':
			// year
			if (('e' == *(ch + 1)) && ('a' == *(ch + 2)) && ('r' == *(ch + 3))) {
				CHECK_FOR_PLURAL_AND_ADJUST_ITERATOR(ch, 3);
			} else {
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
				// 'yeaa'
			}
			RETURN_ERR_IF_NEXT_CHAR_NOT_SPACE_OR_NULL(ch);
			if (is_prev_short_form || !has_value) {
				// ignore this input
			} else {
				if (is_year_specified) {
					RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
					// '1-1 year year 1 year 1 year
				}
				/* Store the year value.
				 * The year value is aggregated because its possible to reach here for the following query which is
				 * valid and is expected to add the years.
				 * 	select date'01-01-2023' + interval'1- 1 year';
				 * Since we initialize and do all they error checks before this point we are safe to do the
				 * operation.
				 */
				year += prev;
				is_year_specified = TRUE;
				has_value = FALSE;
			}
			break;
		case 'm':;
			// month
			// minute
			boolean_t is_month = FALSE;
			if (('o' == *(ch + 1)) && ('n' == *(ch + 2)) && ('t' == *(ch + 3)) && ('h' == *(ch + 4))) {
				CHECK_FOR_PLURAL_AND_ADJUST_ITERATOR(ch, 4);
				is_month = TRUE;
			} else if (('i' == *(ch + 1)) && ('n' == *(ch + 2))
				   && ('u' == *(ch + 3) && ('t' == *(ch + 4)) && ('e' == *(ch + 5)))) {
				CHECK_FOR_PLURAL_AND_ADJUST_ITERATOR(ch, 5);
			} else {
				// 'mon'
				// 'mona'
				// 'minua'
				// 'min'
				// RETURN ERROR
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			}
			RETURN_ERR_IF_NEXT_CHAR_NOT_SPACE_OR_NULL(ch);
			if (is_prev_short_form || !has_value) {
				// ignore this input
			} else {
				if (is_month && is_month_specified) {
					// '1-1 month month 1 month 1 month
					// '1-1 1 month'
					// '1- 1 month'
					// return ERROR
					RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
				} else if (!is_month && (is_minute_specified || is_short_form_time_specified)) {
					// return ERROR
					RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
					// '1-1 month month 1 month 1 month
				}
				if (is_month) {
					month = prev;
					is_month_specified = TRUE;
					has_value = FALSE;
				} else {
					minute = prev;
					is_minute_specified = TRUE;
					has_value = FALSE;
				}
			}
			break;
		case 'd':
			// day
			if (('a' == *(ch + 1)) && ('y' == *(ch + 2))) {
				CHECK_FOR_PLURAL_AND_ADJUST_ITERATOR(ch, 2);
			} else {
				// 'dan'
				// RETURN ERROR
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			}
			RETURN_ERR_IF_NEXT_CHAR_NOT_SPACE_OR_NULL(ch);
			if (is_prev_short_form || !has_value) {
				// ignore this input
			} else {
				if (is_day_specified) {
					// '1 day 1 day'
					// '1 1:1:1 1 day'
					// return ERROR
					RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
				}
				day = prev;
				is_day_specified = TRUE;
				has_value = FALSE;
			}
			break;
		case 'h':
			// hour
			if (('o' == *(ch + 1)) && ('u' == *(ch + 2)) && ('r' == *(ch + 3))) {
				CHECK_FOR_PLURAL_AND_ADJUST_ITERATOR(ch, 3);
			} else {
				// RETURN ERROR
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
				// 'houa'
			}
			RETURN_ERR_IF_NEXT_CHAR_NOT_SPACE_OR_NULL(ch);
			if (is_prev_short_form || !has_value) {
				// ignore this input
			} else {
				if (is_hour_specified || is_short_form_time_specified) {
					// '1 hour 1 hour'
					// '1:1 1 hour'
					// return ERROR
					RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
				}
				hour = prev;
				is_hour_specified = TRUE;
				has_value = FALSE;
			}
			break;
		case 's':
			// second
			if (('e' == *(ch + 1)) && ('c' == *(ch + 2)) && ('o' == *(ch + 3)) && ('n' == *(ch + 4))
			    && ('d' == *(ch + 5))) {
				CHECK_FOR_PLURAL_AND_ADJUST_ITERATOR(ch, 5);
			} else {
				// RETURN ERROR
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
				// 'se'
			}
			RETURN_ERR_IF_NEXT_CHAR_NOT_SPACE_OR_NULL(ch);
			if (is_prev_second_floating_point_value) {
				is_second_specified = TRUE;
				has_value = FALSE;
			} else if (is_prev_short_form || !has_value) {
				// ignore this input
			} else {
				if (is_second_specified || is_short_form_time_specified) {
					// return ERROR
					RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
					// '1-1 year year 1 year 1 year
				}
				// convert chars between prev_start and prev_end to integer
				second = prev;
				is_second_specified = TRUE;
				has_value = FALSE;
			}
			break;
		case ' ':
		case '\t':
		case '\n':
			// Ignore space chars
			break;
		default:
			if (IS_NUMBER(ch)) {
				char *num = ch;
				while (IS_NUMBER(num)) {
					num++;
				}
				// Allow short form day's notation '1 1:'
				if (has_value) {
					if ((':' != *num) && ('.' != *num)) {
						if (INTERVAL_DAY_HOUR == interval_fields_spec) {
							// '1 1' day to hour
						} else {
							// '1 1' -- in mysql this is allowed but in postgres its an error
							// '1 1 day'
							RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
						}
					}
					day = prev;
					is_day_specified = TRUE;
					has_value = FALSE;
				}
				prev_start = ch;
				prev_end = num - 1;
				// consider is_positive and is_negative and form the value and store it in prev
				prev_str = octo_cmalloc(memory_chunks, prev_end - prev_start + 2);
				strncpy(prev_str, prev_start, prev_end - prev_start + 1);
				prev_str[prev_end - prev_start + 1] = '\0';
				STRTOI(prev_str, prev, literal_stmt);
				if (is_negative) {
					prev = -prev;
				}
				if ('.' == *num) {
					// '1.1'
					// 1.1 1.1'
					// '1.1 1:1'
					// '1.1 day'
					// '1.1 second'
					second = prev;
					num++;
					char *num2 = num;
					char  micro[7] = {'0', '0', '0', '0', '0', '0', '\0'};
					int   length = 0;
					while (IS_NUMBER(num2)) {
						if (6 != length) {
							micro[length++] = *num2;
						} else if (5 < length) {
							// more than 6 precision
							// ignore the additional values for now
						}
						num2++;
					}
					STRTOI(micro, prev, literal_stmt);
					if (is_negative) {
						prev = -prev;
					}
					microsecond = prev;
					num = num2;
					if (IS_SPACE(num) && (('s' == *(num + 1)) && ('e' == *(num + 2)) && ('c' == *(num + 3)))) {
						// This is a floating point value and we only support it as seconds in long form
						is_prev_second_floating_point_value = TRUE;
						// let below code for space processing take care of breaking and going to seconds
						// case block
					} else {
						RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
					}
				}
				if (IS_SPACE(num)) {
					has_value = TRUE;
					is_positive = is_negative = FALSE; // consumed already
					is_prev_short_form = FALSE;
					ch = num - 1;
					// End go to next iteration of while loop
					break;
				} else if ('-' == *num) {
					if (has_value || is_month_specified) {
						// '1 1-1'
						// '1 month 1-1'
						RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
					}
					is_prev_short_form = TRUE;
					is_month_specified = TRUE;
					// Store year
					year = prev;
					if (IS_NUMBER(num + 1)) {
						char *month_ptr = num + 1;
						// previous value was year
						num++;
						// traverse till we encounter a non_NUMBER
						while (IS_NUMBER(month_ptr)) {
							month_ptr++;
						}
						// value between num and month_ptr is month value extract it and go back to outer
						// loop
						prev_start = num;
						prev_end = month_ptr - 1;
						prev_str = octo_cmalloc(memory_chunks, prev_end - prev_start + 2);
						strncpy(prev_str, prev_start, prev_end - prev_start + 1);
						prev_str[prev_end - prev_start + 1] = '\0';
						STRTOI(prev_str, prev, literal_stmt);
						if (11 < prev) {
							ERROR(ERR_INTERVAL_FIELD_OUT_OF_RANGE, lit);
							yyerror(NULL, NULL, &literal_stmt, NULL, NULL, NULL);
							return 1;
						}
						if (is_negative) {
							prev = -prev;
						}
						month = prev;
						num = month_ptr - 1;
					} else {
						// only year was specified go back to outer loop
						if ('-' == *(num + 1)) {
							// '1--1'
							RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
						}
					}
					is_positive = is_negative = FALSE; // consumed
					ch = num;
				} else if (':' == *num) {
					if (is_short_form_time_specified) {
						// return ERROR
						// '1:+1:1'
						// '1:-1:1'
						RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
					}
					if (has_value) {
						// '1 1:'
						day = prev;
						is_day_specified = TRUE;
						has_value = FALSE;
					}
					// After specifying time value either verbose or short form of time specification is
					// not allowed
					is_short_form_time_specified = TRUE;
					is_prev_short_form = TRUE;
					// '1:1.111'day to hour
					// Store hour
					hour = prev;
					if (IS_NUMBER(num + 1)) {
						char *minute_ptr = num + 1;
						// previous value was hour
						num++;
						// traverse till we encounter a non_number
						while (IS_NUMBER(minute_ptr)) {
							minute_ptr++;
						}
						// value between num and minute_ptr is minute value extract it
						prev_start = num;
						prev_end = minute_ptr - 1;
						prev_str = octo_cmalloc(memory_chunks, prev_end - prev_start + 2);
						strncpy(prev_str, prev_start, prev_end - prev_start + 1);
						prev_str[prev_end - prev_start + 1] = '\0';
						STRTOI(prev_str, prev, literal_stmt);
						if ((':' == *minute_ptr) && (59 < prev)) {
							ERROR(ERR_INTERVAL_FIELD_OUT_OF_RANGE, lit);
							yyerror(NULL, NULL, &literal_stmt, NULL, NULL, NULL);
							return 1;
						}
						if (is_negative) {
							prev = -prev;
						}
						minute = prev;
						num = minute_ptr - 1;
						if (':' == *minute_ptr) {
							char *second_ptr = minute_ptr + 1;
							// Previous value was minute
							minute_ptr++;
							if (IS_NUMBER(second_ptr)) {
								// traverse till we encounter a non-number
								while (IS_NUMBER(second_ptr)) {
									second_ptr++;
								}
								// value between minute_ptr and second_ptr is second value extract
								// it
								prev_start = minute_ptr;
								prev_end = second_ptr - 1;
								prev_str = octo_cmalloc(memory_chunks, prev_end - prev_start + 2);
								strncpy(prev_str, prev_start, prev_end - prev_start + 1);
								prev_str[prev_end - prev_start + 1] = '\0';
								STRTOI(prev_str, prev, literal_stmt);
								if (60 < prev) {
									ERROR(ERR_INTERVAL_FIELD_OUT_OF_RANGE, lit);
									yyerror(NULL, NULL, &literal_stmt, NULL, NULL, NULL);
									return 1;
								}
								if (is_negative) {
									prev = -prev;
								}
								second = prev;
								num = second_ptr - 1;
								if ('.' == *second_ptr) {
									GET_MICRO_SECONDS(second_ptr, prev, num, microsecond);
								}
							} else {
							}
						} else {
							// process micro seconds here
							if ('.' == *minute_ptr) {
								// 1:1.111'day to hour
								// hour previously stored is minute and minute stored is second
								second = minute;
								minute = hour;
								hour = 0;
								GET_MICRO_SECONDS(minute_ptr, prev, num, microsecond);
							} else if ('\0' == *minute_ptr) {
								// '1:1' minute to second
								if (INTERVAL_MINUTE_SECOND == interval_fields_spec) {
									second = minute;
									minute = hour;
									hour = 0;
								}
							}
							if ((59 < minute) || (60 < second)) {
								ERROR(ERR_INTERVAL_FIELD_OUT_OF_RANGE, lit);
								yyerror(NULL, NULL, &literal_stmt, NULL, NULL, NULL);
								return 1;
							}
						}
					} else {
					}
					is_positive = is_negative = FALSE; // consumed already
					ch = num;
				} else {
					// quantity specified
					has_value = TRUE;
					is_positive = is_negative = FALSE; // consumed already
					is_prev_short_form = FALSE;
					ch = num - 1;
				}
				// go forward
				// Let the next loop iteration decide to which decide next steps
			} else if (('+' == *ch) || ('-' == *ch)) {
				is_positive = ('+' == *ch) ? TRUE : FALSE;
				is_negative = !is_positive;
			} else if ('.' == *ch) {
				// Do not expect '.'
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// Consider anything else as delimiter between two tokens so ignore it
			}
			break;
		}
		ch++;
		if ('\0' == *ch) {
			if (is_positive || is_negative) {
				// sign coming after unit and this is not a short form
				// '1+'
				// '1-+'
				// '1:1:-'
				// '1:1:+'
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			}
		}
	}
	if (has_value) {
		// '1'
		// '1 day 1'
		// '1-1 1'
		// Use field specification to determine the unit
		// If field specification is absent then seconds is the unit
		switch (interval_fields_spec) {
		case INTERVAL_NO_TYPE:
			if (is_second_specified || is_short_form_time_specified) {
				// '1:1+1'
				// '1:1-1'
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1:1 1'
				second = prev;
				is_second_specified = TRUE;
			}
			break;
		case INTERVAL_YEAR:
			/* Its possible to reach here with a non-zero value for year.
			 * Following query is an example for such input.
			 * 	select date'01-01-2023' + interval'1- 1'year;
			 * Aggregate the year value instead of simply assigning it. This
			 * takes care to consider both the values.
			 */
			year += prev;
			is_hour_specified = TRUE;
			break;
		case INTERVAL_MONTH:
			if (is_month_specified) {
				// '1 month 1' month
				// '1- 1'month
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1 year 1'month
				// '1:1 1'month
				// '1 1:1 1'month
				month = prev;
				is_month_specified = TRUE;
			}
			break;
		case INTERVAL_DAY:
			if (is_day_specified) {
				// '1 1:1 1'day
				// '1 day 1'day
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'day
				// '1:1 1'day
				// '1 hour 1'day
				day = prev;
				is_day_specified = TRUE;
			}
			break;
		case INTERVAL_HOUR:
			if (is_hour_specified || is_short_form_time_specified) {
				// '1:1 1'hour
				// '1 hour 1'hour
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'hour
				// '1 second 1'hour
				hour = prev;
				is_hour_specified = TRUE;
			}
			break;
		case INTERVAL_MINUTE:
			if (is_minute_specified || is_short_form_time_specified) {
				// '1:1 1'minute
				// '1 minute 1'minute
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'minute
				// '1 hour 1'minute
				// '1 second 1'minute
				minute = prev;
				is_minute_specified = TRUE;
			}
			break;
		case INTERVAL_SECOND:
			if (is_second_specified || is_short_form_time_specified) {
				// '1:1 1'second
				// '1 second 1'second
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'second
				// '1 month 1'second
				// '1 hour 1'second
				second = prev;
				is_second_specified = TRUE;
			}
			break;
		case INTERVAL_YEAR_MONTH:
			if (is_month_specified) {
				// '1-1 1'year to month
				// '1 year 1 month 1'year to month
				// '1 month 1'year to month
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1:1 1'year to month
				// '1 day 1'year to month
				// '1 month'year to month
				month = prev;
				is_month_specified = TRUE;
			}
			break;
		case INTERVAL_DAY_SECOND:
			if (is_second_specified || is_short_form_time_specified) {
				// '1: 1'day to second
				// '1 second 1'day to second
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'day to second
				// '1 hour 1'day to second
				// '1 day 1'day to second
				// '1'day to second
				second = prev;
				is_second_specified = TRUE;
			}
			break;
		case INTERVAL_DAY_MINUTE:
			if (is_minute_specified || is_short_form_time_specified) {
				// '1: 1'day to minute
				// '1 minute 1'day to minute
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'day to minute
				// '1 hour 1'day to minute
				// '1 day 1'day to minute
				// '1'day to minute
				minute = prev;
				is_minute_specified = TRUE;
			}
			break;
		case INTERVAL_DAY_HOUR:
			if (is_hour_specified || is_short_form_time_specified) {
				// '1: 1'day to hour
				// '1 hour 1'day to hour
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'day to hour
				// '1 minute 1'day to hour
				// '1 day 1'day to hour
				// '1'day to hour
				hour = prev;
				is_hour_specified = TRUE;
			}
			break;
		case INTERVAL_HOUR_SECOND:
			if (is_second_specified || is_short_form_time_specified) {
				// '1: 1'hour to second
				// '1 second 1'hour to second
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'hour to second
				// '1 minute 1'hour to second
				// '1 hour 1'hour to second
				// '1 day 1'hour to second
				// '1'hour to second
				second = prev;
				is_second_specified = TRUE;
			}
			break;
		case INTERVAL_HOUR_MINUTE:
			if (is_minute_specified || is_short_form_time_specified) {
				// '1: 1'hour to minute
				// '1 minute 1'hour to minute
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'hour to minute
				// '1 hour 1'hour to minute
				// '1 second 1'hour to minute
				// '1 day 1'hour to minute
				// '1'hour to minute
				minute = prev;
				is_minute_specified = TRUE;
			}
			break;
		case INTERVAL_MINUTE_SECOND:
			if (is_second_specified || is_short_form_time_specified) {
				// '1: 1'minute to second
				// '1 second 1'minute to second
				RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
			} else {
				// '1-1 1'minute to second
				// '1 minute 1'minute to second
				// '1 day 1'minute to second
				// '1'minute to second
				second = prev;
				is_second_specified = TRUE;
			}
			break;
		default:
			assert(FALSE);
			return 1;
			break;
		}
	}
	if (!is_year_specified && !is_month_specified && !is_day_specified && !is_hour_specified && !is_minute_specified
	    && !is_second_specified && !is_short_form_time_specified) {
		// 'year month'
		// 'year'
		// RETURN ERROR
		RETURN_INVALID_SYNTAX_ERROR(lit, literal_stmt);
	}
	/* Postgres stores interval data as three variables. Day, Month and Microseconds. This leads to expressions having
	 * results offset by a small amount. To have the same result in Octo following calculation is performed.
	 */
	int tmp_seconds;
	tmp_seconds = second;
	tmp_seconds += minute * 60;
	tmp_seconds += hour * 3600;

	hour = tmp_seconds / 3600;
	tmp_seconds = tmp_seconds % 3600;
	minute = tmp_seconds / 60;
	tmp_seconds = tmp_seconds % 60;
	second = tmp_seconds;
	/* Without the above logic following example results in the difference shown below.
	 * 	select date'1-27-7378' - interval'+8 hour -1 minute -1 second'day to minute;
	 * Postgres output: 7378-01-26 16:02:00
	 * Octo: 	    7378-01-26 16:01:00
	 * Following demonstrates what the interval will look like after the above computation.
	 * 	select date'1-27-7378'-interval'7 hours 58 minutes';
	 * Output: 7378-01-26 16:02:00
	 *
	 */
	/* If there is no field spec do not normalize or normalize such that the if field spec has time field in it ensure it is
	 * does not become zero. Time information will be consolidated and passed to mktime to normalize
	 * later. If all of time information is lost here then mktime call will not correctly consider
	 * daylight savings boundaries.
	 * Following example demonstrates how time information affects the result near DST boundaries:
	 * names=> select timestamp with time zone '11-08-2024 08:00:00-05' - interval '744 hours';
	 *         ?column?
	 * ------------------------
	 *  2024-10-08 09:00:00-04
	 * (1 row)
	 *
	 * names=> select timestamp with time zone '11-08-2024 08:00:00-05' - interval '31 days';
	 *         ?column?
	 * ------------------------
	 *  2024-10-08 08:00:00-04
	 */
	if (INTERVAL_NO_TYPE != interval_fields_spec) {
		// Normalize values
		if ((INTERVAL_SECOND != interval_fields_spec) && (INTERVAL_DAY_SECOND != interval_fields_spec)
		    && (INTERVAL_HOUR_SECOND != interval_fields_spec) && (INTERVAL_MINUTE_SECOND != interval_fields_spec)) {
			if ((59 < second) || (-59 > second)) {
				// if second is greater than 59 add minutes to minute and record remaining seconds only in second
				// select date'01-01-2023' + interval '59 minute 60 second'day to hour;
				minute += (int)second / 60;
				second = second % 60;
			}
			if ((INTERVAL_MINUTE != interval_fields_spec) && (INTERVAL_DAY_MINUTE != interval_fields_spec)
			    && (INTERVAL_HOUR_MINUTE != interval_fields_spec)) {
				if ((59 < minute) || (-59 > minute)) {
					// if minute is greater than 59 add hours to hour and record remaining minutes only in
					// minute select date'01-01-2023' + interval '60 minute 1 second'day to hour;
					hour += (int)minute / 60;
					minute = minute % 60;
				}
				if ((INTERVAL_HOUR != interval_fields_spec) && (INTERVAL_DAY_HOUR != interval_fields_spec)) {
					if ((23 < hour) || (-23 > hour)) {
						// if hour is greater than 23 add day to day and record remaining hours only in hour
						// select date'01-01-2023'+interval '1 day 24 hour' minute to second;
						day += (int)hour / 24;
						hour = hour % 24;
					}
					if ((11 < month) || (-11 > month)) {
						// if month is greater than 12 add year to year and record remaining months only in
						// month select date'01-01-2023'+interval '12 months'year;
						year += (int)month / 12;
						month = month % 12;
					}
					// nothing to do with days
				}
			}
		}
	}

	/* Ignore values which belong to fields after the least significant field specification.
	 * Following input can be used to test all of the following paths
	 * 	'1 year 1 month 1 day 1 hour 1 minute 1 second'
	 * 	'1-1 1:1:1'
	 * Also, though we set the non-specified fields to zero, we need to consider if the immediate next field affects
	 * the current field. Day doesn't affect any field or get affected by any other field.
	 */
	switch (interval_fields_spec) {
	case INTERVAL_NO_TYPE:
		// Accept all the values so far deduced from literal
		break;
	case INTERVAL_YEAR:
		if (0 > month) {
			// if ((0 <= year) || ((0 > year) && (-11 > month))) {
			assert(-12 < month);
			if (0 < year) {
				year -= 1;
			}
		}
		// ignore all values apart from year
		month = day = hour = minute = second = microsecond = 0;
		break;
	case INTERVAL_MONTH:
		// ignore all values of fields that come after month
		day = hour = minute = second = microsecond = 0;
		break;
	case INTERVAL_DAY:
		// ignore all values of fields that come after day
		hour = minute = microsecond = second = 0;
		break;
	case INTERVAL_HOUR:
		if (0 > microsecond) {
			if (0 <= second) {
				second -= 1;
			}
		}
		if (0 > second) {
			if (0 <= minute) {
				minute -= 1;
			}
		}
		if (0 > minute) {
			if (0 < hour) {
				hour -= 1;
			}
		}
		// ignore all values of fields that come after hour
		minute = second = microsecond = 0;
		break;
	case INTERVAL_MINUTE:
		if (0 > microsecond) {
			if (0 <= second) {
				second -= 1;
			}
		}
		if (0 > second) {
			if (0 < minute) {
				minute -= 1;
			}
		}
		// ignore all values of fields that come after minute
		second = microsecond = 0;
		break;
	case INTERVAL_SECOND:
		// All values are considered
		break;
	case INTERVAL_YEAR_MONTH:
		// ignore all values of fields that come after month
		day = hour = minute = second = microsecond = 0;
		break;
	case INTERVAL_DAY_SECOND:
		// nothing to ignore as last field is second
		break;
	case INTERVAL_DAY_MINUTE:
		if (0 > microsecond) {
			if (0 <= second) {
				second -= 1;
			}
		}
		if (0 > second) {
			if (0 < minute) {
				minute -= 1;
			}
		}
		// ignore all values of fields that come after minute
		second = microsecond = 0;
		break;
	case INTERVAL_DAY_HOUR:
		if (0 > microsecond) {
			if (0 < second) {
				second -= 1;
			}
		}
		if (0 > second) {
			if (0 < minute) {
				minute -= 1;
			}
		}
		if (0 > minute) {
			if (0 < hour) {
				hour -= 1;
			}
		}
		// ignore all values of fields that come after hour
		minute = second = microsecond = 0;
		break;
	case INTERVAL_HOUR_SECOND:
		// nothing to ignore as last field is second
		break;
	case INTERVAL_HOUR_MINUTE:
		if (0 > microsecond) {
			if (0 <= second) {
				second -= 1;
			}
		}
		if (0 > second) {
			if (0 < minute) {
				minute -= 1;
			}
		}
		// ignore all values of fields that come after minute
		second = microsecond = 0;
		break;
	case INTERVAL_MINUTE_SECOND:
		// nothing to ignore as last field is second
		break;
	default:
		assert(FALSE);
		return 1;
		break;
	}
	// Set the final interval values to SqlInterval structure
	interval->year = year;
	interval->month = month;
	interval->day = day;
	interval->hour = hour;
	interval->minute = minute;
	interval->second = second;
	interval->microsecond = microsecond;

	return 0;
}
