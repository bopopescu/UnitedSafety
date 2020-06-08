/*
skybase_roadinfo.h

Started by Graham Asher, 7th April 2013.
*/

#ifndef SKYBASE_ROADINFO_H__
#define SKYBASE_ROADINFO_H__

#include <string>

#pragma GCC visibility push(default)

namespace SkyBase
{

class RoadInfoFinderImplementation;

class RoadInfo
    {
    public:
    std::string m_road_name;
    std::string m_road_type;
    std::string m_road_speed_limit;
    };

enum RoadInfoStatus
    {
    RoadInfoValid,
    RoadInfoNotFound,
    RoadInfoBadCoords
    };

class RoadInfoFinder
    {
    public:
    RoadInfoFinder(const char* aFileName);
    ~RoadInfoFinder();
    RoadInfoStatus GetRoadInfo(RoadInfo& aRoadInfo,double aLong,double aLat);
    
    private:
    RoadInfoFinderImplementation* m_imp;
    };

}

#pragma GCC visibility pop

#endif

