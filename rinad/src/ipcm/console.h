/*
 * IPC Manager console
 *
 *    Vincenzo Maffione     <v.maffione@nextworks.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __IPCM_CONSOLE_H__
#define __IPCM_CONSOLE_H__

#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sstream>

#define RINA_PREFIX     "ipcm"

#include <librina/concurrency.h>
#include <librina/common.h>
#include <librina/ipc-manager.h>
#include <librina/logs.h>

#include "rina-configuration.h"
#include "helpers.h"
#include "ipcm.h"


namespace rinad {

class IPCManager;

typedef int (*console_function_t)(IPCManager *ipcm,
                                  std::vector<std::string>& args);

class IPCMConsole : public rina::Thread {
                static const unsigned int CMDBUFSIZE = 120;

                IPCManager& ipcm;

                std::map<std::string, console_function_t> commands_map;
                std::ostringstream outstream;

                int init();
                int process_command(int cfd, char *cmdbuf, int size);
                int flush_output(int cfd);

        public:
                IPCMConsole(IPCManager& r);
                void body();
                virtual ~IPCMConsole() throw();
};

}
#endif  /* __IPCM_CONSOLE_H__ */
