<?php
 define("DEVICE_NAME", "TGX Configuration Manager");
 define("DB", $_SERVER['DOCUMENT_ROOT'].'/../www-app/config_mang.db');
 define ("DEBUG", "ON");
 define("ethernet", "eth0");
 define("wireless", "ra0");
 //Defines for message priorities
 define("Wifi_Slider_Value", "1");
 define("Cell_Slider_Value", "2");
 define("Iridium_Slider_Value", "3");
 define("SOS_Msg_Priority", "1");
 define("Wifi_Msg_Priority", "101");
 define("Cell_Msg_Priority", "20");
 define("Iridium_Msg_Priority", "9");

 $GLOBALS["trulink_reserved_ports"] = array(38000, 38022, 39000, 39001, 41414, 39019, 41008, 41106, 44107, 41108, 41109, 41102, 41104, 41204, 41012, 41011, 41103, 41106, 41009, 41100, 41001, 41010, 41105, 41013, 41014, 41015, 41016, 41017, 41092, 41094, 41110);

 $GLOBALS["trulink_protocols"] = array("TCP", "UDP");

 ?>

