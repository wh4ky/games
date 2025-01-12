const std = @import("std");

const Direction = enum(u2) { UP = 0, DOWN = 1, RIGHT = 2, LEFT = 3 };

const Snake = struct { pos_x: u16, pos_y: u16, s: ?*Snake };
const Food = struct { pos_x: u16, pos_y: u16 };

var prng: std.Random.Xoshiro256 = undefined;
var win: std.c.winsize = undefined;
var snakeDirection: u2 = Direction.RIGHT;

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
    const snake: *Snake = try allocator.create(Snake);
    snake.* = Snake{ .pos_x = 1, .pos_y = 1, .s = null };

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
        input = undefined;
        _ = try std.posix.read(std.posix.STDIN_FILENO, &input);

        if (std.mem.count(u8, &input, "q") > 0 or std.mem.count(u8, &input, "\x1b") > 0) {
            break;
        }
        std.posix.nanosleep(0, 20 * 33333333);
    }
    return;
}

// FUNCTIONS //

/// Clears the terminal.
pub inline fn clearwin() !void {
    _ = try std.posix.write(std.posix.STDOUT_FILENO, "\x1b[2J");
    return;
}

/// Updates the global 'win' struct to have the latest window size.
pub inline fn updatewinsize() !void {
    const out: c_int = std.c.ioctl(std.posix.STDOUT_FILENO, std.posix.T.IOCGWINSZ, &win);
    if (out != 0) {
        const err = std.posix.errno(out);
        std.debug.print("C errno: {}\n", .{err});
        return error.FailedCFunctionCall;
    }
    return;
}

/// Moves the cursor to column: x, row: y on the terminal.
pub inline fn movecursor(x: u16, y: u16) !void {
    var tmp: [10]u8 = undefined;
    _ = try std.fmt.bufPrint(&tmp, "\x1b[{d};{d}H", .{ y, x });
    _ = try std.posix.write(std.posix.STDOUT_FILENO, &tmp);
    return;
}

/// Initialize the given food array.
pub fn f_init(f: *[]Food) void {
    for (f.*) |*food| {
        var random = prng.random();
        food.pos_x = random.int(u16) % win.ws_col;
        food.pos_y = random.int(u16) % win.ws_row;
    }
    return;
}

/// Print all food items.
//pub fn f_print(f: *Food, size: u8) !void {}

/// Add a segment to the snake.
pub fn s_add(s: *Snake, alloc: std.mem.Allocator) !void {
    var curr: *Snake = s;
    while (curr.s != null) {
        curr = curr.s;
    }

    const old_x: u16 = curr.pos_x;
    const old_y: u16 = curr.pos_y;
    s_move(s);

    curr.s = try alloc.create(Snake);
    curr.s = Snake{ .pos_x = old_x, .pos_y = old_y, .s = null };

    return;
}

/// Move the snake.
pub fn s_move(s: *Snake) !void {
    var curr: *Snake = s;
    var x: u16 = curr.pos_x;
    var y: u16 = curr.pos_y;

    switch (snakeDirection) {
        0 => {
            y -= 1;
        },
        1 => {
            y += 1;
        },
        2 => {
            x += 1;
        },
        3 => {
            x -= 1;
        },
        else => {},
    }

    // https://betterexplained.com/articles/swap-two-variables-using-xor/
    while (curr != null) {
        curr.pos_x = curr.pos_x ^ x;
        x = curr.pos_x ^ x;
        curr.pos_x = curr.pos_x ^ x;
        curr.pos_y = curr.pos_y ^ y;
        y = curr.pos_y ^ y;
        curr.pos_y = curr.pos_y ^ y;
        curr = curr.s;
    }
    return;
}

/// Print the snake with all it's segments.
pub fn s_print(s: *Snake) !void {
    var curr: *Snake = s;

    while (curr != null) {
        movecursor(curr.pos_x, curr.pos_y);
        std.posix.write(std.posix.STDOUT_FILENO, "\x1b[48;5;7mS\x1b[0m", 14);
        curr = curr.s;
    }
    return;
}
