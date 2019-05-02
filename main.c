#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "track.h"

// TODO track chunks hold onto all events of a given track
// so the first track has all of its timedeltas and stuff in the first track chunk
// this means that we'd want to load all messages for a specific track
// into a particular arrangement.

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("usage: main filename");
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "rb");
    struct Track **tracks;
    if (!file) {
        perror("Error when loading file");
        return EXIT_FAILURE;
    }

    struct HeaderChunk *header;
    struct TrackEvent *event;
    char *note;
    char **chords;
    int pos;

    // create header chunk
    header = read_header_chunk(file);

    // create all the tracks that we will be using
    tracks = calloc(sizeof(size_t), header->n_tracks);

    for (int i = 0; i < header->n_tracks; i++) {

        tracks[i] = create_track(i);
        if (tracks[i] == NULL)
            continue;

        read_track_events(file, tracks[i]);

        printf("Note data for track %d\n", i);

        for (int j = 0; j < tracks[i]->n_events; j++) {
            event = tracks[i]->events[j];
            if (event->event_class == midi && ((event->status <= 0xA0u))) {

                note = get_note(event);
//                printf(" %s%d:%s", event->td > 0 ? "\n" : "", event->status & 0xFu, note);
                printf(" %s", note);
                free(note);
            }
        }

        printf("\n");
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