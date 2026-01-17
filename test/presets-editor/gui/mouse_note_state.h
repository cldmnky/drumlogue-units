#pragma once

struct MouseNoteState {
  int active_note = -1;
  bool is_down = false;

  int OnMouseClick(int note) {
    int release_note = -1;
    if (is_down && active_note != note) {
      release_note = active_note;
    }
    active_note = note;
    is_down = true;
    return release_note;
  }

  int OnMouseRelease() {
    if (!is_down) {
      return -1;
    }
    const int release_note = active_note;
    active_note = -1;
    is_down = false;
    return release_note;
  }
};
