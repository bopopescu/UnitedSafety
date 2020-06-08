#pragma once

//  rapidjson.h - Only inlude this file in .ccp files - do not include in a .h file.
//
//  This is just a convienience file for including rapidjson functionality in a file.
#include <boost/algorithm/string.hpp>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"


using namespace rapidjson;

#define rapidString(a) StringRef(a.c_str(), a.size())

ats::String DocToHex(const Document& dom);
