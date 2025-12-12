#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>
#include <dlfcn.h>
#include <time.h>
#include <math.h>

#include "../qemu-arm/unit_host.h"
#include "../qemu-arm/sdk_stubs.h"
#include "pattern.h"

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 256
#define CHANNELS 2

// UI state
typedef struct {
    int selected_param;
    int selected_step;
    bool editing_pattern;
    bool running;
} ui_state_t;

static ui_state_t g_ui_state = {0};
static pattern_t g_pattern = {0};
static unit_host_state_t* g_unit_state = NULL;
static uint32_t g_frames_until_next_step = 0;
static uint8_t g_last_note = 0;

// Signal handler for clean exit
static void signal_handler(int sig) {
    (void)sig;
    g_ui_state.running = false;
}

// Draw the UI
static void draw_ui(unit_host_state_t* state) {
    clear();
    
    // Title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(0, 0, "╔════════════════════════════════════════════════════════════════════════╗");
    mvprintw(1, 0, "║          DRUMLOGUE UNIT PLAYER - ARM Emulation Mode                   ║");
    mvprintw(2, 0, "╚════════════════════════════════════════════════════════════════════════╗");
    attroff(COLOR_PAIR(1) | A_BOLD);
    
    // Unit info
    if (state && state->unit_header) {
        mvprintw(3, 2, "Unit: %s", state->unit_header->name);
        mvprintw(3, 50, "BPM: %u", g_pattern.tempo_bpm);
    }
    
    // Pattern display
    mvprintw(5, 2, "16-STEP PATTERN:");
    mvprintw(6, 2, "╔");
    for (int i = 0; i < PATTERN_STEPS; i++) {
        addstr("═══");
        if (i < PATTERN_STEPS - 1) addstr("╤");
    }
    addstr("╗");
    
    // Pattern steps
    mvprintw(7, 2, "║");
    for (int i = 0; i < PATTERN_STEPS; i++) {
        if (g_pattern.current_step == i && g_pattern.playing) {
            attron(COLOR_PAIR(3) | A_BOLD);
        } else if (i == g_ui_state.selected_step && g_ui_state.editing_pattern) {
            attron(COLOR_PAIR(2));
        }
        
        if (g_pattern.steps[i].active) {
            const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            uint8_t note = g_pattern.steps[i].note;
            int octave = (note / 12) - 1;
            printw(" %s%d", note_names[note % 12], octave);
        } else {
            addstr(" --");
        }
        
        if (g_pattern.current_step == i && g_pattern.playing) {
            attroff(COLOR_PAIR(3) | A_BOLD);
        } else if (i == g_ui_state.selected_step && g_ui_state.editing_pattern) {
            attroff(COLOR_PAIR(2));
        }
        
        if (i < PATTERN_STEPS - 1) addstr("│");
    }
    addstr("║");
    
    mvprintw(8, 2, "╚");
    for (int i = 0; i < PATTERN_STEPS; i++) {
        addstr("═══");
        if (i < PATTERN_STEPS - 1) addstr("╧");
    }
    addstr("╝");
    
    // Parameters
    mvprintw(10, 2, "PARAMETERS:");
    if (state && state->unit_header) {
        int visible_params = 0;
        for (uint32_t i = 0; i < state->unit_header->num_params && visible_params < 10; i++) {
            unit_param_t* param = &state->unit_header->params[i];
            if (param->type == k_unit_param_type_none) continue;
            
            int y = 11 + visible_params;
            
            if (i == (uint32_t)g_ui_state.selected_param && !g_ui_state.editing_pattern) {
                attron(COLOR_PAIR(2));
            }
            
            mvprintw(y, 2, "[%2u] %-12s ", i, param->name);
            
            // Draw parameter slider
            int32_t value = state->param_values[i];
            int range = param->max - param->min;
            if (range > 0) {
                int bar_width = 30;
                int filled = (int)((float)(value - param->min) / range * bar_width);
                addstr("[");
                for (int b = 0; b < bar_width; b++) {
                    if (b < filled) addstr("█");
                    else addstr("░");
                }
                printw("] %d", value);
            }
            
            if (i == (uint32_t)g_ui_state.selected_param && !g_ui_state.editing_pattern) {
                attroff(COLOR_PAIR(2));
            }
            
            visible_params++;
        }
    }
    
    // Controls
    int control_y = 22;
    attron(COLOR_PAIR(1));
    mvprintw(control_y, 2, "CONTROLS:");
    attroff(COLOR_PAIR(1));
    mvprintw(control_y + 1, 2, "TAB: Switch mode  │  ←/→: Navigate  │  ↑/↓: Adjust value");
    mvprintw(control_y + 2, 2, "SPACE: %s  │  R: Reset pattern  │  Q: Quit", 
             g_pattern.playing ? "Stop" : "Play");
    mvprintw(control_y + 3, 2, "Mode: %s", 
             g_ui_state.editing_pattern ? "PATTERN EDIT" : "PARAMETERS");
    
    refresh();
}

// Process audio and advance pattern
static void process_audio_callback(float* output_buffer, uint32_t frames) {
    if (!g_unit_state || !g_callbacks.unit_render) {
        memset(output_buffer, 0, frames * CHANNELS * sizeof(float));
        return;
    }
    
    // Silence input for synths
    static float input_buffer[BUFFER_SIZE * 2] = {0};
    
    // Check if we need to advance pattern
    if (g_pattern.playing) {
        if (g_frames_until_next_step == 0) {
            // Advance to next step
            pattern_step_t step;
            if (pattern_advance(&g_pattern, &step)) {
                // Send note off for previous note
                if (g_last_note > 0 && g_callbacks.unit_note_off) {
                    g_callbacks.unit_note_off(g_last_note);
                }
                
                // Send note on for current step
                if (step.active && step.note > 0 && g_callbacks.unit_note_on) {
                    g_callbacks.unit_note_on(step.note, step.velocity);
                    g_last_note = step.note;
                }
            }
            
            g_frames_until_next_step = pattern_step_frames(&g_pattern, SAMPLE_RATE);
        }
        
        if (g_frames_until_next_step >= frames) {
            g_frames_until_next_step -= frames;
        } else {
            g_frames_until_next_step = 0;
        }
    }
    
    // Render audio
    g_callbacks.unit_render(input_buffer, output_buffer, frames);
}

// Main audio loop - writes to stdout for piping to audio player
static void audio_loop(void) {
    float output_buffer[BUFFER_SIZE * CHANNELS];
    
    // Write WAV header to stdout for streaming
    // Simple WAV header for infinite stream
    struct {
        char riff[4];
        uint32_t file_size;
        char wave[4];
        char fmt[4];
        uint32_t fmt_size;
        uint16_t format;
        uint16_t channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits;
        char data[4];
        uint32_t data_size;
    } wav_header = {
        {'R','I','F','F'}, 0xFFFFFFFF, {'W','A','V','E'},
        {'f','m','t',' '}, 16, 3, CHANNELS, SAMPLE_RATE,
        SAMPLE_RATE * CHANNELS * 4, CHANNELS * 4, 32,
        {'d','a','t','a'}, 0xFFFFFFFF
    };
    
    if (write(STDOUT_FILENO, &wav_header, sizeof(wav_header)) < 0) {
        return;
    }
    
    while (g_ui_state.running) {
        // Process audio
        process_audio_callback(output_buffer, BUFFER_SIZE);
        
        // Write to stdout
        ssize_t written = write(STDOUT_FILENO, output_buffer, 
                               BUFFER_SIZE * CHANNELS * sizeof(float));
        if (written < 0) {
            break;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <unit.drmlgunit>\n", argv[0]);
        return 1;
    }
    
    // Initialize SDK stubs
    sdk_stubs_init();
    
    // Initialize unit host
    unit_host_config_t config = {
        .sample_rate = SAMPLE_RATE,
        .buffer_size = BUFFER_SIZE,
        .channels = CHANNELS,
        .verbose = false,
        .profile = false
    };
    
    g_unit_state = unit_host_init(&config);
    if (!g_unit_state) {
        fprintf(stderr, "Failed to initialize unit host\n");
        return 1;
    }
    
    // Load unit
    if (unit_host_load_unit(argv[1], g_unit_state) != UNIT_HOST_OK) {
        fprintf(stderr, "Failed to load unit: %s\n", argv[1]);
        return 1;
    }
    
    // Initialize pattern
    pattern_init(&g_pattern);
    
    // Setup ncurses (only if not piping audio)
    if (isatty(STDERR_FILENO)) {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);
        curs_set(0);
        
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
        init_pair(3, COLOR_GREEN, COLOR_BLACK);
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    g_ui_state.running = true;
    
    // Fork: child handles audio, parent handles UI
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child: audio thread
        if (isatty(STDERR_FILENO)) {
            endwin();
        }
        audio_loop();
        exit(0);
    } else {
        // Parent: UI thread
        struct timespec last_draw = {0};
        
        while (g_ui_state.running) {
            // Draw UI at ~30fps
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double elapsed = (now.tv_sec - last_draw.tv_sec) + 
                           (now.tv_nsec - last_draw.tv_nsec) / 1e9;
            
            if (elapsed > 0.033 && isatty(STDERR_FILENO)) {  // ~30fps
                draw_ui(g_unit_state);
                last_draw = now;
            }
            
            // Handle input
            int ch = getch();
            if (ch != ERR) {
                switch (ch) {
                    case 'q':
                    case 'Q':
                        g_ui_state.running = false;
                        break;
                    
                    case '\t':  // Tab: switch mode
                        g_ui_state.editing_pattern = !g_ui_state.editing_pattern;
                        break;
                    
                    case ' ':  // Space: play/stop
                        g_pattern.playing = !g_pattern.playing;
                        if (!g_pattern.playing && g_last_note > 0) {
                            if (g_callbacks.unit_note_off) {
                                g_callbacks.unit_note_off(g_last_note);
                            }
                            g_last_note = 0;
                        }
                        break;
                    
                    case 'r':
                    case 'R':  // Reset pattern
                        pattern_clear(&g_pattern);
                        pattern_init(&g_pattern);
                        break;
                    
                    case KEY_LEFT:
                        if (g_ui_state.editing_pattern) {
                            g_ui_state.selected_step = (g_ui_state.selected_step - 1 + PATTERN_STEPS) % PATTERN_STEPS;
                        }
                        break;
                    
                    case KEY_RIGHT:
                        if (g_ui_state.editing_pattern) {
                            g_ui_state.selected_step = (g_ui_state.selected_step + 1) % PATTERN_STEPS;
                        } else {
                            g_ui_state.selected_param = (g_ui_state.selected_param + 1) % 24;
                        }
                        break;
                    
                    case KEY_UP:
                        if (g_ui_state.editing_pattern) {
                            // Increase note
                            pattern_step_t* step = &g_pattern.steps[g_ui_state.selected_step];
                            if (step->note < 127) {
                                step->note++;
                                step->active = true;
                            }
                        } else {
                            // Increase parameter
                            if (g_unit_state && g_ui_state.selected_param < 24) {
                                unit_param_t* param = &g_unit_state->unit_header->params[g_ui_state.selected_param];
                                int32_t value = g_unit_state->param_values[g_ui_state.selected_param];
                                if (value < param->max) {
                                    value++;
                                    unit_host_set_param(g_unit_state, g_ui_state.selected_param, value);
                                }
                            }
                        }
                        break;
                    
                    case KEY_DOWN:
                        if (g_ui_state.editing_pattern) {
                            // Decrease note
                            pattern_step_t* step = &g_pattern.steps[g_ui_state.selected_step];
                            if (step->note > 0) {
                                step->note--;
                                if (step->note == 0) step->active = false;
                            }
                        } else {
                            // Decrease parameter
                            if (g_unit_state && g_ui_state.selected_param < 24) {
                                unit_param_t* param = &g_unit_state->unit_header->params[g_ui_state.selected_param];
                                int32_t value = g_unit_state->param_values[g_ui_state.selected_param];
                                if (value > param->min) {
                                    value--;
                                    unit_host_set_param(g_unit_state, g_ui_state.selected_param, value);
                                }
                            }
                        }
                        break;
                    
                    case '+':
                    case '=':
                        if (g_pattern.tempo_bpm < 300) g_pattern.tempo_bpm += 5;
                        break;
                    
                    case '-':
                    case '_':
                        if (g_pattern.tempo_bpm > 40) g_pattern.tempo_bpm -= 5;
                        break;
                }
            }
            
            usleep(10000);  // 10ms sleep
        }
        
        // Cleanup
        if (isatty(STDERR_FILENO)) {
            endwin();
        }
        
        // Kill audio child
        kill(pid, SIGTERM);
    }
    
    sdk_stubs_cleanup();
    return 0;
}
