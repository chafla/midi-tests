#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <byteswap.h>

struct HeaderChunk {
    int format;
    unsigned short n_tracks;
    unsigned short division;
};

enum EventType{midi, meta, sysex};

struct TrackEvent {
    unsigned long td;  /// Timedelta offset before the current event
    enum EventType event_type;
    unsigned short status;
    unsigned short event_code;
    char* data;  /// Data of the message - represented in bytes
    unsigned int data_len;
};

struct Track {
    int id;
    char *track_name;
    struct TrackEvent **events;
};

/// Read a big endian short and swap it
void read_short(unsigned short *ptr, size_t size, size_t n, FILE *file) {

    fread(ptr, size, n, file);
    *ptr = __builtin_bswap16(*ptr);

}

void read_uint32(unsigned int *ptr, size_t size, size_t n, FILE *file) {

    fread(ptr, size, n, file);
    *ptr = __builtin_bswap32(*ptr);

}

void read_long(unsigned long *ptr, size_t size, size_t n, FILE *file) {

    fread(ptr, size, n, file);
    *ptr = __builtin_bswap64(*ptr);

}

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
    } while (read_buf & 0x80u);

    return timedelta;
}

/**
 * Read in a track event and add it to its corresponding track
 * @param file: file pointer to read from
 * @param tracks: the track to read the track event from
 */
void read_track_events(FILE *file, struct Track *track) {

    char vlv_timedelta[4];

    char chunk_type[5];
    int bytes_read = 0;
    unsigned int data_length;
    unsigned long timedelta;
    unsigned int read_buf;

    struct TrackEvent *event = NULL;
    struct TrackEvent **track_events;

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
    track_events = calloc(data_length, sizeof(size_t));

    // here's where the fun begins
    // we need to run through data_length bytes and extract events

    while (bytes_read < data_length) {

        read_buf = 0;

        event = calloc(1, sizeof(struct TrackEvent));

        timedelta = 0;

        // read in the timedelta between the past event and the current event
        // it's formatted in a variable length, so the lower 7 bits are data.
        // a 0 in the msb of a byte (like 01111111) means that the byte is the last in a variable-length number

        do {

            bytes_read += fread(&read_buf, sizeof(char), 1, file);

            // thanks http://www.ccarh.org/courses/253/handout/vlv/
            // also: http://www.ccarh.org/courses/253/handout/vlv/vlv.cpp

            timedelta <<= 7u;  // ignore the extra bit at the top of the value
            timedelta |= read_buf & 0x7Fu;



            // Keep reading until we've read the 0 bit
        } while ((read_buf & (unsigned) 0x7F) > 0x80);

        event->td = timedelta;

        // status byte

        read_short(&event->status, sizeof(char), 2, file);
        // fread(&event->status, sizeof(int), 1, file);

        // midi event
        if (event->status <= 0xF000) {
            // first byte is event specifier, second is
            event->event_type = midi;
            int event_code = event->status >> 8u;
            // these two events only have one data byte
            if (event_code == 0xC || event_code == 0xD) {
                event->data = calloc(1, sizeof(int));
                event->data_len = 2;
                // TODO patch this little bit up. We'll need to calloc and drop it into data bytes
                fread(&event->data[1], sizeof(int), 1, file);
            }
            else {
                event->data = calloc(2, sizeof(int));
                event->data_len = 4;
                fread(event->data, sizeof(int), 2, file);
            }
        }

        // meta events
        else if (event->status >= 0xFF00) {

            event->event_type = meta;
            int event_status = event->status & 0xFFu;  // just pull off the last byte

            if (event_status <= 8) {

                // ones with text

                event->data_len = read_vlv(file, &bytes_read);
                event->data = calloc(event->data_len, sizeof(char));

                bytes_read += fread(event->data, sizeof(char), event->data_len, file);
                continue;
            }

            // all the other ones

            switch (event_status) {

                case 0x20:
                    // midi channel prefix
                    break;

                case 0x2F:
                    // end of track
                    break;

                case 0x51:
                    // set tempo
                    break;

                case 0x54:
                    // SMTPE offset
                    break;

                case 0x58:
                    // time signature
                    break;

                case 0x59:
                    // key signature
                    break;

                case 0x7F:
                    // sequencer meta event
                    break;

                default:
                    fprintf(stderr, "invalid meta event code");


            }

        }

    }

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
//    fseek(file, sizeof(char) * 4, SEEK_CUR);

    printf("Header information: n_tracks: %#x, division: %#x, format: %#x\n",
            chunk->n_tracks, chunk->division, chunk->format);

    return chunk;

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

        // now, we'll go through the rest of the midi file.
        // the structure is organized such that there are track chunks for every logical track in the file
        // (think instruments).

    }



    printf("n_tracks: %d, division: %d\n", header->n_tracks, header->division);

    return 0;
}