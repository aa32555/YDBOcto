;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Helper functions used by interval bats tests

; Following routine generates 100 queries with varying interval values
; + operation is used with a simple date value of date'2023-01-01' as the
; left operand. Right operand will have a interval.
; Input: Receives "mysql" or "postgres" as argument
;        If argument is "mysql" generates MySQL favorable queries
;        Otherwise generates Postgres favorable queries

getIntervalSyntaxValidationQueries
	new queries,expressionStr,query,mysqlOrPostgres
	set mysqlOrPostgres=$zcmdline
	if ("mysql"=mysqlOrPostgres) set expressionStr="date'2023-01-01' + "
	else  set expressionStr="date'2023-01-01' + "
	set queries=100
	for queries=queries:-1:1 do
	. ; Consider the simple addition operator for this test
	. ; We are testing the syntax of interval operand and not the addition by itself
	. set query="select "
	. set query=query_expressionStr
	. set query=query_$$randomInterval(mysqlOrPostgres)
	. set query=query_";"
	. write query,!
	quit

randomInterval(mysqlOrPostgres)
	; short form
	; 	`Y-M`, `D H:M:S.UUUUUU`
	; short from with type info
	; 	`Y-M` year to month
	;	`D H:M:S.UUUUUU` day to second
	; verbose input
	; 	`x year y month`
	;	`x days y hours z minutes s seconds`
	; verbose input with type info
	; 	`x year y month` year to month
	;	`x days y hours z minutes s seconds` day to seconds
	; year to month and day to seconds, no mixing of month and day
	new randInt,interval
	set randInt=$random(6)+1,interval=""
	if (mysqlOrPostgres["mysql") do
	. ; verbose form not allowed (3,4)
	. ; short form without type info not allowed(1)
	. ; single type interval without type info not allowed(5)
	. ; only 2,6 is possible
	. set randInt=$random(2)
	. set:(0=randInt) randInt=2
	. set:(1=randInt) randInt=6
	if ((1=randInt)!(2=randInt)) do
	. ; short form
	. if (1=randInt) do
	. . ; without type info
	. . set interval=$$shortWithoutTypeInterval
	. else  do
	. . ; with type info
	. . set interval=$$shortWithTypeInterval(mysqlOrPostgres)
	else  if ((3=randInt)!(4=randInt)) do
	. ; verbose input
	. if (3=randInt) do
	. . ; without type info
	. . set interval=$$verboseWithoutTypeInterval
	. else  do
	. . ; with type info
	. . set interval=$$verboseWithTypeInterval
	else  do
	. ; single type interval
	. if (5=randInt) do
	. . ; without type info
	. . set interval=$$singleWithoutTypeInterval
	. else  do
	. . ; with type info
	. . set interval=$$singleWithTypeInterval(mysqlOrPostgres)
	QUIT interval

getFieldVal(field)
	new randInt
	set field=$get(field,"")
	if ("month"=field) do
	. set randInt=$random(12)
	else  if ("minute"=field) do
	. set randInt=$random(60)
	else  if ("second"=field) do
	. set randInt=$random(60) ; Ideally this should be 61 but its difficult to form valid interval with it
	else  do
	. ; Interval field value is limited to 1000 such that interval expression doesn't exceed date/time value range.
	. ; Search for INTERVAL_FIELD_VALUE in this file to see the range of date/time values selected.
	. set randInt=$random(1000)
	quit randInt

getSign()
	quit $select($random(2)=0:$select($random(2)=1:"",1:"+"),1:"-")

singleWithoutTypeInterval()
	; getRandomSingleTypeIntervalWithoutType()
	new interval
	set interval="interval'"_$$getSign_$$getFieldVal_"'"
	quit interval

singleWithTypeInterval(mysqlOrPostgres)
	; getRandomSingleTypeIntervalWithType(mysqlOrPostgres)
	; mysqlOrPostgres argument will be useful if we decide to allow more Postgres types like decade, microseconds etc
	new interval,randInt
	new typeArr,typeArrCnt
	if ("mysqlt"=mysqlOrPostgres) do
	. ; This is a special case we are requested interval with only time fields
	. set typeArr(0)="hour"
 	. set typeArr(1)="minute"
	. set typeArr(2)="second"
	. set typeArrCnt=3
	else  do
	. set typeArr(0)="year"
	. set typeArr(1)="month"
	. set typeArr(2)="day"
	. set typeArr(3)="hour"
	. set typeArr(4)="minute"
	. set typeArr(5)="second"
	. set typeArrCnt=6
	set randInt=$random(typeArrCnt)
	set interval=$$singleWithoutTypeInterval
	set interval=interval_typeArr(randInt)
	quit interval

getVerboseFieldVal()
	quit $$getSign_$$getFieldVal

verboseWithoutTypeInterval()
	; getRandomVerboseFormIntervalWithoutType()
	new interval
	new arr
	set arr(0)="'"_$$getVerboseFieldVal_" year "_$$getVerboseFieldVal_" month'"
	set arr(1)="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour'"
	set arr(2)="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute'"
	set arr(3)="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_"."_$$getFieldVal_" seconds'"
	set arr(4)="'"_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute'"
	set arr(5)="'"_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_"."_$$getFieldVal_" second'"
	set arr(6)="'"_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_" second'"
	set interval="interval"_arr($random(7))
	quit interval

verboseWithTypeInterval()
	; year to month and day to seconds, no mixing of month and day
	; also first type should not be same as second
	; in day to seconds selection, second type should be lesser than the first i.e. day to hour, day to minute
	; getRandomVerboseFormIntervalWithType()
	new interval,randInt
	new typeArr
	set typeArr(1)="day"
	set typeArr(2)="hour"
	set typeArr(3)="minute"
	set typeArr(4)="second"
	; Select type randomly
	if (1=$random(2)) do
	. set x(0)="year"
	. set x(1)="month"
	else  do
	. set x(0)=$random(3)+1 		; day hour minute 1-3
	. set x(1)=$random(4-x(0))+1	; 4-3(max val) = 1 , 4-1(min val) = 3
	. set x(1)=x(0)+x(1)		; 3+1(max val above), 1+1 or 1+2 or 1+3 (min val above)
	. set x(0)=typeArr(x(0))
	. set x(1)=typeArr(x(1))
	if (1=$random(2)) do
	. ; Determine value string based on type selected
	. new arr
	. set arr("year","month")="'"_$$getVerboseFieldVal_" year "_$$getVerboseFieldVal_" month'"
	. set arr("day","hour")="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour'"
	. set arr("day","minute")="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute'"
	. set arr("day","second")="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_"."_$$getFieldVal_" seconds'"
	. set arr("hour","minute")="'"_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute'"
	. set arr("hour","second")="'"_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_"."_$$getFieldVal_" second'"
	. set arr("minute","second")="'"_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_" second'"
	. set interval="interval"_arr(x(0),x(1))_x(0)_" to "_x(1)
	else  do
	. ; Random value string without looking at type
	. ; This works in Postgres and is expected to work in Octo
	. new arr
	. set arr(0)="'"_$$getVerboseFieldVal_" year "_$$getVerboseFieldVal_" month'"
	. set arr(1)="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour'"
	. set arr(2)="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute'"
	. set arr(3)="'"_$$getVerboseFieldVal_" day "_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_"."_$$getFieldVal_" seconds'"
	. set arr(4)="'"_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute'"
	. set arr(5)="'"_$$getVerboseFieldVal_" hour "_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_"."_$$getFieldVal_" second'"
	. set arr(6)="'"_$$getVerboseFieldVal_" minute "_$$getVerboseFieldVal_" second'"
	. set interval="interval"_arr($random(7))_x(0)_" to "_x(1)
	quit interval

shortWithoutTypeInterval()
	; getRandomShortFormIntervalWithoutType()
	new interval
	new arr
	set arr(0)="'"_$$getSign_$$getFieldVal_"-"_$$getFieldVal("month")_"'" ; '1-1'
	set arr(1)="'"_$$getSign_$$getFieldVal_" "_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_"'" ; '1 1:1'
	set arr(2)="'"_$$getSign_$$getFieldVal_" "_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"."_$$getFieldVal_"'" ; '1 1:1:1.111'
	set arr(3)="'"_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_"'"; '1:1'
	set arr(4)="'"_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"."_$$getFieldVal_"'" ; '1:1:1.111'
	set arr(5)="'"_$$getSign_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"."_$$getFieldVal_"'"; '1:1.111'
	set interval="interval"_arr($random(6))
	; set interval="interval'1 1'" ; not allowed in postgres without type info
	; set interval="interval'1 1:1:1.1111111'" ; Only 6 digits are considered
	quit interval

shortWithTypeInterval(mysqlOrPostgres)
	; getRandomShortFormIntervalWithType(mysqlOrPostgres)
	new delim,interval
	; Select delim between types
	new isMySql
	if (mysqlOrPostgres["mysql") set delim="_",isMySql=1
	else  set delim=" to ",isMySql=0
	; Select type randomly
	new typeArr,typeArrCnt
	if (isMySql&(mysqlOrPostgres["mysqlt")) do
	. set typeArr(1)="hour"
	. set typeArr(2)="minute"
	. set typeArr(3)="second"
	. set typeArrCnt=3
	else  do
	. set typeArr(1)="day"
	. set typeArr(2)="hour"
	. set typeArr(3)="minute"
	. set typeArr(4)="second"
	. set typeArrCnt=4
	if (1=$random(2))&'(isMySql&(mysqlOrPostgres["mysqlt")) do
	. set x(0)="year"
	. set x(1)="month"
	else  do
	. set x(0)=$random(typeArrCnt-1)+1 		; day hour minute 1-3
	. set x(1)=$random(typeArrCnt-x(0))+1	; 4-3(max val) = 1 , 4-1(min val) = 3
	. set x(1)=x(0)+x(1)		; 3+1(max val above), 1+1 or 1+2 or 1+3 (min val above)
	. set x(0)=typeArr(x(0))
	. set x(1)=typeArr(x(1))
	; form interval
	new interval
	set interval="interval'1-1'"
	set interval=interval_x(0)_delim_x(1)
	if (isMySql) do
	. ; MYSQL interval
	. ; Determine value string based on type selected
	. new arr
	. set arr("year","month")="'1-1'"
	. set arr("day","hour")="'0'" ; set arr("day","hour")="'1 1'" ; skip this as its not yet supported
	. set arr("day","minute")="'1 1:1'"
	. set arr("day","second")="'1 1:1:1'"
	. set arr("hour","minute")="'1:1'"
	. set arr("hour","second")="'1:1:1'"
	. set arr("minute","second")="'1:1'"
	. set interval="interval"_arr(x(0),x(1))_x(0)_delim_x(1)
	else  do
	. ; Postgres interval
	. if (1=$random(2)) do
	. . ; Determine value string based on type selected
	. . new arr
	. . set arr("year","month")="'"_$$getSign_$$getFieldVal_"-"_$$getFieldVal("month")_"'" ;x-y
	. . set arr("day","hour")="'"_$$getSign_$$getFieldVal_" "_$$getSign_$$getFieldVal_"'" ;x y
	. . set arr("day","minute")="'"_$$getSign_$$getFieldVal_" "_$$getSign_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"'"; x a:b
	. . set arr("day","second")="'"_$$getSign_$$getFieldVal_" "_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"."_$$getFieldVal_"'"; x a:b:c.z
	. . set arr("hour","minute")="'"_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_"'" ; x:y
	. . set arr("hour","second")="'"_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"."_$$getFieldVal_"'"; x:y:z.z
	. . set arr("minute","second")="'"_$$getSign_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"'" ;x:y
	. . set interval="interval"_arr(x(0),x(1))_x(0)_delim_x(1)
	. else  do
	. . ; Random value string without looking at type
	. . ; This works in Postgres and is expected to work in Octo
	. . new arr
	. . set arr(0)="'"_$$getSign_$$getFieldVal_"-"_$$getFieldVal("month")_"'" ; x-y
	. . set arr(1)="'"_$$getSign_$$getFieldVal_" "_$$getSign_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"'" ;x y:z
	. . set arr(2)="'"_$$getSign_$$getFieldVal_" "_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"."_$$getFieldVal_"'"; x a:b:c.z
	. . set arr(3)="'"_$$getSign_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"'" ; x:y
	. . set arr(4)="'"_$$getSign_$$getFieldVal_":"_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"."_$$getFieldVal_"'" ; x:y:z.z
	. . set arr(5)="'"_$$getSign_$$getFieldVal("minute")_":"_$$getFieldVal("second")_"."_$$getFieldVal_"'"; x:y.z
	. . ; set arr(1)="'1 1'" -- error prone
	. . set interval="interval"_arr($random(6))_x(0)_delim_x(1)
	quit interval

getAdditionQueries
	new mysqlOrPostgres,queries
	set mysqlOrPostgres=$zcmdline
	set queries=100
	for queries=queries:-1:1 do
	. write $$getIntervalExpression("+",mysqlOrPostgres),!
	quit

getSubtractionQueries
	new mysqlOrPostgres,queries
	set mysqlOrPostgres=$zcmdline
	set queries=100
	for queries=queries:-1:1 do
	. write $$getIntervalExpression("-",mysqlOrPostgres),!
	quit

getIntervalExpression(op,mysqlOrPostgres)
	; Get date/time operand
	new dt
	set dt=$$dtValue(mysqlOrPostgres)
	; Get interval operand
	new interval,intervalExpr
	; Notify $$randonInterval to return interval with only time fields
	set:(dt["time'")&("mysql"=mysqlOrPostgres) mysqlOrPostgres=mysqlOrPostgres_"t"
	set interval=$$randomInterval(mysqlOrPostgres)
	; Pick randomly whether its date/time op interval or interval op date/time
	if (("-"=op)!(1=$random(2))) set intervalExpr=dt_" "_op_" "_interval
	else  do
	. set intervalExpr=interval_" + "_dt ; Only expected for + operation
	set query="select "_intervalExpr_";"
	quit query

getDate(mysqlOrPostgres)
	new result
	new year,month,day
	if ("mysql"=mysqlOrPostgres) do
	. set year="2023"
	. set month="01"
	. set day="01"
	. set result=year_"-"_month_"-"_day
	else  do
	. ; Pick values between 2000 to 8000 such that interval expression doesn't exceed date/time boundaries.
	. ; Search for INTERVAL_FIELD_VALUE in this file to see the interval field value selection restriction
	. ; done to facilitate this logic.
	. set year=$random(7) ;0-6
	. set year=year+2 ; 2-8
	. set year=year*1000 ;0,1000,2000,3000,4000,5000,6000,7000,8000,9000
	. set year=year+$random(1000) ; year + (a value in range 0 to 999)
	. set length=4-$length(year)
	. for i=1:1:length do
	. . set year="0"_year
	. ; month
	. set month=$random(12)+1 ; 1-12
	. ; day
	. set day=$random(31)+1 ; 1-31
	. ; Jan 31 Feb 28 (29) March 31 April 30 May 31 June 30 July 31 August 31 Sept 30 Oct 31 Nov 30 Dec 31
	. set lclD=day
	. set isLeapYear=0
	. set skip=0
	. ; is leap year?
	. if (0=(year#4)) do
	. . set isLeapYear=1
	. . if (0=(year#100)) do
	. . . if (0'=(year#400)) do
	. . . . set isLeapYear=0
	. . . else  do
	. . . . set isLeapYear=1
	. ; is feb?
	. if (2=month) do
	. . if (29<=lclD) do
	. . . if (1=isLeapYear) set lclD=29
	. . . else  set lclD=28
	. if (31=lclD) do
	. . set lclD=$select(4=month:30,6=month:30,9=month:30,11=month:30,1:30)
	. set day=lclD
	. set result=year_"-"_month_"-"_day
	set result="date'"_result_"'"
	quit result

getTime(mysqlOrPostgres)
	new result,hour,minute,second
	if ("mysql"=mysqlOrPostgres) do
	. set hour="01"
	. set minute="01"
	. set second="01"
	. set result=hour_":"_minute_":"_second
	else  do
	. set hour=$random(24) ; 0-23
	. set minute=$random(60) ; 0-59
	. set second=$random(60) ; 0-59
	. set result=hour_":"_minute_":"_second
	. set:(0=$random(3)) result=result_"."_$random(1000000) ; add microseconds 0-999999
	set result="time'"_result_"'"
	quit result

getTimeZone()
	new result,timezoneHour,timezoneMinute,randInt,value
	set randInt=$random(3) ; 0,1,2
	if (0=randInt) do
	. ; Pick +00:00 - +00:59
	. set timezoneHour="+00"
	. set value=$random(60) ; 0-59
	. set:(10>value) value="0"_value
	. set timezoneMinute=value
	. set timezone=timezoneHour_":"_timezoneMinute
	else  if (1=randInt) do
	. ; Pick -12 to -1
	. set value=$random(12)+1
	. set:(10>value) value="0"_value
	. set timezoneHour=value
	. set value=$random(60) ; 0-59
	. set:(10>value) value="0"_value
	. set timezoneMinute=value
	. set timezone="-"_timezoneHour_":"_timezoneMinute
	else  do
	. ; Pick +1 to +14
	. set value=$random(14)+1
	. set:(10>value) value="0"_value
	. set timezoneHour=value
	. set value=$random(60) ; 0-59
	. set:(10>value) value="0"_value
	. set timezoneMinute=value
	. set timezone="+"_timezoneHour_":"_timezoneMinute
	set result=timezone
	quit result

getTimeTz(mysqlOrPostgres)
	new result
	set result=$$getTime(mysqlOrPostgres)
	set result=$piece(result,"'",2) ; Get only the value in quotes
	set result="time with time zone'"_result
	set result=result_$$getTimeZone()_"'"
	quit result

getTimestamp(mysqlOrPostgres)
	new result,date,time
	set date=$$getDate(mysqlOrPostgres)
	set date=$piece(date,"'",2)
	set time=$$getTime(mysqlOrPostgres)
	set time=$piece(time,"'",2)
	set result="timestamp'"_date_" "_time_"'"
	quit result

getTimestampTz(mysqlOrPostgres)
	new result
	set result=$$getDate(mysqlOrPostgres)
	set result=$piece(result,"'",2) ; Get only the value in quotes
	set result="timestamp with time zone'"_result_" "_$piece($$getTimeTz(mysqlOrPostgres),"'",2)_"'"
	quit result

dtValue(mysqlOrPostgres)
	new arr,arrCnt
	if ("mysql"=mysqlOrPostgres) do
	. set arr(0)=$$getDate(mysqlOrPostgres)
	. set arr(1)=$$getTimestamp(mysqlOrPostgres)
	. set arr(2)=$$getTime(mysqlOrPostgres)
	. set arrCnt=3
	else  do
	. set arr(0)=$$getDate(mysqlOrPostgres)
	. set arr(1)=$$getTimestamp(mysqlOrPostgres)
	. set arr(2)=$$getTime(mysqlOrPostgres)
	. set arr(3)=$$getTimestampTz(mysqlOrPostgres)
	. set arrCnt=4
	. ; Following should ideally be included $$getTimeTz(mysqlOrPostgres) but as Octo and Postgres select on
	. ; time with time zone values differ its hard to test them so skip it
	. ; set arr(4)=$$getTimeTz(mysqlOrPostgres)
	. ; set arrCnt=5
	new dt
	set dt=arr($random(arrCnt))
	quit dt

getDateAddQueries
	new mysqlOrPostgres,queries
	set mysqlOrPostgres=$zcmdline
	set queries=100
	; Get first operand
	new dt
	new interval
	for queries=queries:-1:1 do
	. set dt=$$dtValue(mysqlOrPostgres)
	. set interval=$$randomInterval(mysqlOrPostgres)
	. write "select date_add("_dt_","_interval_");",!
	quit

getDateSubQueries
	new mysqlOrPostgres,queries
	set mysqlOrPostgres=$zcmdline
	set queries=100
	; Get first operand
	new dt
	new interval
	if ("mysql"=mysqlOrPostgres) set func="date_sub"
	else  set func="date_sub"
	for queries=queries:-1:1 do
	. set dt=$$dtValue(mysqlOrPostgres)
	. set interval=$$randomInterval(mysqlOrPostgres)
	. write "select "_func_"("_dt_","_interval_");",!
	quit

getExtractQueries
	new mysqlOrPostgres,queries
	set mysqlOrPostgres=$zcmdline
	set queries=100
	new typeArr,typeArrCnt
	set typeArr(0)="year"
	set typeArr(1)="month"
	set typeArr(2)="day"
	set typeArr(3)="hour"
	set typeArr(4)="minute"
	set typeArr(5)="second"
	set typeArrCnt=6
	new dt,unit,randInt
	for queries=queries:-1:1 do
	. set dt=$$dtValue(mysqlOrPostgres)
	. if (dt["date'") set randInt=$random(3),unit=$select(0=randInt:"year",1=randInt:"month",2=randInt:"day")
	. else  if ((dt["time'")!(dt["time with time zone'")) set randInt=$random(3),unit=$select(0=randInt:"hour",1=randInt:"minute",2=randInt:"second")
	. else  set unit=typeArr($random(typeArrCnt))
	. write "select extract("_unit_" FROM "_dt_");",!
	quit

getRegInterval()
	;
	quit "interval '1'"

generateAllUsages()
	; Simple usages as select column list and all clause of a select statement
	new interval,query
	set interval=$$getRegInterval
	; select interval '1';
	write "select "_interval_";",!
	; values (interval '1');
	write "values ("_interval_");",!
	; select 1 order by interval '1';
	write "select 1 order by "_interval_";",!
	; select 1 group by interval '1';
	write "select 1 group by "_interval_";",!
	; binary
	FOR op="+","-","*","/","%" DO
	. set query="select "_interval_" "_op_"1;"
	. write query,!
	. set query="select 1 "_op_" "_interval_";"
	. write query,!
	FOR op="||" DO
	. set query="select 'text' "_op_" "_interval_";"
	. write query,!
	. set query="select "_interval_" "_op_"'text';"
	. write query,!
	FOR op="OR","AND" DO
	. set query="select "_interval_" "_op_" 1;"
	. write query,!
	. set query="select 1 "_op_" "_interval_";"
	. write query,!
	FOR op="=","!=","<",">","<=",">=" DO
	. FOR op2="","ANY","ALL" DO
	. . IF (""=op2) do
	. . . set query="select "_interval_" "_op_" 1;"
	. . . write query,!
	. . . set query="select 1 "_op_" "_interval_";"
	. . . write query,!
	. . ELSE  do
	. . . set query="select "_interval_" "_op_" "_op2_"(select 1)"_";"
	. . . write query,!
	. . . set query="select 1 "_op_" "_op2_"(select "_interval_");"
	. . . write query,!
	FOR op="LIKE","SIMILAR TO" DO
	. set query="select 'text' "_op_" "_interval_";"
	. write query,!
	. set query="select "_interval_" "_op_"'text';"
	. write query,!
	FOR op="BETWEEN","NOT BETWEEN" DO
	. set query="select "_interval_" "_op_" 1 AND 2;"
	. write query,!
	. set query="select 1 "_op_" "_interval_" AND 2;"
	. write query,!
	FOR op="IN","NOT IN" DO
	. set query="select "_interval_" "_op_"(1,2);"
	. write query,!
	. set query="select 1 "_op_" ("_interval_",2);"
	. write query,!
	; unary expression
	FOR op="+","-" DO
	. set query="select "_op_interval_";"
	. write query,!
	; NOT
	set query="select NOT "_interval_";"
	write query,!
	; EXISTS
	SET query="select EXISTS(select "_interval_");"
	write query,!
	; cast operation
	set query="select "_interval_"::integer;"
	write query,!
	set query="select "_interval_"::numeric;"
	write query,!
	set query="select "_interval_"::varchar;"
	write query,!
	set query="select "_interval_"::bool;"
	write query,!
	set query="select "_interval_"::date;"
	write query,!
	set query="select "_interval_"::time;"
	write query,!
	set query="select "_interval_"::timestamp;"
	write query,!
	set query="select "_interval_"::time with time zone;"
	write query,!
	set query="select "_interval_"::timestamp with time zone;"
	write query,!
	set query="select CAST("_interval_"as integer);"
	write query,!
	set query="select CAST("_interval_"as numeric);"
	write query,!
	set query="select CAST("_interval_"as varchar);"
	write query,!
	set query="select CAST("_interval_"as bool);"
	write query,!
	set query="select CAST("_interval_"as date);"
	write query,!
	set query="select CAST("_interval_"as time);"
	write query,!
	set query="select CAST("_interval_"as timestamp);"
	write query,!
	set query="select CAST("_interval_"as time with time zone);"
	write query,!
	set query="select CAST("_interval_"as timestamp with time zone);"
	write query,!
	; case operation
	set query="select case "_interval_" when 1 then 1 else 2 end;"
	write query,!
	set query="select case when "_interval_" = 1 then 1 else 2 end;"
	write query,!
	set query="select case when 1 = 1 then "_interval_" else 2 end;"
	write query,!
	set query="select case when 1 = 1 then 1 else "_interval_" end;"
	write query,!
	; arrays
	set query="select arrays("_interval_");"
	write query,!
	; aggregate functions
	FOR op="COUNT","SUM","AVG","MIN","MAX" DO
	. FOR op2="","DISTINCT ","ALL " DO
	. . SET query="select "_op_"("_op2_" "_interval_");"
	. . write query,!
	; sql functions
	FOR op="COALESCE","GREATEST","LEAST","NULLIF" DO
	. SET query="select "_op_"("_interval_",1);"
	. write query,!
	. SET query="select "_op_"(1,"_interval_");"
	. write query,!
	; function call expression
	set query="create function samevalue(INTERVAL) returns integer as $$samevalue^functions;"
	write query,!
	set query="create function samevalue(integer) returns INTERVAL as $$samevalue^functions;"
	write query,!
	set query="create function samevalue(integer) returns integer as $$samevalue^functions;"
	write query,!
	set query="select samevalue("_interval_");"
	write query,!
	set query="create table test(id integer, intrvl interval);"
	write query,!
	set query="create table test(id integer, intrvl interval) global"_""_"^test(keys("_""""_"id"_""""_"))"_""_"readonly;"
	write query,!
	quit

getFuncQueries(mysqlOrPostgres)
	; datetime functions
	; now
	; currenttime
	; currenttimestamp
	; localtime
	; localtimestamp
	; select now() + interval'1';
	; select currenttime() + interval'1';
	; select localtime() + inteval'1';
	; select localtimestamp() + interval'1';
	set mysqlOrPostgres=$zcmdline
	write "select (now()::timestamp + "_$$randomInterval(mysqlOrPostgres)_")::varchar(17);",!
	write "select (current_time + "_$$randomInterval(mysqlOrPostgres)_")::varchar(6);",!
	write "select (current_timestamp + "_$$randomInterval(mysqlOrPostgres)_")::varchar(17);",!
	write "select (localtime + "_$$randomInterval(mysqlOrPostgres)_")::varchar(6);",!
	write "select (localtimestamp + "_$$randomInterval(mysqlOrPostgres)_")::varchar(17);",!
	quit
