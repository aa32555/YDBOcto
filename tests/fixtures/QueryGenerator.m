;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	; #FUTURE_TODO: Implement subqueries into other places (Group By, Having, etc.). Example below
	;               SELECT n1.id from names n1 where n1.id * (select n2.id from names n2 where n2.id = n1.id % 3) = n1.id * 2;
	; #FUTURE_TODO: Make it so subqueries from the select list can be used in other places (Assuming that they can)
	;               EX: Select (subquery) AS alias FROM table WHERE alias = 1;
	; #FUTURE_TODO: Add aliases into FROM clause like example query below.
	;               SELECT n1.id FROM names n1;
	; #FUTURE_TODO: Add typecast operator (::) to valid places, example usage below
	;               https://www.postgresql.org/docs/9.4/sql-expressions.html#SQL-SYNTAX-TYPE-CASTS
	;               New: CAST ("123" as INTEGER)  Old (SQL-92 Compliant): "123"::INTEGER
	; #FUTURE_TODO: Add TRUE and FALSE into any place that has expressions (WHERE, ON, HAVING, ...)
	;               Started with version 9 of WHERE clause
	;               WHERE ((id = 1) = TRUE)
	; #FUTURE_TODO: Expand aggregate functions
	;               SUM(column) -> SUM(column1 + column2 ... + 1 * 100)
	; #FUTURE_TODO: Put aggregate functions into other places in the query
	; #FUTURE_TODO: Combine whereClause(), innerWhereClause(), and havingClause as they are all nearly identical to each other
	; #FUTURE_TODO: Test to see if CASEs comparisons can be made against other CASEs, if so
	;               implement this into WHERE and HAVING clauses

	set $etrap="zshow ""*""  halt"
	;
	set initDone("setQuantifier")=0
	set initDone("arithmeticOperator")=0
	set initDone("comparisonOperators")=0
	set initDone("comparisonOperatorsAlone")=0
	set initDone("booleanOperator")=0
	set initDone("joinTypes")=0
	set initDone("tf")=0
	set initDone("aas")=0
	set GLOBALtotalTables=0
	set existsInHavingExists="FALSE"
	set caseFunctionExists="FALSE"

	set arguments=$zcmdline
	set sqlFile=$piece(arguments," ",2)
	if (sqlFile="") set sqlFile="customers.sql"
	set zwrFile=$piece(arguments," ",3)
	if (zwrFile="") set zwrFile="customers.zwr"
	; The runCount variable determines the amount of .sql files to be generated when this file is ran.
	set runCount=$piece(arguments," ",4)
	if (runCount="") set runCount=1
	; The prefix variable defines the prefix for the .sql files in the form of "prefix###.sql"
	set prefix=$piece(arguments," ",5)
	if (prefix="") set prefix="query"

	do readSQL($$findFile(sqlFile))
	; `data` for `nameslastname` database is filled in directly without parsing a `.zwr` file.
	; This is because Octo's m representation of a single column table is different from a
	; regular table with multiple columns.
	; More details -> https://gitlab.com/YottaDB/DBMS/YDBOcto/-/merge_requests/1071#note_854613974
	if ("postgres-nameslastname.sql"=sqlFile) do fillDefaultNamesLastnameTbl()
	else  do readZWR($$findFile(zwrFile))
	do checkForEmptyTables()
	do initTable()
	do initColumn()
	do initAggrFunc()

	; Choose a max of 4 JOINs. 2 with probability ~ 25%, 3 with probability ~ 12.5%, 4 with probability ~ 6.25%
	; Use the same join count for ALL queries generated in this round.
	set joinCount=$select('$random(2**4):4,'$random(2**3):3,'$random(2**2):2,1:1)
	; With the Northwind data set, 2 or 3 joins in the query with complex subquery invocations have known to cause
	; result rows that are gigabytes long and take a long time to run (greater than 10 minutes)
	; so restrict max joins to 1 in that case.
	set:(sqlFile["northwind")&(1<joinCount) joinCount=1
	; Randomly choose if subqueries will be tested or not (50% yes, 50% no).
	set enableSubQuery=$random(2)
	; Randomly choose if WHERE clause will be present in outermost query or not (87.5% yes, 12.5% no).
	set enableWhereClause=$random(8)
	; Randomly choose if WHERE clause will be present in subquery or not (50% yes, 50% no).
	set enableInnerWhereClause=$random(2)
	; Randomly choose if ORDER BY clause will be present (in outermost query and subquery) or not (50% yes, 50% no).
	set enableOrderByClause=$random(2)
	; Randomly choose if GROUP BY and HAVING clause will be present (in outermost query and subquery) or not (50% yes, 50% no).
	set enableGroupByHavingClause=$random(2)
	; Randomly choose if LIMIT clause will be present (in outermost query and subquery) or not (50% yes, 50% no).
	set enableLimitClause=$select('$random(8):1,1:0)
	zwrite sqlFile,joinCount,enableSubQuery,enableWhereClause,enableInnerWhereClause
	zwrite enableOrderByClause,enableGroupByHavingClause,enableLimitClause
	;Main Loop
	new i,savecolumns
	merge savecolumns=columns
	set file=prefix_0_".sql"
	for i=1:1:runCount do
	. new tableAlias	; refresh the mapping of table names to alias names for each query
	. set aliasNum=0
	. set fromNum=1
	. ; The following variables need to be reset to their default values for each, seperate query.
	. set orderByExists=0,limitExists=0,asteriskExists=0,tableAsteriskExists=0,tableAsteriskAggregateExists=0,outerJoinExists=0,selectNodePosition=0
	. set orderByListLessThanSelectList=0
	. set existsInHavingExists="FALSE",caseFunctionExists="FALSE",outerSelectList=""
	. kill columns			; refresh columns array for next query (remove any temporary tables created in prior query)
	. merge columns=savecolumns
	.
	. ; About 1/5 of queries generated will contain GROUP BY and will also
	. ; match the requirements for having a GROUP BY in the query
	. set allowGBH("alias"_aliasNum)=$select('$random(5):"TRUE",1:"FALSE")
	.
	. set query="",queryDepth=0
	. set query=$$generateQuery(queryDepth,joinCount)
	. write query,!
	.
	. open file:(append)
	. use file
	. ; Add a line to generated query file given LIMIT exists, and ORDER BY does not or is not having all
	. ;   the columns in select in the query.
	. ; This forces the crosscheck function to only count lines as having a LIMIT clause without a correct
	. ; ORDER BY clause can cause different results to be returned by the database
	. if (limitExists&('orderByExists))  write query," ","-- rowcount-only-check",!
	. else  if (limitExists&(orderByListLessThanSelectList))  write query," ","-- rowcount-only-check",!
	. else  if (orderByExists&(asteriskExists))  write query," ","-- sort-needed-check",!
	. else  if (orderByExists&(orderByListLessThanSelectList))  write query," ","--sort-needed-check",!
	. else  write query,!
	. close file
	. ; The following LVNs exist for each individual query,
	. ; thus they need to be KILLED after each query is created
	. kill tableColumn,selectListLVN,subQuerySelectedTables,innerQuerySLLVN,groupByListLVN,groupByColumnLVN,selectedExprGroupBy,groupByComparisonExpressionExists

	quit

; #FUTURE_TODO: Look into using $piece functions instead of $extract and $find for parsers,
;               as it might help to improve the readability of the semi-messy parsers
;Parsers
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This function searches for, and returns, the relative path of the fixture .sql and .zwr files inside of the Octo folder structure.
; It takes in the filename of the desired .sql/.zwr file as a parameter.
findFile(file)
	if ($zsearch(file)="") do
	. set file="../../tests/fixtures/"_file
	quit file

; This function takes in a .sql file, and stores table names, row names, and the variable typing of the rows.
; It takes in a file, or file path, to the .sql file as a parameter.
readSQL(file)
	new line,nlines,count,tableNameStart,tableNameEnd,tableName,columnNameAndTypeStart,columnNameAndTypeEnd,columnName
	new columnType,columnNameAndTypeStart,finalColumnAndTypeEnd,finalColumnName,finalColumnType,columnNameAndType,i,nlines,i2
	open file:(readonly)
	use file
	for  read line($increment(nlines))  quit:$zeof
	close file
	kill line(nlines)  if $increment(nlines,-1) ;kills line that contains EOF

	set i2=1
	for i=1:1:nlines do
	. ; Filters out comment lines (start with '--' or '#'), and/or blank lines
	. ; Also filters out lines that contain the word "EXTRACT"
	. if (($extract(line(i),1)'="#")&($extract(line(i),1)'="")&($extract(line(i),1,2)'="--"))&($find(line(i)," EXTRACT ")=0) do
	. . ;TABLE NAMES
	. . if ($find(line(i),"CREATE TABLE")'=0) do
	. . . set tableNameStart=$find(line(i),"CREATE TABLE ")
	. . . set tableNameEnd=$find(line(i)," ",tableNameStart)
	. . . set tableName=$extract(line(i),tableNameStart,tableNameEnd-2)
	. . . if ($extract(tableName,0,1)="`")  set tableName=$extract(tableName,2,$length(tableName)-1)
	. . . if $increment(GLOBALtotalTables)
	. .
	. . ;set i2=1
	. . ;COLUMN NAMES
	. . if (($find(line(i),"INTEGER")'=0)!($find(line(i),"VARCHAR")'=0)!($find(line(i),"BOOLEAN")'=0)) do
	. . . for  do  quit:(columnNameAndTypeEnd=0)
	. . . . set columnNameAndTypeStart=$find(line(i),"",tableNameEnd)
	. . . . set columnNameAndTypeEnd=$find(line(i),",",tableNameEnd)
	. . . . if ($extract(line(i),columnNameAndTypeStart,columnNameAndTypeEnd)'="") do
	. . . . . ; shifted columnNameStart over 1 to remove leading " " and "(" shifted columnNameEnd over 2 to remove trailing ", "
	. . . . . set columnNameAndType=$extract(line(i),columnNameAndTypeStart+1,columnNameAndTypeEnd-2)
	. . . . . set columnName=$piece(columnNameAndType," ")
	. . . . . set columnType=$piece(columnNameAndType," ",2)
	. . . . . set sqlInfo(tableName,i2,columnName,columnType)=""
	. . . . set tableNameEnd=columnNameAndTypeEnd
	. . . . if $increment(i2)
	. . . if $increment(i2,-1) ; i2 needs to be 1 less at this point, as the above loop runs until a blank line is found
	. . . if ($find(line(i),";")'=0)  do
	. . . . ; shifted over 1 to remove trailing ")"
	. . . . set finalColumnAndTypeEnd=$find(line(i),")",columnNameAndTypeStart-1)
	. . . . ; shifted columnNameAndTypeStart over 1 to remove leading " " shifted finalColumnEnd over 2 to remove trailing ") "
	. . . . set finalColumnAndType=$extract(line(i),columnNameAndTypeStart+1,finalColumnAndTypeEnd-2)
	. . . . set finalColumnName=$piece(finalColumnAndType," ")
	. . . . set finalColumnType=$piece(finalColumnAndType," ",2)
	. . . . set sqlInfo(tableName,i2,finalColumnName,finalColumnType)=""
	. . . . ; if there is no semicolon in line(i) reset i2 to be used again, else keep i2 the same
	. . . . if ($find(line(i),";",finalColumnAndTypeEnd)'=0)  set i2=1
	. set tableNameEnd=0
	quit

; This function takes in the a .zwr file, and stores the data contained within the tables/rows relative to their table name and row name.
; It takes in a file, or file path, to the .zwr file as a parameter.
readZWR(file)
	new line,nlines,holder,firstData,table,i,i2,previous
	open file:(readonly)
	use file
		for  read line($increment(nlines))  quit:$zeof
	close file
	kill line(nlines)  if $increment(nlines,-1)

	set innerNumber=1
	set previous=""
	for i=1:1:nlines do
	. if ($extract(line(i),1)="^") do
	. . set line(i)=$zextract(line(i),1,$zlength(line(i))-1)	; remove trailing double quote
	. . for i2=1:1 do  quit:(holder="")
	. . . set firstData=""
	. . . if (i2=1) do
	. . . . set fullThing=$piece(line(i),"=""") ; EX: ^Names(###)
	. . . . if ($extract($piece(fullThing,"("),2,1000000)'=previous)  set innerNumber=1
	. . . . set previous=$extract($piece(fullThing,"("),2,1000000)
	. . . . set table=$piece(fullThing,"(")_"("_innerNumber_")" ; EX: ^Names(custom number)
	. . . . if $increment(innerNumber)
	. . . . set firstData=$piece($piece(line(i),"=""",2),"|")
	. . . . set data(table,i2,$piece($piece(fullThing,"(",2),")"))=""
	. . . . set data(table,i2+1,firstData)=""
	. . . set holder=" "
	. . . if (i2'=1) do
	. . . . set holder=$piece(line(i),"|",i2)
	. . . . if (holder'="") set data(table,i2+1,holder)=""
	. . . set pKey=$extract(line(i),$find(line(i),"("),$find(line(i),")")-2)
	. . . set primaryKeys($extract(table,0,$find(table,"(")-2),pKey)=""
	quit

fillDefaultNamesLastnameTbl()
	set data("^nameslastname(1)",1,"Cool")=""
	set data("^nameslastname(2)",1,"Burn")=""
	set data("^nameslastname(3)",1,"Killer")=""
	set data("^nameslastname(4)",1,"Nikon")=""
	set data("^nameslastname(5)",1,"")=""
	set data("^nameslastname(6)",1,"Cool")=""
	quit

checkForEmptyTables()
	; If a table "exists" but has no data (such as namesWithAges in names db),
	; then this function will remove it
	new holder,one,two

	set holder=" "
	for  do  quit:(holder="")
	. set holder=$order(sqlInfo(holder))
	. if (holder'="")  set sqlPart(holder)=""

	set holder=" "
	for  do  quit:(holder="")
	. set holder=$order(data(holder))
	. if (holder'="")  set zwrPart($extract(holder,2,$find(holder,"(")-2))=""

	set one=" "
	set two=" "
	for  do  quit:(one="")
	. set one=$order(zwrPart(one))
	. for  do  quit:(two="")
	. . set two=$order(sqlPart(two))
	. . if ($data(sqlPart(one))'=$data(zwrPart(two))&(two'="")&(one'=""))  kill sqlPart(two),sqlInfo(two)  if $increment(GLOBALtotalTables,-1)

	quit

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Grammar Rules
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; https://ronsavage.github.io/SQL/sql-92.bnf.html#set%20quantifier
; This function returns a randomly selected quantifer for either the parent query, or any subsequent subquery.
setQuantifier(curDepth)
	if (initDone("setQuantifier")=0) do
	. set initDone("setQuantifier")=1
	. set quantifier=-1
	. set quantifier($increment(quantifier))="DISTINCT "
	. set quantifier($increment(quantifier))="ALL "
	. set quantifier($increment(quantifier))=""
	. if $increment(quantifier)
	set actualQuantifier=quantifier($random(quantifier))
	set quantifierLVN("alias"_aliasNum)=actualQuantifier
	quit actualQuantifier

; https://ronsavage.github.io/SQL/sql-92.bnf.html#select%20list
; This function returns a select list only for the parent query.
; Currently it can only contain table.column, subquery, and case functions.
; It takes in a curDepth value, since it is recursive.
; This value is continously lowered until it reaches 0, thus ending the recursive loop.
; It stores every value added into the SELECT LIST into a LVN called selectListLVN.
selectList(queryDepth,curDepth)
	new randInt,result,toBeAdded,okToSelectStar,constExpr
	set toBeAdded=""
	; This function serves the same purpose as select sublist in the grammar rules for SQL.
	; Choose "*" in the select column list 12.5% of the time AND if no other columns have been already added to it.
	; Also, until YDBOcto#246 is fixed, cannot use * if SELECT DISTINCT has been chosen already and joinCount is > 1, also
	; cannot use multiple occurence of * if SELECT DISTINCT has been chosen already or joinCount is > 0 as the sum of the number of columns of the tables involved
	; in the FROM and JOIN clauses could end up greater than 31 and cause MAXNRSUBSCRIPTS error. Disable "*" selection in that case.
	write "selectList(): queryDepth:",queryDepth," curDepth:",curDepth,!
	set okToSelectStar=((quantifierLVN("alias"_queryDepth)'="DISTINCT ")!(2>joinCount))
	set okToSelectMultipleStar=((1>joinCount)!(quantifierLVN("alias"_queryDepth)'="DISTINCT "))
	if ((okToSelectStar&('$random(8)))&((asteriskExists=1)&oktoSelectMultipleStar)) do
	. set toBeAdded="*"
	. set asteriskExists=1
	. set selectListLVN(queryDepth,toBeAdded)="star"
	. if ($increment(selectNodePosition))
	. set selectListLVN(queryDepth,toBeAdded,1)=selectNodePosition
	. ; Disallow GROUP BY and HAVING if * is in select column list as it gets complicated
	. set allowGBH("alias"_queryDepth)="FALSE"

	if (toBeAdded'="*") do
	. ; Choose DerivedColumn or Qualifier
	. set randInt=$random(100)
	. ; #FUTURE_TODO: Allow randInt=5 possibility when $$returnCaseFunction infinite-loop/malformed-query issues are resolved
	. ; For now, choose randInt=0 70% of time, randInt=1 10% of time, randInt=2 10% of time, randInt=3 10% of time
	. ; If enableSubQuery is FALSE, do not choose randInt=1
	. set okToSelectTableStarOrderBy=(quantifierLVN("alias"_queryDepth)'="DISTINCT ")
	. set:'okToSelectTableStarOrderBy randInt=$select(('enableSubQuery!(80>randInt)):0,1:1)
	. set:okToSelectTableStarOrderBy randInt=$select(('enableSubQuery!(70>randInt)):0,((80>randInt)&(70<randInt)):3,((90>randInt)&(80<randInt)):2,1:1)
	else  set randInt=-1

	; To avoid ambiquity warnings, this is commented out
	; Regular column notation is to be used
	;if randInt=0 set toBeAdded=$$chooseColumn("")

	; Below check is to ensure that if there is only 1 table, that there can be no subquerying
	if (GLOBALtotalTables=1) set randInt=0

	write "selectList(): randInt=",randInt,!
	; Qualifier notation (table.column) is to be used
	if (randInt=0) do
	. set table=$$chooseTableFromTableColumn()
	. set toBeAdded=table_"."_$$chooseColumn(table)
	. set selectListLVN(queryDepth,toBeAdded)="table.column"
	. if ($increment(selectNodePosition))
	. set selectListLVN(queryDepth,toBeAdded,1)=selectNodePosition

	if (randInt=1) do
	. set aliasNum1More=aliasNum+1
	. set alias="alias"_aliasNum1More
	. set toBeAdded="("_$$generateSubQuery(queryDepth,"limited","")_") AS "_alias
	. set selectListLVN(queryDepth,alias)="subquery"
	. if ($increment(selectNodePosition))
	. set selectListLVN(queryDepth,alias,1)=selectNodePosition

	; Qualifier notation (table.*) to be used
	if (randInt=2) do
	. set table=$$chooseTableFromTableColumn()
	. set toBeAdded=table_".*"
	. set selectListLVN(queryDepth,toBeAdded)="table.*"
	. ; Disallow GROUP BY when table.* is selected
	. ; Postgres generates error when table.* is present in both group by and select column list.
	. ; Because of this reason we will not be able to enable this case as we compare the result with postgres.
	. ; More details -> https://gitlab.com/YottaDB/DBMS/YDBOcto/-/merge_requests/816#note_550782933
	. set allowGBH("alias"_queryDepth)="FALSE"
	. set tableAsteriskExists=1
	. if ($increment(selectNodePosition))
	. set selectListLVN(queryDepth,toBeAdded,1)=selectNodePosition

	if (randInt=3) do
	. new tmpRand
	. set tmpRand=$random(3)
	. if (0=tmpRand) do
	. . ; Binary expression
	. . set table=$$chooseTableFromTableColumn()
	. . set toBeAdded=$$returnBinaryExpression(queryDepth,table,.constExpr)
	. . write "selectList(): Is constExpr: "_constExpr_" BinaryExpression - "_toBeAdded,!
	. . set selectListLVN(queryDepth,toBeAdded)="binary_expression"
	. else  if (1=tmpRand) do
	. . ; Unary expression
	. . ; - simple unary expression
	. . set table=$$chooseTableFromTableColumn()
	. . set toBeAdded=$$returnUnaryExpression(queryDepth,table,.constExpr)
	. . set selectListLVN(queryDepth,toBeAdded)="unary_expression"
	. . write "selectList(): Is constExpr: "_constExpr_" UnaryExpression - "_toBeAdded,!
	. else  do
	. . ; coalesce/greatest/least/nullif
	. . new op,leftOpnd,rightOpnd
	. . new column,tblCol,columnTypes,expType
	. . set tmpRand=$random(4)
	. . set constExpr=0
	. . set op=$select((0=tmpRand):"GREATEST",(1=tmpRand):"LEAST",(2=tmpRand):"COALESCE",(3=tmpRand):"NULLIF")
	. . ; get a column
	. . set table=$$chooseTableFromTableColumn()
	. . set columnTypes=$$getTableColumnTypes(table)
	. . set num=$random(2)
	. . if (columnTypes="VARCHAR") set expType="VARCHAR"
	. . else  if (columnTypes="INTEGER") set expType="INTEGER"
	. . else  if (columnTypes="BOTH") set expType=$select((num=0):"VARCHAR",(num=1):"INTEGER")
	. . if "NONE"'=columnTypes do
	. . . new secondColumn,lclGroupByColumnLVN
	. . . ; get first column
	. . . set column=table_"."_$$getColumnOfType(table,expType)
	. . . ; get second column
	. . . set secondColumn=table_"."_$$getColumnOfType(table,expType)
	. . . ; assign left and right operand
	. . . set:(num) leftOpnd=secondColumn,rightOpnd=column
	. . . set:('num) rightOpnd=secondColumn,leftOpnd=column
	. . . set toBeAdded=op_"("_leftOpnd_","_rightOpnd_")"
	. . . if ($increment(lclGroupByColumnLVN(queryDepth,column)))
	. . . if ($increment(lclGroupByColumnLVN(queryDepth,secondColumn)))
	. . . set isVariableGroupBy=1
	. . . new innerHolder
	. . . set innerHolder="" for  set innerHolder=$order(lclGroupByColumnLVN(queryDepth,innerHolder))  quit:innerHolder=""  do
	. . . . if ($increment(groupByColumnLVN(queryDepth,toBeAdded,innerHolder)))
	. . . if ($DATA(groupByColumnLVN)) zwr groupByColumnLVN
	. . . kill lclGroupByColumnLVN
	. . else  do
	. . . set constExpr=1
	. . . if (0=num) do
	. . . . ; - constant expressions (INT)
	. . . . set leftOpnd=$$getRandInt,rightOpnd=$$getRandInt
	. . . else  if (1=num) do
	. . . . ; - constant expressions (String)
	. . . . set leftOpnd=$$getRandStr,rightOpnd=$$getRandStr
	. . . set toBeAdded=op_"("_leftOpnd_","_rightOpnd_")"
	. . set groupByComparisonExpressionExists(queryDepth,toBeAdded)=0
	. . set selectListLVN(queryDepth,toBeAdded)="conditional_expression"
	. . if ($increment(groupByListLVN(queryDepth,toBeAdded)))
	. . write "selectList(): "_toBeAdded,!
	. if ($increment(selectNodePosition))
	. set selectListLVN(queryDepth,toBeAdded,1)=selectNodePosition

	if (randInt=5) do
	. set toCompare=$random(4)+1
	. set toBeAdded=$$returnCaseFunction("SELECT LIST","randomNumbers","numbers","FALSE",toCompare)
	. set selectListLVN(queryDepth,"alias"_aliasNum)="case_statement"


	if (randInt=3) do
	. new typeStr
	. if ("binary_expression"=selectListLVN(queryDepth,toBeAdded)) set typeStr="binary"
	. else  if ("unary_expression"=selectListLVN(queryDepth,toBeAdded)) set typeStr="unary"
	. else  set typeStr="conditional"
	. ; If GroupBy exists atall we need the binary and unary expressions in it if they are not constants.
	. ; If constant then they may or may not be in GroupBy.
	. ; `constExpr` value indicates whether the expression is constant or not.
	. ; Change the selectListLVN value to `constant_binary_expression` or `constant_unary_expression`
	. ;   when the expression is constant so that groupByClause can identify and ignore it 50% of the time.
	. set:(constExpr) selectListLVN(queryDepth,toBeAdded)=typeStr_"_constant_expression"
	. write "selectList(): expression type - "_selectListLVN(queryDepth,toBeAdded),!

	; #FUTURE_TODO: Remove `((3'=randInt)!(constExpr))`
	;   If aggregate function element gets added to select list from the below block
	;   all columns in the list need to be included in GroupBy. With expressions this is difficult as mainly its randomely formed
	;   and we are not storing any information about the columns used in the expression at this time.
	;   When expression creation is enhanced to store info on columns used in them we can implement this TODO.
	if (((3'=randInt)!(constExpr))&(enableGroupByHavingClause&(allowGBH("alias"_queryDepth)="TRUE")))  do
	. new table,chosenColumn,agg,chosenColumn2,tc
	. ; #FUTURE_TODO: Remove following line when issues (both in Octo and in test_helpers)
	. ;               pertaining to aggregate functions are resolved
	. set curDepth=0
	. set table=$$chooseTableFromTableColumn()
	. if (1=$random(2))  do
	. . ; choose count(table.*) for select list
	. . ; Note: When count(table.*) is present other columns either have to be in group by or be also used in aggregate function
	. . set agg="COUNT("_table_".*)"
	. . set selectListLVN(queryDepth,agg)="aggregate_function_t_asterisk"
	. . set toBeAdded=toBeAdded_","_agg
	. . set tableAsteriskAggregateExists=1
	. . if ($increment(selectNodePosition))
	. . set selectListLVN(queryDepth,agg,1)=selectNodePosition
	. else  do
	. . set chosenColumn=$$chooseColumn(table)
	. . ; #FUTURE_TODO: Add more than one column in SELECT column list that uses aggregate functions
	. . set agg=$$returnAggregateFunction(queryDepth,table,chosenColumn)
	. . ; Fix rare query failure where having `AVG()` results in numeric precision differences between Octo and Postgres
	. . ; Refer description of https://gitlab.com/YottaDB/DBMS/YDBOcto/-/merge_requests/829
	. . set:(enableOrderByClause&(agg["AVG")) agg="ROUND("_agg_",10)"
	. . set selectListLVN(queryDepth,agg)="aggregate_function"
	. . if ($increment(selectNodePosition))
	. . set selectListLVN(queryDepth,agg,1)=selectNodePosition
	. . new i
	. . for i=1:1:8 set chosenColumn2=$$chooseColumn(table) quit:(chosenColumn'=chosenColumn2)
	. . set tc=$select((8'=i):table_"."_chosenColumn2,1:"")
	. . set toBeAdded=toBeAdded_", "_agg_$select(""'=tc:", "_tc,1:"")
	. . if (""'=tc) do
	. . . set selectListLVN(queryDepth,tc)="table.column"
	. . . if ($increment(selectNodePosition))
	. . . set selectListLVN(queryDepth,tc,1)=selectNodePosition

	set result=toBeAdded
	if curDepth>0 do
	. if $increment(curDepth,-1) ; to drop down a "level" in depth
	. if (curDepth'=0) set result=result_", "_$$selectList(queryDepth,curDepth)

	write "selectList() : result=",result,!
	quit result

; https://ronsavage.github.io/SQL/sql-92.bnf.html#table%20expression
; This function controls whether or not to add various other portions of a query into the parent query.
; This includes WHERE, GROUP BY, HAVING, ORDER BY, and LIMIT.
tableExpression(queryDepth)
	new result
	; From Clause should go here, but it needs to be decided on early as to
	; allow for proper column(s) to be chosen
	set result=""

	; We do not rely on values previously added to innerQuerySLLVN at this point. But ignoring it results in
	; innerSelectList() call returning empty list in case of single column table usage.
	; This is because innerSelectList() utilizes innerQuerySLLVN to determine if another column needs to be added.
	; When previous subquery's values are carried over `innerQuerySLLVN` will not be co-herent with the actual list.
	; To prevent this we use a new variable. As the code from this point only rely on selectListLVN
	; and the table itself to select columns, using a new innerQuerySLLVN is safe and doesn't reduce the query variations.
	new innerQuerySLLVN

	write "tableExpression(): queryDepth=",queryDepth,!
	set:enableWhereClause result=result_$$whereClause(queryDepth)
	if (enableGroupByHavingClause&(allowGBH("alias"_queryDepth)="TRUE"))  do
	. set result=result_$$groupbyClause(queryDepth)
	. set result=result_$$havingClause(queryDepth,"query")
	if enableOrderByClause do
	. new orderByClause
	. set orderByClause=$$orderbyClause(queryDepth,0,"QUERY")
	. set result=result_orderByClause
	. set:""'=orderByClause orderByExists=1
	if enableLimitClause do
	. new limitClause
	. set limitClause=$$limitClause
	. set result=result_limitClause
	. set:""'=limitClause limitExists=1

	quit result

; https://ronsavage.github.io/SQL/sql-92.bnf.html#from%20clause
; #FUTURE_TODO: Implement subqueries into FROM clause
; This function returns the fromClause for the parent query. Currently it only supports a single table to be pulled from, but contains code that should enable multiple tables to be pulled from at a time.
fromClause()
	new result,randInt,i,x,chosenTable

	set result="FROM "

	; This is much harder than first thought, a variety of different issues
	; #FUTURE_TODO: This works from a functionality standpoint, the issue is that when a JOIN clause
	;               is also in the query, the tables in the fromClause overlap with those in the JOIN
	;               The tables in the fromClause are the same as those that are to be joined, which
	;               Octo and PostgreSQL both do not like. This can be resolved using aliases.
	;               This TODO requires a lot more work than previously thought, a large number of
	;               other functions will need to be rewritten in order to handle having a list of FROMS.
	;               Going into the future, these changes will likely need to be done in a separate merge
	;               request, as it appears to need a lot of changes in order to get functioning properly.
	;               Example: SELECT * FROM customers, orders RIGHT JOIN orders on (order_id=customer_id)
	;set randInt=$random(GLOBALtotalTables)+1
	;set fromTableList=""
	;set toBeAdded=""
	;for i=1:1:randInt do
	;. for  quit:($find(fromTableList,toBeAdded)=0)  set toBeAdded=$$chooseTable
	;. ;set toBeAdded=$$chooseTable
	;. ;if toBeAdded not in tableColumn
	;. ;if ($find(fromTableList,toBeAdded)=0) do
	;. set fromTableList=fromTableList_toBeAdded
	;. if (i>1) do
	;. . set fromTableList=fromTableList_" AS alias"_aliasNum
	;. ; Isolate last call to exclude comma without additional string operations
	;. if (randInt>1) do
	;. . set fromTableList=fromTableList_", "
	;. set tableColumn(toBeAdded)=""
	;. set randInt=$increment(randInt,-1)

	set chosenTable=$$chooseTable
	set tableColumn(chosenTable)="",tableAlias(chosenTable)=chosenTable
	write "fromClause() : set tableAlias(",chosenTable,")=",tableAlias(chosenTable),!

	; Following two commented out lines, and the comments at the end of the quit statement
	; are intended to support the eventual change to have a list of tables to be selected from.
	;if $increment(fromNum)
	;set tableColumn("f"_fromNum)=chosenTable

	quit result_chosenTable ;_"f"_fromNum ;chosenTable OR fromTableList

; https://ronsavage.github.io/SQL/sql-92.bnf.html#where%20clause
; This function returns the WHERE clause, which contains the WHERE statement as well as a randomly selected condition type.
whereClause(queryDepth)
	do assert(enableWhereClause)
	new result,randInt,i,x
	set result=" WHERE "
	; #FUTURE_TODO: Increase below to 17 when $$returnCaseFunction infinite loop and malformed query issues are resolved
	set randInt=$random(15) ; 0-14 ; Increase this range as new versions of WHERE clauses are added

	set table=$piece(fc," ",2)

	; When randInt=1 or 3 or 4 a INTEGER type column is necessary in the selected table,
	; this code block ensures that this requirement is satisfied, and if it isn't
	; then set randInt to a different value.
	; WHERE clause type 1 is integer function expression
	; WHERE clause type 3 is integer comparison expression
	; WHERE clause type 4 is arithmetic operation based comparison expression
	if ((randInt=1)!(randInt=3)!(randInt=4)) do checkAndChangeCaseIfColumnTypeAbsent(table,"INTEGER",.randInt)

	; When randInt=2 or 8 a VARCHAR type column is necessary in the selected table,
	; this code block ensures that this requirement is satisfied, and if it isn't
	; then set randInt to a different value.
	; WHERE clause type 2 is string concatenation, which requires at least one VARCHAR in order to function
	; WHERE clause type 8 is LIKE with wildcards, this comparison can only occur on VARCHARs, not numeric/integer/boolean
	if ((randInt=2)!(randInt=8)) do checkAndChangeCaseIfColumnTypeAbsent(table,"VARCHAR",.randInt)

	set type=""

	; Don't use a random generator that requires a BOOLEAN type column if it's not present
	; WHERE clause type 10 is boolean-type-column
	; WHERE clause type 11-14 are boolean operations
	if ((10<=randInt)&(randInt<=14)) do
	. set x="sqlInfo("""_table_""")"
	. for i=1:1:15  do  quit:(($find(type,"BOOLEAN")'=0)!(x=""))
	. . set x=$query(@x)
	. . if (x'="")  set type=$qsubscript(x,4)
	. . if ((x'="")&($qsubscript(x,1)'=table))  set randInt=0
	. set:((i=15)!(x="")) randInt=0

	; If enableSubQuery is FALSE, randInt=5 and randInt=9 cannot be allowed as they invoke "generateSubQuery"
	; So in those cases, set randInt=0 instead.
	set:('enableSubQuery)&((randInt=5)!(randInt=9)) randInt=0

	write "whereClause(): queryDepth=",queryDepth," randInt=",randInt,!
	if (randInt=0) do
	. new loopCount,i,leftSide,rightSide,chosenColumn,opened
	. set loopCount=$random(3)+1 ; 1-3
	. set opened="FALSE"
	. for i=1:1:loopCount do
	. . new type
	. . set chosenColumn=$$chooseColumn(table)
	. . set type=$$returnColumnType(table,chosenColumn)
	. . set leftSide=""
	. . set rightSide=""
	. . if $random(2)  set leftSide=table_"."_chosenColumn
	. . else  set leftSide=$$chooseEntry(table,chosenColumn)
	. . if $random(2)  set rightSide=table_"."_chosenColumn
	. . else  set rightSide=$$chooseEntry(table,chosenColumn)
	. . set leftSide=$$getRandFunc(leftSide,type)
	. . set rightSide=$$getRandFunc(rightSide,type)
	. .
	. . ; First portion of WHERE, no AND or OR
	. . if (i=1) set result=result_"("_$$notString_$$comparisonOperators(leftSide,rightSide)
	. . ; Following portions of WHERE, with AND or OR separators
	. . if (i'=1) do
	. . . set result=result_" "_$$booleanOperator_" "
	. . . if (($random(2))&(opened="FALSE"))  do
	. . . . set result=result_"("
	. . . . set opened="TRUE"
	. . . set result=result_$$notString_$$comparisonOperators(leftSide,rightSide)
	. . . if (($random(2))&(opened="TRUE"))  do
	. . . . set result=result_")"
	. . . . set opened="FALSE"
	. . if (i=loopCount)  set result=result_")"
	. . if ((i=loopCount)&(opened="TRUE"))  set result=result_")"

	if (randInt=1) do
	. new condition,chosenColumn,plusMinus,i
	. set chosenColumn=$$getColumnOfType(table,"INTEGER")
	. set plusMinus="+"
	. if $random(2) set plusMinus="-"
	. set result=result_"(("_$$getRandFuncInt(table_"."_chosenColumn)
	. set result=result_plusMinus_$$chooseEntry(table,chosenColumn)_")="_$$getRandFuncInt($$chooseEntry(table,chosenColumn))_")"

	if (randInt=2) do
	. ; Left side of this expression will always be forced to be a varchar,
	. ; the right side of this expression can be either varchar or integer
	. ; This is done as PostgreSQL requires at least one side of a string concatenation
	. ; to be a string/varchar and the other to not be.
	. new leftSide,rightSide,chosenColumn1,chosenColumn2,entry1,entry2,i
	.
	. set chosenColumn1=$$getColumnOfType(table,"VARCHAR")
	. set entry1=$$chooseEntry(table,chosenColumn1)
	. set entry1=$extract(entry1,2,$length(entry1)-1)
	. set leftSide=table_"."_chosenColumn1
	. if $random(2)  set leftSide=$$chooseEntry(table,chosenColumn1)
	. else  set leftSide=$$getRandFuncStr(leftSide)
	.
	. set chosenColumn2=$$chooseColumn(table)
	. set entry2=$$chooseEntry(table,chosenColumn2)
	. set entry2=$extract(entry2,2,$length(entry2)-1)
	. set rightSide=table_"."_chosenColumn2
	. if $random(2)  set rightSide=$$chooseEntry(table,chosenColumn2)
	. else  do
	. . set type=$$returnColumnType(table,chosenColumn2)
	. . set rightSide=$$getRandFunc(rightSide,type)
	.
	. set result=result_$$comparisonOperators("("_leftSide_"||"_rightSide_")","'"_entry1_entry2_"'")

	if (randInt=3) do
	. new chosenColumn,plusMinus,plusMinus2,i
	. set chosenColumn=$$getColumnOfType(table,"INTEGER")
	. set plusMinus="+"
	. if $random(2) set plusMinus="-"
	. set plusMinus2="+"
	. if $random(2) set plusMinus2="-"
	. set result=result_$$comparisonOperators(table_"."_chosenColumn,plusMinus_"("_plusMinus2_$$chooseEntry(table,chosenColumn)_")")

	if (randInt=4) do
	. new chosenColumn,beginning,plusMinus,end,aOperator,i
	. set chosenColumn=$$getColumnOfType(table,"INTEGER")
	. set beginning="("_$$chooseEntry(table,chosenColumn)_")"
	. new plusMinus
	. set plusMinus="+"
	. if $random(2) set plusMinus="-"
	. set endEntry=$$chooseEntry(table,chosenColumn)
	. ; Checks if the selected operator is division/modulo and then checks if the
	. ; divisor is 0, if it is 0, it then forces it to a non zero number (1)
	. set aOperator=$$arithmeticOperator
	. if (((aOperator="/")!(aOperator="%"))&(endEntry=0))  set endEntry=1
	.
	. set end=plusMinus_"("_plusMinus_endEntry_")"
	.
	. set result=result_$$comparisonOperators("("_beginning_" "_aOperator_" "_end_")",table_"."_chosenColumn)

	if (randInt=5) do
	. new alias
	. set result=result_$$notString_"EXISTS ("_$$generateSubQuery(queryDepth,"full","")_")"

	if (randInt=6) do
	. new chosenColumn,notString,entryList,i
	. set chosenColumn=$$chooseColumn(table)
	. set entryList=""
	. set limit=($random($$maxIndex(table)-1)+1)
	. if (limit>8)  set limit=8
	. for i=1:1:limit do
	. . set entryList=entryList_$$chooseEntry(table,chosenColumn)_", "
	. ; strip the ", " off of entryList
	. set entryList=$extract(entryList,0,$length(entryList)-2)
	. set notString=$select($random(2):"NOT ",1:"")	; cannot use $$notString here because "NOT NOT IN" is not valid syntax
	. set result=result_table_"."_chosenColumn_" "_notString_"IN ("_entryList_")"

	if (randInt=7) do
	. new chosenColumn
	. set chosenColumn=$$chooseColumn(table)
	. set result=result_table_"."_chosenColumn_" BETWEEN "_$$chooseEntry(table,chosenColumn)_" AND "_$$chooseEntry(table,chosenColumn)

	if (randInt=8) do
	. new chosenColumn,string,i,randInt,i
	. ; Forces column to be of type "VARCHAR" as to do a string comparison
	. set chosenColumn=$$getColumnOfType(table,"VARCHAR")
	. set string=""
	. set desiredStringLength=$random(7)+1 ; 1-7
	. for i=1:1:desiredStringLength do
	. . set randInt=$random(9) ; 0-8
	. . if ((randInt=0)!(randInt=1)) set string=string_$char($random(27)+64) ; @,A-Z
	. . if ((randInt=2)!(randInt=3)) set string=string_$char($random(26)+97) ; a-z
	. . if (randInt=4) set string=string_$random(10) ; 0-9
	. . if (randInt=5) set string=string_"#"
	. . if (randInt=6) set string=string_"$"
	. . if (randInt=7) set string=string_"%"
	. . if (randInt=8) set string=string_"_"
	. set result=result_table_"."_chosenColumn_" LIKE '"_string_"'"

	if (randInt=9) do
	. new randInt,word,leftSide,rightSide,chosenColumn,entryList,limit,rightType,i,aliasNum1More
	. set word=$$anyAllSome
	. set leftSide=$$chooseColumn(table)
	. set leftType=$$returnColumnType(table,leftSide)
	. set aliasNum1More=aliasNum+1
	. set rightSide="("_$$generateSubQuery(queryDepth,"limited","")_")"
	. set aliasDotColumn=""
	. for i=1:1  do  quit:($find(aliasDotColumn,"alias"_aliasNum1More)'=0)  do assert(i<16)
	. . set aliasDotColumn=$piece(rightSide," ",i)
	. set rightColumn=$piece($piece(aliasDotColumn,".",2),",",1)
	.
	. set holder=""
	. for i=1:1  do  quit:($find(holder,"FROM")'=0)  do assert(i<16)
	. . set holder=$piece(rightSide," ",i)
	. set rightTable=$piece(rightSide," ",i+1)
	.
	. set rightType=$$returnColumnType(rightTable,rightColumn)
	.
	. for i=1:1:15  do  quit:(leftType=rightType)
	. . set leftSide=$$chooseColumn(table)
	. . set leftType=$$returnColumnType(table,leftSide)
	.
	. set leftSide=table_"."_leftSide
	.
	. if (i'=15) do
	. . set result=result_$$comparisonOperators(leftSide,word_" "_rightSide)
	. else  set result=""

	; When randInt=10 a BOOLEAN type column is necessary in the selected table,
	; this code block ensures that this requirement is satisfied, and if it isn't
	; then set randInt to a different value.
	; WHERE clause type 10 is just "WHERE boolean-type-column"
	if (randInt=10)  do
	. set x="sqlInfo("""_table_""")"
	. set result=result_table_"."_$$getColumnOfType(table,"BOOLEAN")

	if ((11<=randInt)&(randInt<=13)) do
	. set x="sqlInfo("""_table_""")"
	. set chosenColumn=table_"."_$$getColumnOfType(table,"BOOLEAN")
	. set:11=randInt result=result_$$notString_chosenColumn
	. set:12=randInt result=result_chosenColumn_" = TRUE"
	. set:13=randInt result=result_chosenColumn_" = FALSE"

	; Boolean expressions can be combined to create more complex Booleans
	; Example: ((id = 1) OR (firstname = 'Zero')) AND (lastname '= 'Cool')
	; #FUTURE_TODO: Maybe combine this with WHERE clause version #0 (randInt=0)
	if (randInt=14) do
	. new leftSide,rightSide
	. set chosenColumn=$$chooseColumn(table)
	. set leftSide=""
	. set rightSide=""
	. if ($random(2))  set leftSide=table_"."_chosenColumn
	. else  set leftSide=$$chooseEntry(table,chosenColumn)
	. if ($random(2))  set rightSide=table_"."_chosenColumn
	. else  set rightSide=$$chooseEntry(table,chosenColumn)
	. set result=result_"("_$$comparisonOperators(leftSide,rightSide)_" = "_$$tf_")"

	; #FUTURE_TODO: This sometimes generates invalid queries. Skip it for now.
	if (randInt=999) do
	. new toCompare
	. set toCompare=$random(4)+1
	. set result=result_toCompare_" = "_$$returnCaseFunction("WHERE","randomNumbers","numbers","FALSE",toCompare)

	; #FUTURE_TODO: This sometimes generates invalid queries. Skip it for now.
	if (randInt=999) do
	. new leftCaseArg,rightCaseArg,i,toCompare
	. set leftColumn=$$chooseColumn(table)
	.
	. set leftType=$$returnColumnType(table,leftColumn)
	. for i=1:1:15  do  quit:(leftType=rightType)
	. . set rightColumn=$$chooseColumn(table)
	. . set rightType=$$returnColumnType(table,rightColumn)
	.
	. set leftCaseArg=table_"."_leftColumn
	. if ($random(2))  set leftCaseArg=$$chooseEntry(table,leftColumn)
	. set rightCaseArg=table_"."_rightColumn
	. if ($random(2))  set rightCaseArg=$$chooseEntry(table,rightColumn)
	.
	. if (i'=15) do
	. . ; #FUTURE_TODO: The following variable "r" is magic, it is not populated, but when
	. . ;               the code is ran it contains a table.column, this needs more looking into
	. . ;               but for the time being it works, and it works properly so it will be left alone
	. . set toCompare=table_"."_$$chooseColumn(table)
	. . set result=result_toCompare_" = "_$$returnCaseFunction("WHERE","lrComparison","columns","FALSE",toCompare)
	. else  set result=""

	write "whereClause(): result=",result,!
	quit result

; https://ronsavage.github.io/SQL/sql-92.bnf.html#group%20by%20clause
; This function returns the GROUP BY clause. It pulls the necessary values from the selectListLVN.
groupbyClause(queryDepth)
	do assert(enableGroupByHavingClause)
	new result,randInt,i,holder,firstholder,anotherholder,anotherholderType,chooseSelectNumber
	set result=""
	set chooseSelectNumber=0
	zwrite tableAlias,tableColumn,selectListLVN

	set firstholder=1
	new i,j,selectNodeNumber
	for j=queryDepth:-1:0 do
	. set selectNodeNumber=0
	. set holder=""
	. for i=1:1 set holder=$order(selectListLVN(j,holder)) quit:(holder="")  do  do assert(i<16)
	. . set anotherholder=""
	. . ; Skip columns in select list with "*" OR aggregate function OR subquery.
	. . write "groupbyClause() : selectListLVN(",j,",",$zwrite(holder),")=",selectListLVN(j,holder),!
	. . quit:selectListLVN(j,holder)="aggregate_function"
	. . quit:selectListLVN(j,holder)="subquery"
	. . quit:selectListLVN(j,holder)="star"
	. . ; table.* must be skipped as adding this to group by when select list hasn't enclosed table.* with COUNT is an invalid case
	. . quit:selectListLVN(j,holder)="table.*"
	. . ; Skip binary/unary constant expressions 50% of the time
	. . quit:((selectListLVN(j,holder)["_constant_expression")&(1=$random(2)))
	. . if (selectListLVN(j,holder)="aggregate_function_t_asterisk")  do
	. . . ; Using anotherholder here because replacing holder itself will cause error
	. . . ; as selectListLVN(j,holder) will not be defined.
	. . . ; This can only occur if selectList() call chooses to include count(tablename.*) with GROUPBY
	. . . set anotherholder=$piece($piece(holder,"(",2),")",1)
	. . . ; # FUTURE_TODO: We want to randomize selection of table.* for group by
	. . . ; Because this will lead to the testing of 2 cases
	. . . ; 1. COUNT(table.*) with group by having table.* and 2. COUNT(table.*) with group by not having table.*
	. . . ; Both are valid usages
	. . . ;quit:(1=$random(2))
	. . else  if (("binary_expression"=selectListLVN(j,holder))!("unary_expression"=selectListLVN(j,holder))!("conditional_expression"=selectListLVN(j,holder))) do
	. . . ; Expression or columns used to form it can be used in GroupBy.
	. . . ; If column is to be added select all columns involved in expression.
	. . . new innerHolder,columnSubExpr,subExpr,count,done
	. . . set count=0
	. . . set (columnSubExpr,subExpr,anotherholderType)=""
	. . . write "groupbyClause(): expression in GroupBy, listing expression and columns to be selected from",!
	. . . if ($DATA(groupByColumnLVN)) zwrite groupByColumnLVN
	. . . zwrite groupByListLVN
	. . . ; Choose either the expression or just the columns
	. . . if (1=$random(2)) do
	. . . . ; Form the holder using just the columns
	. . . . ; #FUTURE_TODO: In this case find the columns in select list and form a string having its corresponding
	. . . . ;   node number list seperated by ',' to be used later when chooseSelectNumber is 1
	. . . . set innerHolder="" for  set innerHolder=$order(groupByColumnLVN(j,holder,innerHolder))  quit:innerHolder=""  do
	. . . . . if columnSubExpr="" set columnSubExpr=innerHolder
	. . . . . else  set columnSubExpr=columnSubExpr_", "_innerHolder
	. . . . set anotherholder=columnSubExpr
	. . . . set anotherholderType="column"
	. . . else  do
	. . . . ; Form the holder using the entire expression
	. . . . set innerHolder=holder
	. . . . ; set innerHolder="" set innerHolder=$order(groupByListLVN(j,innerHolder))
	. . . . if (innerHolder'="") do
	. . . . . set anotherholder=innerHolder
	. . . . . set anotherholderType="expression"
	. . . . else  quit
	. . . set selectedExprGroupBy(j,anotherholder)=anotherholderType
	. . . set selectedExprGroupBy(j,anotherholder,1)=holder ; Only used in Having clause expression 'column' usage
	. . . zwr selectedExprGroupBy
	. . if (((selectListLVN(j,holder)["_expression")&("column"'=$get(anotherholderType,"")))&(0'=$get(selectListLVN(j,holder,1),0))&(j=queryDepth)) do
	. . . set selectNodeNumber=selectListLVN(j,holder,1) ; Possible when we come across subqueries
	. . . set:(((selectListLVN(j,holder)="table.column")!(selectListLVN(j,holder)["_expression"))&(1=$random(2))) chooseSelectNumber=1
	. . set:anotherholder'="" result=result_$select(firstholder:"",1:", ")_$select(chooseSelectNumber:selectNodeNumber,1:anotherholder),firstholder=0
	. . set:anotherholder="" result=result_$select(firstholder:"",1:", ")_$select(chooseSelectNumber:selectNodeNumber,1:holder),firstholder=0
	. . set:chooseSelectNumber chooseSelectNumber=0
	write "groupbyClause() : result = ",result,!
	quit $select(""=result:" ",1:" GROUP BY "_result)

; https://ronsavage.github.io/SQL/sql-92.bnf.html#having%20clause
; #FUTURE_TODO: integrate aggregate functions into all of the below if blocks
; This function returns a HAVING clause for either a subquery or the parent query based on which clauseType is provided.
; "query" and "subquery" are valid clauseTypes
; If the HAVING Clause is to appear in the greater query (most external level) run with "query"
; If the HAVING Clause is to appear within a subquery run with "subquery"
havingClause(queryDepth,clauseType)
	quit:$random(2) ""	; Randomly generate EMPTY HAVING clause

	new result,randInt,holder,count,randValue,table,count,chosenColumn,isAggrFuncColumn
	set result=" HAVING "

	set count=-1
	new i,j,isContantExpression,expressionExists
	set (isConstantExpression,expressionExists)=0
	for j=queryDepth:-1:0 do
	. set holder=""
	. for i=1:1 do  quit:(holder="")  do assert(i<16)
	. . set holder=$order(selectListLVN(j,holder))
	. . set:((""'=holder)&(selectListLVN(j,holder)["_constant_expression")) isConstantExpression=1
	. . set:((""'=holder)&(("binary_expression"=selectListLVN(j,holder))!("unary_expression"=selectListLVN(j,holder))!("conditional_expression"=selectListLVN(j,holder)))) expressionExists=1
	. . if (""=holder)!("subquery"'=selectListLVN(j,holder)) if $increment(count)

	; #FUTURE_TODO: Enable constant expressions in having clause by removing the following quit statement
	quit:isConstantExpression ""; skip generating Having clause when constant expressions are present

	do assert(0<count)
	; #FUTURE_TODO: Increase below to 8 when $$returnCaseFunction infinite loop and malformed query issues are resolved
	set randInt=$random(6) ; 0-5


	; If enableSubQuery is FALSE, randInt=1 and randInt=5 cannot be allowed as they invoke "generateSubQuery"
	; So in those cases, set randInt=0 instead.
	set:('enableSubQuery)&((randInt=1)!(randInt=5)) randInt=0

	; If table.asterisk is present in GroupBy allow randInt=2 and randInt=3 case as others are complicated
	; #FUTURE_TODO: Enhance othercases to use table.asterisk
	set randIntTA=$random(2)
	set:((tableAsteriskAggregateExists)&((randInt'=2)!(randInt'=3))) randInt=(2+randIntTA)

	set:expressionExists randInt=0
	; #FUTURE_TODO: Remove below line of code and enable having clause with table.* and expressions
	quit:(expressionExists&tableAsteriskAggregateExists) ""

	write "havingClause() : randInt = ",randInt,!

	if (randInt=0) do
	. new loopCount,opened,i,leftSide,rightSide,rightRand,leftRand
	. new loopCount,opened,i,leftSide,rightSide,notString,rightRand,leftRand
	. set (leftRand,rightRand)=0
	. set loopCount=$random(3)+1 ; 1-3
	. set opened="FALSE"
	. for i=1:1:loopCount do
	. . new isExpression,selectedQueryDepth
	. . set (isExpression,selectedQueryDepth)=0
	. . set chosenColumn=$$havingChooseColumn(queryDepth,.table,.isAggrFuncColumn,count,.isExpression,.selectedQueryDepth)
	. . if isExpression do
	. . . new innerHolder,tmp,count,lcli
	. . . set count=0
	. . . ; Iterate over the selectedExprGroupBy LVN and note down the count
	. . . set innerHolder=""
	. . . for lcli=1:1 set innerHolder=$order(selectedExprGroupBy(selectedQueryDepth,innerHolder)) quit:(innerHolder="")  do  do assert(i<16)
	. . . . if ($increment(count))
	. . . do assert(count)
	. . . write "Number of GroupBy elements"_count,!
	. . . set count=$random(count)+1 ; As $random returns 0 to (count-1) we add 1 to reflect the correct node
	. . . write "Node seleced for having clause"_count,!
	. . . set tmp=""
	. . . for lcli=1:1:count do
	. . . . set tmp=$order(selectedExprGroupBy(selectedQueryDepth,tmp))
	. . . zwr selectedExprGroupBy
	. . . if "column"=selectedExprGroupBy(selectedQueryDepth,tmp) do
	. . . . new num,count,done
	. . . . set (done,count)=0
	. . . . ; groupByColumnLVN has all the columns used to form the expression use it to form having clause
	. . . . set innerHolder="" for  set innerHolder=$order(groupByColumnLVN(selectedQueryDepth,selectedExprGroupBy(selectedQueryDepth,tmp,1),innerHolder))  quit:(innerHolder="")  do
	. . . . . if ($increment(count))
	. . . . set num=$random(count)+1
	. . . . set innerHolder="" for  set innerHolder=$order(groupByColumnLVN(selectedQueryDepth,selectedExprGroupBy(selectedQueryDepth,tmp,1),innerHolder)) quit:(done)  do
	. . . . . quit:innerHolder=""
	. . . . . if ($increment(count,-1))
	. . . . . set:count<=0 done=1,leftSide=innerHolder
	. . . . set table=$piece(leftSide,".",1)
	. . . . set rightSide=$$chooseEntry(table,$piece(leftSide,".",2))
	. . . else  do
	. . . . ; This is an expression, use groupByColumnLVN to obtain constants to form the correct operand
	. . . . set tmpExpr=tmp
	. . . . set leftSide="("_tmpExpr_")"	; This is an expression
	. . . . write "expression is choosen in having clause, get right from groupByColumnLVN",!
	. . . . zwr groupByColumnLVN
	. . . . set tmp="" set cl=$order(groupByColumnLVN(selectedQueryDepth,tmpExpr,tmp))
	. . . . set table=$piece(cl,".",1)
	. . . . if (groupByComparisonExpressionExists(selectedQueryDepth,tmpExpr)) set rightSide=$$getRandBoolean
	. . . . else  set rightSide=$$chooseEntry(table,$piece(cl,".",2))
	. . else  do
	. . . ; If chosen column is used with aggregate functions in the select column list, use it with an aggregate function
	. . . ; in the having clause too (i.e. ensure leftRand and rightRand never get set to 2).
	. . . ; #FUTURE_TODO: un-comment below code to enable table.* usage in this case
	. . . ; if (isAggrFuncColumn&(chosenColumn="*")) do
	. . . ; . set (leftRand,rightRand)=1
	. . . ; else  do
	. . . ; . set max=$select(""'=isAggrFuncColumn:1,1:3)
	. . . ; . set leftRand=$random(max),rightRand=$random(max)
	. . . set max=$select(""'=isAggrFuncColumn:1,1:3)
	. . . set leftRand=$random(max),rightRand=$random(max)
	. . . if (leftRand=0)  set leftSide=$$chooseEntry(table,chosenColumn)
	. . . if (leftRand=1) set leftSide=$$havingGetTblCol(isAggrFuncColumn,table,chosenColumn)
	. . . if (leftRand=2)  set leftSide=table_"."_chosenColumn
	. . .
	. . . if (rightRand=0)  set rightSide=$$chooseEntry(table,chosenColumn)
	. . . if (rightRand=1) set rightSide=$$havingGetTblCol(isAggrFuncColumn,table,chosenColumn)
	. . . if (rightRand=2)  set rightSide=table_"."_chosenColumn
	. . . ; If COUNT got chosen as an aggregate function on one side, ensure the other side is a count and not a chooseEntry
	. . . if (leftRand=0)&(rightRand=1)&("COUNT"=isAggrFuncColumn) set leftSide=$$chooseCount
	. . . if (leftRand=1)&(rightRand=0)&("COUNT"=isAggrFuncColumn) set rightSide=$$chooseCount
	. . zwrite leftRand,rightRand,leftSide,rightSide
	. .
	. . ; First portion of WHERE, no AND or OR
	. . if (i=1) set result=result_"("_$$notString_$$comparisonOperators(leftSide,rightSide)
	. . ; Following portions of WHERE, with AND or OR separators
	. . if (i'=1) do
	. . . set result=result_" "_$$booleanOperator_" "
	. . . if (($random(2))&(opened="FALSE"))  do
	. . . . set result=result_"("
	. . . . set opened="TRUE"
	. . . set result=result_$$comparisonOperators(leftSide,rightSide)
	. . . if (($random(2))&(opened="TRUE"))  do
	. . . . set result=result_")"
	. . . . set opened="FALSE"
	. . if (i=loopCount)  set result=result_")"
	. . if ((i=loopCount)&(opened="TRUE"))  set result=result_")"

	if (randInt=1) do
	. set existsInHavingExists="TRUE"
	. new alias
	. set result=result_$$notString_"EXISTS ("_$$generateSubQuery(queryDepth,"full","")_")"

	if (randInt=2) do
	. new chosenColumn,notString,entryList,i,tblcol
	. set chosenColumn=$$havingChooseColumn(queryDepth,.table,.isAggrFuncColumn,count)
	. set entryList=""
	. set limit=($random($$maxIndex(table)-1)+1)
	. if (limit>8)  set limit=8
	. for i=1:1:limit do
	. . set entryList=entryList_$select(1<i:", ",1:"")
	. . set entryList=entryList_$select(("COUNT"=isAggrFuncColumn):$$chooseCount,1:$$chooseEntry(table,chosenColumn))
	. set tblcol=$$havingGetTblCol(isAggrFuncColumn,table,chosenColumn)
	. set notString=$select($random(2):"NOT ",1:"")	; cannot use $$notString here because "NOT NOT IN" is not valid syntax
	. set result=result_tblcol_" "_notString_"IN ("_entryList_")"

	if (randInt=3) do
	. new chosenColumn,tblcol
	. set chosenColumn=$$havingChooseColumn(queryDepth,.table,.isAggrFuncColumn,count)
	. set tblcol=$$havingGetTblCol(isAggrFuncColumn,table,chosenColumn)
	. set result=result_tblcol_" BETWEEN "
	. set result=result_$select(("COUNT"=isAggrFuncColumn):$$chooseCount,1:$$chooseEntry(table,chosenColumn))
	. set result=result_" AND "
	. set result=result_$select(("COUNT"=isAggrFuncColumn):$$chooseCount,1:$$chooseEntry(table,chosenColumn))

	if (randInt=4) do
	. new chosenColumn,i
	. ; Forces column to be of type "VARCHAR" as to do a string comparison
	. set type=""
	. for i=1:1:15  quit:("VARCHAR"=type)&("COUNT"'=isAggrFuncColumn)  do
	. . set chosenColumn=$$havingChooseColumn(queryDepth,.table,.isAggrFuncColumn,count)
	. . set type=$$returnColumnType(table,chosenColumn)
	. . write "havingClause() : i = ",i," : table = ",table," : chosenColumn = ",chosenColumn," : type = ",type," : isAggrFuncColumn = ",isAggrFuncColumn,!
	. if (i'=15) do
	. . new string,charCount
	. . set string=""
	. . set desiredStringLength=$random(7)+1 ; 1-7
	. . for charCount=1:1:desiredStringLength do
	. . . set randValue=$random(9) ; 0-8
	. . . if ((randValue=0)!(randValue=1)) set string=string_$char($random(27)+64) ; @,A-Z
	. . . if ((randValue=2)!(randValue=3)) set string=string_$char($random(26)+97) ; a-z
	. . . if (randValue=4) set string=string_$random(10) ; 0-9
	. . . if (randValue=5) set string=string_"#"
	. . . if (randValue=6) set string=string_"$"
	. . . if (randValue=7) set string=string_"%"
	. . . if (randValue=8) set string=string_"_"
	. . set tblcol=$$havingGetTblCol(isAggrFuncColumn,table,chosenColumn)
	. . write "havingClause() : tblcol = ",tblcol,!
	. . set result=result_tblcol_" LIKE '"_string_"'"
	. else  do
	. . ; We try the next random option (i.e. randInt=5). But if enableSubQuery is 0, then try randInt=6 instead
	. . ; as randInt=5 has a call to "generateSubQuery" which should be avoided if enableSubQuery is 0.
	. . set randInt=$select(enableSubQuery:5,1:6)

	if (randInt=5) do
	. new randInt,word,leftSide,rightSide,chosenColumn,entryList,limit,rightType,i,holder,aliasNum1More
	. set word=$$anyAllSome
	. set aliasNum1More=aliasNum+1
	. set rightSide="("_$$generateSubQuery(queryDepth,"limited","")_")"
	. set aliasDotColumn=""
	. for i=1:1  do  quit:($find(aliasDotColumn,"alias"_aliasNum1More)'=0)  do assert(i<16)
	. . set aliasDotColumn=$piece(rightSide," ",i)
	. set rightColumn=$piece($piece(aliasDotColumn,".",2),",",1)
	.
	. set holder=""
	. for i=1:1  do  quit:($find(holder,"FROM")'=0)  do assert(i<16)
	. . set holder=$piece(rightSide," ",i)
	. set rightTable=$piece(rightSide," ",i+1)
	. set rightType=$$returnColumnType(rightTable,rightColumn)
	.
	. for i=1:1:15  do  quit:(leftType=rightType)&(("COUNT"'=isAggrFuncColumn)!(("VARCHAR"'=leftType)&("BOOLEAN"'=leftType)))
	. . set leftSide=$$havingChooseColumn(queryDepth,.table,.isAggrFuncColumn,count)
	. . set leftType=$$returnColumnType(table,leftSide)
	. . set tblcol=$$havingGetTblCol(isAggrFuncColumn,table,leftSide)
	. zwrite i,leftType,rightType,leftSide,tblcol,table,rightSide
	. if (i'=15) set result=result_$$comparisonOperators(tblcol,word_" "_rightSide)
	. else  set result=""

	if (randInt=6) do
	. new toCompare
	. set toCompare=$random(4)+1
	. if (clauseType="subquery")  set result=result_toCompare_" = "_$$returnCaseFunction("WHERE","randomNumbers","numbers","TRUE",toCompare)
	. if (clauseType="query")  set result=result_toCompare_" = "_$$returnCaseFunction("WHERE","randomNumbers","numbers","FALSE",toCompare)

	if (randInt=7) do
	. new leftCaseArg,rightCaseArg,i,toCompare
	. set leftColumn=$$chooseColumn(table)
	.
	. set leftType=$$returnColumnType(table,leftColumn)
	. for i=1:1:15  do  quit:(leftType=rightType)
	. . set rightColumn=$$chooseColumn(table)
	. . set rightType=$$returnColumnType(table,rightColumn)
	.
	. if (clauseType="query") set leftCaseArg=table_"."_leftColumn
	. if (clauseType="subquery") set leftCaseArg="alias"_aliasNum_"."_leftColumn
	. if ($random(2))  set leftCaseArg=$$chooseEntry(table,leftColumn)
	. if (clauseType="query") set rightCaseArg=table_"."_rightColumn
	. if (clauseType="subquery") set rightCaseArg="alias"_aliasNum_"."_rightColumn
	. if ($random(2))  set rightCaseArg=$$chooseEntry(table,rightColumn)
	.
	. if (i'=15) do
	. . if (clauseType="query") do
	. . . set toCompare=table_"."_$$chooseColumn(table)
	. . . set result=result_toCompare_" = "_$$returnCaseFunction("WHERE","lrComparison","columns","FALSE",toCompare)
	. . if (clauseType="subquery") do
	. . . set toCompare="alias"_aliasNum_"."_$$chooseColumn(table)
	. . . set result=result_toCompare_" = "_$$returnCaseFunction("WHERE","lrComparison","columns","TRUE",toCompare)
	. else  set result=""

	quit result

havingGetTblCol(isAggrFuncColumn,table,chosenColumn)
	new tblcol
	if ""'=isAggrFuncColumn set tblcol=$$returnSimilarAggregateFunction(isAggrFuncColumn,table,chosenColumn)
	else  set tblcol=table_"."_chosenColumn
	quit tblcol

; https://ronsavage.github.io/SQL/sql-92.bnf.html#order%20by%20clause
; This function returns an ORDER BY clause for either a subquery or the parent query.
; It takes in an alias number (aNum) as a paremeter.
; It also takes in a location value as a parameter.
; If the ORDER BY Clause is to appear in the parent query run with "QUERY"
; If the ORDER BY Clause is to appear within a subquery run with "SUBQUERY"
orderbyClause(queryDepth,aNum,location)
	do assert(queryDepth!enableOrderByClause)
	new result,holder,orderByListSize
	set result="",orderByListSize=0
	; Following if statement checks for DISTINCT in the parent query as the
	; presence of a DISTINCT qualifier requires the ORDER BY to contain only
	; things that also occur in the SELECT LIST
	if ($data(quantifierLVN("alias"_aNum))'=0)  do
	. new rand
	. set rand=$random(2)
	. ; #FUTURE_TODO: Remove below line when $$returnCaseFunction infinite loop and malformed query issues are resolved
	. set rand=1	; disable going to "else" part as it is code that generates incorrect queries.
	. if (rand!(quantifierLVN("alias"_aNum)="DISTINCT "))  do
	. . write "orderbyClause() : aNum = ",aNum," : location = ",location,!
	. . new firstholder
	. . set firstholder=1
	. . ; Only choose select column list columns at query depth "queryDepth" for ORDER BY due to SELECT DISTINCT usage.
	. . set holder="" for  set holder=$order(selectListLVN(queryDepth,holder))  quit:holder=""  do
	. . . write "orderbyClause() : holder = ",holder,!
	. . . if (holder'="*")  do
	. . . . set result=result_$select(firstholder:"",1:", ")_holder,firstholder=0
	. . . . if ($increment(orderByListSize))
	. else  do
	. . ; #FUTURE_TODO: There is no basis for determining whether an ORDER BY occurs within a subquery
	. . ;               or not, so eventually add the fourth parameter to the below call to
	. . ;               returnCaseFunction when this logic is implemented or needed
	. . if (location="QUERY") do
	. . . set table=$piece(fc," ",2)
	. . . set toCompare=table_"."_$$chooseColumn(table)
	. . . set result=result_$$returnCaseFunction("ORDER BY","randomNumbers","columns","FALSE",toCompare)
	. . if (location="SUBQUERY") do
	. . . set subqueryTable=$extract(innerQuery,$find(innerQuery,"FROM "),$find(innerQuery," "))
	. . . set toCompare="alias"_aNum_"."_$$chooseColumn(subqueryTable)
	. . . set result=result_$$returnCaseFunction("ORDER BY","randomNumbers","columns","TRUE",toCompare)
	else  set result=""
	if (""'=result)  zwr orderByListSize
	if (""'=result) do checkIfOrderByLessThanSelectList(queryDepth,location,orderByListSize)
	quit $select(""=result:"",1:" ORDER BY "_result)

; This function returns a LIMIT Clause, with a number between 0 and 31
limitClause()
	do assert(enableLimitClause)
	new result
	set result=" LIMIT "
	set result=result_($random(2**$random(8)))
	quit result

; This function returns the JOIN clause. It only works for the parent query (not for a sub query).
; This function contains the logic for selecting a JOIN type, what is actually being joined, and the ON clause.
joinClause(queryDepth,joinCount)
	new result,chosenTable1,chosenColumn1,chosenColumn2,chosenTable2,type1,joinType,operator,loopCount
	set result=""

	set joinType=$$joinTypes(joinCount)

	set result=result_" "_joinType_" JOIN "
	; Remove " OUTER" from join type for further processing so "LEFT" AND "LEFT OUTER" joins are treated the same.
	set joinType=$piece(joinType," OUTER",1)

	set chosenTable1=$$chooseTableFromTableColumn()
	set chosenColumn1=$$chooseColumn(chosenTable1)
	set type1=$$returnColumnType(chosenTable1,chosenColumn1)
	set chosenEntry1=$$chooseEntry(chosenTable1,chosenColumn1)

	set chosenEntry2=""
	set aliasNum1More=aliasNum+1
	set asPiece=" AS alias"_aliasNum1More

	set randInt=$random(2)

	; If enableSubQuery is FALSE, randInt=1 cannot be allowed as it invokes "generateSubQuery"
	; So in that case, set randInt=0 instead.
	set:('enableSubQuery)&(randInt=1) randInt=0

	write "joinClause() : randInt = ",randInt,!
	if (randInt=0)  do
	. new i,limiter,typematch
	. ; After 8 attempts at finding two compatible columns (same type), select a new chosenTable2
	. ; repeat this 2 more times and if this fails to produce a column that is valid, then delete
	. ; the join portion of the query
	. set typematch=0
	. for limiter=1:1:3 do  quit:typematch
	. . set chosenTable2=$$chooseTable	; Note: it is possible chosenTable2 is same as chosenTable1
	. . for i=1:1:8  do  quit:typematch
	. . . new type2
	. . . set chosenColumn2=$$chooseColumn(chosenTable2)
	. . . set type2=$$returnColumnType(chosenTable2,chosenColumn2)
	. . . set typematch=(type2=type1)
	. set chosenEntry2=$$chooseEntry(chosenTable2,chosenColumn2)
	. set result=$select(typematch:result_chosenTable2_asPiece,1:"")

	if (randInt=1)  do
	. new subquery,i,aliasName
	. set chosenEntry2=""
	. set aliasName="alias"_aliasNum1More
	. kill innerQuerySLLVN,columns(aliasName)	; kill any leftover artifacts from previous sub queries
	. set subquery=$$generateSubQuery(queryDepth,"full",joinType)
	. ; -------------------------------------------------------------------
	. ; Record select column list of sub-query as column list of table in outer query join clause
	. new aliasColumnName
	. set aliasColumnName="",tableColumnCounts(aliasName)=1
	. for  set aliasColumnName=$order(innerQuerySLLVN(aliasName,aliasColumnName))  quit:""=aliasColumnName  do
	. . new tableName
	. . set tableName=innerQuerySLLVN(aliasName,aliasColumnName)
	. . set columns(aliasName,tableColumnCounts(aliasName),aliasColumnName)=$$getColumnType(tableName,aliasColumnName)
	. . set columnAlias(aliasName,aliasColumnName)=tableName
	. . if $increment(tableColumnCounts(aliasName))
	. ; Note that it is possible we give up and return with result="" at the end of this function.
	. ; In that case, the above (updates to "tableColumnCounts" and "columns" arrays) will still stay but it is benign.
	. ; and so is left as is
	. ; -------------------------------------------------------------------
	. set randFromInnerQuerySLLVN=$random(innerQuerySLLVN),chosenColumn2=""
	. for i=0:1:randFromInnerQuerySLLVN  set chosenColumn2=$order(innerQuerySLLVN(aliasName,chosenColumn2))
	. set chosenTable2=innerQuerySLLVN(aliasName,chosenColumn2)
	. set type2=$$returnColumnType(chosenTable2,chosenColumn2)
	.
	. for i=1:1:15  quit:(type2=type1)  do
	. . set chosenColumn1=$$chooseColumn(chosenTable1)
	. . set type1=$$returnColumnType(chosenTable1,chosenColumn1)
	. set chosenEntry1=$$chooseEntry(chosenTable1,chosenColumn1)
	.
	. ; #FUTURE_TODO: `chosenColumn2` set in the below code isn't used anywhere. Check if the following if block can be removed.
	. if ($find(subquery,"EXISTS (")'=0)  do
	. . set aliasNum1Less=aliasNum-1
	. . for i=0:1:randFromInnerQuerySLLVN  do
	. . . set chosenColumn2=$order(innerQuerySLLVN("alias"_aliasNum1Less,""))
	.
	. if (i'=15)  set result=result_"("_subquery_")"_asPiece
	. set chosenTable2=aliasName
	. if (i=15)  set result=""

	; #FUTURE_TODO: Implement CASE statements into ON clause like in below example
	;               EXAMPLE:https://dev.to/gschro/conditional-column-join-in-sql-5g03
	new i,j,resetOnClause
	set onClause=""
	set loopCount=$random(3)+1
	set opened="FALSE",resetOnClause=0
	if ((result'="")&(joinType'="CROSS")&(joinType'="NATURAL"))  do
	. for i=1:1:loopCount  do
	. . new rightRand
	. . if ($random(2))  set leftSide=chosenTable1_"."_chosenColumn1
	. . else  set leftSide=chosenEntry1
	. .
	. . set rightRand=$random(3) ; 0-2
	. . ;
	. . ; If enableSubQuery is FALSE, rightRand=2 cannot be allowed as it invokes "generateSubQuery"
	. . ; So in that case, set rightRand=0 instead.
	. . set:('enableSubQuery)&(rightRand=2) rightRand=0
	. . ;
	. . write "joinClause() : rightRand = ",rightRand," : chosenEntry2 = ",chosenEntry2,!
	. . if ((rightRand=0)!(chosenEntry2="")) do
	. . . set alias=""
	. . . for aliasI=1:1  do  quit:(alias="AS")
	. . . . set alias=$piece(result," ",aliasI)
	. . . set alias=$piece(result," ",aliasI+1)
	. . .
	. . . if (randInt=0) do
	. . . . set tableInResult=""
	. . . . for tableI=1:1  do  quit:(tableInResult="JOIN")
	. . . . . set tableInResult=$piece(result," ",tableI)
	. . . . set tableInResult=$piece(result," ",tableI+1)
	. . . . set chosenColumn2=$$chooseColumn(tableInResult)
	. . . . set rightSide=alias_"."_chosenColumn2
	. . . . set rightType=$$returnColumnType(tableInResult,chosenColumn2)
	. . . if (randInt=1) do
	. . . . for rightSideI=1:1  do  quit:(($find(rightSide,alias)'=0)&($find(rightSide,")")=0))
	. . . . . set rightSide=$piece(result," ",rightSideI)
	. . . . if ($extract(rightSide,$length(rightSide))=",")  set rightSide=$piece(rightSide,",",1)
	. . . . set tableInResult=""
	. . . . for tableI=1:1  do  quit:(tableInResult="FROM")
	. . . . . set tableInResult=$piece(result," ",tableI)
	. . . . set tableInResult=$piece(result," ",tableI+1)
	. . . . set columnModified=$piece(rightSide,".",2)
	. . . . if ($find(columnModified,",")'=0)  set columnModified=$extract(columnModified,0,$length(columnModified)-1)
	. . . .
	. . . . set rightType=$$returnColumnType(tableInResult,columnModified)
	. . .
	. . . set leftType=""
	. . . for j=1:1:15  do  quit:(leftType=rightType)
	. . . . set leftSide=$$chooseColumn(chosenTable1)
	. . . . set leftType=$$returnColumnType(chosenTable1,leftSide)
	. . . set:j=15 resetOnClause=1	; we could not find a column with a compatible type so reset ON clause
	. . .
	. . . set leftSide=chosenTable1_"."_leftSide
	. . if ((rightRand=1)&(chosenEntry2'=""))  set rightSide=chosenEntry2
	. . if (rightRand=2) do
	. . . new aliasNum2More
	. . . set aliasNum2More=aliasNum+1
	. . . set rightSide=$$anyAllSome_" ("_$$generateSubQuery(queryDepth,"limited","")_")"
	. . .
	. . . set aliasDotColumn=""
	. . . for j=1:1:20  do  quit:($find(aliasDotColumn,"alias"_aliasNum2More)'=0)
	. . . . set aliasDotColumn=$piece(rightSide," ",j)
	. . . do assert(20>j)
	. . . set rightColumn=$piece($piece(aliasDotColumn,".",2),",",1)
	. . .
	. . . new rightI
	. . . set holder=""
	. . . for rightI=1:1  do  quit:($find(holder,"FROM")'=0)
	. . . . set holder=$piece(rightSide," ",rightI)
	. . . set rightTable=$piece(rightSide," ",rightI+1)
	. . . set rightType=$$returnColumnType(rightTable,rightColumn)
	. . .
	. . . set leftType=""
	. . . for j=1:1:15  do  quit:(leftType=rightType)
	. . . . set leftSide=$$chooseColumn(chosenTable1)
	. . . . set leftType=$$returnColumnType(chosenTable1,leftSide)
	. . . set:j=15 resetOnClause=1	; we could not find a column with a compatible type so reset ON clause
	. . .
	. . . set leftSide=chosenTable1_"."_leftSide
	. .
	. . if (($random(2))&(opened="FALSE"))  do
	. . . set onClause=onClause_"("
	. . . set opened="TRUE"
	. . set onClause=onClause_$$comparisonOperators(leftSide,rightSide)
	. . write "joinClause() : onClause = ",onClause,!
	. . if (($random(2))&(opened="TRUE"))  do
	. . . set onClause=onClause_")"
	. . . set opened="FALSE"
	. . if (i<loopCount)  set onClause=onClause_" "_$$booleanOperator_" "_$$notString
	. set onClause=" ON ("_onClause_")"
	. if (opened="TRUE")  set onClause=onClause_")"  set opened="FALSE"
	. set:resetOnClause onClause="",result=""
	if (""'=result) do
	. set tableColumn("alias"_aliasNum1More)=""
	. set tableAlias("alias"_aliasNum1More)=chosenTable2
	. write "joinClause() : tableAlias(alias",aliasNum1More,")=",chosenTable2,!

	quit result_onClause

chooseTableFromTableColumn()
	new tableCount,randInt,i,chosenTable
	; Find out number of tables that are in the FROM/JOIN list
	set tableCount=0,chosenTable=""
	for  set chosenTable=$order(tableColumn(chosenTable))  quit:chosenTable=""  if $increment(tableCount)
	; Choose a table randomly from this list
	set randInt=$random(tableCount)
	set chosenTable="" for i=0:1:randInt set chosenTable=$order(tableColumn(chosenTable))
	quit chosenTable

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Supporting Functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
initTable()
	new x,count
	set table=""
	set tables=0
	set x=""
	for  set x=$order(sqlInfo(x))  set:(x'="") tables=tables+1  quit:x=""
	set count=-1
	set x=""
	for  set x=$order(sqlInfo(x))  quit:x=""  set tables($increment(count))=x
	quit

initColumn()
	new a,currentTable,column,columnCount,x,count
	for a=0:1:GLOBALtotalTables-1 do
	. set currentTable=tables(a)
	. set column=""
	. set columnCount=1
	. set x=""
	. for  set x=$order(sqlInfo(currentTable,x))  quit:x=""  if $increment(columnCount)
	. set tableColumnCounts(currentTable)=columnCount
	. set count=1
	. set x=""
	. for  set x=$order(sqlInfo(currentTable,count,x)) do  quit:(count=columnCount)  if $increment(count)
	. . set columns(currentTable,count,x)=$$getType($order(sqlInfo(currentTable,count,x,"")))
	. . set x=""
	. kill columns(currentTable,count) ;kills empty last entry in this table
	quit

initAggrFunc()
	set aggFunctionInt=-1
	set aggFunctionInt($increment(aggFunctionInt))="AVG"
	set aggFunctionInt($increment(aggFunctionInt))="COUNT"
	set aggFunctionInt($increment(aggFunctionInt))="MIN"
	set aggFunctionInt($increment(aggFunctionInt))="MAX"
	set aggFunctionInt($increment(aggFunctionInt))="SUM"
	if $increment(aggFunctionInt)

	set aggFunctionBool=-1
	set aggFunctionBool($increment(aggFunctionBool))="COUNT"
	if $increment(aggFunctionBool)

	set aggFunction=-1
	set aggFunction($increment(aggFunction))="MIN"
	set aggFunction($increment(aggFunction))="MAX"
	set aggFunction($increment(aggFunction))="COUNT"
	if $increment(aggFunction)

	set aggModifier=-1
	set aggModifier($increment(aggModifier))=""
	set aggModifier($increment(aggModifier))="DISTINCT "
	set aggModifier($increment(aggModifier))="ALL "
	if $increment(aggModifier)
	quit

; This function will randomly select a table from an LVN of all possible tables.
chooseTable()
	quit tables($random(tables))

; This function will randomly select a column from an LVN of all possible columns, based off of the provided table parameter.
chooseColumn(table)
	new CC,selectedColumn,aliastable
	set aliastable=table,table=$$getRealTable(aliastable)
	set CC=$SELECT($data(tableColumnCounts(table))=0:$$columnCounter(table),1:tableColumnCounts(table))
	set selectedColumn=$order(columns(table,$random(CC-1)+1,""))
	quit selectedColumn

; This function is designed for the HAVING clause.
; It will randomly select a table from the selectListLVN, which is required for the HAVING clause.
havingChooseColumn(queryDepth,table,isAggrFuncColumn,count,isExpression,selectedQueryDepth)
	new randValue,holderMinusAggrFunc,chosenColumn
	set randValue=$random(count)+1

	new i,j,holder,done
	set i=randValue
	set done=0
	for j=queryDepth:-1:0 do  quit:done
	. set holder="" for  set holder=$order(selectListLVN(j,holder))  do  quit:done
	. . quit:holder=""
	. . quit:"subquery"=selectListLVN(j,holder) 	; skip columns that are subqueries
	. . if i=0 set done=1  quit
	. . set i=i-1

	; Check if boolean expression is choosen
	; If so return back and indicate to the caller as such and let caller use selectedExprGroupBy value to form the having expression
	if (("unary_expression"=selectListLVN(j,holder))!("binary_expression"=selectListLVN(j,holder))!("conditional_expression"=selectListLVN(j,holder))) do  quit ""
	. write "havingChooseColumn(): expression is selected",!
	. set isExpression=1
	. set selectedQueryDepth=j
	. quit

	; Remove any aggregate function usage
	; (i.e. COUNT(x.y) should yield x.y; COUNT(ALL x.y) should yield x.y; ROUND(AVG(x.y)) should yield x.y;)
	if $find(holder,"ROUND(AVG(") do
	. new tmpHolder
	. ; returnAggregateFunction would have generated AVG(table.column) or AVG(ALL table.column) or AVG(DISTINCT table.column)
	. ; So fetch the 3rd piece this should give us the complete value within AVG()
	. set holderMinusAggrFunc=$piece($piece(holder,"(",3),")")
	. ; Remove ALL or DISTINCT and if they are not present retain just table.column value
	. set tmpHolder=$piece(holderMinusAggrFunc," ",2)
	. ; if tmpHolder is not empty - ALL or DISTINCT was used and tmpHolder now has table.column. Copy it to holderMinusAggrFunc.
	. set:(tmpHolder'="") holderMinusAggrFunc=tmpHolder
	. ; else holderMinusAggrFunc has table.column nothing else to be done
	. set isAggrFuncColumn="AVG"
	else  if $find(holder,"(") do
	. if $find(holder,">") do
	. . ; Account for BOOLEAN type coercion on result of COUNT done in returnAggregateFunction for BOOLEAN column types
	. . set holder=$piece(holder," >",1)
	. set holderMinusAggrFunc=$piece($piece(holder,"(",2),")",1)
	. if $find(holderMinusAggrFunc," ") do
	. . set holderMinusAggrFunc=$piece(holderMinusAggrFunc," ",2)
	. set isAggrFuncColumn=$piece(holder,"(",1)
	else  do
	. set holderMinusAggrFunc=holder
	. set isAggrFuncColumn=""
	set table=$piece(holderMinusAggrFunc,".",1)
	do assert(""'=table)
	set chosenColumn=$piece(holderMinusAggrFunc,".",2)
	do assert(""'=chosenColumn)
	quit chosenColumn

; This function returns the number of columns in the provided table. It is mainly used in other supporting functions.
columnCounter(currentTable)
	set column=""
	set columnCount=1
	set x=""
	for  set x=$order(sqlInfo(currentTable,x))  quit:x=""  if $increment(columnCount)
	quit columnCount

; This function returns a randomly selected entry from a table and column pair, which both need to be passed in as parameters.
; This function assumes that it is being handed a valid set of tableName and columnName.
chooseEntry(tableName,columnName)
	new index,formatted,CC,type,entry,i,randInt,count,maxIndex,randFromMaxIndex,realTable

	set realTable=$$getRealTable(tableName,columnName)
	set maxIndex=$$maxIndex(realTable)

	set randFromMaxIndex=$random(maxIndex-1)+1
	set formatted="^"_realTable_"("_randFromMaxIndex_")"

	; Gets type of selected entry
	set type=$$returnColumnType(realTable,columnName)

	set index=$$returnColumnIndex(realTable,columnName)
	; index will occasionally be set to 0,
	; which causes a blank entry to be returned
	if (index=0)  set index=1

	; randomly selects an entry to be returned
	set entry=$order(data(formatted,index,""))

	;if ' found in entry, replace it with '', both Octo and PSQL require this for things like
	; "Name's Store" or "Stop N' Shop"
	set position=$find(entry,"'")
	if (position'=0)  do
	. for  do  quit:(position=0)
	. . set firstPart=$extract(entry,0,position-2)
	. . set secondPart=$extract(entry,position,1000000)
	. . set entry=firstPart_"''"_secondPart
	. . set position=$find(entry,"'",position+2)

	; If selected entry is not of "INTEGER", "NUMERIC", or "BOOLEAN" type, pre/append a single quote (')
	; This is done to match PSQL format rules, Octo does not care
	if (($find(type,"INTEGER")=0)&($find(type,"NUMERIC")=0)&($find(type,"BOOLEAN")=0)) do
	. if (""=entry) set entry="NULL::varchar"	; Need "::varchar" since this can be used as left side of || operator
	. else  set entry="'"_entry_"'"

	; Convert empty field to default value based on type.
	; No action needed for VARCHAR case as this is incidentally handled by the single-quote wrapping done above.
	; In the case of BOOLEAN, append typecast for compatibility between Octo's 0/1 booleans and Postgres' true/false
	set:(""=entry) entry="0"
	set:("BOOLEAN"=type) entry=entry_"::boolean"

	quit entry

; This function returns the positional number/index of a column in the columns LVN.
; It is mainly used for other supporting functions.
returnColumnIndex(tableName,columnName)
	new realTable
	; Returns index of column
	set index=0
	set realTable=$$getRealTable(tableName,columnName)
	for  quit:($order(columns(realTable,index,""))=columnName)  do assert(index<16) if $increment(index)
	quit index

; This function returns the stored type of whatever tableName and columnName that is provided to it.
; This function also is a common cause of infinite loops as it assumes that it is being given a valid set of tableName and columnName.
returnColumnType(tableName,columnName)
	new realTable,index,type
	set realTable=$$getRealTable(tableName,columnName)
	set index=$$returnColumnIndex(realTable,columnName)
	set type=$order(sqlInfo(realTable,index,columnName,""))
	write "$$returnColumnType() : tableName = ",tableName," : columnName = ",columnName," : realTable = ",realTable," : index = ",index," : type = ",type,!
	quit $$getType(type)

; This function returns the maximum value of items in the data LVN for a given table.
; Essentially this function returns the max possible integer value for items in a given tableName.
maxIndex(tableName)
	new holder,maxIndex,a,thing

	set holder=" "
	for maxIndex=1:1  do  quit:(holder="")
	. set a="^"_tableName_"("_maxIndex_")"
	. set holder=$order(data(a,""))

	set maxIndex=maxIndex-1
	if (maxIndex=0)  set maxIndex=2
	quit maxIndex

comparisonOperators(leftSide,rightSide)
	new compareRand
	; Randomly choose one of the following (set in "compareRand" variable).
	;	x IS NULL          : 1/compareRandMax of the time
	;	x IS NOT NULL      : 1/compareRandMax of the time
	;	x = NULL           : 1/compareRandMax of the time
	;	x = y, x != y etc. : (compareRandMax-3)/compareRandMax of the time
	; compareRandMax is set to 8 75% of the time as it reflects real world query usage.
	; It is set to 2 25% of the time which tests "IS NOT NULL" and "IS NULL" a lot more and therefore tests YDBOcto#782.
	set:'$data(compareRandMax) compareRandMax=$select($random(4):8,1:2)
	set compareRand=$random(compareRandMax)
	quit:compareRand=0 "("_leftSide_" IS NULL)"		; Test "column IS NULL"
	quit:compareRand=1 "("_leftSide_" IS NOT NULL)"		; Test "column IS NOT NULL"
	if (initDone("comparisonOperators")=0) do
	. set initDone("comparisonOperators")=1
	. set comparisonOperators=-1
	. set comparisonOperators($increment(comparisonOperators))="="
	. set comparisonOperators($increment(comparisonOperators))="!="
	. set comparisonOperators($increment(comparisonOperators))="<"
	. set comparisonOperators($increment(comparisonOperators))=">"
	. set comparisonOperators($increment(comparisonOperators))="<="
	. set comparisonOperators($increment(comparisonOperators))=">="
	. if $increment(comparisonOperators)
	set toBeReturned=comparisonOperators($random(comparisonOperators))
	set:compareRand=2 rightSide="NULL"			; Test "column = NULL", "column < NULL" etc.
	quit "("_leftSide_" "_toBeReturned_" "_rightSide_")"	; Test "column1 = column2", "column1 != value1" etc.

getComparisonOperatorAlone()
	if (initDone("comparisonOperatorsAlone")=0) do
	. set initDone("comparisonOperatorsAlone")=1
	. set comparisonOperatorsAlone=-1
	. set comparisonOperatorsAlone($increment(comparisonOperatorsAlone))="="
	. set comparisonOperatorsAlone($increment(comparisonOperatorsAlone))="!="
	. set comparisonOperatorsAlone($increment(comparisonOperatorsAlone))="<"
	. set comparisonOperatorsAlone($increment(comparisonOperatorsAlone))=">"
	. set comparisonOperatorsAlone($increment(comparisonOperatorsAlone))="<="
	. set comparisonOperatorsAlone($increment(comparisonOperatorsAlone))=">="
	. if $increment(comparisonOperatorsAlone)
	set toBeReturned=comparisonOperatorsAlone($random(comparisonOperatorsAlone))
	quit toBeReturned

arithmeticOperator()
	if (initDone("arithmeticOperator")=0) do
	. set initDone("arithmeticOperator")=1
	. set arithmeticOperator=-1
	. set arithmeticOperator($increment(arithmeticOperator))="+"
	. set arithmeticOperator($increment(arithmeticOperator))="-"
	. set arithmeticOperator($increment(arithmeticOperator))="*"
	. ;set arithmeticOperator($increment(arithmeticOperator))="/" ; #FUTURE_TODO: Uncomment when Issue 365 (Octo division returns decimal) is resolved
	. set arithmeticOperator($increment(arithmeticOperator))="%"
	. if $increment(arithmeticOperator)
	quit arithmeticOperator($random(arithmeticOperator))

booleanOperator()
	if (initDone("booleanOperator")=0) do
	. set initDone("booleanOperator")=1
	. set booleanOperator=-1
	. set booleanOperator($increment(booleanOperator))="AND"
	. set booleanOperator($increment(booleanOperator))="OR"
	. if $increment(booleanOperator)
	quit booleanOperator($random(booleanOperator))

joinTypes(joinCount)
	if (initDone("joinTypes")=0) do
	. set initDone("joinTypes")=1
	. set joinTypes=-1
	. ; Choose most of the joins as optional that way we increase the randomness of test coverage
	. set:(0=$random(2)) joinTypes($increment(joinTypes))="LEFT"
	. set:(0=$random(4)) joinTypes($increment(joinTypes))="LEFT OUTER"
	. set:(0=$random(2)) joinTypes($increment(joinTypes))="RIGHT"
	. set:(0=$random(4)) joinTypes($increment(joinTypes))="RIGHT OUTER"
	. ; -------------------------------------------------------------------------------------------------
	. ; Need to disable FULL JOIN for now as Postgres gives the following error
	. ;	FULL JOIN is only supported with merge-joinable or hash-joinable join conditions
	. ; Don't think the below code will ever be enabled as it seems to be a permanent Postgres feature.
	. ; -------------------------------------------------------------------------------------------------
	. ; set joinTypes($increment(joinTypes))="FULL"
	. ; set joinTypes($increment(joinTypes))="FULL OUTER"
	. ; -------------------------------------------------------------------------------------------------
	. ; It is possible in rare cases no join gets chosen. To avoid that, choose INNER JOIN always.
	. set joinTypes($increment(joinTypes))="INNER"
	. set:(0=$random(2)) joinTypes($increment(joinTypes))="CROSS"
	. ; If there is more than 1 JOIN it is possible a NATURAL JOIN follows the FROM clause and another JOIN
	. ; in which case it is possible for column names to be duplicated (which is an error for a NATURAL JOIN).
	. ; Hence disable NATURAL JOIN in case joinCount is greater than 1.
	. set:(joinCount'>1) joinTypes($increment(joinTypes))="NATURAL"
	. if $increment(joinTypes)
	quit joinTypes($random(joinTypes))

tf()
	if (initDone("tf")=0) do
	. set initDone("tf")=1
	. set tf=-1
	. set tf($increment(tf))="TRUE"
	. set tf($increment(tf))="FALSE"
	. if $increment(tf)
	quit tf($random(tf))

; The "aas" ancronym stands for ANY, ALL, and SOME. This function returns one
; of the three words, and was written to prevent unneccesary code duplication.
anyAllSome()
	if (initDone("aas")=0) do
	. set initDone("aas")=1
	. set aas=-1
	. set aas($increment(aas))="ANY"
	. set aas($increment(aas))="ALL"
	. set aas($increment(aas))="SOME"
	. if $increment(aas)
	quit aas($random(aas))

; #FUTURE_TODO: Handle nulls in aggregrate functions, worry about
;               this when outer joins are reimplemented
;               http://www.sqlsnippets.com/en/topic-12656.html
returnAggregateFunction(queryDepth,table,column)
	new result,function,type
	set type=$$returnColumnType(table,column)
	if (type="VARCHAR") do
	. ; This is a VARCHAR type. If inner query do not use COUNT aggregate function as it will pose problems if this
	. ; gets inherited from a subquery select column list into an outer query (because COUNT(col) will be an integer
	. ; whereas col would be of VARCHAR type and those two cannot be compared in higher level query generation clauses).
	. for  set function=aggFunction($random(aggFunction))  quit:('queryDepth)!("COUNT"'=function)
	. write "$$returnAggregateFunction() : table = ",table," : column = ",column," : type is VARCHAR : function = ",function,!
	else  if (type'="BOOLEAN") do
	. set function=aggFunctionInt($random(aggFunctionInt))
	. write "$$returnAggregateFunction() : table = ",table," : column = ",column," : type is NOT VARCHAR : function = ",function,!
	else  do
	. set function=aggFunctionBool($random(aggFunctionBool))
	. write "$$returnAggregateFunction() : table = ",table," : column = ",column," : type is NOT VARCHAR : function = ",function,!
	set result=function_"("_aggModifier($random(aggModifier))_table_"."_column_")"
	; Coerce result of COUNT to BOOLEAN when the specified column is of BOOLEAN type
	set:"BOOLEAN"=type result=result_" > 0"
	quit result

returnSimilarAggregateFunction(isAggrFuncColumn,table,column)
	new result,function
	if "COUNT"=isAggrFuncColumn set function="COUNT"
	else  if "SUM"=isAggrFuncColumn set function="SUM"
	else  if "AVG"=isAggrFuncColumn do
	. new rand
	. set rand=$random(3)
	. set function=$select(0=rand:"MIN",1=rand:"MAX",1:"AVG")
	else  do
	. do assert(("MIN"=isAggrFuncColumn)!("MAX"=isAggrFuncColumn))
	. new rand
	. set rand=$random(2)
	. set function=$select(0=rand:"MIN",1:"MAX")
	; Fix rare query failure where `AVG()` would return a different number of precision digits in postgres
	; even for the same values by rounding to the same number of digits.
	; See https://gitlab.com/YottaDB/DBMS/YDBOcto/-/merge_requests/716#note_386868357
	if "AVG"=function do
	. set function="ROUND("_function
	set result=function_"("_aggModifier($random(aggModifier))_table_"."_column_")"
	if "ROUND(AVG"=function do
	. set result=result_", 11)"
	quit result

; #FUTURE_TODO: Eventually increase the depth that CASEs can occur, currently a single CASE can be nested inside of a single CASE
; Valid "location" parameters are "SELECT LIST", "ORDER BY", "WHERE", "HAVING"
; "SELECT LIST" generates a CASE statement that matches the requirements for SELECT LIST (i.e. an alias)
; "ORDER BY" generates a CASE statement that matches the requirements for ORDER BY
; "WHERE" generates a CASE statement that matches the requirements for WHERE
; "HAVING" generates a CASE statement that matches the requirements for HAVING
; Currently "ORDER BY", "WHERE", and "HAVING" all follow very similar paths through the code
;
; Valid "conditionType" parameters are "randomNumbers", "lrComparison"
; "randomNumbers" simply compares two randomly selected integers between 0 and 4
; "lrComparison" compares two randomly selected items named "leftCaseArg" and "rightCaseArg",
;     which need to be declared right before the call to this function
;
; Valid "resultType" parameters are "numbers", "columns"
; "numbers" set the resulting value to the value of "i" (the loop control variable)
; "columns" set the resulting value to a random column selected from a provided table
;     named "table" which needs to be declared right before the call to this function
;
; Valid "subqueryBoolean" parameters are "TRUE" and "FALSE", run with "TRUE" if CASE
; statement is to occur within a subquery, or "FALSE" if CASE statement is to occur
; if
; in the parent query
returnCaseFunction(location,conditionType,resultType,subqueryBoolean,toCompare)
	new result,whenCount,i,condition,r,limiter
	if (caseFunctionExists="FALSE")  set result="CASE"_$char(10)
	if (caseFunctionExists="TRUE")  set result="  CASE"_$char(10)

	set whenCount=$random(4)+1 ; 1-5
	set limiter=1

	; Limit the amount of nesting that can be done with CASEs
	for i=1:1:whenCount  do
	. set condition=""
	. if (conditionType="randomNumbers")  set condition=$$comparisonOperators($random(5),$random(5))
	. if (conditionType="lrComparison")  set condition=$$comparisonOperators(leftCaseArg,rightCaseArg)
	.
	. set r=""
	. if (resultType="numbers")  set r=i
	. if (resultType="columns")&(subqueryBoolean="FALSE") do
	. . set mainType=$$returnColumnType(table,$piece(toCompare,".",2))
	. . set otherType=""
	. . for  do  quit:(mainType=rType)
	. . . set r=$$chooseColumn(table)
	. . . set rType=$$returnColumnType(table,r)
	. . set r=table_"."_r
	. if (resultType="columns")&(subqueryBoolean="TRUE") do
	. . ; #FUTURE_TODO : innerTable seems to be used below but inherited from parent. Pass it as parameter or derive it here.
	. . if ($find($piece(toCompare,"."),"alias")'=0) do
	. . . set mainType=$$returnColumnType(innerTable,$piece(toCompare,".",2))
	. . else  do
	. . . set mainType=$$returnColumnType($piece(toCompare,"."),$piece(toCompare,".",2))
	. . set otherType=""
	. . for  do  quit:(mainType=rType)
	. . . ;for  do  quit:($data(caseLVN("alias"_aliasNum,location)=11))
	. . . set r=$$chooseColumn(innerTable)
	. . . set rType=$$returnColumnType(innerTable,r)
	. . . ;set caseLVN("alias"_aliasNum,location)=r
	. . set r="alias"_aliasNum_"."_r
	.
	. ; About 1/10 of the time nest another CASE into the current CASE
	. if ('($random(10))&(caseFunctionExists="FALSE")&(location'="WHERE"))  do
	. . set caseFunctionExists="TRUE"
	. . set result=result_"  WHEN "_condition_" THEN "_$char(10)_$$returnCaseFunction(location,conditionType,resultType,subqueryBoolean,toCompare)
	. else  do
	. . if (caseFunctionExists="FALSE")  set result=result_"  WHEN "_condition_" THEN "_r_$char(10)
	. . if (caseFunctionExists="TRUE")  set result=result_"    WHEN "_condition_" THEN "_r_$char(10)
	.
	. if (limiter=15)  set result=""

	if ($random(2))  do
	. if $increment(i)
	. if (caseFunctionExists="FALSE")  set result=result_"  ELSE "_r_$char(10)
	. if (caseFunctionExists="TRUE")  set result=result_"    ELSE "_r_$char(10)

	if (location="SELECT LIST")  do
	. if (caseFunctionExists="FALSE")  if $increment(aliasNum)  set result=result_"END AS alias"_aliasNum
	. if (caseFunctionExists="TRUE")  set result=result_"  END"_$char(10)
	else  do
	. if (caseFunctionExists="FALSE")  set result=result_"END"
	. if (caseFunctionExists="TRUE")  set result=result_"  END"_$char(10)

	set caseFunctionExists="FALSE"

	quit result

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Queries
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This function is responsible for the actual generation of the parent query, and such contains most of the logic to do so.
generateQuery(queryDepth,joinCount)
	new fc,i,query,joinclause
	set selectListDepth=$random(GLOBALtotalTables)+1

	set query="SELECT "
	set query=query_$$setQuantifier
	set fc=$$fromClause ; fromClause and joinClause needs to run before selectList
	; If testing with "names" database, then we could later randomly replace "names" table with VALUES clause
	; In that case, it would be a sub-query. So give it a name in the outer query using the AS keyword.
	set:(sqlFile="names.sql") fc=fc_" AS names"
	set joinclause="" for i=1:1:joinCount  set joinclause=joinclause_$$joinClause(queryDepth,joinCount)  if $increment(aliasNum)
	set outerSelectList=$$selectList(queryDepth,selectListDepth)
	set query=query_outerSelectList
	set query=query_" "_fc_" "_joinclause
	set query=query_$$tableExpression(queryDepth)
	; If testing with the "names" database, randomly (1/8 chance) replace occurrences of "names" table with the VALUES clause.
	; This provides a good test of the VALUES clause.
	do:$random(2)&("names.sql"=sqlFile)
	. new wordcount
	. set wordcount=$length(query," ")
	. for i=1:1:wordcount do
	. . set:('$random(8))&(($piece(query," ",i)="names")!($piece(query," ",i)="NAMES")) $piece(query," ",i)=$$valuesClause()
	. . set:$piece(query," ",i+1)="AS" i=i+2
	quit query_";"

; This function generates all of the subqueries for the various other functions throughout this program.
; Valid parameters are "full" and "limited"
; Passing "full" returns a very general query with minimal restrictions
; Passing "limited" returns a query that contains clauses that limit the
; query to only return a single row, and single column
generateSubQuery(queryDepth,subQueryType,joinType)
	do assert(enableSubQuery)
	new innerFC,alias,innerQuery
	new tableColumnSave,selectListLVNSave,tableAliasSave,groupByListLVNSave,groupByColumnLVNSave,selectedExprGroupBySave,groupByComparisonExpressionExistsSave
	if $increment(aliasNum)
	set alias="alias"_aliasNum

	if $increment(queryDepth)	; increment the query depth for each sub-query

	set allowGBH("alias"_aliasNum)="FALSE"
	if '($random(5))  set allowGBH("alias"_aliasNum)="TRUE"
	; If we are generating the table as a subquery inside a NATURAL JOIN, we do not want to use aggregate functions as
	; we could then end up with two columns on either side of the NATURAL JOIN with the same name but different types
	; (e.g. first_name and COUNT(first_name) would have same name but different types).
	if ("NATURAL"=joinType) set allowGBH("alias"_aliasNum)="FALSE"

	; Do not let sub-query affect outer query global state. So save state
	merge tableColumnSave=tableColumn
	merge selectListLVNSave=selectListLVN
	merge tableAliasSave=tableAlias
	merge groupByListLVNSave=groupByListLVN
	merge groupByColumnLVNSave=groupByColumnLVN
	merge selectedExprGroupBySave=selectedExprGroupBy
	merge groupByComparisonExpressionExistsSave=groupByComparisonExpressionExists
	set innerQuery="SELECT "
	set innerQuery=innerQuery_$$setQuantifier
	set innerFC=$$fromClause ; fromClause needs to run before selectList
	; Need to add this to tableAlias array as later calls (e.g. $$innerSelectList etc.) rely on this
	set tableAlias(alias)=$piece(innerFC," ",2)
	write "generateSubQuery : set tableAlias(",alias,")=",tableAlias(alias),!

	if (subQueryType="full")  set innerQuery=innerQuery_$$innerSelectList(queryDepth,subQueryType,$random(3)+1,alias)
	if (subQueryType="limited")  set innerQuery=innerQuery_$$innerSelectList(queryDepth,subQueryType,0,alias)
	set innerQuery=innerQuery_" "_innerFC_" "_alias
	set innerQuery=innerQuery_$$innerTableExpression(queryDepth,subQueryType)

	; And restore outer query global state
	kill tableColumn,selectListLVN,tableAlias,groupByListLVN,groupByColumnLVN,selectedExprGroupBy,groupByComparisonExpressionExists
	merge tableColumn=tableColumnSave
	merge selectListLVN=selectListLVNSave
	merge tableAlias=tableAliasSave
	merge groupByListLVN=groupByListLVNSave
	merge groupByColumnLVN=groupByColumnLVNSave
	merge selectedExprGroupBy=selectedExprGroupBySave
	merge groupByComparisonExpressionExists=groupByComparisonExpressionExistsSave

	w "generateSubQuery() innerQuery=",innerQuery,!
	quit innerQuery

; #FUTURE_TODO: Try to make as many of these the same as their non-subquery counterparts
;               innerSelectList is almost the same as selectList (Different logic, but same idea)
;               innerTableExpression is almost the same as tableExpression
;               innerWhereClause is almost the same as whereClause
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;Inner Specific Versions of Other Functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; https://ronsavage.github.io/SQL/sql-92.bnf.html#select%20list
; This function returns an select list that matches requirements and utilizes the variables of subqueries.
innerSelectList(queryDepth,subQueryType,curDepth,alias)
	new result,toBeAdded,aliastable,i
	;This function serves the same purpose as select sublist in the grammar rules for SQL.
	write "innerSelectList() :queryDepth=",queryDepth," subQueryType=",subQueryType," curDepth=",curDepth," alias=",alias,!

	do assert(0'=+queryDepth)
	set result=""

	set aliastable=$piece(innerFC," ",2)
	; Qualifier notation (table.column), with the value of the alias variable instead of a table
	set toBeAdded=alias_"."_$$chooseColumn(aliastable)

	if ((subQueryType="full")&(allowGBH("alias"_aliasNum)="FALSE"))  do
	. for i=1:1:15  quit:($data(selectListLVN(queryDepth,toBeAdded))=0)  do
	. . set toBeAdded=alias_"."_$$chooseColumn(aliastable)
	. if (i'=15)  do
	. . set selectListLVN(queryDepth,toBeAdded)="table.column"
	. . do addToInnerQuerySLLVN($piece(toBeAdded,"."),$piece(toBeAdded,".",2),aliastable)
	. . set result=result_toBeAdded
	.
	. if (i=15)  set curDepth=0  set result=$extract(result,0,$length(result)-1)


	if ((subQueryType="full")&(allowGBH("alias"_aliasNum)="TRUE"))  do
	. new table,chosenColumn,agg,chosenColumn2,tc
	. ; #FUTURE_TODO: Remove following line when issues (both in Octo and in test_helpers)
	. ;               pertaining to aggregate functions are resolved
	. set curDepth=0
	. set table="alias"_aliasNum
	. set chosenColumn=$$chooseColumn(table)
	. if enableGroupByHavingClause do
	. . ; Adds aggregate column with an alias whose name is same as the actual column
	. . ; Example: `SELECT MIN(alias1.lastname) as lastname,..`
	. . set agg=$$returnAggregateFunction(queryDepth,table,chosenColumn)
	. . set selectListLVN(queryDepth,agg)="aggregate_function"
	. . do addToInnerQuerySLLVN(table,chosenColumn,$piece(innerFC," ",2))
	. . set toBeAdded=agg_" as "_chosenColumn_", "
	. . set result=result_toBeAdded
	.
	. new setColumn
	. if ($$columnCounter(table)>1)  do
	. . ; Note: In tables that have only 2 columns (e.g. `stock_availability` table in `boolean.sql`)
	. . ; it is possible that the randomly chosen "chosenColumn2" ends up being the same as "chosenColumn"
	. . ; for a lot of iterations (we have seen "i" go as high as up to 16 and fail an "assert(i<16)" below).
	. . ; Hence the use of "64" below which should make it almost impossible for the for loop to assert fail.
	. . for i=1:1 do  quit:(chosenColumn'=chosenColumn2)  do assert(i<64)
	. . . set chosenColumn2=$$chooseColumn(table)
	. . set setColumn=1
	. else  if (0=$get(innerQuerySLLVN,0)) do
	. . ; It is necessary to check `($get(innerQuerySLLVN,0))=0)` as its possible that the aggregate addition code in this
	. . ; routine generates an aggregate function with the same alias as the actual column. In this case if a table with
	. . ; only one column is being used, not checking `innerQuerySLLVN` before addition of a column in else block, we will
	. . ; end up with an ambiguous column name like `RIGHT OUTER JOIN (SELECT ALL MIN(DISTINCT alias1.lastname) as lastname,
	. . ;  alias1.lastname FROM nameslastname alias1 ..`. Here both columns in select list of inner query have the same
	. . ; name which results in ambiguity when referenced by outer query.
	. . do assert(result="")
	. . set chosenColumn2=$$chooseColumn(table)
	. . set setColumn=1
	. if $data(setColumn) do
	. . set tc=table_"."_chosenColumn2
	. . set selectListLVN(queryDepth,tc)="table.column"
	. . do addToInnerQuerySLLVN($piece(tc,"."),$piece(tc,".",2),$piece(innerFC," ",2))
	. . set result=result_tc

	if (subQueryType="limited")  do
	. set selectListLVN(queryDepth,toBeAdded)="table.column"
	. set innerQuerySLLVN($piece(toBeAdded,"."),$piece(toBeAdded,".",2))=$piece(innerFC," ",2)
	. set result=result_toBeAdded

	; #FUTURE_TODO: Move the recursion logic down here
	if ((subQueryType="full")&(curDepth>0)&(allowGBH("alias"_aliasNum)="FALSE"))  do
	. if $increment(curDepth,-1) ; to drop down a "level" in depth
	. if (curDepth'=0) set result=result_", "_$$innerSelectList(queryDepth,subQueryType,curDepth,alias)

	; strip off the ", " at the end of result, only if ", " is present in result
	if ($extract(result,$length(result)-1,$length(result))=", ")  set result=$extract(result,0,$length(result)-2)

	quit result

; https://ronsavage.github.io/SQL/sql-92.bnf.html#table%20expression
; This function returns the table expressions that matches requirements and utilizes the variables of subqueries.
; It only contains code to return a subquery specific WHERE CLAUSE,
; and regular/general GROUP BY, HAVING, LIMIT, and ORDER BY clauses.
innerTableExpression(queryDepth,subQueryType)
	new innerResult,aliasNumSave
	; We only want columns in select list of subquery to be added to innerQuerySLLVN.
	; Not columns that are in select list of grand-subqueries in say the WHERE clause of the subquery.
	; Hence the new below.
	new innerQuerySLLVN
	; From Clause should go here, but it needs to be decided on early as to
	; allow for proper column(s) to be chosen
	set innerResult=""
	set aliasNumSave=aliasNum	; save aliasNum before it gets potentially modified by $$innerWhereClause call below
	set:enableInnerWhereClause innerResult=innerResult_$$innerWhereClause(queryDepth)
	if (enableGroupByHavingClause&(allowGBH("alias"_aliasNumSave)="TRUE")&(subQueryType="full")) do
	. set innerResult=innerResult_$$groupbyClause(queryDepth)
	. set innerResult=innerResult_$$havingClause(queryDepth,"subquery")
	if (subQueryType="full") do
	. ; We have to use ORDER BY in any inner query that also has LIMIT clause used (or else different rows would get returned
	. ; as part of this subquery which would affect the final output of the outer query) . Hence the below "orderbyClause"
	. ; invocation is based on "enableLimitClause" instead of on "enableOrderByClause".
	. set:(enableOrderByClause!enableLimitClause) innerResult=innerResult_$$orderbyClause(queryDepth,aliasNumSave,"SUBQUERY")
	. set:enableLimitClause innerResult=innerResult_$$limitClause
	if (subQueryType="limited") do
	. ; We have to unconditionally use ORDER BY and LIMIT clause in below query (or else we would get errors in query
	. ; as this is an inner level query which should return a scalar).
	. ; Hence the lack of use of 'enableOrderByClause' and 'enableLimitClause' variables.
	. set innerResult=innerResult_$$orderbyClause(queryDepth,aliasNumSave,"SUBQUERY")_" LIMIT 1"

	quit innerResult

; This function updates `randInt` based on the value of `columnType`.
; If the table given by `table` doesn't have a column of type `columnType` the randInt value is set to 0.
; This routine is used by innerWhereClause() and whereClause() to avoid duplicate code to change `randInt` based
; on column type.
checkAndChangeCaseIfColumnTypeAbsent(table,columnType,randInt)
	new x,type
	set type=""
	set x="sqlInfo("""_table_""")"
	set x=$query(@x)
	for i=1:1  do  quit:(($find(type,columnType)'=0)!(x=""))  do assert(i<16)
	. set type=$qsubscript(x,4)
	. if ($qsubscript(x,1)'=table)  set randInt=0
	. set x=$query(@x)
	set:x="" randInt=0
	quit

; https://ronsavage.github.io/SQL/sql-92.bnf.html#where%20clause
; This function returns a WHERE CLAUSE specific to subqueries.
; This is due to the fact that the WHERE clause in a subquery needed a lot more logic to properly run,
; and select/use proper elements within.
innerWhereClause(queryDepth)
	do assert(enableInnerWhereClause)
	new result,randInt,i,x,innerTable
	set result=" WHERE "
	; #FUTURE_TODO: Increase below to 12 when $$returnCaseFunction infinite loop and malformed query issues are resolved
	set randInt=$random(10) ; 0-9 ; Increase this range as new versions of WHERE clauses are added
	;
	; With the Northwind data set, we have seen at least one query with EXISTS in the inner WHERE clause take a long time
	; in Postgres (takes 20 minutes to run while Octo takes 1 second to run). So avoid subquery generation in that case.
	set:(sqlFile["northwind")&((randInt=5)!(randInt=9)) randInt=0

	; If enableSubQuery is FALSE, randInt=5 and randInt=9 cannot be allowed as they invoke "generateSubQuery"
	; So in those cases, set randInt=0 instead.
	set:('enableSubQuery)&((randInt=5)!(randInt=9)) randInt=0

	set innerTable=$piece(innerFC," ",2)

	; When randInt=1 or 3 or 4 a INTEGER type column is necessary in the selected table,
	; this code block ensures that this requirement is satisfied, and if it isn't
	; then set randInt to a different value.
	; WHERE clause type 1 is integer function expression
	; WHERE clause type 3 is integer comparison expression
	; WHERE clause type 4 is arithmetic operation based comparison expression
	if ((randInt=1)!(randInt=3)!(randInt=4)) do checkAndChangeCaseIfColumnTypeAbsent(innerTable,"INTEGER",.randInt)

	; When randInt=2 or 8 a VARCHAR type column is necessary in the selected table,
	; this code block ensures that this requirement is satisfied, and if it isn't
	; then set randInt to a different value.
	; WHERE clause type 2 is string concatenation, which requires at least one VARCHAR in order to function
	; WHERE clause type 8 is LIKE with wildcards, this comparison can only occur on VARCHARs, not numeric/integer
	if ((randInt=2)!(randInt=8)) do checkAndChangeCaseIfColumnTypeAbsent(innerTable,"VARCHAR",.randInt)

	set type=""

	; Don't use a random generator that requires a BOOLEAN type column if it's not present
	; WHERE clause type 12 is boolean-type-column
	; WHERE clause type 13-16 are boolean operations
	if ((12<=randInt)&(randInt<=16)) do
	. set x="sqlInfo("""_table_""")"
	. for i=1:1:15  do  quit:(($find(type,"BOOLEAN")'=0)!(x=""))
	. . set x=$query(@x)
	. . if (x'="")  set type=$qsubscript(x,4)
	. . if ((x'="")&($qsubscript(x,1)'=table))  set randInt=0
	. set:((i=15)!(x="")) randInt=0

	if (randInt=0) do
	. new loopCount,i,leftSide,rightSide,chosenColumn,opened
	. set loopCount=$random(3)+1 ; 1-3
	. set opened="FALSE"
	. for i=1:1:loopCount do
	. . set chosenColumn=$$chooseColumn(innerTable)
	. . set leftSide=""
	. . set rightSide=""
	. . if $random(2)  set leftSide="alias"_aliasNum_"."_chosenColumn
	. . else  set leftSide=$$chooseEntry(innerTable,chosenColumn)
	. . if $random(2)  set rightSide="alias"_aliasNum_"."_chosenColumn
	. . else  set rightSide=$$chooseEntry(innerTable,chosenColumn)
	. .
	. . ; First portion of WHERE, no AND or OR
	. . if (i=1) set result=result_"("_$$notString_$$comparisonOperators(leftSide,rightSide)
	. . ; Following portions of WHERE, with AND or OR separators
	. . if (i'=1) do
	. . . set result=result_" "_$$booleanOperator_" "
	. . . if (($random(2))&(opened="FALSE"))  do
	. . . . set result=result_"("
	. . . . set opened="TRUE"
	. . . set result=result_$$notString_$$comparisonOperators(leftSide,rightSide)
	. . . if (($random(2))&(opened="TRUE"))  do
	. . . . set result=result_")"
	. . . . set opened="FALSE"
	. . if (i=loopCount)  set result=result_")"
	. . if ((i=loopCount)&(opened="TRUE"))  set result=result_")"

	if (randInt=1) do
	. new chosenColumn,plusMinus,i
	. set chosenColumn=$$getColumnOfType(innerTable,"INTEGER")
	. set plusMinus="+"
	. if $random(2) set plusMinus="-"
	. set result=result_"(("_"alias"_aliasNum_"."_chosenColumn_plusMinus_$$chooseEntry(innerTable,chosenColumn)_")="_$$chooseEntry(innerTable,chosenColumn)_")"

	if (randInt=2) do
	. ; Left side of this expression will always be forced to be a varchar,
	. ; the right side of this expression can be either varchar or integer
	. ; This is done as PostgreSQL requires at least one side of a string concatenation
	. ; to be a string/varchar and the other to not be.
	. new leftSide,rightSide,chosenColumn1,chosenColumn2,entry1,entry2,i
	.
	. set leftSide=""
	. set rightSide=""
	.
	. set chosenColumn1=$$getColumnOfType(innerTable,"VARCHAR")
	. set entry1=$$chooseEntry(innerTable,chosenColumn1)
	. set entry1=$extract(entry1,2,$length(entry1)-1)
	. set leftSide="alias"_aliasNum_"."_chosenColumn1
	. if $random(2)  set leftSide=$$chooseEntry(innerTable,chosenColumn1)
	.
	. set chosenColumn2=$$chooseColumn(innerTable)
	. set entry2=$$chooseEntry(innerTable,chosenColumn2)
	. set entry2=$extract(entry2,2,$length(entry2)-1)
	. set rightSide="alias"_aliasNum_"."_chosenColumn2
	. if $random(2)  set rightSide=$$chooseEntry(innerTable,chosenColumn2)
	.
	. set result=result_"("_$$comparisonOperators("("_leftSide_"||"_rightSide_")","'"_entry1_entry2_"'")_")"

	if (randInt=3) do
	. new chosenColumn,plusMinus,plusMinus2,i
	. ; ... WHERE customer_id=-(-3)
	. set chosenColumn=$$getColumnOfType(innerTable,"INTEGER")
	. set plusMinus="+"
	. if $random(2) set plusMinus="-"
	. set plusMinus2="+"
	. if $random(2) set plusMinus2="-"
	. set result=result_"("_$$comparisonOperators("alias"_aliasNum_"."_chosenColumn," "_plusMinus_"("_plusMinus2_$$chooseEntry(innerTable,chosenColumn)_")")_")"

	if (randInt=4) do
	. new chosenColumn,beginning,plusMinus,end,aOperator,i
	.
	. set chosenColumn=$$getColumnOfType(innerTable,"INTEGER")
	. set beginning="("_$$chooseEntry(innerTable,chosenColumn)_")"
	.
	. new plusMinus
	. set plusMinus="+"
	. if $random(2) set plusMinus="-"
	.
	. set endEntry=$$chooseEntry(innerTable,chosenColumn)
	.
	. ; Checks if the selected operator is division/modulo and then checks if the
	. ; divisor is 0, if it is 0, it then forces it to a non zero number (1)
	. set aOperator=$$arithmeticOperator
	. if (((aOperator="/")!(aOperator="%"))&(endEntry=0))  set endEntry=1
	.
	. set end=plusMinus_"("_plusMinus_endEntry_")"
	.
	. set result=result_"("_$$comparisonOperators("("_beginning_" "_aOperator_" "_end_") ","alias"_aliasNum_"."_chosenColumn)_")"

	if (randInt=5) do
	. new alias
	. ; ... WHERE EXISTS (SELECT ... query)
	. set result=result_$$notString_"EXISTS ("_$$generateSubQuery(queryDepth,"full","")_")"

	if (randInt=6) do
	. new chosenColumn,notString,entryList,i
	. set chosenColumn=$$chooseColumn(innerTable)
	. set entryList=""
	. set limit=($random($$maxIndex(innerTable)-1)+1)
	. if (limit>8)  set limit=8
	. for i=1:1:limit do
	. . set entryList=entryList_$$chooseEntry(innerTable,chosenColumn)_", "
	. ; strip the ", " off of entryList
	. set entryList=$extract(entryList,0,$length(entryList)-2)
	. set notString=$select($random(2):"NOT ",1:"")	; cannot use $$notString here because "NOT NOT IN" is not valid syntax
	. set result=result_"alias"_aliasNum_"."_chosenColumn_" "_notString_"IN ("_entryList_")"

	if (randInt=7) do
	. new chosenColumn
	. set chosenColumn=$$chooseColumn(innerTable)
	. ; ... WHERE column BETWEEN entry1 and entry2
	. set result=result_"alias"_aliasNum_"."_chosenColumn_" BETWEEN "_$$chooseEntry(innerTable,chosenColumn)_" AND "_$$chooseEntry(innerTable,chosenColumn)

	if (randInt=8) do
	. new chosenColumn,string,i,randInt,i
	. ; Forces column to be of type "VARCHAR" as to do a string comparison
	. set chosenColumn=$$getColumnOfType(innerTable,"VARCHAR")
	. set string=""
	. set desiredStringLength=$random(7)+1 ; 1-7
	. for i=1:1:desiredStringLength do
	. . set randInt=$random(9) ; 0-8
	. . if ((randInt=0)!(randInt=1)) set string=string_$char($random(27)+64) ; @,A-Z
	. . if ((randInt=2)!(randInt=3)) set string=string_$char($random(26)+97) ; a-z
	. . if (randInt=4) set string=string_$random(10) ; 0-9
	. . if (randInt=5) set string=string_"#"
	. . if (randInt=6) set string=string_"$"
	. . if (randInt=7) set string=string_"%"
	. . if (randInt=8) set string=string_"_"
	. set result=result_"alias"_aliasNum_"."_chosenColumn_" LIKE '"_string_"'"

	if (randInt=9) do
	. new randInt,word,leftSide,rightSide,chosenColumn,entryList,limit,rightType,nextAliasNum,i
	. set word=$$anyAllSome
	.
	. set nextAliasNum=aliasNum+1
	. set rightSide="("_$$generateSubQuery(queryDepth,"limited","")_")"
	. set aliasDotColumn=""
	. for i=1:1  do  quit:($find(aliasDotColumn,"alias"_nextAliasNum)'=0)  do assert(i<16)
	. . set aliasDotColumn=$piece(rightSide," ",i)
	. set rightColumn=$piece($piece(aliasDotColumn,".",2),",",1)
	.
	. set holder=""
	. for i=1:1  do  quit:($find(holder,"FROM")'=0)  do assert(i<16)
	. . set holder=$piece(rightSide," ",i)
	. set rightTable=$piece(rightSide," ",i+1)
	.
	. set rightType=$$returnColumnType(rightTable,rightColumn)
	.
	. for i=1:1:15  do  quit:(leftType=rightType)  do assert(i<16)
	. . set leftSide=$$chooseColumn(innerTable)
	. . set leftType=$$returnColumnType(innerTable,leftSide)
	.
	. set leftSide="alias"_(nextAliasNum-1)_"."_leftSide
	.
	. if (i'=15) do
	. . set result=result_$$comparisonOperators(leftSide,word_" "_rightSide)
	. else  set result=""

	if (randInt=10) do
	. new toCompare
	. set toCompare=$random(4)+1
	. set result=result_toCompare_" = "_$$returnCaseFunction("WHERE","randomNumbers","numbers","TRUE",toCompare)

	if (randInt=11) do
	. new leftCaseArg,rightCaseArg,i,toCompare
	. set leftColumn=$$chooseColumn(innerTable)
	.
	. set leftType=$$returnColumnType(innerTable,leftColumn)
	. for i=1:1:15  do  quit:(leftType=rightType)  do assert(i<16)
	. . set rightColumn=$$chooseColumn(innerTable)
	. . set rightType=$$returnColumnType(innerTable,rightColumn)
	.
	. set leftCaseArg="alias"_aliasNum_"."_leftColumn
	. if ($random(2))  set leftCaseArg=$$chooseEntry(innerTable,leftColumn)
	. set rightCaseArg="alias"_aliasNum_"."_rightColumn
	. if ($random(2))  set rightCaseArg=$$chooseEntry(innerTable,rightColumn)
	.
	. if (i'=15) do
	. . set toCompare="alias"_aliasNum_"."_$$chooseColumn(innerTable)
	. . set result=result_toCompare_" = "_$$returnCaseFunction("WHERE","lrComparison","columns","TRUE",toCompare)
	. else  set result=""

	; When randInt=12 a BOOLEAN type column is necessary in the selected table,
	; this code block ensures that this requirement is satisfied, and if it isn't
	; then set randInt to a different value.
	; WHERE clause type 999 is just "WHERE boolean-type-column"
	if (randInt=12)  do
	. set result=result_table_"."_$$getColumnOfType(table,"BOOLEAN")

	; WHERE NOT booleanColumn, WHERE booleanColumn=TRUE/FALSE
	if ((13<=randInt)&(randInt<=15)) do
	. new i
	. set result=result_table_"."_$$getColumnOfType(table,"BOOLEAN")
	. set chosenColumn=table_"."_$$getColumnOfType(innerTable,"BOOLEAN")
	. set:13=randInt result=result_$$notString_chosenColumn
        . set:14=randInt result=result_chosenColumn_" = TRUE"
        . set:15=randInt result=result_chosenColumn_" = FALSE"

	; Boolean expressions can be combined to create more complex Booleans
	; Example: ((id = 1) OR (firstname = 'Zero')) AND (lastname '= 'Cool')
	; #FUTURE_TODO: Maybe combine this with WHERE clause version #0 (randInt=0)
	if (randInt=16) do
	. new leftSide,rightSide
	. set chosenColumn=$$chooseColumn(innerTable)
	. set leftSide=""
	. set rightSide=""
	. if ($random(2))  set leftSide=table_"."_chosenColumn
	. else  set leftSide=$$chooseEntry(innerTable,chosenColumn)
	. if ($random(2))  set rightSide=table_"."_chosenColumn
	. else  set rightSide=$$chooseEntry(innerTable,chosenColumn)
	. set result=result_"("_$$comparisonOperators(leftSide,rightSide)_" = "_$$tf_")"

	quit result

getColumnOfType(table,neededType)
	new i,type,chosenColumn,origTable,maxcols,availcols
	set origTable=table,table=$$getRealTable(table)
	set maxcols=tableColumnCounts(table),availcols=0
	for i=1:1:maxcols set col=$order(columns(table,i,"")) quit:""=col  do
	. if columns(table,i,col)=neededType set availcols(availcols)=col if $incr(availcols)
	do assert(0<availcols)
	quit availcols($random(availcols))

getColumnType(table,column)
	new maxcols,col
	set maxcols=tableColumnCounts(table)
	for i=1:1:maxcols set col=$order(columns(table,i,"")) quit:col=column  do
	. set col=$select(col="INT":"INTEGER",1:col)
	do assert(i<maxcols)
	quit columns(table,i,col)

getType(typestr)
	new type
	set type=$piece(typestr,"(",1)	; e.g. VARCHAR(32) -> VARCHAR
	set:"TEXT"=type type="VARCHAR"	; convert TEXT -> VARCHAR
	set:"INT"=type type="INTEGER"	; convert INT -> INTEGER
	quit type

getRealTable(table,column)
	new realTable
	if $data(column)&$data(columnAlias(table,column)) set realTable=columnAlias(table,column)
	else  set realTable=$select($data(tableAlias(table)):tableAlias(table),1:table)
	quit realTable

chooseCount();
	; This function is invoked when one needs a data point for comparison against the COUNT aggregate function.
	; For now return a random number between 0 and 7.
	quit $random(8)

getRandFuncInt(colEntry)
	quit $$getRandFunc(colEntry,"INTEGER")

getRandFuncStr(colEntry)
	quit $$getRandFunc(colEntry,"VARCHAR")

getRandFunc(colEntry,type)
	quit:("NULL"=colEntry) colEntry
	quit "samevalue("_colEntry_")"

valuesClause()
	new valueClause,ret
	set valueClause="(values (0, 'Zero', 'Cool'), (1, 'Acid', 'Burn'), (2, 'Cereal', 'Killer'), (3, 'Lord', 'Nikon'), (4, 'Joey', NULL), (5, 'Zero', 'Cool'))"
	set ret="(select * from "_valueClause_" as names(id,firstname,lastname))"
	quit ret

getRandInt()
	new ret
	set ret=$random(100)
	set:(ret=0) ret=1	; Just to avoid divide by zero errors
	quit $random(100)

getRandStr()
	new string,i,randInt,desiredStringLength
	set string=""
	set desiredStringLength=$random(7)+1 ; 1-7
	for i=1:1:desiredStringLength do
	. set randInt=$random(9) ; 0-8
	. if ((randInt=0)!(randInt=1)) set string=string_$char($random(27)+64) ; @,A-Z
	. if ((randInt=2)!(randInt=3)) set string=string_$char($random(26)+97) ; a-z
	. if (randInt=4) set string=string_$random(10) ; 0-9
	. if (randInt=5) set string=string_"#"
	. if (randInt=6) set string=string_"$"
	. if (randInt=7) set string=string_"%"
	. if (randInt=8) set string=string_"_"
	quit "'"_string_"'"

getRandBoolean()
	quit $select(($random(2)=0):"TRUE",1:"FALSE")

; The below routine only executes if the outer most query OrderBy is being processed.
; If OrderBy list doesn't have all nodes from select list `orderByListLessThanSelectList` is set
;   1. In the below query although we do not have all nodes from Select List in OrderBy list we do not need
;      to set `orderByListLessThanSelectList` as the ordering will not differ because duplicate nodes don't effect ordering
;	  `SELECT NOT (Suppliers.Country > 'Denmark'), Suppliers.ContactName, COUNT(ALL Suppliers.City), Suppliers.ContactName ..
;	   .. ORDER BY COUNT(ALL Suppliers.City), NOT (Suppliers.Country > 'Denmark'), Suppliers.ContactName;`
;   2. The set `orderByListLessThanSelectList` is later used to determine if `--sort-needed-check` is required to be added to
;   query or not.
checkIfOrderByLessThanSelectList(queryDepth,location,orderByListSize)
	quit:((0'=queryDepth)!("QUERY"'=location))	; We only need to set orderByListLessThanSelectList for outer query OrderBy
	new innerHolder,selectListCounter
	set innerHolder="",selectListCounter=0 for  set innerHolder=$order(selectListLVN(queryDepth,innerHolder)) quit:""=innerHolder  do
	. if ("table.*"=selectListLVN(queryDepth,innerHolder)) do
	. . new tblStr,totalCol
	. . set tblStr=$piece(innerHolder,".",1)
	. . ; Check how many columns in table
	. . set totalCol=$$columnCounter(tblStr)
	. . ; Add it to counter
	. . if ($increment(selectListCounter,totalCol))
	. else  do
	. . if ($increment(selectListCounter))
	zwr selectListCounter
	set:(selectListCounter>orderByListSize) orderByListLessThanSelectList=1
	quit

; At present the following routine only checks from INTEGER and VARCHAR
getTableColumnTypes(table)
	new i,origTable,maxcols,ret,varcharColumns,intColumns,availcols,col
	set (varcharColumns,intColumns)=0
	set origTable=table,table=$$getRealTable(table)
	set maxcols=tableColumnCounts(table),availcols=0
	for i=1:1:maxcols set col=$order(columns(table,i,"")) quit:""=col  do
	. if columns(table,i,col)="VARCHAR" if $incr(varcharColumns)
	. if columns(table,i,col)="INTEGER" if $incr(intColumns)
	if ((0<varcharColumns)&(0<intColumns)) set ret="BOTH"
	else  if (0<varcharColumns) set ret="VARCHAR"
	else  if (0<intColumns) set ret="INTEGER"
	else  set ret="NONE"
	quit ret

returnUnaryExpression(queryDepth,table,constExpr)
	new randInt,ret,lclGroupByColumnLVN,lclgroupByComparisonExpressionExists
	set lclgroupByComparisonExpressionExists=0
	set ret=""
	set randInt=$random(3)

	if (0=randInt) do
	. ; NOT boolean expression
	. set ret="NOT"_" "_$$getSimpleBinaryExpression(queryDepth,table,.constExpr,1,"BOTH_COMPARISON",.lclGroupByColumnLVN,.lclgroupByComparisonExpressionExists)

	if (1=randInt) do
	. ; The following is does not lead to creation of a unary_STATEMENT but since NULL is constant and only the first argument
	. ; for this expression can be a variable part we included its generation under this routine itself.
	. ; IS NULL or IS NOT NULL
	. new str
	. if (1=$random(2)) set str="IS NULL"
	. else  set str="IS NOT NULL"
	. set ret="("_$$getSimpleBinaryExpression(queryDepth,table,.constExpr,1,"BOTH",.lclGroupByColumnLVN,.lclgroupByComparisonExpressionExists)_")"_" "_str
	. set lclgroupByComparisonExpressionExists=1 ; As both operation results in a boolean value

	if (2=randInt) do
	. ; + or -
	. new str
	. if (1=$random(2)) set str="+"
	. else  set str="-"
	. set ret=str_"("_$$getSimpleBinaryExpression(queryDepth,table,.constExpr,1,"INTEGER",.lclGroupByColumnLVN,.lclgroupByComparisonExpressionExists)_")"

	if ($increment(groupByListLVN(queryDepth,ret)))
	set groupByComparisonExpressionExists(queryDepth,ret)=lclgroupByComparisonExpressionExists
	new innerHolder
	set innerHolder="" for  set innerHolder=$order(lclGroupByColumnLVN(queryDepth,innerHolder))  quit:innerHolder=""  do
	. if ($increment(groupByColumnLVN(queryDepth,ret,innerHolder)))
	kill lclGroupByColumnLVN
	quit ret

getSimpleBinaryExpression(queryDepth,table,constExpr,sizeOfExpression,dataType,lclGroupByColumnLVN,lclgroupByComparisonExpressionExists)
	new randInt,ret,selectComp,columnTypes,expType,toBeAdded,isVariableGroupBy
	set isVariableGroupBy=0
	set ret=""
	set toBeAdded=""
	set (constExpr,selectComp)=0
	; If dataType is specified then this invocation is from returnUnaryExpression,
	;   select comparison operator only if the value is "BOTH_COMPARISON".
	; Otherwise, randomly select COMPARISON so that comparison operators is used.
	; Since comparison operators apply to BOOLEAN, VARCHAR and INTEGER, it can safely
	;   replace any later expType selection done in the following loop.
	if ("BOTH_COMPARISON"=$get(dataType,"")) set (selectComp,lclgroupByComparisonExpressionExists)=1
	else  set:((""=$get(dataType,""))&(0=$random(2))) (selectComp,lclgroupByComparisonExpressionExists)=1

	set randInt=$random(6)
	set columnTypes=$$getTableColumnTypes(table)
	set:("NONE"=columnTypes) randInt=2+$random(3)	; 2,3,4
	if ((randInt=0)!(randInt=1)!(randInt=5)) do
	. ; Set expType here so that all expression elements are of the same type.
	. ;   i.e. we do not want to form below type of expressions when columnTypes is `BOTH`
	. ;   `(samevalue(alias5.order_date) || '07/04/1776') + (samevalue(alias5.customer_id) * 3)`
	. ; Other cases do not rely on columnTypes so no need to set expType for them here.
	. new num
	. set num=$random(2)
	. if (columnTypes="VARCHAR") set expType="VARCHAR"
	. else  if (columnTypes="INTEGER") set expType="INTEGER"
	. else  if (columnTypes="BOTH") set expType=$select((num=0):"VARCHAR",(num=1):"INTEGER")
	. ; If dataType is set then this call is from getUnaryExpression
	. ; Try to get an expression having data type same as dataType value
	. if (""'=$get(dataType,"")) do
	. . ; `BOTH_COMPARISON` - operator needs to be COMPARISON with operands being of VARCHAR or INTEGER type
	. . ; `BOTH` - let operator be what is already set in the previous IF block
	. . ; `dataType=expType` - the selected operand type is same as expected
	. . ; If none of the above is true - the selected type and expected is different
	. . if ("BOTH_COMPARISON"=dataType) set (selectComp,lclgroupByComparisonExpressionExists)=1
	. . else  if ("BOTH"=dataType) ; do nothing
	. . else  if (dataType=expType) ; do nothing
	. . else  do
	. . . ; `expType` chosen is different from the expected type
	. . . if (columnTypes="BOTH") set expType=dataType	; Column of required type is in table so direct assignment is ok
	. . . else  do
	. . . . ; Select a constant expression because we do not have a column of expected type in the table
	. . . . set expType=dataType
	. . . . set:("VARCHAR"=dataType) randInt=3
	. . . . set:("INTEGER"=dataType) randInt=2

	if ((randInt=2)!(randInt=3)!(randInt=4)) do
	. if (dataType="INTEGER") set randInt=2	; Only possible for unary operation using + or - operators

	for i=1:1:sizeOfExpression do
	. new leftOpnd,rightOpnd,op,column,const,num,expr
	. if (0=randInt) do
	. . ; - single column expression
	. . new tblCol
	. . ; get a column
	. . set column=$$getColumnOfType(table,expType)
	. . set tblCol=table_"."_column
	. . ; get a constant based on expression type
	. . set const=$$chooseEntry(table,column)
	. . ; assign left and right operand
	. . set num=$random(2)
	. . set:(num) leftOpnd=const,rightOpnd=tblCol
	. . set:('num) rightOpnd=const,leftOpnd=tblCol
	. . if ($increment(lclGroupByColumnLVN(queryDepth,tblCol)))
	. . set isVariableGroupBy=1
	.
	. if (1=randInt) do
	. . ; - double column
	. . ; #FUTURE_TODO: Enhance this case to have having expressions of double column
	. . ;   For ex: (id!=1)AND(firstname='Cool')
	. . ;   At present the expression formed is similar to: (firstname!=lastname)
	. . new secondColumn
	. . ; get first column
	. . set column=table_"."_$$getColumnOfType(table,expType)
	. . ; get second column
	. . set secondColumn=table_"."_$$getColumnOfType(table,expType)
	. . ; assign left and right operand
	. . set num=$random(2)
	. . set:(num) leftOpnd=secondColumn,rightOpnd=column
	. . set:('num) rightOpnd=secondColumn,leftOpnd=column
	. . if ($increment(lclGroupByColumnLVN(queryDepth,column)))
	. . if ($increment(lclGroupByColumnLVN(queryDepth,secondColumn)))
	. . set isVariableGroupBy=1
	.
	. if (2=randInt) do
	. . ; - constant expressions (INT)
	. . set expType="INTEGER"
	. . set leftOpnd=$$getRandInt,rightOpnd=$$getRandInt
	. . set constExpr=1
	.
	. if (3=randInt) do
	. . ; - constant expressions (String)
	. . set expType="VARCHAR"
	. . set leftOpnd=$$getRandStr,rightOpnd=$$getRandStr
	. . set constExpr=1
	.
	. if (4=randInt) do
	. . ; - boolean constant expression (TRUE/FALSE)
	. . set expType="BOOLEAN"
	. . set leftOpnd=$$getRandBoolean
	. . set rightOpnd=$$getRandBoolean
	. . set constExpr=1
	.
	. if (5=randInt) do
	. . ; - with functions
	. . ; #FUTURE_TODO: Include paramless functions
	. . new func,tblCol
	. . ; get a column
	. . set column=$$getColumnOfType(table,expType)
	. . set tblCol=table_"."_column
	. . ; get a function based on type
	. . set func=$select((expType="VARCHAR"):$$getRandFuncStr(tblCol),(expType="INTEGER"):$$getRandFuncInt(tblCol))
	. . if ($increment(lclGroupByColumnLVN(queryDepth,tblCol)))
	. . set isVariableGroupBy=1
	. . if (0=$random(2)) set column=$$chooseEntry(table,column)
	. . else  do
	. . . set column=tblCol
	. . . if ($increment(lclGroupByColumnLVN(queryDepth,column)))
	. . . set isVariableGroupBy=1
	. . ; assign left and right operand
	. . set num=$random(2)
	. . set:(num) leftOpnd=func,rightOpnd=column
	. . set:('num) rightOpnd=func,leftOpnd=column
	.
	. ; #FUTURE_TODO: Include NULL based expressions
	. ; if (6=randInt) do
	. ; . ; - constant expressions (NULL)
	. ; . set constExpr=1
	.
	. ; Get an operator based on expression type (expType) and selectComp value
	. if (selectComp) set op=$$getComparisonOperatorAlone
	. else  if ("INTEGER"=expType) do
	. . new arithmeticOp
	. . set arithmeticOp=$$arithmeticOperator
	. . ; Avoid / and % operators as its hard to avoid divide by 0 in case of randomly formed expressions
	. . ;   involving columns and constants.
	. . set:(("/"=arithmeticOp)!("%"=arithmeticOp)) arithmeticOp="+"
	. . set op=arithmeticOp
	. else  set op=$select(("BOOLEAN"=expType):$$booleanOperator,("VARCHAR"=expType):"||")
	. ; combine left operand, operator and right operand
	. set expr=leftOpnd_" "_op_" "_rightOpnd
	.
	. ; Combine expressions if sizeOfExpression is > 1
	. if (""'=ret) do
	. . ; Get a new operator to combine previous and new expression.
	. . ; If the operands are having comparison operator then combine them using boolean/comparison operator.
	. . if (selectComp) do
	. . . if (sizeOfExpression<=2) do
	. . . . if (1=$random(2)) set op=$$booleanOperator
	. . . . else  set op=$$getComparisonOperatorAlone
	. . . else  do
	. . . . ; Comparison operator for a 3 operand or more operand expression doesn't work
	. . . . ;   For ex:`select ('%9_#_U#' > '$uc#') != ('$N' != 'w0j') = ('t##8#' < '5Y#um$');` gives an error.
	. . . . ;   So use boolean operators to combine such expressions
	. . . . set op=$$booleanOperator
	. . else  if ("INTEGER"=expType) do
	. . . new arithmeticOp
	. . . set arithmeticOp=$$arithmeticOperator
	. . . ; Avoid / and % operators as its hard to avoid divide by 0 in case of randomly formed expressions
	. . . ; Also, avoid * operaion as its hard to avoid reaching `integer out of range` error case
	. . . ;   involving columns and constants.
	. . . if (("/"=arithmeticOp)!("%"=arithmeticOp)!("*"=arithmeticOp)) do
	. . . . if (1=$random(2)) set arithmeticOp="+"
	. . . . else  set arithmeticOp="-"
	. . . set op=arithmeticOp
	. . else  set op=$select(("BOOLEAN"=expType):$$booleanOperator,("VARCHAR"=expType):"||")
	. . set ret=ret_" "_op_" ("_expr_")"
	. else  do
	. . set ret="("_expr_")"
	quit ret

returnBinaryExpression(queryDepth,table,constExpr)
	new ret,noOfExpr,lclGroupByColumnLVN,lclgroupByComparisonExpressionExists
	set lclgroupByComparisonExpressionExists=0
	; - Boolean expressions
	set noOfExpr=1+$random(8)	; Avoiding 0 value for noOfExpr
	set ret=$$getSimpleBinaryExpression(queryDepth,table,.constExpr,noOfExpr,"",.lclGroupByColumnLVN,.lclgroupByComparisonExpressionExists)
	if ($increment(groupByListLVN(queryDepth,ret)))
	set groupByComparisonExpressionExists(queryDepth,ret)=lclgroupByComparisonExpressionExists
	new innerHolder
	set innerHolder="" for  set innerHolder=$order(lclGroupByColumnLVN(queryDepth,innerHolder))  quit:innerHolder=""  do
	. if ($increment(groupByColumnLVN(queryDepth,ret,innerHolder)))
	kill lclGroupByColumnLVN
	quit ret

assert(cond)	;
	if 'cond zshow "*"  zhalt 1
	quit

notString()	;
	; Randomly returns one or more NOT operators (tests YDBOcto#788)
	quit:$random(2) ""		; return ""            50.000% of the time
	quit:$random(4) "NOT "		; return "NOT"         37.500% of the time
	quit:$random(4) "NOT NOT "	; return "NOT NOT"      9.375% of the time
	quit "NOT NOT NOT "		; return "NOT NOT NOT"  3.125% of the time

; The following routine adds the passed argumets to `innerQuerySLLVN` tree if it doesn't exists already and increments its value
addToInnerQuerySLLVN(table,column,alias)
	if (0=$data(innerQuerySLLVN(table,column))) do
	. set innerQuerySLLVN(table,column)=alias
	. if $increment(innerQuerySLLVN)
	quit

