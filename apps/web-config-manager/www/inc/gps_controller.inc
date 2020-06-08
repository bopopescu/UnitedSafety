<?php

require_once $_SERVER['DOCUMENT_ROOT'].'/inc/db_sqlite3.inc';	//contains functions for db interaction
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/util.inc';			//contains functions for socket interaction, error message display, and logging.
require_once $_SERVER['DOCUMENT_ROOT'].'/inc/dbconfig_controller.inc';			//contains functions for getting, settings dbconfig parameters.

class gpscontroller
{
	private $dbconfig = '';

	function __construct()
	{
		$this->dbconfig = new dbconfigController();
	}

		/**
		 * getGpsData
		 *
		 * Gets the GPS data as xml from getGps() and parses it before returning it to the view controller
		 * @return SimpleXMLElement object $gps
		 * @author - Sean Toscano (sean@absolutetrac.com)
		 */
	function getGpsData()
	{
		$gps = null;
		$nodata = '--';

		$xml = $this->getGps();		//Get the GPS data xml

		debug('getGpsData: xml', $xml);				//DEBUG

		if(!empty($xml))
		{
			$gps = new SimpleXMLElement($xml);			//convert xml to SimpleXMLElement object

			//Parse and validate data
			//Time
			if(!empty($gps->time))
			{
				//Converting date-time returned by GPS from 2013-09-22 17:54:20 to 17:54:20 (UTC) Sep 22, 2013
				$gpsDateTime = explode(" ", $gps->time);
				$gpsDate = date_create_from_format('Y-m-d', $gpsDateTime[0]);
				$gps->time = $gpsDateTime[1]. " (UTC) " . $gpsDate->format('M d, Y');
			}
			else if(isset($gps->time))
			{
				$gps->time = $nodata;
			}
			else
			{
				$gps->addChild('time', $nodata);
			}

			//Satellites
			if(!empty($gps->satellites) || round($gps->satellites) == 0)
			{
				$gps->satellites = round($gps->satellites);
			}
			else if(isset($gps->satellites))
			{
				$gps->satellites = $nodata;
			}
			else
			{
				$gps->addChild('satellites', $nodata);
			}

			//Latitude
			if(!empty($gps->latitude) || round($gps->latitude) == 0)
			{
				$gps->latitude = $this->decimalToNDD((float)$gps->latitude,'lat');			
			}
			else if(isset($gps->latitude))
			{
				$gps->latitude = $nodata;
			}
			else
			{
				$gps->addChild('latitude', $nodata);
			}

			//Longitude
			if(!empty($gps->longitude) || round($gps->longitude) == 0)
			{
				$gps->longitude = $this->decimalToNDD((float)$gps->longitude,'long');			//convert decimal latitude to degree, minutes, seconds format
			}
			else if(isset($gps->longitude))
			{
				$gps->longitude = $nodata;
			}
			else
			{
				$gps->addChild('longitude', $nodata);
			}


			//Quality
			if(isset($gps->quality))
			{
				switch($gps->quality)						//convert gps quality integer to textual representation
				{
					case 0:
						$gps->quality = 'no position';
						break;
					case 1:
						$gps->quality = '3D';
						break;
					case 2:
						$gps->quality = '3D Differential';
						break;
					default:
						$gps->quality = $nodata;
				}
			}
			else
			{
				$gps->addChild('quality', $nodata);
			}



			//Elevation
			if(!empty($gps->elevation) || round($gps->elevation) == 0)
			{
				$gps->elevation = round($gps->elevation).' m (above sea level)';
			}
			else if(isset($gps->elevation))
			{
				$gps->elevation = $nodata;
			}
			else
			{
				$gps->addChild('elevation', $nodata);
			}


			//Heading
			if(!empty($gps->heading) || round($gps->heading) == 0)
			{
				$gps->heading = round($gps->heading).'&deg;';
			}
			else if(isset($gps->heading))
			{
				$gps->heading = $nodata;
			}
			else
			{
				$gps->addChild('heading', $nodata);
			}

			//HDOP
			if(!empty($gps->hdop) || $gps->hdop == 0)
			{
				$gps->hdop = $gps->hdop;
			}
			else if(isset($gps->hdop))
			{
				$gps->hdop = $nodata;
			}
			else
			{
				$gps->addChild('hdop', $nodata);
			}

			//Velocity
			if(!empty($gps->velocity) || round($gps->velocity) == 0)
			{
				$gps->velocity = round($gps->velocity).' kph';
			}
			else if(isset($gps->velocity))
			{
				$gps->velocity = $nodata;
			}
			else
			{
				$gps->addChild('velocity', $nodata);
			}
		}
		else
		{
		}
		return $gps;
	} //END getGpsData

		/**
				 * decimalToNDD
				 *
				 * Converts the GPS latitude/longitude decimal form into the N/S Decimal degrees (e.g.N DD.ddddd) form
				 * @param float $decimal - latitude/longitude value
				 * @param string $type - indicates latitude or longitude
				 * @return string
				 * @author Sean Toscano (sean@absolutetrac.com)
		*/
	function decimalToNDD($decimal, $type)
	{
		$operator = 1;

					//negative latitude implies South while negative longitude implies West.
					if($decimal < 0)
					{
						$operator = -1;

						if($type == 'lat')
							$dms['direction'] = 'S';

						elseif($type == 'long')
							$dms['direction'] = 'W';
					}
					//positive latitude implies North while positive longitude implies East.
					elseif($decimal > 0)
					{
						$operator = 1;

						if($type == 'lat')
							$dms['direction'] = 'N';

						elseif($type == 'long')
							$dms['direction'] = 'E';
					}
					else
					{
						$dms['direction'] = '';
					}

					//Arithmetic Conversion
					$decimal = $decimal * $operator;														// -114.049 * -1 = 114.049
					$dms['degrees'] = $decimal;

					return $dms['direction'].' '.$dms['degrees'].'&deg; ';	// W 114.123847*
				} //END decimalToDMS


				/**
				 * getGps
				 *
				 * Reads the GPS data from the device by calling the admin client function 'getgpsdata'
				 * @return string|boolean
				 * @author Sean Toscano (sean@absolutetrac.com)
				 */
				private function getGps()
				{
					$sh_args = 'getgpsdata';								//admin client command for getting gps data
					$sh_out = atsexec(escapeshellcmd($sh_args));			//socket call
					//debug('getGps: getgpsdata (admin client)', $sh_out);		//DEBUG

					if(strcasecmp($sh_out,'GPS is invalid') != 0 && strcasecmp($sh_out,'phpcmd: fail') != 0 && strcasecmp($sh_out,'phpcmd: invalid command') != 0)
					{
						return $sh_out;				//return xml
					}
					else
					{
						return false;
					}
				} //END getGps


		/**
		 * getGpsType
		 *
		 * Reads the GPS data from dbconfig by calling the admin client function 'dbconfigget'
		 * @return string
		 * @author Sean Toscano (sean@absolutetrac.com)
		 */
		public function getGpsType()
		{
			return $this->dbconfig->getDbconfigData('RedStone', 'GPS');

		} //END getGpsType

		/**
		 * setGpsType
		 *
		 * Updates the gps type in dbconfig
		 * @param string - GPS type [NVS|Telit|GOBI|Ublox|FasTrax]
		 * @return boolean
		 * @author - Sean Toscano (sean@absolutetrac.com)
		 */
		private function setGpsType($value)
		{
			return $this->dbconfig->setDbconfigData('RedStone','GPS',$value);

		} //END setGpsType


		/**
		 * getGpsSource
		 *
		 * Reads the GPS receiver (Internal/External) from dbconfig by calling the admin client function 'dbconfigget'
		 * @return string
		 * @author Sean Toscano (sean@absolutetrac.com)
		 */
		public function getGpsSource()
		{
			return $this->dbconfig->getDbconfigData('RedStone', 'GPSSource');

		} //END getGpsSource

		/**
		 * setGpsSource
		 *
		 * Updates the GPS receiver(source) (Internal/External) in dbconfig by calling the admin client function 'dbconfigset'
		 * @author Sean Toscano (sean@absolutetrac.com)
		 */
		public function setGpsSource($value)
		{
			return $this->dbconfig->setDbconfigData('RedStone','GPSSource',$value);

		} //END setGpsSource

		/**
				 * setGpsData
				 *
				 * Wrapper for setGpsType
				 * @param string - GPS type [NVS|Telit|GOBI|Ublox]
				 * @return boolean
				 * @author - Sean Toscano (sean@absolutetrac.com)
				 */
				function setGpsData($data)
				{

					return $this->setGpsType($data);

				} //END setGpsData

				public function getGpsTypes()
				{
					$gps_types = array();

					$dt = new db_sqlite3();
					$conn = $dt->connect_sqlite(DB);

					$sql="select * from gps order by displayOrder asc";			//fetch the gps types
					$result = $dt->query_sqlite($conn, $sql);

					foreach($result as $gps)
					{
						array_push($gps_types, $gps['type']);
					}

					return $gps_types;
				}

				public function getGpsListHtml()
				{
					$gps_list_html = '';
					$gps_types = $this->getGpsTypes();
					$gps_type = $this->getGpsType();

					foreach($gps_types as $provider)
					{
						$gps_list_html .= '<option value="'.$provider.'"';

						if(strcasecmp($provider,$gps_type) == 0)
							$gps_list_html .= ' selected="selected"';

						$gps_list_html .= '>'.$provider.'</option>';
					}

					return $gps_list_html;
				}

} //END class gpscontroller
?>
