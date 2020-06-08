DROP TABLE t_gps_data;
DROP TABLE t_rpm_data;
DROP TABLE t_speed_data;
DROP TABLE t_seatbelt_data;
DROP TABLE t_distance_data;

CREATE TABLE t_gps_data 
(	
	id			INTEGER PRIMARY KEY AUTOINCREMENT,
	utc			DOUBLE,
	year			INT,
	day			INT,
	month			INT,
	hour			INT,
	minute			INT,
	seconds			DOUBLE,
	ddLat			DOUBLE,
	ddLon			DOUBLE,
	gps_quality		INT,
	num_svs			INT,
	hdop			FLOAT,
	h_ellip			DOUBLE,
	H			DOUBLE,
	N			DOUBLE,
	dgps_age		DOUBLE,
	ref_id			INT,
	ddCOG			DOUBLE,
	sog			DOUBLE,
	HorizErr		DOUBLE,
	VertErr			DOUBLE,
	SphereErr		DOUBLE,
	valid			BOOLEAN,
	ts			TIMESTAMP
);

CREATE TABLE t_rpm_data
(
	id			INTEGER PRIMARY KEY AUTOINCREMENT,
	rpm			INT,
	ts			TIMESTAMP
);

CREATE TABLE t_speed_data
(
	id			INTEGER PRIMARY KEY	AUTOINCREMENT,
	speed		INT
	ts			TIMESTAMP
);

CREATE TABLE t_seatbelt_data
(
	id			INTEGER PRIMARY KEY AUTOINCREMENT,
	buckled			BOOLEAN,
	ts			TIMESTAMP
);

CREATE TABLE t_distance_data
(
	id			INTEGER PRIMARY KEY AUTOINCREMENT,
	distance		INT,
	ts			TIMESTAMP
)
