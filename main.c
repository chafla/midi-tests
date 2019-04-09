#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct HeaderChunk {
    short n_tracks;
    short division;
};

struct TrackEvent {
    int td_to_next;  /// Timedelta offset before the next event
    char status;
    char channel_no;
    int data_bytes;  /// Data of the message - one or two bytes
};

struct TrackChunk {
    int length;
    struct TrackEvent *event;
};

struct TrackEvent *read_track_event(FILE *file) {

    struct TrackEvent *event = calloc(1, sizeof(struct TrackEvent));
    char chunk_type[5];

    // parse header
    fread(chunk_type, sizeof(char), 4, file);
    chunk_type[4] = '\0';
    if (strcmp(chunk_type, "MTrk") != 0) {
        printf("Invalid chunk type, found %s\n", chunk_type);
        return NULL;
    }

    // skip chunk type
    fread(chunk_type, sizeof(char), 8, file);



}

/// read in a header chunk
struct HeaderChunk *read_header_chunk(FILE *file) {
    // create a struct
    struct HeaderChunk *chunk = calloc(1, sizeof(struct HeaderChunk));
    int data_len;
    char chunk_type[5];
    // skip over the chunk type
    fread(chunk_type, sizeof(char), 4, file);
    chunk_type[4] = '\0';
    if (strcmp(chunk_type, "MThd") != 0) {
        printf("Invalid chunk type, found %s\n", chunk_type);
        return NULL;
    }
    printf("Chunk type: %s\n", chunk_type);
//    fseek(file, 4, SEEK_CUR);

    // it's just 6 bytes

    // skip format
    fseek(file, 2, SEEK_CUR);

    fread(&(chunk->n_tracks), 2, 1, file);
    fread(&(chunk->division), 2, 1, file);

    return chunk;

}

// TODO track chunks hold onto all events of a given track
// so the first track has all of its timedeltas and stuff in the first track chunk
// this means that we'd want to load all messages for a specific track
// into a particular arrangement.

int main() {

    FILE *midi = fopen("002 - Top Menu Theme.mid", "rb");
    struct TrackEvent *events;
    if (!midi) {
        perror("Error when loading file");
        return EXIT_FAILURE;
    }
    struct HeaderChunk *header;

    header = read_header_chunk(midi);
    events = calloc(sizeof(struct TrackEvent), header->n_tracks);
    for (int i = 0; i < header->n_tracks; i++) {

        events[i] =

    }

    printf("n_tracks: %d, division: %d\n", header->n_tracks, header->division);

    printf("Hello, World!\n");
    return 0;
}