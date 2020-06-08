#!/bin/sh

DB_DIR='/mnt/update/database/'
CANTEL_DB='cantel.db'
MESSAGES_DB='messages.db'

CANTEL_INIT='create_cantel_db.sql'
MESSAGES_INIT='create_messages_db.sql'
if [ ! -e ${DB_DIR}${MESSAGES_DB} ]; then
	cat ${MESSAGES_INIT}|sqlite3 ${DB_DIR}${MESSAGES_DB}
	sync
fi
if [ ! -e ${DB_DIR}${CANTEL_DB} ]; then
	cat ${CANTEL_INIT}|sqlite3 ${DB_DIR}${CANTEL_DB}
	sync
fi
