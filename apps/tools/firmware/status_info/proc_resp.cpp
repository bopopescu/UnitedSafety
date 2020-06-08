//
// XXX: This program is VERY simplified and expects a VERY SPECIFIC XML response
//	format from the MFG REST server.
#include <iostream>
#include <math.h>
#include <stdio.h>

#include "expat.h"

#include "ats-common.h"
#include "ats-string.h"
#include "command_line_parser.h"

static ats::String g_sn;
static ats::StringMap g_data;
static int g_depth = 0;

static ats::StringList g_str;

class Item
{
public:
	Item()
	{
	}

	virtual ~Item()
	{
	}

	virtual ats::String name() const
	{
		return "Item";
	}

	virtual void set(const ats::String& p_key, const ats::String& p_val)
	{
		fprintf(stderr, "\t\tSetting \"%s\" => \"%s\" for (%s)\n", p_key.c_str(), p_val.c_str(), name().c_str());
		m_data.set(p_key, p_val);
	}

	typedef std::map <const ats::String, Item* (*)()> NewItemMap;
	typedef std::pair <const ats::String, Item* (*)()> NewItemPair;

	static NewItemMap m_item;
	static void init_items();
	static Item* new_item(const ats::String& p_type);

	ats::StringMap m_data;
};

class CommandItem : public Item
{
public:
	CommandItem()
	{
	}

	virtual ~CommandItem()
	{
	}

	virtual ats::String name() const
	{
		return "Command";
	}

};

Item* new_Item()
{
	return new Item();
}

Item* new_CommandItem()
{
	return new CommandItem();
}

Item::NewItemMap Item::m_item;

Item* Item::new_item(const ats::String& p_type)
{
	NewItemMap::const_iterator i = m_item.find(p_type);
	return (i != m_item.end()) ? (i->second)() : new_Item();
}

void Item::init_items()
{
	m_item.insert(NewItemPair("Item", ::new_Item));

	m_item.insert(NewItemPair("CommandItem", new_CommandItem));
	m_item.insert(NewItemPair("cmd", new_CommandItem));
}

static bool g_in_item = false;
static ats::String g_item_type;
static ats::String g_item_key;
static ats::String g_item_val;
static Item* g_cur_item = 0;

static std::vector <Item*> g_item;

void XMLCALL xmlstart(void *p, const char* element, const char** attribute)
{
//	fprintf(stderr, "%s,%d:%s: element='%s', g_depth=%d\n", __FILE__, __LINE__, __FUNCTION__, element, g_depth);

	if((2 == g_depth) && (0 == strcmp("item", element)) && g_item_type.empty())
	{
		g_in_item = true;
	}
	else if(g_in_item && (3 == g_depth))
	{
		g_item_key = element;
	}

	++g_depth;
}

void XMLCALL xmlend(void *p, const char *element)
{
//	fprintf(stderr, "%s,%d:%s: element='%s'\n", __FILE__, __LINE__, __FUNCTION__, element);
	--g_depth;

	if((1 == g_depth) && (0 == strcmp("item", element)) && !g_item_type.empty())
	{
		g_in_item = false;
		g_item_type.clear();
	}

	return ;
}

void handle_data(void *p, const char *content, int length)
{
//fprintf(stderr, "%s,%d:%s: content[%d]='%s', g_depth=%d\n", __FILE__, __LINE__, __FUNCTION__, length, ats::String(content, length).c_str(), g_depth);

	if(g_in_item)
	{
 
		if((3 == g_depth) && g_item_type.empty())
		{
			g_item_type.assign(content, length);
			fprintf(stderr, "\tItem type is %s\n", g_item_type.c_str());

			Item* i = Item::new_item(g_item_type);
			g_cur_item = i;
			g_item.push_back(g_cur_item);
			return;
		}

		if((4 == g_depth) && !(g_item_key.empty()))
		{
			g_item_val.assign(content, length);
			g_cur_item->set(g_item_key, g_item_val);
			g_item_key.clear();
		}

	}

}

void h_xml_parse(void *p, const ats::String& buf)
{
	XML_Parser parser = XML_ParserCreate(NULL);

	if(parser == NULL)
	{
		fprintf(stderr, "%s,%d: Failed to create XML parser\n");
		return ;
	}

//	XML_SetUserData(parser, &md);

	XML_SetElementHandler(parser, xmlstart, xmlend);

	XML_SetCharacterDataHandler(parser, handle_data);

	if ( XML_STATUS_ERROR == XML_Parse( parser, buf.c_str(), buf.size(), 0 ) )
	{
		fprintf(stderr, "failed to parser: %s( line:%lu, column:%lu )\n", XML_ErrorString( XML_GetErrorCode( parser ) ),
			XML_GetCurrentLineNumber( parser ), XML_GetCurrentColumnNumber( parser ));
	}

	XML_ParserFree(parser);
}

class MyData
{
public:
	MyData(
		const ats::String& p_rid,
		const ats::String& p_cmd,
		const ats::String& p_from
	)
	:
	m_rid(p_rid),
	m_cmd(p_cmd),
	m_from(p_from)
	{
	        init_CommandBuffer(&m_cb);
		alloc_dynamic_buffers(&m_cb, 1024, 256 * 1024);
	}

	~MyData()
	{
		free_dynamic_buffers(&m_cb);
	}

	const ats::String& m_rid;
	const ats::String& m_cmd;
	const ats::String& m_from;

	CommandBuffer m_cb;
};

ats::String create_request(MyData& p_md, const ats::String& p_msg, const ats::String& p_meta)
{
	ats::String s;
	ats_sprintf(&s, "wget"
		" -O -"
		" --no-check-certificate"
		" -T 3"
		" -t 3"
		" \"https://trulink.myabsolutetrac.com/index.php/api/1/trulink/chat_msg?X-API-KEY=287ec790e1ce0861866c05ba918f83b36e83842f"
			"&from=sn%%3A%s"
			"&rid=%s"
			"&to=%s"
			"&meta=%s"
			"&msg=%s\"",
		g_sn.c_str(),
		p_md.m_rid.c_str(),
		p_md.m_from.c_str(),
		p_meta.empty() ? "" : (ats::urlencode(p_meta).c_str()),
		p_msg.c_str());
	return s;
}

ats::String create_request(MyData& p_md, const ats::String& p_msg)
{
	return create_request(p_md, p_msg, ats::g_empty);
}

void cmd_ifconfig(MyData& p_md)
{
	std::stringstream o;
	ats::system("ifconfig|./url_encode", &o);
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

void cmd_state(MyData& p_md)
{
	std::stringstream o;
	ats::system("state|./url_encode", &o);
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

void cmd_df(MyData& p_md)
{
	std::stringstream o;
	ats::system("df|./url_encode", &o);
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

void cmd_print_env(MyData& p_md)
{
	std::stringstream o;
	ats::system("print_env|./url_encode", &o);
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

void cmd_interrupts(MyData& p_md)
{
	std::stringstream o;
	ats::system("cat /proc/interrupts|./url_encode", &o);
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

void cmd_update(MyData& p_md)
{
	std::stringstream o;
	ats::system("echo 'Updated!'|./url_encode", &o);
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

void cmd_unknown(MyData& p_md)
{
	std::stringstream o;
	o << "Unknown command \"" << p_md.m_cb.m_argv[0] << "\"";
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

void cmd_nop(MyData& p_md)
{
	std::stringstream o;
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

static void to_deg_min_sec(double p, double& p_deg, double& p_min, double& p_sec)
{
	p_deg = floor(p / 100.0);
	p_min = p - (floor(p / 100.0) * 100.0);
	p_sec = (p - floor(p)) * 100.0;
}

static double to_google_lat(double p, const ats::String& p_dir)
{
	double d;
	double m;
	double s;
	to_deg_min_sec(p, d, m, s);
	return ((d + (m/60.0)) * ((0 == strcasecmp("n", p_dir.c_str())) ? 1.0 : -1.0));
}

static double to_google_lon(double p, const ats::String& p_dir)
{
	double d;
	double m;
	double s;
	to_deg_min_sec(p, d, m, s);
	return ((d + (m/60.0)) * ((0 == strcasecmp("w", p_dir.c_str())) ? -1.0 : 1.0));
}

void cmd_google_map(MyData& p_md)
{
	
	std::stringstream o;
	ats::system("./timeout 3 grep -m1 '^$GPGGA' /dev/ttySER1", &o);
	ats::StringList sl;
	ats::split(sl, o.str(), ",");

	ats::String resp;

	if(sl.size() < 6)
	{
		resp = "Google Map: No Position";
	}
	else
	{
		double lat = strtod(sl[2].c_str(), 0);
		double lon = strtod(sl[4].c_str(), 0);
		char xb[64];
		char yb[64];
		sprintf(xb, "%f", to_google_lat(lat, sl[3]));
		sprintf(yb, "%f", to_google_lon(lon, sl[5]));
		const ats::String x(xb);
		const ats::String y(yb);
//		const ats::String& x = ats::toStr(to_google_lat(lat, sl[3]));
//		const ats::String& y = ats::toStr(to_google_lon(lon, sl[5]));
		resp = "Google Map: " + ats::urlencode("<a href=\"https://maps.google.com/maps?q=loc:" + x + "+" + y + "\" target=\"_blank\">" + x + ", " + y + "</a>");
	}

	const ats::String meta("format=html\n");
	ats::system( create_request(p_md, resp, meta), &(std::cout));
}


// FIXME: "system" is for debugging and proof of concept. It is not a secure command.
void cmd_system(MyData& p_md)
{
	ats::String s;
	int i;

	for(i = 1; i < p_md.m_cb.m_argc; ++i)
	{
		s += (s.empty() ? s : ats::String(" ")) + p_md.m_cb.m_argv[i];
	}

	std::stringstream o;
	ats::system(s + "|./url_encode", &o);
	ats::system( create_request(p_md, o.str()), &(std::cout));
}

typedef std::map <const ats::String, void(*)(MyData&)> FnMap;
typedef std::pair <const ats::String, void(*)(MyData&)> FnPair;

int main()
{
	Item::init_items();

	ats::String s;

	for(;;)
	{
		char buf[1024];
		const size_t nread = fread(buf, 1, sizeof(buf), stdin);

		if(!nread)
		{
			break;
		}

		s.append(buf, nread);
	}

	h_xml_parse(0, s);

	ats::get_file_line(g_sn, "/mnt/nvram/rom/sn.txt", 1);

	int i;

	for(i = 0; i < g_item.size(); ++i)
	{
		Item& item = *(g_item[i]);

		const ats::String& msg = item.m_data.get("msg");
		const ats::String& rid = item.m_data.get("rid");
		const ats::String& from = item.m_data.get("from");

		MyData md(rid, msg, from);
		const char* err = gen_arg_list(msg.c_str(), msg.length(), &md.m_cb);
		printf("%d: msg='%s', err='%s', argc=%d\n", i, msg.c_str(), err, md.m_cb.m_argc);

		if(err)
		{
			fprintf(stderr, "gen_arg_list failed: %s, msg='%s'", err, msg.c_str());
			return 1;
		}

		if(md.m_cb.m_argc <= 0)
		{
			cmd_nop(md);
			continue;
		}

		const ats::String& cmd = md.m_cb.m_argv[0];

		FnMap m;
		m.insert(FnPair("ifconfig", cmd_ifconfig));
		m.insert(FnPair("state", cmd_state));
		m.insert(FnPair("df", cmd_df));
		m.insert(FnPair("print_env", cmd_print_env));
		m.insert(FnPair("interrupts", cmd_interrupts));
		m.insert(FnPair("update", cmd_update));
		m.insert(FnPair("system", cmd_system));
		m.insert(FnPair("googlemap", cmd_google_map));
		m.insert(FnPair("google-map", cmd_google_map));
		m.insert(FnPair("google_map", cmd_google_map));

		FnMap::const_iterator i = m.find(cmd);

		if(i != m.end())
		{
			(i->second)(md);
		}
		else
		{
			cmd_unknown(md);
		}

	}

	return 0;
}
