"-- \n"
"-- FILE: \n"
"-- table-audit.sql \n"
"-- \n"
"-- FUNCTION: \n"
"-- Define the audit trail tables  \n"
"-- Note that these tables must be kept manually in sync with those \n"
"-- in table-create.sql \n"
"-- \n"
"-- HISTORY: \n"
"-- Copyright (C) 2000, 2001 Linas Vepstas \n"
"-- \n"
"-- The change field may be 'a' -- add, 'd' -- delete/drop, 'm' -- modify \n"
"-- \n"
"-- The objtype field may be 'a' -- account, 'c' -- commodity, 'e' -- entry, \n"
"--                          'k' -- kvp, 'p' -- price, 't' -- transaction \n"
"-- \n"
"-- The objtype is the class type of the child class. \n"
"-- \n"
"-- Note that SQL is only partially object-oriented.  Thus, we use the \n"
"-- gncAuditTrail table to define the parent class; the children inherit \n"
"-- from it.  Since SQL doesn't tell the parent who the child was, we use \n"
"-- the 'objtype' field to store the class type of the child. \n"
"-- \n"
"-- Thus, we should really enforce a constraint on this field: \n"
"-- CREATE TABLE gncAccountTrail (  \n"
"--         objtype       CHAR NOT NULL CHECK (objtype == 'a') \n"
"-- and so on.   \n"
" \n"
"CREATE TABLE gncAuditTrail ( \n"
"	sessionGuid		CHAR(32)  NOT NULL,   -- who changed it \n"
"	date_changed 		TIMESTAMP,   -- when they changed it \n"
"        change			CHAR NOT NULL, \n"
"        objtype			CHAR NOT NULL \n"
"); \n"
" \n"
"-- would love to inherit, but can't because this wrecks the primary key \n"
" \n"
"CREATE TABLE gncAccountTrail ( \n"
"	accountGuid	CHAR(32) NOT NULL,  -- override, not a primary key anymore \n"
"	parentGuid	CHAR(32) NOT NULL, \n"
"	bookGuid	CHAR(32) NOT NULL, \n"
"	accountName 	TEXT NOT NULL CHECK (accountName <> ''), \n"
"	accountCode 	TEXT, \n"
"	description 	TEXT, \n"
"	type		TEXT NOT NULL, \n"
"	commodity	TEXT NOT NULL CHECK (commodity <>''), \n"
"	version		INT4 NOT NULL, \n"
"	iguid		INT4 DEFAULT 0 \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE INDEX gncAccountTrail_account_idx ON gncAccountTrail (accountGuid); \n"
" \n"
"CREATE TABLE gncBookTrail ( \n"
"	bookGuid	CHAR(32) NOT NULL, \n"
"	book_open	CHAR DEFAULT 'n', \n"
"	version		INT4 NOT NULL, \n"
"	iguid		INT4 DEFAULT 0 \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE INDEX gncBookTrail_book_idx ON gncBookTrail (bookGuid); \n"
" \n"
"CREATE TABLE gncCommodityTrail ( \n"
"        commodity	TEXT NOT NULL,  -- override, not a primary key anymore \n"
"	fullname	TEXT, \n"
"	namespace	TEXT NOT NULL, \n"
"	mnemonic	TEXT NOT NULL, \n"
"	code		TEXT, \n"
"	fraction	INT DEFAULT '100' \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE INDEX gncCommodityTrail_commodity_idx ON gncCommodityTrail (commodity); \n"
" \n"
"CREATE TABLE gncEntryTrail ( \n"
"	entryGuid		CHAR(32) NOT NULL,  -- override, not a primary key anymore \n"
"	accountGuid		CHAR(32) NOT NULL, \n"
"	transGuid		CHAR(32) NOT NULL, \n"
"	memo			TEXT, \n"
"	action			TEXT, \n"
"	reconciled		CHAR DEFAULT 'n', \n"
"	date_reconciled 	TIMESTAMP, \n"
"	amount			INT8 DEFAULT '0', \n"
"	value			INT8 DEFAULT '0', \n"
"	iguid			INT4 DEFAULT 0 \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE INDEX gncEntryTrail_entry_idx ON gncEntryTrail (entryGuid); \n"
" \n"
"CREATE TABLE gncPriceTrail ( \n"
"	priceGuid	CHAR(32) NOT NULL,  -- override, not a primary key anymore \n"
"	commodity	TEXT NOT NULL CHECK (commodity <>''), \n"
"	currency	TEXT NOT NULL CHECK (commodity <>''), \n"
"	time		TIMESTAMP, \n"
"	source		TEXT, \n"
"	type		TEXT, \n"
"	valueNum	INT8 DEFAULT '0', \n"
"	valueDenom	INT4 DEFAULT '100', \n"
"	version		INT4 NOT NULL, \n"
"	bookGuid	CHAR(32) NOT NULL \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE INDEX gncPriceTrail_price_idx ON gncPriceTrail (priceGuid); \n"
" \n"
"CREATE TABLE gncTransactionTrail ( \n"
"	transGuid	CHAR(32) NOT NULL,  -- override, not a primary key anymore \n"
"	last_modified 	TIMESTAMP DEFAULT 'NOW', \n"
"	date_entered 	TIMESTAMP, \n"
"	date_posted 	TIMESTAMP, \n"
"	num		TEXT, \n"
"	description	TEXT, \n"
"        currency	TEXT NOT NULL CHECK (currency <> ''), \n"
"	version		INT4 NOT NULL, \n"
"	iguid		INT4 DEFAULT 0 \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE INDEX gncTransactionTrail_trans_idx ON gncTransactionTrail (transGuid); \n"
" \n"
"CREATE TABLE gncKVPvalueTrail ( \n"
"	iguid		INT4, \n"
"	ipath		INT4, \n"
"	type		char(4) \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE TABLE gncKVPvalue_int64Trail ( \n"
"	iguid		INT4, \n"
"	ipath		INT4, \n"
"	type		char(4), \n"
"	data		INT8 \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE TABLE gncKVPvalue_dblTrail ( \n"
"	iguid		INT4, \n"
"	ipath		INT4, \n"
"	type		char(4), \n"
"	data		FLOAT8 \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE TABLE gncKVPvalue_numericTrail ( \n"
"	iguid		INT4, \n"
"	ipath		INT4, \n"
"	type		char(4), \n"
"	num		INT8, \n"
"	denom		INT8 \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE TABLE gncKVPvalue_strTrail ( \n"
"	iguid		INT4, \n"
"	ipath		INT4, \n"
"	type		char(4), \n"
"	data		TEXT \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE TABLE gncKVPvalue_guidTrail ( \n"
"	iguid		INT4, \n"
"	ipath		INT4, \n"
"	type		char(4), \n"
"	data		CHAR(32) \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE TABLE gncKVPvalue_timespecTrail ( \n"
"	iguid		INT4, \n"
"	ipath		INT4, \n"
"	type		char(4), \n"
"	data		TIMESTAMP \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"CREATE TABLE gncKVPvalue_listTrail ( \n"
"	iguid		INT4, \n"
"	ipath		INT4, \n"
"	type		char(4), \n"
"	data		TEXT[] \n"
") INHERITS (gncAuditTrail); \n"
" \n"
"-- end of file";
