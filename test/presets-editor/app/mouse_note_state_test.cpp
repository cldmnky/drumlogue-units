#include <cstdio>
#include "../gui/mouse_note_state.h"

int main() {
  MouseNoteState state;

  int release = state.OnMouseClick(60);
  if (release != -1 || !state.is_down || state.active_note != 60) {
    std::fprintf(stderr, "FAIL: initial click did not set state\n");
    return 1;
  }

  release = state.OnMouseClick(69);
  if (release != 60 || !state.is_down || state.active_note != 69) {
    std::fprintf(stderr, "FAIL: second click did not release previous note\n");
    return 1;
  }

  release = state.OnMouseRelease();
  if (release != 69 || state.is_down || state.active_note != -1) {
    std::fprintf(stderr, "FAIL: mouse release did not clear active note\n");
    return 1;
  }

  std::printf("mouse note state tests passed\n");
  return 0;
}
