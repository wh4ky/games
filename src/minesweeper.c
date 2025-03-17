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

#define FPS 60

static inline void winclear(void);
void floodFill(int, int);

struct cell {
  char near;
  bool bomb;
  bool flagged;
  bool revealed;
};

struct map {
  int rows;
  int cols;
  struct cell **map;
} map;

struct cursor {
  int x;
  int y;
} cursor;

struct winsize win;
const int offsets[8][2] = {{1, 1},  {1, 0},  {1, -1}, {0, 1},
                           {0, -1}, {-1, 1}, {-1, 0}, {-1, -1}};

int main(void) {
  srand((unsigned int)time(NULL)); // Seed the random number generator.
  char out_buffer[BUFSIZ];         // Stack-based buffer
  setvbuf(stdout, out_buffer, _IOFBF,
          BUFSIZ); // Set full buffering and ensure stack-based buffer.
  char input[7];

  cursor.x = 1;
  cursor.y = 1;

  // Init timespecs.
  struct timespec start, end, sleep;
  int sleepfor = (int)1e9 / FPS;

  // Initialize map struct.
  map.rows = 10;
  map.cols = 20;
  map.map = calloc((unsigned long)map.rows, sizeof(struct cell *));
  for (int i = 0; i < map.rows; i++) {
    map.map[i] = calloc((unsigned long)map.cols, sizeof(struct cell));
  }

  for (int r = 0; r < map.rows; r++) {
    for (int c = 0; c < map.cols; c++) {
      struct cell default_cell = {
          .near = 0,
          .bomb = false,
          .flagged = false,
          .revealed = false,
      };
      map.map[r][c] = default_cell;
    }
  }

  // Initialize all cells in the map.
  for (int i = 0; i < 20; i++) {
    int r = rand() % map.rows;
    int c = rand() % map.cols;
    map.map[r][c].bomb = true;
    map.map[r][c].near = 9;
  }

  for (int i = 0; i < map.rows; i++) {
    for (int j = 0; j < map.cols; j++) {
      if (map.map[i][j].bomb) {
        for (int k = 0; k < 8; k++) {
          int nr = i + offsets[k][0];
          int nc = j + offsets[k][1];
          if (nr >= 0 && nr < map.rows && nc >= 0 && nc < map.cols &&
              map.map[nr][nc].near != 9) {
            map.map[nr][nc].near += 1;
          }
        }
      }
    }
  }

  // Disable echo and canonical mode.
  struct termios default_t, new_t;
  tcgetattr(STDIN_FILENO, &default_t);
  new_t = default_t;
  new_t.c_lflag &= ~(tcflag_t)(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

  printf("\x1b[m");
  fflush(stdout);

  // Main game loop.
  while (1) {
    winclear();

    // Start clock (so you can have a smooth frame speed).
    clock_gettime(CLOCK_MONOTONIC, &start);

    printf("\x1b[0;0H");
    for (int i = 0; i < map.rows; i++) {
      for (int j = 0; j < map.cols; j++) {
        if (map.map[i][j].revealed) {
          char *color = "";

          switch (map.map[i][j].near) {
          case 1:
            color = "\x1b[38;5;21m";
            break;
          case 2:
            color = "\x1b[38;5;2m";
            break;
          case 3:
            color = "\x1b[38;5;9m";
            break;
          case 4:
            color = "\x1b[38;5;4m";
            break;
          case 5:
            color = "\x1b[38;5;1m";
            break;
          case 6:
            color = "\x1b[38;5;14m";
            break;
          case 7:
            color = "\x1b[38;5;0m";
            break;
          case 8:
            color = "\x1b[38;5;8m";
            break;
          }

          printf("%s%c\x1b[m", color,
                 map.map[i][j].near ? map.map[i][j].near + '0' : ' ');
        } else {
          printf("#");
        }
      }
      printf("\n");
    }
    printf("\n");

    printf("\x1b[%d;%dH", cursor.y, cursor.x);
    fflush(stdout);

    memset(&input, '\0', sizeof(input));          // 'clear' the input buffer.
    read(STDIN_FILENO, input, sizeof(input) - 1); // Get input (POSIX).

    if (input[0] == '\x1b') {
      switch (input[2]) {
      case 'A':
        cursor.y--;
        break;
      case 'B':
        cursor.y++;
        break;
      case 'C':
        cursor.x++;
        break;
      case 'D':
        cursor.x--;
        break;
      }
      if (strcmp(input, "\x1b") == 0) {
        goto exit;
      }
    } else {
      switch (input[0]) {
      case '\n':
        if (map.map[cursor.y - 1][cursor.x - 1].bomb) {
          printf("\x1b[2J!DEATH!");
          fflush(stdout);
          goto exit;
        } else {
          floodFill(cursor.x - 1, cursor.y - 1);
        }
        break;
      }
    }

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
  for (int i = 0; i < map.rows; i++) {
    free(map.map[i]);
  }
  free(map.map);

  tcsetattr(STDIN_FILENO, TCSANOW, &default_t);
  return EXIT_SUCCESS;
}

static inline void winclear(void) {
  fprintf(stdout, "\x1b[2J");
  return;
}

void floodFill(int x, int y) {
  if (map.map[y][x].near == 0 && map.map[y][x].revealed == false) {
    map.map[y][x].revealed = true;
    for (int i = 0; i < 8; i++) {
      int ny = y + offsets[i][0];
      int nx = x + offsets[i][1];
      if (ny >= 0 && ny < map.rows && nx >= 0 && nx < map.cols &&
          map.map[ny][nx].near != 9) {
        floodFill(nx, ny);
        map.map[ny][nx].revealed = true;
      }
    }
  } else {
    map.map[y][x].revealed = true;
  }
}
