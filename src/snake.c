#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// Definitions (yes these #define's are needed)
#define FPS 15

// Sprite strings.
struct Sprite {
  const char *bg_color;
  const char *fg_color;
  const char character;
};

// My snake is indeed a linked list.
// TODO:
// Make this a hashmap.
struct Snake {
  int pos_x;
  int pos_y;
  struct Snake *s;
};

// Food struct.
struct Food {
  int pos_x;
  int pos_y;
};

// Direction enum.
enum direction { UP = 0, DOWN = 1, RIGHT = 2, LEFT = 3 };

// Helper functions
static inline void winclear(void);
static inline void movecursor(int x, int y);
static inline void updatewinsize(void);
static inline void setcolor(const struct Sprite sprite);

// Big boy function.
int check_collisions(struct Snake *s, struct Food *f, int size);

// Functions for the food.
void f_print(struct Food *f, int size);
void f_init(struct Food *f, int size);

// Functions for the snake.
void s_move(struct Snake *s);
void s_add(struct Snake *s);
void s_delete(struct Snake *s);
void s_print(struct Snake *s);

// Used to get terminal size.
struct winsize win;
int snake_dir = RIGHT;

// Sprite definitions.
const struct Sprite snake_sprite = {
    .bg_color = "\x1b[48;5;10m",
    .fg_color = "\x1b[38;5;2m",
    .character = '~',
};

const struct Sprite snake_head_sprite = {
    .bg_color = "\x1b[48;5;2m", .fg_color = "\x1b[38;5;9m", .character = '*'};

const struct Sprite food_sprite = {
    .bg_color = "\x1b[48;5;9m",
    .fg_color = "\x1b[38;5;94m",
    .character = '\'',
};

// String to reset terminal colors.
const char *reset_color = "\x1b[0m";

/*
 * MAIN FUNCTION
 */
int main(void) {
  srand((unsigned int)time(NULL)); // Seed the random number generator.
  char out_buffer[BUFSIZ];         // Stack-based buffer
  setvbuf(stdout, out_buffer, _IOFBF,
          BUFSIZ); // Set full buffering and ensure stack-based buffer.
  char input[5];   // Input buffer.

  // Init timespecs.
  struct timespec start, end, sleep;
  int sleepfor = (int)1e9 / FPS;

  // Get initial terminal size.
  updatewinsize();

  // Initialize the food struct.
  struct Food food[(win.ws_row * win.ws_col) / 1000];
  int food_size = (int)sizeof(food) / (int)sizeof(struct Food);
  f_init(food, food_size);

  // Initialize the player struct.
  struct Snake *player = malloc(sizeof(struct Snake));
  (*player) = (struct Snake){0, 0, NULL};
  s_add(player);

  // Disable echo and canonical mode.
  struct termios default_t, new_t;
  tcgetattr(STDIN_FILENO, &default_t);
  new_t = default_t;
  new_t.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

  // Set the standard input to non-blocking mode.
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  // Main game loop.
  while (1) {
    // Start clock (so you can have a smooth frame speed).
    clock_gettime(CLOCK_MONOTONIC, &start);

    winclear();
    updatewinsize();

    /* Get the input.
     * The `read` function only reads `sizeof(input) - 1` bytes from the input
     * so the read value always has a null terminator (due to the memset call
     * before the read call).
     */
    memset(&input, '\0', sizeof(input));          // 'clear' the input buffer.
    read(STDIN_FILENO, input, sizeof(input) - 1); // Get input (POSIX).

    if (input[0] == '\x1b') {
      switch (input[2]) {
      case 'A':
        snake_dir = UP;
        break;
      case 'B':
        snake_dir = DOWN;
        break;
      case 'C':
        snake_dir = RIGHT;
        break;
      case 'D':
        snake_dir = LEFT;
        break;
      }
      if (strcmp(input, "\x1b") == 0)
        goto exit;

    } else {
      switch (input[0]) {
      case 'w':
      case 'k':
        snake_dir = UP;
        break;
      case 's':
      case 'j':
        snake_dir = DOWN;
        break;
      case 'd':
      case 'l':
        snake_dir = RIGHT;
        break;
      case 'a':
      case 'h':
        snake_dir = LEFT;
        break;
      case 'q':
        goto exit;
        break;
      }
    }

    s_move(player);

    if (check_collisions(player, food, food_size) == 1)
      goto exit;

    f_print(food, food_size);
    s_print(player);

    movecursor(win.ws_col, win.ws_row);
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &end);
    int remaining_time =
        sleepfor - (int)((end.tv_sec - start.tv_sec) * (long)1e9 +
                         (end.tv_nsec - start.tv_nsec));
    if (remaining_time > 0) {
      sleep.tv_sec = remaining_time / (long)1e9;  // Convert to seconds
      sleep.tv_nsec = remaining_time % (long)1e9; // Remaining nanoseconds
      nanosleep(&sleep, NULL);
    }
  }

exit:
  s_delete(player);
  tcsetattr(STDIN_FILENO, TCSANOW, &default_t);
  return EXIT_SUCCESS;
}

/*
 *
 * FUNCTION DEFINITIONS
 *
 */

/*
 * Clears the whole terminal window.
 */
static inline void winclear(void) {
  fprintf(stdout, "\x1b[2J");
  return;
}

/*
 * Moves the cursor to the 'x', 'y' coordinates.
 */
static inline void movecursor(int x, int y) {
  fprintf(stdout, "\x1b[%d;%dH", y, x);
  return;
}

/*
 * Updates the window size.
 */
static inline void updatewinsize(void) {
  ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
  return;
}

/*
 * Sets the background and foreground color of the given 'Sprite' struct.
 */
static inline void setcolor(const struct Sprite sprite) {
  fprintf(stdout, "%s%s", sprite.fg_color, sprite.bg_color);
  return;
}

/*
 * WTF even is this long ass function?
 * Just don't read this.
 * This is supposed to check collisions, idk how though.
 */
int check_collisions(struct Snake *s, struct Food *f, int size) {
  bool **occupied = calloc(win.ws_row, sizeof(bool *));
  for (int i = 0; i < win.ws_row; i++) {
    occupied[i] = calloc(win.ws_col, sizeof(bool));
  }

  // Get occupied cells of the snake.
  struct Snake *curr = s->s;
  while (curr != NULL) {
    if (curr->pos_y > 0 && curr->pos_y < win.ws_row && curr->pos_x > 0 &&
        curr->pos_x < win.ws_col) {
      occupied[curr->pos_y][curr->pos_x] = true;
    }
    curr = curr->s;
  }

  // Get occupied cells by the food.
  for (int i = 0; i < size; i++) {
    if (f[i].pos_x > 0 && f[i].pos_x < win.ws_col && f[i].pos_y > 0 &&
        f[i].pos_y < win.ws_row) {
      occupied[f[i].pos_y][f[i].pos_x] = true;
    } else {
      // Get out of bound food within boundaries
      f[i].pos_x = rand() % win.ws_col;
      f[i].pos_y = rand() % win.ws_row;
      occupied[f[i].pos_y][f[i].pos_x] = true;
    }
  }

  // Check if there was collision at all.
  if (s->pos_x > 0 && s->pos_x < win.ws_col && s->pos_y > 0 &&
      s->pos_y < win.ws_row && occupied[s->pos_y][s->pos_x]) {
    // Check food collision.
    for (int i = 0; i < size; i++) {
      if (s->pos_x == f[i].pos_x && s->pos_y == f[i].pos_y) {
        f[i].pos_x = rand() % win.ws_col;
        f[i].pos_y = rand() % win.ws_row;
        s_add(s);
        for (int i = 0; i < win.ws_row; i++) {
          free(occupied[i]);
        }
        free(occupied);
        return 0;
      }
    }

    // Check snake-to-snake collision.
    curr = s->s;
    while (curr != NULL) {
      if (s->pos_x == curr->pos_x && s->pos_y == curr->pos_y) {
        movecursor(0, 0);
        write(STDOUT_FILENO, " ! Death ! ", 11);
        for (int i = 0; i < win.ws_row; i++) {
          free(occupied[i]);
        }
        free(occupied);
        return 1;
      }
      curr = curr->s;
    }
  }

  // Boundry check.
  curr = s;
  while (curr != NULL) {
    if (curr->pos_x < 0) {
      curr->pos_x = win.ws_col;
    } else if (curr->pos_x > win.ws_col) {
      curr->pos_x = 0;
    }
    if (curr->pos_y < 0) {
      curr->pos_y = win.ws_row;
    } else if (curr->pos_y > win.ws_row) {
      curr->pos_y = 0;
    }
    curr = curr->s;
  }

  for (int i = 0; i < win.ws_row; i++) {
    free(occupied[i]);
  }
  free(occupied);
  return 0;
}

/*
 * Outputs the given 'Food' struct and 'food_sprite' 'Sprite' struct to
 * 'STDOUT_FILENO'.
 */
void f_print(struct Food *f, int size) {
  setcolor(food_sprite);
  for (int i = 0; i < size; i++) {
    fprintf(stdout, "\x1b[%d;%dH%c", f[i].pos_y, f[i].pos_x,
            food_sprite.character);
  }
  fprintf(stdout, "%s", reset_color);
  return;
}

/*
 * Generates random positions for each 'Food' struct in the 'Food' struct
 * array.
 */
void f_init(struct Food *f, int size) {
  for (int i = 0; i < size; i++) {
    f[i].pos_x = rand() % win.ws_col;
    f[i].pos_y = rand() % win.ws_row;
  }
  return;
}

/*
 * Moves the given 'Snake' struct in the 'snake_dir' direction.
 */
void s_move(struct Snake *s) {
  struct Snake *curr = s;
  int x = curr->pos_x;
  int y = curr->pos_y;

  switch (snake_dir) {
  case 0:
    y--;
    break;
  case 1:
    y++;
    break;
  case 2:
    x++;
    break;
  case 3:
    x--;
    break;
  }

  // https://betterexplained.com/articles/swap-two-variables-using-xor/
  while (curr != NULL) {
    curr->pos_x ^= x;
    x ^= curr->pos_x;
    curr->pos_x ^= x;

    curr->pos_y ^= y;
    y ^= curr->pos_y;
    curr->pos_y ^= y;

    curr = curr->s;
  }
  return;
}

/*
 * Adds a 'Snake' struct to the given 'Snake' struct's 's' field.
 */
void s_add(struct Snake *s) {
  struct Snake *curr = s;
  while (curr->s != NULL) {
    curr = curr->s;
  }

  int old_x = curr->pos_x;
  int old_y = curr->pos_y;

  s_move(s);

  curr->s = malloc(sizeof(struct Snake));
  *curr->s = (struct Snake){old_x, old_y, NULL};

  return;
}

/*
 * Deletes the given 'Snake' struct.
 */
void s_delete(struct Snake *s) {
  struct Snake *curr = s;
  struct Snake *nxt;

  while (curr != NULL) {
    nxt = curr->s;
    free(curr);
    curr = nxt;
  }
  return;
}

/*
 * Outputs the given 'Snake' struct to 'stdout'.
 */
void s_print(struct Snake *s) {
  struct Snake *curr = s;

  setcolor(snake_head_sprite);
  movecursor(curr->pos_x, curr->pos_y);
  fprintf(stdout, "%c", snake_head_sprite.character);
  curr = curr->s;

  setcolor(snake_sprite);
  while (curr != NULL) {
    movecursor(curr->pos_x, curr->pos_y);
    fprintf(stdout, "%c", snake_sprite.character);
    curr = curr->s;
  }

  fprintf(stdout, "%s", reset_color);
  return;
}
