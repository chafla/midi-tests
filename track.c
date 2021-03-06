//
// Created by Matt on 4/26/2019.
//

#include "track.h"
#include <byteswap.h>



size_t read_uint32(unsigned int *ptr, size_t size, size_t n, FILE *file) {

    size_t bytes_read = 0;

    bytes_read += fread(ptr, size, n, file);
    *ptr = __builtin_bswap32(*ptr);
    return bytes_read;

}

/// Read a variable length value in and convert it to an unsigned long
unsigned long read_vlv(FILE *file, int *bytes_read) {
    unsigned short read_buf = 0;
    unsigned long timedelta = 0;
    int *bytes_counter;

    // failsafe, we'll just discard the value in this case
    if (bytes_read == NULL) {
        int tmp;
        bytes_counter = &tmp;
    }
    else {
        bytes_counter = bytes_read;
    }


    do {
        read_buf = 0;

        *bytes_counter += fread(&read_buf, sizeof(char), 1, file);

        // thanks http://www.ccarh.org/courses/253/handout/vlv/

        timedelta <<= 7u;  // ignore the extra bit at the top of the value
        timedelta |= (read_buf & 0x7Fu);



        // Keep reading until we've read a 0 as the msb
    } while ((read_buf) >= 0x80);

    return timedelta;
}


/**
 * Read in a track event and add it to its corresponding track
 * @param file: file pointer to read from
 * @param tracks: the track to read the track event from
 */
void read_track_events(FILE *file, struct Track *track) {

    char chunk_type[5];
    int bytes_read = 0;
    unsigned char cur_event_type = 0;
    unsigned char status_byte = 0;
    unsigned int data_length = 0;
    unsigned int running_status = -1;

    struct TrackEvent *event = NULL;
    int event_capacity = 3000;
    struct TrackEvent **track_events = calloc(event_capacity, sizeof(struct TrackEvent));
    int cur_track_event = 0;

    // parse header
    fread(chunk_type, sizeof(char), 4, file);
    chunk_type[4] = '\0';

    if (strcmp(chunk_type, "MTrk") != 0) {
        printf("Invalid chunk type for track %d, found %s\n", track->id, chunk_type);
        return;
    }

    // get the number of bytes contained by data segment
    // fread(&data_length, sizeof(int), 1, file);  // 4 bytes
    read_uint32(&data_length, sizeof(int), 1, file);

    // let's make a safe bet here and say we'll need at least data_length bytes.
    // we can drop this later
    // track_events = calloc(data_length, sizeof(size_t));

    // here's where the fun begins
    // we need to run through data_length bytes and extract events

    while (bytes_read < data_length) {

        event = calloc(1, sizeof(struct TrackEvent));
        event->data_len = 0;
        event->data = NULL;

        // resize if we end up filling up too much
        // not using realloc because it scares me
        if (cur_track_event + 2 >= event_capacity) {

            struct TrackEvent **new_events = calloc(event_capacity * 2, sizeof(struct TrackEvent));
            for (int i = 0; i < event_capacity + 1; i++) {

                new_events[i] = track_events[i];

            }


            event_capacity *= 2;

            free(track_events);

            track_events = new_events;
        }

        track_events[cur_track_event] = event;
        cur_track_event++;



        // read in the timedelta between the past event and the current event
        // it's formatted in a variable length, so the lower 7 bits are data.
        // a 0 in the msb of a byte (like 01111111) means that the byte is the last in a variable-length number

        event->td = read_vlv(file, &bytes_read);

        // status byte

        // pull in event class
        bytes_read += fread(&status_byte, sizeof(char), 1, file);

        // the first two only have one status byte, and a fixed length.

        if (status_byte < 0x80) {
            // we probably have a running status!
            // https://web.archive.org/web/20130305092440/http://home.roadrunner.com/~jgglatt/tech/midispec/run.htm
            // https://stackoverflow.com/questions/6886087/decoding-unknown-midi-events



            // move back by one byte, since it wasn't actually a status byte
            fseek(file, -1L, SEEK_CUR);
            status_byte = running_status;
            bytes_read--;
        }

        running_status = status_byte;


        if (status_byte == 0xF0 || status_byte == 0xF7) {

            // sysex event
            event->status = status_byte;  // this is significantly shorter
            event->event_class = sysex;
            event->data_len = read_vlv(file, &bytes_read);

            event->data = calloc(event->data_len, sizeof(char));

            bytes_read += fread(event->data, sizeof(char), event->data_len, file);

            if (status_byte == 0xF0)
                event->event_type = s_event;
            else
                event->event_type = s_escape;

            continue;

        }

        // midi events
        else if (0x80 <= status_byte && status_byte < 0xF0) {
            // first byte is event specifier
            event->event_class = midi;
            event->status = status_byte;

            // these two events only have one data byte
            if ((status_byte >> 4u == 0xCu) || ((status_byte >> 4u) == 0xDu)) {

                event->data_len = 1;
            }

            // these ones are two
            else {

                event->data_len = 2;
            }

            // using read_arr here ended up reading data in incorrectly as they're separate values
            event->data = calloc(event->data_len, sizeof(char));
            bytes_read += fread(event->data, sizeof(char), event->data_len, file);

            switch (status_byte >> 4u) {

                case (0x8):
                    event->event_type = mi_off;
                    break;
                case (0x9):
                    event->event_type = mi_on;
                    break;
                case (0xA):
                    event->event_type = mi_pressure;
                    break;
                case (0xB):
                    event->event_type = mi_cont_change;
                    break;
                case (0xC):
                    event->event_type = mi_prog_change;
                    break;
                case (0xD):
                    event->event_type = mi_key_pressure;
                    break;
                case (0xE):
                    event->event_type = mi_pitch_bend;
                    break;
                default:
                    fprintf(stderr, "invalid status byte.\n");
                    break;
            }

            continue;
        }



        // meta events

        bytes_read += fread(&cur_event_type, sizeof(char), 1, file);

        // bring the values together
        event->status = 0u | ((unsigned int) status_byte << 8u) | ((unsigned int) cur_event_type);

        if (event->status >= 0xFF00) {

            // meta events

            event->event_class = meta;
            unsigned int event_type = event->status & 0xFFu;  // just pull off the last byte

            // ones with text
            if (event_type < 8) {
                event->data_len = read_vlv(file, &bytes_read);
                event->data = calloc(event->data_len + 1, sizeof(char));

                bytes_read += fread(event->data, sizeof(char), event->data_len, file);

                // null terminate for good measure
                event->data[event->data_len] = '\0';

                switch (event->status) {
                    case (0xFF00):
                        event->event_type = me_seq_num;
                        break;

                    case (0xFF01):
                        event->event_type = me_text;
                        break;

                    case (0xFF02):
                        event->event_type = me_copyright;
                        break;
                    case (0xFF03):
                        event->event_type = me_seq_name;
                        break;
                    case (0xFF04):
                        event->event_type = me_instr_name;
                        break;
                    case (0xFF05):
                        event->event_type = me_lyric;
                        break;
                    case (0xFF06):
                        event->event_type = me_marker;
                        break;
                    case (0xFF07):
                        event->event_type = me_cue_pt;
                        break;
                    case (0xFF08):
                        event->event_type = me_prog_name;
                        break;
                    case (0xFF09):
                        event->event_type = me_dev_name;
                        break;
                    case (0xFF20):
                        // TODO
                        event->event_type = me_midi_chan_pfx;
                        break;
                    default:
                        fprintf(stderr, "unknown event status");
                }


                continue;
            }

                // sequencer meta event
                // pull this one out since it uses a VLV
            else if (event_type == 0x7F) {

                // sequencer meta event
                event->data_len = read_vlv(file, &bytes_read);

                event->data = calloc(event->data_len, sizeof(char));
                bytes_read += fread(event->data, sizeof(char), event->data_len, file);

                event->event_type = me_seq_m_event;

                continue;

            }

            else {

                bytes_read += fread(&event->data_len, sizeof(char), 1, file);

                event->data = calloc(event->data_len, sizeof(char));
                bytes_read += fread(event->data, sizeof(char), event->data_len, file);

                // all the other ones

                switch (event_type) {
                    case 0x20:
                        event->event_type = me_midi_chan_pfx;
                        break;

                    case (0x21):
                        event->event_type = me_midi_port;
                        break;

                    case 0x2F:
                        event->event_type = me_track_end;
                        if (data_length != bytes_read) {
                            fprintf(stderr, "Track end event reached with more bytes to go.\n");
                        }
                        break;

                    case 0x51:
                        event->event_type = me_set_tempo;
                        break;

                    case 0x54:
                        event->event_type = me_smtpe_offset;
                        break;

                    case 0x58:
                        // time signature
                        event->event_type = me_time_sig;
                        break;

                    case 0x59:
                        event->event_type = me_key_sig;
                        // key signature
                        break;

                    default:
                        fprintf(stderr, "invalid meta event code\n");


                }
            }

        }

    }

    if (bytes_read != data_length) {
        fprintf(stderr, "Discrepancy exists between bytes read and expected data length\n");
    }

    track->n_events = cur_track_event;
    track->events = track_events;

}


/// Return a new track
struct Track *create_track(int id) {
    struct Track *new_track = calloc(1, sizeof(struct Track));
    new_track->id = id;
    new_track->track_name = NULL;
    new_track->events = NULL;

    return new_track;
}

/// read in a header chunk
struct HeaderChunk *read_header_chunk(FILE *file) {
    // create a struct
    struct HeaderChunk *chunk = calloc(1, sizeof(struct HeaderChunk));
    char chunk_type[5];
    unsigned chunk_len;
    // skip over the chunk type
    fread(chunk_type, sizeof(char), 4, file);
    chunk_type[4] = '\0';
    if (strcmp(chunk_type, "MThd") != 0) {
        printf("Invalid chunk type, found %s\n", chunk_type);
        return NULL;
    }
    printf("Chunk type: %s\n", chunk_type);

    // skip the chunk length, this is guaranteed to be 6
    fread(&chunk_len, sizeof(int), 1, file);

    chunk_len = __builtin_bswap32(chunk_len);

    // it's just 6 bytes

    // get midi file format
    fread(&chunk->format, sizeof(char), 2, file);

    fread(&(chunk->n_tracks), sizeof(short), 1, file);
    fread(&(chunk->division), sizeof(short), 1, file);
    chunk->n_tracks = __builtin_bswap16(chunk->n_tracks);
    chunk->division = __builtin_bswap16(chunk->division);
    // skip

    printf("Header information: n_tracks: %#x, division: %#x, format: %#x\n",
           chunk->n_tracks, chunk->division, chunk->format);

    return chunk;

}

char *get_note(struct TrackEvent *event) {

    char* note_ret = calloc(4, sizeof(char));
    note_ret[0] = '\0';
    note_ret[3] = '\0';

    if (event->event_class != midi || event->status == 0 || event->status >= 0xA0u) {
        return note_ret;
    }


    char *keys = "C#D#EF#G#A#B";

    // get the scale, e.g. C[5]
    // 0x3C is middle C (C4), so we want to offset from there
    // ironically, C5 is 0xC (12) above.
    int sharp = 0;
    char scale = (event->data[0] / 12 - 1);     // number of scale, like the 5 in G5
    char note = keys[event->data[0] % 12];  // actual note

    if (note == '#') {
        // pull one note flat
        // haha this is awful
        note = keys[event->data[0] % 12 - 1];
        sharp = 1;
    }



    sprintf(note_ret, "%c%s%d", note, sharp ? "#" : "", scale);

    return note_ret;
}

/// free up all memory associated with the track itself
void destroy_track(struct Track *track) {

    struct TrackEvent *cur_event;

    if (track->track_name != NULL) {

        free(track->track_name);
        track->track_name = NULL;
    }

    if (track->n_events > 0 && track->events != NULL) {

        for (unsigned int i = 0; i < track->n_events; i++) {

            cur_event = track->events[i];

            if (track->events[i] != NULL) {

                // destroy the events

                if (cur_event->data) {
                    free(cur_event->data);
                    cur_event->data = NULL;
                }

                free(cur_event);

            }

        }

    }

    free(track->events);

    free(track);

}

void print_event(struct TrackEvent *event) {

    char *note;

    printf("Class: %d\n", event->event_class);
    printf("td: %#lx\n", event->td);
    printf("status: %#x\n", event->status);
    printf("data:");
    for (int j = 0; j < event->data_len; j++) {
        printf(" %#x", event->data[j] & 0xFFu);

    }
    printf("\n");

    if (event->event_class == midi) {
        // inefficient hack
        note = get_note(event);
        printf("Note: %s\n", note);
        free(note);
    }

    printf("data (chars): %s\n", event->data);
}

void print_track_events(struct Track *track) {

    for (int i = 0; i < track->n_events; i++) {

        struct TrackEvent *cur_event = track->events[i];

        printf("************\n");
        printf("Event %d:\n", i);
        print_event(cur_event);



    }

}
