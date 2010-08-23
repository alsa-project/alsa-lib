/**
 * \file include/use-case.h
 * \brief use case interface for the ALSA driver
 * \author Liam Girdwood <lrg@slimlogic.co.uk>
 * \author Stefan Schmidt <stefan@slimlogic.co.uk>
 * \author Jaroslav Kysela <perex@perex.cz>
 * \author Justin Xu <justinx@slimlogic.co.uk>
 * \date 2008-2010
 */
/*
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *  Copyright (C) 2008-2010 SlimLogic Ltd
 *  Copyright (C) 2010 Wolfson Microelectronics PLC
 *  Copyright (C) 2010 Texas Instruments Inc.
 *
 *  Support for the verb/device/modifier core logic and API,
 *  command line tool and file parser was kindly sponsored by
 *  Texas Instruments Inc.
 *  Support for multiple active modifiers and devices,
 *  transition sequences, multiple client access and user defined use
 *  cases was kindly sponsored by Wolfson Microelectronics PLC.
 */

#ifndef __ALSA_USE_CASE_H
#define __ALSA_USE_CASE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Use Case Interface
 *  The ALSA Use Case manager interface.
 *  See \ref Usecase page for more details.
 *  \{
 */

/**
 * ALSA Use Case Interface
 *
 * The use case manager works by configuring the sound card ALSA kcontrols to
 * change the hardware digital and analog audio routing to match the requested
 * device use case. The use case manager kcontrol configurations are stored in
 * easy to modify text files.
 *
 * An audio use case can be defined by a verb and device parameter. The verb
 * describes the use case action i.e. a phone call, listening to music, recording
 * a conversation etc. The device describes the physical audio capture and playback
 * hardware i.e. headphones, phone handset, bluetooth headset, etc.
 *
 * It's intended clients will mostly only need to set the use case verb and
 * device for each system use case change (as the verb and device parameters
 * cover most audio use cases).
 *
 * However there are times when a use case has to be modified at runtime. e.g.
 *
 *  o Incoming phone call when the device is playing music
 *  o Recording sections of a phone call
 *  o Playing tones during a call.
 *
 * In order to allow asynchronous runtime use case adaptations, we have a third
 * optional modifier parameter that can be used to further configure
 * the use case during live audio runtime.
 *
 * This interface allows clients to :-
 *
 *  o Query the supported use case verbs, devices and modifiers for the machine.
 *  o Set and Get use case verbs, devices and modifiers for the machine.
 *  o Get the ALSA PCM playback and capture device PCMs for use case verb and
 *     modifier.
 *  o Get the QoS parameter for each use case verb and modifier.
 *  o Get the ALSA master playback and capture volume/switch kcontrols
 *     for each use case.
 */


/*
 * Use Case Verb.
 *
 * The use case verb is the main device audio action. e.g. the "HiFi" use
 * case verb will configure the audio hardware for HiFi Music playback
 * and capture.
 */
#define SND_USE_CASE_VERB_INACTIVE		"Inactive"
#define SND_USE_CASE_VERB_HIFI			"HiFi"
#define SND_USE_CASE_VERB_HIFI_LOW_POWER	"HiFi Low Power"
#define SND_USE_CASE_VERB_VOICE		"Voice"
#define SND_USE_CASE_VERB_VOICE_LOW_POWER	"Voice Low Power"
#define SND_USE_CASE_VERB_VOICECALL		"Voice Call"
#define SND_USE_CASE_VERB_IP_VOICECALL		"Voice Call IP"
#define SND_USE_CASE_VERB_ANALOG_RADIO		"FM Analog Radio"
#define SND_USE_CASE_VERB_DIGITAL_RADIO	"FM Digital Radio"
/* add new verbs to end of list */


/*
 * Use Case Device.
 *
 * Physical system devices the render and capture audio. Devices can be OR'ed
 * together to support audio on similtanious devices.
 */
#define SND_USE_CASE_DEV_NONE		"None"
#define SND_USE_CASE_DEV_SPEAKER	"Speaker"
#define SND_USE_CASE_DEV_LINE		"Line"
#define SND_USE_CASE_DEV_HEADPHONES	"Headphones"
#define SND_USE_CASE_DEV_HEADSET	"Headset"
#define SND_USE_CASE_DEV_HANDSET	"Handset"
#define SND_USE_CASE_DEV_BLUETOOTH	"Bluetooth"
#define SND_USE_CASE_DEV_EARPIECE	"Earpiece"
#define SND_USE_CASE_DEV_SPDIF		"SPDIF"
#define SND_USE_CASE_DEV_HDMI		"HDMI"
/* add new devices to end of list */


/*
 * Use Case Modifiers.
 *
 * The use case modifier allows runtime configuration changes to deal with
 * asynchronous events.
 *
 * e.g. to record a voice call :-
 *  1. Set verb to SND_USE_CASE_VERB_VOICECALL (for voice call)
 *  2. Set modifier SND_USE_CASE_MOD_CAPTURE_VOICE when capture required.
 *  3. Call snd_use_case_get_verb_capture_pcm() to get ALSA source PCM
 *     with captured voice pcm data.
 *
 * e.g. to play a ring tone when listenin to MP3 Music :-
 *  1. Set verb to SND_USE_CASE_VERB_HIFI (for MP3 playback)
 *  2. Set modifier to SND_USE_CASE_MOD_PLAY_TONE when incoming call happens.
 *  3. Call snd_use_case_get_verb_playback_pcm() to get ALSA PCM sink for
 *     ringtone pcm data.
 */
#define SND_USE_CASE_MOD_CAPTURE_VOICE		"Capture Voice"
#define SND_USE_CASE_MOD_CAPTURE_MUSIC		"Capture Music"
#define SND_USE_CASE_MOD_PLAY_MUSIC		"Play Music"
#define SND_USE_CASE_MOD_PLAY_VOICE		"Play Voice"
#define SND_USE_CASE_MOD_PLAY_TONE		"Play Tone"
#define SND_USE_CASE_MOD_ECHO_REF		"Echo Reference"
/* add new modifiers to end of list */


/**
 * QoS - Quality of Service
 *
 * The interface allows clients to determine the audio QoS required for each
 * use case verb and modifier. It's intended as an optional hint to the
 * audio driver in order to lower power consumption.
 *
 */
enum snd_use_case_qos {
	SND_USE_CASE_QOS_UNKNOWN,
	SND_USE_CASE_QOS_MUSIC,
	SND_USE_CASE_QOS_VOICE,
	SND_USE_CASE_QOS_TONES,
};

/*
 * Use Case Control Aliases.
 *
 * Use cases often use different internal hardware paths to route digital and
 * analog audio. This can mean different controls are used to change volumes
 * depending on the particular use case. This interface allows clients to
 * find out the hardware controls associated with each use case.
 */
enum snd_use_case_control_alias {
	SND_USE_CASE_ALIAS_PLAYBACK_VOLUME = 0,
	SND_USE_CASE_ALIAS_PLAYBACK_SWITCH,
	SND_USE_CASE_ALIAS_CAPTURE_VOLUME,
	SND_USE_CASE_ALIAS_CAPTURE_SWITCH,
};

/** use case container */
typedef struct snd_use_case_mgr snd_use_case_mgr_t;

/**
 * \brief List supported use case verbs for given soundcard
 * \param uc_mgr Use case manager
 * \param verb Returned list of supported use case verbs
 * \return Number of use case verbs if success, otherwise a negative error code
 */
int snd_use_case_get_verb_list(snd_use_case_mgr_t *uc_mgr, const char **verb[]);

/**
 * \brief List supported use case devices for given use case verb
 * \param uc_mgr Use case manager
 * \param verb Verb name.
 * \param device Returned list of supported use case devices
 * \return Number of use case devices if success, otherwise a negative error code
 */
int snd_use_case_get_device_list(snd_use_case_mgr_t *uc_mgr,
		const char *verb, const char **device[]);

/**
 * \brief List supported use case verb modifiers for given verb
 * \param uc_mgr use case manager
 * \param verb verb id.
 * \param mod returned list of supported use case modifier id and names
 * \return number of use case modifiers if success, otherwise a negative error code
 */
int snd_use_case_get_mod_list(snd_use_case_mgr_t *uc_mgr,
		const char *verb, const char **mod[]);

/**
 * \brief Get current use case verb for sound card
 * \param uc_mgr Use case manager
 * \return Verb if success, otherwise NULL
 */
const char *snd_use_case_get_verb(snd_use_case_mgr_t *uc_mgr);

/**
 * \brief Set new use case verb for sound card
 * \param uc_mgr Use case manager
 * \param verb Verb
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_set_verb(snd_use_case_mgr_t *uc_mgr, const char *verb);

/**
 * \brief Enable use case device for current use case verb
 * \param uc_mgr Use case manager
 * \param device Device
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_enable_device(snd_use_case_mgr_t *uc_mgr, const char *device);

/**
 * \brief Disable use case device for current use case verb
 * \param uc_mgr Use case manager
 * \param device Device
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_disable_device(snd_use_case_mgr_t *uc_mgr, const char *device);

/**
 * \brief Disable old_device and then enable new_device.
 *        If from_device is not enabled just return.
 *        Check transmit sequence firstly.
 * \param uc_mgr Use case manager
 * \param old the device to be closed
 * \param new the device to be opened
 * \return 0 = successful negative = error
 */
int snd_use_case_switch_device(snd_use_case_mgr_t *uc_mgr,
			const char *old, const char *new);


/**
 * \brief Enable use case modifier for current use case verb
 * \param uc_mgr Use case manager
 * \param modifier Modifier
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_enable_modifier(snd_use_case_mgr_t *uc_mgr,
		const char *modifier);

/**
 * \brief Disable use case modifier for curent use case verb
 * \param uc_mgr Use case manager
 * \param modifier Modifier
 * \return Zero if success, otherwise a negative error code
 */
int snd_use_case_disable_modifier(snd_use_case_mgr_t *uc_mgr,
		const char *modifier);

/**
 * \brief Disable old_modifier and then enable new_modifier.
 *        If old_modifier is not enabled just return.
 *        Check transmit sequence firstly.
 * \param uc_mgr Use case manager
 * \param old the modifier to be closed
 * \param new the modifier to be opened
 * \return 0 = successful negative = error
 */
int snd_use_case_switch_modifier(snd_use_case_mgr_t *uc_mgr,
			const char *old, const char *new);

/**
 * \brief Get device status for current use case verb
 * \param uc_mgr Use case manager
 * \param device_name The device we are interested in.
 * \return - 1 = enabled, 0 = disabled, negative = error
 */
int snd_use_case_get_device_status(snd_use_case_mgr_t *uc_mgr,
		const char *device_name);

/**
 * \brief Get modifier status for current use case verb
 * \param uc_mgr Use case manager
 * \param device_name The device we are interested in.
 * \return - 1 = enabled, 0 = disabled, negative = error
 */
int snd_use_case_get_modifier_status(snd_use_case_mgr_t *uc_mgr,
		const char *modifier_name);

/**
 * \brief Get the current use case verb QoS
 * \param uc_mgr Use case manager
 * \return QoS level
 */
enum snd_use_case_qos snd_use_case_get_verb_qos(snd_use_case_mgr_t *uc_mgr);

/**
 * \brief Get use case modifier QoS
 * \param uc_mgr use case manager
 * \param modifier Modifier
 * \return QoS level
 */
enum snd_use_case_qos snd_use_case_get_mod_qos(snd_use_case_mgr_t *uc_mgr,
	const char *modifier);

/**
 * \brief Get the current use case verb playback PCM sink ID.
 * \param uc_mgr use case manager
 * \return PCM number if success, otherwise negative
 */
int snd_use_case_get_verb_playback_pcm(snd_use_case_mgr_t *uc_mgr);

/**
 * \brief Get the current use case verb capture PCM source ID
 * \param uc_mgr Use case manager
 * \return PCM number if success, otherwise negative
 */
int snd_use_case_get_verb_capture_pcm(snd_use_case_mgr_t *uc_mgr);

/**
 * \brief Get use case modifier playback PCM sink ID
 * \param uc_mgr Use case manager
 * \param modifier Modifier
 * \return PCM number if success, otherwise negative
 */
int snd_use_case_get_mod_playback_pcm(snd_use_case_mgr_t *uc_mgr,
	const char *modifier);

/**
 * \brief Get use case modifier capture PCM source ID
 * \param uc_mgr Use case manager
 * \param modifier Modifier
 * \return PCM number if success, otherwise negative
 */
int snd_use_case_get_mod_capture_pcm(snd_use_case_mgr_t *uc_mgr,
	const char *modifier);

/**
 * \brief Get ALSA volume/mute control names depending on use case device.
 * \param uc_mgr Use case manager
 * \param type The control type we are looking for
 * \param device The device we are interested in.
 * \return name if success, otherwise NULL
 *
 * Get the control name for common volume and mute controls that are aliased
 * in the current use case verb.
 */
const char *snd_use_case_get_device_ctl_elem_id(snd_use_case_mgr_t *uc_mgr,
		enum snd_use_case_control_alias type, const char *device);

/**
 * \brief Get ALSA volume/mute control names depending on use case modifier.
 * \param uc_mgr Use case manager
 * \param type The control type we are looking for
 * \param modifier The modifier we are interested in.
 * \return name if success, otherwise NULL
 *
 * Get the control name for common volume and mute controls that are aliased
 * in the current use case modifier.
 */
const char *snd_use_case_get_modifier_ctl_elem_id(snd_use_case_mgr_t *uc_mgr,
		enum snd_use_case_control_alias type, const char *modifier);

/**
 * \brief Open and initialise use case core for sound card
 * \param card_name Sound card name.
 * \return Use case handle if success, otherwise NULL
 */
snd_use_case_mgr_t *snd_use_case_mgr_open(const char *card_name);


/**
 * \brief Reload and re-parse use case configuration files for sound card.
 * \param uc_mgr Use case manager
 * \return zero if success, otherwise a negative error code
 */
int snd_use_case_mgr_reload(snd_use_case_mgr_t *uc_mgr);

/**
 * \brief Close use case manager
 * \param uc_mgr Use case manager
 * \return zero if success, otherwise a negative error code
 */
int snd_use_case_mgr_close(snd_use_case_mgr_t *uc_mgr);

/**
 * \brief Reset use case manager verb, device, modifier to deafult settings.
 * \param uc_mgr Use case manager
 * \return zero if success, otherwise a negative error code
 */
int snd_use_case_mgr_reset(snd_use_case_mgr_t *uc_mgr);

/**
 * \brief Dump current sound card use case control settings
 * \param card_name Sound card name
 * \return zero if success, otherwise a negative error code
 */
int snd_use_case_dump(const char *card_name);

/**
 *  \}
 */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_USE_CASE_H */
