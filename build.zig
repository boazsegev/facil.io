const std = @import("std");

// Although this function looks imperative, note that its job is to
// declaratively construct a build graph that will be executed by an external
// runner.
pub fn build(b: *std.Build) !void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard optimization options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall. Here we do not
    // set a preferred release mode, allowing the user to decide how to optimize.
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "facil.io",
        // .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });

    // Generate flags
    var flags = std.ArrayList([]const u8).init(std.heap.page_allocator);
    if (lib.optimize != .Debug) try flags.append("-Os");
    try flags.append("-Wno-return-type-c-linkage");
    try flags.append("-fno-sanitize=undefined");
    try flags.append("-DFIO_OVERRIDE_MALLOC");
    try flags.append("-DFIO_HTTP_EXACT_LOGGING");

    // Include paths
    lib.addIncludePath("lib/facil");
    lib.addIncludePath("lib/facil/fiobj");
    lib.addIncludePath("lib/facil/http");
    lib.addIncludePath("lib/facil/http/parsers");

    // C source files
    lib.addCSourceFiles(&.{
        "lib/facil/fio.c",
        "lib/facil/http/http.c",
        "lib/facil/http/http1.c",
        "lib/facil/http/websockets.c",
        "lib/facil/http/http_internal.c",
        "lib/facil/fiobj/fiobj_numbers.c",
        "lib/facil/fiobj/fio_siphash.c",
        "lib/facil/fiobj/fiobj_str.c",
        "lib/facil/fiobj/fiobj_ary.c",
        "lib/facil/fiobj/fiobj_data.c",
        "lib/facil/fiobj/fiobj_hash.c",
        "lib/facil/fiobj/fiobj_json.c",
        "lib/facil/fiobj/fiobject.c",
        "lib/facil/fiobj/fiobj_mustache.c",
    }, flags.items);

    // link against libc
    lib.linkLibC();

    // This declares intent for the library to be installed into the standard
    // location when the user invokes the "install" step (the default step when
    // running `zig build`).
    lib.install();
}
