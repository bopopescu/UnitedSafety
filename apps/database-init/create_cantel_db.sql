CREATE TABLE message_table 
(
	mid		 INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	msg_priority	 INTEGER,
	event_time	 TIMESTAMP,
	latitude	 DOUBLE,
	longitude	 DOUBLE,
	speed		 DOUBLE,
	heading		 DOUBLE,
	inputs		 INTEGER,
	event_type	 INTEGER,
	usr_msg_id	 INTEGER,
	usr_msg_data	 TEXT
);
