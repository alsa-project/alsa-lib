/* hw_params */
struct snd_ext_parm {
	unsigned int min, max;
	unsigned int num_list;
	unsigned int *list;
	unsigned int active: 1;
	unsigned int integer: 1;
};

static inline snd_mask_t *hw_param_mask(snd_pcm_hw_params_t *params,
					snd_pcm_hw_param_t var)
{
	return &params->masks[var - SND_PCM_HW_PARAM_FIRST_MASK];
}

static inline snd_interval_t *hw_param_interval(snd_pcm_hw_params_t *params,
						snd_pcm_hw_param_t var)
{
	return &params->intervals[var - SND_PCM_HW_PARAM_FIRST_INTERVAL];
}

int snd_ext_parm_set_minmax(struct snd_ext_parm *parm, unsigned int min, unsigned int max);
int snd_ext_parm_set_list(struct snd_ext_parm *parm, unsigned int num_list, const unsigned int *list);
void snd_ext_parm_clear(struct snd_ext_parm *parm);
int snd_interval_list(snd_interval_t *ival, int num_list, unsigned int *list);
int snd_ext_parm_interval_refine(snd_interval_t *ival, struct snd_ext_parm *parm, int type);
int snd_ext_parm_mask_refine(snd_mask_t *mask, struct snd_ext_parm *parm, int type);
