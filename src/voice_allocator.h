// voice_allocator.h — Polyphonic voice slot manager (steal-oldest policy)
// Header-only, no Daisy dependencies. Matches ms20_filter.h portability.

#pragma once
#include <cstdint>

template <int N>
class VoiceAllocator {
public:
    void Init() {
        for (int i = 0; i < N; i++) {
            slots_[i].midi_note = -1;
            slots_[i].age = 0;
        }
        age_counter_ = 0;
    }

    // Returns voice index (0..N-1) to trigger.
    // Priority: (1) retrigger same note, (2) free slot, (3) steal oldest.
    int NoteOn(int midi_note) {
        age_counter_++;

        // Retrigger if this note is already playing
        for (int i = 0; i < N; i++) {
            if (slots_[i].midi_note == midi_note) {
                slots_[i].age = age_counter_;
                return i;
            }
        }

        // Use a free slot
        for (int i = 0; i < N; i++) {
            if (slots_[i].midi_note == -1) {
                slots_[i].midi_note = midi_note;
                slots_[i].age = age_counter_;
                return i;
            }
        }

        // All slots busy — steal the oldest
        int oldest = 0;
        for (int i = 1; i < N; i++) {
            if (slots_[i].age < slots_[oldest].age)
                oldest = i;
        }
        slots_[oldest].midi_note = midi_note;
        slots_[oldest].age = age_counter_;
        return oldest;
    }

    // Returns voice index that was released, or -1 if note not found
    // (already stolen or duplicate NoteOff).
    int NoteOff(int midi_note) {
        for (int i = 0; i < N; i++) {
            if (slots_[i].midi_note == midi_note) {
                slots_[i].midi_note = -1;
                return i;
            }
        }
        return -1;
    }

    // True if any voice slot is gated (has an assigned note).
    bool AnyGated() const {
        for (int i = 0; i < N; i++) {
            if (slots_[i].midi_note != -1) return true;
        }
        return false;
    }

private:
    struct Slot {
        int midi_note;      // -1 = free
        uint32_t age;       // monotonic counter; higher = newer
    };

    Slot slots_[N];
    uint32_t age_counter_;
};
