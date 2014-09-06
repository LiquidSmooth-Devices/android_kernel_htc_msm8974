/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
 *
 *
 *  This driver is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This driver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <sound/core.h>
#include "hda_codec.h"
#include <sound/asoundef.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <sound/jack.h>
#include "hda_local.h"
#include "hda_beep.h"
#include "hda_jack.h"
#include <sound/hda_hwdep.h>

#define CREATE_TRACE_POINTS
#include "hda_trace.h"


struct hda_vendor_id {
	unsigned int id;
	const char *name;
};

static struct hda_vendor_id hda_vendor_ids[] = {
	{ 0x1002, "ATI" },
	{ 0x1013, "Cirrus Logic" },
	{ 0x1057, "Motorola" },
	{ 0x1095, "Silicon Image" },
	{ 0x10de, "Nvidia" },
	{ 0x10ec, "Realtek" },
	{ 0x1102, "Creative" },
	{ 0x1106, "VIA" },
	{ 0x111d, "IDT" },
	{ 0x11c1, "LSI" },
	{ 0x11d4, "Analog Devices" },
	{ 0x13f6, "C-Media" },
	{ 0x14f1, "Conexant" },
	{ 0x17e8, "Chrontel" },
	{ 0x1854, "LG" },
	{ 0x1aec, "Wolfson Microelectronics" },
	{ 0x434d, "C-Media" },
	{ 0x8086, "Intel" },
	{ 0x8384, "SigmaTel" },
	{} 
};

static DEFINE_MUTEX(preset_mutex);
static LIST_HEAD(hda_preset_tables);

int snd_hda_add_codec_preset(struct hda_codec_preset_list *preset)
{
	mutex_lock(&preset_mutex);
	list_add_tail(&preset->list, &hda_preset_tables);
	mutex_unlock(&preset_mutex);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_add_codec_preset);

int snd_hda_delete_codec_preset(struct hda_codec_preset_list *preset)
{
	mutex_lock(&preset_mutex);
	list_del(&preset->list);
	mutex_unlock(&preset_mutex);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_delete_codec_preset);

#ifdef CONFIG_SND_HDA_POWER_SAVE
static void hda_power_work(struct work_struct *work);
static void hda_keep_power_on(struct hda_codec *codec);
#define hda_codec_is_power_on(codec)	((codec)->power_on)
#else
static inline void hda_keep_power_on(struct hda_codec *codec) {}
#define hda_codec_is_power_on(codec)	1
#endif

const char *snd_hda_get_jack_location(u32 cfg)
{
	static char *bases[7] = {
		"N/A", "Rear", "Front", "Left", "Right", "Top", "Bottom",
	};
	static unsigned char specials_idx[] = {
		0x07, 0x08,
		0x17, 0x18, 0x19,
		0x37, 0x38
	};
	static char *specials[] = {
		"Rear Panel", "Drive Bar",
		"Riser", "HDMI", "ATAPI",
		"Mobile-In", "Mobile-Out"
	};
	int i;
	cfg = (cfg & AC_DEFCFG_LOCATION) >> AC_DEFCFG_LOCATION_SHIFT;
	if ((cfg & 0x0f) < 7)
		return bases[cfg & 0x0f];
	for (i = 0; i < ARRAY_SIZE(specials_idx); i++) {
		if (cfg == specials_idx[i])
			return specials[i];
	}
	return "UNKNOWN";
}
EXPORT_SYMBOL_HDA(snd_hda_get_jack_location);

const char *snd_hda_get_jack_connectivity(u32 cfg)
{
	static char *jack_locations[4] = { "Ext", "Int", "Sep", "Oth" };

	return jack_locations[(cfg >> (AC_DEFCFG_LOCATION_SHIFT + 4)) & 3];
}
EXPORT_SYMBOL_HDA(snd_hda_get_jack_connectivity);

const char *snd_hda_get_jack_type(u32 cfg)
{
	static char *jack_types[16] = {
		"Line Out", "Speaker", "HP Out", "CD",
		"SPDIF Out", "Digital Out", "Modem Line", "Modem Hand",
		"Line In", "Aux", "Mic", "Telephony",
		"SPDIF In", "Digitial In", "Reserved", "Other"
	};

	return jack_types[(cfg & AC_DEFCFG_DEVICE)
				>> AC_DEFCFG_DEVICE_SHIFT];
}
EXPORT_SYMBOL_HDA(snd_hda_get_jack_type);

static inline unsigned int
make_codec_cmd(struct hda_codec *codec, hda_nid_t nid, int direct,
	       unsigned int verb, unsigned int parm)
{
	u32 val;

	if ((codec->addr & ~0xf) || (direct & ~1) || (nid & ~0x7f) ||
	    (verb & ~0xfff) || (parm & ~0xffff)) {
		printk(KERN_ERR "hda-codec: out of range cmd %x:%x:%x:%x:%x\n",
		       codec->addr, direct, nid, verb, parm);
		return ~0;
	}

	val = (u32)codec->addr << 28;
	val |= (u32)direct << 27;
	val |= (u32)nid << 20;
	val |= verb << 8;
	val |= parm;
	return val;
}

static int codec_exec_verb(struct hda_codec *codec, unsigned int cmd,
			   unsigned int *res)
{
	struct hda_bus *bus = codec->bus;
	int err;

	if (cmd == ~0)
		return -1;

	if (res)
		*res = -1;
 again:
	snd_hda_power_up(codec);
	mutex_lock(&bus->cmd_mutex);
	trace_hda_send_cmd(codec, cmd);
	err = bus->ops.command(bus, cmd);
	if (!err && res) {
		*res = bus->ops.get_response(bus, codec->addr);
		trace_hda_get_response(codec, *res);
	}
	mutex_unlock(&bus->cmd_mutex);
	snd_hda_power_down(codec);
	if (res && *res == -1 && bus->rirb_error) {
		if (bus->response_reset) {
			snd_printd("hda_codec: resetting BUS due to "
				   "fatal communication error\n");
			trace_hda_bus_reset(bus);
			bus->ops.bus_reset(bus);
		}
		goto again;
	}
	
	if (!err)
		bus->response_reset = 0;
	return err;
}

unsigned int snd_hda_codec_read(struct hda_codec *codec, hda_nid_t nid,
				int direct,
				unsigned int verb, unsigned int parm)
{
	unsigned cmd = make_codec_cmd(codec, nid, direct, verb, parm);
	unsigned int res;
	if (codec_exec_verb(codec, cmd, &res))
		return -1;
	return res;
}
EXPORT_SYMBOL_HDA(snd_hda_codec_read);

int snd_hda_codec_write(struct hda_codec *codec, hda_nid_t nid, int direct,
			 unsigned int verb, unsigned int parm)
{
	unsigned int cmd = make_codec_cmd(codec, nid, direct, verb, parm);
	unsigned int res;
	return codec_exec_verb(codec, cmd,
			       codec->bus->sync_write ? &res : NULL);
}
EXPORT_SYMBOL_HDA(snd_hda_codec_write);

void snd_hda_sequence_write(struct hda_codec *codec, const struct hda_verb *seq)
{
	for (; seq->nid; seq++)
		snd_hda_codec_write(codec, seq->nid, 0, seq->verb, seq->param);
}
EXPORT_SYMBOL_HDA(snd_hda_sequence_write);

int snd_hda_get_sub_nodes(struct hda_codec *codec, hda_nid_t nid,
			  hda_nid_t *start_id)
{
	unsigned int parm;

	parm = snd_hda_param_read(codec, nid, AC_PAR_NODE_COUNT);
	if (parm == -1)
		return 0;
	*start_id = (parm >> 16) & 0x7fff;
	return (int)(parm & 0x7fff);
}
EXPORT_SYMBOL_HDA(snd_hda_get_sub_nodes);

static hda_nid_t *lookup_conn_list(struct snd_array *array, hda_nid_t nid)
{
	int i, len;
	for (i = 0; i < array->used; ) {
		hda_nid_t *p = snd_array_elem(array, i);
		if (nid == *p)
			return p;
		len = p[1];
		i += len + 2;
	}
	return NULL;
}

int snd_hda_get_conn_list(struct hda_codec *codec, hda_nid_t nid,
			  const hda_nid_t **listp)
{
	struct snd_array *array = &codec->conn_lists;
	int len, err;
	hda_nid_t list[HDA_MAX_CONNECTIONS];
	hda_nid_t *p;
	bool added = false;

 again:
	
	p = lookup_conn_list(array, nid);
	if (p) {
		if (listp)
			*listp = p + 2;
		return p[1];
	}
	if (snd_BUG_ON(added))
		return -EINVAL;

	
	len = snd_hda_get_raw_connections(codec, nid, list, HDA_MAX_CONNECTIONS);
	if (len < 0)
		return len;
	err = snd_hda_override_conn_list(codec, nid, len, list);
	if (err < 0)
		return err;
	added = true;
	goto again;
}
EXPORT_SYMBOL_HDA(snd_hda_get_conn_list);

int snd_hda_get_connections(struct hda_codec *codec, hda_nid_t nid,
			     hda_nid_t *conn_list, int max_conns)
{
	const hda_nid_t *list;
	int len = snd_hda_get_conn_list(codec, nid, &list);

	if (len <= 0)
		return len;
	if (len > max_conns) {
		snd_printk(KERN_ERR "hda_codec: "
			   "Too many connections %d for NID 0x%x\n",
			   len, nid);
		return -EINVAL;
	}
	memcpy(conn_list, list, len * sizeof(hda_nid_t));
	return len;
}
EXPORT_SYMBOL_HDA(snd_hda_get_connections);

int snd_hda_get_raw_connections(struct hda_codec *codec, hda_nid_t nid,
				hda_nid_t *conn_list, int max_conns)
{
	unsigned int parm;
	int i, conn_len, conns;
	unsigned int shift, num_elems, mask;
	unsigned int wcaps;
	hda_nid_t prev_nid;

	if (snd_BUG_ON(!conn_list || max_conns <= 0))
		return -EINVAL;

	wcaps = get_wcaps(codec, nid);
	if (!(wcaps & AC_WCAP_CONN_LIST) &&
	    get_wcaps_type(wcaps) != AC_WID_VOL_KNB)
		return 0;

	parm = snd_hda_param_read(codec, nid, AC_PAR_CONNLIST_LEN);
	if (parm & AC_CLIST_LONG) {
		
		shift = 16;
		num_elems = 2;
	} else {
		
		shift = 8;
		num_elems = 4;
	}
	conn_len = parm & AC_CLIST_LENGTH;
	mask = (1 << (shift-1)) - 1;

	if (!conn_len)
		return 0; 

	if (conn_len == 1) {
		
		parm = snd_hda_codec_read(codec, nid, 0,
					  AC_VERB_GET_CONNECT_LIST, 0);
		if (parm == -1 && codec->bus->rirb_error)
			return -EIO;
		conn_list[0] = parm & mask;
		return 1;
	}

	
	conns = 0;
	prev_nid = 0;
	for (i = 0; i < conn_len; i++) {
		int range_val;
		hda_nid_t val, n;

		if (i % num_elems == 0) {
			parm = snd_hda_codec_read(codec, nid, 0,
						  AC_VERB_GET_CONNECT_LIST, i);
			if (parm == -1 && codec->bus->rirb_error)
				return -EIO;
		}
		range_val = !!(parm & (1 << (shift-1))); 
		val = parm & mask;
		if (val == 0) {
			snd_printk(KERN_WARNING "hda_codec: "
				   "invalid CONNECT_LIST verb %x[%i]:%x\n",
				    nid, i, parm);
			return 0;
		}
		parm >>= shift;
		if (range_val) {
			
			if (!prev_nid || prev_nid >= val) {
				snd_printk(KERN_WARNING "hda_codec: "
					   "invalid dep_range_val %x:%x\n",
					   prev_nid, val);
				continue;
			}
			for (n = prev_nid + 1; n <= val; n++) {
				if (conns >= max_conns) {
					snd_printk(KERN_ERR "hda_codec: "
						   "Too many connections %d for NID 0x%x\n",
						   conns, nid);
					return -EINVAL;
				}
				conn_list[conns++] = n;
			}
		} else {
			if (conns >= max_conns) {
				snd_printk(KERN_ERR "hda_codec: "
					   "Too many connections %d for NID 0x%x\n",
					   conns, nid);
				return -EINVAL;
			}
			conn_list[conns++] = val;
		}
		prev_nid = val;
	}
	return conns;
}

static bool add_conn_list(struct snd_array *array, hda_nid_t nid)
{
	hda_nid_t *p = snd_array_new(array);
	if (!p)
		return false;
	*p = nid;
	return true;
}

int snd_hda_override_conn_list(struct hda_codec *codec, hda_nid_t nid, int len,
			       const hda_nid_t *list)
{
	struct snd_array *array = &codec->conn_lists;
	hda_nid_t *p;
	int i, old_used;

	p = lookup_conn_list(array, nid);
	if (p)
		*p = -1; 

	old_used = array->used;
	if (!add_conn_list(array, nid) || !add_conn_list(array, len))
		goto error_add;
	for (i = 0; i < len; i++)
		if (!add_conn_list(array, list[i]))
			goto error_add;
	return 0;

 error_add:
	array->used = old_used;
	return -ENOMEM;
}
EXPORT_SYMBOL_HDA(snd_hda_override_conn_list);

int snd_hda_get_conn_index(struct hda_codec *codec, hda_nid_t mux,
			   hda_nid_t nid, int recursive)
{
	hda_nid_t conn[HDA_MAX_NUM_INPUTS];
	int i, nums;

	nums = snd_hda_get_connections(codec, mux, conn, ARRAY_SIZE(conn));
	for (i = 0; i < nums; i++)
		if (conn[i] == nid)
			return i;
	if (!recursive)
		return -1;
	if (recursive > 5) {
		snd_printd("hda_codec: too deep connection for 0x%x\n", nid);
		return -1;
	}
	recursive++;
	for (i = 0; i < nums; i++) {
		unsigned int type = get_wcaps_type(get_wcaps(codec, conn[i]));
		if (type == AC_WID_PIN || type == AC_WID_AUD_OUT)
			continue;
		if (snd_hda_get_conn_index(codec, conn[i], nid, recursive) >= 0)
			return i;
	}
	return -1;
}
EXPORT_SYMBOL_HDA(snd_hda_get_conn_index);

int snd_hda_queue_unsol_event(struct hda_bus *bus, u32 res, u32 res_ex)
{
	struct hda_bus_unsolicited *unsol;
	unsigned int wp;

	trace_hda_unsol_event(bus, res, res_ex);
	unsol = bus->unsol;
	if (!unsol)
		return 0;

	wp = (unsol->wp + 1) % HDA_UNSOL_QUEUE_SIZE;
	unsol->wp = wp;

	wp <<= 1;
	unsol->queue[wp] = res;
	unsol->queue[wp + 1] = res_ex;

	queue_work(bus->workq, &unsol->work);

	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_queue_unsol_event);

static void process_unsol_events(struct work_struct *work)
{
	struct hda_bus_unsolicited *unsol =
		container_of(work, struct hda_bus_unsolicited, work);
	struct hda_bus *bus = unsol->bus;
	struct hda_codec *codec;
	unsigned int rp, caddr, res;

	while (unsol->rp != unsol->wp) {
		rp = (unsol->rp + 1) % HDA_UNSOL_QUEUE_SIZE;
		unsol->rp = rp;
		rp <<= 1;
		res = unsol->queue[rp];
		caddr = unsol->queue[rp + 1];
		if (!(caddr & (1 << 4))) 
			continue;
		codec = bus->caddr_tbl[caddr & 0x0f];
		if (codec && codec->patch_ops.unsol_event)
			codec->patch_ops.unsol_event(codec, res);
	}
}

static int init_unsol_queue(struct hda_bus *bus)
{
	struct hda_bus_unsolicited *unsol;

	if (bus->unsol) 
		return 0;

	unsol = kzalloc(sizeof(*unsol), GFP_KERNEL);
	if (!unsol) {
		snd_printk(KERN_ERR "hda_codec: "
			   "can't allocate unsolicited queue\n");
		return -ENOMEM;
	}
	INIT_WORK(&unsol->work, process_unsol_events);
	unsol->bus = bus;
	bus->unsol = unsol;
	return 0;
}

static void snd_hda_codec_free(struct hda_codec *codec);

static int snd_hda_bus_free(struct hda_bus *bus)
{
	struct hda_codec *codec, *n;

	if (!bus)
		return 0;
	if (bus->workq)
		flush_workqueue(bus->workq);
	if (bus->unsol)
		kfree(bus->unsol);
	list_for_each_entry_safe(codec, n, &bus->codec_list, list) {
		snd_hda_codec_free(codec);
	}
	if (bus->ops.private_free)
		bus->ops.private_free(bus);
	if (bus->workq)
		destroy_workqueue(bus->workq);
	kfree(bus);
	return 0;
}

static int snd_hda_bus_dev_free(struct snd_device *device)
{
	struct hda_bus *bus = device->device_data;
	bus->shutdown = 1;
	return snd_hda_bus_free(bus);
}

#ifdef CONFIG_SND_HDA_HWDEP
static int snd_hda_bus_dev_register(struct snd_device *device)
{
	struct hda_bus *bus = device->device_data;
	struct hda_codec *codec;
	list_for_each_entry(codec, &bus->codec_list, list) {
		snd_hda_hwdep_add_sysfs(codec);
		snd_hda_hwdep_add_power_sysfs(codec);
	}
	return 0;
}
#else
#define snd_hda_bus_dev_register	NULL
#endif

int  snd_hda_bus_new(struct snd_card *card,
			      const struct hda_bus_template *temp,
			      struct hda_bus **busp)
{
	struct hda_bus *bus;
	int err;
	static struct snd_device_ops dev_ops = {
		.dev_register = snd_hda_bus_dev_register,
		.dev_free = snd_hda_bus_dev_free,
	};

	if (snd_BUG_ON(!temp))
		return -EINVAL;
	if (snd_BUG_ON(!temp->ops.command || !temp->ops.get_response))
		return -EINVAL;

	if (busp)
		*busp = NULL;

	bus = kzalloc(sizeof(*bus), GFP_KERNEL);
	if (bus == NULL) {
		snd_printk(KERN_ERR "can't allocate struct hda_bus\n");
		return -ENOMEM;
	}

	bus->card = card;
	bus->private_data = temp->private_data;
	bus->pci = temp->pci;
	bus->modelname = temp->modelname;
	bus->power_save = temp->power_save;
	bus->ops = temp->ops;

	mutex_init(&bus->cmd_mutex);
	mutex_init(&bus->prepare_mutex);
	INIT_LIST_HEAD(&bus->codec_list);

	snprintf(bus->workq_name, sizeof(bus->workq_name),
		 "hd-audio%d", card->number);
	bus->workq = create_singlethread_workqueue(bus->workq_name);
	if (!bus->workq) {
		snd_printk(KERN_ERR "cannot create workqueue %s\n",
			   bus->workq_name);
		kfree(bus);
		return -ENOMEM;
	}

	err = snd_device_new(card, SNDRV_DEV_BUS, bus, &dev_ops);
	if (err < 0) {
		snd_hda_bus_free(bus);
		return err;
	}
	if (busp)
		*busp = bus;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_bus_new);

#ifdef CONFIG_SND_HDA_GENERIC
#define is_generic_config(codec) \
	(codec->modelname && !strcmp(codec->modelname, "generic"))
#else
#define is_generic_config(codec)	0
#endif

#ifdef MODULE
#define HDA_MODREQ_MAX_COUNT	2	
#else
#define HDA_MODREQ_MAX_COUNT	0	
#endif

static const struct hda_codec_preset *
find_codec_preset(struct hda_codec *codec)
{
	struct hda_codec_preset_list *tbl;
	const struct hda_codec_preset *preset;
	int mod_requested = 0;

	if (is_generic_config(codec))
		return NULL; 

 again:
	mutex_lock(&preset_mutex);
	list_for_each_entry(tbl, &hda_preset_tables, list) {
		if (!try_module_get(tbl->owner)) {
			snd_printk(KERN_ERR "hda_codec: cannot module_get\n");
			continue;
		}
		for (preset = tbl->preset; preset->id; preset++) {
			u32 mask = preset->mask;
			if (preset->afg && preset->afg != codec->afg)
				continue;
			if (preset->mfg && preset->mfg != codec->mfg)
				continue;
			if (!mask)
				mask = ~0;
			if (preset->id == (codec->vendor_id & mask) &&
			    (!preset->rev ||
			     preset->rev == codec->revision_id)) {
				mutex_unlock(&preset_mutex);
				codec->owner = tbl->owner;
				return preset;
			}
		}
		module_put(tbl->owner);
	}
	mutex_unlock(&preset_mutex);

	if (mod_requested < HDA_MODREQ_MAX_COUNT) {
		char name[32];
		if (!mod_requested)
			snprintf(name, sizeof(name), "snd-hda-codec-id:%08x",
				 codec->vendor_id);
		else
			snprintf(name, sizeof(name), "snd-hda-codec-id:%04x*",
				 (codec->vendor_id >> 16) & 0xffff);
		request_module(name);
		mod_requested++;
		goto again;
	}
	return NULL;
}

static int get_codec_name(struct hda_codec *codec)
{
	const struct hda_vendor_id *c;
	const char *vendor = NULL;
	u16 vendor_id = codec->vendor_id >> 16;
	char tmp[16];

	if (codec->vendor_name)
		goto get_chip_name;

	for (c = hda_vendor_ids; c->id; c++) {
		if (c->id == vendor_id) {
			vendor = c->name;
			break;
		}
	}
	if (!vendor) {
		sprintf(tmp, "Generic %04x", vendor_id);
		vendor = tmp;
	}
	codec->vendor_name = kstrdup(vendor, GFP_KERNEL);
	if (!codec->vendor_name)
		return -ENOMEM;

 get_chip_name:
	if (codec->chip_name)
		return 0;

	if (codec->preset && codec->preset->name)
		codec->chip_name = kstrdup(codec->preset->name, GFP_KERNEL);
	else {
		sprintf(tmp, "ID %x", codec->vendor_id & 0xffff);
		codec->chip_name = kstrdup(tmp, GFP_KERNEL);
	}
	if (!codec->chip_name)
		return -ENOMEM;
	return 0;
}

static void  setup_fg_nodes(struct hda_codec *codec)
{
	int i, total_nodes, function_id;
	hda_nid_t nid;

	total_nodes = snd_hda_get_sub_nodes(codec, AC_NODE_ROOT, &nid);
	for (i = 0; i < total_nodes; i++, nid++) {
		function_id = snd_hda_param_read(codec, nid,
						AC_PAR_FUNCTION_TYPE);
		switch (function_id & 0xff) {
		case AC_GRP_AUDIO_FUNCTION:
			codec->afg = nid;
			codec->afg_function_id = function_id & 0xff;
			codec->afg_unsol = (function_id >> 8) & 1;
			break;
		case AC_GRP_MODEM_FUNCTION:
			codec->mfg = nid;
			codec->mfg_function_id = function_id & 0xff;
			codec->mfg_unsol = (function_id >> 8) & 1;
			break;
		default:
			break;
		}
	}
}

static int read_widget_caps(struct hda_codec *codec, hda_nid_t fg_node)
{
	int i;
	hda_nid_t nid;

	codec->num_nodes = snd_hda_get_sub_nodes(codec, fg_node,
						 &codec->start_nid);
	codec->wcaps = kmalloc(codec->num_nodes * 4, GFP_KERNEL);
	if (!codec->wcaps)
		return -ENOMEM;
	nid = codec->start_nid;
	for (i = 0; i < codec->num_nodes; i++, nid++)
		codec->wcaps[i] = snd_hda_param_read(codec, nid,
						     AC_PAR_AUDIO_WIDGET_CAP);
	return 0;
}

static int read_pin_defaults(struct hda_codec *codec)
{
	int i;
	hda_nid_t nid = codec->start_nid;

	for (i = 0; i < codec->num_nodes; i++, nid++) {
		struct hda_pincfg *pin;
		unsigned int wcaps = get_wcaps(codec, nid);
		unsigned int wid_type = get_wcaps_type(wcaps);
		if (wid_type != AC_WID_PIN)
			continue;
		pin = snd_array_new(&codec->init_pins);
		if (!pin)
			return -ENOMEM;
		pin->nid = nid;
		pin->cfg = snd_hda_codec_read(codec, nid, 0,
					      AC_VERB_GET_CONFIG_DEFAULT, 0);
		pin->ctrl = snd_hda_codec_read(codec, nid, 0,
					       AC_VERB_GET_PIN_WIDGET_CONTROL,
					       0);
	}
	return 0;
}

static struct hda_pincfg *look_up_pincfg(struct hda_codec *codec,
					 struct snd_array *array,
					 hda_nid_t nid)
{
	int i;
	for (i = 0; i < array->used; i++) {
		struct hda_pincfg *pin = snd_array_elem(array, i);
		if (pin->nid == nid)
			return pin;
	}
	return NULL;
}

static void set_pincfg(struct hda_codec *codec, hda_nid_t nid,
		       unsigned int cfg)
{
	int i;
	for (i = 0; i < 4; i++) {
		snd_hda_codec_write(codec, nid, 0,
				    AC_VERB_SET_CONFIG_DEFAULT_BYTES_0 + i,
				    cfg & 0xff);
		cfg >>= 8;
	}
}

int snd_hda_add_pincfg(struct hda_codec *codec, struct snd_array *list,
		       hda_nid_t nid, unsigned int cfg)
{
	struct hda_pincfg *pin;
	unsigned int oldcfg;

	if (get_wcaps_type(get_wcaps(codec, nid)) != AC_WID_PIN)
		return -EINVAL;

	oldcfg = snd_hda_codec_get_pincfg(codec, nid);
	pin = look_up_pincfg(codec, list, nid);
	if (!pin) {
		pin = snd_array_new(list);
		if (!pin)
			return -ENOMEM;
		pin->nid = nid;
	}
	pin->cfg = cfg;

	cfg = snd_hda_codec_get_pincfg(codec, nid);
	if (oldcfg != cfg)
		set_pincfg(codec, nid, cfg);
	return 0;
}

int snd_hda_codec_set_pincfg(struct hda_codec *codec,
			     hda_nid_t nid, unsigned int cfg)
{
	return snd_hda_add_pincfg(codec, &codec->driver_pins, nid, cfg);
}
EXPORT_SYMBOL_HDA(snd_hda_codec_set_pincfg);

unsigned int snd_hda_codec_get_pincfg(struct hda_codec *codec, hda_nid_t nid)
{
	struct hda_pincfg *pin;

#ifdef CONFIG_SND_HDA_HWDEP
	pin = look_up_pincfg(codec, &codec->user_pins, nid);
	if (pin)
		return pin->cfg;
#endif
	pin = look_up_pincfg(codec, &codec->driver_pins, nid);
	if (pin)
		return pin->cfg;
	pin = look_up_pincfg(codec, &codec->init_pins, nid);
	if (pin)
		return pin->cfg;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_codec_get_pincfg);

static void restore_pincfgs(struct hda_codec *codec)
{
	int i;
	for (i = 0; i < codec->init_pins.used; i++) {
		struct hda_pincfg *pin = snd_array_elem(&codec->init_pins, i);
		set_pincfg(codec, pin->nid,
			   snd_hda_codec_get_pincfg(codec, pin->nid));
	}
}

void snd_hda_shutup_pins(struct hda_codec *codec)
{
	int i;
	if (codec->bus->shutdown)
		return;
	for (i = 0; i < codec->init_pins.used; i++) {
		struct hda_pincfg *pin = snd_array_elem(&codec->init_pins, i);
		
		snd_hda_codec_read(codec, pin->nid, 0,
				   AC_VERB_SET_PIN_WIDGET_CONTROL, 0);
	}
	codec->pins_shutup = 1;
}
EXPORT_SYMBOL_HDA(snd_hda_shutup_pins);

#ifdef CONFIG_PM
static void restore_shutup_pins(struct hda_codec *codec)
{
	int i;
	if (!codec->pins_shutup)
		return;
	if (codec->bus->shutdown)
		return;
	for (i = 0; i < codec->init_pins.used; i++) {
		struct hda_pincfg *pin = snd_array_elem(&codec->init_pins, i);
		snd_hda_codec_write(codec, pin->nid, 0,
				    AC_VERB_SET_PIN_WIDGET_CONTROL,
				    pin->ctrl);
	}
	codec->pins_shutup = 0;
}
#endif

static void init_hda_cache(struct hda_cache_rec *cache,
			   unsigned int record_size);
static void free_hda_cache(struct hda_cache_rec *cache);

static void restore_init_pincfgs(struct hda_codec *codec)
{
	snd_array_free(&codec->driver_pins);
#ifdef CONFIG_SND_HDA_HWDEP
	snd_array_free(&codec->user_pins);
#endif
	restore_pincfgs(codec);
	snd_array_free(&codec->init_pins);
}

struct hda_cvt_setup {
	hda_nid_t nid;
	u8 stream_tag;
	u8 channel_id;
	u16 format_id;
	unsigned char active;	
	unsigned char dirty;	
};

static struct hda_cvt_setup *
get_hda_cvt_setup(struct hda_codec *codec, hda_nid_t nid)
{
	struct hda_cvt_setup *p;
	int i;

	for (i = 0; i < codec->cvt_setups.used; i++) {
		p = snd_array_elem(&codec->cvt_setups, i);
		if (p->nid == nid)
			return p;
	}
	p = snd_array_new(&codec->cvt_setups);
	if (p)
		p->nid = nid;
	return p;
}

static void snd_hda_codec_free(struct hda_codec *codec)
{
	if (!codec)
		return;
	snd_hda_jack_tbl_clear(codec);
	restore_init_pincfgs(codec);
#ifdef CONFIG_SND_HDA_POWER_SAVE
	cancel_delayed_work(&codec->power_work);
	flush_workqueue(codec->bus->workq);
#endif
	list_del(&codec->list);
	snd_array_free(&codec->mixers);
	snd_array_free(&codec->nids);
	snd_array_free(&codec->cvt_setups);
	snd_array_free(&codec->conn_lists);
	snd_array_free(&codec->spdif_out);
	codec->bus->caddr_tbl[codec->addr] = NULL;
	if (codec->patch_ops.free)
		codec->patch_ops.free(codec);
	module_put(codec->owner);
	free_hda_cache(&codec->amp_cache);
	free_hda_cache(&codec->cmd_cache);
	kfree(codec->vendor_name);
	kfree(codec->chip_name);
	kfree(codec->modelname);
	kfree(codec->wcaps);
	kfree(codec);
}

static void hda_set_power_state(struct hda_codec *codec, hda_nid_t fg,
				unsigned int power_state);

int  snd_hda_codec_new(struct hda_bus *bus,
				unsigned int codec_addr,
				struct hda_codec **codecp)
{
	struct hda_codec *codec;
	char component[31];
	int err;

	if (snd_BUG_ON(!bus))
		return -EINVAL;
	if (snd_BUG_ON(codec_addr > HDA_MAX_CODEC_ADDRESS))
		return -EINVAL;

	if (bus->caddr_tbl[codec_addr]) {
		snd_printk(KERN_ERR "hda_codec: "
			   "address 0x%x is already occupied\n", codec_addr);
		return -EBUSY;
	}

	codec = kzalloc(sizeof(*codec), GFP_KERNEL);
	if (codec == NULL) {
		snd_printk(KERN_ERR "can't allocate struct hda_codec\n");
		return -ENOMEM;
	}

	codec->bus = bus;
	codec->addr = codec_addr;
	mutex_init(&codec->spdif_mutex);
	mutex_init(&codec->control_mutex);
	init_hda_cache(&codec->amp_cache, sizeof(struct hda_amp_info));
	init_hda_cache(&codec->cmd_cache, sizeof(struct hda_cache_head));
	snd_array_init(&codec->mixers, sizeof(struct hda_nid_item), 32);
	snd_array_init(&codec->nids, sizeof(struct hda_nid_item), 32);
	snd_array_init(&codec->init_pins, sizeof(struct hda_pincfg), 16);
	snd_array_init(&codec->driver_pins, sizeof(struct hda_pincfg), 16);
	snd_array_init(&codec->cvt_setups, sizeof(struct hda_cvt_setup), 8);
	snd_array_init(&codec->conn_lists, sizeof(hda_nid_t), 64);
	snd_array_init(&codec->spdif_out, sizeof(struct hda_spdif_out), 16);
	if (codec->bus->modelname) {
		codec->modelname = kstrdup(codec->bus->modelname, GFP_KERNEL);
		if (!codec->modelname) {
			snd_hda_codec_free(codec);
			return -ENODEV;
		}
	}

#ifdef CONFIG_SND_HDA_POWER_SAVE
	INIT_DELAYED_WORK(&codec->power_work, hda_power_work);
	hda_keep_power_on(codec);
#endif

	list_add_tail(&codec->list, &bus->codec_list);
	bus->caddr_tbl[codec_addr] = codec;

	codec->vendor_id = snd_hda_param_read(codec, AC_NODE_ROOT,
					      AC_PAR_VENDOR_ID);
	if (codec->vendor_id == -1)
		codec->vendor_id = snd_hda_param_read(codec, AC_NODE_ROOT,
						      AC_PAR_VENDOR_ID);
	codec->subsystem_id = snd_hda_param_read(codec, AC_NODE_ROOT,
						 AC_PAR_SUBSYSTEM_ID);
	codec->revision_id = snd_hda_param_read(codec, AC_NODE_ROOT,
						AC_PAR_REV_ID);

	setup_fg_nodes(codec);
	if (!codec->afg && !codec->mfg) {
		snd_printdd("hda_codec: no AFG or MFG node found\n");
		err = -ENODEV;
		goto error;
	}

	err = read_widget_caps(codec, codec->afg ? codec->afg : codec->mfg);
	if (err < 0) {
		snd_printk(KERN_ERR "hda_codec: cannot malloc\n");
		goto error;
	}
	err = read_pin_defaults(codec);
	if (err < 0)
		goto error;

	if (!codec->subsystem_id) {
		hda_nid_t nid = codec->afg ? codec->afg : codec->mfg;
		codec->subsystem_id =
			snd_hda_codec_read(codec, nid, 0,
					   AC_VERB_GET_SUBSYSTEM_ID, 0);
	}

	
	hda_set_power_state(codec,
			    codec->afg ? codec->afg : codec->mfg,
			    AC_PWRST_D0);

	snd_hda_codec_proc_new(codec);

	snd_hda_create_hwdep(codec);

	sprintf(component, "HDA:%08x,%08x,%08x", codec->vendor_id,
		codec->subsystem_id, codec->revision_id);
	snd_component_add(codec->bus->card, component);

	if (codecp)
		*codecp = codec;
	return 0;

 error:
	snd_hda_codec_free(codec);
	return err;
}
EXPORT_SYMBOL_HDA(snd_hda_codec_new);

int snd_hda_codec_configure(struct hda_codec *codec)
{
	int err;

	codec->preset = find_codec_preset(codec);
	if (!codec->vendor_name || !codec->chip_name) {
		err = get_codec_name(codec);
		if (err < 0)
			return err;
	}

	if (is_generic_config(codec)) {
		err = snd_hda_parse_generic_codec(codec);
		goto patched;
	}
	if (codec->preset && codec->preset->patch) {
		err = codec->preset->patch(codec);
		goto patched;
	}

	
	err = snd_hda_parse_generic_codec(codec);
	if (err < 0)
		printk(KERN_ERR "hda-codec: No codec parser is available\n");

 patched:
	if (!err && codec->patch_ops.unsol_event)
		err = init_unsol_queue(codec->bus);
	
	if (!err && (codec->afg || !*codec->bus->card->mixername))
		snprintf(codec->bus->card->mixername,
			 sizeof(codec->bus->card->mixername),
			 "%s %s", codec->vendor_name, codec->chip_name);
	return err;
}
EXPORT_SYMBOL_HDA(snd_hda_codec_configure);

void snd_hda_codec_setup_stream(struct hda_codec *codec, hda_nid_t nid,
				u32 stream_tag,
				int channel_id, int format)
{
	struct hda_codec *c;
	struct hda_cvt_setup *p;
	unsigned int oldval, newval;
	int type;
	int i;

	if (!nid)
		return;

	snd_printdd("hda_codec_setup_stream: "
		    "NID=0x%x, stream=0x%x, channel=%d, format=0x%x\n",
		    nid, stream_tag, channel_id, format);
	p = get_hda_cvt_setup(codec, nid);
	if (!p)
		return;
	
	if (p->stream_tag != stream_tag || p->channel_id != channel_id) {
		oldval = snd_hda_codec_read(codec, nid, 0, AC_VERB_GET_CONV, 0);
		newval = (stream_tag << 4) | channel_id;
		if (oldval != newval)
			snd_hda_codec_write(codec, nid, 0,
					    AC_VERB_SET_CHANNEL_STREAMID,
					    newval);
		p->stream_tag = stream_tag;
		p->channel_id = channel_id;
	}
	
	if (p->format_id != format) {
		oldval = snd_hda_codec_read(codec, nid, 0,
					    AC_VERB_GET_STREAM_FORMAT, 0);
		if (oldval != format) {
			msleep(1);
			snd_hda_codec_write(codec, nid, 0,
					    AC_VERB_SET_STREAM_FORMAT,
					    format);
		}
		p->format_id = format;
	}
	p->active = 1;
	p->dirty = 0;

	
	type = get_wcaps_type(get_wcaps(codec, nid));
	list_for_each_entry(c, &codec->bus->codec_list, list) {
		for (i = 0; i < c->cvt_setups.used; i++) {
			p = snd_array_elem(&c->cvt_setups, i);
			if (!p->active && p->stream_tag == stream_tag &&
			    get_wcaps_type(get_wcaps(c, p->nid)) == type)
				p->dirty = 1;
		}
	}
}
EXPORT_SYMBOL_HDA(snd_hda_codec_setup_stream);

static void really_cleanup_stream(struct hda_codec *codec,
				  struct hda_cvt_setup *q);

void __snd_hda_codec_cleanup_stream(struct hda_codec *codec, hda_nid_t nid,
				    int do_now)
{
	struct hda_cvt_setup *p;

	if (!nid)
		return;

	if (codec->no_sticky_stream)
		do_now = 1;

	snd_printdd("hda_codec_cleanup_stream: NID=0x%x\n", nid);
	p = get_hda_cvt_setup(codec, nid);
	if (p) {
		if (do_now)
			really_cleanup_stream(codec, p);
		else
			p->active = 0;
	}
}
EXPORT_SYMBOL_HDA(__snd_hda_codec_cleanup_stream);

static void really_cleanup_stream(struct hda_codec *codec,
				  struct hda_cvt_setup *q)
{
	hda_nid_t nid = q->nid;
	if (q->stream_tag || q->channel_id)
		snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_CHANNEL_STREAMID, 0);
	if (q->format_id)
		snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_STREAM_FORMAT, 0
);
	memset(q, 0, sizeof(*q));
	q->nid = nid;
}

static void purify_inactive_streams(struct hda_codec *codec)
{
	struct hda_codec *c;
	int i;

	list_for_each_entry(c, &codec->bus->codec_list, list) {
		for (i = 0; i < c->cvt_setups.used; i++) {
			struct hda_cvt_setup *p;
			p = snd_array_elem(&c->cvt_setups, i);
			if (p->dirty)
				really_cleanup_stream(c, p);
		}
	}
}

#ifdef CONFIG_PM
static void hda_cleanup_all_streams(struct hda_codec *codec)
{
	int i;

	for (i = 0; i < codec->cvt_setups.used; i++) {
		struct hda_cvt_setup *p = snd_array_elem(&codec->cvt_setups, i);
		if (p->stream_tag)
			really_cleanup_stream(codec, p);
	}
}
#endif


#define HDA_HASH_KEY(nid, dir, idx) (u32)((nid) + ((idx) << 16) + ((dir) << 24))
#define HDA_HASH_PINCAP_KEY(nid) (u32)((nid) + (0x02 << 24))
#define HDA_HASH_PARPCM_KEY(nid) (u32)((nid) + (0x03 << 24))
#define HDA_HASH_PARSTR_KEY(nid) (u32)((nid) + (0x04 << 24))
#define INFO_AMP_CAPS	(1<<0)
#define INFO_AMP_VOL(ch)	(1 << (1 + (ch)))

static void  init_hda_cache(struct hda_cache_rec *cache,
				     unsigned int record_size)
{
	memset(cache, 0, sizeof(*cache));
	memset(cache->hash, 0xff, sizeof(cache->hash));
	snd_array_init(&cache->buf, record_size, 64);
}

static void free_hda_cache(struct hda_cache_rec *cache)
{
	snd_array_free(&cache->buf);
}

static struct hda_cache_head  *get_hash(struct hda_cache_rec *cache, u32 key)
{
	u16 idx = key % (u16)ARRAY_SIZE(cache->hash);
	u16 cur = cache->hash[idx];
	struct hda_cache_head *info;

	while (cur != 0xffff) {
		info = snd_array_elem(&cache->buf, cur);
		if (info->key == key)
			return info;
		cur = info->next;
	}
	return NULL;
}

static struct hda_cache_head  *get_alloc_hash(struct hda_cache_rec *cache,
					      u32 key)
{
	struct hda_cache_head *info = get_hash(cache, key);
	if (!info) {
		u16 idx, cur;
		
		info = snd_array_new(&cache->buf);
		if (!info)
			return NULL;
		cur = snd_array_index(&cache->buf, info);
		info->key = key;
		info->val = 0;
		idx = key % (u16)ARRAY_SIZE(cache->hash);
		info->next = cache->hash[idx];
		cache->hash[idx] = cur;
	}
	return info;
}

static inline struct hda_amp_info *
get_alloc_amp_hash(struct hda_codec *codec, u32 key)
{
	return (struct hda_amp_info *)get_alloc_hash(&codec->amp_cache, key);
}

u32 query_amp_caps(struct hda_codec *codec, hda_nid_t nid, int direction)
{
	struct hda_amp_info *info;

	info = get_alloc_amp_hash(codec, HDA_HASH_KEY(nid, direction, 0));
	if (!info)
		return 0;
	if (!(info->head.val & INFO_AMP_CAPS)) {
		if (!(get_wcaps(codec, nid) & AC_WCAP_AMP_OVRD))
			nid = codec->afg;
		info->amp_caps = snd_hda_param_read(codec, nid,
						    direction == HDA_OUTPUT ?
						    AC_PAR_AMP_OUT_CAP :
						    AC_PAR_AMP_IN_CAP);
		if (info->amp_caps)
			info->head.val |= INFO_AMP_CAPS;
	}
	return info->amp_caps;
}
EXPORT_SYMBOL_HDA(query_amp_caps);

int snd_hda_override_amp_caps(struct hda_codec *codec, hda_nid_t nid, int dir,
			      unsigned int caps)
{
	struct hda_amp_info *info;

	info = get_alloc_amp_hash(codec, HDA_HASH_KEY(nid, dir, 0));
	if (!info)
		return -EINVAL;
	info->amp_caps = caps;
	info->head.val |= INFO_AMP_CAPS;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_override_amp_caps);

static unsigned int
query_caps_hash(struct hda_codec *codec, hda_nid_t nid, u32 key,
		unsigned int (*func)(struct hda_codec *, hda_nid_t))
{
	struct hda_amp_info *info;

	info = get_alloc_amp_hash(codec, key);
	if (!info)
		return 0;
	if (!info->head.val) {
		info->head.val |= INFO_AMP_CAPS;
		info->amp_caps = func(codec, nid);
	}
	return info->amp_caps;
}

static unsigned int read_pin_cap(struct hda_codec *codec, hda_nid_t nid)
{
	return snd_hda_param_read(codec, nid, AC_PAR_PIN_CAP);
}

u32 snd_hda_query_pin_caps(struct hda_codec *codec, hda_nid_t nid)
{
	return query_caps_hash(codec, nid, HDA_HASH_PINCAP_KEY(nid),
			       read_pin_cap);
}
EXPORT_SYMBOL_HDA(snd_hda_query_pin_caps);

int snd_hda_override_pin_caps(struct hda_codec *codec, hda_nid_t nid,
			      unsigned int caps)
{
	struct hda_amp_info *info;
	info = get_alloc_amp_hash(codec, HDA_HASH_PINCAP_KEY(nid));
	if (!info)
		return -ENOMEM;
	info->amp_caps = caps;
	info->head.val |= INFO_AMP_CAPS;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_override_pin_caps);

static unsigned int get_vol_mute(struct hda_codec *codec,
				 struct hda_amp_info *info, hda_nid_t nid,
				 int ch, int direction, int index)
{
	u32 val, parm;

	if (info->head.val & INFO_AMP_VOL(ch))
		return info->vol[ch];

	parm = ch ? AC_AMP_GET_RIGHT : AC_AMP_GET_LEFT;
	parm |= direction == HDA_OUTPUT ? AC_AMP_GET_OUTPUT : AC_AMP_GET_INPUT;
	parm |= index;
	val = snd_hda_codec_read(codec, nid, 0,
				 AC_VERB_GET_AMP_GAIN_MUTE, parm);
	info->vol[ch] = val & 0xff;
	info->head.val |= INFO_AMP_VOL(ch);
	return info->vol[ch];
}

static void put_vol_mute(struct hda_codec *codec, struct hda_amp_info *info,
			 hda_nid_t nid, int ch, int direction, int index,
			 int val)
{
	u32 parm;

	parm = ch ? AC_AMP_SET_RIGHT : AC_AMP_SET_LEFT;
	parm |= direction == HDA_OUTPUT ? AC_AMP_SET_OUTPUT : AC_AMP_SET_INPUT;
	parm |= index << AC_AMP_SET_INDEX_SHIFT;
	if ((val & HDA_AMP_MUTE) && !(info->amp_caps & AC_AMPCAP_MUTE) &&
	    (info->amp_caps & AC_AMPCAP_MIN_MUTE))
		; 
	else
		parm |= val;
	snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_AMP_GAIN_MUTE, parm);
	info->vol[ch] = val;
}

int snd_hda_codec_amp_read(struct hda_codec *codec, hda_nid_t nid, int ch,
			   int direction, int index)
{
	struct hda_amp_info *info;
	info = get_alloc_amp_hash(codec, HDA_HASH_KEY(nid, direction, index));
	if (!info)
		return 0;
	return get_vol_mute(codec, info, nid, ch, direction, index);
}
EXPORT_SYMBOL_HDA(snd_hda_codec_amp_read);

int snd_hda_codec_amp_update(struct hda_codec *codec, hda_nid_t nid, int ch,
			     int direction, int idx, int mask, int val)
{
	struct hda_amp_info *info;

	info = get_alloc_amp_hash(codec, HDA_HASH_KEY(nid, direction, idx));
	if (!info)
		return 0;
	if (snd_BUG_ON(mask & ~0xff))
		mask &= 0xff;
	val &= mask;
	val |= get_vol_mute(codec, info, nid, ch, direction, idx) & ~mask;
	if (info->vol[ch] == val)
		return 0;
	put_vol_mute(codec, info, nid, ch, direction, idx, val);
	return 1;
}
EXPORT_SYMBOL_HDA(snd_hda_codec_amp_update);

int snd_hda_codec_amp_stereo(struct hda_codec *codec, hda_nid_t nid,
			     int direction, int idx, int mask, int val)
{
	int ch, ret = 0;

	if (snd_BUG_ON(mask & ~0xff))
		mask &= 0xff;
	for (ch = 0; ch < 2; ch++)
		ret |= snd_hda_codec_amp_update(codec, nid, ch, direction,
						idx, mask, val);
	return ret;
}
EXPORT_SYMBOL_HDA(snd_hda_codec_amp_stereo);

#ifdef CONFIG_PM
void snd_hda_codec_resume_amp(struct hda_codec *codec)
{
	struct hda_amp_info *buffer = codec->amp_cache.buf.list;
	int i;

	for (i = 0; i < codec->amp_cache.buf.used; i++, buffer++) {
		u32 key = buffer->head.key;
		hda_nid_t nid;
		unsigned int idx, dir, ch;
		if (!key)
			continue;
		nid = key & 0xff;
		idx = (key >> 16) & 0xff;
		dir = (key >> 24) & 0xff;
		for (ch = 0; ch < 2; ch++) {
			if (!(buffer->head.val & INFO_AMP_VOL(ch)))
				continue;
			put_vol_mute(codec, buffer, nid, ch, dir, idx,
				     buffer->vol[ch]);
		}
	}
}
EXPORT_SYMBOL_HDA(snd_hda_codec_resume_amp);
#endif 

static u32 get_amp_max_value(struct hda_codec *codec, hda_nid_t nid, int dir,
			     unsigned int ofs)
{
	u32 caps = query_amp_caps(codec, nid, dir);
	
	caps = (caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT;
	if (ofs < caps)
		caps -= ofs;
	return caps;
}

int snd_hda_mixer_amp_volume_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	u16 nid = get_amp_nid(kcontrol);
	u8 chs = get_amp_channels(kcontrol);
	int dir = get_amp_direction(kcontrol);
	unsigned int ofs = get_amp_offset(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = chs == 3 ? 2 : 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = get_amp_max_value(codec, nid, dir, ofs);
	if (!uinfo->value.integer.max) {
		printk(KERN_WARNING "hda_codec: "
		       "num_steps = 0 for NID=0x%x (ctl = %s)\n", nid,
		       kcontrol->id.name);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_amp_volume_info);


static inline unsigned int
read_amp_value(struct hda_codec *codec, hda_nid_t nid,
	       int ch, int dir, int idx, unsigned int ofs)
{
	unsigned int val;
	val = snd_hda_codec_amp_read(codec, nid, ch, dir, idx);
	val &= HDA_AMP_VOLMASK;
	if (val >= ofs)
		val -= ofs;
	else
		val = 0;
	return val;
}

static inline int
update_amp_value(struct hda_codec *codec, hda_nid_t nid,
		 int ch, int dir, int idx, unsigned int ofs,
		 unsigned int val)
{
	unsigned int maxval;

	if (val > 0)
		val += ofs;
	
	maxval = get_amp_max_value(codec, nid, dir, 0);
	if (val > maxval)
		val = maxval;
	return snd_hda_codec_amp_update(codec, nid, ch, dir, idx,
					HDA_AMP_VOLMASK, val);
}

int snd_hda_mixer_amp_volume_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = get_amp_nid(kcontrol);
	int chs = get_amp_channels(kcontrol);
	int dir = get_amp_direction(kcontrol);
	int idx = get_amp_index(kcontrol);
	unsigned int ofs = get_amp_offset(kcontrol);
	long *valp = ucontrol->value.integer.value;

	if (chs & 1)
		*valp++ = read_amp_value(codec, nid, 0, dir, idx, ofs);
	if (chs & 2)
		*valp = read_amp_value(codec, nid, 1, dir, idx, ofs);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_amp_volume_get);

int snd_hda_mixer_amp_volume_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = get_amp_nid(kcontrol);
	int chs = get_amp_channels(kcontrol);
	int dir = get_amp_direction(kcontrol);
	int idx = get_amp_index(kcontrol);
	unsigned int ofs = get_amp_offset(kcontrol);
	long *valp = ucontrol->value.integer.value;
	int change = 0;

	snd_hda_power_up(codec);
	if (chs & 1) {
		change = update_amp_value(codec, nid, 0, dir, idx, ofs, *valp);
		valp++;
	}
	if (chs & 2)
		change |= update_amp_value(codec, nid, 1, dir, idx, ofs, *valp);
	snd_hda_power_down(codec);
	return change;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_amp_volume_put);

int snd_hda_mixer_amp_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			  unsigned int size, unsigned int __user *_tlv)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = get_amp_nid(kcontrol);
	int dir = get_amp_direction(kcontrol);
	unsigned int ofs = get_amp_offset(kcontrol);
	bool min_mute = get_amp_min_mute(kcontrol);
	u32 caps, val1, val2;

	if (size < 4 * sizeof(unsigned int))
		return -ENOMEM;
	caps = query_amp_caps(codec, nid, dir);
	val2 = (caps & AC_AMPCAP_STEP_SIZE) >> AC_AMPCAP_STEP_SIZE_SHIFT;
	val2 = (val2 + 1) * 25;
	val1 = -((caps & AC_AMPCAP_OFFSET) >> AC_AMPCAP_OFFSET_SHIFT);
	val1 += ofs;
	val1 = ((int)val1) * ((int)val2);
	if (min_mute || (caps & AC_AMPCAP_MIN_MUTE))
		val2 |= TLV_DB_SCALE_MUTE;
	if (put_user(SNDRV_CTL_TLVT_DB_SCALE, _tlv))
		return -EFAULT;
	if (put_user(2 * sizeof(unsigned int), _tlv + 1))
		return -EFAULT;
	if (put_user(val1, _tlv + 2))
		return -EFAULT;
	if (put_user(val2, _tlv + 3))
		return -EFAULT;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_amp_tlv);

void snd_hda_set_vmaster_tlv(struct hda_codec *codec, hda_nid_t nid, int dir,
			     unsigned int *tlv)
{
	u32 caps;
	int nums, step;

	caps = query_amp_caps(codec, nid, dir);
	nums = (caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT;
	step = (caps & AC_AMPCAP_STEP_SIZE) >> AC_AMPCAP_STEP_SIZE_SHIFT;
	step = (step + 1) * 25;
	tlv[0] = SNDRV_CTL_TLVT_DB_SCALE;
	tlv[1] = 2 * sizeof(unsigned int);
	tlv[2] = -nums * step;
	tlv[3] = step;
}
EXPORT_SYMBOL_HDA(snd_hda_set_vmaster_tlv);

static struct snd_kcontrol *
_snd_hda_find_mixer_ctl(struct hda_codec *codec,
			const char *name, int idx)
{
	struct snd_ctl_elem_id id;
	memset(&id, 0, sizeof(id));
	id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	id.index = idx;
	if (snd_BUG_ON(strlen(name) >= sizeof(id.name)))
		return NULL;
	strcpy(id.name, name);
	return snd_ctl_find_id(codec->bus->card, &id);
}

struct snd_kcontrol *snd_hda_find_mixer_ctl(struct hda_codec *codec,
					    const char *name)
{
	return _snd_hda_find_mixer_ctl(codec, name, 0);
}
EXPORT_SYMBOL_HDA(snd_hda_find_mixer_ctl);

static int find_empty_mixer_ctl_idx(struct hda_codec *codec, const char *name)
{
	int idx;
	for (idx = 0; idx < 16; idx++) { 
		if (!_snd_hda_find_mixer_ctl(codec, name, idx))
			return idx;
	}
	return -EBUSY;
}

int snd_hda_ctl_add(struct hda_codec *codec, hda_nid_t nid,
		    struct snd_kcontrol *kctl)
{
	int err;
	unsigned short flags = 0;
	struct hda_nid_item *item;

	if (kctl->id.subdevice & HDA_SUBDEV_AMP_FLAG) {
		flags |= HDA_NID_ITEM_AMP;
		if (nid == 0)
			nid = get_amp_nid_(kctl->private_value);
	}
	if ((kctl->id.subdevice & HDA_SUBDEV_NID_FLAG) != 0 && nid == 0)
		nid = kctl->id.subdevice & 0xffff;
	if (kctl->id.subdevice & (HDA_SUBDEV_NID_FLAG|HDA_SUBDEV_AMP_FLAG))
		kctl->id.subdevice = 0;
	err = snd_ctl_add(codec->bus->card, kctl);
	if (err < 0)
		return err;
	item = snd_array_new(&codec->mixers);
	if (!item)
		return -ENOMEM;
	item->kctl = kctl;
	item->nid = nid;
	item->flags = flags;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_ctl_add);

int snd_hda_add_nid(struct hda_codec *codec, struct snd_kcontrol *kctl,
		    unsigned int index, hda_nid_t nid)
{
	struct hda_nid_item *item;

	if (nid > 0) {
		item = snd_array_new(&codec->nids);
		if (!item)
			return -ENOMEM;
		item->kctl = kctl;
		item->index = index;
		item->nid = nid;
		return 0;
	}
	printk(KERN_ERR "hda-codec: no NID for mapping control %s:%d:%d\n",
	       kctl->id.name, kctl->id.index, index);
	return -EINVAL;
}
EXPORT_SYMBOL_HDA(snd_hda_add_nid);

void snd_hda_ctls_clear(struct hda_codec *codec)
{
	int i;
	struct hda_nid_item *items = codec->mixers.list;
	for (i = 0; i < codec->mixers.used; i++)
		snd_ctl_remove(codec->bus->card, items[i].kctl);
	snd_array_free(&codec->mixers);
	snd_array_free(&codec->nids);
}

static int hda_lock_devices(struct snd_card *card)
{
	spin_lock(&card->files_lock);
	if (card->shutdown) {
		spin_unlock(&card->files_lock);
		return -EINVAL;
	}
	card->shutdown = 1;
	spin_unlock(&card->files_lock);
	return 0;
}

static void hda_unlock_devices(struct snd_card *card)
{
	spin_lock(&card->files_lock);
	card->shutdown = 0;
	spin_unlock(&card->files_lock);
}

int snd_hda_codec_reset(struct hda_codec *codec)
{
	struct snd_card *card = codec->bus->card;
	int i, pcm;

	if (hda_lock_devices(card) < 0)
		return -EBUSY;
	
	if (!list_empty(&card->ctl_files)) {
		hda_unlock_devices(card);
		return -EBUSY;
	}
	for (pcm = 0; pcm < codec->num_pcms; pcm++) {
		struct hda_pcm *cpcm = &codec->pcm_info[pcm];
		if (!cpcm->pcm)
			continue;
		if (cpcm->pcm->streams[0].substream_opened ||
		    cpcm->pcm->streams[1].substream_opened) {
			hda_unlock_devices(card);
			return -EBUSY;
		}
	}

	

#ifdef CONFIG_SND_HDA_POWER_SAVE
	cancel_delayed_work(&codec->power_work);
	flush_workqueue(codec->bus->workq);
#endif
	snd_hda_ctls_clear(codec);
	
	for (i = 0; i < codec->num_pcms; i++) {
		if (codec->pcm_info[i].pcm) {
			snd_device_free(card, codec->pcm_info[i].pcm);
			clear_bit(codec->pcm_info[i].device,
				  codec->bus->pcm_dev_bits);
		}
	}
	if (codec->patch_ops.free)
		codec->patch_ops.free(codec);
	snd_hda_jack_tbl_clear(codec);
	codec->proc_widget_hook = NULL;
	codec->spec = NULL;
	free_hda_cache(&codec->amp_cache);
	free_hda_cache(&codec->cmd_cache);
	init_hda_cache(&codec->amp_cache, sizeof(struct hda_amp_info));
	init_hda_cache(&codec->cmd_cache, sizeof(struct hda_cache_head));
	
	snd_array_free(&codec->driver_pins);
	restore_pincfgs(codec);
	codec->num_pcms = 0;
	codec->pcm_info = NULL;
	codec->preset = NULL;
	memset(&codec->patch_ops, 0, sizeof(codec->patch_ops));
	codec->slave_dig_outs = NULL;
	codec->spdif_status_reset = 0;
	module_put(codec->owner);
	codec->owner = NULL;

	
	hda_unlock_devices(card);
	return 0;
}

typedef int (*map_slave_func_t)(void *, struct snd_kcontrol *);

static int map_slaves(struct hda_codec *codec, const char * const *slaves,
		      const char *suffix, map_slave_func_t func, void *data) 
{
	struct hda_nid_item *items;
	const char * const *s;
	int i, err;

	items = codec->mixers.list;
	for (i = 0; i < codec->mixers.used; i++) {
		struct snd_kcontrol *sctl = items[i].kctl;
		if (!sctl || !sctl->id.name ||
		    sctl->id.iface != SNDRV_CTL_ELEM_IFACE_MIXER)
			continue;
		for (s = slaves; *s; s++) {
			char tmpname[sizeof(sctl->id.name)];
			const char *name = *s;
			if (suffix) {
				snprintf(tmpname, sizeof(tmpname), "%s %s",
					 name, suffix);
				name = tmpname;
			}
			if (!strcmp(sctl->id.name, name)) {
				err = func(data, sctl);
				if (err)
					return err;
				break;
			}
		}
	}
	return 0;
}

static int check_slave_present(void *data, struct snd_kcontrol *sctl)
{
	return 1;
}

static int get_kctl_0dB_offset(struct snd_kcontrol *kctl)
{
	int _tlv[4];
	const int *tlv = NULL;
	int val = -1;

	if (kctl->vd[0].access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
		
		mm_segment_t fs = get_fs();
		set_fs(get_ds());
		if (!kctl->tlv.c(kctl, 0, sizeof(_tlv), _tlv))
			tlv = _tlv;
		set_fs(fs);
	} else if (kctl->vd[0].access & SNDRV_CTL_ELEM_ACCESS_TLV_READ)
		tlv = kctl->tlv.p;
	if (tlv && tlv[0] == SNDRV_CTL_TLVT_DB_SCALE)
		val = -tlv[2] / tlv[3];
	return val;
}

static int put_kctl_with_value(struct snd_kcontrol *kctl, int val)
{
	struct snd_ctl_elem_value *ucontrol;
	ucontrol = kzalloc(sizeof(*ucontrol), GFP_KERNEL);
	if (!ucontrol)
		return -ENOMEM;
	ucontrol->value.integer.value[0] = val;
	ucontrol->value.integer.value[1] = val;
	kctl->put(kctl, ucontrol);
	kfree(ucontrol);
	return 0;
}

static int init_slave_0dB(void *data, struct snd_kcontrol *slave)
{
	int offset = get_kctl_0dB_offset(slave);
	if (offset > 0)
		put_kctl_with_value(slave, offset);
	return 0;
}

static int init_slave_unmute(void *data, struct snd_kcontrol *slave)
{
	return put_kctl_with_value(slave, 1);
}

int __snd_hda_add_vmaster(struct hda_codec *codec, char *name,
			unsigned int *tlv, const char * const *slaves,
			  const char *suffix, bool init_slave_vol,
			  struct snd_kcontrol **ctl_ret)
{
	struct snd_kcontrol *kctl;
	int err;

	if (ctl_ret)
		*ctl_ret = NULL;

	err = map_slaves(codec, slaves, suffix, check_slave_present, NULL);
	if (err != 1) {
		snd_printdd("No slave found for %s\n", name);
		return 0;
	}
	kctl = snd_ctl_make_virtual_master(name, tlv);
	if (!kctl)
		return -ENOMEM;
	err = snd_hda_ctl_add(codec, 0, kctl);
	if (err < 0)
		return err;

	err = map_slaves(codec, slaves, suffix,
			 (map_slave_func_t)snd_ctl_add_slave, kctl);
	if (err < 0)
		return err;

	
	put_kctl_with_value(kctl, 0);
	if (init_slave_vol)
		map_slaves(codec, slaves, suffix,
			   tlv ? init_slave_0dB : init_slave_unmute, kctl);

	if (ctl_ret)
		*ctl_ret = kctl;
	return 0;
}
EXPORT_SYMBOL_HDA(__snd_hda_add_vmaster);

static int vmaster_mute_mode_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	static const char * const texts[] = {
		"Off", "On", "Follow Master"
	};
	unsigned int index;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 3;
	index = uinfo->value.enumerated.item;
	if (index >= 3)
		index = 2;
	strcpy(uinfo->value.enumerated.name, texts[index]);
	return 0;
}

static int vmaster_mute_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hda_vmaster_mute_hook *hook = snd_kcontrol_chip(kcontrol);
	ucontrol->value.enumerated.item[0] = hook->mute_mode;
	return 0;
}

static int vmaster_mute_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hda_vmaster_mute_hook *hook = snd_kcontrol_chip(kcontrol);
	unsigned int old_mode = hook->mute_mode;

	hook->mute_mode = ucontrol->value.enumerated.item[0];
	if (hook->mute_mode > HDA_VMUTE_FOLLOW_MASTER)
		hook->mute_mode = HDA_VMUTE_FOLLOW_MASTER;
	if (old_mode == hook->mute_mode)
		return 0;
	snd_hda_sync_vmaster_hook(hook);
	return 1;
}

static struct snd_kcontrol_new vmaster_mute_mode = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Mute-LED Mode",
	.info = vmaster_mute_mode_info,
	.get = vmaster_mute_mode_get,
	.put = vmaster_mute_mode_put,
};

int snd_hda_add_vmaster_hook(struct hda_codec *codec,
			     struct hda_vmaster_mute_hook *hook,
			     bool expose_enum_ctl)
{
	struct snd_kcontrol *kctl;

	if (!hook->hook || !hook->sw_kctl)
		return 0;
	snd_ctl_add_vmaster_hook(hook->sw_kctl, hook->hook, codec);
	hook->codec = codec;
	hook->mute_mode = HDA_VMUTE_FOLLOW_MASTER;
	if (!expose_enum_ctl)
		return 0;
	kctl = snd_ctl_new1(&vmaster_mute_mode, hook);
	if (!kctl)
		return -ENOMEM;
	return snd_hda_ctl_add(codec, 0, kctl);
}
EXPORT_SYMBOL_HDA(snd_hda_add_vmaster_hook);

void snd_hda_sync_vmaster_hook(struct hda_vmaster_mute_hook *hook)
{
	if (!hook->hook || !hook->codec)
		return;
	switch (hook->mute_mode) {
	case HDA_VMUTE_FOLLOW_MASTER:
		snd_ctl_sync_vmaster_hook(hook->sw_kctl);
		break;
	default:
		hook->hook(hook->codec, hook->mute_mode);
		break;
	}
}
EXPORT_SYMBOL_HDA(snd_hda_sync_vmaster_hook);


int snd_hda_mixer_amp_switch_info(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_info *uinfo)
{
	int chs = get_amp_channels(kcontrol);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = chs == 3 ? 2 : 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_amp_switch_info);

int snd_hda_mixer_amp_switch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = get_amp_nid(kcontrol);
	int chs = get_amp_channels(kcontrol);
	int dir = get_amp_direction(kcontrol);
	int idx = get_amp_index(kcontrol);
	long *valp = ucontrol->value.integer.value;

	if (chs & 1)
		*valp++ = (snd_hda_codec_amp_read(codec, nid, 0, dir, idx) &
			   HDA_AMP_MUTE) ? 0 : 1;
	if (chs & 2)
		*valp = (snd_hda_codec_amp_read(codec, nid, 1, dir, idx) &
			 HDA_AMP_MUTE) ? 0 : 1;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_amp_switch_get);

int snd_hda_mixer_amp_switch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = get_amp_nid(kcontrol);
	int chs = get_amp_channels(kcontrol);
	int dir = get_amp_direction(kcontrol);
	int idx = get_amp_index(kcontrol);
	long *valp = ucontrol->value.integer.value;
	int change = 0;

	snd_hda_power_up(codec);
	if (chs & 1) {
		change = snd_hda_codec_amp_update(codec, nid, 0, dir, idx,
						  HDA_AMP_MUTE,
						  *valp ? 0 : HDA_AMP_MUTE);
		valp++;
	}
	if (chs & 2)
		change |= snd_hda_codec_amp_update(codec, nid, 1, dir, idx,
						   HDA_AMP_MUTE,
						   *valp ? 0 : HDA_AMP_MUTE);
	hda_call_check_power_status(codec, nid);
	snd_hda_power_down(codec);
	return change;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_amp_switch_put);

#ifdef CONFIG_SND_HDA_INPUT_BEEP
int snd_hda_mixer_amp_switch_put_beep(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	long *valp = ucontrol->value.integer.value;

	snd_hda_enable_beep_device(codec, *valp);
	return snd_hda_mixer_amp_switch_put(kcontrol, ucontrol);
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_amp_switch_put_beep);
#endif 


#define AMP_VAL_IDX_SHIFT	19
#define AMP_VAL_IDX_MASK	(0x0f<<19)

int snd_hda_mixer_bind_switch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned long pval;
	int err;

	mutex_lock(&codec->control_mutex);
	pval = kcontrol->private_value;
	kcontrol->private_value = pval & ~AMP_VAL_IDX_MASK; 
	err = snd_hda_mixer_amp_switch_get(kcontrol, ucontrol);
	kcontrol->private_value = pval;
	mutex_unlock(&codec->control_mutex);
	return err;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_bind_switch_get);

int snd_hda_mixer_bind_switch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	unsigned long pval;
	int i, indices, err = 0, change = 0;

	mutex_lock(&codec->control_mutex);
	pval = kcontrol->private_value;
	indices = (pval & AMP_VAL_IDX_MASK) >> AMP_VAL_IDX_SHIFT;
	for (i = 0; i < indices; i++) {
		kcontrol->private_value = (pval & ~AMP_VAL_IDX_MASK) |
			(i << AMP_VAL_IDX_SHIFT);
		err = snd_hda_mixer_amp_switch_put(kcontrol, ucontrol);
		if (err < 0)
			break;
		change |= err;
	}
	kcontrol->private_value = pval;
	mutex_unlock(&codec->control_mutex);
	return err < 0 ? err : change;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_bind_switch_put);

int snd_hda_mixer_bind_ctls_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct hda_bind_ctls *c;
	int err;

	mutex_lock(&codec->control_mutex);
	c = (struct hda_bind_ctls *)kcontrol->private_value;
	kcontrol->private_value = *c->values;
	err = c->ops->info(kcontrol, uinfo);
	kcontrol->private_value = (long)c;
	mutex_unlock(&codec->control_mutex);
	return err;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_bind_ctls_info);

int snd_hda_mixer_bind_ctls_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct hda_bind_ctls *c;
	int err;

	mutex_lock(&codec->control_mutex);
	c = (struct hda_bind_ctls *)kcontrol->private_value;
	kcontrol->private_value = *c->values;
	err = c->ops->get(kcontrol, ucontrol);
	kcontrol->private_value = (long)c;
	mutex_unlock(&codec->control_mutex);
	return err;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_bind_ctls_get);

int snd_hda_mixer_bind_ctls_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct hda_bind_ctls *c;
	unsigned long *vals;
	int err = 0, change = 0;

	mutex_lock(&codec->control_mutex);
	c = (struct hda_bind_ctls *)kcontrol->private_value;
	for (vals = c->values; *vals; vals++) {
		kcontrol->private_value = *vals;
		err = c->ops->put(kcontrol, ucontrol);
		if (err < 0)
			break;
		change |= err;
	}
	kcontrol->private_value = (long)c;
	mutex_unlock(&codec->control_mutex);
	return err < 0 ? err : change;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_bind_ctls_put);

int snd_hda_mixer_bind_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			   unsigned int size, unsigned int __user *tlv)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	struct hda_bind_ctls *c;
	int err;

	mutex_lock(&codec->control_mutex);
	c = (struct hda_bind_ctls *)kcontrol->private_value;
	kcontrol->private_value = *c->values;
	err = c->ops->tlv(kcontrol, op_flag, size, tlv);
	kcontrol->private_value = (long)c;
	mutex_unlock(&codec->control_mutex);
	return err;
}
EXPORT_SYMBOL_HDA(snd_hda_mixer_bind_tlv);

struct hda_ctl_ops snd_hda_bind_vol = {
	.info = snd_hda_mixer_amp_volume_info,
	.get = snd_hda_mixer_amp_volume_get,
	.put = snd_hda_mixer_amp_volume_put,
	.tlv = snd_hda_mixer_amp_tlv
};
EXPORT_SYMBOL_HDA(snd_hda_bind_vol);

struct hda_ctl_ops snd_hda_bind_sw = {
	.info = snd_hda_mixer_amp_switch_info,
	.get = snd_hda_mixer_amp_switch_get,
	.put = snd_hda_mixer_amp_switch_put,
	.tlv = snd_hda_mixer_amp_tlv
};
EXPORT_SYMBOL_HDA(snd_hda_bind_sw);


static int snd_hda_spdif_mask_info(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_hda_spdif_cmask_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_PROFESSIONAL |
					   IEC958_AES0_NONAUDIO |
					   IEC958_AES0_CON_EMPHASIS_5015 |
					   IEC958_AES0_CON_NOT_COPYRIGHT;
	ucontrol->value.iec958.status[1] = IEC958_AES1_CON_CATEGORY |
					   IEC958_AES1_CON_ORIGINAL;
	return 0;
}

static int snd_hda_spdif_pmask_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.iec958.status[0] = IEC958_AES0_PROFESSIONAL |
					   IEC958_AES0_NONAUDIO |
					   IEC958_AES0_PRO_EMPHASIS_5015;
	return 0;
}

static int snd_hda_spdif_default_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	int idx = kcontrol->private_value;
	struct hda_spdif_out *spdif = snd_array_elem(&codec->spdif_out, idx);

	ucontrol->value.iec958.status[0] = spdif->status & 0xff;
	ucontrol->value.iec958.status[1] = (spdif->status >> 8) & 0xff;
	ucontrol->value.iec958.status[2] = (spdif->status >> 16) & 0xff;
	ucontrol->value.iec958.status[3] = (spdif->status >> 24) & 0xff;

	return 0;
}

static unsigned short convert_from_spdif_status(unsigned int sbits)
{
	unsigned short val = 0;

	if (sbits & IEC958_AES0_PROFESSIONAL)
		val |= AC_DIG1_PROFESSIONAL;
	if (sbits & IEC958_AES0_NONAUDIO)
		val |= AC_DIG1_NONAUDIO;
	if (sbits & IEC958_AES0_PROFESSIONAL) {
		if ((sbits & IEC958_AES0_PRO_EMPHASIS) ==
		    IEC958_AES0_PRO_EMPHASIS_5015)
			val |= AC_DIG1_EMPHASIS;
	} else {
		if ((sbits & IEC958_AES0_CON_EMPHASIS) ==
		    IEC958_AES0_CON_EMPHASIS_5015)
			val |= AC_DIG1_EMPHASIS;
		if (!(sbits & IEC958_AES0_CON_NOT_COPYRIGHT))
			val |= AC_DIG1_COPYRIGHT;
		if (sbits & (IEC958_AES1_CON_ORIGINAL << 8))
			val |= AC_DIG1_LEVEL;
		val |= sbits & (IEC958_AES1_CON_CATEGORY << 8);
	}
	return val;
}

static unsigned int convert_to_spdif_status(unsigned short val)
{
	unsigned int sbits = 0;

	if (val & AC_DIG1_NONAUDIO)
		sbits |= IEC958_AES0_NONAUDIO;
	if (val & AC_DIG1_PROFESSIONAL)
		sbits |= IEC958_AES0_PROFESSIONAL;
	if (sbits & IEC958_AES0_PROFESSIONAL) {
		if (sbits & AC_DIG1_EMPHASIS)
			sbits |= IEC958_AES0_PRO_EMPHASIS_5015;
	} else {
		if (val & AC_DIG1_EMPHASIS)
			sbits |= IEC958_AES0_CON_EMPHASIS_5015;
		if (!(val & AC_DIG1_COPYRIGHT))
			sbits |= IEC958_AES0_CON_NOT_COPYRIGHT;
		if (val & AC_DIG1_LEVEL)
			sbits |= (IEC958_AES1_CON_ORIGINAL << 8);
		sbits |= val & (0x7f << 8);
	}
	return sbits;
}

static void set_dig_out(struct hda_codec *codec, hda_nid_t nid,
			int verb, int val)
{
	const hda_nid_t *d;

	snd_hda_codec_write_cache(codec, nid, 0, verb, val);
	d = codec->slave_dig_outs;
	if (!d)
		return;
	for (; *d; d++)
		snd_hda_codec_write_cache(codec, *d, 0, verb, val);
}

static inline void set_dig_out_convert(struct hda_codec *codec, hda_nid_t nid,
				       int dig1, int dig2)
{
	if (dig1 != -1)
		set_dig_out(codec, nid, AC_VERB_SET_DIGI_CONVERT_1, dig1);
	if (dig2 != -1)
		set_dig_out(codec, nid, AC_VERB_SET_DIGI_CONVERT_2, dig2);
}

static int snd_hda_spdif_default_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	int idx = kcontrol->private_value;
	struct hda_spdif_out *spdif = snd_array_elem(&codec->spdif_out, idx);
	hda_nid_t nid = spdif->nid;
	unsigned short val;
	int change;

	mutex_lock(&codec->spdif_mutex);
	spdif->status = ucontrol->value.iec958.status[0] |
		((unsigned int)ucontrol->value.iec958.status[1] << 8) |
		((unsigned int)ucontrol->value.iec958.status[2] << 16) |
		((unsigned int)ucontrol->value.iec958.status[3] << 24);
	val = convert_from_spdif_status(spdif->status);
	val |= spdif->ctls & 1;
	change = spdif->ctls != val;
	spdif->ctls = val;
	if (change && nid != (u16)-1)
		set_dig_out_convert(codec, nid, val & 0xff, (val >> 8) & 0xff);
	mutex_unlock(&codec->spdif_mutex);
	return change;
}

#define snd_hda_spdif_out_switch_info	snd_ctl_boolean_mono_info

static int snd_hda_spdif_out_switch_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	int idx = kcontrol->private_value;
	struct hda_spdif_out *spdif = snd_array_elem(&codec->spdif_out, idx);

	ucontrol->value.integer.value[0] = spdif->ctls & AC_DIG1_ENABLE;
	return 0;
}

static inline void set_spdif_ctls(struct hda_codec *codec, hda_nid_t nid,
				  int dig1, int dig2)
{
	set_dig_out_convert(codec, nid, dig1, dig2);
	
	if ((get_wcaps(codec, nid) & AC_WCAP_OUT_AMP) &&
	    (dig1 & AC_DIG1_ENABLE))
		snd_hda_codec_amp_stereo(codec, nid, HDA_OUTPUT, 0,
					    HDA_AMP_MUTE, 0);
}

static int snd_hda_spdif_out_switch_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	int idx = kcontrol->private_value;
	struct hda_spdif_out *spdif = snd_array_elem(&codec->spdif_out, idx);
	hda_nid_t nid = spdif->nid;
	unsigned short val;
	int change;

	mutex_lock(&codec->spdif_mutex);
	val = spdif->ctls & ~AC_DIG1_ENABLE;
	if (ucontrol->value.integer.value[0])
		val |= AC_DIG1_ENABLE;
	change = spdif->ctls != val;
	spdif->ctls = val;
	if (change && nid != (u16)-1)
		set_spdif_ctls(codec, nid, val & 0xff, -1);
	mutex_unlock(&codec->spdif_mutex);
	return change;
}

static struct snd_kcontrol_new dig_mixes[] = {
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, CON_MASK),
		.info = snd_hda_spdif_mask_info,
		.get = snd_hda_spdif_cmask_get,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, PRO_MASK),
		.info = snd_hda_spdif_mask_info,
		.get = snd_hda_spdif_pmask_get,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.info = snd_hda_spdif_mask_info,
		.get = snd_hda_spdif_default_get,
		.put = snd_hda_spdif_default_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, SWITCH),
		.info = snd_hda_spdif_out_switch_info,
		.get = snd_hda_spdif_out_switch_get,
		.put = snd_hda_spdif_out_switch_put,
	},
	{ } 
};

int snd_hda_create_spdif_out_ctls(struct hda_codec *codec,
				  hda_nid_t associated_nid,
				  hda_nid_t cvt_nid)
{
	int err;
	struct snd_kcontrol *kctl;
	struct snd_kcontrol_new *dig_mix;
	int idx;
	struct hda_spdif_out *spdif;

	idx = find_empty_mixer_ctl_idx(codec, "IEC958 Playback Switch");
	if (idx < 0) {
		printk(KERN_ERR "hda_codec: too many IEC958 outputs\n");
		return -EBUSY;
	}
	spdif = snd_array_new(&codec->spdif_out);
	for (dig_mix = dig_mixes; dig_mix->name; dig_mix++) {
		kctl = snd_ctl_new1(dig_mix, codec);
		if (!kctl)
			return -ENOMEM;
		kctl->id.index = idx;
		kctl->private_value = codec->spdif_out.used - 1;
		err = snd_hda_ctl_add(codec, associated_nid, kctl);
		if (err < 0)
			return err;
	}
	spdif->nid = cvt_nid;
	spdif->ctls = snd_hda_codec_read(codec, cvt_nid, 0,
					 AC_VERB_GET_DIGI_CONVERT_1, 0);
	spdif->status = convert_to_spdif_status(spdif->ctls);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_create_spdif_out_ctls);

struct hda_spdif_out *snd_hda_spdif_out_of_nid(struct hda_codec *codec,
					       hda_nid_t nid)
{
	int i;
	for (i = 0; i < codec->spdif_out.used; i++) {
		struct hda_spdif_out *spdif =
				snd_array_elem(&codec->spdif_out, i);
		if (spdif->nid == nid)
			return spdif;
	}
	return NULL;
}
EXPORT_SYMBOL_HDA(snd_hda_spdif_out_of_nid);

void snd_hda_spdif_ctls_unassign(struct hda_codec *codec, int idx)
{
	struct hda_spdif_out *spdif = snd_array_elem(&codec->spdif_out, idx);

	mutex_lock(&codec->spdif_mutex);
	spdif->nid = (u16)-1;
	mutex_unlock(&codec->spdif_mutex);
}
EXPORT_SYMBOL_HDA(snd_hda_spdif_ctls_unassign);

void snd_hda_spdif_ctls_assign(struct hda_codec *codec, int idx, hda_nid_t nid)
{
	struct hda_spdif_out *spdif = snd_array_elem(&codec->spdif_out, idx);
	unsigned short val;

	mutex_lock(&codec->spdif_mutex);
	if (spdif->nid != nid) {
		spdif->nid = nid;
		val = spdif->ctls;
		set_spdif_ctls(codec, nid, val & 0xff, (val >> 8) & 0xff);
	}
	mutex_unlock(&codec->spdif_mutex);
}
EXPORT_SYMBOL_HDA(snd_hda_spdif_ctls_assign);

static int spdif_share_sw_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_multi_out *mout = snd_kcontrol_chip(kcontrol);
	ucontrol->value.integer.value[0] = mout->share_spdif;
	return 0;
}

static int spdif_share_sw_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct hda_multi_out *mout = snd_kcontrol_chip(kcontrol);
	mout->share_spdif = !!ucontrol->value.integer.value[0];
	return 0;
}

static struct snd_kcontrol_new spdif_share_sw = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "IEC958 Default PCM Playback Switch",
	.info = snd_ctl_boolean_mono_info,
	.get = spdif_share_sw_get,
	.put = spdif_share_sw_put,
};

int snd_hda_create_spdif_share_sw(struct hda_codec *codec,
				  struct hda_multi_out *mout)
{
	if (!mout->dig_out_nid)
		return 0;
	
	return snd_hda_ctl_add(codec, mout->dig_out_nid,
			      snd_ctl_new1(&spdif_share_sw, mout));
}
EXPORT_SYMBOL_HDA(snd_hda_create_spdif_share_sw);


#define snd_hda_spdif_in_switch_info	snd_hda_spdif_out_switch_info

static int snd_hda_spdif_in_switch_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = codec->spdif_in_enable;
	return 0;
}

static int snd_hda_spdif_in_switch_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value;
	unsigned int val = !!ucontrol->value.integer.value[0];
	int change;

	mutex_lock(&codec->spdif_mutex);
	change = codec->spdif_in_enable != val;
	if (change) {
		codec->spdif_in_enable = val;
		snd_hda_codec_write_cache(codec, nid, 0,
					  AC_VERB_SET_DIGI_CONVERT_1, val);
	}
	mutex_unlock(&codec->spdif_mutex);
	return change;
}

static int snd_hda_spdif_in_status_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct hda_codec *codec = snd_kcontrol_chip(kcontrol);
	hda_nid_t nid = kcontrol->private_value;
	unsigned short val;
	unsigned int sbits;

	val = snd_hda_codec_read(codec, nid, 0, AC_VERB_GET_DIGI_CONVERT_1, 0);
	sbits = convert_to_spdif_status(val);
	ucontrol->value.iec958.status[0] = sbits;
	ucontrol->value.iec958.status[1] = sbits >> 8;
	ucontrol->value.iec958.status[2] = sbits >> 16;
	ucontrol->value.iec958.status[3] = sbits >> 24;
	return 0;
}

static struct snd_kcontrol_new dig_in_ctls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("", CAPTURE, SWITCH),
		.info = snd_hda_spdif_in_switch_info,
		.get = snd_hda_spdif_in_switch_get,
		.put = snd_hda_spdif_in_switch_put,
	},
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = SNDRV_CTL_NAME_IEC958("", CAPTURE, DEFAULT),
		.info = snd_hda_spdif_mask_info,
		.get = snd_hda_spdif_in_status_get,
	},
	{ } 
};

int snd_hda_create_spdif_in_ctls(struct hda_codec *codec, hda_nid_t nid)
{
	int err;
	struct snd_kcontrol *kctl;
	struct snd_kcontrol_new *dig_mix;
	int idx;

	idx = find_empty_mixer_ctl_idx(codec, "IEC958 Capture Switch");
	if (idx < 0) {
		printk(KERN_ERR "hda_codec: too many IEC958 inputs\n");
		return -EBUSY;
	}
	for (dig_mix = dig_in_ctls; dig_mix->name; dig_mix++) {
		kctl = snd_ctl_new1(dig_mix, codec);
		if (!kctl)
			return -ENOMEM;
		kctl->private_value = nid;
		err = snd_hda_ctl_add(codec, nid, kctl);
		if (err < 0)
			return err;
	}
	codec->spdif_in_enable =
		snd_hda_codec_read(codec, nid, 0,
				   AC_VERB_GET_DIGI_CONVERT_1, 0) &
		AC_DIG1_ENABLE;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_create_spdif_in_ctls);

#ifdef CONFIG_PM

#define build_cmd_cache_key(nid, verb)	((verb << 8) | nid)
#define get_cmd_cache_nid(key)		((key) & 0xff)
#define get_cmd_cache_cmd(key)		(((key) >> 8) & 0xffff)

int snd_hda_codec_write_cache(struct hda_codec *codec, hda_nid_t nid,
			      int direct, unsigned int verb, unsigned int parm)
{
	int err = snd_hda_codec_write(codec, nid, direct, verb, parm);
	struct hda_cache_head *c;
	u32 key;

	if (err < 0)
		return err;
	
	verb = verb | (parm >> 8);
	parm &= 0xff;
	key = build_cmd_cache_key(nid, verb);
	mutex_lock(&codec->bus->cmd_mutex);
	c = get_alloc_hash(&codec->cmd_cache, key);
	if (c)
		c->val = parm;
	mutex_unlock(&codec->bus->cmd_mutex);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_codec_write_cache);

int snd_hda_codec_update_cache(struct hda_codec *codec, hda_nid_t nid,
			       int direct, unsigned int verb, unsigned int parm)
{
	struct hda_cache_head *c;
	u32 key;

	
	verb = verb | (parm >> 8);
	parm &= 0xff;
	key = build_cmd_cache_key(nid, verb);
	mutex_lock(&codec->bus->cmd_mutex);
	c = get_hash(&codec->cmd_cache, key);
	if (c && c->val == parm) {
		mutex_unlock(&codec->bus->cmd_mutex);
		return 0;
	}
	mutex_unlock(&codec->bus->cmd_mutex);
	return snd_hda_codec_write_cache(codec, nid, direct, verb, parm);
}
EXPORT_SYMBOL_HDA(snd_hda_codec_update_cache);

void snd_hda_codec_resume_cache(struct hda_codec *codec)
{
	struct hda_cache_head *buffer = codec->cmd_cache.buf.list;
	int i;

	for (i = 0; i < codec->cmd_cache.buf.used; i++, buffer++) {
		u32 key = buffer->key;
		if (!key)
			continue;
		snd_hda_codec_write(codec, get_cmd_cache_nid(key), 0,
				    get_cmd_cache_cmd(key), buffer->val);
	}
}
EXPORT_SYMBOL_HDA(snd_hda_codec_resume_cache);

void snd_hda_sequence_write_cache(struct hda_codec *codec,
				  const struct hda_verb *seq)
{
	for (; seq->nid; seq++)
		snd_hda_codec_write_cache(codec, seq->nid, 0, seq->verb,
					  seq->param);
}
EXPORT_SYMBOL_HDA(snd_hda_sequence_write_cache);
#endif 

void snd_hda_codec_set_power_to_all(struct hda_codec *codec, hda_nid_t fg,
				    unsigned int power_state,
				    bool eapd_workaround)
{
	hda_nid_t nid = codec->start_nid;
	int i;

	for (i = 0; i < codec->num_nodes; i++, nid++) {
		unsigned int wcaps = get_wcaps(codec, nid);
		if (!(wcaps & AC_WCAP_POWER))
			continue;
		if (eapd_workaround && power_state == AC_PWRST_D3 &&
		    get_wcaps_type(wcaps) == AC_WID_PIN &&
		    (snd_hda_query_pin_caps(codec, nid) & AC_PINCAP_EAPD)) {
			int eapd = snd_hda_codec_read(codec, nid, 0,
						AC_VERB_GET_EAPD_BTLENABLE, 0);
			if (eapd & 0x02)
				continue;
		}
		snd_hda_codec_write(codec, nid, 0, AC_VERB_SET_POWER_STATE,
				    power_state);
	}

	if (power_state == AC_PWRST_D0) {
		unsigned long end_time;
		int state;
		
		end_time = jiffies + msecs_to_jiffies(500);
		do {
			state = snd_hda_codec_read(codec, fg, 0,
						   AC_VERB_GET_POWER_STATE, 0);
			if (state == power_state)
				break;
			msleep(1);
		} while (time_after_eq(end_time, jiffies));
	}
}
EXPORT_SYMBOL_HDA(snd_hda_codec_set_power_to_all);

static void hda_set_power_state(struct hda_codec *codec, hda_nid_t fg,
				unsigned int power_state)
{
	if (codec->patch_ops.set_power_state) {
		codec->patch_ops.set_power_state(codec, fg, power_state);
		return;
	}

	
	if (power_state == AC_PWRST_D3)
		msleep(100);
	snd_hda_codec_read(codec, fg, 0, AC_VERB_SET_POWER_STATE,
			    power_state);
	snd_hda_codec_set_power_to_all(codec, fg, power_state, true);
}

#ifdef CONFIG_SND_HDA_HWDEP
static void hda_exec_init_verbs(struct hda_codec *codec)
{
	if (codec->init_verbs.list)
		snd_hda_sequence_write(codec, codec->init_verbs.list);
}
#else
static inline void hda_exec_init_verbs(struct hda_codec *codec) {}
#endif

#ifdef CONFIG_PM
static void hda_call_codec_suspend(struct hda_codec *codec)
{
	if (codec->patch_ops.suspend)
		codec->patch_ops.suspend(codec, PMSG_SUSPEND);
	hda_cleanup_all_streams(codec);
	hda_set_power_state(codec,
			    codec->afg ? codec->afg : codec->mfg,
			    AC_PWRST_D3);
#ifdef CONFIG_SND_HDA_POWER_SAVE
	snd_hda_update_power_acct(codec);
	cancel_delayed_work(&codec->power_work);
	codec->power_on = 0;
	codec->power_transition = 0;
	codec->power_jiffies = jiffies;
#endif
}

static void hda_call_codec_resume(struct hda_codec *codec)
{
	hda_set_power_state(codec,
			    codec->afg ? codec->afg : codec->mfg,
			    AC_PWRST_D0);
	restore_pincfgs(codec); 
	restore_shutup_pins(codec);
	hda_exec_init_verbs(codec);
	snd_hda_jack_set_dirty_all(codec);
	if (codec->patch_ops.resume)
		codec->patch_ops.resume(codec);
	else {
		if (codec->patch_ops.init)
			codec->patch_ops.init(codec);
		snd_hda_codec_resume_amp(codec);
		snd_hda_codec_resume_cache(codec);
	}
}
#endif 


int  snd_hda_build_controls(struct hda_bus *bus)
{
	struct hda_codec *codec;

	list_for_each_entry(codec, &bus->codec_list, list) {
		int err = snd_hda_codec_build_controls(codec);
		if (err < 0) {
			printk(KERN_ERR "hda_codec: cannot build controls "
			       "for #%d (error %d)\n", codec->addr, err);
			err = snd_hda_codec_reset(codec);
			if (err < 0) {
				printk(KERN_ERR
				       "hda_codec: cannot revert codec\n");
				return err;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_build_controls);

int snd_hda_codec_build_controls(struct hda_codec *codec)
{
	int err = 0;
	hda_exec_init_verbs(codec);
	
	if (codec->patch_ops.init)
		err = codec->patch_ops.init(codec);
	if (!err && codec->patch_ops.build_controls)
		err = codec->patch_ops.build_controls(codec);
	if (err < 0)
		return err;
	return 0;
}

struct hda_rate_tbl {
	unsigned int hz;
	unsigned int alsa_bits;
	unsigned int hda_fmt;
};

#define HDA_RATE(base, mult, div) \
	(AC_FMT_BASE_##base##K | (((mult) - 1) << AC_FMT_MULT_SHIFT) | \
	 (((div) - 1) << AC_FMT_DIV_SHIFT))

static struct hda_rate_tbl rate_bits[] = {
	

	
	{ 8000, SNDRV_PCM_RATE_8000, HDA_RATE(48, 1, 6) },
	{ 11025, SNDRV_PCM_RATE_11025, HDA_RATE(44, 1, 4) },
	{ 16000, SNDRV_PCM_RATE_16000, HDA_RATE(48, 1, 3) },
	{ 22050, SNDRV_PCM_RATE_22050, HDA_RATE(44, 1, 2) },
	{ 32000, SNDRV_PCM_RATE_32000, HDA_RATE(48, 2, 3) },
	{ 44100, SNDRV_PCM_RATE_44100, HDA_RATE(44, 1, 1) },
	{ 48000, SNDRV_PCM_RATE_48000, HDA_RATE(48, 1, 1) },
	{ 88200, SNDRV_PCM_RATE_88200, HDA_RATE(44, 2, 1) },
	{ 96000, SNDRV_PCM_RATE_96000, HDA_RATE(48, 2, 1) },
	{ 176400, SNDRV_PCM_RATE_176400, HDA_RATE(44, 4, 1) },
	{ 192000, SNDRV_PCM_RATE_192000, HDA_RATE(48, 4, 1) },
#define AC_PAR_PCM_RATE_BITS	11
	

	
	{ 9600, SNDRV_PCM_RATE_KNOT, HDA_RATE(48, 1, 5) },

	{ 0 } 
};

unsigned int snd_hda_calc_stream_format(unsigned int rate,
					unsigned int channels,
					unsigned int format,
					unsigned int maxbps,
					unsigned short spdif_ctls)
{
	int i;
	unsigned int val = 0;

	for (i = 0; rate_bits[i].hz; i++)
		if (rate_bits[i].hz == rate) {
			val = rate_bits[i].hda_fmt;
			break;
		}
	if (!rate_bits[i].hz) {
		snd_printdd("invalid rate %d\n", rate);
		return 0;
	}

	if (channels == 0 || channels > 8) {
		snd_printdd("invalid channels %d\n", channels);
		return 0;
	}
	val |= channels - 1;

	switch (snd_pcm_format_width(format)) {
	case 8:
		val |= AC_FMT_BITS_8;
		break;
	case 16:
		val |= AC_FMT_BITS_16;
		break;
	case 20:
	case 24:
	case 32:
		if (maxbps >= 32 || format == SNDRV_PCM_FORMAT_FLOAT_LE)
			val |= AC_FMT_BITS_32;
		else if (maxbps >= 24)
			val |= AC_FMT_BITS_24;
		else
			val |= AC_FMT_BITS_20;
		break;
	default:
		snd_printdd("invalid format width %d\n",
			    snd_pcm_format_width(format));
		return 0;
	}

	if (spdif_ctls & AC_DIG1_NONAUDIO)
		val |= AC_FMT_TYPE_NON_PCM;

	return val;
}
EXPORT_SYMBOL_HDA(snd_hda_calc_stream_format);

static unsigned int get_pcm_param(struct hda_codec *codec, hda_nid_t nid)
{
	unsigned int val = 0;
	if (nid != codec->afg &&
	    (get_wcaps(codec, nid) & AC_WCAP_FORMAT_OVRD))
		val = snd_hda_param_read(codec, nid, AC_PAR_PCM);
	if (!val || val == -1)
		val = snd_hda_param_read(codec, codec->afg, AC_PAR_PCM);
	if (!val || val == -1)
		return 0;
	return val;
}

static unsigned int query_pcm_param(struct hda_codec *codec, hda_nid_t nid)
{
	return query_caps_hash(codec, nid, HDA_HASH_PARPCM_KEY(nid),
			       get_pcm_param);
}

static unsigned int get_stream_param(struct hda_codec *codec, hda_nid_t nid)
{
	unsigned int streams = snd_hda_param_read(codec, nid, AC_PAR_STREAM);
	if (!streams || streams == -1)
		streams = snd_hda_param_read(codec, codec->afg, AC_PAR_STREAM);
	if (!streams || streams == -1)
		return 0;
	return streams;
}

static unsigned int query_stream_param(struct hda_codec *codec, hda_nid_t nid)
{
	return query_caps_hash(codec, nid, HDA_HASH_PARSTR_KEY(nid),
			       get_stream_param);
}

int snd_hda_query_supported_pcm(struct hda_codec *codec, hda_nid_t nid,
				u32 *ratesp, u64 *formatsp, unsigned int *bpsp)
{
	unsigned int i, val, wcaps;

	wcaps = get_wcaps(codec, nid);
	val = query_pcm_param(codec, nid);

	if (ratesp) {
		u32 rates = 0;
		for (i = 0; i < AC_PAR_PCM_RATE_BITS; i++) {
			if (val & (1 << i))
				rates |= rate_bits[i].alsa_bits;
		}
		if (rates == 0) {
			snd_printk(KERN_ERR "hda_codec: rates == 0 "
				   "(nid=0x%x, val=0x%x, ovrd=%i)\n",
					nid, val,
					(wcaps & AC_WCAP_FORMAT_OVRD) ? 1 : 0);
			return -EIO;
		}
		*ratesp = rates;
	}

	if (formatsp || bpsp) {
		u64 formats = 0;
		unsigned int streams, bps;

		streams = query_stream_param(codec, nid);
		if (!streams)
			return -EIO;

		bps = 0;
		if (streams & AC_SUPFMT_PCM) {
			if (val & AC_SUPPCM_BITS_8) {
				formats |= SNDRV_PCM_FMTBIT_U8;
				bps = 8;
			}
			if (val & AC_SUPPCM_BITS_16) {
				formats |= SNDRV_PCM_FMTBIT_S16_LE;
				bps = 16;
			}
			if (wcaps & AC_WCAP_DIGITAL) {
				if (val & AC_SUPPCM_BITS_32)
					formats |= SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE;
				if (val & (AC_SUPPCM_BITS_20|AC_SUPPCM_BITS_24))
					formats |= SNDRV_PCM_FMTBIT_S32_LE;
				if (val & AC_SUPPCM_BITS_24)
					bps = 24;
				else if (val & AC_SUPPCM_BITS_20)
					bps = 20;
			} else if (val & (AC_SUPPCM_BITS_20|AC_SUPPCM_BITS_24|
					  AC_SUPPCM_BITS_32)) {
				formats |= SNDRV_PCM_FMTBIT_S32_LE;
				if (val & AC_SUPPCM_BITS_32)
					bps = 32;
				else if (val & AC_SUPPCM_BITS_24)
					bps = 24;
				else if (val & AC_SUPPCM_BITS_20)
					bps = 20;
			}
		}
		if (streams & AC_SUPFMT_FLOAT32) {
			formats |= SNDRV_PCM_FMTBIT_FLOAT_LE;
			if (!bps)
				bps = 32;
		}
		if (streams == AC_SUPFMT_AC3) {
			
			formats |= SNDRV_PCM_FMTBIT_U8;
			bps = 8;
		}
		if (formats == 0) {
			snd_printk(KERN_ERR "hda_codec: formats == 0 "
				   "(nid=0x%x, val=0x%x, ovrd=%i, "
				   "streams=0x%x)\n",
					nid, val,
					(wcaps & AC_WCAP_FORMAT_OVRD) ? 1 : 0,
					streams);
			return -EIO;
		}
		if (formatsp)
			*formatsp = formats;
		if (bpsp)
			*bpsp = bps;
	}

	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_query_supported_pcm);

int snd_hda_is_supported_format(struct hda_codec *codec, hda_nid_t nid,
				unsigned int format)
{
	int i;
	unsigned int val = 0, rate, stream;

	val = query_pcm_param(codec, nid);
	if (!val)
		return 0;

	rate = format & 0xff00;
	for (i = 0; i < AC_PAR_PCM_RATE_BITS; i++)
		if (rate_bits[i].hda_fmt == rate) {
			if (val & (1 << i))
				break;
			return 0;
		}
	if (i >= AC_PAR_PCM_RATE_BITS)
		return 0;

	stream = query_stream_param(codec, nid);
	if (!stream)
		return 0;

	if (stream & AC_SUPFMT_PCM) {
		switch (format & 0xf0) {
		case 0x00:
			if (!(val & AC_SUPPCM_BITS_8))
				return 0;
			break;
		case 0x10:
			if (!(val & AC_SUPPCM_BITS_16))
				return 0;
			break;
		case 0x20:
			if (!(val & AC_SUPPCM_BITS_20))
				return 0;
			break;
		case 0x30:
			if (!(val & AC_SUPPCM_BITS_24))
				return 0;
			break;
		case 0x40:
			if (!(val & AC_SUPPCM_BITS_32))
				return 0;
			break;
		default:
			return 0;
		}
	} else {
		
	}

	return 1;
}
EXPORT_SYMBOL_HDA(snd_hda_is_supported_format);

static int hda_pcm_default_open_close(struct hda_pcm_stream *hinfo,
				      struct hda_codec *codec,
				      struct snd_pcm_substream *substream)
{
	return 0;
}

static int hda_pcm_default_prepare(struct hda_pcm_stream *hinfo,
				   struct hda_codec *codec,
				   unsigned int stream_tag,
				   unsigned int format,
				   struct snd_pcm_substream *substream)
{
	snd_hda_codec_setup_stream(codec, hinfo->nid, stream_tag, 0, format);
	return 0;
}

static int hda_pcm_default_cleanup(struct hda_pcm_stream *hinfo,
				   struct hda_codec *codec,
				   struct snd_pcm_substream *substream)
{
	snd_hda_codec_cleanup_stream(codec, hinfo->nid);
	return 0;
}

static int set_pcm_default_values(struct hda_codec *codec,
				  struct hda_pcm_stream *info)
{
	int err;

	
	if (info->nid && (!info->rates || !info->formats)) {
		err = snd_hda_query_supported_pcm(codec, info->nid,
				info->rates ? NULL : &info->rates,
				info->formats ? NULL : &info->formats,
				info->maxbps ? NULL : &info->maxbps);
		if (err < 0)
			return err;
	}
	if (info->ops.open == NULL)
		info->ops.open = hda_pcm_default_open_close;
	if (info->ops.close == NULL)
		info->ops.close = hda_pcm_default_open_close;
	if (info->ops.prepare == NULL) {
		if (snd_BUG_ON(!info->nid))
			return -EINVAL;
		info->ops.prepare = hda_pcm_default_prepare;
	}
	if (info->ops.cleanup == NULL) {
		if (snd_BUG_ON(!info->nid))
			return -EINVAL;
		info->ops.cleanup = hda_pcm_default_cleanup;
	}
	return 0;
}

int snd_hda_codec_prepare(struct hda_codec *codec,
			  struct hda_pcm_stream *hinfo,
			  unsigned int stream,
			  unsigned int format,
			  struct snd_pcm_substream *substream)
{
	int ret;
	mutex_lock(&codec->bus->prepare_mutex);
	ret = hinfo->ops.prepare(hinfo, codec, stream, format, substream);
	if (ret >= 0)
		purify_inactive_streams(codec);
	mutex_unlock(&codec->bus->prepare_mutex);
	return ret;
}
EXPORT_SYMBOL_HDA(snd_hda_codec_prepare);

void snd_hda_codec_cleanup(struct hda_codec *codec,
			   struct hda_pcm_stream *hinfo,
			   struct snd_pcm_substream *substream)
{
	mutex_lock(&codec->bus->prepare_mutex);
	hinfo->ops.cleanup(hinfo, codec, substream);
	mutex_unlock(&codec->bus->prepare_mutex);
}
EXPORT_SYMBOL_HDA(snd_hda_codec_cleanup);

const char *snd_hda_pcm_type_name[HDA_PCM_NTYPES] = {
	"Audio", "SPDIF", "HDMI", "Modem"
};

static int get_empty_pcm_device(struct hda_bus *bus, int type)
{
	
	static int audio_idx[HDA_PCM_NTYPES][5] = {
		[HDA_PCM_TYPE_AUDIO] = { 0, 2, 4, 5, -1 },
		[HDA_PCM_TYPE_SPDIF] = { 1, -1 },
		[HDA_PCM_TYPE_HDMI]  = { 3, 7, 8, 9, -1 },
		[HDA_PCM_TYPE_MODEM] = { 6, -1 },
	};
	int i;

	if (type >= HDA_PCM_NTYPES) {
		snd_printk(KERN_WARNING "Invalid PCM type %d\n", type);
		return -EINVAL;
	}

	for (i = 0; audio_idx[type][i] >= 0 ; i++)
		if (!test_and_set_bit(audio_idx[type][i], bus->pcm_dev_bits))
			return audio_idx[type][i];

	
	for (i = 10; i < 32; i++) {
		if (!test_and_set_bit(i, bus->pcm_dev_bits))
			return i;
	}

	snd_printk(KERN_WARNING "Too many %s devices\n",
		snd_hda_pcm_type_name[type]);
	return -EAGAIN;
}

static int snd_hda_attach_pcm(struct hda_codec *codec, struct hda_pcm *pcm)
{
	struct hda_bus *bus = codec->bus;
	struct hda_pcm_stream *info;
	int stream, err;

	if (snd_BUG_ON(!pcm->name))
		return -EINVAL;
	for (stream = 0; stream < 2; stream++) {
		info = &pcm->stream[stream];
		if (info->substreams) {
			err = set_pcm_default_values(codec, info);
			if (err < 0)
				return err;
		}
	}
	return bus->ops.attach_pcm(bus, codec, pcm);
}

int snd_hda_codec_build_pcms(struct hda_codec *codec)
{
	unsigned int pcm;
	int err;

	if (!codec->num_pcms) {
		if (!codec->patch_ops.build_pcms)
			return 0;
		err = codec->patch_ops.build_pcms(codec);
		if (err < 0) {
			printk(KERN_ERR "hda_codec: cannot build PCMs"
			       "for #%d (error %d)\n", codec->addr, err);
			err = snd_hda_codec_reset(codec);
			if (err < 0) {
				printk(KERN_ERR
				       "hda_codec: cannot revert codec\n");
				return err;
			}
		}
	}
	for (pcm = 0; pcm < codec->num_pcms; pcm++) {
		struct hda_pcm *cpcm = &codec->pcm_info[pcm];
		int dev;

		if (!cpcm->stream[0].substreams && !cpcm->stream[1].substreams)
			continue; 

		if (!cpcm->pcm) {
			dev = get_empty_pcm_device(codec->bus, cpcm->pcm_type);
			if (dev < 0)
				continue; 
			cpcm->device = dev;
			err = snd_hda_attach_pcm(codec, cpcm);
			if (err < 0) {
				printk(KERN_ERR "hda_codec: cannot attach "
				       "PCM stream %d for codec #%d\n",
				       dev, codec->addr);
				continue; 
			}
		}
	}
	return 0;
}

int __devinit snd_hda_build_pcms(struct hda_bus *bus)
{
	struct hda_codec *codec;

	list_for_each_entry(codec, &bus->codec_list, list) {
		int err = snd_hda_codec_build_pcms(codec);
		if (err < 0)
			return err;
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_build_pcms);

int snd_hda_check_board_config(struct hda_codec *codec,
			       int num_configs, const char * const *models,
			       const struct snd_pci_quirk *tbl)
{
	if (codec->modelname && models) {
		int i;
		for (i = 0; i < num_configs; i++) {
			if (models[i] &&
			    !strcmp(codec->modelname, models[i])) {
				snd_printd(KERN_INFO "hda_codec: model '%s' is "
					   "selected\n", models[i]);
				return i;
			}
		}
	}

	if (!codec->bus->pci || !tbl)
		return -1;

	tbl = snd_pci_quirk_lookup(codec->bus->pci, tbl);
	if (!tbl)
		return -1;
	if (tbl->value >= 0 && tbl->value < num_configs) {
#ifdef CONFIG_SND_DEBUG_VERBOSE
		char tmp[10];
		const char *model = NULL;
		if (models)
			model = models[tbl->value];
		if (!model) {
			sprintf(tmp, "#%d", tbl->value);
			model = tmp;
		}
		snd_printdd(KERN_INFO "hda_codec: model '%s' is selected "
			    "for config %x:%x (%s)\n",
			    model, tbl->subvendor, tbl->subdevice,
			    (tbl->name ? tbl->name : "Unknown device"));
#endif
		return tbl->value;
	}
	return -1;
}
EXPORT_SYMBOL_HDA(snd_hda_check_board_config);

int snd_hda_check_board_codec_sid_config(struct hda_codec *codec,
			       int num_configs, const char * const *models,
			       const struct snd_pci_quirk *tbl)
{
	const struct snd_pci_quirk *q;

	
	for (q = tbl; q->subvendor; q++) {
		unsigned int mask = 0xffff0000 | q->subdevice_mask;
		unsigned int id = (q->subdevice | (q->subvendor << 16)) & mask;
		if ((codec->subsystem_id & mask) == id)
			break;
	}

	if (!q->subvendor)
		return -1;

	tbl = q;

	if (tbl->value >= 0 && tbl->value < num_configs) {
#ifdef CONFIG_SND_DEBUG_VERBOSE
		char tmp[10];
		const char *model = NULL;
		if (models)
			model = models[tbl->value];
		if (!model) {
			sprintf(tmp, "#%d", tbl->value);
			model = tmp;
		}
		snd_printdd(KERN_INFO "hda_codec: model '%s' is selected "
			    "for config %x:%x (%s)\n",
			    model, tbl->subvendor, tbl->subdevice,
			    (tbl->name ? tbl->name : "Unknown device"));
#endif
		return tbl->value;
	}
	return -1;
}
EXPORT_SYMBOL_HDA(snd_hda_check_board_codec_sid_config);

int snd_hda_add_new_ctls(struct hda_codec *codec,
			 const struct snd_kcontrol_new *knew)
{
	int err;

	for (; knew->name; knew++) {
		struct snd_kcontrol *kctl;
		int addr = 0, idx = 0;
		if (knew->iface == -1)	
			continue;
		for (;;) {
			kctl = snd_ctl_new1(knew, codec);
			if (!kctl)
				return -ENOMEM;
			if (addr > 0)
				kctl->id.device = addr;
			if (idx > 0)
				kctl->id.index = idx;
			err = snd_hda_ctl_add(codec, 0, kctl);
			if (!err)
				break;
			if (!addr && codec->addr)
				addr = codec->addr;
			else if (!idx && !knew->index) {
				idx = find_empty_mixer_ctl_idx(codec,
							       knew->name);
				if (idx <= 0)
					return err;
			} else
				return err;
		}
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_add_new_ctls);

#ifdef CONFIG_SND_HDA_POWER_SAVE
static void hda_power_work(struct work_struct *work)
{
	struct hda_codec *codec =
		container_of(work, struct hda_codec, power_work.work);
	struct hda_bus *bus = codec->bus;

	if (!codec->power_on || codec->power_count) {
		codec->power_transition = 0;
		return;
	}

	trace_hda_power_down(codec);
	hda_call_codec_suspend(codec);
	if (bus->ops.pm_notify)
		bus->ops.pm_notify(bus);
}

static void hda_keep_power_on(struct hda_codec *codec)
{
	codec->power_count++;
	codec->power_on = 1;
	codec->power_jiffies = jiffies;
}

void snd_hda_update_power_acct(struct hda_codec *codec)
{
	unsigned long delta = jiffies - codec->power_jiffies;
	if (codec->power_on)
		codec->power_on_acct += delta;
	else
		codec->power_off_acct += delta;
	codec->power_jiffies += delta;
}

void snd_hda_power_up(struct hda_codec *codec)
{
	struct hda_bus *bus = codec->bus;

	codec->power_count++;
	if (codec->power_on || codec->power_transition)
		return;

	trace_hda_power_up(codec);
	snd_hda_update_power_acct(codec);
	codec->power_on = 1;
	codec->power_jiffies = jiffies;
	if (bus->ops.pm_notify)
		bus->ops.pm_notify(bus);
	hda_call_codec_resume(codec);
	cancel_delayed_work(&codec->power_work);
	codec->power_transition = 0;
}
EXPORT_SYMBOL_HDA(snd_hda_power_up);

#define power_save(codec)	\
	((codec)->bus->power_save ? *(codec)->bus->power_save : 0)

void snd_hda_power_down(struct hda_codec *codec)
{
	--codec->power_count;
	if (!codec->power_on || codec->power_count || codec->power_transition)
		return;
	if (power_save(codec)) {
		codec->power_transition = 1; 
		queue_delayed_work(codec->bus->workq, &codec->power_work,
				msecs_to_jiffies(power_save(codec) * 1000));
	}
}
EXPORT_SYMBOL_HDA(snd_hda_power_down);

int snd_hda_check_amp_list_power(struct hda_codec *codec,
				 struct hda_loopback_check *check,
				 hda_nid_t nid)
{
	const struct hda_amp_list *p;
	int ch, v;

	if (!check->amplist)
		return 0;
	for (p = check->amplist; p->nid; p++) {
		if (p->nid == nid)
			break;
	}
	if (!p->nid)
		return 0; 

	for (p = check->amplist; p->nid; p++) {
		for (ch = 0; ch < 2; ch++) {
			v = snd_hda_codec_amp_read(codec, p->nid, ch, p->dir,
						   p->idx);
			if (!(v & HDA_AMP_MUTE) && v > 0) {
				if (!check->power_on) {
					check->power_on = 1;
					snd_hda_power_up(codec);
				}
				return 1;
			}
		}
	}
	if (check->power_on) {
		check->power_on = 0;
		snd_hda_power_down(codec);
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_check_amp_list_power);
#endif


int snd_hda_ch_mode_info(struct hda_codec *codec,
			 struct snd_ctl_elem_info *uinfo,
			 const struct hda_channel_mode *chmode,
			 int num_chmodes)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = num_chmodes;
	if (uinfo->value.enumerated.item >= num_chmodes)
		uinfo->value.enumerated.item = num_chmodes - 1;
	sprintf(uinfo->value.enumerated.name, "%dch",
		chmode[uinfo->value.enumerated.item].channels);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_ch_mode_info);

int snd_hda_ch_mode_get(struct hda_codec *codec,
			struct snd_ctl_elem_value *ucontrol,
			const struct hda_channel_mode *chmode,
			int num_chmodes,
			int max_channels)
{
	int i;

	for (i = 0; i < num_chmodes; i++) {
		if (max_channels == chmode[i].channels) {
			ucontrol->value.enumerated.item[0] = i;
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_ch_mode_get);

int snd_hda_ch_mode_put(struct hda_codec *codec,
			struct snd_ctl_elem_value *ucontrol,
			const struct hda_channel_mode *chmode,
			int num_chmodes,
			int *max_channelsp)
{
	unsigned int mode;

	mode = ucontrol->value.enumerated.item[0];
	if (mode >= num_chmodes)
		return -EINVAL;
	if (*max_channelsp == chmode[mode].channels)
		return 0;
	
	*max_channelsp = chmode[mode].channels;
	if (chmode[mode].sequence)
		snd_hda_sequence_write_cache(codec, chmode[mode].sequence);
	return 1;
}
EXPORT_SYMBOL_HDA(snd_hda_ch_mode_put);


int snd_hda_input_mux_info(const struct hda_input_mux *imux,
			   struct snd_ctl_elem_info *uinfo)
{
	unsigned int index;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = imux->num_items;
	if (!imux->num_items)
		return 0;
	index = uinfo->value.enumerated.item;
	if (index >= imux->num_items)
		index = imux->num_items - 1;
	strcpy(uinfo->value.enumerated.name, imux->items[index].label);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_input_mux_info);

int snd_hda_input_mux_put(struct hda_codec *codec,
			  const struct hda_input_mux *imux,
			  struct snd_ctl_elem_value *ucontrol,
			  hda_nid_t nid,
			  unsigned int *cur_val)
{
	unsigned int idx;

	if (!imux->num_items)
		return 0;
	idx = ucontrol->value.enumerated.item[0];
	if (idx >= imux->num_items)
		idx = imux->num_items - 1;
	if (*cur_val == idx)
		return 0;
	snd_hda_codec_write_cache(codec, nid, 0, AC_VERB_SET_CONNECT_SEL,
				  imux->items[idx].index);
	*cur_val = idx;
	return 1;
}
EXPORT_SYMBOL_HDA(snd_hda_input_mux_put);



static void setup_dig_out_stream(struct hda_codec *codec, hda_nid_t nid,
				 unsigned int stream_tag, unsigned int format)
{
	struct hda_spdif_out *spdif = snd_hda_spdif_out_of_nid(codec, nid);

	
	if (codec->spdif_status_reset && (spdif->ctls & AC_DIG1_ENABLE))
		set_dig_out_convert(codec, nid,
				    spdif->ctls & ~AC_DIG1_ENABLE & 0xff,
				    -1);
	snd_hda_codec_setup_stream(codec, nid, stream_tag, 0, format);
	if (codec->slave_dig_outs) {
		const hda_nid_t *d;
		for (d = codec->slave_dig_outs; *d; d++)
			snd_hda_codec_setup_stream(codec, *d, stream_tag, 0,
						   format);
	}
	
	if (codec->spdif_status_reset && (spdif->ctls & AC_DIG1_ENABLE))
		set_dig_out_convert(codec, nid,
				    spdif->ctls & 0xff, -1);
}

static void cleanup_dig_out_stream(struct hda_codec *codec, hda_nid_t nid)
{
	snd_hda_codec_cleanup_stream(codec, nid);
	if (codec->slave_dig_outs) {
		const hda_nid_t *d;
		for (d = codec->slave_dig_outs; *d; d++)
			snd_hda_codec_cleanup_stream(codec, *d);
	}
}

void snd_hda_bus_reboot_notify(struct hda_bus *bus)
{
	struct hda_codec *codec;

	if (!bus)
		return;
	list_for_each_entry(codec, &bus->codec_list, list) {
		if (hda_codec_is_power_on(codec) &&
		    codec->patch_ops.reboot_notify)
			codec->patch_ops.reboot_notify(codec);
	}
}
EXPORT_SYMBOL_HDA(snd_hda_bus_reboot_notify);

int snd_hda_multi_out_dig_open(struct hda_codec *codec,
			       struct hda_multi_out *mout)
{
	mutex_lock(&codec->spdif_mutex);
	if (mout->dig_out_used == HDA_DIG_ANALOG_DUP)
		
		cleanup_dig_out_stream(codec, mout->dig_out_nid);
	mout->dig_out_used = HDA_DIG_EXCLUSIVE;
	mutex_unlock(&codec->spdif_mutex);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_multi_out_dig_open);

int snd_hda_multi_out_dig_prepare(struct hda_codec *codec,
				  struct hda_multi_out *mout,
				  unsigned int stream_tag,
				  unsigned int format,
				  struct snd_pcm_substream *substream)
{
	mutex_lock(&codec->spdif_mutex);
	setup_dig_out_stream(codec, mout->dig_out_nid, stream_tag, format);
	mutex_unlock(&codec->spdif_mutex);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_multi_out_dig_prepare);

int snd_hda_multi_out_dig_cleanup(struct hda_codec *codec,
				  struct hda_multi_out *mout)
{
	mutex_lock(&codec->spdif_mutex);
	cleanup_dig_out_stream(codec, mout->dig_out_nid);
	mutex_unlock(&codec->spdif_mutex);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_multi_out_dig_cleanup);

int snd_hda_multi_out_dig_close(struct hda_codec *codec,
				struct hda_multi_out *mout)
{
	mutex_lock(&codec->spdif_mutex);
	mout->dig_out_used = 0;
	mutex_unlock(&codec->spdif_mutex);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_multi_out_dig_close);

int snd_hda_multi_out_analog_open(struct hda_codec *codec,
				  struct hda_multi_out *mout,
				  struct snd_pcm_substream *substream,
				  struct hda_pcm_stream *hinfo)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	runtime->hw.channels_max = mout->max_channels;
	if (mout->dig_out_nid) {
		if (!mout->analog_rates) {
			mout->analog_rates = hinfo->rates;
			mout->analog_formats = hinfo->formats;
			mout->analog_maxbps = hinfo->maxbps;
		} else {
			runtime->hw.rates = mout->analog_rates;
			runtime->hw.formats = mout->analog_formats;
			hinfo->maxbps = mout->analog_maxbps;
		}
		if (!mout->spdif_rates) {
			snd_hda_query_supported_pcm(codec, mout->dig_out_nid,
						    &mout->spdif_rates,
						    &mout->spdif_formats,
						    &mout->spdif_maxbps);
		}
		mutex_lock(&codec->spdif_mutex);
		if (mout->share_spdif) {
			if ((runtime->hw.rates & mout->spdif_rates) &&
			    (runtime->hw.formats & mout->spdif_formats)) {
				runtime->hw.rates &= mout->spdif_rates;
				runtime->hw.formats &= mout->spdif_formats;
				if (mout->spdif_maxbps < hinfo->maxbps)
					hinfo->maxbps = mout->spdif_maxbps;
			} else {
				mout->share_spdif = 0;
				
			}
		}
		mutex_unlock(&codec->spdif_mutex);
	}
	return snd_pcm_hw_constraint_step(substream->runtime, 0,
					  SNDRV_PCM_HW_PARAM_CHANNELS, 2);
}
EXPORT_SYMBOL_HDA(snd_hda_multi_out_analog_open);

int snd_hda_multi_out_analog_prepare(struct hda_codec *codec,
				     struct hda_multi_out *mout,
				     unsigned int stream_tag,
				     unsigned int format,
				     struct snd_pcm_substream *substream)
{
	const hda_nid_t *nids = mout->dac_nids;
	int chs = substream->runtime->channels;
	struct hda_spdif_out *spdif =
			snd_hda_spdif_out_of_nid(codec, mout->dig_out_nid);
	int i;

	mutex_lock(&codec->spdif_mutex);
	if (mout->dig_out_nid && mout->share_spdif &&
	    mout->dig_out_used != HDA_DIG_EXCLUSIVE) {
		if (chs == 2 &&
		    snd_hda_is_supported_format(codec, mout->dig_out_nid,
						format) &&
		    !(spdif->status & IEC958_AES0_NONAUDIO)) {
			mout->dig_out_used = HDA_DIG_ANALOG_DUP;
			setup_dig_out_stream(codec, mout->dig_out_nid,
					     stream_tag, format);
		} else {
			mout->dig_out_used = 0;
			cleanup_dig_out_stream(codec, mout->dig_out_nid);
		}
	}
	mutex_unlock(&codec->spdif_mutex);

	
	snd_hda_codec_setup_stream(codec, nids[HDA_FRONT], stream_tag,
				   0, format);
	if (!mout->no_share_stream &&
	    mout->hp_nid && mout->hp_nid != nids[HDA_FRONT])
		
		snd_hda_codec_setup_stream(codec, mout->hp_nid, stream_tag,
					   0, format);
	
	for (i = 0; i < ARRAY_SIZE(mout->hp_out_nid); i++)
		if (!mout->no_share_stream && mout->hp_out_nid[i])
			snd_hda_codec_setup_stream(codec,
						   mout->hp_out_nid[i],
						   stream_tag, 0, format);
	for (i = 0; i < ARRAY_SIZE(mout->extra_out_nid); i++)
		if (!mout->no_share_stream && mout->extra_out_nid[i])
			snd_hda_codec_setup_stream(codec,
						   mout->extra_out_nid[i],
						   stream_tag, 0, format);

	
	for (i = 1; i < mout->num_dacs; i++) {
		if (chs >= (i + 1) * 2) 
			snd_hda_codec_setup_stream(codec, nids[i], stream_tag,
						   i * 2, format);
		else if (!mout->no_share_stream) 
			snd_hda_codec_setup_stream(codec, nids[i], stream_tag,
						   0, format);
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_multi_out_analog_prepare);

int snd_hda_multi_out_analog_cleanup(struct hda_codec *codec,
				     struct hda_multi_out *mout)
{
	const hda_nid_t *nids = mout->dac_nids;
	int i;

	for (i = 0; i < mout->num_dacs; i++)
		snd_hda_codec_cleanup_stream(codec, nids[i]);
	if (mout->hp_nid)
		snd_hda_codec_cleanup_stream(codec, mout->hp_nid);
	for (i = 0; i < ARRAY_SIZE(mout->hp_out_nid); i++)
		if (mout->hp_out_nid[i])
			snd_hda_codec_cleanup_stream(codec,
						     mout->hp_out_nid[i]);
	for (i = 0; i < ARRAY_SIZE(mout->extra_out_nid); i++)
		if (mout->extra_out_nid[i])
			snd_hda_codec_cleanup_stream(codec,
						     mout->extra_out_nid[i]);
	mutex_lock(&codec->spdif_mutex);
	if (mout->dig_out_nid && mout->dig_out_used == HDA_DIG_ANALOG_DUP) {
		cleanup_dig_out_stream(codec, mout->dig_out_nid);
		mout->dig_out_used = 0;
	}
	mutex_unlock(&codec->spdif_mutex);
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_multi_out_analog_cleanup);


static int is_in_nid_list(hda_nid_t nid, const hda_nid_t *list)
{
	for (; *list; list++)
		if (*list == nid)
			return 1;
	return 0;
}


static void sort_pins_by_sequence(hda_nid_t *pins, short *sequences,
				  int num_pins)
{
	int i, j;
	short seq;
	hda_nid_t nid;

	for (i = 0; i < num_pins; i++) {
		for (j = i + 1; j < num_pins; j++) {
			if (sequences[i] > sequences[j]) {
				seq = sequences[i];
				sequences[i] = sequences[j];
				sequences[j] = seq;
				nid = pins[i];
				pins[i] = pins[j];
				pins[j] = nid;
			}
		}
	}
}


static void add_auto_cfg_input_pin(struct auto_pin_cfg *cfg, hda_nid_t nid,
				   int type)
{
	if (cfg->num_inputs < AUTO_CFG_MAX_INS) {
		cfg->inputs[cfg->num_inputs].pin = nid;
		cfg->inputs[cfg->num_inputs].type = type;
		cfg->num_inputs++;
	}
}

static void sort_autocfg_input_pins(struct auto_pin_cfg *cfg)
{
	int i, j;

	for (i = 0; i < cfg->num_inputs; i++) {
		for (j = i + 1; j < cfg->num_inputs; j++) {
			if (cfg->inputs[i].type > cfg->inputs[j].type) {
				struct auto_pin_cfg_item tmp;
				tmp = cfg->inputs[i];
				cfg->inputs[i] = cfg->inputs[j];
				cfg->inputs[j] = tmp;
			}
		}
	}
}

static void reorder_outputs(unsigned int nums, hda_nid_t *pins)
{
	hda_nid_t nid;

	switch (nums) {
	case 3:
	case 4:
		nid = pins[1];
		pins[1] = pins[2];
		pins[2] = nid;
		break;
	}
}

int snd_hda_parse_pin_defcfg(struct hda_codec *codec,
			     struct auto_pin_cfg *cfg,
			     const hda_nid_t *ignore_nids,
			     unsigned int cond_flags)
{
	hda_nid_t nid, end_nid;
	short seq, assoc_line_out;
	short sequences_line_out[ARRAY_SIZE(cfg->line_out_pins)];
	short sequences_speaker[ARRAY_SIZE(cfg->speaker_pins)];
	short sequences_hp[ARRAY_SIZE(cfg->hp_pins)];
	int i;

	memset(cfg, 0, sizeof(*cfg));

	memset(sequences_line_out, 0, sizeof(sequences_line_out));
	memset(sequences_speaker, 0, sizeof(sequences_speaker));
	memset(sequences_hp, 0, sizeof(sequences_hp));
	assoc_line_out = 0;

	codec->ignore_misc_bit = true;
	end_nid = codec->start_nid + codec->num_nodes;
	for (nid = codec->start_nid; nid < end_nid; nid++) {
		unsigned int wid_caps = get_wcaps(codec, nid);
		unsigned int wid_type = get_wcaps_type(wid_caps);
		unsigned int def_conf;
		short assoc, loc, conn, dev;

		
		if (wid_type != AC_WID_PIN)
			continue;
		
		if (ignore_nids && is_in_nid_list(nid, ignore_nids))
			continue;

		def_conf = snd_hda_codec_get_pincfg(codec, nid);
		if (!(get_defcfg_misc(snd_hda_codec_get_pincfg(codec, nid)) &
		      AC_DEFCFG_MISC_NO_PRESENCE))
			codec->ignore_misc_bit = false;
		conn = get_defcfg_connect(def_conf);
		if (conn == AC_JACK_PORT_NONE)
			continue;
		loc = get_defcfg_location(def_conf);
		dev = get_defcfg_device(def_conf);

		
		if (dev == AC_JACK_LINE_OUT) {
			if (conn == AC_JACK_PORT_FIXED)
				dev = AC_JACK_SPEAKER;
		}

		switch (dev) {
		case AC_JACK_LINE_OUT:
			seq = get_defcfg_sequence(def_conf);
			assoc = get_defcfg_association(def_conf);

			if (!(wid_caps & AC_WCAP_STEREO))
				if (!cfg->mono_out_pin)
					cfg->mono_out_pin = nid;
			if (!assoc)
				continue;
			if (!assoc_line_out)
				assoc_line_out = assoc;
			else if (assoc_line_out != assoc)
				continue;
			if (cfg->line_outs >= ARRAY_SIZE(cfg->line_out_pins))
				continue;
			cfg->line_out_pins[cfg->line_outs] = nid;
			sequences_line_out[cfg->line_outs] = seq;
			cfg->line_outs++;
			break;
		case AC_JACK_SPEAKER:
			seq = get_defcfg_sequence(def_conf);
			assoc = get_defcfg_association(def_conf);
			if (cfg->speaker_outs >= ARRAY_SIZE(cfg->speaker_pins))
				continue;
			cfg->speaker_pins[cfg->speaker_outs] = nid;
			sequences_speaker[cfg->speaker_outs] = (assoc << 4) | seq;
			cfg->speaker_outs++;
			break;
		case AC_JACK_HP_OUT:
			seq = get_defcfg_sequence(def_conf);
			assoc = get_defcfg_association(def_conf);
			if (cfg->hp_outs >= ARRAY_SIZE(cfg->hp_pins))
				continue;
			cfg->hp_pins[cfg->hp_outs] = nid;
			sequences_hp[cfg->hp_outs] = (assoc << 4) | seq;
			cfg->hp_outs++;
			break;
		case AC_JACK_MIC_IN:
			add_auto_cfg_input_pin(cfg, nid, AUTO_PIN_MIC);
			break;
		case AC_JACK_LINE_IN:
			add_auto_cfg_input_pin(cfg, nid, AUTO_PIN_LINE_IN);
			break;
		case AC_JACK_CD:
			add_auto_cfg_input_pin(cfg, nid, AUTO_PIN_CD);
			break;
		case AC_JACK_AUX:
			add_auto_cfg_input_pin(cfg, nid, AUTO_PIN_AUX);
			break;
		case AC_JACK_SPDIF_OUT:
		case AC_JACK_DIG_OTHER_OUT:
			if (cfg->dig_outs >= ARRAY_SIZE(cfg->dig_out_pins))
				continue;
			cfg->dig_out_pins[cfg->dig_outs] = nid;
			cfg->dig_out_type[cfg->dig_outs] =
				(loc == AC_JACK_LOC_HDMI) ?
				HDA_PCM_TYPE_HDMI : HDA_PCM_TYPE_SPDIF;
			cfg->dig_outs++;
			break;
		case AC_JACK_SPDIF_IN:
		case AC_JACK_DIG_OTHER_IN:
			cfg->dig_in_pin = nid;
			if (loc == AC_JACK_LOC_HDMI)
				cfg->dig_in_type = HDA_PCM_TYPE_HDMI;
			else
				cfg->dig_in_type = HDA_PCM_TYPE_SPDIF;
			break;
		}
	}

	if (!cfg->line_outs && cfg->hp_outs > 1 &&
	    !(cond_flags & HDA_PINCFG_NO_HP_FIXUP)) {
		int i = 0;
		while (i < cfg->hp_outs) {
			
			if ((sequences_hp[i] & 0x0f) == 0x0f) {
				i++;
				continue;
			}
			
			cfg->line_out_pins[cfg->line_outs] = cfg->hp_pins[i];
			sequences_line_out[cfg->line_outs] = sequences_hp[i];
			cfg->line_outs++;
			cfg->hp_outs--;
			memmove(cfg->hp_pins + i, cfg->hp_pins + i + 1,
				sizeof(cfg->hp_pins[0]) * (cfg->hp_outs - i));
			memmove(sequences_hp + i, sequences_hp + i + 1,
				sizeof(sequences_hp[0]) * (cfg->hp_outs - i));
		}
		memset(cfg->hp_pins + cfg->hp_outs, 0,
		       sizeof(hda_nid_t) * (AUTO_CFG_MAX_OUTS - cfg->hp_outs));
		if (!cfg->hp_outs)
			cfg->line_out_type = AUTO_PIN_HP_OUT;

	}

	
	sort_pins_by_sequence(cfg->line_out_pins, sequences_line_out,
			      cfg->line_outs);
	sort_pins_by_sequence(cfg->speaker_pins, sequences_speaker,
			      cfg->speaker_outs);
	sort_pins_by_sequence(cfg->hp_pins, sequences_hp,
			      cfg->hp_outs);

	if (!cfg->line_outs &&
	    !(cond_flags & HDA_PINCFG_NO_LO_FIXUP)) {
		if (cfg->speaker_outs) {
			cfg->line_outs = cfg->speaker_outs;
			memcpy(cfg->line_out_pins, cfg->speaker_pins,
			       sizeof(cfg->speaker_pins));
			cfg->speaker_outs = 0;
			memset(cfg->speaker_pins, 0, sizeof(cfg->speaker_pins));
			cfg->line_out_type = AUTO_PIN_SPEAKER_OUT;
		} else if (cfg->hp_outs) {
			cfg->line_outs = cfg->hp_outs;
			memcpy(cfg->line_out_pins, cfg->hp_pins,
			       sizeof(cfg->hp_pins));
			cfg->hp_outs = 0;
			memset(cfg->hp_pins, 0, sizeof(cfg->hp_pins));
			cfg->line_out_type = AUTO_PIN_HP_OUT;
		}
	}

	reorder_outputs(cfg->line_outs, cfg->line_out_pins);
	reorder_outputs(cfg->hp_outs, cfg->hp_pins);
	reorder_outputs(cfg->speaker_outs, cfg->speaker_pins);

	sort_autocfg_input_pins(cfg);

	snd_printd("autoconfig: line_outs=%d (0x%x/0x%x/0x%x/0x%x/0x%x) type:%s\n",
		   cfg->line_outs, cfg->line_out_pins[0], cfg->line_out_pins[1],
		   cfg->line_out_pins[2], cfg->line_out_pins[3],
		   cfg->line_out_pins[4],
		   cfg->line_out_type == AUTO_PIN_HP_OUT ? "hp" :
		   (cfg->line_out_type == AUTO_PIN_SPEAKER_OUT ?
		    "speaker" : "line"));
	snd_printd("   speaker_outs=%d (0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		   cfg->speaker_outs, cfg->speaker_pins[0],
		   cfg->speaker_pins[1], cfg->speaker_pins[2],
		   cfg->speaker_pins[3], cfg->speaker_pins[4]);
	snd_printd("   hp_outs=%d (0x%x/0x%x/0x%x/0x%x/0x%x)\n",
		   cfg->hp_outs, cfg->hp_pins[0],
		   cfg->hp_pins[1], cfg->hp_pins[2],
		   cfg->hp_pins[3], cfg->hp_pins[4]);
	snd_printd("   mono: mono_out=0x%x\n", cfg->mono_out_pin);
	if (cfg->dig_outs)
		snd_printd("   dig-out=0x%x/0x%x\n",
			   cfg->dig_out_pins[0], cfg->dig_out_pins[1]);
	snd_printd("   inputs:");
	for (i = 0; i < cfg->num_inputs; i++) {
		snd_printd(" %s=0x%x",
			    hda_get_autocfg_input_label(codec, cfg, i),
			    cfg->inputs[i].pin);
	}
	snd_printd("\n");
	if (cfg->dig_in_pin)
		snd_printd("   dig-in=0x%x\n", cfg->dig_in_pin);

	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_parse_pin_defcfg);

int snd_hda_get_input_pin_attr(unsigned int def_conf)
{
	unsigned int loc = get_defcfg_location(def_conf);
	unsigned int conn = get_defcfg_connect(def_conf);
	if (conn == AC_JACK_PORT_NONE)
		return INPUT_PIN_ATTR_UNUSED;
	
	if (conn == AC_JACK_PORT_FIXED || conn == AC_JACK_PORT_BOTH)
		return INPUT_PIN_ATTR_INT;
	if ((loc & 0x30) == AC_JACK_LOC_INTERNAL)
		return INPUT_PIN_ATTR_INT;
	if ((loc & 0x30) == AC_JACK_LOC_SEPARATE)
		return INPUT_PIN_ATTR_DOCK;
	if (loc == AC_JACK_LOC_REAR)
		return INPUT_PIN_ATTR_REAR;
	if (loc == AC_JACK_LOC_FRONT)
		return INPUT_PIN_ATTR_FRONT;
	return INPUT_PIN_ATTR_NORMAL;
}
EXPORT_SYMBOL_HDA(snd_hda_get_input_pin_attr);


static const char *hda_get_input_pin_label(struct hda_codec *codec,
					   hda_nid_t pin, bool check_location)
{
	unsigned int def_conf;
	static const char * const mic_names[] = {
		"Internal Mic", "Dock Mic", "Mic", "Front Mic", "Rear Mic",
	};
	int attr;

	def_conf = snd_hda_codec_get_pincfg(codec, pin);

	switch (get_defcfg_device(def_conf)) {
	case AC_JACK_MIC_IN:
		if (!check_location)
			return "Mic";
		attr = snd_hda_get_input_pin_attr(def_conf);
		if (!attr)
			return "None";
		return mic_names[attr - 1];
	case AC_JACK_LINE_IN:
		if (!check_location)
			return "Line";
		attr = snd_hda_get_input_pin_attr(def_conf);
		if (!attr)
			return "None";
		if (attr == INPUT_PIN_ATTR_DOCK)
			return "Dock Line";
		return "Line";
	case AC_JACK_AUX:
		return "Aux";
	case AC_JACK_CD:
		return "CD";
	case AC_JACK_SPDIF_IN:
		return "SPDIF In";
	case AC_JACK_DIG_OTHER_IN:
		return "Digital In";
	default:
		return "Misc";
	}
}

static int check_mic_location_need(struct hda_codec *codec,
				   const struct auto_pin_cfg *cfg,
				   int input)
{
	unsigned int defc;
	int i, attr, attr2;

	defc = snd_hda_codec_get_pincfg(codec, cfg->inputs[input].pin);
	attr = snd_hda_get_input_pin_attr(defc);
	
	if (attr <= INPUT_PIN_ATTR_NORMAL)
		return 1;

	attr = 0;
	for (i = 0; i < cfg->num_inputs; i++) {
		defc = snd_hda_codec_get_pincfg(codec, cfg->inputs[i].pin);
		attr2 = snd_hda_get_input_pin_attr(defc);
		if (attr2 >= INPUT_PIN_ATTR_NORMAL) {
			if (attr && attr != attr2)
				return 1; 
			attr = attr2;
		}
	}
	return 0;
}

const char *hda_get_autocfg_input_label(struct hda_codec *codec,
					const struct auto_pin_cfg *cfg,
					int input)
{
	int type = cfg->inputs[input].type;
	int has_multiple_pins = 0;

	if ((input > 0 && cfg->inputs[input - 1].type == type) ||
	    (input < cfg->num_inputs - 1 && cfg->inputs[input + 1].type == type))
		has_multiple_pins = 1;
	if (has_multiple_pins && type == AUTO_PIN_MIC)
		has_multiple_pins &= check_mic_location_need(codec, cfg, input);
	return hda_get_input_pin_label(codec, cfg->inputs[input].pin,
				       has_multiple_pins);
}
EXPORT_SYMBOL_HDA(hda_get_autocfg_input_label);

static int find_idx_in_nid_list(hda_nid_t nid, const hda_nid_t *list, int nums)
{
	int i;
	for (i = 0; i < nums; i++)
		if (list[i] == nid)
			return i;
	return -1;
}

static const char *check_output_sfx(hda_nid_t nid, const hda_nid_t *pins,
				    int num_pins, int *indexp)
{
	static const char * const channel_sfx[] = {
		" Front", " Surround", " CLFE", " Side"
	};
	int i;

	i = find_idx_in_nid_list(nid, pins, num_pins);
	if (i < 0)
		return NULL;
	if (num_pins == 1)
		return "";
	if (num_pins > ARRAY_SIZE(channel_sfx)) {
		if (indexp)
			*indexp = i;
		return "";
	}
	return channel_sfx[i];
}

static int fill_audio_out_name(struct hda_codec *codec, hda_nid_t nid,
			       const struct auto_pin_cfg *cfg,
			       const char *name, char *label, int maxlen,
			       int *indexp)
{
	unsigned int def_conf = snd_hda_codec_get_pincfg(codec, nid);
	int attr = snd_hda_get_input_pin_attr(def_conf);
	const char *pfx = "", *sfx = "";

	
	if (!strcmp(name, "Line Out") && attr == INPUT_PIN_ATTR_INT)
		name = "Speaker";
	
	switch (attr) {
	case INPUT_PIN_ATTR_DOCK:
		pfx = "Dock ";
		break;
	case INPUT_PIN_ATTR_FRONT:
		pfx = "Front ";
		break;
	}
	if (cfg) {
		
		sfx = check_output_sfx(nid, cfg->line_out_pins, cfg->line_outs,
				       indexp);
		if (!sfx)
			sfx = check_output_sfx(nid, cfg->speaker_pins, cfg->speaker_outs,
					       indexp);
		if (!sfx) {
			
			int idx = find_idx_in_nid_list(nid, cfg->hp_pins,
						       cfg->hp_outs);
			if (idx >= 0)
				*indexp = idx;
			sfx = "";
		}
	}
	snprintf(label, maxlen, "%s%s%s", pfx, name, sfx);
	return 1;
}

int snd_hda_get_pin_label(struct hda_codec *codec, hda_nid_t nid,
			  const struct auto_pin_cfg *cfg,
			  char *label, int maxlen, int *indexp)
{
	unsigned int def_conf = snd_hda_codec_get_pincfg(codec, nid);
	const char *name = NULL;
	int i;

	if (indexp)
		*indexp = 0;
	if (get_defcfg_connect(def_conf) == AC_JACK_PORT_NONE)
		return 0;

	switch (get_defcfg_device(def_conf)) {
	case AC_JACK_LINE_OUT:
		return fill_audio_out_name(codec, nid, cfg, "Line Out",
					   label, maxlen, indexp);
	case AC_JACK_SPEAKER:
		return fill_audio_out_name(codec, nid, cfg, "Speaker",
					   label, maxlen, indexp);
	case AC_JACK_HP_OUT:
		return fill_audio_out_name(codec, nid, cfg, "Headphone",
					   label, maxlen, indexp);
	case AC_JACK_SPDIF_OUT:
	case AC_JACK_DIG_OTHER_OUT:
		if (get_defcfg_location(def_conf) == AC_JACK_LOC_HDMI)
			name = "HDMI";
		else
			name = "SPDIF";
		if (cfg && indexp) {
			i = find_idx_in_nid_list(nid, cfg->dig_out_pins,
						 cfg->dig_outs);
			if (i >= 0)
				*indexp = i;
		}
		break;
	default:
		if (cfg) {
			for (i = 0; i < cfg->num_inputs; i++) {
				if (cfg->inputs[i].pin != nid)
					continue;
				name = hda_get_autocfg_input_label(codec, cfg, i);
				if (name)
					break;
			}
		}
		if (!name)
			name = hda_get_input_pin_label(codec, nid, true);
		break;
	}
	if (!name)
		return 0;
	strlcpy(label, name, maxlen);
	return 1;
}
EXPORT_SYMBOL_HDA(snd_hda_get_pin_label);

int snd_hda_add_imux_item(struct hda_input_mux *imux, const char *label,
			  int index, int *type_idx)
{
	int i, label_idx = 0;
	if (imux->num_items >= HDA_MAX_NUM_INPUTS) {
		snd_printd(KERN_ERR "hda_codec: Too many imux items!\n");
		return -EINVAL;
	}
	for (i = 0; i < imux->num_items; i++) {
		if (!strncmp(label, imux->items[i].label, strlen(label)))
			label_idx++;
	}
	if (type_idx)
		*type_idx = label_idx;
	if (label_idx > 0)
		snprintf(imux->items[imux->num_items].label,
			 sizeof(imux->items[imux->num_items].label),
			 "%s %d", label, label_idx);
	else
		strlcpy(imux->items[imux->num_items].label, label,
			sizeof(imux->items[imux->num_items].label));
	imux->items[imux->num_items].index = index;
	imux->num_items++;
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_add_imux_item);


#ifdef CONFIG_PM

int snd_hda_suspend(struct hda_bus *bus)
{
	struct hda_codec *codec;

	list_for_each_entry(codec, &bus->codec_list, list) {
		if (hda_codec_is_power_on(codec))
			hda_call_codec_suspend(codec);
		if (codec->patch_ops.post_suspend)
			codec->patch_ops.post_suspend(codec);
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_suspend);

int snd_hda_resume(struct hda_bus *bus)
{
	struct hda_codec *codec;

	list_for_each_entry(codec, &bus->codec_list, list) {
		if (codec->patch_ops.pre_resume)
			codec->patch_ops.pre_resume(codec);
		if (snd_hda_codec_needs_resume(codec))
			hda_call_codec_resume(codec);
	}
	return 0;
}
EXPORT_SYMBOL_HDA(snd_hda_resume);
#endif 


void *snd_array_new(struct snd_array *array)
{
	if (array->used >= array->alloced) {
		int num = array->alloced + array->alloc_align;
		int size = (num + 1) * array->elem_size;
		int oldsize = array->alloced * array->elem_size;
		void *nlist;
		if (snd_BUG_ON(num >= 4096))
			return NULL;
		nlist = krealloc(array->list, size, GFP_KERNEL);
		if (!nlist)
			return NULL;
		memset(nlist + oldsize, 0, size - oldsize);
		array->list = nlist;
		array->alloced = num;
	}
	return snd_array_elem(array, array->used++);
}
EXPORT_SYMBOL_HDA(snd_array_new);

void snd_array_free(struct snd_array *array)
{
	kfree(array->list);
	array->used = 0;
	array->alloced = 0;
	array->list = NULL;
}
EXPORT_SYMBOL_HDA(snd_array_free);

void snd_print_pcm_bits(int pcm, char *buf, int buflen)
{
	static unsigned int bits[] = { 8, 16, 20, 24, 32 };
	int i, j;

	for (i = 0, j = 0; i < ARRAY_SIZE(bits); i++)
		if (pcm & (AC_SUPPCM_BITS_8 << i))
			j += snprintf(buf + j, buflen - j,  " %d", bits[i]);

	buf[j] = '\0'; 
}
EXPORT_SYMBOL_HDA(snd_print_pcm_bits);

MODULE_DESCRIPTION("HDA codec core");
MODULE_LICENSE("GPL");
