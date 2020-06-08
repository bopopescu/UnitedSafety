#pragma once

#include "ats-common.h"
#include "gcc.h"

#define LED_ON (0)
#define LED_OFF (1)

struct BlinkerProgramContext;

typedef void* bp_StringMap;

EXTERN_C const char* bp_post_command(BlinkerProgramContext*, const char* p_cmd);

EXTERN_C const char* bp_get_value(const bp_StringMap, const char*);

EXTERN_C const char* bp_get_name(BlinkerProgramContext* bpc);

EXTERN_C int bp_get_led_addr_byte_pin(const char* p_name, int* p_expander, int* p_byte, int* p_pin);

EXTERN_C void bp_set_named_led(BlinkerProgramContext* bpc, const char** p_owner, const char* p_name, int p_value);

EXTERN_C void bp_set_led(BlinkerProgramContext* bpc, const char** p_owner, int p_expander, int p_byte, int p_pin, int p_value);

EXTERN_C void* bp_get_named_gpio_context(BlinkerProgramContext* p_bpc, const char* p_name, int p_priority);

EXTERN_C void* bp_get_gpio_context(BlinkerProgramContext* p_bpc, int p_expander, int p_byte, int p_pin, int p_priority);

EXTERN_C void bp_put_gpio_context(void* p_gc);
