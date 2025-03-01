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

// Sprite strings.
struct Sprite {
  const char *bg_color;
  const char *fg_color;
  const char character;
};

// My snake is indeed a linked list.
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
void winclear(void);
void movecursor(int x, int y);
void nsleep(int nsec);
void updatewinsize(void);
void setcolor(const struct Sprite sprite);

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
    .bg_color = "\x1b[48;5;7m",
    .fg_color = "\x1b[38;5;15m",
    .character = '~',
};

const struct Sprite snake_head_sprite = {
    .bg_color = "\x1b[48;5;8m", .fg_color = "\x1b[38;5;9m", .character = '0'};

const struct Sprite food_sprite = {
    .bg_color = "\x1b[48;5;2m",
    .fg_color = "\x1b[38;5;10m",
    .character = '@',
};

// String to reset terminal colors.
const char *reset_color = "\x1b[0m";

/*
 * MAIN FUNCTION
 */
int main(void) {
  srand((unsigned int)time(NULL)); // Seed the random number generator.
  char input[5];                   // Input buffer.

  // Init timespecs.
  struct timespec start, end, sleep;
  int sleepfor = 2 * 33333333;

  // Get initial terminal size.
  updatewinsize();

  // Initialize the food struct.
  struct Food food[10];
  int food_size = sizeof(food) / sizeof(struct Food);
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

    if (strcmp(input, "\x1b[A") == 0 || strcmp(input, "w") == 0 ||
        strcmp(input, "k") == 0) {
      snake_dir = UP;
    } else if (strcmp(input, "\x1b[B") == 0 || strcmp(input, "s") == 0 ||
               strcmp(input, "j") == 0) {
      snake_dir = DOWN;
    } else if (strcmp(input, "\x1b[C") == 0 || strcmp(input, "d") == 0 ||
               strcmp(input, "l") == 0) {
      snake_dir = RIGHT;
    } else if (strcmp(input, "\x1b[D") == 0 || strcmp(input, "a") == 0 ||
               strcmp(input, "h") == 0) {
      snake_dir = LEFT;
    }

    if (strcmp(input, "\x1b") == 0) {
      break;
    }

    s_move(player);

    if (check_collisions(player, food, food_size) == 1) {
      break;
    }

    f_print(food, food_size);
    s_print(player);

    clock_gettime(CLOCK_MONOTONIC, &end);
    int elapsed_time = (int)((end.tv_sec - start.tv_sec) * (long)1e9 +
                             (end.tv_nsec - start.tv_nsec));
    int remaining_time = sleepfor - elapsed_time;
    if (remaining_time > 0) {
      sleep.tv_sec = remaining_time / (long)1e9;  // Convert to seconds
      sleep.tv_nsec = remaining_time % (long)1e9; // Remaining nanoseconds
      nanosleep(&sleep, NULL);
    }
  }

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
void winclear(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  return;
}

/*
 * Moves the cursor to the 'x', 'y' coordinates.
 */
void movecursor(int x, int y) {
  char buf[12];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y, x);
  write(STDOUT_FILENO, buf, strlen(buf));
  return;
}

/*
 * Sleep for 'nsec' nanoseconds.
 */
void nsleep(int nsec) {
  struct timespec tm = {0, nsec};
  nanosleep(&tm, NULL);
  return;
}

/*
 * Updates the window size.
 */
void updatewinsize(void) {
  ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
  return;
}

/*
 * Sets the background and foreground color of the given 'Sprite' struct.
 */
void setcolor(const struct Sprite sprite) {
  size_t bufsize = strlen(sprite.fg_color) + strlen(sprite.bg_color) + 1;
  char *buffer = malloc(bufsize * sizeof(char));
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocate buffer for sprite color!");
    return;
  }

  snprintf(buffer, bufsize, "%s%s", sprite.fg_color, sprite.bg_color);
  write(STDOUT_FILENO, buffer, bufsize);

  free(buffer);
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
  size_t bufsize = (size_t)(size * (11));
  // 10 for cursor movement, 1 for the food_sprite.character
  char *outbuf = malloc(bufsize);
  int index = 0;

  setcolor(food_sprite);

  for (int i = 0; i < size; i++) {
    index +=
        snprintf(outbuf + index, bufsize - (size_t)index, "\x1b[%d;%dH%c\n",
                 f[i].pos_y, f[i].pos_x, food_sprite.character);
  }
  write(STDOUT_FILENO, outbuf, strlen(outbuf));
  write(STDOUT_FILENO, reset_color, strlen(reset_color));

  free(outbuf);
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
 * Outputs the given 'Snake' struct to 'STDOUT_FILENO'.
 */
void s_print(struct Snake *s) {
  struct Snake *curr = s;

  size_t bufsize = 1;
  int index = 0;
  char *outbuf = malloc(bufsize);
  if (outbuf == NULL) {
    fprintf(stderr, "Failed to alocate memory for snake sprite segments.");
    return;
  }

  setcolor(snake_head_sprite);
  movecursor(curr->pos_x, curr->pos_y);
  write(STDOUT_FILENO, &snake_head_sprite.character, 1);
  curr = curr->s;

  setcolor(snake_sprite);
  while (curr != NULL) {
    bufsize += 11; // 10 for cursor movement, 1 for the snake_sprite.character

    char *temp = realloc(outbuf, bufsize);
    if (temp == NULL) {
      fprintf(stderr, "Failed to realocate memory for snake sprite segment.");
      free(outbuf);
      return;
    }
    outbuf = temp;

    index +=
        snprintf(outbuf + index, bufsize - (size_t)index, "\x1b[%d;%dH%c\n",
                 curr->pos_y, curr->pos_x, snake_sprite.character);
    curr = curr->s;
  }

  write(STDOUT_FILENO, outbuf, strlen(outbuf));
  write(STDOUT_FILENO, reset_color, strlen(reset_color));
  free(outbuf);
  return;
}
