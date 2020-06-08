#include <fstream>
#include <iomanip>      // std::setfill, std::setw
#include <iostream>
#include "ats-common.h"
#include "SafetyLinkMessage.h"

extern const ats::String g_CALAMPDBcolumnname[];

SafetyLinkMessage::SafetyLinkMessage(ats::StringMap& data) :	CalAmpsMessage()
{
    m_optionsHeader.setMobileId(data.get("mobile_id"), (OptionsHeader::MOBILE_ID_TYPE)data.get_int("mobile_id_type"));

    m_messageHeader.setServiceType(MessageHeader::ServiceType::ACKNOWLEDGED_REQUEST);
    m_messageHeader.setMessageType(MessageHeader::MessageType::SAFETYLINK_EVENT );

    updateTime = QDateTime::fromString(QString(data.get("event_time").c_str()), "yyyy-MM-dd HH:mm:ss");
    fixTime = QDateTime::fromString(QString(data.get("fix_time").c_str()), "yyyy-MM-dd HH:mm:ss");
    latitude = qRound(data.get_double("latitude") * 10000000);
    longitude = qRound(data.get_double("longitude") * 10000000);
    altitude = qRound(data.get_double("altitude") * 100);
    speed = (data.get_int("speed")*1000)/36; //Convert to cm/s
    heading = qRound(data.get_double("heading"));
    h_accuracy = qRound(data.get_double("h_accuracy") * 100);
    v_accuracy = qRound(data.get_double("v_accuracy") * 100);
    satellites = data.get_int("satellites");
    fix_status = data.get_int("fix_status");  // uses the value as set by message-assembler (modified there to meet cams spec)
    unit_status = (data.get_int("unit_status"));
    event_type = (data.get_int("event_type"));
    std::cerr << "event_type:" << event_type <<std::endl;
    for (std::map<ats::String,ats::String>::iterator it=data.begin(); it!=data.end(); ++it)
      std::cout << it->first << " => " << it->second << '\n';
}

void SafetyLinkMessage::WriteData(QByteArray& data)
{
    qint16 data16;
    qint32 data32;

    ats::String optionsHeader = m_optionsHeader.data();
    ats::String messageHeader = m_messageHeader.data();
    m_messageHeader.incSequenceNumber();

    std::cerr << "msgHeader message type: " << m_messageHeader.getMessageType() << std::endl;
    std::cerr << "msgHeader message seq: " << m_messageHeader.getSequenceNum() << std::endl;
    const int offset = optionsHeader.length() + messageHeader.length();
    data.append(optionsHeader.c_str(), optionsHeader.length());
    data.append(messageHeader.c_str(), messageHeader.length());

    data32 = qToBigEndian((qint32)(updateTime.toMSecsSinceEpoch()/1000));
    for(int i=0; i<4; i++)
        data[offset+i]=((uchar *)&data32)[i];

    data32 = qToBigEndian((qint32)(fixTime.toMSecsSinceEpoch()/1000));
    for(int i=0; i<4; i++)
        data[offset+4+i]=((uchar *)&data32)[i];

    data32 = qToBigEndian(latitude);
    for(int i=0; i<4; i++)
        data[offset+8+i]=((uchar *)&data32)[i];

    data32 = qToBigEndian(longitude);
    for(int i=0; i<4; i++)
        data[offset+12+i]=((uchar *)&data32)[i];

    data32 = qToBigEndian(altitude);
    for(int i=0; i<4; i++)
        data[offset+16+i]=((uchar *)&data32)[i];

    data32 = qToBigEndian(speed);
    for(int i=0; i<4; i++)
        data[offset+20+i]=((uchar *)&data32)[i];

    data16 = qToBigEndian(heading);
    for(int i=0; i<2; i++)
        data[offset+24+i]=((uchar *)&data16)[i];

    data16 = qToBigEndian(h_accuracy);
    for(int i=0; i<2; i++)
        data[offset+26+i]=((uchar *)&data16)[i];

    data16 = qToBigEndian(v_accuracy);
    for(int i=0; i<2; i++)
        data[offset+28+i]=((uchar *)&data16)[i];

    data[offset+30] = satellites; //Satilites
    data[offset+31] = fix_status; // Fix Status

    data16 = qToBigEndian(unit_status);
    for(int i=0; i<2; i++)
        data[offset+32+i]=((uchar *)&data16)[i];

    data16 = qToBigEndian(event_type);
    for(int i=0; i<2; i++)
        data[offset+34+i]=((uchar *)&data16)[i];
}


std::string SafetyLinkMessage::getMessage()
{
    std::string s;
    QByteArray ba;
    WriteData(ba);
    s.append(ba.begin(),ba.end());

    std::ostringstream outstr;
    outstr << "message[size:" << s.length() << "]:";
    for(uint i = 0; i < s.length(); ++i)
    {
        outstr << std::hex <<	std::setfill('0') << std::setw(2) << std::uppercase << (s[i] & 0xff) << ',';
    }

    std::cerr << outstr.str().c_str() << std::endl;

    return s;
}
