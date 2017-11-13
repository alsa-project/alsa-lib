/*
 * user-control-element-set.c - a program to test in-kernel implementation of
 *				user-defined control element set.
 *
 * Copyright (c) 2015-2016 Takashi Sakamoto
 *
 * Licensed under the terms of the GNU General Public License, version 2.
 */

#include "../include/asoundlib.h"
#include <sound/tlv.h>
#include <stdbool.h>

struct elem_set_trial {
	snd_ctl_t *handle;

	snd_ctl_elem_type_t type;
	unsigned int member_count;
	unsigned int element_count;

	snd_ctl_elem_id_t *id;

	int (*add_elem_set)(struct elem_set_trial *trial,
			    snd_ctl_elem_info_t *info);
	int (*check_elem_props)(struct elem_set_trial *trial,
				snd_ctl_elem_info_t *info);
	void (*change_elem_members)(struct elem_set_trial *trial,
				    snd_ctl_elem_value_t *elem_data);
	int (*allocate_elem_set_tlv)(struct elem_set_trial *trial,
				     unsigned int **tlv);

	bool tlv_readable;
};

struct chmap_entry {
	unsigned int type;
	unsigned int length;
	unsigned int maps[0];
};

/*
 * History of TLV feature:
 *
 * 2016/09/15: 398fa4db6c69 ("ALSA: control: move layout of TLV payload to UAPI
 *			      header")
 * 2012/07/21: 2d3391ec0ecc ("ALSA: PCM: channel mapping API implementation")
 * 2011/11/20: bf1d1c9b6179 ("ALSA: tlv: add DECLARE_TLV_DB_RANGE()")
 * 2009/07/16: 085f30654175 ("ALSA: Add new TLV types for dBwith min/max")
 * 2006/09/06: 55a29af5ed5d ("[ALSA] Add definition of TLV dB range compound")
 * 2006/08/28: 063a40d9111c ("Add the definition of linear volume TLV")
 * 2006/08/28: 42750b04c5ba ("[ALSA] Control API - TLV implementation for
 *			      additional information like dB scale")
 */

/* Operations for elements in an element set with boolean type. */
static int add_bool_elem_set(struct elem_set_trial *trial,
			     snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_boolean_elem_set(trial->handle, info,
				trial->element_count, trial->member_count);
}

static void change_bool_elem_members(struct elem_set_trial *trial,
				     snd_ctl_elem_value_t *elem_data)
{
	int val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_boolean(elem_data, i);
		snd_ctl_elem_value_set_boolean(elem_data, i, !val);
	}
}

static int allocate_bool_elem_set_tlv(struct elem_set_trial *trial,
				      unsigned int **tlv)
{
	/*
	 * Performs like a toggle switch for attenuation, because they're bool
	 * elements.
	 */
	static const SNDRV_CTL_TLVD_DECLARE_DB_MINMAX(range, -10000, 0);

	*tlv = malloc(sizeof(range));
	if (*tlv == NULL)
		return -ENOMEM;
	memcpy(*tlv, range, sizeof(range));

	return 0;
}

/* Operations for elements in an element set with integer type. */
static int add_int_elem_set(struct elem_set_trial *trial,
			    snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_integer_elem_set(trial->handle, info,
				trial->element_count, trial->member_count,
				0, 25, 1);
}

static int check_int_elem_props(struct elem_set_trial *trial,
				snd_ctl_elem_info_t *info)
{
	if (snd_ctl_elem_info_get_min(info) != 0)
		return -EIO;
	if (snd_ctl_elem_info_get_max(info) != 25)
		return -EIO;
	if (snd_ctl_elem_info_get_step(info) != 1)
		return -EIO;

	return 0;
}

static void change_int_elem_members(struct elem_set_trial *trial,
				    snd_ctl_elem_value_t *elem_data)
{
	long val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_integer(elem_data, i);
		snd_ctl_elem_value_set_integer(elem_data, i, ++val);
	}
}

static int allocate_int_elem_set_tlv(struct elem_set_trial *trial,
				     unsigned int **tlv)
{
	unsigned int len, pos;
	unsigned int i, j;
	struct chmap_entry *entry;

	/* Calculate size of TLV packet for channel-mapping information. */
	len = 0;
	for (i = 1; i <= 25; ++i) {
		len += sizeof(struct chmap_entry);
		len += i * sizeof(unsigned int);
	}

	*tlv = malloc(len);
	if (*tlv == NULL)
		return -ENOMEM;

	/*
	 * Emulate channel-mapping information in in-kernel implementation.
	 * Here, 25 entries are for each different channel.
	 */
	pos = 0;
	for (i = 1; i <= 25 && pos < len; ++i) {
		entry = (struct chmap_entry *)&(*tlv)[pos];
		entry->type = SNDRV_CTL_TLVT_CHMAP_FIXED;
		entry->length = i * sizeof(unsigned int);
		for (j = 0; j < i; ++j)
			entry->maps[j] = SND_CHMAP_MONO + j;
		pos += sizeof(struct chmap_entry) + i * sizeof(unsigned int);
	}

	return 0;
}

/* Operations for elements in an element set with enumerated type. */
static const char *const labels[] = {
	"trusty",
	"utopic",
	"vivid",
	"willy",
	"xenial",
};

static int add_enum_elem_set(struct elem_set_trial *trial,
			     snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_enumerated_elem_set(trial->handle, info,
				trial->element_count, trial->member_count,
				sizeof(labels) / sizeof(labels[0]),
				labels);
}

static int check_enum_elem_props(struct elem_set_trial *trial,
				 snd_ctl_elem_info_t *info)
{
	unsigned int items;
	unsigned int i;
	const char *label;
	int err;

	items = snd_ctl_elem_info_get_items(info);
	if (items != sizeof(labels) / sizeof(labels[0]))
		return -EIO;

	/* Enumerate and validate all of labels registered to this element. */
	for (i = 0; i < items; ++i) {
		snd_ctl_elem_info_set_item(info, i);
		err = snd_ctl_elem_info(trial->handle, info);
		if (err < 0)
			return err;

		label = snd_ctl_elem_info_get_item_name(info);
		if (strncmp(label, labels[i], strlen(labels[i])) != 0)
			return -EIO;
	}

	return 0;
}

static void change_enum_elem_members(struct elem_set_trial *trial,
				     snd_ctl_elem_value_t *elem_data)
{
	unsigned int val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_enumerated(elem_data, i);
		snd_ctl_elem_value_set_enumerated(elem_data, i, ++val);
	}
}

/* Operations for elements in an element set with bytes type. */
static int add_bytes_elem_set(struct elem_set_trial *trial,
			      snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_bytes_elem_set(trial->handle, info,
				trial->element_count, trial->member_count);
}

static void change_bytes_elem_members(struct elem_set_trial *trial,
				      snd_ctl_elem_value_t *elem_data)
{
	unsigned char val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_byte(elem_data, i);
		snd_ctl_elem_value_set_byte(elem_data, i, ++val);
	}
}

static int allocate_bytes_elem_set_tlv(struct elem_set_trial *trial,
				       unsigned int **tlv)
{
	/*
	 * Emulate AK4396.
	 * 20 * log10(x/255) (dB)
	 * Here, x is written value.
	 */
	static const SNDRV_CTL_TLVD_DECLARE_DB_LINEAR(range, -4813, 0);

	*tlv = malloc(sizeof(range));
	if (*tlv == NULL)
		return -ENOMEM;
	memcpy(*tlv, range, sizeof(range));

	return 0;
}

/* Operations for elements in an element set with iec958 type. */
static int add_iec958_elem_set(struct elem_set_trial *trial,
			       snd_ctl_elem_info_t *info)
{
	int err;

	snd_ctl_elem_info_get_id(info, trial->id);

	err = snd_ctl_elem_add_iec958(trial->handle, trial->id);
	if (err < 0)
	        return err;

	/*
	 * In historical reason, the above API is not allowed to fill all of
	 * fields in identification data.
	 */
	return snd_ctl_elem_info(trial->handle, info);
}

static void change_iec958_elem_members(struct elem_set_trial *trial,
				       snd_ctl_elem_value_t *elem_data)
{
	snd_aes_iec958_t data;

	/* To suppress GCC warnings. */
	trial->element_count = 1;

	snd_ctl_elem_value_get_iec958(elem_data, &data);
	/* This is an arbitrary number. */
	data.pad = 10;
	snd_ctl_elem_value_set_iec958(elem_data, &data);
}

/* Operations for elements in an element set with integer64 type. */
static int add_int64_elem_set(struct elem_set_trial *trial,
			      snd_ctl_elem_info_t *info)
{
	return snd_ctl_add_integer64_elem_set(trial->handle, info,
				trial->element_count, trial->member_count,
				0, 10000, 1);
}

static int check_int64_elem_props(struct elem_set_trial *trial,
				  snd_ctl_elem_info_t *info)
{
	if (snd_ctl_elem_info_get_min64(info) != 0)
		return -EIO;
	if (snd_ctl_elem_info_get_max64(info) != 10000)
		return -EIO;
	if (snd_ctl_elem_info_get_step64(info) != 1)
		return -EIO;

	return 0;
}

static void change_int64_elem_members(struct elem_set_trial *trial,
				      snd_ctl_elem_value_t *elem_data)
{
	long long val;
	unsigned int i;

	for (i = 0; i < trial->member_count; ++i) {
		val = snd_ctl_elem_value_get_integer64(elem_data, i);
		snd_ctl_elem_value_set_integer64(elem_data, i, ++val);
	}
}

static int allocate_int64_elem_set_tlv(struct elem_set_trial *trial,
				       unsigned int **tlv)
{
	/*
	 * Use this fomula between linear/dB value:
	 *
	 *  Linear: dB range (coeff)
	 *   0<-> 4: -59.40<->-56.36 (44)
	 *   4<->22: -56.36<->-45.56 (60)
	 *  22<->33: -45.56<->-40.72 (76)
	 *  33<->37: -40.72<->-38.32 (44)
	 *  37<->48: -38.32<->-29.96 (76)
	 *  48<->66: -29.96<->-22.04 (60)
	 *  66<->84: -22.04<-> -8.36 (44)
	 *  84<->95:  -8.36<-> -1.76 (60)
	 *  95<->99:  -1.76<->  0.00 (76)
	 * 100<->..:   0.0
	 */
	static const SNDRV_CTL_TLVD_DECLARE_DB_RANGE(range,
		 0,   4, SNDRV_CTL_TLVD_DB_SCALE_ITEM(-5940, 44, 1),
		 4,  22, SNDRV_CTL_TLVD_DB_SCALE_ITEM(-5636, 60, 0),
		22,  33, SNDRV_CTL_TLVD_DB_SCALE_ITEM(-4556, 76, 0),
		33,  37, SNDRV_CTL_TLVD_DB_SCALE_ITEM(-4072, 44, 0),
		37,  48, SNDRV_CTL_TLVD_DB_SCALE_ITEM(-3832, 76, 0),
		48,  66, SNDRV_CTL_TLVD_DB_SCALE_ITEM(-2996, 60, 0),
		66,  84, SNDRV_CTL_TLVD_DB_SCALE_ITEM(-2204, 44, 0),
		84,  95, SNDRV_CTL_TLVD_DB_SCALE_ITEM( -836, 60, 0),
		95,  99, SNDRV_CTL_TLVD_DB_SCALE_ITEM( -176, 76, 0),
		100, 10000, SNDRV_CTL_TLVD_DB_SCALE_ITEM(0, 0, 0),
	);

	*tlv = malloc(sizeof(range));
	if (*tlv == NULL)
		return -ENOMEM;
	memcpy(*tlv, range, sizeof(range));

	return 0;
}

/* Common operations. */
static int add_elem_set(struct elem_set_trial *trial)
{
	snd_ctl_elem_info_t *info;
	char name[64] = {0};
	int err;

	snprintf(name, 64, "userspace-control-element-%s",
		 snd_ctl_elem_type_name(trial->type));

	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_info_set_interface(info, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_info_set_name(info, name);

	err = trial->add_elem_set(trial, info);
	if (err >= 0)
		snd_ctl_elem_info_get_id(info, trial->id);

	return err;
}

static int check_event(struct elem_set_trial *trial, unsigned int mask,
		       unsigned int expected_count)
{
	struct pollfd pfds;
	int count;
	unsigned short revents;
	snd_ctl_event_t *event;
	int err;

	snd_ctl_event_alloca(&event);

	if (snd_ctl_poll_descriptors_count(trial->handle) != 1)
		return -ENXIO;

	if (snd_ctl_poll_descriptors(trial->handle, &pfds, 1) != 1)
		return -ENXIO;

	while (expected_count > 0) {
		count = poll(&pfds, 1, 1000);
		if (count < 0)
			return errno;
		/* Some events are already supplied. */
		if (count == 0)
			return -ETIMEDOUT;

		err = snd_ctl_poll_descriptors_revents(trial->handle, &pfds,
						       count, &revents);
		if (err < 0)
			return err;
		if (revents & POLLERR)
			return -EIO;
		if (!(revents & POLLIN))
			continue;

		err = snd_ctl_read(trial->handle, event);
		if (err < 0)
			return err;
		if (snd_ctl_event_get_type(event) != SND_CTL_EVENT_ELEM)
			continue;
		/*
		 * I expect each event is generated separately to the same
		 * element or several events are generated at once.
		 */
		if ((snd_ctl_event_elem_get_mask(event) & mask) != mask)
			continue;
		--expected_count;
	}

	if (expected_count != 0)
		return -EIO;

	return 0;
}

static int check_elem_list(struct elem_set_trial *trial)
{
	snd_ctl_elem_list_t *list;
	snd_ctl_elem_id_t *id;
	int e;
	unsigned int i;
	int err;

	snd_ctl_elem_list_alloca(&list);
	snd_ctl_elem_id_alloca(&id);

	err = snd_ctl_elem_list(trial->handle, list);
	if (err < 0)
		return err;

	/* Certainly some elements are already added. */
	if (snd_ctl_elem_list_get_count(list) == 0)
		return -EIO;

	err = snd_ctl_elem_list_alloc_space(list,
					    snd_ctl_elem_list_get_count(list));
	if (err < 0)
		return err;

	err = snd_ctl_elem_list(trial->handle, list);
	if (err < 0)
		goto end;

	if (trial->element_count > snd_ctl_elem_list_get_count(list)) {
		err = -EIO;
		goto end;
	}

	i = 0;
	for (e = 0; e < snd_ctl_elem_list_get_count(list); ++e) {
		snd_ctl_elem_list_get_id(list, e, id);

		if (strcmp(snd_ctl_elem_id_get_name(id),
			   snd_ctl_elem_id_get_name(trial->id)) != 0)
			continue;
		if (snd_ctl_elem_id_get_interface(id) !=
		    snd_ctl_elem_id_get_interface(trial->id))
			continue;
		if (snd_ctl_elem_id_get_device(id) !=
		    snd_ctl_elem_id_get_device(trial->id))
			continue;
		if (snd_ctl_elem_id_get_subdevice(id) !=
		    snd_ctl_elem_id_get_subdevice(trial->id))
			continue;

		/*
		 * Here, I expect the list includes element ID data in numerical
		 * order. Actually, it does.
		 */
		if (snd_ctl_elem_id_get_numid(id) !=
		    snd_ctl_elem_id_get_numid(trial->id) + i)
			continue;
		if (snd_ctl_elem_id_get_index(id) !=
		    snd_ctl_elem_id_get_index(trial->id) + i)
			continue;

		++i;
	}

	if (i != trial->element_count)
		err = -EIO;
end:
	snd_ctl_elem_list_free_space(list);

	return err;
}

static int check_elem_set_props(struct elem_set_trial *trial)
{
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	unsigned int numid;
	unsigned int index;
	unsigned int i;
	unsigned int j;
	int err;

	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_info_alloca(&info);

	snd_ctl_elem_info_set_id(info, trial->id);
	numid = snd_ctl_elem_id_get_numid(trial->id);
	index = snd_ctl_elem_id_get_index(trial->id);

	for (i = 0; i < trial->element_count; ++i) {
		snd_ctl_elem_info_set_index(info, index + i);

		/*
		 * In Linux 4.0 or former, ioctl(SNDRV_CTL_IOCTL_ELEM_ADD)
		 * doesn't fill all of fields for identification.
		 */
		if (numid > 0)
			snd_ctl_elem_info_set_numid(info, numid + i);

		err = snd_ctl_elem_info(trial->handle, info);
		if (err < 0)
			return err;

		/* Check some common properties. */
		if (snd_ctl_elem_info_get_type(info) != trial->type)
			return -EIO;
		if (snd_ctl_elem_info_get_count(info) != trial->member_count)
			return -EIO;

		/*
		 * In a case of IPC, this is the others. But in this case,
		 * it's myself.
		 */
		if (snd_ctl_elem_info_get_owner(info) != getpid())
			return -EIO;

		/*
		 * Just adding an element set by userspace applications,
		 * included elements are initially locked.
		 */
		if (!snd_ctl_elem_info_is_locked(info))
			return -EIO;

		/*
		 * In initial state, any application can register TLV data for
		 * user-defined element set except for IEC 958 type, thus
		 * elements in any user-defined set should allow any write
		 * operation.
		 */
		if (trial->type != SND_CTL_ELEM_TYPE_IEC958 &&
		    !snd_ctl_elem_info_is_tlv_writable(info))
			return -EIO;

		/* Check type-specific properties. */
		if (trial->check_elem_props != NULL) {
			err = trial->check_elem_props(trial, info);
			if (err < 0)
				return err;
		}

		snd_ctl_elem_info_get_id(info, id);
		err = snd_ctl_elem_unlock(trial->handle, id);
		if (err < 0)
			return err;

		/*
		 * Till kernel v4.14, ALSA control core allows elements in any
		 * user-defined set to have TLV_READ flag even if they have no
		 * TLV data in their initial state. In this case, any read
		 * operation for TLV data should return -ENXIO.
		 */
		if (snd_ctl_elem_info_is_tlv_readable(info)) {
			unsigned int data[32];
			err = snd_ctl_elem_tlv_read(trial->handle, trial->id,
						    data, sizeof(data));
			if (err >= 0)
				return -EIO;
			if (err != -ENXIO)
				return err;

			trial->tlv_readable = true;
		}

	}

	return 0;
}

static int check_elems(struct elem_set_trial *trial)
{
	snd_ctl_elem_value_t *data;
	unsigned int numid;
	unsigned int index;
	unsigned int i;
	int err;

	snd_ctl_elem_value_alloca(&data);

	snd_ctl_elem_value_set_id(data, trial->id);
	numid = snd_ctl_elem_id_get_numid(trial->id);
	index = snd_ctl_elem_id_get_index(trial->id);

	for (i = 0; i < trial->element_count; ++i) {
		snd_ctl_elem_value_set_index(data, index + i);

		/*
		 * In Linux 4.0 or former, ioctl(SNDRV_CTL_IOCTL_ELEM_ADD)
		 * doesn't fill all of fields for identification.
		 */
		if (numid > 0)
			snd_ctl_elem_value_set_numid(data, numid + i);

		err = snd_ctl_elem_read(trial->handle, data);
		if (err < 0)
			return err;

		/* Change members of an element in this element set. */
		trial->change_elem_members(trial, data);

		err = snd_ctl_elem_write(trial->handle, data);
		if (err < 0)
			return err;
	}

	return 0;
}

static int check_tlv(struct elem_set_trial *trial)
{
	unsigned int *tlv;
	int mask;
	unsigned int count;
	unsigned int len;
	unsigned int *curr;
	int err;

	err = trial->allocate_elem_set_tlv(trial, &tlv);
	if (err < 0)
		return err;

	len = tlv[1] + sizeof(unsigned int) * 2;
	curr = malloc(len);
	if (curr == NULL) {
		free(tlv);
		return -ENOMEM;
	}

	/*
	 * In in-kernel implementation, write and command operations are the
	 * same for an element set added by userspace applications. Here, I
	 * use write.
	 */
	err = snd_ctl_elem_tlv_write(trial->handle, trial->id,
				     (const unsigned int *)tlv);
	if (err < 0)
		goto end;

	/*
	 * Since kernel v4.14, any write operation to an element in user-defined
	 * set can change state of the other elements in the same set. In this
	 * case, any TLV data is firstly available after the operation.
	 */
	if (!trial->tlv_readable) {
		mask = SND_CTL_EVENT_MASK_INFO | SND_CTL_EVENT_MASK_TLV;
		count = trial->element_count;
	} else {
		mask = SND_CTL_EVENT_MASK_TLV;
		count = 1;
	}
	err = check_event(trial, mask, count);
	if (err < 0)
		goto end;
	if (!trial->tlv_readable) {
		snd_ctl_elem_info_t *info;
		snd_ctl_elem_info_alloca(&info);

		snd_ctl_elem_info_set_id(info, trial->id);
		err = snd_ctl_elem_info(trial->handle, info);
		if (err < 0)
			return err;
		if (!snd_ctl_elem_info_is_tlv_readable(info))
			return -EIO;

		/* Now TLV data is available for this element set. */
		trial->tlv_readable = true;
	}

	err = snd_ctl_elem_tlv_read(trial->handle, trial->id, curr, len);
	if (err < 0)
		goto end;

	if (memcmp(curr, tlv, len) != 0)
		err = -EIO;
end:
	free(tlv);
	free(curr);
	return 0;
}

int main(void)
{
	struct elem_set_trial trial = {0};
	unsigned int i;
	int err;

	snd_ctl_elem_id_alloca(&trial.id);

	err = snd_ctl_open(&trial.handle, "hw:0", 0);
	if (err < 0)
		return EXIT_FAILURE;

	err = snd_ctl_subscribe_events(trial.handle, 1);
	if (err < 0)
		return EXIT_FAILURE;

	/* Test all of types. */
	for (i = 0; i < SND_CTL_ELEM_TYPE_LAST; ++i) {
		trial.type = i + 1;

		/* Assign type-dependent operations. */
		switch (trial.type) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			trial.element_count = 900;
			trial.member_count = 128;
			trial.add_elem_set = add_bool_elem_set;
			trial.check_elem_props = NULL;
			trial.change_elem_members = change_bool_elem_members;
			trial.allocate_elem_set_tlv =
						allocate_bool_elem_set_tlv;
			trial.tlv_readable = false;
			break;
		case SND_CTL_ELEM_TYPE_INTEGER:
			trial.element_count = 900;
			trial.member_count = 128;
			trial.add_elem_set = add_int_elem_set;
			trial.check_elem_props = check_int_elem_props;
			trial.change_elem_members = change_int_elem_members;
			trial.allocate_elem_set_tlv =
						allocate_int_elem_set_tlv;
			trial.tlv_readable = false;
			break;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
			trial.element_count = 900;
			trial.member_count = 128;
			trial.add_elem_set = add_enum_elem_set;
			trial.check_elem_props = check_enum_elem_props;
			trial.change_elem_members = change_enum_elem_members;
			trial.allocate_elem_set_tlv = NULL;
			trial.tlv_readable = false;
			break;
		case SND_CTL_ELEM_TYPE_BYTES:
			trial.element_count = 900;
			trial.member_count = 512;
			trial.add_elem_set = add_bytes_elem_set;
			trial.check_elem_props = NULL;
			trial.change_elem_members = change_bytes_elem_members;
			trial.allocate_elem_set_tlv =
						allocate_bytes_elem_set_tlv;
			trial.tlv_readable = false;
			break;
		case SND_CTL_ELEM_TYPE_IEC958:
			trial.element_count = 1;
			trial.member_count = 1;
			trial.add_elem_set = add_iec958_elem_set;
			trial.check_elem_props = NULL;
			trial.change_elem_members = change_iec958_elem_members;
			trial.allocate_elem_set_tlv = NULL;
			trial.tlv_readable = false;
			break;
		case SND_CTL_ELEM_TYPE_INTEGER64:
		default:
			trial.element_count = 900;
			trial.member_count = 64;
			trial.add_elem_set = add_int64_elem_set;
			trial.check_elem_props = check_int64_elem_props;
			trial.change_elem_members = change_int64_elem_members;
			trial.allocate_elem_set_tlv =
						allocate_int64_elem_set_tlv;
			trial.tlv_readable = false;
			break;
		}

		/* Test an operation to add an element set. */
		err = add_elem_set(&trial);
		if (err < 0) {
			printf("Fail to add an element set with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}
		err = check_event(&trial, SND_CTL_EVENT_MASK_ADD,
				  trial.element_count);
		if (err < 0) {
			printf("Fail to check some events to add elements with "
			       "%s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}

		/* Check added elements are retrieved in a list. */
		err = check_elem_list(&trial);
		if (err < 0) {
			printf("Fail to list each element with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}

		/* Check properties of each element in this element set. */
		err = check_elem_set_props(&trial);
		if (err < 0) {
			printf("Fail to check properties of each element with "
			       "%s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}

		/*
		 * Test operations to change the state of members in each
		 * element in the element set.
		 */
		err = check_elems(&trial);
		if (err < 0) {
			printf("Fail to change status of each element with %s "
			       "type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}
		err = check_event(&trial, SND_CTL_EVENT_MASK_VALUE,
				  trial.element_count);
		if (err < 0) {
			printf("Fail to check some events to change status of "
			       "each elements with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}

		/*
		 * Test an operation to change TLV data of this element set,
		 * except for enumerated and IEC958 type.
		 */
		if (trial.allocate_elem_set_tlv != NULL) {
			err = check_tlv(&trial);
			if (err < 0) {
				printf("Fail to change TLV data of an element "
				       "set with %s type.\n",
				       snd_ctl_elem_type_name(trial.type));
				break;
			}
		}

		/* Test an operation to remove elements in this element set. */
		err = snd_ctl_elem_remove(trial.handle, trial.id);
		if (err < 0) {
			printf("Fail to remove elements with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}
		err = check_event(&trial, SND_CTL_EVENT_MASK_REMOVE,
						  trial.element_count);
		if (err < 0) {
			printf("Fail to check some events to remove each "
			       "element with %s type.\n",
			       snd_ctl_elem_type_name(trial.type));
			break;
		}
	}

	if (err < 0) {
		printf("%s\n", snd_strerror(err));

		/* To ensure. */
		snd_ctl_elem_remove(trial.handle, trial.id);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
