/****************************************************************************
 *                                                                          *
 *                               mixer.h                                    *
 *                           Mixer Interface                                *
 *                                                                          *
 ****************************************************************************/

typedef struct _snd_mixer snd_mixer_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_mixer_open(snd_mixer_t **handle, char *name);
int snd_mixer_close(snd_mixer_t *handle);
int snd_mixer_card(snd_mixer_t *handle);
int snd_mixer_poll_descriptor(snd_mixer_t *handle);

#ifdef __cplusplus
}
#endif

/*
 *  Simple (legacy) mixer API
 */

typedef enum _snd_mixer_channel_id {
	SND_MIXER_CHN_FRONT_LEFT = 0,
	SND_MIXER_CHN_FRONT_RIGHT,
	SND_MIXER_CHN_FRONT_CENTER,
	SND_MIXER_CHN_REAR_LEFT,
	SND_MIXER_CHN_REAR_RIGHT,
	SND_MIXER_CHN_WOOFER,
	SND_MIXER_CHN_LAST = 31,
	SND_MIXER_CHN_MONO = SND_MIXER_CHN_FRONT_LEFT
} snd_mixer_channel_id_t;

#define SND_MIXER_CHN_MASK_MONO		(1<<SND_MIXER_CHN_MONO)
#define SND_MIXER_CHN_MASK_FRONT_LEFT	(1<<SND_MIXER_CHN_FRONT_LEFT)
#define SND_MIXER_CHN_MASK_FRONT_RIGHT	(1<<SND_MIXER_CHN_FRONT_RIGHT)
#define SND_MIXER_CHN_MASK_FRONT_CENTER	(1<<SND_MIXER_CHN_FRONT_CENTER)
#define SND_MIXER_CHN_MASK_REAR_LEFT	(1<<SND_MIXER_CHN_REAR_LEFT)
#define SND_MIXER_CHN_MASK_REAR_RIGHT	(1<<SND_MIXER_CHN_REAR_RIGHT)
#define SND_MIXER_CHN_MASK_WOOFER	(1<<SND_MIXER_CHN_WOOFER)
#define SND_MIXER_CHN_MASK_STEREO	(SND_MIXER_CHN_MASK_FRONT_LEFT|SND_MIXER_CHN_MASK_FRONT_RIGHT)

#define SND_MIXER_SCTCAP_VOLUME         (1<<0)
#define SND_MIXER_SCTCAP_JOINTLY_VOLUME (1<<1)
#define SND_MIXER_SCTCAP_MUTE           (1<<2)
#define SND_MIXER_SCTCAP_JOINTLY_MUTE   (1<<3)
#define SND_MIXER_SCTCAP_CAPTURE        (1<<4)
#define SND_MIXER_SCTCAP_JOINTLY_CAPTURE (1<<5)
#define SND_MIXER_SCTCAP_EXCL_CAPTURE   (1<<6)

typedef struct _snd_mixer_sid {
	unsigned char name[60];
	unsigned int index;
} snd_mixer_sid_t;

typedef struct _snd_mixer_simple_control_list {
	unsigned int controls_offset;	/* W: first control ID to get */
	unsigned int controls_request;	/* W: count of control IDs to get */
	unsigned int controls_count;	/* R: count of available (set) IDs */
	unsigned int controls;		/* R: count of all available controls */
	snd_mixer_sid_t *pids;		/* W: IDs */
        char reserved[50];
} snd_mixer_simple_control_list_t;

typedef struct _snd_mixer_simple_control {
	snd_mixer_sid_t sid;		/* WR: simple control identification */
	unsigned int caps;		/* RO: capabilities */
	unsigned int channels;		/* RO: bitmap of active channels */
	unsigned int mute;		/* RW: bitmap of muted channels */
	unsigned int capture;		/* RW: bitmap of capture channels */
	int capture_group;		/* RO: capture group (for exclusive capture) */
	long min;			/* RO: minimum value */
	long max;			/* RO: maximum value */
	char reserved[32];
	union {
		struct {
			long front_left;	/* front left value */
			long front_right;	/* front right value */
			long front_center;	/* front center */
			long rear_left;		/* left rear */
			long rear_right;	/* right rear */
			long woofer;		/* woofer */
		} names;
		long values[32];
	} volume;                       /* RW */
} snd_mixer_simple_control_t;

typedef struct _snd_mixer_simple_callbacks {
	void *private_data;	/* may be used by an application */
	void (*rebuild) (snd_mixer_t *handle, void *private_data);
	void (*value) (snd_mixer_t *handle, void *private_data, snd_mixer_sid_t *id);
	void (*change) (snd_mixer_t *handle, void *private_data, snd_mixer_sid_t *id);
	void (*add) (snd_mixer_t *handle, void *private_data, snd_mixer_sid_t *id);
	void (*remove) (snd_mixer_t *handle, void *private_data, snd_mixer_sid_t *id);
	void *reserved[58];	/* reserved for future use - must be NULL!!! */
} snd_mixer_simple_callbacks_t;

#ifdef __cplusplus
extern "C" {
#endif

const char *snd_mixer_simple_channel_name(int channel);
int snd_mixer_simple_control_list(snd_mixer_t *handle, snd_mixer_simple_control_list_t *list);
int snd_mixer_simple_control_read(snd_mixer_t *handle, snd_mixer_simple_control_t *simple);
int snd_mixer_simple_control_write(snd_mixer_t *handle, snd_mixer_simple_control_t *simple);
int snd_mixer_simple_read(snd_mixer_t *handle, snd_mixer_simple_callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif

