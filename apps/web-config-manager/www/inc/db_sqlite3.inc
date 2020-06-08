<?php
date_default_timezone_set('UTC'); //@todo change this to local time respective to where the device is

class db_sqlite3
{
	/**
	 * connect_sqlite
	 * 
	 * Open connection to sqlite file
	 * @param string - path to sqlite file
	 * @return object - sqlite db object
	 * @author Lee Wei (lee@absolutetrac.com)
	 */ 
	function connect_sqlite($db_name)
	{
		$conn = null;
		try{
			$conn = new PDO('sqlite:'.$db_name);	//Using PHP PDO driver to remain database agnostic
			$conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);	//PROD
			 //$conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_WARNING);	//DEV
		}catch(PDOException $e){
			//echo "Exception is:".$e->getMessage();
		}
		return $conn;
	} //END connect_sqlite

	/**
	 * query_sqlite
	 * 
	 * Executes the sql prepared statement passed in $sql with data values, optionally, passed in $data
	 * This function returns the result set of the query and is intended for executing SELECT statements.
	 * 
	 * @param object $conn	- sqlite db object
	 * @param string $sql	- SQL prepared statement
	 * @param array $data	- optional array to hold data values for the prepared statement.
	 * @return array		- contains sql query result
	 * @author Lee Wei (lee@absolutetrac.com); Sean Toscano (sean@absolutetrac.com)  
	 */
	function query_sqlite($conn, $sql, $data=array())
	{
		$result=array();	//instantiation
		try{
			$sth=$conn->prepare($sql);	//prepare the sql statement for execution
			$sth->execute($data);		//execute the prepared sql statement
			$result=$sth->fetchAll();	//returns array containing all the result set rows
		}catch(PDOException $e){
			//echo "Exception is:".$e->getMessage();
		}

		return $result;				//array containing all the result set rows
	} //END query_sqlite

	/**
	 * exec_sqlite
	 * 
	 * Executes the sql prepared statement passed in $sql with data values passed in $data
	 * This function returns the number of rows that were modified by the query and is intended for executing INSERT/UPDATE statements.
	 * 
	 * @param object $conn	- sqlite db object
	 * @param string $sql	- SQL prepared statement
	 * @param array $data	- array to hold data values for the prepared statement.
	 * @return integer 		- number of rows that were modified by the query
	 * @author Lee Wei (lee@absolutetrac.com); Sean Toscano (sean@absolutetrac.com)  
	 */
	function exec_sqlite($conn, $sql, $data=array())
	{
		$count=0; //initialize

		try{
			$sth = $conn->prepare($sql);	//prepare the sql statement for execution
			$count=$sth->execute($data);	//execute the prepared sql statement and return the number of rows that were modified by the query
		}catch(PDOException $e){
			//echo "Exception is:".$e->getMessage();
		}
		
		return $count;
	} //END exec_sqlite

	/**
	 * close_sqlite
	 * @param object $conn - sqlite db object
	 */
	function close_sqlite($conn)
	{
		$conn=null;
	} //END close_sqlite
	
} //END class db_sqlite3
?>
