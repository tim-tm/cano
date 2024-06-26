#include "main.h"

#include <locale.h>
#include "cgetopt.h"

char *string_modes[MODE_COUNT] = {"NORMAL", "INSERT", "SEARCH", "COMMAND", "VISUAL"};
_Static_assert(sizeof(string_modes)/sizeof(*string_modes) == MODE_COUNT, "Number of modes");

void load_config_from_file(State *state, Buffer *buffer, char *config_filename, char *syntax_filename);

Brace find_opposite_brace(char opening) {
    switch(opening) {
        case '(':
            return (Brace){.brace = ')', .closing = 0};
            break;
        case '{':
            return (Brace){.brace = '}', .closing = 0};
            break;
        case '[':
            return (Brace){.brace = ']', .closing = 0};
            break;
        case ')':
            return (Brace){.brace = '(', .closing = 1};
            break;
        case '}':
            return (Brace){.brace = '{', .closing = 1};
            break;
        case ']':
            return (Brace){.brace = '[', .closing = 1};
            break;
		default:
		    return (Brace){.brace = '0'};		
    }
}

int contains_c_extension(const char *str) {
    const char *extension = ".c";
    size_t str_len = strlen(str);
    size_t extension_len = strlen(extension);

    if (str_len >= extension_len) {
        const char *suffix = str + (str_len - extension_len);
        if (strcmp(suffix, extension) == 0) {
            return 1;
        }
    }

    return 0;
}

void *check_for_errors(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;

    bool loop = 1; /* loop to be used later on, to make it constantly check for errors. Right now it just runs once. */
    while (loop) {

        char path[1035];

        /* Open the command for reading. */
        char command[1024];
        sprintf(command, "gcc %s -o /dev/null -Wall -Wextra -Werror -std=c99 2> errors.cano && echo $? > success.cano", threadArgs->path_to_file);
        FILE *fp = popen(command, "r");
        if (fp == NULL) {
            loop = 0;
            static char return_message[] = "Failed to run command";
            WRITE_LOG("Failed to run command");
            return (void *)return_message;
        }
        pclose(fp);

        FILE *should_check_for_errors = fopen("success.cano", "r");

        if (should_check_for_errors == NULL) {
            loop = 0;
            WRITE_LOG("Failed to open file");
            return (void *)NULL;
        }
        while (fgets(path, sizeof(path) -1, should_check_for_errors) != NULL) {
            WRITE_LOG("return code: %s", path);
            if (!(strcmp(path, "0") == 0)) {
                FILE *file_contents = fopen("errors.cano", "r");
                if (fp == NULL) {
                    loop = 0;
                    WRITE_LOG("Failed to open file");
                    return (void *)NULL;
                }

                fseek(file_contents, 0, SEEK_END);
                long filesize = ftell(file_contents);
                fseek(file_contents, 0, SEEK_SET);

                char *buffer = malloc(filesize + 1);
                if (buffer == NULL) {
                    WRITE_LOG("Failed to allocate memory");
                    return (void *)NULL;
                }
                fread(buffer, 1, filesize, file_contents);
                buffer[filesize] = '\0';

                char *bufffer = malloc(filesize + 1);

                while (fgets(path, sizeof(path) -1, file_contents) != NULL) {
                    strcat(bufffer, path);
                    strcat(buffer, "\n");
                }

                char *return_message = malloc(filesize + 1);
                if (return_message == NULL) {
                    WRITE_LOG("Failed to allocate memory");
                    free(buffer);
                    return (void *)NULL;
                }
                strcpy(return_message, buffer);

                free(buffer); 
                loop = 0;
                fclose(file_contents);

                return (void *)return_message;
            }
            else {
                loop = 0;
                static char return_message[] = "No errors found";
                return (void *)return_message;
            }
        }

    }

    return (void *)NULL;
}


Ncurses_Color rgb_to_ncurses(int r, int g, int b) {

    Ncurses_Color color = {0};

    color.r = (int) ((r / 256.0) * 1000);
    color.g = (int) ((g / 256.0) * 1000);
    color.b = (int) ((b / 256.0) * 1000);
    return color;

}
    
void init_ncurses_color(int id, int r, int g, int b) {
        Ncurses_Color color = rgb_to_ncurses(r, g, b);
        init_color(id, color.r, color.g, color.b);
}


void free_buffer(Buffer *buffer) {
    free(buffer->data.data);
    free(buffer->rows.data);
    free(buffer->filename);
    buffer->data.count = 0;
    buffer->rows.count = 0;
    buffer->data.capacity = 0;
    buffer->rows.capacity = 0;
}

void free_undo(Undo *undo) {
    free(undo->data.data);
}

void free_undo_stack(Undo_Stack *undo) {
    for(size_t i = 0; i < undo->count; i++) {
        free_undo(&undo->data[i]);
    }
    free(undo->data);
}


size_t search(Buffer *buffer, char *command, size_t command_s) {
    for(size_t i = buffer->cursor+1; i < buffer->data.count+buffer->cursor; i++) {
        size_t pos = i % buffer->data.count;
        if(strncmp(buffer->data.data+pos, command, command_s) == 0) {
            // result found and will return the location of the word.
            return pos;
        }
    }
    return buffer->cursor;
}

void replace(Buffer *buffer, State *state, char *new_str, size_t old_str_s, size_t new_str_s) { 
    if (buffer == NULL || new_str == NULL) {
        WRITE_LOG("Error: null pointer");
        return;
    }

    for(size_t i = 0; i < old_str_s; i++) {
        buffer_delete_char(buffer, state);
    }

    for(size_t i = 0; i < new_str_s; i++) {
        buffer_insert_char(state, buffer, new_str[i]);
    }
}


void find_and_replace(Buffer *buffer, State *state, char *old_str, char *new_str) { 
    size_t old_str_s = strlen(old_str);
    size_t new_str_s = strlen(new_str);

    // Search for the old string in the buffer
    size_t position = search(buffer, old_str, old_str_s);
    if (position != (buffer->cursor)) {
        buffer->cursor = position;
        // If the old string is found, replace it with the new string
        replace(buffer, state, new_str, old_str_s, new_str_s);
    }
}
    
void reset_command(char *command, size_t *command_s) {
    memset(command, 0, *command_s);
    *command_s = 0;
}


void handle_save(Buffer *buffer) {
    FILE *file = fopen(buffer->filename, "w"); 
    fwrite(buffer->data.data, buffer->data.count, sizeof(char), file);
    fclose(file);
}

Buffer *load_buffer_from_file(char *filename) {
    Buffer *buffer = calloc(1, sizeof(Buffer));
    size_t filename_s = strlen(filename)+1; 
    buffer->filename = calloc(filename_s, sizeof(char));
    strncpy(buffer->filename, filename, filename_s);
    FILE *file = fopen(filename, "a+");
    if(file == NULL) CRASH("Could not open file");
    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer->data.count = length;
    buffer->data.capacity = (length+1)*2;
    buffer->data.data = calloc(buffer->data.capacity+1, sizeof(char));
	ASSERT(buffer->data.data != NULL, "buffer allocated properly");
    fread(buffer->data.data, length, 1, file);
    fclose(file);
    buffer_calculate_rows(buffer);
    return buffer;
}

void shift_str_left(char *str, size_t *str_s, size_t index) {
    for(size_t i = index; i < *str_s; i++) {
        str[i] = str[i+1];
    }
    *str_s -= 1;
}

void shift_str_right(char *str, size_t *str_s, size_t index) {
    *str_s += 1;
    for(size_t i = *str_s; i > index; i--) {
        str[i] = str[i-1];
    }
}

void undo_push(State *state, Undo_Stack *stack, Undo undo) {
    DA_APPEND(stack, undo);
    state->cur_undo = (Undo){0};
}

Undo undo_pop(Undo_Stack *stack) {
    if(stack->count <= 0) return (Undo){0};
    return stack->data[--stack->count];
}

void buffer_calculate_rows(Buffer *buffer) {
    buffer->rows.count = 0;
    size_t start = 0;
    for(size_t i = 0; i < buffer->data.count; i++) {
        if(buffer->data.data[i] == '\n') {
            DA_APPEND(&buffer->rows, ((Row){.start = start, .end = i}));
            start = i + 1;
        }
    }

    DA_APPEND(&buffer->rows, ((Row){.start = start, .end = buffer->data.count}));
}

void buffer_insert_char(State *state, Buffer *buffer, char ch) {
	ASSERT(buffer != NULL, "buffer exists");
	ASSERT(state != NULL, "state exists");		
    if(buffer->cursor > buffer->data.count) buffer->cursor = buffer->data.count;
    DA_APPEND(&buffer->data, ch);
    memmove(&buffer->data.data[buffer->cursor + 1], &buffer->data.data[buffer->cursor], buffer->data.count - 1 - buffer->cursor);
    buffer->data.data[buffer->cursor] = ch;
    buffer->cursor++;
    state->cur_undo.end = buffer->cursor;
    buffer_calculate_rows(buffer);
}

void buffer_delete_char(Buffer *buffer, State *state) {
    if(buffer->cursor < buffer->data.count) {
        DA_APPEND(&state->cur_undo.data, buffer->data.data[buffer->cursor]);
        memmove(&buffer->data.data[buffer->cursor], &buffer->data.data[buffer->cursor+1], buffer->data.count - buffer->cursor - 1);
        buffer->data.count--;
        buffer_calculate_rows(buffer);
    }
}

size_t buffer_get_row(const Buffer *buffer) {
    ASSERT(buffer->cursor <= buffer->data.count, "cursor: %zu", buffer->cursor);
    ASSERT(buffer->rows.count >= 1, "there must be at least one line");
    for(size_t i = 0; i < buffer->rows.count; i++) {
        if(buffer->rows.data[i].start <= buffer->cursor && buffer->cursor <= buffer->rows.data[i].end) {
            return i;
        }
    }
    return 0;
}

size_t index_get_row(Buffer *buffer, size_t index) {
    ASSERT(index <= buffer->data.count, "index: %zu", index);
    ASSERT(buffer->rows.count >= 1, "there must be at least one line");
    for(size_t i = 0; i < buffer->rows.count; i++) {
        if(buffer->rows.data[i].start <= index && index <= buffer->rows.data[i].end) {
            return i;
        }
    }
    return 0;
}

void buffer_yank_line(Buffer *buffer, State *state, size_t offset) {
    size_t row = buffer_get_row(buffer);
    if(offset > index_get_row(buffer, buffer->data.count)) return;
    Row cur = buffer->rows.data[row+offset];
    size_t initial_s = state->clipboard.len;
    state->clipboard.len = cur.end - cur.start + 1; // account for new line
    state->clipboard.str = realloc(state->clipboard.str, 
                                   initial_s+state->clipboard.len*sizeof(char));
    if(state->clipboard.str == NULL) CRASH("null");
    strncpy(state->clipboard.str+initial_s, buffer->data.data+cur.start, state->clipboard.len);
    state->clipboard.len += initial_s;
}

void buffer_yank_char(Buffer *buffer, State *state) {
    reset_command(state->clipboard.str, &state->clipboard.len);
    state->clipboard.len = 2; 
    state->clipboard.str = realloc(state->clipboard.str, 
                                   state->clipboard.len*sizeof(char));
    if(state->clipboard.str == NULL) CRASH("null");
    strncpy(state->clipboard.str, buffer->data.data+buffer->cursor, state->clipboard.len);
}

void buffer_yank_selection(Buffer *buffer, State *state, size_t start, size_t end) {
    state->clipboard.len = end-start+2;
    state->clipboard.str = realloc(state->clipboard.str, 
                                   state->clipboard.len*sizeof(char));
    if(state->clipboard.str == NULL) CRASH("null");
    strncpy(state->clipboard.str, buffer->data.data+start, state->clipboard.len);
}

void buffer_delete_selection(Buffer *buffer, State *state, size_t start, size_t end) {
    int count = end - start;
    buffer_yank_selection(buffer, state, start, end);
    buffer->cursor = start;
    while(count >= 0) {
        buffer_delete_char(buffer, state);
        count--;
    }
}

void buffer_move_up(Buffer *buffer) {
    size_t row = buffer_get_row(buffer);
    size_t col = buffer->cursor - buffer->rows.data[row].start;
    if(row > 0) {
        buffer->cursor = buffer->rows.data[row-1].start + col;
        if(buffer->cursor > buffer->rows.data[row-1].end) {
            buffer->cursor = buffer->rows.data[row-1].end;
        }
    }
}

void buffer_move_down(Buffer *buffer) {
    size_t row = buffer_get_row(buffer);
    size_t col = buffer->cursor - buffer->rows.data[row].start;
    if(row < buffer->rows.count - 1) {
        buffer->cursor = buffer->rows.data[row+1].start + col;
        if(buffer->cursor > buffer->rows.data[row+1].end) {
            buffer->cursor = buffer->rows.data[row+1].end;
        }
    }
}

void buffer_move_right(Buffer *buffer) {
    if(buffer->cursor < buffer->data.count) buffer->cursor++;
}

void buffer_move_left(Buffer *buffer) {
    if(buffer->cursor > 0) buffer->cursor--;
}

int skip_to_char(Buffer *buffer, int cur_pos, int direction, char c) {
    if(buffer->data.data[cur_pos] == c) {
        cur_pos += direction;
        while(cur_pos > 0 && cur_pos <= (int)buffer->data.count && buffer->data.data[cur_pos] != c) {
            if(cur_pos > 1 && cur_pos < (int)buffer->data.count && buffer->data.data[cur_pos] == '\\') {
                cur_pos += direction;
            }
            cur_pos += direction;
        }
    }
    return cur_pos;
}

void buffer_next_brace(Buffer *buffer) {
    int cur_pos = buffer->cursor;
    Brace initial_brace = find_opposite_brace(buffer->data.data[cur_pos]);
    size_t brace_stack = 0;
    if(initial_brace.brace == '0') return;
    int direction = (initial_brace.closing) ? -1 : 1;
    while(cur_pos >= 0 && cur_pos <= (int)buffer->data.count) {
        cur_pos += direction;
        cur_pos = skip_to_char(buffer, cur_pos, direction, '"');
        cur_pos = skip_to_char(buffer, cur_pos, direction, '\'');
        Brace cur_brace = find_opposite_brace(buffer->data.data[cur_pos]);
        if(cur_brace.brace == '0') continue;
        if((cur_brace.closing && direction == -1) || (!cur_brace.closing && direction == 1)) {
            brace_stack++;
        } else {
            if(brace_stack-- == 0 && cur_brace.brace == find_opposite_brace(initial_brace.brace).brace) {
                buffer->cursor = cur_pos;
                break;
            }
        }
    }
}

int isword(char ch) {
    if(isalnum(ch) || ch == '_') return 1;
    return 0;
}
    
void buffer_create_indent(Buffer *buffer, State *state) {
    if(state->config.indent > 0) {
        for(size_t i = 0; i < state->config.indent*state->num_of_braces; i++) {
            buffer_insert_char(state, buffer, ' ');
        }
    } else {
        for(size_t i = 0; i < state->num_of_braces; i++) {
            buffer_insert_char(state, buffer, '\t');
        }    
    }
}

void buffer_newline_indent(Buffer *buffer, State *state) {
    buffer_insert_char(state, buffer, '\n');
    buffer_create_indent(buffer, state);
}

int handle_motion_keys(Buffer *buffer, State *state, int ch, size_t *repeating_count) {
    (void)repeating_count;
    switch(ch) {
        case 'g': { // Move to the start of the file or to the line specified by repeating_count
            size_t row = buffer_get_row(buffer);            
            if(state->repeating.repeating_count >= buffer->rows.count) state->repeating.repeating_count = buffer->rows.count;
            if(state->repeating.repeating_count == 0) state->repeating.repeating_count = 1;                
            buffer->cursor = buffer->rows.data[state->repeating.repeating_count-1].start;
            state->repeating.repeating_count = 0;
            if(state->leader != LEADER_D) break;
            // TODO: this doens't work with row jumps
            size_t end = buffer->rows.data[row].end;
            size_t start = buffer->cursor;
            CREATE_UNDO(INSERT_CHARS, start);
            buffer_delete_selection(buffer, state, start, end);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case 'G': { // Move to the end of the file or to the line specified by repeating_count
            size_t row = buffer_get_row(buffer);
            size_t start = buffer->rows.data[row].start;
            if(state->repeating.repeating_count > 0) {
                if(state->repeating.repeating_count >= buffer->rows.count) state->repeating.repeating_count = buffer->rows.count;
                buffer->cursor = buffer->rows.data[state->repeating.repeating_count-1].start;
                state->repeating.repeating_count = 0;                
            } else {
                buffer->cursor = buffer->data.count;   
            }
            if(state->leader != LEADER_D) break;
            // TODO: this doesn't work with row jumps
            size_t end = buffer->cursor;
            CREATE_UNDO(INSERT_CHARS, start);
            buffer_delete_selection(buffer, state, start, end);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case '0': { // Move to the start of the line
            size_t row = buffer_get_row(buffer);
            size_t end = buffer->cursor;
            buffer->cursor = buffer->rows.data[row].start;
            if(state->leader != LEADER_D) break;
            size_t start = buffer->cursor;
            CREATE_UNDO(INSERT_CHARS, start);
            buffer_delete_selection(buffer, state, start, end);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case '$': { // Move to the end of the line
            size_t row = buffer_get_row(buffer);
            size_t start = buffer->cursor;
            buffer->cursor = buffer->rows.data[row].end;
            if(state->leader != LEADER_D) break;
            size_t end = buffer->cursor;
            CREATE_UNDO(INSERT_CHARS, start);
            buffer_delete_selection(buffer, state, start, end-1);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case 'e': { // Move to the end of the next word
            size_t start = buffer->cursor;
            if(buffer->cursor+1 < buffer->data.count && !isword(buffer->data.data[buffer->cursor+1])) buffer->cursor++;
            while(buffer->cursor+1 < buffer->data.count && 
                (isword(buffer->data.data[buffer->cursor+1]) || isspace(buffer->data.data[buffer->cursor]))
            ) {
                buffer->cursor++;
            }
            if(state->leader != LEADER_D) break;
            size_t end = buffer->cursor;
            CREATE_UNDO(INSERT_CHARS, start);
            buffer_delete_selection(buffer, state, start, end);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case 'b': { // Move to the start of the previous word
            if(buffer->cursor == 0) break;
            size_t end = buffer->cursor;
            if(buffer->cursor-1 > 0 && !isword(buffer->data.data[buffer->cursor-1])) buffer->cursor--;
            while(buffer->cursor-1 > 0 && 
                (isword(buffer->data.data[buffer->cursor-1]) || isspace(buffer->data.data[buffer->cursor+1]))
            ) {
                buffer->cursor--;
            }
            if(buffer->cursor-1 == 0) buffer->cursor--;
            if(state->leader != LEADER_D) break;
            size_t start = buffer->cursor;
            CREATE_UNDO(INSERT_CHARS, start);
            buffer_delete_selection(buffer, state, start, end);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case 'w': { // Move to the start of the next word
            size_t start = buffer->cursor;
            while(buffer->cursor < buffer->data.count && 
                (isword(buffer->data.data[buffer->cursor]) || isspace(buffer->data.data[buffer->cursor+1]))
            ) {
                buffer->cursor++;
            }
            if(buffer->cursor < buffer->data.count) buffer->cursor++;
            if(state->leader != LEADER_D) break;
            size_t end = buffer->cursor-1;
            CREATE_UNDO(INSERT_CHARS, start);
            buffer_delete_selection(buffer, state, start, end-1);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case LEFT_ARROW:
        case 'h': // Move left
            buffer_move_left(buffer);
            break;
        case DOWN_ARROW:
        case 'j': // Move down
            buffer_move_down(buffer);
            break;
        case UP_ARROW:
        case 'k': // Move up
            buffer_move_up(buffer);
            break;
        case RIGHT_ARROW:
        case 'l': // Move right
            buffer_move_right(buffer);
            break;
        case '%': { // Move to the matching brace
            buffer_next_brace(buffer);
            break;
        }
        default: { // If the key is not a motion key, return 0
            return 0;
        } 
    }
    return 1; // If the key is a motion key, return 1
}
    
int handle_leader_keys(State *state) {
    switch(state->ch) {
        case 'd':
            state->leader = LEADER_D;
            break;
        case 'y':
            state->leader = LEADER_Y;
            break;
        default:
            return 0;
    }    
    return 1;
}

int handle_modifying_keys(Buffer *buffer, State *state) {
    switch(state->ch) {
        case 'x': {
            CREATE_UNDO(INSERT_CHARS, buffer->cursor);
            reset_command(state->clipboard.str, &state->clipboard.len);
            buffer_yank_char(buffer, state);
            buffer_delete_char(buffer, state);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case 'd': {
            switch(state->leader) {
                case LEADER_D: {
                    size_t repeat = state->repeating.repeating_count;
                    if(repeat == 0) repeat = 1;
                    if(repeat > buffer->rows.count - buffer_get_row(buffer)) repeat = buffer->rows.count - buffer_get_row(buffer);
                    for(size_t i = 0; i < repeat; i++) {
                        reset_command(state->clipboard.str, &state->clipboard.len);
                        buffer_yank_line(buffer, state, 0);
                        size_t row = buffer_get_row(buffer);
                        Row cur = buffer->rows.data[row];
                        size_t offset = buffer->cursor - cur.start;
                        CREATE_UNDO(INSERT_CHARS, cur.start);
                        if(row == 0) {
                            buffer_delete_selection(buffer, state, cur.start, cur.end);
                        } else {
                            state->cur_undo.start -= 1;
                            buffer_delete_selection(buffer, state, cur.start-1, cur.end-1);
                        }
                        undo_push(state, &state->undo_stack, state->cur_undo);
                        buffer_calculate_rows(buffer);
                        if(row >= buffer->rows.count) row = buffer->rows.count-1;
                        cur = buffer->rows.data[row];
                        size_t pos = cur.start + offset;
                        if(pos > cur.end) pos = cur.end;
                        buffer->cursor = pos;
                    }
                    state->repeating.repeating_count = 0;
                } break;
                default:
                    break;
            }
        } break;
        case 'r': {
            CREATE_UNDO(REPLACE_CHAR, buffer->cursor);
            DA_APPEND(&state->cur_undo.data, buffer->data.data[buffer->cursor]);
            state->ch = frontend_getch(state->main_win); 
            buffer->data.data[buffer->cursor] = state->ch;
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        default: {
            return 0;
        }
    }
    return 1;
}

int handle_normal_to_insert_keys(Buffer *buffer, State *state) {
    switch(state->ch) {
        case 'i': {
			state->config.mode = INSERT;		
			if(state->repeating.repeating_count) {
				state->ch = frontend_getch(state->main_win);
			}
        } break;
        case 'I': {
            size_t row = buffer_get_row(buffer);
            Row cur = buffer->rows.data[row];
            buffer->cursor = cur.start;            
            while(buffer->cursor < cur.end && isspace(buffer->data.data[buffer->cursor])) buffer->cursor++;
            state->config.mode = INSERT;
        } break;
        case 'a':
            if(buffer->cursor < buffer->data.count) buffer->cursor++;
            state->config.mode = INSERT;
            break;
        case 'A': {
            size_t row = buffer_get_row(buffer);
            size_t end = buffer->rows.data[row].end;
            buffer->cursor = end;
            state->config.mode = INSERT;
        } break;
        case 'o': {
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
            size_t row = buffer_get_row(buffer);
            size_t end = buffer->rows.data[row].end;
            buffer->cursor = end;
            buffer_newline_indent(buffer, state);
            state->config.mode = INSERT;
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case 'O': {
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
            size_t row = buffer_get_row(buffer);
            size_t start = buffer->rows.data[row].start;
            buffer->cursor = start;
            buffer_newline_indent(buffer, state);
            state->config.mode = INSERT;
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        default: {
            return 0;
        }
    }
    CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
    return 1;
}
    
int check_keymaps(Buffer *buffer, State *state) {
    (void)buffer;
    for(size_t i = 0; i < state->config.key_maps.count; i++) {
        if(state->ch == state->config.key_maps.data[i].a) {
            for(size_t j = 0; j < state->config.key_maps.data[i].b_s; j++) {
                state->ch = state->config.key_maps.data[i].b[j];
                state->key_func[state->config.mode](buffer, &buffer, state);   
            }
            return 1;
        }
    }
    return 0;
}

int compare_name(File const *leftp, File const *rightp)
{
    return strcoll(leftp->name, rightp->name);
}

void scan_files(Files *files, char *directory) {
    DIR *dp = opendir(directory);
    if(dp == NULL) {
        WRITE_LOG("Failed to open directory: %s\n", directory);
        CRASH("Failed to open directory");
    }

    struct dirent *dent;
    while((dent = readdir(dp)) != NULL) {
        // Do not ignore .. in order to navigate back to the last directory
        if(strcmp(dent->d_name, ".") == 0) continue;
        
        char *path = calloc(256, sizeof(char));
        strcpy(path, directory);
        strcat(path, "/");
        strcat(path, dent->d_name);
        
        char *name = calloc(256, sizeof(char));
        strcpy(name, dent->d_name);

        if(dent->d_type == DT_DIR) {
            strcat(name, "/");
            DA_APPEND(files, ((File){name, path, true}));
        } else if(dent->d_type == DT_REG) {
            DA_APPEND(files, ((File){name, path, false}));
        }
    }
    closedir(dp);
    qsort(files->data, files->count,
        sizeof *files->data, (__compar_fn_t)&compare_name);
}

void free_files(Files *files) {
    for(size_t i = 0; i < files->count; ++i) {
        free(files->data[i].name);
        free(files->data[i].path);
    }
    free(files);
}

void handle_normal_keys(Buffer *buffer, Buffer **modify_buffer, State *state) {
    (void)modify_buffer;
    
    if(check_keymaps(buffer, state)) return;
    if(state->leader == LEADER_NONE && handle_leader_keys(state)) return;   
    if(isdigit(state->ch) && !(state->ch == '0' && state->num.count == 0)) {
        DA_APPEND(&state->num, state->ch);
        return;
    } 
    
    if(!isdigit(state->ch) && state->num.count > 0) {
        ASSERT(state->num.data, "num is not initialized");
        state->repeating.repeating_count = atoi(state->num.data);
        if(state->repeating.repeating_count == 0) return;
        state->num.count = 0;
        for(size_t i = 0; i < state->repeating.repeating_count; i++) {
            state->key_func[state->config.mode](buffer, modify_buffer, state);
        }
        state->repeating.repeating_count = 0;
        memset(state->num.data, 0, state->num.capacity);
        return;
    }

    switch(state->ch) {
        case ':':
            state->x = 1;
            frontend_move_cursor(state->status_bar, state->x, 1);
            state->config.mode = COMMAND;
            break;
        case '/':
            if(state->is_exploring) break;
            reset_command(state->command, &state->command_s);        
            state->x = state->command_s+1;
            frontend_move_cursor(state->status_bar, state->x, 1);        
            state->config.mode = SEARCH;
            break;
        case 'v':
            if(state->is_exploring) break;
            buffer->visual.start = buffer->cursor;
            buffer->visual.end = buffer->cursor;
            buffer->visual.is_line = 0;
            state->config.mode = VISUAL;
            break;
        case 'V':
            if(state->is_exploring) break;
            buffer->visual.start = buffer->rows.data[buffer_get_row(buffer)].start;
            buffer->visual.end = buffer->rows.data[buffer_get_row(buffer)].end;
            buffer->visual.is_line = 1;
            state->config.mode = VISUAL;
            break;
        case ctrl('o'): {
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
            buffer_insert_char(state, buffer, '\n');
            buffer_create_indent(buffer, state);
            undo_push(state, &state->undo_stack, state->cur_undo);
        } break;
        case 'n': {
            size_t index = search(buffer, state->command, state->command_s);
            buffer->cursor = index;
        } break;
        case 'u': {
            Undo undo = undo_pop(&state->undo_stack); 
            Undo redo = {0};
            redo.start = undo.start;
            state->cur_undo = redo;
            switch(undo.type) {
                case NONE:
                    break;
                case INSERT_CHARS:
                    state->cur_undo.type = (undo.data.count > 1) ? DELETE_MULT_CHAR : DELETE_CHAR;
                    state->cur_undo.end = undo.start + undo.data.count;
                    buffer->cursor = undo.start;
                    for(size_t i = 0; i < undo.data.count; i++) {
                        buffer_insert_char(state, buffer, undo.data.data[i]);
                    } 
                    break;
                case DELETE_CHAR:
                    state->cur_undo.type = INSERT_CHARS;
                    buffer->cursor = undo.start;
                    buffer_delete_char(buffer, state);
                    break;
                case DELETE_MULT_CHAR:
                    state->cur_undo.type = INSERT_CHARS;
                    state->cur_undo.end = undo.end;
                    buffer->cursor = undo.start;
                    for(size_t i = undo.start; i < undo.end; i++) {
                        buffer_delete_char(buffer, state);
                    }
                    break;
                case REPLACE_CHAR:
                    state->cur_undo.type = REPLACE_CHAR;
                    buffer->cursor = undo.start;
                    DA_APPEND(&undo.data, buffer->data.data[buffer->cursor]);
                    buffer->data.data[buffer->cursor] = undo.data.data[0]; 
                    break;
            }
            undo_push(state, &state->redo_stack, state->cur_undo);
            free_undo(&undo);
        } break;
        case 'U': {
            Undo redo = undo_pop(&state->redo_stack); 
            Undo undo = {0};
            undo.start = redo.start;
            state->cur_undo = undo;
            switch(redo.type) {
                case NONE:
                    return;
                    break;
                case INSERT_CHARS:
                    state->cur_undo.type = (redo.data.count > 1) ? DELETE_MULT_CHAR : DELETE_CHAR;
                    state->cur_undo.end = redo.start + redo.data.count;
                    buffer->cursor = redo.start;
                    for(size_t i = 0; i < redo.data.count; i++) {
                        buffer_insert_char(state, buffer, redo.data.data[i]);
                    } 
                    break;
                case DELETE_CHAR:
                    state->cur_undo.type = INSERT_CHARS;
                    buffer->cursor = redo.start;
                    buffer_delete_char(buffer, state);
                    break;
                case DELETE_MULT_CHAR:
                    state->cur_undo.type = INSERT_CHARS;
                    state->cur_undo.end = redo.end;
                    buffer->cursor = undo.start;
                    for(size_t i = redo.start; i < redo.end; i++) {
                        buffer_delete_char(buffer, state);
                    }
                    break;
                case REPLACE_CHAR:
                    state->cur_undo.type = REPLACE_CHAR;
                    buffer->cursor = redo.start;
                    DA_APPEND(&redo.data, buffer->data.data[buffer->cursor]);
                    buffer->data.data[buffer->cursor] = redo.data.data[0]; 
                    break;
            }
            undo_push(state, &state->undo_stack, state->cur_undo);
            free_undo(&redo);
        } break;
        case ctrl('s'): {
            handle_save(buffer);
            state->config.QUIT = 1;
        } break;
        case ctrl('c'):
        case ESCAPE:
            state->repeating.repeating_count = 0;
            reset_command(state->command, &state->command_s);
            state->config.mode = NORMAL;
            break;
        case KEY_RESIZE: {
            frontend_resize_window(state);
        } break;
        case 'y': {
            switch(state->leader) {
                case LEADER_Y: {
                    if(state->repeating.repeating_count == 0) state->repeating.repeating_count = 1;
                    reset_command(state->clipboard.str, &state->clipboard.len);
                    for(size_t i = 0; i < state->repeating.repeating_count; i++) {
                        buffer_yank_line(buffer, state, i);
                    }
                    state->repeating.repeating_count = 0;
                } break;
                default:
                    break;
            }
        } break;
        case 'p': {
            if(state->clipboard.len == 0) break;
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
            for(size_t i = 0; i < state->clipboard.len-1; i++) {
                buffer_insert_char(state, buffer, state->clipboard.str[i]);
            }
            state->cur_undo.end = buffer->cursor;
            undo_push(state, &state->undo_stack, state->cur_undo); 
        } break;
        case ctrl('n'): {
            state->is_exploring = !state->is_exploring;
        } break;
        default: {
            if(state->is_exploring) {
                switch(state->ch) {
                    case DOWN_ARROW:
                    case 'j': // Move down
                        if (state->explore_cursor < state->files->count-1) {
                            state->explore_cursor++;
                        }
                        break;
                    case UP_ARROW:
                    case 'k': // Move up
                        if (state->explore_cursor > 0) {
                            state->explore_cursor--;
                        }
                        break;
                    case ENTER: {
                        File f = state->files->data[state->explore_cursor];
                        if (f.is_directory) {
                            char str[256];
                            strcpy(str, f.path);

                            free_files(state->files);
                            state->files = calloc(32, sizeof(File));
                            scan_files(state->files, str);
                            state->explore_cursor = 0;
                        } else {
                            // TODO: Load syntax highlighting right here
                            state->buffer = load_buffer_from_file(f.path);
							char *config_filename = NULL;
							char *syntax_filename = NULL;							
						    load_config_from_file(state, state->buffer, config_filename, syntax_filename);			
                            state->is_exploring = false;
                        }
                    } break;
                }
                break;
            }

            if(handle_modifying_keys(buffer, state)) break;
            if(handle_motion_keys(buffer, state, state->ch, &state->repeating.repeating_count)) break;
            if(handle_normal_to_insert_keys(buffer, state)) break;
        } break;
    }
    if(state->repeating.repeating_count == 0) {
        state->leader = LEADER_NONE;
    }
}

void handle_insert_keys(Buffer *buffer, Buffer **modify_buffer, State *state) {
    (void)modify_buffer;
	ASSERT(buffer, "buffer exists");
	ASSERT(state, "state exists");
	
    switch(state->ch) { 
        case '\b':
        case 127:
        case KEY_BACKSPACE: { 
            if(buffer->cursor > 0) {
                buffer->cursor--;
                buffer_delete_char(buffer, state);
            }
        } break;
        case ctrl('s'): {
            handle_save(buffer);
            state->config.QUIT = 1;
        } break;
        case ctrl('c'):        
        case ESCAPE: // Switch to NORMAL mode
            //state->cur_undo.end = buffer->cursor;
            if(state->cur_undo.end != state->cur_undo.start) undo_push(state, &state->undo_stack, state->cur_undo);
            state->config.mode = NORMAL;
            break;
        case LEFT_ARROW: { // Move cursor left
            state->cur_undo.end = buffer->cursor;
            if(state->cur_undo.end != state->cur_undo.start) undo_push(state, &state->undo_stack, state->cur_undo);
            buffer_move_left(buffer);
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
        } break;
        case DOWN_ARROW: { // Move cursor down
            state->cur_undo.end = buffer->cursor;
            if(state->cur_undo.end != state->cur_undo.start) undo_push(state, &state->undo_stack, state->cur_undo);
            buffer_move_down(buffer);
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
        } break;
        case UP_ARROW: {   // Move cursor up
            state->cur_undo.end = buffer->cursor;
            if(state->cur_undo.end != state->cur_undo.start) undo_push(state, &state->undo_stack, state->cur_undo);
            buffer_move_up(buffer);
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
        } break;
        case RIGHT_ARROW: { // Move cursor right
            state->cur_undo.end = buffer->cursor;
            if(state->cur_undo.end != state->cur_undo.start) undo_push(state, &state->undo_stack, state->cur_undo);
            buffer_move_right(buffer);
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
        } break;
        case KEY_RESIZE: {
            frontend_resize_window(state);
        } break;
        case KEY_TAB:
            if(state->config.indent > 0) {
                for(size_t i = 0; (int)i < state->config.indent; i++) {
                    buffer_insert_char(state, buffer, ' ');
                }
            } else {
                buffer_insert_char(state, buffer, '\t');
            }
            break;
        case KEY_ENTER:
        case ENTER: {
            if(state->cur_undo.end != state->cur_undo.start) undo_push(state, &state->undo_stack, state->cur_undo);
            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);
            Brace brace = find_opposite_brace(buffer->data.data[buffer->cursor]);
            buffer_newline_indent(buffer, state);
            if(brace.brace != '0' && brace.closing) {
                buffer_insert_char(state, buffer, '\n');
                if(state->num_of_braces == 0) state->num_of_braces = 1;
                if(state->config.indent > 0) {
                    for(size_t i = 0; i < state->config.indent*(state->num_of_braces-1); i++) {
                        buffer_insert_char(state, buffer, ' ');
                        buffer->cursor--;
                    }
                } else {
                    for(size_t i = 0; i < state->num_of_braces-1; i++) {
                        buffer_insert_char(state, buffer, '\t');                            
                        buffer->cursor--;
                    }        
                }
                buffer->cursor--;
            }
        } break;
        default: { // Handle other characters
			ASSERT(buffer->data.count >= buffer->cursor, "check");
			ASSERT(buffer->data.data, "check2");
			Brace brace = (Brace){0};
			Brace cur_brace;
			if(buffer->cursor == 0) cur_brace = find_opposite_brace(buffer->data.data[0]);			
			else cur_brace = find_opposite_brace(buffer->data.data[buffer->cursor]);
			
            if((cur_brace.brace != '0' && cur_brace.closing && 
                state->ch == find_opposite_brace(cur_brace.brace).brace)) {
                buffer->cursor++;
                break;
            };
            brace = find_opposite_brace(state->ch);
            // TODO: make quotes auto close
            buffer_insert_char(state, buffer, state->ch);
            if(brace.brace != '0' && !brace.closing) {
                buffer_insert_char(state, buffer, brace.brace);
	            undo_push(state, &state->undo_stack, state->cur_undo);
                buffer->cursor--;
	            CREATE_UNDO(DELETE_MULT_CHAR, buffer->cursor);				
            }
        } break;
    }
}

void handle_command_keys(Buffer *buffer, Buffer **modify_buffer, State *state) {
    (void)modify_buffer;
    switch(state->ch) {
        case '\b':
        case 127:
        case KEY_BACKSPACE: {
            if(state->x != 1) {
                shift_str_left(state->command, &state->command_s, --state->x);
                frontend_move_cursor(state->status_bar, 1, state->x);
            }
        } break;
        case ctrl('c'):        
        case ESCAPE:
            reset_command(state->command, &state->command_s);
            state->config.mode = NORMAL;
            break;
        case ctrl('s'): {
            handle_save(buffer);
            state->config.QUIT = 1;
        } break;
        case KEY_ENTER:
        case ENTER: {
            if(state->command[0] == '!') {
                shift_str_left(state->command, &state->command_s, 0);
                FILE *file = popen(state->command, "r");
                if(file == NULL) {
                    CRASH("could not run command");
                }
                while(fgets(state->status_bar_msg, sizeof(state->status_bar_msg), file) != NULL) {
                    state->is_print_msg = 1;
                }
                pclose(file);
            } else {
                size_t command_s = 0;
                Command_Token *command = lex_command(state, view_create(state->command, state->command_s), &command_s);
                execute_command(buffer, state, command, command_s);
            }
            reset_command(state->command, &state->command_s);
            state->config.mode = NORMAL;
        } break;
        case LEFT_ARROW:
            if(state->x > 1) state->x--;
            break;
        case DOWN_ARROW:
            break;
        case UP_ARROW:
            break;
        case RIGHT_ARROW:
            if(state->x < state->command_s) state->x++;
            break;
        case KEY_RESIZE:
            frontend_resize_window(state);
            break;
        default: {
            shift_str_right(state->command, &state->command_s, state->x);
            state->command[(state->x++)-1] = state->ch;
        } break;
    }
}

void handle_search_keys(Buffer *buffer, Buffer **modify_buffer, State *state) {
    (void)modify_buffer;
    (void)buffer;
    switch(state->ch) {
        case '\b':
        case 127:
        case KEY_BACKSPACE: {
            if(state->x != 1) {
                shift_str_left(state->command, &state->command_s, --state->x);
                frontend_move_cursor(state->status_bar, 1, state->x);
            }
        } break;
        case ctrl('c'):        
        case ESCAPE:
            reset_command(state->command, &state->command_s);
            state->config.mode = NORMAL;
            break;
        case ENTER: {
            size_t index = search(buffer, state->command, state->command_s);
            if(state->command_s > 2 && strncmp(state->command, "s/", 2) == 0) {
                char str[128]; // replace with the maximum length of your command
                strncpy(str, state->command+2, state->command_s-2);
                str[state->command_s-2] = '\0'; // ensure null termination

                char *token = strtok(str, "/");
                int count = 0;
                char args[2][100];

                while (token != NULL) {
                    char temp_buffer[100];
                    strcpy(temp_buffer, token);
                    if(count == 0) {
                        strcpy(args[0], temp_buffer);
                    } else if(count == 1) {
                        strcpy(args[1], temp_buffer);
                    }
                    count++;

                    // log for args.
                    token = strtok(NULL, "/");
                }
                index = search(buffer, args[0], strlen(args[0]));
                find_and_replace(buffer, state, args[0], args[1]);
            } 
            buffer->cursor = index;
            state->config.mode = NORMAL;
        } break;
        case ctrl('s'): {
            handle_save(buffer);
            state->config.QUIT = 1;
        } break;
        case LEFT_ARROW:
            if(state->x > 1) state->x--;
            break;
        case DOWN_ARROW:
            break;
        case UP_ARROW:
            break;
        case RIGHT_ARROW:
            if(state->x < state->command_s) state->x++;
            break;
        case KEY_RESIZE:
            frontend_resize_window(state);
            break;
        default: {
            shift_str_right(state->command, &state->command_s, state->x);
            state->command[(state->x++)-1] = state->ch;
        } break;
    }
}

void handle_visual_keys(Buffer *buffer, Buffer **modify_buffer, State *state) {
    (void)modify_buffer;
    frontend_cursor_visible(0);
    switch(state->ch) {
        case ctrl('c'):        
        case ESCAPE: {
            state->config.mode = NORMAL;
            frontend_cursor_visible(1);        
            state->buffer->visual = (Visual){0};        
        } break;
        case ENTER: break;
        case ctrl('s'): {
            handle_save(buffer);
            state->config.QUIT = 1;
        } break;
        case 'd':
        case 'x': {
            int cond = (buffer->visual.start > buffer->visual.end);
            size_t start = (cond) ? buffer->visual.end : buffer->visual.start;
            size_t end = (cond) ? buffer->visual.start : buffer->visual.end;
            CREATE_UNDO(INSERT_CHARS, start);
            buffer_delete_selection(buffer, state, start, end);
            undo_push(state, &state->undo_stack, state->cur_undo);
            state->config.mode = NORMAL;
            frontend_cursor_visible(1);
        } break;
        case '>': {
            int cond = (buffer->visual.start > buffer->visual.end);
            size_t start = (cond) ? buffer->visual.end : buffer->visual.start;
            size_t end = (cond) ? buffer->visual.start : buffer->visual.end;
            size_t position = buffer->cursor + state->config.indent;
            size_t row = index_get_row(buffer, start);
            size_t end_row = index_get_row(buffer, end);
            for(size_t i = row; i <= end_row; i++) {
                buffer_calculate_rows(buffer);
                buffer->cursor = buffer->rows.data[i].start;
                if(state->config.indent > 0) {
                    for(size_t i = 0; (int)i < state->config.indent; i++) {
                        buffer_insert_char(state, buffer, ' ');
                    }
                } else {
                     buffer_insert_char(state, buffer, '\t');                           
                }
            }
            buffer->cursor = position;
            state->config.mode = NORMAL;
            frontend_cursor_visible(1);                    
        } break;
        case '<': {
            int cond = (buffer->visual.start > buffer->visual.end);
            size_t start = (cond) ? buffer->visual.end : buffer->visual.start;
            size_t end = (cond) ? buffer->visual.start : buffer->visual.end;
            size_t row = index_get_row(buffer, start);
            size_t end_row = index_get_row(buffer, end);            
            size_t offset = 0;
            for(size_t i = row; i <= end_row; i++) {
                buffer_calculate_rows(buffer);                
                buffer->cursor = buffer->rows.data[i].start;
                if(state->config.indent > 0) {
                    for(size_t j = 0; (int)j < state->config.indent; j++) {
                        if(isspace(buffer->data.data[buffer->cursor])) {
                            buffer_delete_char(buffer, state);
                            offset++;
                            buffer_calculate_rows(buffer);
                        }
                    }
                } else {
                    if(isspace(buffer->data.data[buffer->cursor])) {
                        buffer_delete_char(buffer, state);
                        offset++;
                        buffer_calculate_rows(buffer);
                    }
                }
            }
            state->config.mode = NORMAL;
            frontend_cursor_visible(1);                                
        } break;
        case 'y': {
            reset_command(state->clipboard.str, &state->clipboard.len);
            int cond = (buffer->visual.start > buffer->visual.end);
            size_t start = (cond) ? buffer->visual.end : buffer->visual.start;
            size_t end = (cond) ? buffer->visual.start : buffer->visual.end;
            buffer_yank_selection(buffer, state, start, end);
            buffer->cursor = start;
            state->config.mode = NORMAL;
            frontend_cursor_visible(1);                                            
            break;
        }
        default: {
            handle_motion_keys(buffer, state, state->ch, &state->repeating.repeating_count);
            if(buffer->visual.is_line) {
                buffer->visual.end = buffer->rows.data[buffer_get_row(buffer)].end;
                if(buffer->visual.start >= buffer->visual.end) {
                    buffer->visual.end = buffer->rows.data[buffer_get_row(buffer)].start;
                    buffer->visual.start = buffer->rows.data[index_get_row(buffer, buffer->visual.start)].end;
                }
            } else {
                buffer->visual.end = buffer->cursor;
            }
        } break;
    }
}
    
State init_state() {
    State state = {0};
    state.config = (Config){0};
    state.config.relative_nums = 1;
    state.config.auto_indent = 1;
    state.config.syntax = 1;
    state.config.indent = 4;
    state.config.undo_size = 16;
    state.config.lang = " ";
    // Control variables
    state.config.QUIT = 0;
    state.config.mode = NORMAL;
    // Colors
    state.config.background_color = -1; // -1 for terminal background color.
    state.config.leaders[0] = ' ';
    state.config.leaders[1] = 'r';
    state.config.leaders[2] = 'd';
    state.config.leaders[3] = 'y';
    state.config.key_maps = (Maps){0};
    state.config.vars[0] = (Config_Vars){{"syntax", sizeof("syntax")-1}, &state.config.syntax};
    state.config.vars[1] = (Config_Vars){{"indent", sizeof("indent")-1}, &state.config.indent};
    state.config.vars[2] = (Config_Vars){{"auto-indent", sizeof("auto-indent")-1}, &state.config.auto_indent};
    state.config.vars[3] = (Config_Vars){{"undo-size", sizeof("undo-size")-1}, &state.config.undo_size};
    state.config.vars[4] = (Config_Vars){{"relative", sizeof("relative")-1}, &state.config.relative_nums};
    return state;
}

void load_config_from_file(State *state, Buffer *buffer, char *config_filename, char *syntax_filename) {
    if(config_filename == NULL) {
        char default_config_filename[128] = {0};
        char config_dir[64] = {0};
		if(!state->env) {
			state->env = calloc(128, sizeof(char));
	        char *env = getenv("HOME");			
	        if(env == NULL) CRASH("could not get HOME");			
			state->env = env;
		}
        sprintf(config_dir, "%s/.config/cano", state->env);
        struct stat st = {0};
        if(stat(config_dir, &st) == -1) {
            mkdir(config_dir, 0755);
        }
        sprintf(default_config_filename, "%s/config.cano", config_dir);
        config_filename = default_config_filename;

        char *language = strip_off_dot(buffer->filename, strlen(buffer->filename));
        if(language != NULL) {
            syntax_filename = calloc(strlen(config_dir)+strlen(language)+sizeof(".cyntax")+1, sizeof(char));
            sprintf(syntax_filename, "%s/%s.cyntax", config_dir, language);
            free(language);
        }
    }
    char **lines = calloc(2, sizeof(char*));
    size_t lines_s = 0;
    int err = read_file_by_lines(config_filename, &lines, &lines_s);
    if(err == 0) {
        for(size_t i = 0; i < lines_s; i++) {
            size_t cmd_s = 0;
            Command_Token *cmd = lex_command(state, view_create(lines[i], strlen(lines[i])), &cmd_s);
            execute_command(buffer, state, cmd, cmd_s);
            free(lines[i]);
        }
    }
    free(lines);

    if(syntax_filename != NULL) {
        Color_Arr color_arr = parse_syntax_file(syntax_filename);
        if(color_arr.arr != NULL) {
            for(size_t i = 0; i < color_arr.arr_s; i++) {
                init_pair(color_arr.arr[i].custom_slot, color_arr.arr[i].custom_id, state->config.background_color);
                init_ncurses_color(color_arr.arr[i].custom_id, color_arr.arr[i].custom_r, 
                                   color_arr.arr[i].custom_g, color_arr.arr[i].custom_b);
            }

            free(color_arr.arr);
        }
    }
}

char *get_help_page(char *page) {
    if (page == NULL) return NULL;

    char *env = getenv("HOME");
    if(env == NULL) CRASH("could not get HOME");

    char *help_page = calloc(128, sizeof(char));
    if (help_page == NULL) CRASH("could not calloc memory for help page");
    snprintf(help_page, 128, "%s/.local/share/cano/help/%s", env, page);

    // check if file exists
    struct stat st;
    if (stat(help_page, &st) != 0) return NULL;

    return help_page;
}
    
void handle_flags(char *program, char **argv, int argc, char **config_filename, char **help_filename) {
    char *flag = NULL;
    
    struct option longopts[] = {
        {"help", no_argument, NULL, 'h'},
        {"config", optional_argument, NULL, 'c'},
    };
        
    char opt = cgetopt_long(argc, argv, "", longopts, NULL);
    
    while(true) {
        if(opt == -1) break;
        switch(opt) {
            case 'c':
                flag = optarg;
                if(flag == NULL) {
                    fprintf(stderr, "usage: %s --config <config.cano> <filename>\n", program);
                    exit(EXIT_FAILURE);
                }
                *config_filename = flag;
                break;           
            case 'h':
                flag = optarg;
                if (flag == NULL) {
                    *help_filename = get_help_page("general");
                } else {
                    *help_filename = get_help_page(flag);
                }
    
                if (*help_filename == NULL) {
                    fprintf(stderr, "Failed to open help page. Check for typos or if you installed cano properly.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                fprintf(stderr, "Unexpected flag");
                exit(EXIT_FAILURE);
       }
       opt = cgetopt_long(argc, argv, "", longopts, NULL);            
    }
}

/* ------------------------- FUNCTIONS END ------------------------- */

int main(int argc, char **argv) {
    WRITE_LOG("starting (int main)");
    setlocale(LC_ALL, "");
    char *program = argv[0];        
    char *config_filename = NULL, *syntax_filename = NULL, *help_filename = NULL;
    handle_flags(program, argv, argc, &config_filename, &help_filename);
    
    char *filename = argv[optind];

    // define functions based on current mode
    void(*key_func[MODE_COUNT])(Buffer *buffer, Buffer **modify_buffer, struct State *state) = {
        handle_normal_keys, handle_insert_keys, handle_search_keys, handle_command_keys, handle_visual_keys
    };

    State state = init_state();
    state.command = calloc(64, sizeof(char));
    state.key_func = key_func;
    state.files = calloc(32, sizeof(File));
    state.config.lang = "UNUSED";
    scan_files(state.files, ".");

    frontend_init(&state);

    if(filename == NULL) filename = "out.txt";

    if (help_filename != NULL) {
        state.buffer = load_buffer_from_file(help_filename);
        free(help_filename);
    } else {
        state.buffer = load_buffer_from_file(filename);
    }
    
    load_config_from_file(&state, state.buffer, config_filename, syntax_filename);
    
    char status_bar_msg[128] = {0};
    state.status_bar_msg = status_bar_msg;
    buffer_calculate_rows(state.buffer);

    while(state.ch != ctrl('q') && state.config.QUIT != 1) {
        state_render(&state);
        state.ch = frontend_getch(state.main_win);
        state.key_func[state.config.mode](state.buffer, &state.buffer, &state);
    }
    
    frontend_end();

    free_buffer(state.buffer);
    free_undo_stack(&state.undo_stack);
    free_undo_stack(&state.redo_stack);
    free_files(state.files);

    return 0;
}
