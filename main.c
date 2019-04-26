#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <byteswap.h>

struct HeaderChunk {
    int format;
    unsigned short n_tracks;
    unsigned short division;
};

enum EventClass{midi, meta, sysex};

enum EventType{
    // meta events
    me_seq_num = 0xFF00,  // auto register the rest
    me_text,
    me_copyright,
    me_seq_name,
    me_instr_name,
    me_lyric,
    me_marker,
    me_cue_pt,
    me_midi_chan_pfx = 0xFF20,
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
    unsigned long td;  /// Timedelta offset before the current event
    enum EventClass event_class;
    enum EventType event_type;
    unsigned short status;
    unsigned short event_code;
    char* data;  /// Data of the message - represented in bytes
    unsigned int data_len;
};

struct Track {
    int id;
    char *track_name;
    unsigned int n_events;
    struct TrackEvent **events;
};

/// Read a big endian short and swap it
size_t read_short(unsigned short *ptr, size_t size, size_t n, FILE *file) {

    size_t bytes_read = 0;

    bytes_read += fread(ptr, size, n, file);
    *ptr = __builtin_bswap16(*ptr);
    return bytes_read;

}

size_t read_uint32(unsigned int *ptr, size_t size, size_t n, FILE *file) {

    size_t bytes_read = 0;

    bytes_read += fread(ptr, size, n, file);
    *ptr = __builtin_bswap32(*ptr);
    return bytes_read;

}

size_t read_long(unsigned long *ptr, size_t size, size_t n, FILE *file) {

    size_t bytes_read = 0;

    bytes_read += fread(ptr, size, n, file);
    *ptr = __builtin_bswap64(*ptr);
    return bytes_read;

}

/**
 * Read in a given number of bytes in reverse order
 * n_bytes: number of bytes to read in
 * file: file to read from
 * return: char pointer to little-endian
 */
char *read_arr(size_t n_bytes, FILE *file) {

    char *arr = calloc(n_bytes, sizeof(char));

    for (int i = n_bytes - 1; i >= 0; i--) {

        fread(&arr[i], sizeof(char), 1, file);

    }

    return arr;

}

/// Read a variable length value in and convert it to an unsigned long
unsigned long read_vlv(FILE *file, int *bytes_read) {
    unsigned int read_buf = 0;
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

        *bytes_counter += fread(&read_buf, sizeof(char), 1, file);

        // thanks http://www.ccarh.org/courses/253/handout/vlv/

        timedelta <<= 7u;  // ignore the extra bit at the top of the value
        timedelta |= read_buf & 0x7Fu;



        // Keep reading until we've read a 0 as the msb
    } while ((read_buf & 0x7Fu) > 0x80);

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
    int cur_midi_prefix = -1;  // current active midi channel prefix
    unsigned char cur_event_type;
    unsigned char cur_event_class;
    unsigned int data_length;
    unsigned long timedelta;
    unsigned int read_buf;

    int at_track_end = 0;

    struct TrackEvent *event = NULL;
    int event_capacity = 3000;
    struct TrackEvent **track_events = calloc(event_capacity, sizeof(struct TrackEvent));
    int cur_track_event = 0;

    // parse header
    fread(chunk_type, sizeof(char), 4, file);
    chunk_type[4] = '\0';

    if (strcmp(chunk_type, "MTrk") != 0) {
        printf("Invalid chunk type, found %s\n", chunk_type);
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
        bytes_read += fread(&cur_event_class, sizeof(char), 1, file);

        // the first two only have one status byte, and a fixed length.

        if (cur_event_class == 0xF0 || cur_event_class == 0xF7) {

            // sysex event
            event->status = cur_event_class;  // this is significantly shorter
            event->event_class = sysex;
            event->data_len = read_vlv(file, &bytes_read);

            event->data = read_arr(event->data_len, file);
            bytes_read += event->data_len;

            if (cur_event_class == 0xF0)
                event->event_type = s_event;
            else
                event->event_type = s_escape;

            continue;



        }

        // midi events
        else if (cur_event_class < 0xF0) {
            // first byte is event specifier
            event->event_class = midi;
            event->status = cur_event_class;

            // these two events only have one data byte
            if (((cur_event_class & 0xC0u) == 0) || \
                    (cur_event_class & 0xD0u) == 0) {

                event->data_len = 1;
            }

            // these ones are two
            else {

                event->data_len = 2;
            }


            event->data = read_arr(event->data_len, file);
            bytes_read += event->data_len;

            // TODO add enums here
            continue;
        }

        // meta events

        bytes_read += fread(&cur_event_type, sizeof(char), 1, file);

        // bring the values together
        event->status = 0u | ((unsigned int) cur_event_class << 8u) | ((unsigned int) cur_event_type);

        if (event->status >= 0xFF00) {

            // meta events

            event->event_class = meta;
            int event_status = event->status & 0xFFu;  // just pull off the last byte

            // ones with text
            if (event_status <= 8) {
                event->data_len = read_vlv(file, &bytes_read);
                event->data = calloc(event->data_len + 1, sizeof(char));

                // interestingly, text is read in the correct endianness

                bytes_read += fread(event->data, sizeof(char), event->data_len, file);

                // null terminate for good measure
                event->data[event->data_len] = '\0';

                switch (event->status) {
                    case (0xFF00):
                        event->event_code = me_seq_num;
                        break;

                    case (0xFF01):
                        event->event_code = me_text;
                        break;

                    case (0xFF02):
                        event->event_code = me_copyright;
                        break;
                    case (0xFF03):
                        event->event_code = me_seq_name;
                        break;
                    case (0xFF04):
                        event->event_code = me_instr_name;
                        break;
                    case (0xFF05):
                        event->event_code = me_lyric;
                        break;
                    case (0xFF06):
                        event->event_code = me_marker;
                        break;
                    case (0xFF07):
                        event->event_code = me_cue_pt;
                        break;
                    default:
                        fprintf(stderr, "unknown event status");
                }


                continue;
            }

            // sequencer meta event
            // pull this one out since it uses a VLV
            else if (event_status == 0x7F) {

                // sequencer meta event
                event->data_len = read_vlv(file, &bytes_read);

                event->data = read_arr(event->data_len, file);
                bytes_read += event->data_len;

                event->event_type = me_seq_m_event;

                continue;

            }

            else {
                bytes_read += fread(&event->data_len, sizeof(char), 1, file);

                event->data = read_arr(event->data_len, file);
                bytes_read += event->data_len;

                // all the other ones

                switch (event_status) {
                    case 0x20:
                        event->event_code = me_midi_chan_pfx;
                        break;

                    case 0x2F:
                        event->event_code = me_track_end;
                        break;

                    case 0x51:
                        event->event_code = me_set_tempo;
                        break;

                    case 0x54:
                        event->event_code = me_smtpe_offset;
                        break;

                    case 0x58:
                        // time signature
                        event->event_code = me_time_sig;
                        break;

                    case 0x59:
                        event->event_code = me_time_sig;
                        // key signature
                        break;

                    default:
                        fprintf(stderr, "invalid meta event code");


                }
            }

        }

    }

    if (bytes_read != data_length) {
        fprintf(stderr, "Discrepancy exists between bytes read and expected data length");
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

void print_track_events(struct Track *track) {

    for (int i = 0; i < track->n_events; i++) {

        struct TrackEvent *cur_event = track->events[i];

        printf("************\n");
        printf("Event %d:\n", i);
        printf("Class: %d\n", cur_event->event_class);
        printf("td: %#lx\n", cur_event->td);
        printf("status: %#x\n", cur_event->status);
        printf("data:");
        for (int j = 0; j < cur_event->data_len; j++) {
            printf(" %#x", cur_event->data[j] & 0xFFu);

        }
        printf("\n");

        printf("data (chars): %s\n", cur_event->data);



    }

}

// TODO track chunks hold onto all events of a given track
// so the first track has all of its timedeltas and stuff in the first track chunk
// this means that we'd want to load all messages for a specific track
// into a particular arrangement.

int main() {

    FILE *midi = fopen("PMD-Explorers of Sky MIDI RIP/002 - Top Menu Theme.mid", "rb");
    struct Track **tracks;
    if (!midi) {
        perror("Error when loading file");
        return EXIT_FAILURE;
    }
    struct HeaderChunk *header;

    // create header chunk
    header = read_header_chunk(midi);

    // create all the tracks that we will be using
    tracks = calloc(sizeof(size_t), header->n_tracks);

    for (int i = 0; i < header->n_tracks; i++) {

        tracks[i] = create_track(i);
        read_track_events(midi, tracks[i]);
        print_track_events(tracks[i]);

        // now, we'll go through the rest of the midi file.
        // the structure is organized such that there are track chunks for every logical track in the file
        // (think instruments).

    }

    printf("n_tracks: %d, division: %d\n", header->n_tracks, header->division);


    for (unsigned int i = 0; i < header->n_tracks; i++) {

        if (tracks[i] != NULL) {
            destroy_track(tracks[i]);
        }

    }

    free(tracks);

    free(header);


    return 0;
}