%ydboctoodata ; YDB/SMH - Test Octo with OData
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	;
; All rights reserved.						;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
 ;
catalog(result,args) ; [Public] GET /octo/odata/v4
 new oldio s oldio=$io
 o "s":(shell="/bin/sh":comm="$ydb_dist/plugin/bin/octo")::"pipe"
 u "s"
 w "\d",!
 for i=0:1 read octodata(i):.1 quit:octodata(i)=""  quit:$zeof
 kill octodata(i) ; Last is the empty data since we read nothing
 kill octodata(i-1) ; (6 rows)
 u oldio c "s"
 new output
 set output("@odata.context")="https://"_HTTPREQ("header","host")_"/octo/odata/v4/$metadata"
 for i=0:0 set i=$order(octodata(i)) quit:'i  do
 . new tablename set tablename=$piece(octodata(i),"|",2)
 . set (output("value",i,"name"),output("value",i,"url"))=tablename
 . set output("value",i,"kind")="EntitySet"
 do encode^%webjson($name(output),$name(result))
 set result("mime")="application/json;odata.metadata=minimal;odata.streaming=true;IEEE754Compatible=false;charset=utf-8"
 quit
 ;
names(result,args) ; [Public] Get /octo/names
 new oldio s oldio=$io
 o "s":(shell="/bin/sh":comm="$ydb_dist/plugin/bin/octo")::"pipe"
 u "s"
 w "select * from names;",!
 for i=0:1 read octodata(i):.1 quit:octodata(i)=""  quit:$zeof
 kill octodata(i) ; Last is the empty data since we read nothing
 kill octodata(i-1) ; (6 rows)
 u oldio c "s"
 new columns
 new header set header=octodata(0)
 new columnNum for columnNum=1:1:$length(header,"|") set columns(columnNum)=$piece(header,"|",columnNum)
 new output
 set output("@odata.context")="/octo/odata/v4/names/$metadata"
 set output("@odata.nextLink")=""
 for i=0:0 set i=$order(octodata(i)) quit:'i  do
 . set output("value",i,"@odata.id")="/octo/names("_$piece(octodata(i),"|")_")"
 . set output("value",i,"@odata.etag")=$zysuffix(octodata(i))
 . set output("value",i,"@odata.editLink")=""
 . for columnNum=1:1:$length(octodata(i),"|") set output("value",i,columns(columnNum))=$piece(octodata(i),"|",columnNum)
 do encode^%webjson($name(output),$name(result))
 set result("mime")="application/json;odata.metadata=minimal;odata.streaming=true;IEEE754Compatible=false;charset=utf-8"
 quit
