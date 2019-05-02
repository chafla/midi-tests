//
// Created by Matt on 4/26/2019.
//

#ifndef MIDIS_TRACK_H
#define MIDIS_TRACK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct HeaderChunk {
    int format;
    unsigned short n_tracks;
    unsigned short division;
};

enum EventClass {
    midi,
    meta,
    sysex
};

enum EventType {
    // meta events
    me_seq_num = 0xFF00,  // auto register the rest
    me_text,
    me_copyright,
    me_seq_name,
    me_instr_name,
    me_lyric,
    me_marker,
    me_cue_pt,
    me_prog_name,
    me_dev_name,
    me_midi_chan_pfx = 0xFF20,
    me_midi_port,
    me_track_end = 0xFF2F,
    me_set_tempo = 0xFF51,
    me_smtpe_offset = 0xFF54,
    me_time_sig = 0xFF58,
    me_key_sig = 0xFF59,
    me_seq_m_event = 0xFF7F,
    // sys events
    s_event = 0xF0,
    s_escape = 0xF7,
    // midi events
    // for these, the second hex value represents the midi channel
    mi_off = 0x80,
    mi_on = 0x90,
    mi_pressure = 0xA0,
    mi_cont_change = 0xB0,
    mi_prog_change = 0xC0,
    mi_key_pressure = 0xD0,
    mi_pitch_bend = 0xE0,

    // TODO add midi channel mode messages

};

struct TrackEvent {
    unsigned long td;              /// Timedelta offset before the current event
    enum EventClass event_class;   /// Overarching event class
    enum EventType event_type;     /// More specific event type, subclass
    unsigned short status;         /// status byte of the event
    char* data;                    /// Data of the message - represented in bytes
    unsigned int data_len;         /// Number of bytes in data
};

struct Track {
    int id;                        /// Track number. Always 1 in a type 1 midi file
    char *track_name;              /// Possible track name.
    unsigned int n_events;         /// number of track events
    struct TrackEvent **events;    /// pointer to all track events
};


size_t read_uint32(unsigned int *ptr, size_t size, size_t n, FILE *file);

unsigned long read_vlv(FILE *file, int *bytes_read);

void read_track_events(FILE *file, struct Track *track);

struct Track *create_track(int id);

void destroy_track(struct Track *track);

struct HeaderChunk *read_header_chunk(FILE *file);

char *get_note(struct TrackEvent *event);

void print_event(struct TrackEvent *event);

void print_track_events(struct Track *track);


#endif //MIDIS_TRACK_H
