const std = @import("std");

const Direction = enum(u2) { UP = 0, DOWN = 1, RIGHT = 2, LEFT = 3 };

const Snake = struct { pos_x: u16, pos_y: u16, s: ?*Snake };
const Food = struct { pos_x: u16, pos_y: u16 };

var prng: std.Random.Xoshiro256 = undefined;
var win: std.c.winsize = undefined;
var snakeDirection: Direction = Direction.RIGHT;

pub fn main() !void {
    var input: [5]u8 = undefined;
    try updatewinsize();

    // Initialize random numbers.
    prng = std.Random.DefaultPrng.init(blk: {
        var seed: u32 = undefined;
        try std.posix.getrandom(std.mem.asBytes(&seed));
        break :blk seed;
    });

    // Initialize allocator.
    var AreaAlloc: std.heap.ArenaAllocator = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer AreaAlloc.deinit();
    var allocator: std.mem.Allocator = AreaAlloc.allocator();

    // Allocate snake
    const player: *Snake = try allocator.create(Snake);
    player.* = Snake{ .pos_x = 1, .pos_y = 1, .s = null };

    // Allocate food.
    var food: []Food = try allocator.alloc(Food, 10);
    f_init(&food);

    // Initialize termios.
    const defaultTerm: std.posix.termios = try std.posix.tcgetattr(std.posix.STDIN_FILENO);
    defer _ = std.posix.tcsetattr(std.posix.STDIN_FILENO, std.posix.TCSA.NOW, defaultTerm) catch {};
    var newTerm: std.posix.termios = defaultTerm;
    newTerm.lflag.ECHO = false;
    newTerm.lflag.ICANON = false;
    try std.posix.tcsetattr(std.posix.STDIN_FILENO, std.posix.TCSA.NOW, newTerm);

    // Main game loop.
    while (true) {
        try clearwin();
        try updatewinsize();
        // TODO: Read only if there is input (Epoll).

        input = undefined;
        _ = try std.posix.read(std.posix.STDIN_FILENO, &input);

        if (std.mem.count(u8, &input, "\x1b[A") > 0 or std.mem.count(u8, &input, "w") > 0) {
            snakeDirection = Direction.UP;
        } else if (std.mem.count(u8, &input, "\x1b[B") > 0 or std.mem.count(u8, &input, "s") > 0) {
            snakeDirection = Direction.DOWN;
        } else if (std.mem.count(u8, &input, "\x1b[C") > 0 or std.mem.count(u8, &input, "d") > 0) {
            snakeDirection = Direction.RIGHT;
        } else if (std.mem.count(u8, &input, "\x1b[D") > 0 or std.mem.count(u8, &input, "a") > 0) {
            snakeDirection = Direction.LEFT;
        }

        try s_move(player);
        if (try checkCollisions(player, &food, allocator)) {
            break;
        }

        try f_print(&food);
        try s_print(player);

        if (std.mem.count(u8, &input, "q") > 0 or std.mem.count(u8, &input, "\x1b") > 0) {
            break;
        }
        std.posix.nanosleep(0, 4 * 33333333);
    }
    return;
}

// FUNCTIONS //

/// Clears the terminal.
inline fn clearwin() !void {
    _ = try std.posix.write(std.posix.STDOUT_FILENO, "\x1b[2J");
    return;
}

/// Updates the global 'win' struct to have the latest window size.
inline fn updatewinsize() !void {
    const out: c_int = std.c.ioctl(std.posix.STDOUT_FILENO, std.posix.T.IOCGWINSZ, &win);
    if (out != 0) {
        const err = std.posix.errno(out);
        std.debug.print("C errno: {}\n", .{err});
        return error.FailedCFunctionCall;
    }
    return;
}

/// Moves the cursor to column: x, row: y on the terminal.
inline fn movecursor(x: u16, y: u16) !void {
    var tmp: [10]u8 = undefined;
    _ = try std.fmt.bufPrint(&tmp, "\u{001b}[{d};{d}H", .{ y, x });
    _ = try std.posix.write(std.posix.STDOUT_FILENO, &tmp);
    return;
}

/// Check collisions.
fn checkCollisions(s: *Snake, f: *[]Food, SnakeAllocator: std.mem.Allocator) !bool {
    var AreaAlloc: std.heap.ArenaAllocator = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer AreaAlloc.deinit();
    var allocator: std.mem.Allocator = AreaAlloc.allocator();

    const occupied = try allocator.alloc([]bool, win.ws_row);
    for (occupied, 0..) |_, i| {
        occupied[i] = try allocator.alloc(bool, win.ws_col);
    }

    // Get occupied cells of the snake.
    var curr: ?*Snake = s.s;
    while (curr) |snake| {
        if (snake.pos_y > 0 and snake.pos_y < win.ws_row and snake.pos_x > 0 and snake.pos_x < win.ws_col) {
            occupied[snake.pos_y][snake.pos_x] = true;
        }
        curr = snake.s;
    }

    // Get occupied cells by the food.
    for (f.*) |*food| {
        var random = prng.random();
        if (food.pos_x > 0 and food.pos_x < win.ws_col and food.pos_y > 0 and food.pos_y < win.ws_row) {
            occupied[food.pos_y][food.pos_x] = true;
        } else {
            // Get out of bound food within boundaries
            food.pos_x = random.int(u16) % win.ws_col;
            food.pos_y = random.int(u16) % win.ws_row;
            occupied[food.pos_y][food.pos_x] = true;
        }
    }

    // Check if there was collision at all.
    if (s.pos_x > 0 and s.pos_x < win.ws_col and s.pos_y > 0 and s.pos_y < win.ws_row and occupied[s.pos_y][s.pos_x]) {
        // Check food collision.
        for (f.*) |*food| {
            var random = prng.random();
            if (s.pos_x == food.pos_x and s.pos_y == food.pos_y) {
                food.pos_x = random.int(u16) % win.ws_col;
                food.pos_y = random.int(u16) % win.ws_row;
                try s_add(s, SnakeAllocator);
                return false;
            }
        }

        // Check snake-to-snake collision.
        curr = s.s;
        while (curr) |snake| {
            if (s.pos_x == snake.pos_x and s.pos_y == snake.pos_y) {
                try movecursor(0, 0);
                _ = try std.posix.write(std.posix.STDOUT_FILENO, " ! Death ! ");
                return true; // EXIT
            }
            curr = snake.s;
        }
    }

    // Boundry check.
    curr = s;
    while (curr) |snake| {
        if (snake.pos_x < 0) {
            snake.pos_x = win.ws_col;
        } else if (snake.pos_x > win.ws_col) {
            snake.pos_x = 0;
        }
        if (snake.pos_y < 0) {
            snake.pos_y = win.ws_row;
        } else if (snake.pos_y > win.ws_row) {
            snake.pos_y = 0;
        }
        curr = snake.s;
    }

    return false;
}

/// Initialize the given food array.
fn f_init(f: *[]Food) void {
    for (f.*) |*food| {
        var random = prng.random();
        food.pos_x = random.int(u16) % win.ws_col;
        food.pos_y = random.int(u16) % win.ws_row;
    }
    return;
}

/// Print all food items.
pub fn f_print(f: *[]Food) !void {
    for (f.*) |*food| {
        try movecursor(food.pos_x, food.pos_y);
        _ = try std.posix.write(std.posix.STDOUT_FILENO, "\u{001b}[48;5;2m*\u{001b}[0m\n");
    }

    return;
}

/// Add a segment to the snake.
fn s_add(s: *Snake, alloc: std.mem.Allocator) !void {
    var curr: ?*Snake = s.s;
    while (curr) |snake| {
        curr = snake.s;
    }

    curr = s;
    if (curr) |snake| {
        const old_x: u16 = snake.pos_x;
        const old_y: u16 = snake.pos_y;
        try s_move(s);

        snake.s = try alloc.create(Snake);
        snake.s.?.* = Snake{ .pos_x = old_x, .pos_y = old_y, .s = null };
    }
    return;
}

/// Move the snake.
fn s_move(s: *Snake) !void {
    var curr: ?*Snake = s;
    var x: u16 = undefined;
    var y: u16 = undefined;

    if (curr) |segment| {
        x = segment.pos_x;
        y = segment.pos_y;
    }

    switch (snakeDirection) {
        Direction.UP => {
            y -= 1;
        },
        Direction.DOWN => {
            y += 1;
        },
        Direction.RIGHT => {
            x += 1;
        },
        Direction.LEFT => {
            x -= 1;
        },
    }

    // https://betterexplained.com/articles/swap-two-variables-using-xor/
    while (curr) |segment| {
        segment.pos_x = segment.pos_x ^ x;
        x = segment.pos_x ^ x;
        segment.pos_x = segment.pos_x ^ x;
        segment.pos_y = segment.pos_y ^ y;
        y = segment.pos_y ^ y;
        segment.pos_y = segment.pos_y ^ y;
        curr = segment.s;
    }
    return;
}

/// Print the snake with all it's segments.
fn s_print(s: *Snake) !void {
    var curr: ?*Snake = s;
    while (curr) |snake| {
        try movecursor(snake.pos_x, snake.pos_y);
        _ = try std.posix.write(std.posix.STDOUT_FILENO, "\u{001b}[48;5;7mS\u{001b}[0m");
        curr = snake.s;
    }
    return;
}
