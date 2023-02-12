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
    lib.addIncludePath(".");
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

    // install ("export") headers that might be needed
    const headers = [_][]const u8{
        // "lib/facil/legacy/fio_mem.h",
        // "lib/facil/redis/resp_parser.h",
        // "lib/facil/redis/redis_engine.h",
        "lib/facil/fiobj/fiobj_data.h",
        "lib/facil/fiobj/fiobj_json.h",
        "lib/facil/fiobj/fiobj_hash.h",
        "lib/facil/fiobj/fiobj_ary.h",
        "lib/facil/fiobj/fiobj_numbers.h",
        "lib/facil/fiobj/fio_tmpfile.h",
        "lib/facil/fiobj/mustache_parser.h",
        "lib/facil/fiobj/fiobj_mustache.h",
        "lib/facil/fiobj/fiobj4fio.h",
        "lib/facil/fiobj/fio_json_parser.h",
        "lib/facil/fiobj/fiobject.h",
        "lib/facil/fiobj/fiobj_str.h",
        "lib/facil/fiobj/fiobj.h",
        "lib/facil/fiobj/fio_siphash.h",
        // "lib/facil/tls/fio_tls.h",
        // "lib/facil/cli/fio_cli.h",
        "lib/facil/http/parsers/hpack.h",
        "lib/facil/http/parsers/websocket_parser.h",
        "lib/facil/http/parsers/http1_parser.h",
        "lib/facil/http/parsers/http_mime_parser.h",
        "lib/facil/http/http_internal.h",
        "lib/facil/http/http.h",
        "lib/facil/http/http1.h",
        "lib/facil/http/websockets.h",
        "lib/facil/fio.h",
    };
    for (headers) |h| lib.installHeader(h, std.fs.path.basename(h));

    // This declares intent for the library to be installed into the standard
    // location when the user invokes the "install" step (the default step when
    // running `zig build`).
    lib.install();
}
