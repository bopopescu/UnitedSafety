#include <string.h>
#include <dlfcn.h>

#include "ats-string.h"
#include "GPIOManager.h"
#include "MyData.h"
#include "BlinkerProgram.h"

typedef void (*blink_init)(struct BlinkerProgramContext*);
typedef void (*blink_start)();
typedef void (*blink_stop)();
typedef void (*blink_fini)();
typedef void (*blink_data)(const char* p_data);

class BlinkerProgram
{
public:
	BlinkerProgram()
	{
		m_owner = 0;
		m_prog = 0;
		m_init = 0;
		m_start = 0;
		m_stop = 0;
		m_fini = 0;
		m_data = 0;
	}

	Blinker* m_owner;
	void* m_prog;

	blink_init m_init;
	blink_start m_start;
	blink_stop m_stop;
	blink_fini m_fini;
	blink_data m_data;

	pthread_t m_thread;
};

struct BlinkerProgramContext
{
	BlinkerProgram m_bp;
};

typedef std::map <const ats::String, BlinkerProgramContext> BlinkerProgramContextMap;
typedef std::pair <const ats::String, BlinkerProgramContext> BlinkerProgramContextPair;

static BlinkerProgramContextMap g_bp;

extern MyData* g_md;

EXTERN_C const char* bp_get_value(const bp_StringMap p_sm, const char* p_key)
{

	if(!p_key)
	{
		return "";
	}

	const ats::StringMap& m = *((const ats::StringMap*)p_sm);
	return (m.get(p_key)).c_str();
}

EXTERN_C const char* bp_post_command(BlinkerProgramContext*, const char* p_cmd)
{

	if(!p_cmd)
	{
		return "";
	}

	const ats::String& emsg = post_command(p_cmd);

	if(!emsg.empty())
	{
		// ATS FIXME: Modify/update API so that the real error message can be returned.
		return "failed";
	}

	return "";
}

EXTERN_C const char* bp_get_name(BlinkerProgramContext* bpc)
{
	return (bpc && bpc->m_bp.m_owner) ? (bpc->m_bp.m_owner->name().c_str()) : 0;
}

EXTERN_C int bp_get_led_addr_byte_pin(const char* p_name, int* p_expander, int* p_byte, int* p_pin)
{
	int e, b, p;
	const char* emsg = g_md->get_led_addr_byte_pin(p_name ? p_name : ats::String(), e, b, p);

	if(p_expander)	
	{
		*p_expander = e;
	}

	if(p_byte)
	{
		*p_byte = b;
	}

	if(p_pin)
	{
		*p_pin = p;
	}

	return emsg ? 0 : 1;
}

EXTERN_C void bp_set_named_led(BlinkerProgramContext* bpc, const char** p_owner, const char* p_name, int p_value)
{
	const ats::String s((p_owner && (*p_owner)) ? (*p_owner) : "");

	if(p_owner)
	{
		g_md->set_led(p_name ? p_name : "", p_value, (*p_owner) ? (&s) : 0);
	}
	else
	{
		g_md->set_led(p_name ? p_name : "", p_value, bpc ? (&(bpc->m_bp.m_owner->name())) : 0);
	}

}

EXTERN_C void bp_set_led(BlinkerProgramContext* bpc, const char** p_owner, int p_expander, int p_byte, int p_pin, int p_value)
{
	const ats::String s((p_owner && (*p_owner)) ? (*p_owner) : "");

	if(p_owner)
	{
		set_led(*g_md, (*p_owner) ? (&s) : 0, p_expander, p_byte, p_pin, p_value);
	}
	else
	{
		set_led(*g_md, bpc ? (&(bpc->m_bp.m_owner->name())) : 0, p_expander, p_byte, p_pin, p_value);
	}

}

EXTERN_C void* bp_get_named_gpio_context(BlinkerProgramContext* p_bpc, const char* p_name, int p_priority)
{

	if(!(p_bpc && p_bpc->m_bp.m_owner))
	{
		return 0;
	}

	int e, b, p;

	if(g_md->get_led_addr_byte_pin(p_name ? p_name : ats::String(), e, b, p))
	{
		return 0;
	}

	return g_md->m_gpio->get_gpio_context(&(p_bpc->m_bp.m_owner->name()), e, b, p, p_priority);
}

EXTERN_C void* bp_get_gpio_context(BlinkerProgramContext* p_bpc, int p_expander, int p_byte, int p_pin, int p_priority)
{

	if(!(p_bpc && p_bpc->m_bp.m_owner))
	{
		return 0;
	}

	return g_md->m_gpio->get_gpio_context(&(p_bpc->m_bp.m_owner->name()), p_expander, p_byte, p_pin, p_priority);
}

EXTERN_C void bp_put_gpio_context(void* p_gc)
{

	if(!p_gc)
	{
		return;
	}

	g_md->m_gpio->put_gpio_context((GPIOContext*)p_gc);
}

static void* blinker_program_thread(void* p)
{
	BlinkerProgramContext& bpc = *((BlinkerProgramContext*)p);
	bpc.m_bp.m_start();

	if(bpc.m_bp.m_owner)
	{
		bpc.m_bp.m_owner->m_md->add_to_reaper_list(bpc.m_bp.m_owner->name());
	}

	return 0;
}

static void h_unload_blinker_program(BlinkerProgramContextMap::iterator& p_i)
{
	BlinkerProgram& bp = (p_i->second).m_bp;
	MyData& md = *(bp.m_owner->m_md);
	bp.m_stop();
	bp.m_fini();
	pthread_join(bp.m_thread, 0);
	dlclose(bp.m_prog);
	bp.m_prog = 0;
	bp.m_owner->unset_app();
	md.m_gpio->flush(md);
}

ats::String MyData::load_blinker_program(Blinker& p_b, const ats::StringMap& p_arg)
{
	const ats::String& fname = p_arg.get("app");
	const char* emsg = p_b.set_app(fname);

	if(emsg)
	{
		return ("could not set app \"" + fname + "\": ") + emsg;
	}

	const ats::String& name = p_b.name();
	lock_blink_program();

	std::pair <BlinkerProgramContextMap::iterator, bool> r = g_bp.insert(BlinkerProgramContextPair(name, BlinkerProgramContext()));

	BlinkerProgram& bp = ((r.first)->second).m_bp;

	if(!(r.second))
	{

		if(p_arg.has_key("data"))
		{
			unlock_blink_program();
			bp.m_data(p_arg.get("data").c_str());
			return ats::String();
		}
		else
		{
			h_unload_blinker_program(r.first);
		}

	}

	BlinkerProgramContext& bpc = (r.first)->second;

	if(!(bp.m_prog = dlopen(fname.c_str(), RTLD_NOW)))
	{
		g_bp.erase(r.first);
		p_b.unset_app();
		unlock_blink_program();
		return dlerror();
	}

	bp.m_owner = &p_b;
	bp.m_init = (blink_init)dlsym(bp.m_prog, "blink_init");
	bp.m_start = (blink_start)dlsym(bp.m_prog, "blink_start");
	bp.m_stop = (blink_stop)dlsym(bp.m_prog, "blink_stop");
	bp.m_fini = (blink_fini)dlsym(bp.m_prog, "blink_fini");
	bp.m_data = (blink_data)dlsym(bp.m_prog, "blink_data");

	bp.m_init(&bpc);
	pthread_create(&(bp.m_thread), 0, blinker_program_thread, &bpc);
	unlock_blink_program();
	return ats::String();
}

void MyData::unload_blinker_program(const ats::String& p_key)
{
	lock_blink_program();
	BlinkerProgramContextMap::iterator i = g_bp.find(p_key);

	if(i != g_bp.end())
	{
		h_unload_blinker_program(i);
		g_bp.erase(i);
	}

	unlock_blink_program();
}

namespace MyMutex
{
enum MUTEX
{
	GENERAL,
	IRQ,
	SCHED,
	BLINK,
	BLINK_PROGRAM,
	GARBAGE,
	REAPER,
	MAX_MUTEX // This must always be last
};
}

MyData::MyData()
{
	{
		m_mutex = new pthread_mutex_t[MyMutex::MAX_MUTEX];

		for(int i = 0; i < MyMutex::MAX_MUTEX; ++i)
		{
			pthread_mutex_init(m_mutex + i, 0);
		}

	}

	memset(m_exp_addr, 0, sizeof(m_exp_addr));
	m_gpio = new GPIOManager();

	sem_init(&m_irq_sem, 0, 0);
	m_fd = -1;

	initialize_leds();

	sem_init(&m_reaper_sem, 0, 0);
	pthread_create(&m_reaper_thread, 0, MyData::blinker_reaper_thread, this);
}

void MyData::initialize_leds()
{
	// Red and Green LEDs
	m_led.set("cell",   "0,2,0");
	m_led.set("cell.r", "0,3,0");

	m_led.set("gps",    "0,2,1");
	m_led.set("gps.r",  "0,3,1");

	m_led.set("sat",    "0,2,2");
	m_led.set("sat.r",  "0,3,2");

	m_led.set("wifi",   "0,2,3");
	m_led.set("wifi.r", "0,3,3");

	m_led.set("zigbee",   "0,2,4");
	m_led.set("zigbee.r", "0,3,4");

	m_led.set("inp6",   "0,2,5");
	m_led.set("inp6.r", "0,3,5");
}

void* MyData::blinker_reaper_thread(void* p)
{
	MyData& md = *((MyData*)p);

	for(;;)
	{
		sem_wait(&(md.m_reaper_sem));
		md.lock_blink();
		md.lock_reaper();

		if(md.m_reaper_map.empty())
		{
			md.unlock_reaper();
			md.unlock_blink();
			continue;
		}

		std::vector <BlinkerMap::iterator> reap_list;
		std::vector <Blinker*> delete_list;
		delete_list.reserve(md.m_reaper_map.size());

		{
			reap_list.reserve(md.m_reaper_map.size());
			ReaperMap::iterator i = md.m_reaper_map.begin();

			while(i != md.m_reaper_map.end())
			{
				BlinkerMap::iterator j = md.m_BlinkerMap.find(i->first);
				++i;

				if(j != md.m_BlinkerMap.end())
				{
					reap_list.push_back(j);
				}

			}

			md.m_reaper_map.clear();
			md.unlock_reaper();
		}

		std::vector<BlinkerMap::iterator>::const_iterator i = reap_list.begin();

		while(i != reap_list.end())
		{
			Blinker* b;
			md.h_remove_blinker(*i, b);
			delete_list.push_back(b);
			++i;
		}

		md.unlock_blink();

		md.lock_garbage();

		for(size_t i = 0; i < delete_list.size(); ++i)
		{
			delete delete_list[i];
		}

		md.unlock_garbage();
	}

	return 0;
}

const char* MyData::get_led_addr_byte_pin(const ats::String& p_name, int& p_expander, int& p_byte, int& p_pin)
{
	const ats::String& info = m_led.get(p_name);

	if(info.empty())
	{
		return "LED name not found";
	}

	ats::StringList list;
	ats::split(list, info, ",");

	if(list.size() < 3)
	{
		return "LED spec wrong";
	}

	p_expander = strtol(list[0].c_str(), 0, 0);
	p_byte = strtol(list[1].c_str(), 0, 0);
	p_pin = strtol(list[2].c_str(), 0, 0);
	return 0;
}

void MyData::lock_irq() const
{
	pthread_mutex_lock(m_mutex + MyMutex::IRQ);
}

void MyData::unlock_irq() const
{
	pthread_mutex_unlock(m_mutex + MyMutex::IRQ);
}

void MyData::lock_blink() const
{
	pthread_mutex_lock(m_mutex + MyMutex::BLINK);
}

void MyData::unlock_blink() const
{
	pthread_mutex_unlock(m_mutex + MyMutex::BLINK);
}

void MyData::lock() const
{
	pthread_mutex_lock(m_mutex + MyMutex::GENERAL);
}

void MyData::unlock() const
{
	pthread_mutex_unlock(m_mutex + MyMutex::GENERAL);
}

void MyData::lock_blink_program() const
{
	pthread_mutex_lock(m_mutex + MyMutex::BLINK_PROGRAM);
}

void MyData::unlock_blink_program() const
{
	pthread_mutex_unlock(m_mutex + MyMutex::BLINK_PROGRAM);
}

void MyData::lock_reaper() const
{
	pthread_mutex_lock(m_mutex + MyMutex::REAPER);
}

void MyData::unlock_reaper() const
{
	pthread_mutex_unlock(m_mutex + MyMutex::REAPER);
}

void MyData::add_to_reaper_list(const ats::String& p_name)
{
	lock_reaper();

	if((m_reaper_map.insert(ReaperPair(p_name, 0))).second)
	{
		sem_post(&m_reaper_sem);
	}

	unlock_reaper();
}

void MyData::remove_from_reaper_list(const ats::String& p_name)
{
	lock_reaper();
	ReaperMap::iterator i = m_reaper_map.find(p_name);

	if(i != m_reaper_map.end())
	{
		m_reaper_map.erase(i);
	}

	unlock_reaper();
}

Blinker* MyData::add_blinker(const ats::String& p_name, const ats::String& p_led, const ats::String& p_script, bool& p_is_new, ats::String& p_emsg)
{
	lock_blink();
	Blinker* b;
	std::pair <BlinkerMap::iterator, bool> r = m_BlinkerMap.insert(BlinkerPair(p_name, 0));
	p_is_new = r.second;

	if(p_is_new)
	{
		b = new Blinker(*this, p_script);

		if((!p_led.empty()) && (!b->initialize_leds(p_led, p_emsg)))
		{
			m_BlinkerMap.erase(r.first);
			unlock_blink();
			delete b;
			return 0;
		}

		b->lock();
		(r.first)->second = b;
		b->m_name = &((r.first)->first);
	}
	else
	{
		b = (r.first)->second;
		b->lock();

		if(b->running_app(false))
		{
			b->unlock();
			unload_blinker_program(p_name);
			b->lock();
		}
		else if(b->h_running_script() && b->m_script.empty())
		{
			b->unlock();
			b->clean_up_script_thread();
			b->lock();
		}

	}

	remove_from_reaper_list(p_name);
	return b;
}

void MyData::garbage_collect_blinker(Blinker* p)
{

	if(!p)
	{
		return;
	}

	lock_garbage();
	m_BlinkerMap_garbage.insert(BlinkerPtrPair(p, 0));
	// XXX: Only the garbage collector is aware of Blinker* "b" now.
	unlock_garbage();
}

void MyData::remove_blinker(const ats::String& p_name)
{
	lock_blink();
	add_to_reaper_list(p_name);  
	unlock_blink();
}

void MyData::h_remove_blinker(const ats::String& p_name, Blinker*& p_b)
{
	h_remove_blinker(m_BlinkerMap.find(p_name), p_b);
}

void MyData::h_remove_blinker(BlinkerMap::iterator p_i, Blinker*& p_b)
{
	if(m_BlinkerMap.end() == p_i)
	{
		p_b = 0;
		return;
	}

	p_b = p_i->second;

	if(p_b->running_app())
	{
		unload_blinker_program(p_b->name());
	}
	else
	{
		p_b->stop();
		p_b->clean_up_script_thread();
	}

	detach_blinker(p_i);
	return;
}

void MyData::detach_blinker(BlinkerMap::iterator p_i)
{
	Blinker* b = p_i->second;
	b->detach();
	m_BlinkerMap.erase(p_i);
}

void MyData::lock_garbage() const
{
	pthread_mutex_lock(m_mutex + MyMutex::GARBAGE);
}

void MyData::unlock_garbage() const
{
	pthread_mutex_unlock(m_mutex + MyMutex::GARBAGE);
}

bool MyData::set_led(const ats::String& p_name, const bool p_val)
{
	return set_led(p_name, p_val, 0);
}

bool MyData::set_led(const ats::String& p_name, const bool p_val, const ats::String* p_owner)
{
	const ats::String& pin = m_led.get(p_name);

	if(pin.empty())
	{
		return false;
	}

	ats::StringList list;
	ats::split(list, pin, ",");

	if(list.size() >= 3)
	{
		const int expander = strtol(list[0].c_str(), 0, 0);
		const int byte = strtol(list[1].c_str(), 0, 0);
		const int pin = strtol(list[2].c_str(), 0, 0);
		::set_led(*this, p_owner, expander, byte, pin, p_val ? 0 : 1);  // inverted for 5000 from previous versions
		return true;
	}

	return false;
}
