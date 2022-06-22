set_project("tomasulo")
set_version("0.0.1")

add_rules("mode.debug", "mode.release")
set_languages("cxx17")
set_warnings("everything", "error")
add_cxflags("-Wno-c++98-compat", "-Wno-old-style-cast")


target("tomasulo")
    set_kind("shared")
    set_prefixname("")
    add_files("src/*.cpp")

    on_load(function (target)
        os.execv("python", {"-m", "pip", "install", "pybind11"})
        local pyinclude = os.iorunv("python", {"-m", "pybind11", "--includes"})
        for _, inc in ipairs(pyinclude:split(' ')) do
            target:add("cxflags", "-isystem"..inc:sub(3))
        end
    end)
    after_build(function (target)
        local targetfile = target:targetfile()
        os.mkdir("lib")
        os.cp(targetfile, path.join("./lib", path.filename(targetfile)))
    end)


--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro defination
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

