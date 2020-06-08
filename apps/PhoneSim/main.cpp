    //---------------------------------------------------------------
//
// PhoneSim - inject simulated phone messages to Gemalto or CAMS
//
//  Usage: PhoneSim --help [--key=val]...
//
//	where key can be any of the following:
//    imei  - the IMEI to be used in the output messages
//		lat   - the starting latitude to be used in the output messages
//		lon   - the starting longitude to be used in the output messages.
//		event - the event type as specified in the SafetyLink Supported Message Types document
//		host  - the host to send the data to.
//    port  - the port to send the data to.
//
// When run the program presents a menu of event types that can be sent.  If an [event=xxx] argument
// was provided the menu will not appear and the message will go out immediately.
// Position will change slightly with each message sent
//
// Defaults:
// 	Time will be pulled from the system.
//  IMEI will be 111222333444555
//  position will be 51.000 -114.000 - modified by date/time offset.
//  host will be Gemalto portal
//  port will be Gemalto portal port for phones.
//
// All other values in the message will be modified slightly between messages.  The current date and time will
// be used to make the modifications so no 2 messages should land on the same spot.
//
// Message types requiring further input will prompt for any required data.  To run from the command line events with
// extra data have the key/data pairs attached to the event= string as follows
// event=SL_START_MONITORING SL_TZOFFSETSEC=3600 SL_MANUAL_LOCATION="51.123,-113.92745"
//
//  Note these use the Event Types names and Data Type names provided in the documentation as the keys.
//
// 'q' will exit the program
//
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <math.h>
#include "tclap/CmdLine.h"
#include <boost/algorithm/string.hpp>
#include "boost/format.hpp"
#include "SafetyLinkMessage.h"   // defines the calamp message being sent.
#include "colordef.h"
#include <boost/array.hpp>
#include <boost/asio.hpp>

using namespace std;
using namespace TCLAP;
using namespace boost;
using boost::format;
using boost::asio::ip::udp;


class PSimData
{
public:
    string m_IMEI;
    double m_Lat;
    double m_Lon;
    string m_Host;
    int m_Port;
    int m_Event;

    PSimData() : m_IMEI("111222333444555"), m_Lat(51.0), m_Lon(-113.0), m_Host("199.167.39.10"), m_Port(19015), m_Event(-1){}
    virtual ~PSimData(){};
    string ToString()
    {
        string strOut;
        strOut  = "  IMEI: "  GREEN_ON + m_IMEI + RESET_COLOR "\n" ;
        strOut += "  Host: " GREEN_ON + m_Host + RESET_COLOR "\n" ;
        strOut += "  Port: " GREEN_ON + str( format("%d") % m_Port) + RESET_COLOR "\n";
        strOut += "  Position: (" GREEN_ON + str( format("%.9f") % m_Lat) + "," + str( format("%.9f") % m_Lon) + RESET_COLOR ")\n";
        if (m_Event != -1)
            strOut += "Event: " + str( format("%d") % m_Event) + "\n";

        return strOut;
    }
};

class UDPClient
{
public:
    UDPClient(
        boost::asio::io_service& io_service,
        const std::string& host,
        const std::string& port
    ) : io_service_(io_service), socket_(io_service, udp::endpoint(udp::v4(), 0)) {
        udp::resolver resolver(io_service_);
        udp::resolver::query query(udp::v4(), host, port);
        udp::resolver::iterator iter = resolver.resolve(query);
        endpoint_ = *iter;
    }

    ~UDPClient()
    {
        socket_.close();
    }

    void send(const std::string& msg)
    {
        cout << "UDPClient.send: msg len is " << msg.size() << std::endl;
        socket_.send_to(boost::asio::buffer(msg, msg.size()), endpoint_);
    }

private:
    boost::asio::io_service& io_service_;
    udp::socket socket_;
    udp::endpoint endpoint_;
};


bool ProcessCommandLine(CmdLine &cmd, int argc, char *argv[], PSimData &data);
bool SendEvent(PSimData &data);
void BuildPosition(PSimData &data, ats::StringMap &smCalAmpData);

int g_seq;
//-------------------------------------------------------------------------------------------------
int main(int argc, char * argv[])
{
    PSimData simData;

  // parse the command line -  presents the  usage if --help or -h was entered
    CmdLine cmd("PhoneSim - inject simulated phone messages to Gemalto or CAMS", ' ', "0.1");

  if (!ProcessCommandLine(cmd, argc, argv, simData))
    exit(0);

  // present the menu if the event wasn't set
  if (simData.m_Event == -1 )
  {
    cout << "Interactive mode selected\n";
      cout << simData.ToString();

    string choice;

      do
      {
        cout << "\n\n";

        cout << "\t102 - Power Down Event\n";
        cout << "\t103 - Power Up Event\n";
        cout << "\t104 - Low Battery\n";
        cout << "\t105 - Power Critical Battery\n";
        cout << "\t110 - Transmission Path Change\n";
        cout << "\t201 - Stationary Event\n";
        cout << "\t400 - Request State\n";
        cout << "\t401 - Request Heartbeat\n";
        cout << "\t402 - Position Update\n";
        cout << "\t403 - Start Monitoring\n";
        cout << "\t404 - Stop Monitoring\n";
        cout << "\t405 - Assist\n";
        cout << "\t406 - Assist Cancel\n";
        cout << "\t  q - quit program\n";

        cout << "\nEnter Option: ";
        cin >> choice;
        cout << endl;
        if (choice != "q")
        {
            simData.m_Event = atoi(choice.c_str());
            if (!SendEvent(simData))
                cout << RED_ON "Invalid entry - please try again..." RESET_COLOR;
            else
                cout << GREEN_ON "Message succesfully sent" RESET_COLOR << endl;
        }

      } while (choice != "q");
    }
    else
    {
        if (!SendEvent(simData))
            cout << RED_ON "ERROR:" RESET_COLOR " Invalid event " RED_ON << str( format("%d") % simData.m_Event) << RESET_COLOR " - please try again..."  << endl;
        else
            cout << GREEN_ON "Message succesfully sent" RESET_COLOR << endl;
    }

  exit(0);
}

//-------------------------------------------------------------------------------------------------
bool ProcessCommandLine(CmdLine &cmd, int argc, char *argv[], PSimData &data)
{
    try
    {
        ValueArg<string>  imeiArg("i", "imei", "IMEI", false, "111222333444555", "string");
        ValueArg<double>  latArg("l", "lat", "Starting latitude", false, 51.000, "double");
        ValueArg<double>  lonArg("n", "lon", "Starting longitude", false, -114.000, "double");
        ValueArg<string>  hostArg("H", "host", "Host portal", false, "199.167.39.10", "string");
        ValueArg<int>     portArg("P", "port", "Port on Host portal", false, 17154, "int");
        ValueArg<int>     eventArg("e", "event", "Event Type", false, -1, "int");
        cmd.add( imeiArg );
        cmd.add( latArg );
        cmd.add( lonArg );
        cmd.add( hostArg );
        cmd.add( portArg );
        cmd.add( eventArg );

        cmd.parse(argc, argv);

        data.m_IMEI = imeiArg.getValue();
        data.m_Lat  = latArg.getValue();
        data.m_Lon  = lonArg.getValue();
        data.m_Host = hostArg.getValue();
        data.m_Port = portArg.getValue();
        data.m_Event = eventArg.getValue();
    }
    catch (ArgException &e)  // catch any exceptions
    {
        cerr << "error: " << e.error() << " for arg " << e.argId() << endl;
        return false;
    }
    return true;
}

//-------------------------------------------------------------------------------------------------
// SendEvent - Sends the EVENT entered on the command line
// 		return: true if event was sent.
//  				  false if the event is not known or invalid
bool SendEvent(PSimData &data)
{
  ats::StringMap smCalAmpData;
  smCalAmpData.set("mobile_id", data.m_IMEI, true);
  smCalAmpData.set("mobile_id_type","2", true);
  BuildPosition(data, smCalAmpData);

  switch (data.m_Event)
  {
    case 102:
  case 103:
  case 104:
  case 105:
  case 401:
  case 402:
  case 403:
  case 404:
  case 405:
      smCalAmpData.set("event_type", str( format("%d") % data.m_Event), true);
      break;
    default:
      return false;
  }

  try
  {
      SafetyLinkMessage msg(smCalAmpData);
      msg.getMessageHeader().setSequenceNumber(g_seq);

      std::string outData;
      outData = msg.getMessage();
      cerr << g_seq << ": outData Len:" << outData.length() << endl;
      boost::asio::io_service io_service;
      UDPClient udpClient(io_service, data.m_Host, str( format("%d") % data.m_Port));
      udpClient.send(outData);
      g_seq++;
  }
  catch  (ArgException &e)  // catch any exceptions
  {
      cerr << "error: " << e.error() << " for arg " << e.argId() << endl;
      return false;
  }

  return true;
}



//-------------------------------------------------------------------------------------------------
// BuildPosition - sets the string map with the modified position and other fields
//
void BuildPosition(PSimData &data, ats::StringMap &smCalAmpData)
{
    const double DEG_TO_RAD = 0.0174532925199;
    time_t rawT = time(NULL);
    struct tm *timeinfo;
    unsigned int t = (unsigned)rawT;
    timeinfo = localtime(&rawT);

    char buf[128];
    strftime(buf, 128, "%F %T",timeinfo);
    double lat = data.m_Lat + (1 / 60.) * cos((t%360) * DEG_TO_RAD);
    double lon = data.m_Lon + (1 / 60.) * sin((t%360) * DEG_TO_RAD);
    double h = 1000.0 + ((double)(t%1000) - 500 / 10.0);
    double speed = 30 + ((t%1000) - 500) / 100.0;
    double heading = (t%360);

    smCalAmpData.set("event_time", buf, true);
    smCalAmpData.set("fix_time", buf, true);
    smCalAmpData.set("latitude", str( format("%.9f") % lat), true);
    smCalAmpData.set("longitude", str( format("%.9f") % lon), true);
    smCalAmpData.set("altitude", str( format("%.1f") % h), true);
    smCalAmpData.set("speed", str( format("%.1f") % speed), true);
    smCalAmpData.set("heading", str( format("%d") % heading), true);
    smCalAmpData.set("h_accuracy", "2.5", true);
    smCalAmpData.set("v_accuracy", "2.6", true);
    smCalAmpData.set("satellites", str( format("%.1f") % (7 +(t%8))), true);
    smCalAmpData.set("fix_status", "2", true);
    smCalAmpData.set("unit_status", "8", true);
}

