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

int snd_mixer_open(void **handle, int card, int device);
int snd_mixer_close(void *handle);
int snd_mixer_file_descriptor(void *handle);
int snd_mixer_info(void *handle, snd_mixer_info_t * info);
int snd_mixer_elements(void *handle, snd_mixer_elements_t * elements);
int snd_mixer_routes(void *handle, snd_mixer_routes_t * routes);
int snd_mixer_groups(void *handle, snd_mixer_groups_t * groups);
int snd_mixer_group(void *handle, snd_mixer_group_t * group);
int snd_mixer_element_info(void *handle, snd_mixer_element_info_t * info);
int snd_mixer_element_read(void *handle, snd_mixer_element_t * element);
int snd_mixer_element_write(void *handle, snd_mixer_element_t * element);
int snd_mixer_read(void *handle, snd_mixer_callbacks_t * callbacks);

void snd_mixer_set_bit(unsigned int *bitmap, int bit, int val);
int snd_mixer_get_bit(unsigned int *bitmap, int bit);

int snd_mixer_element_has_info(snd_mixer_eid_t *eid);
int snd_mixer_element_info_build(void *handle, snd_mixer_element_info_t * info);
int snd_mixer_element_info_free(snd_mixer_element_info_t * info);
int snd_mixer_element_has_control(snd_mixer_eid_t *eid);
int snd_mixer_element_build(void *handle, snd_mixer_element_t * element);
int snd_mixer_element_free(snd_mixer_element_t * element);

#ifdef __cplusplus
}
#endif

