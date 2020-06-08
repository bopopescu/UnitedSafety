#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ats-common.h"
#include "ats-string.h"
#include "socket_interface.h"
#include "SocketQuery.h"
#include "command_line_parser.h"
#include "MyData.h"
#include "TestCommand.h"

bool TestCommand::m_running_server = false;
static bool g_running_uut_test = false;
static bool g_stop_uut_test = false;

const ats::String TestCommand::m_testmode("TestMode");

static void step1_configure_io(MyData& p_md)
{
	const ats::String* owner = &(TestCommand::m_testmode);
	int i;
	for(i = 0; i < 6; ++i)
	{ // [Input] Port
		write_expander(p_md, owner, EXP_1, 6, i, 1);
		write_expander(p_md, owner, EXP_1, 7, i, 1);
	}

	for(i = 0; i < 6; ++i)
	{ // [Output] Port
		write_expander(p_md, owner, EXP_2, 6, i, 1);
		write_expander(p_md, owner, EXP_2, 7, i, 1);
	}

	write_expander_hw(p_md, EXP_1, 6);
	write_expander_hw(p_md, EXP_1, 7);
	write_expander_hw(p_md, EXP_2, 6);
	write_expander_hw(p_md, EXP_2, 7);
}

static void step3_configure_output(MyData& p_md)
{
	const ats::String* owner = &(TestCommand::m_testmode);
	int i;

	for(i = 0; i < 6; ++i)
	{ // [Output] Port
		write_expander(p_md, owner, EXP_2, 7, i, 0);
	}

	write_expander_hw(p_md, EXP_2, 7);
}

static void step4_configure_output_low(MyData& p_md)
{
	const ats::String* owner = &(TestCommand::m_testmode);
	int i;

	for(i = 0; i < 6; ++i)
	{ // [Output] Port
		write_expander(p_md, owner, EXP_2, 2, i, 0);
		write_expander(p_md, owner, EXP_2, 3, i, 0);
	}

	write_expander_hw(p_md, EXP_2, 2);
	write_expander_hw(p_md, EXP_2, 3);
}

static void prepare_for_led_test(MyData& p_md)
{
	const ats::String* owner = &(TestCommand::m_testmode);
	int i;

	// LEDs will be on bytes 2 or 3 (which correspond to configuration bytes 6 or 7 respectively).
	int exp;
	int byte;
	int pin;
	TestCommand::get_led_info(p_md.m_led.get("act"), exp, byte, pin);
	write_expander(p_md, owner, exp, (2 == byte) ? 6 : 7, pin, 0);

	TestCommand::get_led_info(p_md.m_led.get("wifi"), exp, byte, pin);
	write_expander(p_md, owner, exp, (2 == byte) ? 6 : 7, pin, 0);

	TestCommand::get_led_info(p_md.m_led.get("sat"), exp, byte, pin);
	write_expander(p_md, owner, exp, (2 == byte) ? 6 : 7, pin, 0);

	TestCommand::get_led_info(p_md.m_led.get("gps"), exp, byte, pin);
	write_expander(p_md, owner, exp, (2 == byte) ? 6 : 7, pin, 0);

	TestCommand::get_led_info(p_md.m_led.get("cell"), exp, byte, pin);
	write_expander(p_md, owner, exp, (2 == byte) ? 6 : 7, pin, 0);

	write_expander_hw(p_md, EXP_1, 6);
	write_expander_hw(p_md, EXP_1, 7);
	write_expander_hw(p_md, EXP_2, 6);
	write_expander_hw(p_md, EXP_2, 7);

	for(i = 0; i < 6; ++i)
	{ // [Input] Port
		write_expander(p_md, owner, EXP_2, 6, i, 0);
	}

	write_expander_hw(p_md, EXP_2, 6);
}

static void step7_uut_led_test(MyData& p_md)
{
	// All outputs must be high to protect the FET on the UUT during the LED test.
	// This is because the UUT will set its inputs to outputs (not normal operation)
	// and may damage the FETs if the MTU is driving its outputs to low.
	const ats::String* owner = &(TestCommand::m_testmode);
	write_expander(p_md, owner, EXP_2, 3, 0, 1);
	write_expander(p_md, owner, EXP_2, 3, 1, 1);
	write_expander(p_md, owner, EXP_2, 3, 2, 1);
	write_expander(p_md, owner, EXP_2, 3, 3, 1);
	write_expander(p_md, owner, EXP_2, 3, 4, 1);
	write_expander(p_md, owner, EXP_2, 3, 5, 1);
	write_expander_hw(p_md, EXP_2, 3);
}

static void* test_server(void* p)
{
	ClientData* cd = (ClientData*)p;
	MyData& md = *((MyData*)(cd->m_data->m_hook));

	bool command_too_long = false;

	const size_t max_command_length = 1024;
	char cmdline_buf[max_command_length + 1];
	char* cmdline = cmdline_buf;

	ClientDataCache cdc;
	init_ClientDataCache(&cdc);

	CommandBuffer cb;
	init_CommandBuffer(&cb);

	for(;;)
	{
		char ebuf[256];
		const int c = client_getc_cached(cd, ebuf, sizeof(ebuf), &cdc);

		if(c < 0)
		{
			if(c != -ENODATA) ats_logf(ATSLOG(3), "%s,%d: %s", __FILE__, __LINE__, ebuf);
			break;
		}

		if((c != '\r') && (c != '\n'))
		{
			if(size_t(cmdline - cmdline_buf) >= max_command_length) command_too_long = true;
			else *(cmdline++) = c;

			continue;
		}

		if(command_too_long)
		{
			ats_logf(ATSLOG(0), "%s,%d: command is too long", __FILE__, __LINE__);
			cmdline = cmdline_buf;
			command_too_long = false;
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s,%d: command is too long\r", __FILE__, __LINE__);
			continue;
		}

		{
			const char* err = gen_arg_list(cmdline_buf, cmdline - cmdline_buf, &cb);
			cmdline = cmdline_buf;

			if(err)
			{
				ats_logf(ATSLOG(0), "%s,%d: gen_arg_list failed (%s)", __FILE__, __LINE__, err);
				ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s,%d: gen_arg_list failed: %s\r", __FILE__, __LINE__, err);
				continue;
			}

		}

		if(cb.m_argc <= 0)
		{
			continue;
		}

		const ats::String cmd(cb.m_argv[0]);

		if("step1_config_io" == cmd)
		{
			step1_configure_io(md);
			usleep(500 * 1000);
			usleep(500 * 1000);
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
		}
		else if("step2_check_io" == cmd)
		{
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
		}
		else if("step3_config_output" == cmd)
		{
			step3_configure_output(md);
			usleep(100 * 1000);
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
		}
		else if("step4_set_outputs" == cmd)
		{
			step4_configure_output_low(md);
			usleep(100 * 1000);
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
		}
		else if("step5_check_outputs" == cmd)
		{
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
		}
		else if("step6_check" == cmd)
		{
			ats::StringMap sm;
			sm.from_args(cb.m_argc - 1, cb.m_argv + 1);

			const int byte = sm.get_int("byte");
			const int pin = sm.get_int("pin");
			const int val = sm.get_int("val");

			if(byte || pin || val)
			{
				const ats::String* owner = &(TestCommand::m_testmode);
				write_expander(md, owner, EXP_2, byte, pin, val);
				write_expander_hw(md, EXP_2, byte);
			}

			usleep(100 * 1000);
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
		}
		else if("step6_tell" == cmd)
		{
			ats::StringMap sm;
			sm.from_args(cb.m_argc - 1, cb.m_argv + 1);

			const int byte = sm.get_int("byte");
			int v;
			read_expander_hw(md, EXP_2, byte, v);

			ClientData_send_cmd(cd, MSG_NOSIGNAL, "%s: ok\nval=0x%02X\n\r", cmd.c_str(), v);

			// AWARE360 FIXME: UUT devices will not ask for step7, so force it here.
			step7_uut_led_test(md);
		}
		else if("step7_uut_led_test" == cmd)
		{
			step7_uut_led_test(md);
		}
		else
		{
			ClientData_send_cmd(cd, MSG_NOSIGNAL, "Invalid command \"%s\"\n\r", cmd.c_str());
		}

	}

	return 0;
}

// Description: Turns off all INPUT/OUTPUT LEDs provided that the I/O Expander
//	polarity pins are set correctly/in-standard-mode.
static void blank_input_output_leds(MyData& p_md)
{
	const ats::String* owner = &(TestCommand::m_testmode);
	int byte = 2;
	int i;

	for(i = 0; i < 6; ++i)
	{
		write_expander(p_md, owner, EXP_2, byte, i, 1);
		write_expander_hw(p_md, EXP_2, byte);
	}

	byte = 3;

	for(i = 0; i < 6; ++i)
	{
		write_expander(p_md, owner, EXP_2, byte, i, 0);
		write_expander_hw(p_md, EXP_2, byte);
	}

}

static void blank_leds(MyData& p_md, const ats::String* p_owner)
{
	int exp;
	int byte;
	int pin;
	TestCommand::get_led_info(p_md.m_led.get("act"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);

	TestCommand::get_led_info(p_md.m_led.get("wifi"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);

	TestCommand::get_led_info(p_md.m_led.get("sat"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);

	TestCommand::get_led_info(p_md.m_led.get("gps"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);

	TestCommand::get_led_info(p_md.m_led.get("cell"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
}

static void led_phase1(MyData& p_md, const ats::String* p_owner)
{
	int exp;
	int byte;
	int pin;
	TestCommand::get_led_info(p_md.m_led.get("act"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 0);

	TestCommand::get_led_info(p_md.m_led.get("wifi"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 0);

	TestCommand::get_led_info(p_md.m_led.get("sat"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 0);

	TestCommand::get_led_info(p_md.m_led.get("gps"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 0);

	TestCommand::get_led_info(p_md.m_led.get("cell"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
}

static void led_phase2(MyData& p_md, const ats::String* p_owner)
{
	int exp;
	int byte;
	int pin;
	TestCommand::get_led_info(p_md.m_led.get("act"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);

	TestCommand::get_led_info(p_md.m_led.get("wifi"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);

	TestCommand::get_led_info(p_md.m_led.get("sat"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);

	TestCommand::get_led_info(p_md.m_led.get("gps"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);

	TestCommand::get_led_info(p_md.m_led.get("cell"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
}

static void led_phase3(MyData& p_md, const ats::String* p_owner)
{
	int exp;
	int byte;
	int pin;
	TestCommand::get_led_info(p_md.m_led.get("act"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 1);

	TestCommand::get_led_info(p_md.m_led.get("wifi"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 1);

	TestCommand::get_led_info(p_md.m_led.get("sat"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 1);

	TestCommand::get_led_info(p_md.m_led.get("gps"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 1);

	TestCommand::get_led_info(p_md.m_led.get("cell"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
	write_expander(p_md, p_owner, exp, byte, pin, 1);
	write_expander_hw(p_md, exp, byte);
}

static void led_phase4(MyData& p_md, const ats::String* p_owner)
{
	int exp;
	int byte;
	int pin;
	TestCommand::get_led_info(p_md.m_led.get("act"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);

	TestCommand::get_led_info(p_md.m_led.get("wifi"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);

	TestCommand::get_led_info(p_md.m_led.get("sat"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);

	TestCommand::get_led_info(p_md.m_led.get("gps"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);

	TestCommand::get_led_info(p_md.m_led.get("cell"), exp, byte, pin);
	write_expander(p_md, p_owner, exp, byte, pin, 0);
	write_expander_hw(p_md, exp, byte);
	usleep(250 * 1000);
}

static ats::String do_uut_test(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb, ats::StringMap& p_sm, FILE* p_f)
{
	p_md.lock();

	if(g_running_uut_test)
	{
		p_md.unlock();
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "Already running UUT test\"\n");
		return "N/A (Not tested, test already in progress)";
	}

	bool result = true;
	ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "STARTING_UUT_TEST\n");
	fprintf(p_f, "STARTING_UUT_TEST\n");
	g_running_uut_test = true;
	p_md.unlock();

			const ats::String& peer_ip = p_sm.get("peer");
			ClientSocket* cs = new ClientSocket();
			init_ClientSocket(cs);
			cs->m_keepalive_count = 3;
			cs->m_keepalive_idle = 1;
			cs->m_keepalive_interval = 1;
			const int port = 12345;
			ats::SocketQuery q(peer_ip, port, cs);

			// 1)  All I2C Chip input/output should be configured as High impedance inputs for both the UUT and test fixture.
			ats::String resp;
			step1_configure_io(p_md);
			resp = q.query("step1_config_io");

			// 2)  500ms later a read should show: OUTx should all read low, INx should all read high. Default state
			resp = q.query("step2_check_io");

			const ats::String exp1_addr(p_md.m_config.get("gpio-expander-address"));
			const ats::String exp2_addr(p_md.m_config.get("gpio-expander-address2"));

			int v;
			read_expander_hw(p_md, EXP_2, 0, v);
			const int valid_pin_mask = 0x3f;

			if(v & valid_pin_mask)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "1. FAIL: EXP_2(%s) Input register 0 is 0x%02X, not 0x0\n", exp2_addr.c_str(), v);
				fprintf(p_f, "1. FAIL: EXP_2(%s) Input register 0 is 0x%02X, not 0x0\n", exp2_addr.c_str(), v);
				result = false;
			}
			else
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "1. PASS: EXP_2(%s) Input register 0 is 0x0\n", exp2_addr.c_str());
				fprintf(p_f, "1. PASS: EXP_2(%s) Input register 0 is 0x0\n", exp2_addr.c_str());
			}

			read_expander_hw(p_md, EXP_2, 1, v);

			if(v & valid_pin_mask)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "2. FAIL: EXP_2(%s) Input register 1 is 0x%02X, not 0x0\n", exp2_addr.c_str(), v);
				fprintf(p_f, "2. FAIL: EXP_2(%s) Input register 1 is 0x%02X, not 0x0\n", exp2_addr.c_str(), v);
				result = false;
			}
			else
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "2. PASS: EXP_2(%s) Input register 1 is 0x0\n", exp2_addr.c_str());
				fprintf(p_f, "2. PASS: EXP_2(%s) Input register 1 is 0x0\n", exp2_addr.c_str());
			}

			// 3)  Configure all OUTx to outputs on both units.
			step3_configure_output(p_md);
			resp = q.query("step3_config_output");

			// 4)  Set all OUTx to low
			step4_configure_output_low(p_md);
			resp = q.query("step4_set_outputs");

			// 5)  100ms later all INx should read low
			resp = q.query("step5_check_outputs");

			read_expander_hw(p_md, EXP_2, 0, v);

			if(v & valid_pin_mask)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "3. FAIL: EXP_2(%s) Input register 0 is 0x%02X, not 0x0\n", exp2_addr.c_str(), v);
				fprintf(p_f, "3. FAIL: EXP_2(%s) Input register 0 is 0x%02X, not 0x0\n", exp2_addr.c_str(), v);
				result = false;
			}
			else
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "3. PASS: EXP_2(%s) Input register 0 is 0x0\n", exp2_addr.c_str());
				fprintf(p_f, "3. PASS: EXP_2(%s) Input register 0 is 0x0\n", exp2_addr.c_str());
			}

			read_expander_hw(p_md, EXP_2, 1, v);

			if(v & valid_pin_mask)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "4. FAIL: EXP_2(%s) Input register 1 is 0x%02X, not 0x0\n", exp2_addr.c_str(), v);
				fprintf(p_f, "4. FAIL: EXP_2(%s) Input register 1 is 0x%02X, not 0x0\n", exp2_addr.c_str(), v);
				result = false;
			}
			else
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "4. PASS: EXP_2(%s) Input register 1 is 0x0\n", exp2_addr.c_str());
				fprintf(p_f, "4. PASS: EXP_2(%s) Input register 1 is 0x0\n", exp2_addr.c_str());
			}

			// 6)  Toggle one OUT at a time high and read the inputs 100ms later to verify the corresponding INx goes high and no others
			int i;

			for(i = 0; i < 6; ++i)
			{
				ats::String s;
				ats_sprintf(&s, "step6_check byte=3 pin=%i val=%d", i, 1);
				resp = q.query(s);

				if(i > 0)
				{
					ats_sprintf(&s, "step6_check byte=3 pin=%i val=%d", i - 1, 0);
					resp = q.query(s);
				}

				read_expander_hw(p_md, EXP_2, 0, v);
				const bool pass = (i ? (1 << i) : 1) == (v & valid_pin_mask);

				if(!pass)
				{
					result = false;
				}

				const char* msg = pass ? "PASS" : "FAIL";
				const char* level = pass ? "HIGH-ALONE" : "NOT-HIGH-ALONE";

				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "5%c. %s: UUT Input pin %d is %s [0x%02X]\n", 'a' + i, msg, i, level, v);
				fprintf(p_f, "5%c. %s: UUT Input pin %d is %s [0x%02X]\n", 'a' + i, msg, i, level, v);
			}

			// UUT side

			int byte = 3;

			for(i = 0; i < 6; ++i)
			{
				const ats::String* owner = &(TestCommand::m_testmode);
				write_expander(p_md, owner, EXP_2, byte, i, 1);

				if(i > 0)
				{
					write_expander(p_md, owner, EXP_2, byte, i - 1, 0);
				}

				write_expander_hw(p_md, EXP_2, byte);
				usleep(100 * 1000);

				ats::String s;
				ats_sprintf(&s, "step6_tell byte=0");
				resp = q.query(s);

				ats::StringList sl;
				ats::split(sl, resp, "\n");

				ats::String resp_val;
				int pin_val = 0;

				bool pass = false;

				if(sl.size() >= 2)
				{
					resp_val = sl[1];
					ats::String k;	
					ats::String v;
					ats::StringMap::get_key_val(resp_val.c_str(), k, v);

					const int pin = i ? (1 << i) : 1;
					pin_val = strtol(v.c_str(), 0, 0);

					if(("val" == k) && (pin == pin_val))
					{
						pass = true;
					}
				}

				if(!pass)
				{
					result = false;
				}

				const char* msg = pass ? "PASS" : "FAIL";
				const char* level = pass ? "HIGH-ALONE" : "NOT-HIGH-ALONE";
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "6%c. %s: MTU Input pin %d is %s [0x%02X, \"%s\"]\n", 'a' + i, msg, i, level, pin_val, resp_val.c_str());
				fprintf(p_f, "6%c. %s: MTU Input pin %d is %s [0x%02X, \"%s\"]\n", 'a' + i, msg, i, level, pin_val, resp_val.c_str());
			}

			// 7)
			prepare_for_led_test(p_md);

			const ats::String* owner = &(TestCommand::m_testmode);
			blank_leds(p_md, owner);
			blank_input_output_leds(p_md);

			const int order[] = {5, 4, 3, 2, 1, 0};

			while(!g_stop_uut_test)
			{
				led_phase1(p_md, owner);

				byte = 3;

				for(i = 0; i < 6; ++i)
				{
					write_expander(p_md, owner, EXP_2, byte, order[i], 1);

					if(i > 0)
					{
						write_expander(p_md, owner, EXP_2, byte, order[i-1], 0);
					}

					write_expander_hw(p_md, EXP_2, byte);
					usleep(250 * 1000);
				}
				write_expander(p_md, owner, EXP_2, byte, order[i-1], 0);
				write_expander_hw(p_md, EXP_2, byte);

				if(g_stop_uut_test)
				{
					break;
				}

				byte = 2;

				for(i = 0; i < 6; ++i)
				{
					write_expander(p_md, owner, EXP_2, byte, order[i], 0);

					if(i > 0)
					{
						write_expander(p_md, owner, EXP_2, byte, order[i - 1], 1);
					}

					write_expander_hw(p_md, EXP_2, byte);
					usleep(250 * 1000);
				}

				write_expander(p_md, owner, EXP_2, byte, order[i-1], 1);
				write_expander_hw(p_md, EXP_2, byte);

				if(g_stop_uut_test)
				{
					break;
				}

				// Phase 2
				led_phase2(p_md, owner);
				byte = 3;

				for(i = 0; i < 6; ++i)
				{
					write_expander(p_md, owner, EXP_2, byte, order[i], 1);
					write_expander_hw(p_md, EXP_2, byte);
					usleep(250 * 1000);
				}

				if(g_stop_uut_test)
				{
					break;
				}

				byte = 2;

				for(i = 0; i < 6; ++i)
				{
					write_expander(p_md, owner, EXP_2, byte, order[i], 0);
					write_expander_hw(p_md, EXP_2, byte);
					usleep(250 * 1000);
				}

				if(g_stop_uut_test)
				{
					break;
				}

				// Phase 3
				led_phase3(p_md, owner);
				byte = 3;

				for(i = 0; i < 6; ++i)
				{
					write_expander(p_md, owner, EXP_2, byte, order[i], 0);

					if(i > 0)
					{
						write_expander(p_md, owner, EXP_2, byte, order[i-1], 1);
					}

					write_expander_hw(p_md, EXP_2, byte);
					usleep(250 * 1000);
				}
				write_expander(p_md, owner, EXP_2, byte, order[i-1], 1);
				write_expander_hw(p_md, EXP_2, byte);

				if(g_stop_uut_test)
				{
					break;
				}

				byte = 2;

				for(i = 0; i < 6; ++i)
				{
					write_expander(p_md, owner, EXP_2, byte, order[i], 1);

					if(i > 0)
					{
						write_expander(p_md, owner, EXP_2, byte, order[i - 1], 0);
					}

					write_expander_hw(p_md, EXP_2, byte);
					usleep(250 * 1000);
				}

				write_expander(p_md, owner, EXP_2, byte, order[5], 0);
				write_expander_hw(p_md, EXP_2, byte);

				if(g_stop_uut_test)
				{
					break;
				}

				// Phase 4
				led_phase4(p_md, owner);
				byte = 3;

				for(i = 0; i < 6; ++i)
				{
					write_expander(p_md, owner, EXP_2, byte, order[i], 0);
					write_expander_hw(p_md, EXP_2, byte);
					usleep(250 * 1000);
				}

				if(g_stop_uut_test)
				{
					break;
				}

				byte = 2;

				for(i = 0; i < 6; ++i)
				{
					write_expander(p_md, owner, EXP_2, byte, order[i], 1);
					write_expander_hw(p_md, EXP_2, byte);
					usleep(250 * 1000);
				}

			}

			p_md.lock();
			g_running_uut_test = false;
			g_stop_uut_test = false;
			p_md.unlock();

	return result ? "PASS" : "FAIL";
}

void TestCommand::fn(MyData& p_md, ClientData& p_cd, const CommandBuffer& p_cb)
{

	const ats::String cmd(p_cb.m_argv[1]);

	if("mfg" == cmd)
	{

		// mfg [option=val] ... [option=val]
		ats::StringMap sm;
		sm.from_args(p_cb.m_argc - 2, p_cb.m_argv + 2);

		const bool uut_mode = sm.get("mode") != "mtu";

		if(uut_mode)
		{
			const char* test_fname = "/var/log/io-test.txt";
			FILE* f = fopen(test_fname, "w");

			if(f)
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s: ok\n", cmd.c_str());
				const ats::String& result = do_uut_test(p_md, p_cd, p_cb, sm, f);
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "TEST_COMPLETE: %s\n\r", result.c_str());
				fprintf(f, "TEST_COMPLETE: %s\n\r", result.c_str());
				fclose(f);
			}
			else
			{
				ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s: error\nCould not open \"%s\" for writing. (%d) %s\n\r", cmd.c_str(), test_fname, errno, strerror(errno));
			}

		}

	}
	else if("stop_uut_test" == cmd)
	{
		p_md.lock();

		if(g_running_uut_test)
		{
			g_stop_uut_test = true;
			p_md.unlock();
			ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s: ok\nStopping Test\n\r", cmd.c_str());
			return;
		}

		p_md.unlock();
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s: ok\n\r", cmd.c_str());
	}
	else
	{
		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "error\nInvalid Test command \"%s\"\n", cmd.c_str());
	}

}

void TestCommand::start_test_server(MyData& p_md)
{
	p_md.lock();

	if(m_running_server)
	{
		p_md.unlock();
//		ClientData_send_cmd(&p_cd, MSG_NOSIGNAL, "%s: \nAlready running a server\n", cmd.c_str());
		return;
	}

	p_md.unlock();

	static ServerData sd;
	init_ServerData(&sd, 64);
	sd.m_port = 12345;
	sd.m_hook = &p_md;
	sd.m_cs = test_server;
	::start_server(&sd);
}

void TestCommand::get_led_info(const ats::String& p_led_spec, int& p_exp, int& p_byte, int& p_pin)
{
	ats::StringList s;
	ats::split(s, p_led_spec, ",");

	p_exp = (s.size() >= 1) ? strtol(s[0].c_str(), 0, 0) : 0;
	p_byte = (s.size() >= 2) ? strtol(s[1].c_str(), 0, 0) : 2;
	p_pin = (s.size() >= 3) ? strtol(s[2].c_str(), 0, 0) : 0;
}
