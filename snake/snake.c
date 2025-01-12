// clang snake.c -std=gnu99 -O3 -g -Wextra -Wall

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// My snake is indeed a linked list.
struct snake {
  int pos_x;
  int pos_y;
  struct snake *s;
};

// Food struct for points ect.
struct food {
  int pos_x;
  int pos_y;
};

enum direction { UP = 0, DOWN = 1, RIGHT = 2, LEFT = 3 };

void winclear(void);
void movecursor(int x, int y);
void nsleep(int nsec);
void updatewinsize(void);

int check_collisions(struct snake *s, struct food *f, int size);

// Dunctions for the food.
void f_print(struct food *f, int size);
void f_init(struct food *f, int size);

// Functions for the snake.
void s_move(struct snake *s);
void s_add(struct snake *s);
void s_delete(struct snake *s);
void s_print(struct snake *s);

// Used to get terminal size.
struct winsize win;
int snake_dir = RIGHT;

int main(void) {
  srand(time(NULL));
  char input[5];

  // Get initial terminal size.
  updatewinsize();

  // Initialize the food struct.
  struct food food[10];
  int food_size = sizeof(food) / sizeof(struct food);
  f_init(food, food_size);

  // Initialize the player struct.
  struct snake *player = malloc(sizeof(struct snake));
  (*player) = (struct snake){0, 0, NULL};
  s_add(player);

  // Disable echo and canonical mode.
  struct termios default_t, new_t;
  tcgetattr(STDIN_FILENO, &default_t);
  new_t = default_t;
  new_t.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

  // Set the standard input to non-blocking mode
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

  while (1) {
    winclear();
    updatewinsize();

    // Get input.
    memset(&input, '\0', sizeof(input));      // 'clear' the input buffer.
    read(STDIN_FILENO, input, sizeof(input)); // Get input (POSIX).

    if (strcmp(input, "\x1b[A") == 0) {
      snake_dir = UP;
    } else if (strcmp(input, "\x1b[B") == 0) {
      snake_dir = DOWN;
    } else if (strcmp(input, "\x1b[C") == 0) {
      snake_dir = RIGHT;
    } else if (strcmp(input, "\x1b[D") == 0) {
      snake_dir = LEFT;
    }

    s_move(player);
    if (check_collisions(player, food, food_size) == 1) {
      s_delete(player);
      break;
    }

    f_print(food, food_size);
    s_print(player);

    nsleep(2 * 33333333); // Should be read as: x / 30fps

    if (strcmp(input, "\x1b") == 0) {
      s_delete(player);
      break;
    }
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &default_t);
  return EXIT_SUCCESS;
}

//////////////// FUNCTION DEFINITIONS ////////////////

void winclear(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  return;
}

void movecursor(int x, int y) {
  char buf[12];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y, x);
  write(STDOUT_FILENO, buf, strlen(buf));
  return;
}

void nsleep(int nsec) {
  struct timespec tm = {0, nsec};
  nanosleep(&tm, NULL);
  return;
}

void updatewinsize(void) {
  ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
  return;
}

int check_collisions(struct snake *s, struct food *f, int size) {
  bool **occupied = calloc(win.ws_row, sizeof(bool *));
  for (int i = 0; i < win.ws_row; i++) {
    occupied[i] = calloc(win.ws_col, sizeof(bool));
  }

  // Get occupied cells of the snake.
  struct snake *curr = s->s;
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

void f_print(struct food *f, int size) {
  size_t bufsize = size * (10 + 14);
  // 10 for movecursor() 14 for formated output string.
  char *outbuf = malloc(bufsize);
  int index = 0;

  // movecursor(f[i].pos_x, f[i].pos_y);
  for (int i = 0; i < size; i++) {
    index +=
        snprintf(outbuf + index, bufsize - index,
                 "\x1b[%d;%dH\x1b[48;5;2m*\x1b[0m\n", f[i].pos_y, f[i].pos_x);
  }
  write(STDOUT_FILENO, outbuf, strlen(outbuf));

  free(outbuf);
  return;
}

void f_init(struct food *f, int size) {
  for (int i = 0; i < size; i++) {
    f[i].pos_x = rand() % win.ws_col;
    f[i].pos_y = rand() % win.ws_row;
  }
  return;
}

void s_move(struct snake *s) {
  struct snake *curr = s;
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
    curr->pos_x = curr->pos_x ^ x;
    x = curr->pos_x ^ x;
    curr->pos_x = curr->pos_x ^ x;
    curr->pos_y = curr->pos_y ^ y;
    y = curr->pos_y ^ y;
    curr->pos_y = curr->pos_y ^ y;
    curr = curr->s;
  }
  return;
}

void s_add(struct snake *s) {
  struct snake *curr = s;
  while (curr->s != NULL) {
    curr = curr->s;
  }

  int old_x = curr->pos_x;
  int old_y = curr->pos_y;

  s_move(s);

  curr->s = malloc(sizeof(struct snake));
  *curr->s = (struct snake){old_x, old_y, NULL};

  return;
}

void s_delete(struct snake *s) {
  struct snake *curr = s;
  struct snake *nxt;

  while (curr != NULL) {
    nxt = curr->s;
    free(curr);
    curr = nxt;
  }
  return;
}

void s_print(struct snake *s) {
  struct snake *curr = s;

  while (curr != NULL) {
    movecursor(curr->pos_x, curr->pos_y);
    write(STDOUT_FILENO, "\x1b[48;5;7mS\x1b[0m", 14);
    curr = curr->s;
  }
  return;
}
