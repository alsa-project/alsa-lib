/****************************************************************************
 *                                                                          *
 *                               mixer.h                                    *
 *                           Mixer Interface                                *
 *                                                                          *
 ****************************************************************************/

typedef struct snd_mixer_callbacks {
	void *private_data;	/* should be used by an application */
	void (*rebuild) (void *private_data);
	void (*element) (void *private_data, int cmd, snd_mixer_eid_t *eid);
	void (*group) (void *private_data, int cmd, snd_mixer_gid_t *gid);
	void *reserved[28];	/* reserved for the future use - must be NULL!!! */
} snd_mixer_callbacks_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_mixer snd_mixer_t;

int snd_mixer_open(snd_mixer_t **handle, int card, int device);
int snd_mixer_close(snd_mixer_t *handle);
int snd_mixer_file_descriptor(snd_mixer_t *handle);
int snd_mixer_info(snd_mixer_t *handle, snd_mixer_info_t * info);
int snd_mixer_elements(snd_mixer_t *handle, snd_mixer_elements_t * elements);
int snd_mixer_routes(snd_mixer_t *handle, snd_mixer_routes_t * routes);
int snd_mixer_groups(snd_mixer_t *handle, snd_mixer_groups_t * groups);
int snd_mixer_group_read(snd_mixer_t *handle, snd_mixer_group_t * group);
int snd_mixer_group_write(snd_mixer_t *handle, snd_mixer_group_t * group);
int snd_mixer_element_info(snd_mixer_t *handle, snd_mixer_element_info_t * info);
int snd_mixer_element_read(snd_mixer_t *handle, snd_mixer_element_t * element);
int snd_mixer_element_write(snd_mixer_t *handle, snd_mixer_element_t * element);
int snd_mixer_read_filter(snd_mixer_t *handle, snd_mixer_filter_t * filter);
int snd_mixer_read(snd_mixer_t *handle, snd_mixer_callbacks_t * callbacks);

void snd_mixer_set_bit(unsigned int *bitmap, int bit, int val);
int snd_mixer_get_bit(unsigned int *bitmap, int bit);

const char *snd_mixer_channel_name(int channel);

int snd_mixer_element_has_info(snd_mixer_eid_t *eid);
int snd_mixer_element_info_build(snd_mixer_t *handle, snd_mixer_element_info_t * info);
int snd_mixer_element_info_free(snd_mixer_element_info_t * info);
int snd_mixer_element_has_control(snd_mixer_eid_t *eid);
int snd_mixer_element_build(snd_mixer_t *handle, snd_mixer_element_t * element);
int snd_mixer_element_free(snd_mixer_element_t * element);

#ifdef __cplusplus
}
#endif

