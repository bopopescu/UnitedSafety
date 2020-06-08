/*
 * Copyright © 2008-2010 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <modbus.h>
#include "modbus-private.h"
#include "SysModbusRegisters.h"

#define MODBUS_MAX_HOLD_REGISTERS	MODBUS_MAX_REGISTERS
#define SERIAL_DEV_NAME	"/dev/ttySP1"

int main(int argc, char *argv[])
{
  modbus_t *ctx;
  int rc;

  ctx = modbus_new_rtu(SERIAL_DEV_NAME, 115200, 'N', 8, 1);
  modbus_set_slave(ctx, 1);
  modbus_connect(ctx);

  mb_mapping = modbus_mapping_new(0, 0,
                                  MODBUS_MAX_HOLD_REGISTERS, 0);
  if (mb_mapping == NULL) {
    fprintf(stderr, "Failed to allocate the mapping: %s\n",
            modbus_strerror(errno));
    modbus_free(ctx);
    return -1;
  }
  CheckAndLoadNewParameters(mb_mapping->tab_registers, MODBUS_MAX_HOLD_REGISTERS);
	
  if(GetHolderRegister(MODBUS_FUNC_ENABLE) == 0)	//Disable
    {
      printf("Check Modbus is disabled\n");
      goto exit_modbus_slaver;
    }
  else
    {
      printf("Modbus is enabled\n");
    }
	
  for(;;) {
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

    rc = modbus_receive(ctx, query);
    if (rc >= 0) {
      int offset = ctx->backend->header_length;
      //int slave = query[offset - 1];
      int function = query[offset];
      //uint16_t address = (query[offset + 1] << 8) + query[offset + 2];
	  printf("Function: 0x%x\n", function);
      if(function == _FC_READ_HOLDING_REGISTERS ||
         function == _FC_WRITE_SINGLE_REGISTER ||
		 function == _FC_WRITE_MULTIPLE_REGISTERS ||
         function == _FC_WRITE_AND_READ_REGISTERS)
        {
          if(modbus_register_handler(ctx, query, rc, mb_mapping) > 0)
            {
              modbus_reply(ctx, query, rc, mb_mapping);
            }
          else
            {
              modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            }
        }
      else
        {
          modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
        }
		
    } else {
      /* Connection closed by the client or server */
	  continue;
    }
    CheckAndSaveParameters(mb_mapping->tab_registers, MODBUS_MAX_HOLD_REGISTERS);
  }
	
 exit_modbus_slaver:
  printf("Quit the loop: %s\n", modbus_strerror(errno));

  modbus_mapping_free(mb_mapping);
  modbus_free(ctx);

  return 0;
}
