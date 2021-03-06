/* packet-usb-audio.c
 *
 * usb audio dissector
 * Tomasz Mon 2012
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* the parsing of audio-specific descriptors is based on
   USB Device Class Definition for Audio Devices, Release 2.0 and
   USB Audio Device Class Specification for Basic Audio Devices, Release 1.0 */

#include "config.h"

#include <epan/packet.h>
#include <epan/expert.h>
#include <epan/reassemble.h>
#include "packet-usb.h"

/* XXX - we use the same macro for mpeg sections,
         can we put this in a common include file? */
#define USB_AUDIO_BCD44_TO_DEC(x)  ((((x)&0xf0) >> 4) * 10 + ((x)&0x0f))

void proto_register_usb_audio(void);
void proto_reg_handoff_usb_audio(void);

/* protocols and header fields */
static int proto_usb_audio = -1;
static int hf_midi_cable_number = -1;
static int hf_midi_code_index = -1;
static int hf_midi_event = -1;
static int hf_ac_if_desc_subtype = -1;
static int hf_ac_if_hdr_ver = -1;
static int hf_ac_if_hdr_total_len = -1;
static int hf_ac_if_hdr_bInCollection = -1;
static int hf_ac_if_hdr_if_num = -1;
static int hf_ac_if_hdr_category = -1;
static int hf_ac_if_hdr_controls = -1;
static int hf_ac_if_hdr_controls_latency = -1;
static int hf_ac_if_hdr_controls_rsv = -1;
static int hf_ac_if_input_terminalid = -1;
static int hf_ac_if_input_terminaltype = -1;
static int hf_ac_if_input_assocterminal = -1;
static int hf_ac_if_input_csourceid = -1;
static int hf_ac_if_input_nrchannels = -1;
static int hf_ac_if_input_wchannelconfig = -1;
static int hf_ac_if_input_wchannelconfig_d0 = -1;
static int hf_ac_if_input_wchannelconfig_d1 = -1;
static int hf_ac_if_input_wchannelconfig_d2 = -1;
static int hf_ac_if_input_wchannelconfig_d3 = -1;
static int hf_ac_if_input_wchannelconfig_d4 = -1;
static int hf_ac_if_input_wchannelconfig_d5 = -1;
static int hf_ac_if_input_wchannelconfig_d6 = -1;
static int hf_ac_if_input_wchannelconfig_d7 = -1;
static int hf_ac_if_input_wchannelconfig_d8 = -1;
static int hf_ac_if_input_wchannelconfig_d9 = -1;
static int hf_ac_if_input_wchannelconfig_d10 = -1;
static int hf_ac_if_input_wchannelconfig_d11 = -1;
static int hf_ac_if_input_wchannelconfig_rsv = -1;
static int hf_ac_if_input_bmchannelconfig = -1;
static int hf_ac_if_input_bmchannelconfig_d0 = -1;
static int hf_ac_if_input_bmchannelconfig_d1 = -1;
static int hf_ac_if_input_bmchannelconfig_d2 = -1;
static int hf_ac_if_input_bmchannelconfig_d3 = -1;
static int hf_ac_if_input_bmchannelconfig_d4 = -1;
static int hf_ac_if_input_bmchannelconfig_d5 = -1;
static int hf_ac_if_input_bmchannelconfig_d6 = -1;
static int hf_ac_if_input_bmchannelconfig_d7 = -1;
static int hf_ac_if_input_bmchannelconfig_d8 = -1;
static int hf_ac_if_input_bmchannelconfig_d9 = -1;
static int hf_ac_if_input_bmchannelconfig_d10 = -1;
static int hf_ac_if_input_bmchannelconfig_d11 = -1;
static int hf_ac_if_input_bmchannelconfig_d12 = -1;
static int hf_ac_if_input_bmchannelconfig_d13 = -1;
static int hf_ac_if_input_bmchannelconfig_d14 = -1;
static int hf_ac_if_input_bmchannelconfig_d15 = -1;
static int hf_ac_if_input_bmchannelconfig_d16 = -1;
static int hf_ac_if_input_bmchannelconfig_d17 = -1;
static int hf_ac_if_input_bmchannelconfig_d18 = -1;
static int hf_ac_if_input_bmchannelconfig_d19 = -1;
static int hf_ac_if_input_bmchannelconfig_d20 = -1;
static int hf_ac_if_input_bmchannelconfig_d21 = -1;
static int hf_ac_if_input_bmchannelconfig_d22 = -1;
static int hf_ac_if_input_bmchannelconfig_d23 = -1;
static int hf_ac_if_input_bmchannelconfig_d24 = -1;
static int hf_ac_if_input_bmchannelconfig_d25 = -1;
static int hf_ac_if_input_bmchannelconfig_d26 = -1;
static int hf_ac_if_input_bmchannelconfig_rsv = -1;
static int hf_ac_if_input_bmchannelconfig_d31 = -1;
static int hf_ac_if_input_channelnames = -1;
static int hf_ac_if_input_controls = -1;
static int hf_ac_if_input_controls_copy = -1;
static int hf_ac_if_input_controls_connector = -1;
static int hf_ac_if_input_controls_overload = -1;
static int hf_ac_if_input_controls_cluster = -1;
static int hf_ac_if_input_controls_underflow = -1;
static int hf_ac_if_input_controls_overflow = -1;
static int hf_ac_if_input_controls_rsv = -1;
static int hf_ac_if_input_terminal = -1;
static int hf_ac_if_output_terminalid = -1;
static int hf_ac_if_output_terminaltype = -1;
static int hf_ac_if_output_assocterminal = -1;
static int hf_ac_if_output_sourceid = -1;
static int hf_ac_if_output_clk_sourceid = -1;
static int hf_ac_if_output_controls = -1;
static int hf_ac_if_output_controls_copy = -1;
static int hf_ac_if_output_controls_connector = -1;
static int hf_ac_if_output_controls_overload = -1;
static int hf_ac_if_output_controls_underflow = -1;
static int hf_ac_if_output_controls_overflow = -1;
static int hf_ac_if_output_controls_rsv = -1;
static int hf_ac_if_output_terminal = -1;
static int hf_ac_if_fu_unitid = -1;
static int hf_ac_if_fu_sourceid = -1;
static int hf_ac_if_fu_controlsize = -1;
static int hf_ac_if_fu_controls = -1;
static int hf_ac_if_fu_control = -1;
static int hf_ac_if_fu_controls_d0 = -1;
static int hf_ac_if_fu_controls_d1 = -1;
static int hf_ac_if_fu_controls_d2 = -1;
static int hf_ac_if_fu_controls_d3 = -1;
static int hf_ac_if_fu_controls_d4 = -1;
static int hf_ac_if_fu_controls_d5 = -1;
static int hf_ac_if_fu_controls_d6 = -1;
static int hf_ac_if_fu_controls_d7 = -1;
static int hf_ac_if_fu_controls_d8 = -1;
static int hf_ac_if_fu_controls_d9 = -1;
static int hf_ac_if_fu_controls_rsv = -1;
static int hf_ac_if_fu_ifeature = -1;
static int hf_ac_if_mu_unitid = -1;
static int hf_ac_if_mu_nrinpins = -1;
static int hf_ac_if_mu_sourceid = -1;
static int hf_ac_if_mu_nrchannels = -1;
static int hf_ac_if_mu_channelconfig = -1;
static int hf_ac_if_mu_channelconfig_d0 = -1;
static int hf_ac_if_mu_channelconfig_d1 = -1;
static int hf_ac_if_mu_channelconfig_d2 = -1;
static int hf_ac_if_mu_channelconfig_d3 = -1;
static int hf_ac_if_mu_channelconfig_d4 = -1;
static int hf_ac_if_mu_channelconfig_d5 = -1;
static int hf_ac_if_mu_channelconfig_d6 = -1;
static int hf_ac_if_mu_channelconfig_d7 = -1;
static int hf_ac_if_mu_channelconfig_d8 = -1;
static int hf_ac_if_mu_channelconfig_d9 = -1;
static int hf_ac_if_mu_channelconfig_d10 = -1;
static int hf_ac_if_mu_channelconfig_d11 = -1;
static int hf_ac_if_mu_channelconfig_rsv = -1;
static int hf_ac_if_mu_channelnames = -1;
static int hf_ac_if_mu_controls = -1;
static int hf_ac_if_mu_imixer = -1;
static int hf_ac_if_clksrc_id = -1;
static int hf_ac_if_clksrc_attr = -1;
static int hf_ac_if_clksrc_attr_type = -1;
static int hf_ac_if_clksrc_attr_d2 = -1;
static int hf_ac_if_clksrc_attr_rsv = -1;
static int hf_ac_if_clksrc_controls = -1;
static int hf_ac_if_clksrc_controls_freq = -1;
static int hf_ac_if_clksrc_controls_validity = -1;
static int hf_ac_if_clksrc_controls_rsv = -1;
static int hf_ac_if_clksrc_assocterminal = -1;
static int hf_ac_if_clksrc_clocksource = -1;
static int hf_ac_if_clksel_id = -1;
static int hf_ac_if_clksel_nrpins = -1;
static int hf_ac_if_clksel_sourceid = -1;
static int hf_ac_if_clksel_controls = -1;
static int hf_ac_if_clksel_controls_clksel = -1;
static int hf_ac_if_clksel_controls_rsv = -1;
static int hf_ac_if_clksel_clockselector = -1;
static int hf_as_if_desc_subtype = -1;
static int hf_as_if_gen_term_id = -1;
static int hf_as_if_gen_delay = -1;
static int hf_as_if_gen_format = -1;
static int hf_as_if_ft_formattype = -1;
static int hf_as_if_ft_maxbitrate = -1;
static int hf_as_if_ft_nrchannels = -1;
static int hf_as_if_ft_subframesize = -1;
static int hf_as_if_ft_bitresolution = -1;
static int hf_as_if_ft_samplesperframe = -1;
static int hf_as_if_ft_samfreqtype = -1;
static int hf_as_if_ft_lowersamfreq = -1;
static int hf_as_if_ft_uppersamfreq = -1;
static int hf_as_if_ft_samfreq = -1;
static int hf_as_ep_desc_subtype = -1;

static reassembly_table midi_data_reassembly_table;

static gint ett_usb_audio      = -1;
static gint ett_usb_audio_desc = -1;

static gint ett_ac_if_hdr_controls = -1;
static gint ett_ac_if_fu_controls = -1;
static gint ett_ac_if_fu_controls0 = -1;
static gint ett_ac_if_fu_controls1 = -1;
static gint ett_ac_if_input_wchannelconfig = -1;
static gint ett_ac_if_input_bmchannelconfig = -1;
static gint ett_ac_if_input_controls = -1;
static gint ett_ac_if_output_controls = -1;
static gint ett_ac_if_mu_channelconfig = -1;
static gint ett_ac_if_clksrc_attr = -1;
static gint ett_ac_if_clksrc_controls = -1;
static gint ett_ac_if_clksel_controls = -1;

static dissector_handle_t sysex_handle;
static dissector_handle_t usb_audio_bulk_handle;

#define AUDIO_IF_SUBCLASS_UNDEFINED        0x00
#define AUDIO_IF_SUBCLASS_AUDIOCONTROL     0x01
#define AUDIO_IF_SUBCLASS_AUDIOSTREAMING   0x02
#define AUDIO_IF_SUBCLASS_MIDISTREAMING    0x03

#if 0
static const value_string usb_audio_subclass_vals[] = {
    {AUDIO_IF_SUBCLASS_UNDEFINED,          "SUBCLASS_UNDEFINED"},
    {AUDIO_IF_SUBCLASS_AUDIOCONTROL,       "AUDIOCONSTROL"},
    {AUDIO_IF_SUBCLASS_AUDIOSTREAMING,     "AUDIOSTREAMING"},
    {AUDIO_IF_SUBCLASS_MIDISTREAMING,      "MIDISTREAMING"},
    {0, NULL}
};
#endif

static const value_string code_index_vals[] = {
    { 0x0, "Miscellaneous (Reserved)" },
    { 0x1, "Cable events (Reserved)" },
    { 0x2, "Two-byte System Common message" },
    { 0x3, "Three-byte System Common message" },
    { 0x4, "SysEx starts or continues" },
    { 0x5, "SysEx ends with following single byte/Single-byte System Common Message" },
    { 0x6, "SysEx ends with following two bytes" },
    { 0x7, "SysEx ends with following three bytes" },
    { 0x8, "Note-off" },
    { 0x9, "Note-on" },
    { 0xA, "Poly-KeyPress" },
    { 0xB, "Control Change" },
    { 0xC, "Program Change" },
    { 0xD, "Channel Pressure" },
    { 0xE, "PitchBend Change" },
    { 0xF, "Single Byte" },
    { 0, NULL }
};

/* USB audio specification, section A.8 */
#define CS_INTERFACE       0x24
#define CS_ENDPOINT        0x25

static const value_string aud_descriptor_type_vals[] = {
        {CS_INTERFACE, "audio class interface"},
        {CS_ENDPOINT,  "audio class endpoint"},
        {0,NULL}
};
static value_string_ext aud_descriptor_type_vals_ext =
    VALUE_STRING_EXT_INIT(aud_descriptor_type_vals);

#define AC_SUBTYPE_HEADER                0x01
#define AC_SUBTYPE_INPUT_TERMINAL        0x02
#define AC_SUBTYPE_OUTPUT_TERMINAL       0x03
#define AC_SUBTYPE_MIXER_UNIT            0x04
#define AC_SUBTYPE_SELECTOR_UNIT         0x05
#define AC_SUBTYPE_FEATURE_UNIT          0x06
#define AC_SUBTYPE_EFFECT_UNIT           0x07
#define AC_SUBTYPE_PROCESSING_UNIT       0x08
#define AC_SUBTYPE_EXTENSION_UNIT        0x09
#define AC_SUBTYPE_CLOCK_SOURCE          0x0A
#define AC_SUBTYPE_CLOCK_SELECTOR        0x0B
#define AC_SUBTYPE_CLOCK_MULTIPLIER      0x0C
#define AC_SUBTYPE_SAMPLE_RATE_CONVERTER 0x0D

static const value_string ac_subtype_vals[] = {
    {AC_SUBTYPE_HEADER,                "Header Descriptor"},
    {AC_SUBTYPE_INPUT_TERMINAL,        "Input terminal descriptor"},
    {AC_SUBTYPE_OUTPUT_TERMINAL,       "Output terminal descriptor"},
    {AC_SUBTYPE_MIXER_UNIT,            "Mixer unit descriptor"},
    {AC_SUBTYPE_SELECTOR_UNIT,         "Selector unit descriptor"},
    {AC_SUBTYPE_FEATURE_UNIT,          "Feature unit descriptor"},
    {AC_SUBTYPE_EFFECT_UNIT,           "Effect unit descriptor"},
    {AC_SUBTYPE_PROCESSING_UNIT,       "Processing unit descriptor"},
    {AC_SUBTYPE_EXTENSION_UNIT,        "Extension unit descriptor"},
    {AC_SUBTYPE_CLOCK_SOURCE,          "Clock source descriptor"},
    {AC_SUBTYPE_CLOCK_SELECTOR,        "Clock selector descriptor"},
    {AC_SUBTYPE_CLOCK_MULTIPLIER,      "Clock multiplier descriptor"},
    {AC_SUBTYPE_SAMPLE_RATE_CONVERTER, "Sample rate converter descriptor"},
    {0,NULL}
};
static value_string_ext ac_subtype_vals_ext =
    VALUE_STRING_EXT_INIT(ac_subtype_vals);

#define AS_SUBTYPE_GENERAL         0x01
#define AS_SUBTYPE_FORMAT_TYPE     0x02
#define AS_SUBTYPE_ENCODER         0x03

static const value_string as_subtype_vals[] = {
    {AS_SUBTYPE_GENERAL,     "General AS Descriptor"},
    {AS_SUBTYPE_FORMAT_TYPE, "Format type descriptor"},
    {AS_SUBTYPE_ENCODER,     "Encoder descriptor"},
    {0,NULL}
};
static value_string_ext as_subtype_vals_ext =
    VALUE_STRING_EXT_INIT(as_subtype_vals);

/* Table A-7: Audio Function Category Codes */
static const value_string audio_function_categories_vals[] = {
    {0x00, "Undefinied"},
    {0x01, "Desktop speaker"},
    {0x02, "Home theater"},
    {0x03, "Microphone"},
    {0x04, "Headset"},
    {0x05, "Telephone"},
    {0x06, "Converter"},
    {0x07, "Voice/Sound recorder"},
    {0x08, "I/O box"},
    {0x09, "Musical instrument"},
    {0x0A, "Pro-audio"},
    {0x0B, "Audio/Video"},
    {0x0C, "Control panel"},
    {0xFF, "Other"},
    {0,NULL}
};
static value_string_ext audio_function_categories_vals_ext =
    VALUE_STRING_EXT_INIT(audio_function_categories_vals);

/* Described in 4.7.2 Class-Specific AC Interface Descriptor */
static const value_string controls_capabilities_vals[] = {
    {0x00, "Not present"},
    {0x01, "Present, read-only"},
    {0x02, "Value not allowed"},
    {0x03, "Host programmable"},
    {0,NULL}
};
static value_string_ext controls_capabilities_vals_ext =
    VALUE_STRING_EXT_INIT(controls_capabilities_vals);

/* Described in 4.7.2.1 Clock Source Descriptor */
static const value_string clock_types_vals[] = {
    {0x00, "External clock"},
    {0x01, "Internal fixed clock"},
    {0x02, "Internal variable clock"},
    {0x03, "Internal programmable clock"},
    {0,NULL}
};

static const value_string clock_sync_vals[] = {
    {0x00, "Free running"},
    {0x01, "Synchronized to the Start of Frame"},
    {0,NULL}
};

/* From http://www.usb.org/developers/docs/devclass_docs/termt10.pdf */
static const value_string terminal_types_vals[] = {
    /* USB Terminal Types */
    {0x0100, "USB Undefined"},
    {0x0101, "USB Streaming"},
    {0x01FF, "USB vendor specific"},
    /* Input Terminal Tyoes */
    {0x0200, "Input Undefined"},
    {0x0201, "Microphone"},
    {0x0202, "Desktop Microphone"},
    {0x0203, "Personal microphone"},
    {0x0204, "Omni-directional icrophone"},
    {0x0205, "Microphone array"},
    {0x0206, "Processing microphone array"},
    {0x0300, "Output Undefined"},
    {0x0301, "Speaker"},
    {0x0302, "Headphones"},
    {0x0303, "Head Mounted Display Audio"},
    {0x0304, "Desktop speaker"},
    {0x0305, "Room speaker"},
    {0x0306, "Communication speaker"},
    {0x0307, "Low frequency effects speaker"},
    /* Bi-directional Terminal Types */
    {0x0400, "Bi-directional Undefined"},
    {0x0401, "Handset"},
    {0x0402, "Headset"},
    {0x0403, "Speakerphone, no echoreduction"},
    {0x0404, "Echo-suppressing speakerphone"},
    {0x0405, "Echo-canceling speakerphone"},
    /* Telephony Terminal Types */
    {0x0500, "Telephony Undefined"},
    {0x0501, "Phone line"},
    {0x0502, "Telephone"},
    {0x0503, "Down Line Pone"},
    /* External Terminal Types */
    {0x0600, "External Undefined"},
    {0x0601, "Analog connector"},
    {0x0602, "Digital audio interface"},
    {0x0603, "Line connector"},
    {0x0604, "Legacy audio connector"},
    {0x0605, "S/PDIF interface"},
    {0x0606, "1394 DA stream"},
    {0x0607, "1394 DV stream soudtrack"},
    /* Embedded Function Terminal Types */
    {0x0700, "Embedded Undefined"},
    {0x0701, "Level Calibration Noise Source"},
    {0x0702, "Equalization Noise"},
    {0x0703, "CD player"},
    {0x0704, "DAT"},
    {0x0705, "DCC"},
    {0x0706, "MiniDisk"},
    {0x0707, "Analog Tape"},
    {0x0708, "Phonograph"},
    {0x0709, "VCR Audio"},
    {0x070A, "Video Disc Audio"},
    {0x070B, "DVD Audio"},
    {0x070C, "TV Tuner Audio"},
    {0x070D, "Satellite Receiver Audio"},
    {0x070E, "cable Tuner Audio"},
    {0x070F, "DSS Audio"},
    {0x0710, "Radio Receiver"},
    {0x0711, "Radio Transmitter"},
    {0x0712, "Multi-track Recorder"},
    {0x0713, "Synthesizer"},
    {0,NULL}
};
static value_string_ext terminal_types_vals_ext =
    VALUE_STRING_EXT_INIT(terminal_types_vals);

typedef struct _audio_conv_info_t {
    /* the major version of the USB audio class specification,
       taken from the AC header descriptor */
    guint8 ver_major;
} audio_conv_info_t;

static int hf_sysex_msg_fragments = -1;
static int hf_sysex_msg_fragment = -1;
static int hf_sysex_msg_fragment_overlap = -1;
static int hf_sysex_msg_fragment_overlap_conflicts = -1;
static int hf_sysex_msg_fragment_multiple_tails = -1;
static int hf_sysex_msg_fragment_too_long_fragment = -1;
static int hf_sysex_msg_fragment_error = -1;
static int hf_sysex_msg_fragment_count = -1;
static int hf_sysex_msg_reassembled_in = -1;
static int hf_sysex_msg_reassembled_length = -1;
static int hf_sysex_msg_reassembled_data = -1;

static gint ett_sysex_msg_fragment = -1;
static gint ett_sysex_msg_fragments = -1;

static expert_field ei_usb_audio_undecoded = EI_INIT;

static const fragment_items sysex_msg_frag_items = {
    /* Fragment subtrees */
    &ett_sysex_msg_fragment,
    &ett_sysex_msg_fragments,
    /* Fragment fields */
    &hf_sysex_msg_fragments,
    &hf_sysex_msg_fragment,
    &hf_sysex_msg_fragment_overlap,
    &hf_sysex_msg_fragment_overlap_conflicts,
    &hf_sysex_msg_fragment_multiple_tails,
    &hf_sysex_msg_fragment_too_long_fragment,
    &hf_sysex_msg_fragment_error,
    &hf_sysex_msg_fragment_count,
    /* Reassembled in field */
    &hf_sysex_msg_reassembled_in,
    /* Reassembled length field */
    &hf_sysex_msg_reassembled_length,
    &hf_sysex_msg_reassembled_data,
    /* Tag */
    "Message fragments"
};

static inline gboolean
is_sysex_code(guint8 code)
{
    return (code == 0x04 || code == 0x05 || code == 0x06 || code == 0x07);
}

static gboolean
is_last_sysex_packet_in_tvb(tvbuff_t *tvb, gint offset)
{
    gboolean last   = TRUE;
    gint     length = tvb_reported_length(tvb);

    offset += 4;
    while (offset < length)
    {
        guint8 code = tvb_get_guint8(tvb, offset);
        code &= 0x0F;

        if (is_sysex_code(code))
        {
            last = FALSE;
            break;
        }

        offset += 4;
    }

    return last;
}

static void
dissect_usb_midi_event(tvbuff_t *tvb, packet_info *pinfo,
                       proto_tree *usb_audio_tree, proto_tree *parent_tree,
                       gint offset)
{
    guint8      code;
    guint8      cable;
    gboolean    save_fragmented;
    proto_tree *tree = NULL;

    col_set_str(pinfo->cinfo, COL_INFO, "USB-MIDI Event Packets");

    code = tvb_get_guint8(tvb, offset);
    cable = (code & 0xF0) >> 4;
    code &= 0x0F;

    if (parent_tree)
    {
        proto_item *ti;

        ti = proto_tree_add_protocol_format(usb_audio_tree, proto_usb_audio, tvb, offset, 4, "USB Midi Event Packet");
        tree = proto_item_add_subtree(ti, ett_usb_audio);
        proto_tree_add_item(tree, hf_midi_cable_number, tvb, offset, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(tree, hf_midi_code_index, tvb, offset, 1, ENC_BIG_ENDIAN);
        proto_tree_add_item(tree, hf_midi_event, tvb, offset+1, 3, ENC_BIG_ENDIAN);
    }

    save_fragmented = pinfo->fragmented;

    /* Reassemble SysEx commands */
    if (is_sysex_code(code))
    {
        tvbuff_t* new_tvb = NULL;
        fragment_head *frag_sysex_msg = NULL;

        pinfo->fragmented = TRUE;

        if (code == 0x04)
        {
            frag_sysex_msg = fragment_add_seq_next(&midi_data_reassembly_table,
                tvb, offset+1,
                pinfo,
                cable, /* ID for fragments belonging together */
                NULL,
                3,
                TRUE);
        }
        else
        {
            frag_sysex_msg = fragment_add_seq_next(&midi_data_reassembly_table,
                tvb, offset+1,
                pinfo,
                cable, /* ID for fragments belonging together */
                NULL,
                (gint)(code - 4),
                FALSE);
        }

        if (is_last_sysex_packet_in_tvb(tvb, offset))
        {
            new_tvb = process_reassembled_data(tvb, offset+1, pinfo,
                "Reassembled Message", frag_sysex_msg, &sysex_msg_frag_items,
                NULL, usb_audio_tree);

            if (code != 0x04) { /* Reassembled */
                col_append_str(pinfo->cinfo, COL_INFO,
                        " (SysEx Reassembled)");
            } else { /* Not last packet of reassembled Short Message */
                col_append_str(pinfo->cinfo, COL_INFO,
                        " (SysEx fragment)");
            }

            if (new_tvb)
            {
                call_dissector(sysex_handle, new_tvb, pinfo, parent_tree);
            }
        }
    }

    pinfo->fragmented = save_fragmented;
}


/* dissect the body of an AC interface header descriptor
   return the number of bytes dissected (which may be smaller than the
   body's length) */
static gint
dissect_ac_if_hdr_body(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info)
{
    gint     offset_start;
    guint16  bcdADC;
    guint8   ver_major;
    double   ver;
    guint8   if_in_collection, i;
    audio_conv_info_t *audio_conv_info;

    static const int *bm_controls[] = {
        &hf_ac_if_hdr_controls_latency,
        &hf_ac_if_hdr_controls_rsv,
        NULL
    };

    offset_start = offset;

    bcdADC = tvb_get_letohs(tvb, offset);
    ver_major = USB_AUDIO_BCD44_TO_DEC(bcdADC>>8);
    ver = ver_major + USB_AUDIO_BCD44_TO_DEC(bcdADC&0xFF) / 100.0;

    proto_tree_add_double_format_value(tree, hf_ac_if_hdr_ver,
            tvb, offset, 2, ver, "%2.2f", ver);
    audio_conv_info = (audio_conv_info_t *)usb_conv_info->class_data;
    if(!audio_conv_info) {
        audio_conv_info = wmem_new(wmem_file_scope(), audio_conv_info_t);
        usb_conv_info->class_data = audio_conv_info;
        usb_conv_info->class_data_type = USB_CONV_AUDIO;
        /* XXX - set reasonable default values for all components
           that are not filled in by this function */
    } else if (usb_conv_info->class_data_type != USB_CONV_AUDIO) {
        /* Don't dissect if another USB type is in the conversation */
        return 0;
    }
    audio_conv_info->ver_major = ver_major;
    offset += 2;

    /* version 1 refers to the Basic Audio Device specification,
       version 2 is the Audio Device class specification, see above */
    if (ver_major==1) {
        proto_tree_add_item(tree, hf_ac_if_hdr_total_len,
                tvb, offset, 2, ENC_LITTLE_ENDIAN);
        offset += 2;
        if_in_collection = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(tree, hf_ac_if_hdr_bInCollection,
                tvb, offset, 1, ENC_LITTLE_ENDIAN);
        offset++;

        for (i=0; i<if_in_collection; i++) {
            proto_tree_add_item(tree, hf_ac_if_hdr_if_num,
                    tvb, offset, 1, ENC_LITTLE_ENDIAN);
            offset++;
        }
    }
    else if (ver_major==2) {
        proto_tree_add_item(tree, hf_ac_if_hdr_category,
                tvb, offset, 1, ENC_LITTLE_ENDIAN);
        offset++;
        proto_tree_add_item(tree, hf_ac_if_hdr_total_len,
                tvb, offset, 2, ENC_LITTLE_ENDIAN);
        offset += 2;
        proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_hdr_controls,
                ett_ac_if_hdr_controls, bm_controls, ENC_LITTLE_ENDIAN);
        offset++;
    }

    return offset-offset_start;
}

static gint
dissect_ac_if_input_terminal(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info)
{
    audio_conv_info_t *audio_conv_info;
    gint               offset_start;

    static const int *input_wchannelconfig[] = {
        &hf_ac_if_input_wchannelconfig_d0,
        &hf_ac_if_input_wchannelconfig_d1,
        &hf_ac_if_input_wchannelconfig_d2,
        &hf_ac_if_input_wchannelconfig_d3,
        &hf_ac_if_input_wchannelconfig_d4,
        &hf_ac_if_input_wchannelconfig_d5,
        &hf_ac_if_input_wchannelconfig_d6,
        &hf_ac_if_input_wchannelconfig_d7,
        &hf_ac_if_input_wchannelconfig_d8,
        &hf_ac_if_input_wchannelconfig_d9,
        &hf_ac_if_input_wchannelconfig_d10,
        &hf_ac_if_input_wchannelconfig_d11,
        &hf_ac_if_input_wchannelconfig_rsv,
        NULL
    };

    static const int *input_bmchannelconfig[] = {
        &hf_ac_if_input_bmchannelconfig_d0,
        &hf_ac_if_input_bmchannelconfig_d1,
        &hf_ac_if_input_bmchannelconfig_d2,
        &hf_ac_if_input_bmchannelconfig_d3,
        &hf_ac_if_input_bmchannelconfig_d4,
        &hf_ac_if_input_bmchannelconfig_d5,
        &hf_ac_if_input_bmchannelconfig_d6,
        &hf_ac_if_input_bmchannelconfig_d7,
        &hf_ac_if_input_bmchannelconfig_d8,
        &hf_ac_if_input_bmchannelconfig_d9,
        &hf_ac_if_input_bmchannelconfig_d10,
        &hf_ac_if_input_bmchannelconfig_d11,
        &hf_ac_if_input_bmchannelconfig_d12,
        &hf_ac_if_input_bmchannelconfig_d13,
        &hf_ac_if_input_bmchannelconfig_d14,
        &hf_ac_if_input_bmchannelconfig_d15,
        &hf_ac_if_input_bmchannelconfig_d16,
        &hf_ac_if_input_bmchannelconfig_d17,
        &hf_ac_if_input_bmchannelconfig_d18,
        &hf_ac_if_input_bmchannelconfig_d19,
        &hf_ac_if_input_bmchannelconfig_d20,
        &hf_ac_if_input_bmchannelconfig_d21,
        &hf_ac_if_input_bmchannelconfig_d22,
        &hf_ac_if_input_bmchannelconfig_d23,
        &hf_ac_if_input_bmchannelconfig_d24,
        &hf_ac_if_input_bmchannelconfig_d25,
        &hf_ac_if_input_bmchannelconfig_d26,
        &hf_ac_if_input_bmchannelconfig_rsv,
        &hf_ac_if_input_bmchannelconfig_d31,
        NULL
    };

    static const int *controls[] = {
        &hf_ac_if_input_controls_copy,
        &hf_ac_if_input_controls_connector,
        &hf_ac_if_input_controls_overload,
        &hf_ac_if_input_controls_cluster,
        &hf_ac_if_input_controls_underflow,
        &hf_ac_if_input_controls_overflow,
        &hf_ac_if_input_controls_rsv,
        NULL
    };

    /* the caller has already checked that usb_conv_info!=NULL */
    audio_conv_info = (audio_conv_info_t *)usb_conv_info->class_data;
    if (!audio_conv_info)
        return 0;

    /* do not try to dissect unknown versions */
    if (!((audio_conv_info->ver_major==1) || (audio_conv_info->ver_major==2)))
        return 0;

    offset_start = offset;

    proto_tree_add_item(tree, hf_ac_if_input_terminalid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_input_terminaltype, tvb, offset, 2, ENC_LITTLE_ENDIAN);
    offset += 2;

    proto_tree_add_item(tree, hf_ac_if_input_assocterminal, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    if (audio_conv_info->ver_major == 2) {
        proto_tree_add_item(tree, hf_ac_if_input_csourceid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
        offset += 1;
    }

    proto_tree_add_item(tree, hf_ac_if_input_nrchannels, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    if (audio_conv_info->ver_major==1) {
        proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_input_wchannelconfig, ett_ac_if_input_wchannelconfig, input_wchannelconfig, ENC_LITTLE_ENDIAN);
        offset += 2;
    } else if (audio_conv_info->ver_major==2) {
        proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_input_bmchannelconfig, ett_ac_if_input_bmchannelconfig, input_bmchannelconfig, ENC_LITTLE_ENDIAN);
        offset += 4;
    }

    proto_tree_add_item(tree, hf_ac_if_input_channelnames, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    if (audio_conv_info->ver_major==2) {
        proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_input_controls, ett_ac_if_input_controls, controls, ENC_LITTLE_ENDIAN);
        offset += 2;
    }

    proto_tree_add_item(tree, hf_ac_if_input_terminal, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    return offset-offset_start;
}

static gint
dissect_ac_if_output_terminal(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info)
{
    audio_conv_info_t *audio_conv_info;
    gint               offset_start;

    static const int *controls[] = {
        &hf_ac_if_output_controls_copy,
        &hf_ac_if_output_controls_connector,
        &hf_ac_if_output_controls_overload,
        &hf_ac_if_output_controls_underflow,
        &hf_ac_if_output_controls_overflow,
        &hf_ac_if_output_controls_rsv,
        NULL
    };

    /* the caller has already checked that usb_conv_info!=NULL */
    audio_conv_info = (audio_conv_info_t *)usb_conv_info->class_data;
    if (!audio_conv_info)
        return 0;

    /* do not try to dissect unknown versions */
    if (!((audio_conv_info->ver_major==1) || (audio_conv_info->ver_major==2)))
        return 0;

    offset_start = offset;

    proto_tree_add_item(tree, hf_ac_if_output_terminalid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_output_terminaltype, tvb, offset, 2, ENC_LITTLE_ENDIAN);
    offset += 2;

    proto_tree_add_item(tree, hf_ac_if_output_assocterminal, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_output_sourceid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    if (audio_conv_info->ver_major==2) {
        proto_tree_add_item(tree, hf_ac_if_output_clk_sourceid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
        offset += 1;

        proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_output_controls, ett_ac_if_output_controls, controls, ENC_LITTLE_ENDIAN);
        offset += 2;
    }

    proto_tree_add_item(tree, hf_ac_if_output_terminal, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    return offset-offset_start;
}

static gint
dissect_ac_if_feature_unit(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info _U_, guint8 desc_len)
{
    gint     offset_start;
    guint8 controlsize;
    proto_tree *bitmap_tree;
    proto_item *ti;

    static const int *fu_controls0[] = {
        &hf_ac_if_fu_controls_d0,
        &hf_ac_if_fu_controls_d1,
        &hf_ac_if_fu_controls_d2,
        &hf_ac_if_fu_controls_d3,
        &hf_ac_if_fu_controls_d4,
        &hf_ac_if_fu_controls_d5,
        &hf_ac_if_fu_controls_d6,
        &hf_ac_if_fu_controls_d7,
        NULL };

    static const int *fu_controls1[] = {
        &hf_ac_if_fu_controls_d8,
        &hf_ac_if_fu_controls_d9,
        &hf_ac_if_fu_controls_rsv,
        NULL };

    offset_start = offset;

    proto_tree_add_item(tree, hf_ac_if_fu_unitid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_fu_sourceid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_fu_controlsize, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    controlsize = tvb_get_guint8(tvb, offset) + 1;
    offset += 1;

    ti = proto_tree_add_item(tree, hf_ac_if_fu_controls, tvb, offset, controlsize, ENC_NA);
    bitmap_tree = proto_item_add_subtree(ti, ett_ac_if_fu_controls);

    proto_tree_add_bitmask(bitmap_tree, tvb, offset, hf_ac_if_fu_control, ett_ac_if_fu_controls0, fu_controls0, ENC_LITTLE_ENDIAN);

    if(controlsize >= 1){
        proto_tree_add_bitmask(bitmap_tree, tvb, offset + 1, hf_ac_if_fu_control, ett_ac_if_fu_controls1, fu_controls1, ENC_LITTLE_ENDIAN);
    }

    offset += controlsize;

    if(offset < desc_len){
        proto_tree_add_item(tree, hf_ac_if_fu_ifeature, tvb, offset, 1, ENC_LITTLE_ENDIAN);
        offset += 1;
    }

    return offset-offset_start;
}

static gint
dissect_ac_if_mixed_unit(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info _U_)
{
    gint     offset_start;
    guint8 nrinpins;

    static const int *mu_channelconfig[] = {
        &hf_ac_if_mu_channelconfig_d0,
        &hf_ac_if_mu_channelconfig_d1,
        &hf_ac_if_mu_channelconfig_d2,
        &hf_ac_if_mu_channelconfig_d3,
        &hf_ac_if_mu_channelconfig_d4,
        &hf_ac_if_mu_channelconfig_d5,
        &hf_ac_if_mu_channelconfig_d6,
        &hf_ac_if_mu_channelconfig_d7,
        &hf_ac_if_mu_channelconfig_d8,
        &hf_ac_if_mu_channelconfig_d9,
        &hf_ac_if_mu_channelconfig_d10,
        &hf_ac_if_mu_channelconfig_d11,
        &hf_ac_if_mu_channelconfig_rsv,
        NULL
    };

    offset_start = offset;

    proto_tree_add_item(tree, hf_ac_if_mu_unitid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_mu_nrinpins, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    nrinpins = tvb_get_guint8(tvb, offset);
    offset += 1;

    while(nrinpins){
        proto_tree_add_item(tree, hf_ac_if_mu_sourceid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
        nrinpins--;
        offset += 1;
    }

    proto_tree_add_item(tree, hf_ac_if_mu_nrchannels, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_mu_channelconfig, ett_ac_if_mu_channelconfig, mu_channelconfig, ENC_LITTLE_ENDIAN);
    offset += 2;

    proto_tree_add_item(tree, hf_ac_if_mu_channelnames, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_mu_controls, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_mu_imixer, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    return offset-offset_start;
}

static gint
dissect_ac_if_clock_source(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info _U_)
{
    gint offset_start;
    static const int *cs_attributes[] = {
        &hf_ac_if_clksrc_attr_type,
        &hf_ac_if_clksrc_attr_d2,
        &hf_ac_if_clksrc_attr_rsv,
        NULL
    };
    static const int *cs_controls[] = {
        &hf_ac_if_clksrc_controls_freq,
        &hf_ac_if_clksrc_controls_validity,
        &hf_ac_if_clksrc_controls_rsv,
        NULL
    };
    offset_start = offset;

    proto_tree_add_item(tree, hf_ac_if_clksrc_id, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_clksrc_attr, ett_ac_if_clksrc_attr, cs_attributes, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_clksrc_controls, ett_ac_if_clksrc_controls, cs_controls, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_clksrc_assocterminal, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_clksrc_clocksource, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    return offset-offset_start;
}

static gint
dissect_ac_if_clock_selector(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info _U_)
{
    gint   offset_start;
    guint8 nrinpins;
    static const int *cs_controls[] = {
        &hf_ac_if_clksel_controls_clksel,
        &hf_ac_if_clksel_controls_rsv,
        NULL
    };
    offset_start = offset;

    proto_tree_add_item(tree, hf_ac_if_clksel_id, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_clksel_nrpins, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    nrinpins = tvb_get_guint8(tvb, offset);
    offset += 1;

    while (nrinpins) {
        proto_tree_add_item(tree, hf_ac_if_clksel_sourceid, tvb, offset, 1, ENC_LITTLE_ENDIAN);
        nrinpins--;
        offset += 1;
    }

    proto_tree_add_bitmask(tree, tvb, offset, hf_ac_if_clksel_controls, ett_ac_if_clksel_controls, cs_controls, ENC_LITTLE_ENDIAN);
    offset += 1;

    proto_tree_add_item(tree, hf_ac_if_clksel_clockselector, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    offset += 1;

    return offset-offset_start;
}

static gint
dissect_as_if_general_body(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info)
{
    audio_conv_info_t *audio_conv_info;
    gint               offset_start;

    /* the caller has already checked that usb_conv_info!=NULL */
    audio_conv_info = (audio_conv_info_t *)usb_conv_info->class_data;
    if (!audio_conv_info)
        return 0;

    offset_start = offset;

    if (audio_conv_info->ver_major==1) {
        proto_tree_add_item(tree, hf_as_if_gen_term_id,
                tvb, offset, 1, ENC_LITTLE_ENDIAN);
        offset++;
        proto_tree_add_item(tree, hf_as_if_gen_delay,
                tvb, offset, 1, ENC_LITTLE_ENDIAN);
        offset++;
        proto_tree_add_item(tree, hf_as_if_gen_format,
                tvb, offset, 2, ENC_LITTLE_ENDIAN);
        offset += 2;
    }

    return offset-offset_start;
}

static gint
dissect_as_if_format_type_body(tvbuff_t *tvb, gint offset, packet_info *pinfo _U_,
        proto_tree *tree, usb_conv_info_t *usb_conv_info)
{
    audio_conv_info_t *audio_conv_info;
    gint               offset_start;
    guint8 SamFreqType;
    guint8 format_type;

    /* the caller has already checked that usb_conv_info!=NULL */
    audio_conv_info = (audio_conv_info_t *)usb_conv_info->class_data;
    if (!audio_conv_info)
        return 0;

    offset_start = offset;

    proto_tree_add_item(tree, hf_as_if_ft_formattype, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    format_type = tvb_get_guint8(tvb, offset);
    offset++;


    switch(format_type){
        case 1:
            proto_tree_add_item(tree, hf_as_if_ft_nrchannels, tvb, offset, 1, ENC_LITTLE_ENDIAN);
            offset += 1;

            proto_tree_add_item(tree, hf_as_if_ft_subframesize, tvb, offset, 1, ENC_LITTLE_ENDIAN);
            offset += 1;

            proto_tree_add_item(tree, hf_as_if_ft_bitresolution, tvb, offset, 1, ENC_LITTLE_ENDIAN);
            offset += 1;

            proto_tree_add_item(tree, hf_as_if_ft_samfreqtype, tvb, offset, 1, ENC_LITTLE_ENDIAN);
            SamFreqType = tvb_get_guint8(tvb, offset);
            offset++;

            if(SamFreqType == 0){
                proto_tree_add_item(tree, hf_as_if_ft_lowersamfreq, tvb, offset, 3, ENC_LITTLE_ENDIAN);
                offset += 3;
                proto_tree_add_item(tree, hf_as_if_ft_uppersamfreq, tvb, offset, 3, ENC_LITTLE_ENDIAN);
                offset += 3;
            }else {
                while(SamFreqType){
                    proto_tree_add_item(tree, hf_as_if_ft_samfreq, tvb, offset, 3, ENC_LITTLE_ENDIAN);
                    offset += 3;
                    SamFreqType--;
                }
            }
        break;
        case 2:
            proto_tree_add_item(tree, hf_as_if_ft_maxbitrate, tvb, offset, 2, ENC_LITTLE_ENDIAN);
            offset += 2;

            proto_tree_add_item(tree, hf_as_if_ft_samplesperframe, tvb, offset, 2, ENC_LITTLE_ENDIAN);
            offset += 2;

            proto_tree_add_item(tree, hf_as_if_ft_samfreqtype, tvb, offset, 1, ENC_LITTLE_ENDIAN);
            SamFreqType = tvb_get_guint8(tvb, offset);
            offset++;

            if(SamFreqType == 0){
                proto_tree_add_item(tree, hf_as_if_ft_lowersamfreq, tvb, offset, 3, ENC_LITTLE_ENDIAN);
                offset += 3;
                proto_tree_add_item(tree, hf_as_if_ft_uppersamfreq, tvb, offset, 3, ENC_LITTLE_ENDIAN);
                offset += 3;
            }else {
                while(SamFreqType){
                    proto_tree_add_item(tree, hf_as_if_ft_samfreq, tvb, offset, 3, ENC_LITTLE_ENDIAN);
                    offset += 3;
                    SamFreqType--;
                }
            }
        break;
        default:
        break;
    }

    return offset-offset_start;
}

static gint
dissect_usb_audio_descriptor(tvbuff_t *tvb, packet_info *pinfo,
        proto_tree *tree, void *data)
{
    gint             offset = 0;
    gint             bytes_dissected = 0;
    usb_conv_info_t *usb_conv_info;
    proto_tree       *desc_tree = NULL;
    proto_item       *desc_tree_item;
    guint8           desc_len;
    guint8           desc_type;
    guint8           desc_subtype;
    const gchar     *subtype_str;

    usb_conv_info = (usb_conv_info_t *)data;
    if (!usb_conv_info || usb_conv_info->interfaceClass!=IF_CLASS_AUDIO)
        return 0;

    desc_len  = tvb_get_guint8(tvb, offset);
    desc_type = tvb_get_guint8(tvb, offset+1);

    if (desc_type==CS_INTERFACE &&
            usb_conv_info->interfaceSubclass==AUDIO_IF_SUBCLASS_AUDIOCONTROL) {

        desc_tree = proto_tree_add_subtree(tree, tvb, offset, desc_len,
                ett_usb_audio_desc, &desc_tree_item,
                "Class-specific Audio Control Interface Descriptor");

        dissect_usb_descriptor_header(desc_tree, tvb, offset,
            &aud_descriptor_type_vals_ext);
        offset += 2;

        desc_subtype = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(desc_tree, hf_ac_if_desc_subtype,
                tvb, offset, 1, ENC_LITTLE_ENDIAN);
        subtype_str = try_val_to_str_ext(desc_subtype, &ac_subtype_vals_ext);
        if (subtype_str)
            proto_item_append_text(desc_tree_item, ": %s", subtype_str);
        offset++;

        bytes_dissected = offset;
        switch(desc_subtype) {
            case AC_SUBTYPE_HEADER:
                /* these subfunctions return the number of bytes dissected,
                   this is not necessarily the length of the body
                   as some components are not yet dissected
                   we rely on the descriptor's length byte instead */
                bytes_dissected += dissect_ac_if_hdr_body(tvb, offset, pinfo, desc_tree, usb_conv_info);
                break;
            case AC_SUBTYPE_INPUT_TERMINAL:
                bytes_dissected += dissect_ac_if_input_terminal(tvb, offset, pinfo, desc_tree, usb_conv_info);
                break;
            case AC_SUBTYPE_OUTPUT_TERMINAL:
                bytes_dissected += dissect_ac_if_output_terminal(tvb, offset, pinfo, desc_tree, usb_conv_info);
                break;
            case AC_SUBTYPE_MIXER_UNIT:
                bytes_dissected += dissect_ac_if_mixed_unit(tvb, offset, pinfo, desc_tree, usb_conv_info);
                break;
            case AC_SUBTYPE_FEATURE_UNIT:
                bytes_dissected += dissect_ac_if_feature_unit(tvb, offset, pinfo, desc_tree, usb_conv_info, desc_len);
                break;
            case AC_SUBTYPE_CLOCK_SOURCE:
                bytes_dissected += dissect_ac_if_clock_source(tvb, offset, pinfo, desc_tree, usb_conv_info);
                break;
            case AC_SUBTYPE_CLOCK_SELECTOR:
                bytes_dissected += dissect_ac_if_clock_selector(tvb, offset, pinfo, desc_tree, usb_conv_info);
                break;
            default:
                break;
        }
    }
    else if (desc_type==CS_INTERFACE &&
            usb_conv_info->interfaceSubclass==AUDIO_IF_SUBCLASS_AUDIOSTREAMING) {

        desc_tree = proto_tree_add_subtree(tree, tvb, offset, desc_len,
                ett_usb_audio_desc, &desc_tree_item,
                "Class-specific Audio Streaming Interface Descriptor");

        dissect_usb_descriptor_header(desc_tree, tvb, offset,
            &aud_descriptor_type_vals_ext);
        offset += 2;

        desc_subtype = tvb_get_guint8(tvb, offset);
        proto_tree_add_item(desc_tree, hf_as_if_desc_subtype,
                tvb, offset, 1, ENC_LITTLE_ENDIAN);
        subtype_str = try_val_to_str_ext(desc_subtype, &as_subtype_vals_ext);
        if (subtype_str)
            proto_item_append_text(desc_tree_item, ": %s", subtype_str);
        offset++;

        bytes_dissected = offset;
        switch(desc_subtype) {
            case AS_SUBTYPE_GENERAL:
                bytes_dissected += dissect_as_if_general_body(tvb, offset, pinfo,
                        desc_tree, usb_conv_info);
                break;
            case AS_SUBTYPE_FORMAT_TYPE:
                bytes_dissected += dissect_as_if_format_type_body(tvb, offset, pinfo,
                        desc_tree, usb_conv_info);
                break;
            default:
                break;
        }
    }
    /* there are no class-specific endpoint descriptors for audio control */
    else if (desc_type == CS_ENDPOINT &&
            usb_conv_info->interfaceSubclass==AUDIO_IF_SUBCLASS_AUDIOSTREAMING) {

        desc_tree = proto_tree_add_subtree(tree, tvb, offset, desc_len,
                ett_usb_audio_desc, &desc_tree_item,
                "Class-specific Audio Streaming Endpoint Descriptor");

        dissect_usb_descriptor_header(desc_tree, tvb, offset,
            &aud_descriptor_type_vals_ext);
        offset += 2;

        proto_tree_add_item(desc_tree, hf_as_ep_desc_subtype,
                tvb, offset, 1, ENC_LITTLE_ENDIAN);
        offset++;

        bytes_dissected = offset;
    }
    else
        return 0;

    if (bytes_dissected < desc_len) {
        proto_tree_add_expert(desc_tree, pinfo, &ei_usb_audio_undecoded, tvb, bytes_dissected, desc_len-bytes_dissected);
    }
    return desc_len;
}


/* dissector for usb midi bulk data */
static int
dissect_usb_audio_bulk(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, void *data)
{
    usb_conv_info_t *usb_conv_info;
    proto_tree      *tree;
    proto_item      *ti;
    gint             offset, length;
    gint             i;

    /* Reject the packet if data is NULL */
    if (data == NULL)
        return 0;
    usb_conv_info = (usb_conv_info_t *)data;

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "USBAUDIO");

    ti   = proto_tree_add_protocol_format(parent_tree, proto_usb_audio, tvb, 0, -1, "USB Audio");
    tree = proto_item_add_subtree(ti, ett_usb_audio);

    length = tvb_reported_length(tvb);
    offset = 0;

    switch (usb_conv_info->interfaceSubclass)
    {
        case AUDIO_IF_SUBCLASS_MIDISTREAMING:
            col_set_str(pinfo->cinfo, COL_INFO, "USB-MIDI Event Packets");

            for (i = 0; i < length / 4; i++)
            {
                dissect_usb_midi_event(tvb, pinfo, tree, parent_tree, offset);
                offset += 4;
            }
            break;
        default:
            proto_tree_add_expert(tree, pinfo, &ei_usb_audio_undecoded, tvb, offset, length);
    }

    return length;
}

void
proto_register_usb_audio(void)
{
    static hf_register_info hf[] = {
        { &hf_midi_cable_number,
            { "Cable Number", "usbaudio.midi.cable_number", FT_UINT8, BASE_HEX,
              NULL, 0xF0, NULL, HFILL }},
        { &hf_midi_code_index,
            { "Code Index", "usbaudio.midi.code_index", FT_UINT8, BASE_HEX,
              VALS(code_index_vals), 0x0F, NULL, HFILL }},
        { &hf_midi_event,
            { "MIDI Event", "usbaudio.midi.event", FT_UINT24, BASE_HEX,
              NULL, 0, NULL, HFILL }},

        { &hf_ac_if_desc_subtype,
            { "Subtype", "usbaudio.ac_if_subtype", FT_UINT8, BASE_HEX|BASE_EXT_STRING,
                &ac_subtype_vals_ext, 0x00, "bDescriptorSubtype", HFILL }},
        { &hf_ac_if_hdr_ver,
            { "Version", "usbaudio.ac_if_hdr.bcdADC",
                FT_DOUBLE, BASE_NONE, NULL, 0, "bcdADC", HFILL }},
        { &hf_ac_if_hdr_total_len,
            { "Total length", "usbaudio.ac_if_hdr.wTotalLength",
              FT_UINT16, BASE_DEC, NULL, 0x00, "wTotalLength", HFILL }},
        { &hf_ac_if_hdr_bInCollection,
            { "Total number of interfaces", "usbaudio.ac_if_hdr.bInCollection",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bInCollection", HFILL }},
        { &hf_ac_if_hdr_if_num,
            { "Interface number", "usbaudio.ac_if_hdr.baInterfaceNr",
              FT_UINT8, BASE_DEC, NULL, 0x00, "baInterfaceNr", HFILL }},
        { &hf_ac_if_hdr_category,
            { "Category", "usbaudio.ac_if_hdr.bCategory",
              FT_UINT8, BASE_HEX|BASE_EXT_STRING, &audio_function_categories_vals_ext, 0x00, "bCategory", HFILL }},
        { &hf_ac_if_hdr_controls,
            { "Controls", "usbaudio.ac_if_hdr.bmControls",
              FT_UINT8, BASE_HEX, NULL, 0x00, "bmControls", HFILL }},
        { &hf_ac_if_hdr_controls_latency,
            { "Latency Control", "usbaudio.ac_if_hdr.bmControls.latency",
              FT_UINT8, BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x03, NULL, HFILL }},
        { &hf_ac_if_hdr_controls_rsv,
            { "Reserved", "usbaudio.ac_if_hdr.bmControls.rsv",
              FT_UINT8, BASE_HEX, NULL, 0xFC, "Must be zero", HFILL }},
        { &hf_ac_if_input_terminalid,
            { "Terminal ID", "usbaudio.ac_if_input.bTerminalID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bTerminalID", HFILL }},
        { &hf_ac_if_input_terminaltype,
            { "Terminal Type", "usbaudio.ac_if_input.wTerminalType", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &terminal_types_vals_ext, 0x00, "wTerminalType", HFILL }},
        { &hf_ac_if_input_assocterminal,
            { "Assoc Terminal", "usbaudio.ac_if_input.bAssocTerminal",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bAssocTerminal", HFILL }},
        { &hf_ac_if_input_csourceid,
            { "Connected Clock Entity", "usbaudio.ac_if_input.bCSourceID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bCSourceID", HFILL }},
        { &hf_ac_if_input_nrchannels,
            { "Number Channels", "usbaudio.ac_if_input.bNrChannels",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bNrChannels", HFILL }},
        { &hf_ac_if_input_wchannelconfig,
            { "Channel Config", "usbaudio.ac_if_input.wChannelConfig",
              FT_UINT16, BASE_HEX, NULL, 0x00, "wChannelConfig", HFILL }},
        { &hf_ac_if_input_wchannelconfig_d0,
            { "Left Front", "usbaudio.ac_if_input.wChannelConfig.d0",
              FT_BOOLEAN, 16, NULL, 0x0001, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d1,
            { "Right Front", "usbaudio.ac_if_input.wChannelConfig.d1",
              FT_BOOLEAN, 16, NULL, 0x0002, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d2,
            { "Center Front", "usbaudio.ac_if_input.wChannelConfig.d2",
              FT_BOOLEAN, 16, NULL, 0x0004, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d3,
            { "Low Frequency Enhancement", "usbaudio.ac_if_input.wChannelConfig.d3",
              FT_BOOLEAN, 16, NULL, 0x0008, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d4,
            { "Left Surround", "usbaudio.ac_if_input.wChannelConfig.d4",
              FT_BOOLEAN, 16, NULL, 0x0010, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d5,
            { "Right Surround", "usbaudio.ac_if_input.wChannelConfig.d5",
              FT_BOOLEAN, 16, NULL, 0x0020, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d6,
            { "Left of Center", "usbaudio.ac_if_input.wChannelConfig.d6",
              FT_BOOLEAN, 16, NULL, 0x0040, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d7,
            { "Right of Center", "usbaudio.ac_if_input.wChannelConfig.d7",
              FT_BOOLEAN, 16, NULL, 0x0080, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d8,
            { "Surround", "usbaudio.ac_if_input.wChannelConfig.d8",
              FT_BOOLEAN, 16, NULL, 0x0100, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d9,
            { "Side Left", "usbaudio.ac_if_input.wChannelConfig.d9",
              FT_BOOLEAN, 16, NULL, 0x0200, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d10,
            { "Side Right", "usbaudio.ac_if_input.wChannelConfig.d10",
              FT_BOOLEAN, 16, NULL, 0x0400, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_d11,
            { "Top", "usbaudio.ac_if_input.wChannelConfig.d11",
              FT_BOOLEAN, 16, NULL, 0x0800, NULL, HFILL }},
        { &hf_ac_if_input_wchannelconfig_rsv,
            { "Reserved", "usbaudio.ac_if_input.wChannelConfig.rsv",
              FT_UINT16, BASE_HEX, NULL, 0xF000, NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig,
            { "Channel Config", "usbaudio.ac_if_input.bmChannelConfig",
              FT_UINT32, BASE_HEX, NULL, 0x00000000, "bmChannelConfig", HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d0,
            { "Front Left", "usbaudio.ac_if_input.bmChannelConfig.d0",
              FT_BOOLEAN, 32, NULL, (1u << 0), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d1,
            { "Front Right", "usbaudio.ac_if_input.bmChannelConfig.d1",
              FT_BOOLEAN, 32, NULL, (1u << 1), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d2,
            { "Front Center", "usbaudio.ac_if_input.bmChannelConfig.d2",
              FT_BOOLEAN, 32, NULL, (1u << 2), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d3,
            { "Low Frequency Effects", "usbaudio.ac_if_input.bmChannelConfig.d3",
              FT_BOOLEAN, 32, NULL, (1u << 3), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d4,
            { "Back Left", "usbaudio.ac_if_input.bmChannelConfig.d4",
              FT_BOOLEAN, 32, NULL, (1u << 4), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d5,
            { "Back Right", "usbaudio.ac_if_input.bmChannelConfig.d5",
              FT_BOOLEAN, 32, NULL, (1u << 5), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d6,
            { "Front Left of Center", "usbaudio.ac_if_input.bmChannelConfig.d6",
              FT_BOOLEAN, 32, NULL, (1u << 6), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d7,
            { "Front Right of Center", "usbaudio.ac_if_input.bmChannelConfig.d7",
              FT_BOOLEAN, 32, NULL, (1u << 7), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d8,
            { "Back Center", "usbaudio.ac_if_input.bmChannelConfig.d8",
              FT_BOOLEAN, 32, NULL, (1u << 8), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d9,
            { "Side Left", "usbaudio.ac_if_input.bmChannelConfig.d9",
              FT_BOOLEAN, 32, NULL, (1u << 9), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d10,
            { "Side Right", "usbaudio.ac_if_input.bmChannelConfig.d10",
              FT_BOOLEAN, 32, NULL, (1u << 10), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d11,
            { "Top Center", "usbaudio.ac_if_input.bmChannelConfig.d11",
              FT_BOOLEAN, 32, NULL, (1u << 11), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d12,
            { "Top Front Left", "usbaudio.ac_if_input.bmChannelConfig.d12",
              FT_BOOLEAN, 32, NULL, (1u << 12), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d13,
            { "Top Front Center", "usbaudio.ac_if_input.bmChannelConfig.d13",
              FT_BOOLEAN, 32, NULL, (1u << 13), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d14,
            { "Top Front Right", "usbaudio.ac_if_input.bmChannelConfig.d14",
              FT_BOOLEAN, 32, NULL, (1u << 14), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d15,
            { "Top Back Left", "usbaudio.ac_if_input.bmChannelConfig.d15",
              FT_BOOLEAN, 32, NULL, (1u << 15), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d16,
            { "Top Back Center", "usbaudio.ac_if_input.bmChannelConfig.d16",
              FT_BOOLEAN, 32, NULL, (1u << 16), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d17,
            { "Top Back Right", "usbaudio.ac_if_input.bmChannelConfig.d17",
              FT_BOOLEAN, 32, NULL, (1u << 17), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d18,
            { "Top Front Left of Center", "usbaudio.ac_if_input.bmChannelConfig.d18",
              FT_BOOLEAN, 32, NULL, (1u << 18), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d19,
            { "Top Front Right of Center", "usbaudio.ac_if_input.bmChannelConfig.d19",
              FT_BOOLEAN, 32, NULL, (1u << 19), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d20,
            { "Left Low Frequency Effects", "usbaudio.ac_if_input.bmChannelConfig.d20",
              FT_BOOLEAN, 32, NULL, (1u << 20), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d21,
            { "Right Low Frequency Effects", "usbaudio.ac_if_input.bmChannelConfig.d21",
              FT_BOOLEAN, 32, NULL, (1u << 21), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d22,
            { "Top Side Left", "usbaudio.ac_if_input.bmChannelConfig.d22",
              FT_BOOLEAN, 32, NULL, (1u << 22), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d23,
            { "Top Side Right", "usbaudio.ac_if_input.bmChannelConfig.d23",
              FT_BOOLEAN, 32, NULL, (1u << 23), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d24,
            { "Bottom Center", "usbaudio.ac_if_input.bmChannelConfig.d24",
              FT_BOOLEAN, 32, NULL, (1u << 24), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d25,
            { "Back Left of Center", "usbaudio.ac_if_input.bmChannelConfig.d25",
              FT_BOOLEAN, 32, NULL, (1u << 25), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d26,
            { "Back Right of Center", "usbaudio.ac_if_input.bmChannelConfig.d26",
              FT_BOOLEAN, 32, NULL, (1u << 26), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_rsv,
            { "Reserved", "usbaudio.ac_if_input.bmChannelConfig.rsv",
              FT_BOOLEAN, 32, NULL, (0xFu << 27), NULL, HFILL }},
        { &hf_ac_if_input_bmchannelconfig_d31,
            { "Raw Data", "usbaudio.ac_if_input.bmChannelConfig.d31",
              FT_BOOLEAN, 32, NULL, (1u << 31), NULL, HFILL }},
        { &hf_ac_if_input_channelnames,
            { "Channel Names", "usbaudio.ac_if_input.iChannelNames",
              FT_UINT8, BASE_DEC, NULL, 0x00, "iChannelNames", HFILL }},
        { &hf_ac_if_input_controls,
            { "Controls", "usbaudio.ac_if_input.bmControls",
              FT_UINT16, BASE_HEX, NULL, 0x0000, "bmControls", HFILL }},
        { &hf_ac_if_input_controls_copy,
            { "Copy Protect Control", "usbaudio.ac_if_input.bmControls.copy", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x0003, NULL, HFILL }},
        { &hf_ac_if_input_controls_connector,
            { "Connector Control", "usbaudio.ac_if_input.bmControls.connector", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x000C, NULL, HFILL }},
        { &hf_ac_if_input_controls_overload,
            { "Overload Control", "usbaudio.ac_if_input.bmControls.overload", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x0030, NULL, HFILL }},
        { &hf_ac_if_input_controls_cluster,
            { "Cluster Control", "usbaudio.ac_if_input.bmControls.cluster", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x00C0, NULL, HFILL }},
        { &hf_ac_if_input_controls_underflow,
            { "Underflow Control", "usbaudio.ac_if_input.bmControls.underflow", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x0300, NULL, HFILL }},
        { &hf_ac_if_input_controls_overflow,
            { "Overflow Control", "usbaudio.ac_if_input.bmControls.overflow", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x0C00, NULL, HFILL }},
        { &hf_ac_if_input_controls_rsv,
            { "Reserved", "usbaudio.ac_if_input.bmControls.rsv",
              FT_UINT16, BASE_HEX, NULL, 0xF000, "Must be zero", HFILL }},
        { &hf_ac_if_input_terminal,
            { "String descriptor index", "usbaudio.ac_if_input.iTerminal",
              FT_UINT8, BASE_DEC, NULL, 0x00, "iTerminal", HFILL }},
        { &hf_ac_if_output_terminalid,
            { "Terminal ID", "usbaudio.ac_if_output.bTerminalID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bTerminalID", HFILL }},
        { &hf_ac_if_output_terminaltype,
            { "Terminal Type", "usbaudio.ac_if_output.wTerminalType", FT_UINT16,
               BASE_HEX|BASE_EXT_STRING, &terminal_types_vals_ext, 0x00, "wTerminalType", HFILL }},
        { &hf_ac_if_output_assocterminal,
            { "Assoc Terminal", "usbaudio.ac_if_output.bAssocTerminal",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bAssocTerminal", HFILL }},
        { &hf_ac_if_output_sourceid,
            { "Source ID", "usbaudio.ac_if_output.bSourceID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bSourceID", HFILL }},
        { &hf_ac_if_output_clk_sourceid,
            { "Connected Clock Entity", "usbaudio.ac_if_output.bCSourceID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bCSourceID", HFILL }},
        { &hf_ac_if_output_controls,
            { "Controls", "usbaudio.ac_if_output.bmControls",
              FT_UINT16, BASE_HEX, NULL, 0x0000, "bmControls", HFILL }},
        { &hf_ac_if_output_controls_copy,
            { "Copy Protect Control", "usbaudio.ac_if_output.bmControls.copy", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x0003, NULL, HFILL }},
        { &hf_ac_if_output_controls_connector,
            { "Connector Control", "usbaudio.ac_if_output.bmControls.connector", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x000C, NULL, HFILL }},
        { &hf_ac_if_output_controls_overload,
            { "Overload Control", "usbaudio.ac_if_output.bmControls.overload", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x0030, NULL, HFILL }},
        { &hf_ac_if_output_controls_underflow,
            { "Underflow Control", "usbaudio.ac_if_output.bmControls.underflow", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x00C0, NULL, HFILL }},
        { &hf_ac_if_output_controls_overflow,
            { "Overflow Control", "usbaudio.ac_if_output.bmControls.overflow", FT_UINT16,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x0300, NULL, HFILL }},
        { &hf_ac_if_output_controls_rsv,
            { "Reserved", "usbaudio.ac_if_output.bmControls.rsv",
              FT_UINT16, BASE_HEX, NULL, 0xFC00, "Must be zero", HFILL }},
        { &hf_ac_if_output_terminal,
            { "String descriptor index", "usbaudio.ac_if_output.iTerminal",
              FT_UINT8, BASE_DEC, NULL, 0x00, "iTerminal", HFILL }},
        { &hf_ac_if_fu_unitid,
            { "Unit ID", "usbaudio.ac_if_fu.bUnitID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bUnitID", HFILL }},
        { &hf_ac_if_fu_sourceid,
            { "Source ID", "usbaudio.ac_if_fu.bSourceID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bSourceID", HFILL }},
        { &hf_ac_if_fu_controlsize,
            { "Control Size", "usbaudio.ac_if_fu.bControlSize",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bControlSize", HFILL }},
        { &hf_ac_if_fu_controls,
            { "Controls", "usbaudio.ac_if_fu.bmaControls",
              FT_BYTES, BASE_NONE, NULL, 0x00, "bmaControls", HFILL }},
        { &hf_ac_if_fu_control,
            { "Control", "usbaudio.ac_if_fu.bmaControl",
              FT_UINT8, BASE_HEX, NULL, 0x00, "bmaControls", HFILL }},
        { &hf_ac_if_fu_controls_d0,
            { "Mute", "usbaudio.ac_if_fu.bmaControls.d0",
              FT_BOOLEAN, 8, NULL, 0x01, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d1,
            { "Volume", "usbaudio.ac_if_fu.bmaControls.d1",
              FT_BOOLEAN, 8, NULL, 0x02, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d2,
            { "Bass", "usbaudio.ac_if_fu.bmaControls.d2",
              FT_BOOLEAN, 8, NULL, 0x04, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d3,
            { "Mid", "usbaudio.ac_if_fu.bmaControls.d3",
              FT_BOOLEAN, 8, NULL, 0x08, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d4,
            { "Treble", "usbaudio.ac_if_fu.bmaControls.d4",
              FT_BOOLEAN, 8, NULL, 0x10, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d5,
            { "Graphic Equalizer", "usbaudio.ac_if_fu.bmaControls.d5",
              FT_BOOLEAN, 8, NULL, 0x20, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d6,
            { "Automatic Gain", "usbaudio.ac_if_fu.bmaControls.d6",
              FT_BOOLEAN, 8, NULL, 0x40, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d7,
            { "Delay", "usbaudio.ac_if_fu.bmaControls.d7",
              FT_BOOLEAN, 8, NULL, 0x80, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d8,
            { "Bass Boost", "usbaudio.ac_if_fu.bmaControls.d8",
              FT_BOOLEAN, 8, NULL, 0x01, NULL, HFILL }},
        { &hf_ac_if_fu_controls_d9,
            { "Loudness", "usbaudio.ac_if_fu.bmaControls.d9",
              FT_BOOLEAN, 8, NULL, 0x02, NULL, HFILL }},
        { &hf_ac_if_fu_controls_rsv,
            { "Reserved", "usbaudio.ac_if_fu.bmaControls.rsv",
              FT_UINT8, BASE_HEX, NULL, 0xFC, "Must be zero", HFILL }},
        { &hf_ac_if_fu_ifeature,
            { "Feature", "usbaudio.ac_if_fu.iFeature",
              FT_UINT8, BASE_DEC, NULL, 0x00, "iFeature", HFILL }},
        { &hf_ac_if_mu_unitid,
            { "Unit ID", "usbaudio.ac_if_mu.bUnitID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bUnitID", HFILL }},
        { &hf_ac_if_mu_nrinpins,
            { "Number In Pins", "usbaudio.ac_if_mu.bNrInPins",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bNrInPins", HFILL }},
        { &hf_ac_if_mu_sourceid,
            { "Source ID", "usbaudio.ac_if_mu.baSourceID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "baSourceID", HFILL }},
        { &hf_ac_if_mu_nrchannels,
            { "Number Channels", "usbaudio.ac_if_mu.bNrChannels",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bNrChannels", HFILL }},
        { &hf_ac_if_mu_channelconfig,
            { "Channel Config", "usbaudio.ac_if_mu.wChannelConfig",
              FT_UINT16, BASE_HEX, NULL, 0x00, "wChannelConfig", HFILL }},
        { &hf_ac_if_mu_channelconfig_d0,
            { "Left Front", "usbaudio.ac_if_mu.wChannelConfig.d0",
              FT_BOOLEAN, 16, NULL, 0x0001, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d1,
            { "Right Front", "usbaudio.ac_if_mu.wChannelConfig.d1",
              FT_BOOLEAN, 16, NULL, 0x0002, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d2,
            { "Center Front", "usbaudio.ac_if_mu.wChannelConfig.d2",
              FT_BOOLEAN, 16, NULL, 0x0004, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d3,
            { "Low Frequency Enhancement", "usbaudio.ac_if_mu.wChannelConfig.d3",
              FT_BOOLEAN, 16, NULL, 0x0008, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d4,
            { "Left Surround", "usbaudio.ac_if_mu.wChannelConfig.d4",
              FT_BOOLEAN, 16, NULL, 0x0010, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d5,
            { "Right Surround", "usbaudio.ac_if_mu.wChannelConfig.d5",
              FT_BOOLEAN, 16, NULL, 0x0020, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d6,
            { "Left of Center", "usbaudio.ac_if_mu.wChannelConfig.d6",
              FT_BOOLEAN, 16, NULL, 0x0040, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d7,
            { "Right of Center", "usbaudio.ac_if_mu.wChannelConfig.d7",
              FT_BOOLEAN, 16, NULL, 0x0080, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d8,
            { "Surround", "usbaudio.ac_if_mu.wChannelConfig.d8",
              FT_BOOLEAN, 16, NULL, 0x0100, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d9,
            { "Side Left", "usbaudio.ac_if_mu.wChannelConfig.d9",
              FT_BOOLEAN, 16, NULL, 0x0200, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d10,
            { "Side Right", "usbaudio.ac_if_mu.wChannelConfig.d10",
              FT_BOOLEAN, 16, NULL, 0x0400, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_d11,
            { "Top", "usbaudio.ac_if_mu.wChannelConfig.d11",
              FT_BOOLEAN, 16, NULL, 0x0800, NULL, HFILL }},
        { &hf_ac_if_mu_channelconfig_rsv,
            { "Reserved", "usbaudio.ac_if_mu.wChannelConfig.rsv",
              FT_UINT16, BASE_HEX, NULL, 0xF000, NULL, HFILL }},
        { &hf_ac_if_mu_channelnames,
            { "Channel Names", "usbaudio.ac_if_mu.iChannelNames",
              FT_UINT8, BASE_DEC, NULL, 0x00, "iChannelNames", HFILL }},
        { &hf_ac_if_mu_controls,
            { "Controls", "usbaudio.ac_if_mu.bmControls",
              FT_UINT8, BASE_HEX, NULL, 0x00, "bmControls", HFILL }},
        { &hf_ac_if_mu_imixer,
            { "Mixer", "usbaudio.ac_if_mu.iMixer",
              FT_UINT8, BASE_DEC, NULL, 0x00, "iMixer", HFILL }},
        { &hf_ac_if_clksrc_id,
            { "Clock Source Entity", "usbaudio.ac_if_clksrc.bClockID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bClockID", HFILL }},
        { &hf_ac_if_clksrc_attr,
            { "Attributes", "usbaudio.ac_if_clksrc.bmAttributes",
              FT_UINT8, BASE_HEX, NULL, 0x00, "bmAttributes", HFILL }},
        { &hf_ac_if_clksrc_attr_type,
            { "Type", "usbaudio.ac_if_clksrc.bmAttributes.type", FT_UINT8,
              BASE_HEX, VALS(clock_types_vals), 0x03, NULL, HFILL }},
        { &hf_ac_if_clksrc_attr_d2,
            { "Synchronization", "usbaudio.ac_if_clksrc.bmAttributes.d2", FT_UINT8,
              BASE_HEX, VALS(clock_sync_vals), 0x04, NULL, HFILL }},
        { &hf_ac_if_clksrc_attr_rsv,
            { "Reserved", "usbaudio.ac_if_clksrc.bmAttributes.rsv",
              FT_UINT8, BASE_HEX, NULL, 0xF8, "Must be zero", HFILL }},
        { &hf_ac_if_clksrc_controls,
            { "Controls", "usbaudio.ac_if_clksrc.bmControls",
              FT_UINT8, BASE_HEX, NULL, 0x00, "bmControls", HFILL }},
        { &hf_ac_if_clksrc_controls_freq,
            { "Clock Frequency Control", "usbaudio.ac_if_clksrc.bmControls.freq", FT_UINT8,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x03, NULL, HFILL }},
        { &hf_ac_if_clksrc_controls_validity,
            { "Clock Validity Control", "usbaudio.ac_if_clksrc.bmControls.validity", FT_UINT8,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x0C, NULL, HFILL }},
        { &hf_ac_if_clksrc_controls_rsv,
            { "Reserved", "usbaudio.ac_if_clksrc.bmControls.rsv",
              FT_UINT8, BASE_HEX, NULL, 0xF0, "Must be zero", HFILL }},
        { &hf_ac_if_clksrc_assocterminal,
            { "Terminal", "usbaudio.ac_if_clksrc.bAssocTerminal",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bAssocTerminal", HFILL }},
        { &hf_ac_if_clksrc_clocksource,
            { "String descriptor index", "usbaudio.ac_if_clksrc.iClockSource",
              FT_UINT8, BASE_DEC, NULL, 0x00, "iClockSource", HFILL }},
        { &hf_ac_if_clksel_id,
            { "Clock Selector Entity", "usbaudio.ac_if_clksel.bClockID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bClockID", HFILL }},
        { &hf_ac_if_clksel_nrpins,
            { "Number of Input Pins", "usbaudio.ac_if_clksel.bNrInPins",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bNrInPins", HFILL }},
        { &hf_ac_if_clksel_sourceid,
            { "Connected Clock Entity", "usbaudio.ac_if_clksel.baCSourceID",
              FT_UINT8, BASE_DEC, NULL, 0x00, "baCSourceID", HFILL }},
        { &hf_ac_if_clksel_controls,
            { "Controls", "usbaudio.ac_if_clksel.bmControls",
              FT_UINT8, BASE_HEX, NULL, 0x00, "bmControls", HFILL }},
        { &hf_ac_if_clksel_controls_clksel,
            { "Clock Selector Control", "usbaudio.ac_if_clksel.bmControls.clksel", FT_UINT8,
              BASE_HEX|BASE_EXT_STRING, &controls_capabilities_vals_ext, 0x03, NULL, HFILL }},
        { &hf_ac_if_clksel_controls_rsv,
            { "Reserved", "usbaudio.ac_if_clksel.bmControls.rsv",
              FT_UINT8, BASE_HEX, NULL, 0xFC, "Must be zero", HFILL }},
        { &hf_ac_if_clksel_clockselector,
            { "String descriptor index", "usbaudio.ac_if_clksel.iClockSelector",
              FT_UINT8, BASE_DEC, NULL, 0x00, "iClockSelector", HFILL }},
        { &hf_as_if_desc_subtype,
            { "Subtype", "usbaudio.as_if_subtype", FT_UINT8, BASE_HEX|BASE_EXT_STRING,
                &as_subtype_vals_ext, 0x00, "bDescriptorSubtype", HFILL }},
        { &hf_as_if_gen_term_id,
            { "Terminal ID", "usbaudio.as_if_gen.bTerminalLink",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bTerminalLink", HFILL }},
        { &hf_as_if_gen_delay,
            { "Interface delay in frames", "usbaudio.as_if_gen.bDelay",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bDelay", HFILL }},
        { &hf_as_if_gen_format,
            { "Format", "usbaudio.as_if_gen.wFormatTag",
              FT_UINT16, BASE_HEX, NULL, 0x00, "wFormatTag", HFILL }},
        { &hf_as_if_ft_formattype,
            { "FormatType", "usbaudio.as_if_ft.bFormatType",
              FT_UINT8, BASE_DEC, NULL, 0x00, "wFormatType", HFILL }},
        { &hf_as_if_ft_maxbitrate,
            { "Max Bit Rate", "usbaudio.as_if_ft.wMaxBitRate",
              FT_UINT16, BASE_DEC, NULL, 0x00, "wMaxBitRate", HFILL }},
        { &hf_as_if_ft_nrchannels,
            { "Number Channels", "usbaudio.as_if_ft.bNrChannels",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bNrChannels", HFILL }},
        { &hf_as_if_ft_subframesize,
            { "Subframe Size", "usbaudio.as_if_ft.bSubframeSize",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bSubframeSize", HFILL }},
        { &hf_as_if_ft_bitresolution,
            { "Bit Resolution", "usbaudio.as_if_ft.bBitResolution",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bBitResolution", HFILL }},
        { &hf_as_if_ft_samplesperframe,
            { "Samples Per Frame", "usbaudio.as_if_ft.wSamplesPerFrame",
              FT_UINT16, BASE_DEC, NULL, 0x00, "wSamplesPerFrame", HFILL }},
        { &hf_as_if_ft_samfreqtype,
            { "Samples Frequence Type", "usbaudio.as_if_ft.bSamFreqType",
              FT_UINT8, BASE_DEC, NULL, 0x00, "bSamFreqType", HFILL }},
        { &hf_as_if_ft_lowersamfreq,
            { "Lower Samples Frequence", "usbaudio.as_if_ft.tLowerSamFreq",
              FT_UINT24, BASE_DEC, NULL, 0x00, "tLowerSamFreq", HFILL }},
        { &hf_as_if_ft_uppersamfreq,
            { "Upper Samples Frequence", "usbaudio.as_if_ft.tUpperSamFreq",
              FT_UINT24, BASE_DEC, NULL, 0x00, "tUpperSamFreq", HFILL }},
        { &hf_as_if_ft_samfreq,
            { "Samples Frequence", "usbaudio.as_if_ft.tSamFreq",
              FT_UINT24, BASE_DEC, NULL, 0x00, "tSamFreq", HFILL }},
        { &hf_as_ep_desc_subtype,
            { "Subtype", "usbaudio.as_ep_subtype", FT_UINT8,
                BASE_HEX, NULL, 0x00, "bDescriptorSubtype", HFILL }},

        { &hf_sysex_msg_fragments,
            { "Message fragments", "usbaudio.sysex.fragments",
              FT_NONE, BASE_NONE, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_fragment,
            { "Message fragment", "usbaudio.sysex.fragment",
              FT_FRAMENUM, BASE_NONE, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_fragment_overlap,
            { "Message fragment overlap", "usbaudio.sysex.fragment.overlap",
              FT_BOOLEAN, 0, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_fragment_overlap_conflicts,
            { "Message fragment overlapping with conflicting data",
              "usbaudio.sysex.fragment.overlap.conflicts",
              FT_BOOLEAN, 0, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_fragment_multiple_tails,
            { "Message has multiple tail fragments",
              "usbaudio.sysex.fragment.multiple_tails",
              FT_BOOLEAN, 0, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_fragment_too_long_fragment,
            { "Message fragment too long", "usbaudio.sysex.fragment.too_long_fragment",
              FT_BOOLEAN, 0, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_fragment_error,
            { "Message defragmentation error", "usbaudio.sysex.fragment.error",
              FT_FRAMENUM, BASE_NONE, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_fragment_count,
            { "Message fragment count", "usbaudio.sysex.fragment.count",
              FT_UINT32, BASE_DEC, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_reassembled_in,
            { "Reassembled in", "usbaudio.sysex.reassembled.in",
              FT_FRAMENUM, BASE_NONE, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_reassembled_length,
            { "Reassembled length", "usbaudio.sysex.reassembled.length",
              FT_UINT32, BASE_DEC, NULL, 0x00, NULL, HFILL }},
        { &hf_sysex_msg_reassembled_data,
            { "Reassembled data", "usbaudio.sysex.reassembled.data",
              FT_BYTES, BASE_NONE, NULL, 0x00, NULL, HFILL }}
    };

    static gint *usb_audio_subtrees[] = {
        &ett_usb_audio,
        &ett_usb_audio_desc,
        &ett_sysex_msg_fragment,
        &ett_sysex_msg_fragments,
        &ett_ac_if_hdr_controls,
        &ett_ac_if_fu_controls,
        &ett_ac_if_fu_controls0,
        &ett_ac_if_fu_controls1,
        &ett_ac_if_input_wchannelconfig,
        &ett_ac_if_input_bmchannelconfig,
        &ett_ac_if_input_controls,
        &ett_ac_if_output_controls,
        &ett_ac_if_mu_channelconfig,
        &ett_ac_if_clksrc_attr,
        &ett_ac_if_clksrc_controls,
        &ett_ac_if_clksel_controls
    };

    static ei_register_info ei[] = {
        { &ei_usb_audio_undecoded, { "usbaudio.undecoded", PI_UNDECODED, PI_WARN, "Not dissected yet (report to wireshark.org)", EXPFILL }},
    };

    expert_module_t *expert_usb_audio;

    proto_usb_audio = proto_register_protocol("USB Audio", "USBAUDIO", "usbaudio");
    proto_register_field_array(proto_usb_audio, hf, array_length(hf));
    proto_register_subtree_array(usb_audio_subtrees, array_length(usb_audio_subtrees));
    expert_usb_audio = expert_register_protocol(proto_usb_audio);
    expert_register_field_array(expert_usb_audio, ei, array_length(ei));
    reassembly_table_register(&midi_data_reassembly_table,
                          &addresses_reassembly_table_functions);

    usb_audio_bulk_handle = register_dissector("usbaudio", dissect_usb_audio_bulk, proto_usb_audio);
}

void
proto_reg_handoff_usb_audio(void)
{
    dissector_handle_t usb_audio_descr_handle;

    usb_audio_descr_handle = create_dissector_handle(
            dissect_usb_audio_descriptor, proto_usb_audio);
    dissector_add_uint("usb.descriptor", IF_CLASS_AUDIO, usb_audio_descr_handle);

    dissector_add_uint("usb.bulk", IF_CLASS_AUDIO, usb_audio_bulk_handle);

    sysex_handle = find_dissector_add_dependency("sysex", proto_usb_audio);
}
/*
 * Editor modelines
 *
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * ex: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
