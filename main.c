#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct HeaderChunk {
    int format;
    int n_tracks;
    int division;
};

enum EventType{midi, meta, sysex};

struct TrackEvent {
    unsigned long td;  /// Timedelta offset before the current event
    enum EventType event_type;
    int status;
    int data_bytes[2];  /// Data of the message - one or two bytes
};

struct Track {
    int id;
    char *track_name;
    struct TrackEvent **events;
};

// TODO will need to track meta events too

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

    struct TrackEvent *event = calloc(1, sizeof(struct TrackEvent));

    // parse header
    fread(chunk_type, sizeof(char), 4, file);
    chunk_type[4] = '\0';

    if (strcmp(chunk_type, "MTrk") != 0) {
        printf("Invalid chunk type, found %s\n", chunk_type);
        return;
    }

    // get the number of bytes contained by data segment
    fread(&data_length, sizeof(int), 1, file);  // 4 bytes

    // here's where the fun begins
    // we need to run through data_length bytes and extract events

    while (bytes_read < data_length) {

        timedelta = 0;

        // read in the timedelta between the past event and the current event
        // it's formatted in a variable length, so the lower 7 bits are data.
        // a 0 in the msb of a byte (like 01111111) means that the byte is the last in a variable-length number

        do {

            bytes_read += fread(&read_buf, sizeof(char), 1, file);

            // thanks http://www.ccarh.org/courses/253/handout/vlv/

            timedelta <<= (unsigned) 7;  // ignore the extra bit at the top of the value
            timedelta |= (read_buf & (unsigned) 0x7F);



            // Keep reading until we've read the 0 bit
        } while (read_buf & (unsigned) 0x7);

        event->td = timedelta;

        // status byte

        fread(&event->status, sizeof(int), 1, file);

        // midi event
        if (event->status <= 0xF0) {
            // first byte is event specifier, second is
            event->event_type = midi;
            int event_code = event->status >> 8;
            // these two events only have one data byte
            if (event_code == 0xC || event_code == 0xD) {
                fread(&event->data_bytes[1], sizeof(int), 1, file);
            }
            else {
                fread(event->data_bytes, sizeof(int), 2, file);
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

// TODO MIDI IS BIG ENDIAN
// this means that we'll need to perform a swap every time we try to read in a value

/// read in a header chunk
struct HeaderChunk *read_header_chunk(FILE *file) {
    // create a struct
    struct HeaderChunk *chunk = calloc(1, sizeof(struct HeaderChunk));
    char chunk_type[5];
    int chunk_len;
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

    // it's just 6 bytes

    // get midi file format
    fread(&chunk->format, sizeof(char), 2, file);

    fread(&(chunk->n_tracks), sizeof(char), 2, file);
    fread(&(chunk->division), sizeof(char), 2, file);
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