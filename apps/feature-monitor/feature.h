#pragma once

#include "ats-string.h"

#include "RedStoneSocketQuery.h"

// Description:
//
// XXX: The feature-monitor server must be running before using this class.
class FeatureQuery
{
public:

	FeatureQuery() : m_q("feature-monitor")
	{
	}

	// Description: Returns true if feature "p_feature" is enabled. False is
	//	returned otherwise.
	bool feature_on(const ats::String& p_feature) const
	{
		const ats::String& resp = m_q.query("feature \"" + p_feature + "\"");

		if(resp.empty())
		{
			return false;
		}

		const size_t i = resp.find('\n');

		if(ats::String::npos == i)
		{
			return false;
		}

		return (0 == resp.compare(i + 1, 3, "on\n"));
	}

	// Description: Returns a list of all enabled features (list is saved in "p_feature").
	//
	// ATS FIXME: Make it clear everywhere that feature names may NOT have commas in them
	//	(commas are used to separate multiple features in the response of the "feature"
	//	command).
	//
	// AWARE360: This function is deprecated.
	void get_features_on(ats::StringList& p_feature) const
	{
		const ats::String& resp = m_q.query("feature");
		const size_t i = resp.find('\n');

		if(ats::String::npos == i)
		{
			return;
		}

		ats::split(p_feature, resp.c_str() + i + 1, ",");
	}

	// Description: Returns a list of all enabled features (list is saved in "p_feature").
	//
	// ATS FIXME: Make it clear everywhere that feature names may NOT have commas in them
	//	(commas are used to separate multiple features in the response of the "feature"
	//	command).
	void get_features(ats::StringList& p_on_list, ats::StringList& p_off_list) const
	{
		const ats::String ref(": ok\n");
		const ats::String& resp = m_q.query("features");
		const size_t i = resp.find(ref);

		if(ats::String::npos == i)
		{
			return;
		}

		ats::StringList list;
		ats::split(list, resp.c_str() + i + ref.length(), ";");

		switch(list.size())
		{
		case 2: ats::split(p_off_list, list[1], "\n");
		case 1: ats::split(p_on_list, list[0], "\n");
		}

	}

private:
	ats::RedStoneSocketQuery m_q;
};
