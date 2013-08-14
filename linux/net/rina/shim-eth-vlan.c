/*
 *  Shim IPC Process over Ethernet (using VLANs)
 *
 *    Francesco Salvestrini <f.salvestrini@nextworks.it>
 *    Sander Vrijders       <sander.vrijders@intec.ugent.be>
 *    Miquel Tarzan         <miquel.tarzan@i2cat.net>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/if_ether.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/netdevice.h>
#include <linux/if_packet.h>

#define SHIM_NAME     "shim-eth-vlan"
#define MAJOR_VERSION 0
#define MINOR_VERSION 3
#define RINA_PREFIX   SHIM_NAME

#include "logs.h"
#include "common.h"
#include "kipcm.h"
#include "debug.h"
#include "utils.h"
#include "ipcp-utils.h"
#include "ipcp-factories.h"

/* FIXME: To be removed ABSOLUTELY */
extern struct kipcm * default_kipcm;

/* Holds the configuration of one shim instance */
struct eth_vlan_info {
        uint16_t      vlan_id;
        char *        interface_name;
	struct name * name;
        struct name * dif_name;
};

enum port_id_state {
        PORT_STATE_NULL = 1,
        PORT_STATE_RECIPIENT_ALLOCATE_PENDING,
        PORT_STATE_INITIATOR_ALLOCATE_PENDING,
        PORT_STATE_ALLOCATED
};

/* Hold the information related to one flow*/
struct shim_eth_flow {
	struct list_head   list;
        struct name *             source;
        struct name *             dest;
        port_id_t          port_id;
        enum port_id_state port_id_state;

        /* FIXME: Will be a kfifo holding the SDUs or a sk_buff_head */
        /* QUEUE(sdu_queue, sdu *); */
};

/*
 * Contains all the information associated to an instance of a
 * shim Ethernet IPC Process
 */
struct ipcp_instance_data {
	struct list_head       list;

        /* IPC process id and DIF name */
        ipc_process_id_t       id;

        /* The configuration of the shim IPC Process */
        struct eth_vlan_info * info;

	/* The IPC Process using the shim-eth-vlan */
	struct name * app_name;

        /* Stores the state of flows indexed by port_id */
	struct list_head flows;
};

static struct shim_eth_flow * find_flow(struct ipcp_instance_data * data,
                                        port_id_t                   id)
{
        struct shim_eth_flow * flow;

        list_for_each_entry(flow, &data->flows, list) {
                if (flow->port_id == id) {
                        return flow;
                }
        }

        return NULL;
}

static int eth_vlan_flow_allocate_request(struct ipcp_instance_data * data,
                                          const struct name *         source,
                                          const struct name *         dest,
                                          const struct flow_spec *    fspec,
                                          port_id_t                   id)
{
	struct shim_eth_flow * flow;

        ASSERT(data);
        ASSERT(source);
        ASSERT(dest);

        if (find_flow(data, id)) {
                LOG_ERR("A flow already exists on port %d", id);
                return -1;
        }

        flow = rkzalloc(sizeof(*flow), GFP_KERNEL);
        if (!flow)
                return -1;

	flow->port_id = id;
	flow->port_id_state = PORT_STATE_NULL;
	
	/* Keep them as name * ? Or as MAC or 'IP'? */
        flow->dest = name_dup(dest);
        if (!flow->dest) {
                rkfree(flow);
                return -1;
        }
        flow->source = name_dup(source);
        if (!flow->source) {
                name_destroy(flow->dest);
                rkfree(flow);
                return -1;
        }

	/* FIXME: Convert the names to IP addresses */

        /* 
	 * FIXME: If the destination is in the ARP cache,
	 * transition to ALLOCATED
	 */
	flow->port_id_state = PORT_STATE_ALLOCATED;

	/* FIXME: If it is not, send an ARP request */
	flow->port_id_state = PORT_STATE_INITIATOR_ALLOCATE_PENDING;

        INIT_LIST_HEAD(&flow->list);
        list_add(&flow->list, &data->flows);

        if (kipcm_flow_add(default_kipcm, data->id, id)) {
                list_del(&flow->list);
		name_destroy(flow->source);
                name_destroy(flow->dest);
                rkfree(flow);
                return -1;
        }

        return 0;
}

static int eth_vlan_flow_allocate_response(struct ipcp_instance_data * data,
                                           port_id_t                   id,
                                           response_reason_t *         resp)
{
	struct shim_eth_flow * flow;

        ASSERT(data);
        ASSERT(resp);

	flow = find_flow(data, id);
        if (!flow) {
                LOG_ERR("Flow does not exist, cannot remove");
                return -1;
        }

	if (flow->port_id_state != PORT_STATE_RECIPIENT_ALLOCATE_PENDING) {
		LOG_ERR("Flow is not in the right state to call this");
                return -1;	
	}

        /* 
	 * On positive response, flow should transition to allocated state 
	 */
        if (resp == 0) {
		/* FIXME: Deliver frames to application */
		flow->port_id_state = PORT_STATE_ALLOCATED;
        } else {
		/* FIXME: Drop all frames in queue */
		flow->port_id_state = PORT_STATE_NULL;
	}


        return 0;
}

static int eth_vlan_flow_deallocate(struct ipcp_instance_data * data,
                                    port_id_t                   id)
{
        struct shim_eth_flow * flow;

        ASSERT(data);
        flow = find_flow(data, id);
        if (!flow) {
                LOG_ERR("Flow does not exist, cannot remove");
                return -1;
        }

        list_del(&flow->list);
        name_destroy(flow->dest);
        name_destroy(flow->source);
        rkfree(flow);

        if (kipcm_flow_remove(default_kipcm, id))
                return -1;

        return 0;
}

static int eth_vlan_application_register(struct ipcp_instance_data * data,
                                         const struct name *         name)
{
        ASSERT(data);
        ASSERT(name);

	if (data->app_name) {
		char * tmp = name_tostring(name);
		LOG_ERR("Application %s is already registered", tmp);
		rkfree(tmp);
		return -1;
	}
	
	data->app_name = name_dup(name);
       	if (!data->app_name) {
                char * tmp = name_tostring(name);
                LOG_ERR("Application %s registration has failed", tmp);
                rkfree(tmp);
                return -1;
        }

        /* FIXME: Add in ARP cache */

        return 0;
}

static int eth_vlan_application_unregister(struct ipcp_instance_data * data,
                                           const struct name *         name)
{
        ASSERT(data);
        ASSERT(name);
	if (!data->app_name) {
		LOG_ERR("Shim-eth-vlan has no application registered");
		return -1;
	}

	/* FIXME: Remove from ARP cache */

	name_destroy(data->app_name);
	data->app_name = NULL;
	return 0;
}

static int eth_vlan_sdu_write(struct ipcp_instance_data * data,
                              port_id_t                   id,
                              struct sdu *                sdu)
{
	ASSERT(data);
        ASSERT(sdu);

        LOG_MISSING;

        return -1;
}

/* Filter the devices here. Accept packets from VLANs that are configured */
static int eth_vlan_rcv(struct sk_buff *     skb,
                        struct net_device *  dev,
                        struct packet_type * pt,
                        struct net_device *  orig_dev)
{
        if (skb->pkt_type == PACKET_OTHERHOST ||
            skb->pkt_type == PACKET_LOOPBACK) {
                kfree_skb(skb);
                return -1;
        }

        skb = skb_share_check(skb, GFP_ATOMIC);
        if (!skb) {
                /* FIXME: A message here might be helpful ... */
                return -1;
        }

        /* Get the SDU out of the sk_buff */

        kfree_skb(skb);
        return 0;
};

static struct ipcp_instance_ops eth_vlan_instance_ops = {
        .flow_allocate_request  = eth_vlan_flow_allocate_request,
        .flow_allocate_response = eth_vlan_flow_allocate_response,
        .flow_deallocate        = eth_vlan_flow_deallocate,
        .application_register   = eth_vlan_application_register,
        .application_unregister = eth_vlan_application_unregister,
        .sdu_write              = eth_vlan_sdu_write,
};

static struct ipcp_factory_data {
        struct list_head instances;
} eth_vlan_data;

static int eth_vlan_init(struct ipcp_factory_data * data)
{
        ASSERT(data);

        bzero(&eth_vlan_data, sizeof(eth_vlan_data));
        INIT_LIST_HEAD(&(data->instances));

	LOG_INFO("%s v%d.%d intialized", 
		 SHIM_NAME, MAJOR_VERSION, MINOR_VERSION);

        return 0;
}

static int eth_vlan_fini(struct ipcp_factory_data * data)
{

        ASSERT(data);

        ASSERT(list_empty(&(data->instances)));

        return 0;
}

static struct ipcp_instance_data *
find_instance(struct ipcp_factory_data * data,
              ipc_process_id_t           id)
{

	struct ipcp_instance_data * pos;

	list_for_each_entry(pos, &(data->instances), list) {
		if (pos->id == id) {
			return pos;
		}
	}

	return NULL;
       
}

static struct ipcp_instance * eth_vlan_create(struct ipcp_factory_data * data,
                                              ipc_process_id_t           id)
{
        struct ipcp_instance * inst;

        ASSERT(data);
	
	/* Check if there already is an instance with that id */
	if (find_instance(data,id)) {
		LOG_ERR("There's a shim instance with id %d already", id);
		return NULL;
	} 

        /* Create an instance */
        inst = rkzalloc(sizeof(*inst), GFP_KERNEL);
        if (!inst)
                return NULL;

        /* fill it properly */
        inst->ops  = &eth_vlan_instance_ops;
        inst->data = rkzalloc(sizeof(struct ipcp_instance_data), GFP_KERNEL);
        if (!inst->data) {
                rkfree(inst);
                return NULL;
        }

	inst->data->id = id;
        
        inst->data->info = rkzalloc(sizeof(*inst->data->info), GFP_KERNEL);
        if (!inst->data->info) {
                LOG_DBG("Failed creation of inst->data->info");
                rkfree(inst->data);
                rkfree(inst);
                return NULL;
        }

	inst->data->info->interface_name = 
		rkzalloc(sizeof(*inst->data->info->interface_name), GFP_KERNEL);
	if (!inst->data->info->interface_name) {
                LOG_DBG("Failed creation of interface_name");
                rkfree(inst->data->info);
		rkfree(inst->data);
                rkfree(inst);
                return NULL;
        }

        inst->data->info->dif_name = name_create();
        if (!inst->data->info->dif_name) {
                LOG_DBG("Failed creation of dif_name");
		rkfree(inst->data->info->interface_name);
                rkfree(inst->data->info);
                rkfree(inst->data);
                rkfree(inst);
                return NULL;
        }

        inst->data->info->name = name_create();
        if (!inst->data->info->name) {
                LOG_DBG("Failed creation of ipc name");
                name_destroy(inst->data->info->dif_name);
		rkfree(inst->data->info->interface_name);
                rkfree(inst->data->info);
                rkfree(inst->data);
                rkfree(inst);
                return NULL;
        }

        /*
         * Bind the shim-instance to the shims set, to keep all our data
         * structures linked (somewhat) together
         */
        list_add(&(data->instances), &(inst->data->list));

        return inst;
}

static string_t * create_vlan_interface_name(string_t * interface_name, 
					     uint16_t vlan_id)
{
	char string_vlan_id[4];
	string_t * complete_interface;
	
	complete_interface =
		rkmalloc(sizeof(*complete_interface),
			 GFP_KERNEL);
	if (!complete_interface)
		return NULL;

	strcpy(complete_interface, interface_name);
	sprintf(string_vlan_id,"%d",vlan_id);
	strcat(complete_interface, ".");
	strcat(complete_interface, string_vlan_id);

	return complete_interface;
}


struct ipcp_instance * eth_vlan_configure(struct ipcp_factory_data *  data,
                                           struct ipcp_instance *     inst,
                                           const struct ipcp_config * cfg)
{
        struct ipcp_instance_data * instance;
        struct eth_vlan_info *      info;
        struct ipcp_config *        tmp;
	struct ipcp_config_entry *  entry;
	struct ipcp_config_value *  value;
	bool_t                      reconfigure;
        uint16_t                    old_vlan_id;
        string_t *                  old_interface_name;
	struct packet_type          eth_vlan_packet_type;
	string_t *                  complete_interface;
	struct net_device *dev;

        ASSERT(data);
        ASSERT(inst);
        ASSERT(cfg);

       
        instance = find_instance(data, inst->data->id);
        if (!instance) {
                LOG_ERR("Configure called on empty eth vlan shim instance");
                return inst;
        }

        /* If reconfigure = 1, break down all communication and setup again */
        reconfigure = 0;
        old_vlan_id = 0;
        old_interface_name = NULL;
	info = instance->info;
	
        /* Get configuration struct pertaining to this shim instance */
	old_vlan_id = info->vlan_id;
	old_interface_name = info->interface_name;


        /* Retrieve configuration of IPC process from params */
	list_for_each_entry(tmp, &(cfg->list), list) {
		entry = tmp->entry;
		value = entry->value;
		if (!strcmp(entry->name, "dif-name") &&
                    value->type == IPCP_CONFIG_STRING) {
                        if (name_cpy((info->dif_name),
                                          (struct name *) value->data)) {
				LOG_ERR("Failed to copy DIF name");
                                return inst;
                        }
		} else if (!strcmp(entry->name, "name") &&
                    value->type == IPCP_CONFIG_STRING) {
                        if (name_cpy((info->name),
                                          (struct name *) value->data)) {
				LOG_ERR("Failed to copy IPC Process name");
                                return inst;
                        }
		} else if (!strcmp(entry->name, "vlan-id") &&
			value->type == IPCP_CONFIG_UINT) {
                        info->vlan_id = 
				* (uint16_t *) value->data;
                        if (!reconfigure && info->vlan_id != old_vlan_id) {
                                reconfigure = 1;
                        }
                } else if (!strcmp(entry->name,"interface-name") &&
                           value->type == IPCP_CONFIG_STRING) {
			info->interface_name =
				rkmalloc(sizeof(*info->interface_name), 
					GFP_KERNEL);
			if (!strcpy(info->interface_name, 
					(string_t *) value->data)) {
				LOG_ERR("Failed to copy interface name");
			}
                        if (!reconfigure && !strcmp(info->interface_name, 
                                                    old_interface_name)) {
                                reconfigure = 1;
                        }
                } else {
                        LOG_WARN("Unknown config param for eth shim");
                }
        }

	eth_vlan_packet_type.type = cpu_to_be16(ETH_P_RINA);
	eth_vlan_packet_type.func = eth_vlan_rcv;

        if (reconfigure) {
            
                /* Remove previous handler if there's one */
                if (old_interface_name && old_vlan_id != 0) {
                      
                        /* First construct the complete interface name */
			complete_interface= 
				create_vlan_interface_name(old_interface_name, 
							   old_vlan_id);
			if (!complete_interface) {
				return inst;
			}
                        /* Remove the handler */
                        read_lock(&dev_base_lock);
                        dev = __dev_get_by_name(&init_net, complete_interface);
                        if (!dev) {
				LOG_ERR("Invalid device to configure: %s",
					complete_interface);
                                return inst;
                        }
                        eth_vlan_packet_type.dev = dev;
                        dev_remove_pack(&eth_vlan_packet_type);
                        read_unlock(&dev_base_lock);
                        rkfree(complete_interface);
                }
        }

	complete_interface= 
		create_vlan_interface_name(info->interface_name, 
					   info->vlan_id);
	if (!complete_interface) {
		return inst;
	}
        /* Add the handler */
	read_lock(&dev_base_lock);
	dev = __dev_get_by_name(&init_net, complete_interface);
	if (!dev) {
		LOG_ERR("Invalid device to configure: %s",
			complete_interface);
		return inst;
	}
	eth_vlan_packet_type.dev = dev;
	dev_add_pack(&eth_vlan_packet_type);
	read_unlock(&dev_base_lock);
	rkfree(complete_interface);

	LOG_DBG("Configured shim eth vlan IPC Process");

        return inst;
}

static int eth_vlan_destroy(struct ipcp_factory_data * data,
                            struct ipcp_instance *     instance)
{
	struct list_head * pos, * q;

        ASSERT(data);
        ASSERT(instance);

        /* Retrieve the instance */
	list_for_each_safe(pos, q, &(data->instances)) {
                struct ipcp_instance_data * inst;

		 inst = list_entry(pos, struct ipcp_instance_data, list);

		 if (inst->id == instance->data->id) {
			 /* Unbind from the instances set */
			 list_del(pos);

			 /* Destroy it */
			 name_destroy(inst->info->dif_name);
			 name_destroy(inst->info->name);
			 rkfree(inst->info->interface_name);
			 rkfree(inst->info);
			 rkfree(inst);
		 }
	}

        return 0;
}

static struct ipcp_factory_ops eth_vlan_ops = {
        .init      = eth_vlan_init,
        .fini      = eth_vlan_fini,
        .create    = eth_vlan_create,
        .destroy   = eth_vlan_destroy,
        .configure = eth_vlan_configure,
};

struct ipcp_factory * shim;

static int __init mod_init(void)
{
        shim =  kipcm_ipcp_factory_register(default_kipcm,
                                            SHIM_NAME,
                                            &eth_vlan_data,
                                            &eth_vlan_ops);
        if (!shim) {
                LOG_CRIT("Cannot register %s factory", SHIM_NAME);
                return -1;
        }

        return 0;
}

static void __exit mod_exit(void)
{
        ASSERT(shim);

        if (kipcm_ipcp_factory_unregister(default_kipcm, shim)) {
                LOG_CRIT("Cannot unregister %s factory", SHIM_NAME);
                return;
        }
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_DESCRIPTION("RINA Shim IPC over Ethernet");

MODULE_LICENSE("GPL");

MODULE_AUTHOR("Francesco Salvestrini <f.salvestrini@nextworks.it>");
MODULE_AUTHOR("Miquel Tarzan <miquel.tarzan@i2cat.net>");
MODULE_AUTHOR("Sander Vrijders <sander.vrijders@intec.ugent.be>");
