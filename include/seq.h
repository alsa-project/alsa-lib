/****************************************************************************
 *                                                                          *
 *                                seq.h                                     *
 *                              Sequencer                                   *
 *                                                                          *
 ****************************************************************************/

typedef sndrv_seq_tick_time_t snd_seq_tick_time_t;
typedef sndrv_seq_position_t snd_seq_position_t;
typedef sndrv_seq_frequency_t snd_seq_frequency_t;
typedef sndrv_seq_instr_cluster_t snd_seq_instr_cluster_t;
typedef struct sndrv_seq_port_info snd_seq_port_info_t;
typedef struct sndrv_seq_port_subscribe snd_seq_port_subscribe_t;
typedef struct sndrv_seq_event snd_seq_event_t;
typedef struct sndrv_seq_addr snd_seq_addr_t;
typedef struct sndrv_seq_ev_volume snd_seq_ev_volume_t;
typedef struct sndrv_seq_ev_loop snd_seq_ev_loop_t;
typedef struct sndrv_seq_remove_events snd_seq_remove_events_t;
typedef struct sndrv_seq_query_subs snd_seq_query_subs_t;
typedef struct sndrv_seq_real_time snd_seq_real_time_t;
typedef struct sndrv_seq_system_info snd_seq_system_info_t;
typedef struct sndrv_seq_client_info snd_seq_client_info_t;
typedef struct sndrv_seq_queue_info snd_seq_queue_info_t;
typedef struct sndrv_seq_queue_status snd_seq_queue_status_t;
typedef struct sndrv_seq_queue_tempo snd_seq_queue_tempo_t;
typedef struct sndrv_seq_queue_owner snd_seq_queue_owner_t;
typedef struct sndrv_seq_queue_timer snd_seq_queue_timer_t;
typedef struct sndrv_seq_queue_client snd_seq_queue_client_t;
typedef struct sndrv_seq_client_pool snd_seq_client_pool_t;
typedef struct sndrv_seq_instr snd_seq_instr_t;
typedef struct sndrv_seq_instr_data snd_seq_instr_data_t;
typedef struct sndrv_seq_instr_free snd_seq_instr_free_t;
typedef struct sndrv_seq_instr_put snd_seq_instr_put_t;
typedef struct sndrv_seq_instr_get snd_seq_instr_get_t;
typedef union sndrv_seq_timestamp snd_seq_timestamp_t;

typedef enum sndrv_seq_client_type snd_seq_client_type_t;
typedef enum sndrv_seq_stop_mode snd_seq_stop_mode_t;

#define snd_seq_event_bounce_ext_data	sndrv_seq_event_bounce_ext_data 
#define snd_seq_ev_is_result_type	sndrv_seq_ev_is_result_type     
#define snd_seq_ev_is_channel_type	sndrv_seq_ev_is_channel_type    
#define snd_seq_ev_is_note_type		sndrv_seq_ev_is_note_type       
#define snd_seq_ev_is_control_type	sndrv_seq_ev_is_control_type    
#define snd_seq_ev_is_queue_type	sndrv_seq_ev_is_queue_type      
#define snd_seq_ev_is_message_type	sndrv_seq_ev_is_message_type    
#define snd_seq_ev_is_sample_type	sndrv_seq_ev_is_sample_type     
#define snd_seq_ev_is_user_type		sndrv_seq_ev_is_user_type       
#define snd_seq_ev_is_fixed_type	sndrv_seq_ev_is_fixed_type      
#define snd_seq_ev_is_instr_type	sndrv_seq_ev_is_instr_type      
#define snd_seq_ev_is_variable_type	sndrv_seq_ev_is_variable_type   
#define snd_seq_ev_is_varipc_type	sndrv_seq_ev_is_varipc_type     
#define snd_seq_ev_is_reserved		sndrv_seq_ev_is_reserved        
#define snd_seq_ev_is_direct		sndrv_seq_ev_is_direct          
#define snd_seq_ev_is_prior		sndrv_seq_ev_is_prior           
#define snd_seq_ev_length_type		sndrv_seq_ev_length_type        
#define snd_seq_ev_is_fixed		sndrv_seq_ev_is_fixed           
#define snd_seq_ev_is_variable		sndrv_seq_ev_is_variable        
#define snd_seq_ev_is_varusr		sndrv_seq_ev_is_varusr          
#define snd_seq_ev_is_varipc		sndrv_seq_ev_is_varipc          
#define snd_seq_ev_timestamp_type	sndrv_seq_ev_timestamp_type     
#define snd_seq_ev_is_tick		sndrv_seq_ev_is_tick            
#define snd_seq_ev_is_real		sndrv_seq_ev_is_real            
#define snd_seq_ev_timemode_type	sndrv_seq_ev_timemode_type      
#define snd_seq_ev_is_abstime		sndrv_seq_ev_is_abstime         
#define snd_seq_ev_is_reltime		sndrv_seq_ev_is_reltime         
#define snd_seq_queue_sync_port		sndrv_seq_queue_sync_port       
#define snd_seq_queue_owner		sndrv_seq_queue_owner           

#ifdef SNDRV_SEQ_SYNC_SUPPORT
#define SND_SEQ_SYNC_SUPPORT SNDRV_SEQ_SYNC_SUPPORT
#endif

#define SND_SEQ_EVENT_SYSTEM SNDRV_SEQ_EVENT_SYSTEM
#define SND_SEQ_EVENT_RESULT SNDRV_SEQ_EVENT_RESULT
#define SND_SEQ_EVENT_NOTE SNDRV_SEQ_EVENT_NOTE
#define SND_SEQ_EVENT_NOTEON SNDRV_SEQ_EVENT_NOTEON
#define SND_SEQ_EVENT_NOTEOFF SNDRV_SEQ_EVENT_NOTEOFF
#define SND_SEQ_EVENT_KEYPRESS SNDRV_SEQ_EVENT_KEYPRESS
#define SND_SEQ_EVENT_CONTROLLER SNDRV_SEQ_EVENT_CONTROLLER
#define SND_SEQ_EVENT_PGMCHANGE SNDRV_SEQ_EVENT_PGMCHANGE
#define SND_SEQ_EVENT_CHANPRESS SNDRV_SEQ_EVENT_CHANPRESS
#define SND_SEQ_EVENT_PITCHBEND SNDRV_SEQ_EVENT_PITCHBEND
#define SND_SEQ_EVENT_CONTROL14 SNDRV_SEQ_EVENT_CONTROL14
#define SND_SEQ_EVENT_NONREGPARAM SNDRV_SEQ_EVENT_NONREGPARAM
#define SND_SEQ_EVENT_REGPARAM SNDRV_SEQ_EVENT_REGPARAM
#define SND_SEQ_EVENT_SONGPOS SNDRV_SEQ_EVENT_SONGPOS
#define SND_SEQ_EVENT_SONGSEL SNDRV_SEQ_EVENT_SONGSEL
#define SND_SEQ_EVENT_QFRAME SNDRV_SEQ_EVENT_QFRAME
#define SND_SEQ_EVENT_TIMESIGN SNDRV_SEQ_EVENT_TIMESIGN
#define SND_SEQ_EVENT_KEYSIGN SNDRV_SEQ_EVENT_KEYSIGN
#define SND_SEQ_EVENT_START SNDRV_SEQ_EVENT_START
#define SND_SEQ_EVENT_CONTINUE SNDRV_SEQ_EVENT_CONTINUE
#define SND_SEQ_EVENT_STOP SNDRV_SEQ_EVENT_STOP
#define SND_SEQ_EVENT_SETPOS_TICK SNDRV_SEQ_EVENT_SETPOS_TICK
#define SND_SEQ_EVENT_SETPOS_TIME SNDRV_SEQ_EVENT_SETPOS_TIME
#define SND_SEQ_EVENT_TEMPO SNDRV_SEQ_EVENT_TEMPO
#define SND_SEQ_EVENT_CLOCK SNDRV_SEQ_EVENT_CLOCK
#define SND_SEQ_EVENT_TICK SNDRV_SEQ_EVENT_TICK
#define SND_SEQ_EVENT_SYNC SNDRV_SEQ_EVENT_SYNC
#define SND_SEQ_EVENT_SYNC_POS SNDRV_SEQ_EVENT_SYNC_POS
#define SND_SEQ_EVENT_TUNE_REQUEST SNDRV_SEQ_EVENT_TUNE_REQUEST
#define SND_SEQ_EVENT_RESET SNDRV_SEQ_EVENT_RESET
#define SND_SEQ_EVENT_SENSING SNDRV_SEQ_EVENT_SENSING
#define SND_SEQ_EVENT_ECHO SNDRV_SEQ_EVENT_ECHO
#define SND_SEQ_EVENT_OSS SNDRV_SEQ_EVENT_OSS
#define SND_SEQ_EVENT_CLIENT_START SNDRV_SEQ_EVENT_CLIENT_START
#define SND_SEQ_EVENT_CLIENT_EXIT SNDRV_SEQ_EVENT_CLIENT_EXIT
#define SND_SEQ_EVENT_CLIENT_CHANGE SNDRV_SEQ_EVENT_CLIENT_CHANGE
#define SND_SEQ_EVENT_PORT_START SNDRV_SEQ_EVENT_PORT_START
#define SND_SEQ_EVENT_PORT_EXIT SNDRV_SEQ_EVENT_PORT_EXIT
#define SND_SEQ_EVENT_PORT_CHANGE SNDRV_SEQ_EVENT_PORT_CHANGE
#define SND_SEQ_EVENT_PORT_SUBSCRIBED SNDRV_SEQ_EVENT_PORT_SUBSCRIBED
#define SND_SEQ_EVENT_PORT_USED SNDRV_SEQ_EVENT_PORT_USED
#define SND_SEQ_EVENT_PORT_UNSUBSCRIBED SNDRV_SEQ_EVENT_PORT_UNSUBSCRIBED
#define SND_SEQ_EVENT_PORT_UNUSED SNDRV_SEQ_EVENT_PORT_UNUSED
#define SND_SEQ_EVENT_SAMPLE SNDRV_SEQ_EVENT_SAMPLE
#define SND_SEQ_EVENT_SAMPLE_CLUSTER SNDRV_SEQ_EVENT_SAMPLE_CLUSTER
#define SND_SEQ_EVENT_SAMPLE_START SNDRV_SEQ_EVENT_SAMPLE_START
#define SND_SEQ_EVENT_SAMPLE_STOP SNDRV_SEQ_EVENT_SAMPLE_STOP
#define SND_SEQ_EVENT_SAMPLE_FREQ SNDRV_SEQ_EVENT_SAMPLE_FREQ
#define SND_SEQ_EVENT_SAMPLE_VOLUME SNDRV_SEQ_EVENT_SAMPLE_VOLUME
#define SND_SEQ_EVENT_SAMPLE_LOOP SNDRV_SEQ_EVENT_SAMPLE_LOOP
#define SND_SEQ_EVENT_SAMPLE_POSITION SNDRV_SEQ_EVENT_SAMPLE_POSITION
#define SND_SEQ_EVENT_SAMPLE_PRIVATE1 SNDRV_SEQ_EVENT_SAMPLE_PRIVATE1
#define SND_SEQ_EVENT_USR0 SNDRV_SEQ_EVENT_USR0
#define SND_SEQ_EVENT_USR1 SNDRV_SEQ_EVENT_USR1
#define SND_SEQ_EVENT_USR2 SNDRV_SEQ_EVENT_USR2
#define SND_SEQ_EVENT_USR3 SNDRV_SEQ_EVENT_USR3
#define SND_SEQ_EVENT_USR4 SNDRV_SEQ_EVENT_USR4
#define SND_SEQ_EVENT_USR5 SNDRV_SEQ_EVENT_USR5
#define SND_SEQ_EVENT_USR6 SNDRV_SEQ_EVENT_USR6
#define SND_SEQ_EVENT_USR7 SNDRV_SEQ_EVENT_USR7
#define SND_SEQ_EVENT_USR8 SNDRV_SEQ_EVENT_USR8
#define SND_SEQ_EVENT_USR9 SNDRV_SEQ_EVENT_USR9
#define SND_SEQ_EVENT_INSTR_BEGIN SNDRV_SEQ_EVENT_INSTR_BEGIN
#define SND_SEQ_EVENT_INSTR_END SNDRV_SEQ_EVENT_INSTR_END
#define SND_SEQ_EVENT_INSTR_INFO SNDRV_SEQ_EVENT_INSTR_INFO
#define SND_SEQ_EVENT_INSTR_INFO_RESULT SNDRV_SEQ_EVENT_INSTR_INFO_RESULT
#define SND_SEQ_EVENT_INSTR_FINFO SNDRV_SEQ_EVENT_INSTR_FINFO
#define SND_SEQ_EVENT_INSTR_FINFO_RESULT SNDRV_SEQ_EVENT_INSTR_FINFO_RESULT
#define SND_SEQ_EVENT_INSTR_RESET SNDRV_SEQ_EVENT_INSTR_RESET
#define SND_SEQ_EVENT_INSTR_STATUS SNDRV_SEQ_EVENT_INSTR_STATUS
#define SND_SEQ_EVENT_INSTR_STATUS_RESULT SNDRV_SEQ_EVENT_INSTR_STATUS_RESULT
#define SND_SEQ_EVENT_INSTR_PUT SNDRV_SEQ_EVENT_INSTR_PUT
#define SND_SEQ_EVENT_INSTR_GET SNDRV_SEQ_EVENT_INSTR_GET
#define SND_SEQ_EVENT_INSTR_GET_RESULT SNDRV_SEQ_EVENT_INSTR_GET_RESULT
#define SND_SEQ_EVENT_INSTR_FREE SNDRV_SEQ_EVENT_INSTR_FREE
#define SND_SEQ_EVENT_INSTR_LIST SNDRV_SEQ_EVENT_INSTR_LIST
#define SND_SEQ_EVENT_INSTR_LIST_RESULT SNDRV_SEQ_EVENT_INSTR_LIST_RESULT
#define SND_SEQ_EVENT_INSTR_CLUSTER SNDRV_SEQ_EVENT_INSTR_CLUSTER
#define SND_SEQ_EVENT_INSTR_CLUSTER_GET SNDRV_SEQ_EVENT_INSTR_CLUSTER_GET
#define SND_SEQ_EVENT_INSTR_CLUSTER_RESULT SNDRV_SEQ_EVENT_INSTR_CLUSTER_RESULT
#define SND_SEQ_EVENT_INSTR_CHANGE SNDRV_SEQ_EVENT_INSTR_CHANGE
#define SND_SEQ_EVENT_LENGTH_VARIABLE SNDRV_SEQ_EVENT_LENGTH_VARIABLE
#define SND_SEQ_EVENT_SYSEX SNDRV_SEQ_EVENT_SYSEX
#define SND_SEQ_EVENT_BOUNCE SNDRV_SEQ_EVENT_BOUNCE
#define SND_SEQ_EVENT_USR_VAR0 SNDRV_SEQ_EVENT_USR_VAR0
#define SND_SEQ_EVENT_USR_VAR1 SNDRV_SEQ_EVENT_USR_VAR1
#define SND_SEQ_EVENT_USR_VAR2 SNDRV_SEQ_EVENT_USR_VAR2
#define SND_SEQ_EVENT_USR_VAR3 SNDRV_SEQ_EVENT_USR_VAR3
#define SND_SEQ_EVENT_USR_VAR4 SNDRV_SEQ_EVENT_USR_VAR4
#define SND_SEQ_EVENT_LENGTH_VARIPC SNDRV_SEQ_EVENT_LENGTH_VARIPC
#define SND_SEQ_EVENT_IPCSHM SNDRV_SEQ_EVENT_IPCSHM
#define SND_SEQ_EVENT_USR_VARIPC0 SNDRV_SEQ_EVENT_USR_VARIPC0
#define SND_SEQ_EVENT_USR_VARIPC1 SNDRV_SEQ_EVENT_USR_VARIPC1
#define SND_SEQ_EVENT_USR_VARIPC2 SNDRV_SEQ_EVENT_USR_VARIPC2
#define SND_SEQ_EVENT_USR_VARIPC3 SNDRV_SEQ_EVENT_USR_VARIPC3
#define SND_SEQ_EVENT_USR_VARIPC4 SNDRV_SEQ_EVENT_USR_VARIPC4
#define SND_SEQ_EVENT_KERNEL_ERROR SNDRV_SEQ_EVENT_KERNEL_ERROR
#define SND_SEQ_EVENT_KERNEL_QUOTE SNDRV_SEQ_EVENT_KERNEL_QUOTE
#define SND_SEQ_EVENT_NONE SNDRV_SEQ_EVENT_NONE
#define SND_SEQ_ADDRESS_UNKNOWN SNDRV_SEQ_ADDRESS_UNKNOWN
#define SND_SEQ_ADDRESS_SUBSCRIBERS SNDRV_SEQ_ADDRESS_SUBSCRIBERS
#define SND_SEQ_ADDRESS_BROADCAST SNDRV_SEQ_ADDRESS_BROADCAST
#define SND_SEQ_QUEUE_DIRECT SNDRV_SEQ_QUEUE_DIRECT
#define SND_SEQ_TIME_STAMP_TICK SNDRV_SEQ_TIME_STAMP_TICK
#define SND_SEQ_TIME_STAMP_REAL SNDRV_SEQ_TIME_STAMP_REAL
#define SND_SEQ_TIME_STAMP_MASK SNDRV_SEQ_TIME_STAMP_MASK
#define SND_SEQ_TIME_MODE_ABS SNDRV_SEQ_TIME_MODE_ABS
#define SND_SEQ_TIME_MODE_REL SNDRV_SEQ_TIME_MODE_REL
#define SND_SEQ_TIME_MODE_MASK SNDRV_SEQ_TIME_MODE_MASK
#define SND_SEQ_EVENT_LENGTH_FIXED SNDRV_SEQ_EVENT_LENGTH_FIXED
#define SND_SEQ_EVENT_LENGTH_VARIABLE SNDRV_SEQ_EVENT_LENGTH_VARIABLE
#define SND_SEQ_EVENT_LENGTH_VARUSR SNDRV_SEQ_EVENT_LENGTH_VARUSR
#define SND_SEQ_EVENT_LENGTH_VARIPC SNDRV_SEQ_EVENT_LENGTH_VARIPC
#define SND_SEQ_EVENT_LENGTH_MASK SNDRV_SEQ_EVENT_LENGTH_MASK
#define SND_SEQ_PRIORITY_NORMAL SNDRV_SEQ_PRIORITY_NORMAL
#define SND_SEQ_PRIORITY_HIGH SNDRV_SEQ_PRIORITY_HIGH
#define SND_SEQ_PRIORITY_MASK SNDRV_SEQ_PRIORITY_MASK
#define SND_SEQ_EVENT_NOTE SNDRV_SEQ_EVENT_NOTE
#define SND_SEQ_EVENT_NOTE SNDRV_SEQ_EVENT_NOTE
#define SND_SEQ_QUEUE_DIRECT SNDRV_SEQ_QUEUE_DIRECT
#define SND_SEQ_PRIORITY_MASK SNDRV_SEQ_PRIORITY_MASK
#define SND_SEQ_EVENT_LENGTH_MASK SNDRV_SEQ_EVENT_LENGTH_MASK
#define SND_SEQ_EVENT_LENGTH_FIXED SNDRV_SEQ_EVENT_LENGTH_FIXED
#define SND_SEQ_EVENT_LENGTH_VARIABLE SNDRV_SEQ_EVENT_LENGTH_VARIABLE
#define SND_SEQ_EVENT_LENGTH_VARUSR SNDRV_SEQ_EVENT_LENGTH_VARUSR
#define SND_SEQ_EVENT_LENGTH_VARIPC SNDRV_SEQ_EVENT_LENGTH_VARIPC
#define SND_SEQ_TIME_STAMP_MASK SNDRV_SEQ_TIME_STAMP_MASK
#define SND_SEQ_TIME_STAMP_TICK SNDRV_SEQ_TIME_STAMP_TICK
#define SND_SEQ_TIME_STAMP_REAL SNDRV_SEQ_TIME_STAMP_REAL
#define SND_SEQ_TIME_MODE_MASK SNDRV_SEQ_TIME_MODE_MASK
#define SND_SEQ_TIME_MODE_ABS SNDRV_SEQ_TIME_MODE_ABS
#define SND_SEQ_TIME_MODE_REL SNDRV_SEQ_TIME_MODE_REL
#define SND_SEQ_CLIENT_SYSTEM SNDRV_SEQ_CLIENT_SYSTEM
#define SND_SEQ_CLIENT_DUMMY SNDRV_SEQ_CLIENT_DUMMY
#define SND_SEQ_CLIENT_OSS SNDRV_SEQ_CLIENT_OSS
#define SND_SEQ_FILTER_BROADCAST SNDRV_SEQ_FILTER_BROADCAST
#define SND_SEQ_FILTER_MULTICAST SNDRV_SEQ_FILTER_MULTICAST
#define SND_SEQ_FILTER_BOUNCE SNDRV_SEQ_FILTER_BOUNCE
#define SND_SEQ_FILTER_USE_EVENT SNDRV_SEQ_FILTER_USE_EVENT
#define SND_SEQ_REMOVE_DEST SNDRV_SEQ_REMOVE_DEST
#define SND_SEQ_REMOVE_DEST_CHANNEL SNDRV_SEQ_REMOVE_DEST_CHANNEL
#define SND_SEQ_REMOVE_TIME_BEFORE SNDRV_SEQ_REMOVE_TIME_BEFORE
#define SND_SEQ_REMOVE_TIME_AFTER SNDRV_SEQ_REMOVE_TIME_AFTER
#define SND_SEQ_REMOVE_EVENT_TYPE SNDRV_SEQ_REMOVE_EVENT_TYPE
#define SND_SEQ_REMOVE_IGNORE_OFF SNDRV_SEQ_REMOVE_IGNORE_OFF
#define SND_SEQ_REMOVE_TAG_MATCH SNDRV_SEQ_REMOVE_TAG_MATCH
#define SND_SEQ_PORT_SYSTEM_TIMER SNDRV_SEQ_PORT_SYSTEM_TIMER
#define SND_SEQ_PORT_SYSTEM_ANNOUNCE SNDRV_SEQ_PORT_SYSTEM_ANNOUNCE
#define SND_SEQ_PORT_CAP_READ SNDRV_SEQ_PORT_CAP_READ
#define SND_SEQ_PORT_CAP_WRITE SNDRV_SEQ_PORT_CAP_WRITE
#define SND_SEQ_PORT_CAP_SYNC_READ SNDRV_SEQ_PORT_CAP_SYNC_READ
#define SND_SEQ_PORT_CAP_SYNC_WRITE SNDRV_SEQ_PORT_CAP_SYNC_WRITE
#define SND_SEQ_PORT_CAP_DUPLEX SNDRV_SEQ_PORT_CAP_DUPLEX
#define SND_SEQ_PORT_CAP_SUBS_READ SNDRV_SEQ_PORT_CAP_SUBS_READ
#define SND_SEQ_PORT_CAP_SUBS_WRITE SNDRV_SEQ_PORT_CAP_SUBS_WRITE
#define SND_SEQ_PORT_CAP_NO_EXPORT SNDRV_SEQ_PORT_CAP_NO_EXPORT
#define SND_SEQ_PORT_TYPE_SPECIFIC SNDRV_SEQ_PORT_TYPE_SPECIFIC
#define SND_SEQ_PORT_TYPE_MIDI_GENERIC SNDRV_SEQ_PORT_TYPE_MIDI_GENERIC
#define SND_SEQ_PORT_TYPE_MIDI_GM SNDRV_SEQ_PORT_TYPE_MIDI_GM
#define SND_SEQ_PORT_TYPE_MIDI_GS SNDRV_SEQ_PORT_TYPE_MIDI_GS
#define SND_SEQ_PORT_TYPE_MIDI_XG SNDRV_SEQ_PORT_TYPE_MIDI_XG
#define SND_SEQ_PORT_TYPE_MIDI_MT32 SNDRV_SEQ_PORT_TYPE_MIDI_MT32
#define SND_SEQ_PORT_TYPE_SYNTH SNDRV_SEQ_PORT_TYPE_SYNTH
#define SND_SEQ_PORT_TYPE_DIRECT_SAMPLE SNDRV_SEQ_PORT_TYPE_DIRECT_SAMPLE
#define SND_SEQ_PORT_TYPE_SAMPLE SNDRV_SEQ_PORT_TYPE_SAMPLE
#define SND_SEQ_PORT_TYPE_APPLICATION SNDRV_SEQ_PORT_TYPE_APPLICATION
#define SND_SEQ_GROUP_SYSTEM SNDRV_SEQ_GROUP_SYSTEM
#define SND_SEQ_GROUP_DEVICE SNDRV_SEQ_GROUP_DEVICE
#define SND_SEQ_GROUP_APPLICATION SNDRV_SEQ_GROUP_APPLICATION
#define SND_SEQ_PORT_FLG_GIVEN_PORT SNDRV_SEQ_PORT_FLG_GIVEN_PORT
#define SND_SEQ_QUEUE_FLG_SYNC SNDRV_SEQ_QUEUE_FLG_SYNC
#define SND_SEQ_QUEUE_FLG_SYNC_LOST SNDRV_SEQ_QUEUE_FLG_SYNC_LOST
#define SND_SEQ_SYNC_TICK SNDRV_SEQ_SYNC_TICK
#define SND_SEQ_SYNC_TIME SNDRV_SEQ_SYNC_TIME
#define SND_SEQ_SYNC_MODE SNDRV_SEQ_SYNC_MODE
#define SND_SEQ_SYNC_FMT_PRIVATE_CLOCK SNDRV_SEQ_SYNC_FMT_PRIVATE_CLOCK
#define SND_SEQ_SYNC_FMT_PRIVATE_TIME SNDRV_SEQ_SYNC_FMT_PRIVATE_TIME
#define SND_SEQ_SYNC_FMT_MIDI_CLOCK SNDRV_SEQ_SYNC_FMT_MIDI_CLOCK
#define SND_SEQ_SYNC_FMT_MTC SNDRV_SEQ_SYNC_FMT_MTC
#define SND_SEQ_SYNC_FMT_DTL SNDRV_SEQ_SYNC_FMT_DTL
#define SND_SEQ_SYNC_FMT_SMPTE SNDRV_SEQ_SYNC_FMT_SMPTE
#define SND_SEQ_SYNC_FMT_MIDI_TICK SNDRV_SEQ_SYNC_FMT_MIDI_TICK
#define SND_SEQ_SYNC_FPS_24 SNDRV_SEQ_SYNC_FPS_24
#define SND_SEQ_SYNC_FPS_25 SNDRV_SEQ_SYNC_FPS_25
#define SND_SEQ_SYNC_FPS_30_DP SNDRV_SEQ_SYNC_FPS_30_DP
#define SND_SEQ_SYNC_FPS_30_NDP SNDRV_SEQ_SYNC_FPS_30_NDP
#define SND_SEQ_TIMER_ALSA SNDRV_SEQ_TIMER_ALSA
#define SND_SEQ_TIMER_MIDI_CLOCK SNDRV_SEQ_TIMER_MIDI_CLOCK
#define SND_SEQ_TIMER_MIDI_TICK SNDRV_SEQ_TIMER_MIDI_TICK
#define SND_SEQ_QUERY_SUBS_READ SNDRV_SEQ_QUERY_SUBS_READ
#define SND_SEQ_QUERY_SUBS_WRITE SNDRV_SEQ_QUERY_SUBS_WRITE
#define SND_SEQ_INSTR_ATYPE_DATA SNDRV_SEQ_INSTR_ATYPE_DATA
#define SND_SEQ_INSTR_ATYPE_ALIAS SNDRV_SEQ_INSTR_ATYPE_ALIAS
#define SND_SEQ_INSTR_ID_DLS1 SNDRV_SEQ_INSTR_ID_DLS1
#define SND_SEQ_INSTR_ID_DLS2 SNDRV_SEQ_INSTR_ID_DLS2
#define SND_SEQ_INSTR_ID_SIMPLE SNDRV_SEQ_INSTR_ID_SIMPLE
#define SND_SEQ_INSTR_ID_SOUNDFONT SNDRV_SEQ_INSTR_ID_SOUNDFONT
#define SND_SEQ_INSTR_ID_GUS_PATCH SNDRV_SEQ_INSTR_ID_GUS_PATCH
#define SND_SEQ_INSTR_ID_INTERWAVE SNDRV_SEQ_INSTR_ID_INTERWAVE
#define SND_SEQ_INSTR_ID_OPL2_3 SNDRV_SEQ_INSTR_ID_OPL2_3
#define SND_SEQ_INSTR_ID_OPL4 SNDRV_SEQ_INSTR_ID_OPL4
#define SND_SEQ_INSTR_TYPE0_DLS1 SNDRV_SEQ_INSTR_TYPE0_DLS1
#define SND_SEQ_INSTR_TYPE0_DLS2 SNDRV_SEQ_INSTR_TYPE0_DLS2
#define SND_SEQ_INSTR_TYPE1_SIMPLE SNDRV_SEQ_INSTR_TYPE1_SIMPLE
#define SND_SEQ_INSTR_TYPE1_SOUNDFONT SNDRV_SEQ_INSTR_TYPE1_SOUNDFONT
#define SND_SEQ_INSTR_TYPE1_GUS_PATCH SNDRV_SEQ_INSTR_TYPE1_GUS_PATCH
#define SND_SEQ_INSTR_TYPE1_INTERWAVE SNDRV_SEQ_INSTR_TYPE1_INTERWAVE
#define SND_SEQ_INSTR_TYPE2_OPL2_3 SNDRV_SEQ_INSTR_TYPE2_OPL2_3
#define SND_SEQ_INSTR_TYPE2_OPL4 SNDRV_SEQ_INSTR_TYPE2_OPL4
#define SND_SEQ_INSTR_PUT_CMD_CREATE SNDRV_SEQ_INSTR_PUT_CMD_CREATE
#define SND_SEQ_INSTR_PUT_CMD_REPLACE SNDRV_SEQ_INSTR_PUT_CMD_REPLACE
#define SND_SEQ_INSTR_PUT_CMD_MODIFY SNDRV_SEQ_INSTR_PUT_CMD_MODIFY
#define SND_SEQ_INSTR_PUT_CMD_ADD SNDRV_SEQ_INSTR_PUT_CMD_ADD
#define SND_SEQ_INSTR_PUT_CMD_REMOVE SNDRV_SEQ_INSTR_PUT_CMD_REMOVE
#define SND_SEQ_INSTR_GET_CMD_FULL SNDRV_SEQ_INSTR_GET_CMD_FULL
#define SND_SEQ_INSTR_GET_CMD_PARTIAL SNDRV_SEQ_INSTR_GET_CMD_PARTIAL
#define SND_SEQ_INSTR_QUERY_FOLLOW_ALIAS SNDRV_SEQ_INSTR_QUERY_FOLLOW_ALIAS
#define SND_SEQ_INSTR_FREE_CMD_ALL SNDRV_SEQ_INSTR_FREE_CMD_ALL
#define SND_SEQ_INSTR_FREE_CMD_PRIVATE SNDRV_SEQ_INSTR_FREE_CMD_PRIVATE
#define SND_SEQ_INSTR_FREE_CMD_CLUSTER SNDRV_SEQ_INSTR_FREE_CMD_CLUSTER
#define SND_SEQ_INSTR_FREE_CMD_SINGLE SNDRV_SEQ_INSTR_FREE_CMD_SINGLE


#define SND_SEQ_OPEN_OUTPUT	1
#define SND_SEQ_OPEN_INPUT	2
#define SND_SEQ_OPEN_DUPLEX	(SND_SEQ_OPEN_OUTPUT|SND_SEQ_OPEN_INPUT)

#define SND_SEQ_NONBLOCK	1

enum _snd_seq_type {
	SND_SEQ_TYPE_HW,
	SND_SEQ_TYPE_SHM,
	SND_SEQ_TYPE_INET,
};

#ifdef SND_ENUM_TYPECHECK
typedef struct __snd_seq_type *snd_seq_type_t;
#else
typedef enum _snd_seq_type snd_seq_type_t;
#endif

#define	SND_SEQ_TYPE_HW ((snd_seq_type_t) SND_SEQ_TYPE_HW)
#define	SND_SEQ_TYPE_SHM ((snd_seq_type_t) SND_SEQ_TYPE_SHM)
#define	SND_SEQ_TYPE_INET ((snd_seq_type_t) SND_SEQ_TYPE_INET)

typedef struct _snd_seq snd_seq_t;

#ifdef __cplusplus
extern "C" {
#endif

int snd_seq_open(snd_seq_t **handle, char *name, int streams, int mode);
int snd_seq_close(snd_seq_t *handle);
int snd_seq_poll_descriptor(snd_seq_t *handle);
int snd_seq_nonblock(snd_seq_t *handle, int nonblock);
int snd_seq_client_id(snd_seq_t *handle);
int snd_seq_output_buffer_size(snd_seq_t *handle);
int snd_seq_input_buffer_size(snd_seq_t *handle);
int snd_seq_resize_output_buffer(snd_seq_t *handle, size_t size);
int snd_seq_resize_input_buffer(snd_seq_t *handle, size_t size);
int snd_seq_system_info(snd_seq_t *handle, snd_seq_system_info_t *info);
int snd_seq_get_client_info(snd_seq_t *handle, snd_seq_client_info_t *info);
int snd_seq_get_any_client_info(snd_seq_t *handle, int client, snd_seq_client_info_t *info);
int snd_seq_set_client_info(snd_seq_t *handle, snd_seq_client_info_t *info);
int snd_seq_create_port(snd_seq_t *handle, snd_seq_port_info_t *info);
int snd_seq_delete_port(snd_seq_t *handle, snd_seq_port_info_t *info);
int snd_seq_get_port_info(snd_seq_t *handle, int port, snd_seq_port_info_t *info);
int snd_seq_get_any_port_info(snd_seq_t *handle, int client, int port, snd_seq_port_info_t *info);
int snd_seq_set_port_info(snd_seq_t *handle, int port, snd_seq_port_info_t *info);
int snd_seq_get_port_subscription(snd_seq_t *handle, snd_seq_port_subscribe_t *sub);
int snd_seq_subscribe_port(snd_seq_t *handle, snd_seq_port_subscribe_t *sub);
int snd_seq_unsubscribe_port(snd_seq_t *handle, snd_seq_port_subscribe_t *sub);
int snd_seq_query_port_subscribers(snd_seq_t *seq, snd_seq_query_subs_t * subs);
int snd_seq_get_queue_status(snd_seq_t *handle, int q, snd_seq_queue_status_t *status);
int snd_seq_get_queue_tempo(snd_seq_t *handle, int q, snd_seq_queue_tempo_t *tempo);
int snd_seq_set_queue_tempo(snd_seq_t *handle, int q, snd_seq_queue_tempo_t *tempo);
int snd_seq_get_queue_owner(snd_seq_t *handle, int q, snd_seq_queue_owner_t *owner);
int snd_seq_set_queue_owner(snd_seq_t *handle, int q, snd_seq_queue_owner_t *owner);
int snd_seq_get_queue_timer(snd_seq_t *handle, int q, snd_seq_queue_timer_t *timer);
int snd_seq_set_queue_timer(snd_seq_t *handle, int q, snd_seq_queue_timer_t *timer);
int snd_seq_get_queue_client(snd_seq_t *handle, int q, snd_seq_queue_client_t *queue);
int snd_seq_set_queue_client(snd_seq_t *handle, int q, snd_seq_queue_client_t *queue);
int snd_seq_create_queue(snd_seq_t *seq, snd_seq_queue_info_t *info);
int snd_seq_alloc_named_queue(snd_seq_t *seq, char *name);
int snd_seq_alloc_queue(snd_seq_t *handle);
#ifdef SND_SEQ_SYNC_SUPPORT
int snd_seq_alloc_sync_queue(snd_seq_t *seq, char *name);
#endif
int snd_seq_free_queue(snd_seq_t *handle, int q);
int snd_seq_get_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info);
int snd_seq_set_queue_info(snd_seq_t *seq, int q, snd_seq_queue_info_t *info);
int snd_seq_get_named_queue(snd_seq_t *seq, char *name);
int snd_seq_get_client_pool(snd_seq_t *handle, snd_seq_client_pool_t * info);
int snd_seq_set_client_pool(snd_seq_t *handle, snd_seq_client_pool_t * info);
int snd_seq_query_next_client(snd_seq_t *handle, snd_seq_client_info_t * info);
int snd_seq_query_next_port(snd_seq_t *handle, snd_seq_port_info_t * info);
#ifdef SND_SEQ_SYNC_SUPPORT
int snd_seq_add_sync_master(snd_seq_t *seq, int queue, snd_seq_addr_t *dest, snd_seq_queue_sync_t *info);
int snd_seq_remove_sync_master(snd_seq_t *seq, int queue, snd_seq_addr_t *dest);
int snd_seq_add_sync_std_master(snd_seq_t *seq, int queue, snd_seq_addr_t *dest, int format, int time_format, unsigned char *opt_info);
#define snd_seq_add_sync_master_clock(seq,q,dest) snd_seq_add_sync_std_master(seq, q, dest, SND_SEQ_SYNC_FMT_MIDI_CLOCK, 0, 0)
#define snd_seq_add_sync_master_mtc(seq,q,dest,tfmt) snd_seq_add_sync_std_master(seq, q, dest, SND_SEQ_SYNC_FMT_MTC, tfmt, 0)

int snd_seq_set_sync_slave(snd_seq_t *seq, int queue, snd_seq_addr_t *src, snd_seq_queue_sync_t *info);
int snd_seq_reset_sync_slave(snd_seq_t *seq, int queue, snd_seq_addr_t *src);

#endif

/* event routines */
snd_seq_event_t *snd_seq_create_event(void);
int snd_seq_free_event(snd_seq_event_t *ev);
ssize_t snd_seq_event_length(snd_seq_event_t *ev);
int snd_seq_event_output(snd_seq_t *handle, snd_seq_event_t *ev);
int snd_seq_event_output(snd_seq_t *handle, snd_seq_event_t *ev);
int snd_seq_event_output_buffer(snd_seq_t *handle, snd_seq_event_t *ev);
int snd_seq_event_output_direct(snd_seq_t *handle, snd_seq_event_t *ev);
int snd_seq_event_input(snd_seq_t *handle, snd_seq_event_t **ev);
int snd_seq_event_input_pending(snd_seq_t *seq, int fetch_sequencer);
int snd_seq_drain_output(snd_seq_t *handle);
int snd_seq_event_output_pending(snd_seq_t *seq);
int snd_seq_extract_output(snd_seq_t *handle, snd_seq_event_t **ev);
int snd_seq_drop_output(snd_seq_t *handle);
int snd_seq_drop_output_buffer(snd_seq_t *handle);
int snd_seq_drop_input(snd_seq_t *handle);
int snd_seq_drop_input_buffer(snd_seq_t *handle);
int snd_seq_remove_events(snd_seq_t *handle, snd_seq_remove_events_t *info);
/* misc */
void snd_seq_set_bit(int nr, void *array);
int snd_seq_change_bit(int nr, void *array);
int snd_seq_get_bit(int nr, void *array);

#ifdef __cplusplus
}
#endif

