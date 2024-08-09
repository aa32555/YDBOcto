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

; Implements the SQL DATE_ADD function
DATEADD(date,type,year,month,day,hour,minute,second,microsecond)
	quit

PostgreSQL(date,type,year,month,day,hour,minute,second,microsecond)
	quit $$AddInterval^%ydboctoplanhelpers(date,type,year,month,day,hour,minute,second,microsecond)

MySQL(date,type,year,month,day,hour,minute,second,microsecond)
	quit $$AddInterval^%ydboctoplanhelpers(date,type,year,month,day,hour,minute,second,microsecond)
