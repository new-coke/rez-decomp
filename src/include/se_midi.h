/* SE MIDI output: 3-byte messages via ulsnd_handle_send_midi; result in sound_sd_err. */

#ifndef REZ_SE_MIDI_H
#define REZ_SE_MIDI_H

typedef struct MidiEvent {
    int unk_00;                 /* never set by the senders */
    unsigned char status;
    unsigned char data1;
    unsigned char data2;
} MidiEvent;

extern int sound_sd_err;

#endif
