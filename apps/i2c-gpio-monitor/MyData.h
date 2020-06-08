#pragma once

#include <semaphore.h>
#include <set>

#include "ats-string.h"
#include "state_machine.h"
#include "ClientMessageManager.h"
#include "GPIOManager.h"
#include "Blinker.h"

#define EXP_1 0
#define EXP_2 1

class MyData : public StateMachineData
{
public:
	CommandMap m_cmd;
	BlinkerMap m_BlinkerMap;
	ats::StringMap m_led;
	ats::StringMap m_irq_task;
	ats::StringMap m_config;

private:
	BlinkerPtrMap m_BlinkerMap_garbage;

public:
	int m_exp_addr[2];
	sem_t m_irq_sem;
	pthread_t m_irq_thread;
	pthread_t m_irq_task_thread;
	pthread_t m_debounce_thread;
	pthread_t m_inputLED_thread;
	int m_fd;

	GPIOManager* m_gpio;

	const char* get_led_addr_byte_pin(const ats::String& p_name, int& p_expander, int& p_byte, int& p_pin);

	MyData();

	void initialize_leds();

	void lock_irq() const;

	void unlock_irq() const;

	void lock_blink() const;

	void unlock_blink() const;

	void lock_blink_program() const;

	void unlock_blink_program() const;

	void lock() const;

	void unlock() const;

	bool set_led(const ats::String& p_name, bool p_val);

	bool set_led(const ats::String& p_name, const bool p_val, const ats::String* p_owner);

	// Description: Adds a new blinker and aquires "lock_blink" which the caller MUST release
	//	by calling "unlock_blink". On error no blinker is returned and no locks are held.
	//
	//	"p_led" is a comma separated list of LED names. The named LEDs will be driven by the
	//	blinker.
	//
	//	"p_is_new" is set to true if the requested blinker named "p_name" did not exist previously
	//	(and thus was created). "p_is_new" is set to false if blinker "p_name" already exists.
	//	Callers can use "p_is_new" to determine if they must perform initial blinker setup, or
	//	just update existing blinker settings.
	//
	//	"p_emsg" will contain the error message if NULL is returned.
	//
	// Return: A valid blinker pointer is returned and "lock_blink" is held on success. The caller
	//	MUST call "unlock_blink" as soon as they are finished with the blinker. On error, NULL is
	//	returned and "lock_blink" is not held, so the caller must not call "unlock_blink".
	Blinker* add_blinker(const ats::String& p_name, const ats::String& p_led, const ats::String& p_script, bool& p_is_new, ats::String& p_emsg);

	// Description: Immediately removes the named blinker.
	//
	// NOTE: Blinkers are removed automatically when they expire. This function is useful when the caller
	//	wishes to remove the named blinker immediately.
	void remove_blinker(const ats::String& p_name);

	ats::String load_blinker_program(Blinker& p_b, const ats::StringMap& p_arg);
	void unload_blinker_program(const ats::String& p_key);

	void add_to_reaper_list(const ats::String& p_name);

	// Description: List for Unix domain sockets or regular sockets.
	std::set<std::string> m_udsSet;

private:
	pthread_mutex_t* m_mutex;

	void lock_garbage() const;

	void unlock_garbage() const;

	// Description: Performs the actual "detach" operation (disassociate the blinker object pointer
	//	from its name) on blinker "p_i". The blinkers previous name will be immediately available
	//	for use again (by "add_blinker").
	void detach_blinker(BlinkerMap::iterator p_i);

	void lock_reaper() const;

	void unlock_reaper() const;

	pthread_t m_reaper_thread;
	sem_t m_reaper_sem;
	static void* blinker_reaper_thread(void*);

	void remove_from_reaper_list(const ats::String& p_name);

	void h_remove_blinker(const ats::String& p_name, Blinker*& p);
	void h_remove_blinker(BlinkerMap::iterator p_i, Blinker*& p_b);

	void garbage_collect_blinker(Blinker* p);

	typedef std::map <const ats::String, void*> ReaperMap;
	typedef std::pair <const ats::String, void*> ReaperPair;

	// Description: Mapping of blinkers to reap.
	ReaperMap m_reaper_map;

};

void set_led(MyData& p_md, const ats::String* p_owner, int p_expander, int p_byte, int p_pin, int p_val);

void init_post_command();

ats::String post_command(const ats::String& p_cmd);

int read_expander_hw(MyData& p_md, int p_expander, int p_byte, int& p_val);

int write_expander_hw(MyData& p_md, int p_expander, int p_byte);

void write_expander(MyData& p_md, const ats::String* p_owner, int p_expander, int p_byte, int p_pin, int p_val);
