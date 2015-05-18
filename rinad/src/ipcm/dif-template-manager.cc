/*
 * DIF Template Manager
 *
 *    Eduard Grasa     <eduard.grasa@i2cat.net>
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

#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <sys/inotify.h>

#define RINA_PREFIX     "ipcm.dif-template-manager"
#include <librina/logs.h>

#include "configuration.h"
#include "dif-template-manager.h"

using namespace std;


namespace rinad {

//Class DIF Template Monitor
DIFTemplateMonitor::DIFTemplateMonitor(rina::ThreadAttributes * thread_attrs,
		   	   	       const std::string& folder,
		   	   	       DIFTemplateManager * dtm) :
		   	   			       rina::SimpleThread(thread_attrs)
{
	folder_name = folder;
	stop = false;
	dif_template_manager = dtm;
}

DIFTemplateMonitor::~DIFTemplateMonitor() throw()
{
}

void DIFTemplateMonitor::do_stop()
{
	rina::ScopedLock g(lock);
	stop = true;
}

bool DIFTemplateMonitor::has_to_stop()
{
	rina::ScopedLock g(lock);
	return stop;
}

static bool str_ends_with(const std::string& str, const std::string& suffix)
{
	if (suffix.size() > str.size()) {
		return false;
	}

	return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

void DIFTemplateMonitor::process_events(int fd)
{
        char buf[4096]
            __attribute__ ((aligned(__alignof__(struct inotify_event))));
        const struct inotify_event *event;
        int i;
        ssize_t len;
        char *ptr;
        std::stringstream ss;
        DIFTemplate * dif_template;

        for (;;) {
                len = read(fd, buf, sizeof buf);
                if (len == -1 && errno != EAGAIN) {
                    LOG_ERR("Problems reading inotify file descriptor");
                    return;
                }

                if (len <= 0)
                    return;

                for (ptr = buf; ptr < buf + len;
                		ptr += sizeof(struct inotify_event) + event->len) {
                	event = (const struct inotify_event *) ptr;
                	std::string file_name = std::string(event->name);

                	//ignore *.swp, *.swx, *~*, *4913*
                	if (file_name.find(".swx") != std::string::npos ||
                			file_name.find(".swp") != std::string::npos ||
                			file_name.find("~") != std::string::npos ||
                			file_name.find("4913") != std::string::npos) {
                		continue;
                	}

			if (! str_ends_with(file_name, ".dif")) {
				continue;
			}

                	if (file_name == "ipcmanager.conf") {
                		continue;
                	}

                	if (event->mask & IN_CLOSE_WRITE)
                		LOG_DBG("The file of DIF template %s has been modified.",
                				event->name);

                		ss << folder_name << "/" << file_name;
                		dif_template = parse_dif_template(ss.str(), file_name);
                		ss.str(std::string());
                		if (dif_template != 0) {
                			//TODO augment dif_template with the defaults
                			dif_template_manager->add_dif_template(file_name, dif_template);
                		}
                	if (event->mask & IN_DELETE) {
                		LOG_DBG("The file of DIF template %s has been deleted.",
                				event->name);
                		dif_template_manager->remove_dif_template(file_name);
                	}
                }
        }
}

int DIFTemplateMonitor::run()
{
	int fd;
	int wd;
	int pollnum;
	struct pollfd fds[1];

	LOG_DBG("DIF Template monitor started, monitoring folder %s",
			folder_name.c_str());

	fd = inotify_init1(IN_NONBLOCK);
	if (fd == -1) {
		LOG_ERR("Error initializing inotify, stopping DIF template monitor");
		return -1;
	}

	wd = inotify_add_watch(fd, folder_name.c_str(), IN_CLOSE_WRITE | IN_DELETE);
	if (wd == -1) {
		LOG_ERR("Error adding a watch, stopping DIF template monitor");
		close(fd);
		return -1;
	}

	fds[0].fd = fd;
	fds[0].events = POLLIN;

	while (!has_to_stop()) {
		pollnum = poll(fds, 1, 1000);

		if (pollnum == EINVAL) {
			LOG_ERR("Poll returned EINVAL, stopping DIF template monitor");
			return -1;
		}

		if (pollnum <= 0) {
			//No changes during this period or error that can be ignored
			continue;
		}

		if (fds[0].revents & POLLIN) {
			process_events(fd);
		}
	}

        /* Close inotify file descriptor */
        close(fd);

        LOG_DBG("DIF Template Manager stopped");

	return 0;
}

//Class DIF Template Manager
const std::string DIFTemplateManager::DEFAULT_TEMPLATE_NAME = "default.dif";

DIFTemplateManager::DIFTemplateManager(const std::string& folder)
{
	folder_name = folder;
	default_template = 0;

	//load current templates from template folder
	if (load_initial_dif_templates() != 0) {
		return;
	}

	//Create a thread that monitors the DIF template folder when required
	rina::ThreadAttributes thread_attrs;
	thread_attrs.setJoinable();
	template_monitor = new DIFTemplateMonitor(&thread_attrs, folder, this);
}

DIFTemplateManager::~DIFTemplateManager()
{
	void * status;

	if (template_monitor) {
		template_monitor->do_stop();
		template_monitor->join(&status);

		delete template_monitor;
	}

	//TODO destroy all DIF templates in the map
}

int DIFTemplateManager::load_initial_dif_templates()
{
	DIR *dirp;
	struct dirent *dp;
	DIFTemplate * dif_template;
	std::string file_name;
	std::stringstream ss;
	std::list<rinad::DIFTemplate *> templates;

	if ((dirp = opendir(folder_name.c_str())) == NULL) {
		LOG_ERR("Failed to open folder %s", folder_name.c_str());
		return -1;
	}

	do {
		errno = 0;
		if ((dp = readdir(dirp)) != NULL) {
			if (! str_ends_with(std::string(dp->d_name), ".dif")) {
				continue;
			}

			LOG_DBG("Found DIF template file called: %s", dp->d_name);
			file_name = std::string(dp->d_name);
			ss << folder_name << "/" << file_name;
			dif_template = parse_dif_template(ss.str(), file_name);
			ss.str(std::string());

			if (strcmp(dp->d_name, DEFAULT_TEMPLATE_NAME.c_str()) == 0) {
				LOG_DBG("Default DIF template found");
				default_template = dif_template;
				add_dif_template(file_name, dif_template);
			} else {
				templates.push_back(dif_template);
			}
		}
	} while (dp != NULL);

	(void) closedir(dirp);

	//Add all templates, cannot be done before because first we need to add
	//the default template (if present)
	for (std::list<rinad::DIFTemplate *>::iterator it = templates.begin();
			it != templates.end(); ++it) {
		add_dif_template((*it)->templateName, *it);
	}

	if (!default_template) {
		LOG_WARN("Default DIF template not present");
	}

	return 0;
}

rinad::DIFTemplate * DIFTemplateManager::get_dif_template(const std::string& name)
{
	rina::ReadScopedLock g(templates_lock);

	std::map<std::string, rinad::DIFTemplate*>::iterator it = dif_templates.find(name);
	if (it == dif_templates.end()) {
		return 0;
	} else {
		return it->second;
	}
}

void DIFTemplateManager::augment_dif_template(rinad::DIFTemplate * dif_template)
{
	if (!default_template || dif_template->templateName == DEFAULT_TEMPLATE_NAME ||
			dif_template->difType != rina::NORMAL_IPC_PROCESS) {
		return;
	}

	if (dif_template->dataTransferConstants.address_length_ == 0) {
		dif_template->dataTransferConstants = default_template->dataTransferConstants;
	}

	if (dif_template->qosCubes.size() == 0) {
		for (std::list<rina::QoSCube>::iterator it = default_template->qosCubes.begin();
				it != default_template->qosCubes.end(); ++it) {
			dif_template->qosCubes.push_back(rina::QoSCube(*it));
		}
	}

	if (dif_template->etConfiguration.declared_dead_interval_in_ms_ == 120000 &&
			dif_template->etConfiguration.enrollment_timeout_in_ms_ == 10000 &&
			dif_template->etConfiguration.max_number_of_enrollment_attempts_ == 3 &&
			dif_template->etConfiguration.neighbor_enroller_period_in_ms_ == 10000 &&
			dif_template->etConfiguration.watchdog_period_in_ms_ == 60000) {
		dif_template->etConfiguration = default_template->etConfiguration;
	}

	if (dif_template->rmtConfiguration.max_queue_policy_.name_ == "") {
		dif_template->rmtConfiguration = default_template->rmtConfiguration;
	}

	if (dif_template->knownIPCProcessAddresses.size() == 0) {
		for (std::list<rinad::KnownIPCProcessAddress>::iterator it =
				default_template->knownIPCProcessAddresses.begin();
				it != default_template->knownIPCProcessAddresses.end(); ++it) {
			dif_template->knownIPCProcessAddresses.push_back(rinad::KnownIPCProcessAddress(*it));
		}
	}

	if (dif_template->addressPrefixes.size() == 0) {
		for (std::list<rinad::AddressPrefixConfiguration>::iterator it =
				default_template->addressPrefixes.begin();
				it != default_template->addressPrefixes.end(); ++it) {
			dif_template->addressPrefixes.push_back(rinad::AddressPrefixConfiguration(*it));
		}
	}

	if (dif_template->pdufTableGeneratorConfiguration.pduft_generator_policy_.name_ == "" ||
			dif_template->pdufTableGeneratorConfiguration.link_state_routing_configuration_.routing_algorithm_ == "") {
		dif_template->pdufTableGeneratorConfiguration = default_template->pdufTableGeneratorConfiguration;
	}

	if (dif_template->policySets.size() == 0 && default_template->policySets.size() != 0) {
		for (std::map<std::string, std::string>::iterator it = default_template->policySets.begin();
				it != default_template->policySets.end(); ++it) {
			dif_template->policySets[it->first] = it->second;
		}
	}

	if (dif_template->policySetParameters.size() == 0 && default_template->policySetParameters.size() != 0) {
		for (std::map<std::string, std::string>::iterator it = default_template->policySetParameters.begin();
				it != default_template->policySetParameters.end(); ++it) {
			dif_template->policySetParameters[it->first] = it->second;
		}
	}

	if (dif_template->configParameters.size() == 0 && default_template->configParameters.size() != 0) {
		for (std::map<std::string, std::string>::iterator it =
				default_template->configParameters.begin();
				it != default_template->configParameters.end(); ++it) {
			dif_template->configParameters[it->first] = it->second;
		}
	}
}

void DIFTemplateManager::add_dif_template(const std::string& name,
					  rinad::DIFTemplate * dif_template)
{
	if (!dif_template) {
		LOG_ERR("Cannot add a bogus dif_template");
		return;
	}

	rina::WriteScopedLock g(templates_lock);

	//Augment dif_template with default_template
	augment_dif_template(dif_template);

	//If the template already exists destroy the old version
	internal_remove_dif_template(name);
	dif_templates[name] = dif_template;

	if (name == DEFAULT_TEMPLATE_NAME) {
		default_template = dif_template;
	}

	LOG_INFO("Added or modified DIF template called: %s", name.c_str());
}

void DIFTemplateManager::remove_dif_template(const std::string& name)
{
	rina::WriteScopedLock g(templates_lock);

	internal_remove_dif_template(name);

	if (name == DEFAULT_TEMPLATE_NAME) {
		LOG_WARN("Default DIF template removed");
		default_template = 0;
	}
}

void DIFTemplateManager::internal_remove_dif_template(const std::string& name)
{
	std::map<std::string, rinad::DIFTemplate*>::iterator it = dif_templates.find(name);
	if (it != dif_templates.end()) {
		dif_templates.erase(name);
		delete it->second;
		LOG_INFO("Removed DIF template called: %s", name.c_str());
	}
}

std::list<rinad::DIFTemplate *> DIFTemplateManager::get_all_dif_templates()
{
	std::map<std::string, rinad::DIFTemplate*>::iterator it;
	std::list<rinad::DIFTemplate *> result;
	rina::ReadScopedLock g(templates_lock);

	for (it = dif_templates.begin(); it != dif_templates.end(); ++it) {
		result.push_back(it->second);
	}

	return result;
}

} //namespace rinad
