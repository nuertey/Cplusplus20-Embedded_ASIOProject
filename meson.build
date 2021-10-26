project(
    'TemperatureReadoutApplication', 
    'cpp', 
    default_options: [
                        'buildtype=release', 
                        'c_std=gnu99',
                        'cpp_std=c++2a'
                     ],
    license: 'Copyright (c) 2021 Nuertey Odzeyem',
    version: '0.0.1',
    # This meson version is important since we are depending upon certain
    # meson features existing in the code that follows. An example of 
    # such a feature is:
    #
    # wrap-git  clone-recursive - also clone submodules of the repository
    # (only available since version 0.48.0)  
    meson_version: '>= 0.53.0'
)

cxx = meson.get_compiler('cpp')

assert(cxx.get_id() == 'gcc', 'For now, this project only supports the GCC compiler.')
assert(host_machine.system() == 'linux')

compiler_settings = [
    '-Wall',
    
    # Narrowing of variable conversions is especially dangerous on Embedded
    # platforms so flag it as a compiler error.
    '-Werror=narrowing',
    '-Wmisleading-indentation',
    
    # Apropos of clean coding guidelines, we do not want to see unused
    # parameters:
    '-Wunused',
    '-Wno-unused-parameter',
    '-pedantic',
    
    # Expose the dynamic symbol table so we can see actual symbol names
    # instead of the binary name during system crash logging (backtrace):
    '-rdynamic',
    '-O3',
    
    # It pays to turn this on when compile fails. Verbose builds, i.e. -E -dI
    # TBD Nuertey Odzeyem: Turn off during production and normal usage.
    '-v',
    
    # Include debugging symbols in order to be able to debug potential
    # core dumps. If after build, binary size is too large, can strip off
    # debugging symbols post-build: 
    '-g',
    
    # Enable stack unwinding from asynchronous events. That is, allow 
    # walking back the function call stack from any arbitrary point in 
    # the code. This gives us the power to peek inside a function caller's
    # context to, for example, retrieve the location from which the called 
    # function has been called from without actually completing its 
    # execution and exiting its code execution. In my experience, such 
    # a compile strategy aids the debug and profiling of asynchronous 
    # code, of which state machine events, be it from timers' expiries 
    # or network packet arrivals or whatnot triggers, are just a few.
    '-fasynchronous-unwind-tables',
    
    # Apologies Khalil Gibran but spontaneously said this Seer, "I see in
    # our future the potential for performance profiling with the uftrace
    # linux tool. Hence enable its enabling compiler flag a priori."
    '-finstrument-functions',
    
    # Automagically detect memory leaks when attempting to invoke the 
    # executable. Should such a memory leak occur, the return value of 
    # the program executable would then be non-zero. Conceivably and 
    # consequentially then, that value could then be checked and subsequently
    # flagged in some automated test script or suite:
    '-fsanitize=address', 
    '-fno-omit-frame-pointer',
    '-static-libasan',
    
    # And of course, the quintessential one--undefined behavior. There be
    # dragons! Since we should never rely on Undefined Behavior--as its
    # effects are equally likely to be the manifestation of a quantum shift
    # precipitating the onset of a black hole which lugubriously will 
    # swallow the earth whole and make it disappear--flag those too:
    '-fsanitize=undefined',
    
    # Enable ASIO's Handler Tracking debugging facility. Other useful 
    # defines that can be used to control the interface, functionality, 
    # and behaviour of ASIO can be found at:
    #
    # https://think-async.com/Asio/asio-1.19.2/doc/asio/using.html#asio.using.macros
    #'-DASIO_ENABLE_HANDLER_TRACKING'
]

add_project_arguments(cxx.get_supported_arguments(compiler_settings), language: 'cpp')

# Include directories
#
# Most C/C++ projects have headers in different directories than sources.
# Thus you need to specify include directories. Let's assume that we are
# at some subdirectory and wish to add its include subdirectory to some
# target's search path. To create a include directory object we do this:
# 
# incdir = include_directories('include')
# 
# The incdir variable now holds a reference to the include subdir. Now 
# we pass that as an argument to a build target:
# 
# executable('someprog', 'someprog.c', include_directories : incdir)
# 
# Note that these two commands can be given in any subdirectories and it
# will still work. Meson will keep track of the locations and generate 
# proper compiler flags to make it all work.
# 
# Another thing to note is that include_directories adds both the source
# directory and corresponding build directory to include path, so you 
# don't have to manually add those.

incdir = []
incdir += include_directories(
  '/usr/local/include', # $ASIO_ROOT.
)
# Can add more previously defined top_inc include_directories here:
# incdir += top_inc 

# Note that the subproject object is not used as the dependency, but 
# rather, one obtains the declared dependency from it with get_variable.
# This is because the subproject as it is could have multiple declared
# dependencies all of which may or may not be depended upon:
sensor_node = subproject('TestArtifactSensorNode')

# Note that meson WILL error out if the dependency is designated required
# and the dependency is not found. This may or may not satisfy present needs. 
# However, another useful strategy is to declare the dependency optional,
# its actual utility notwithstanding, and then perform further configurations 
# as needed. Such is how the case of the spdlog dependency is handled
# further below.

# Note that the functionality that used to be provided by librt - the 
# POSIX.1b Realtime Extensions library has now been absorbed in here 
# (-lpthread) and libc generally.
thread_dep = dependency('threads', required : true)

unwind_dep = dependency('libunwind', required : true)

# How To Toggle Between System Libraries And Embedded Sources:
#
# To illustrate the correct way to have a project depend upon system 
# libraries and proactively fall back upon embedding sources if that 
# dependency is not available, behold the following code snippet. Its 
# strategy comes in handy when, for example, building distros. When 
# building distros, as a rule one is forbidden to embed dependency sources.
# Hence it becomes imperative to produce a buildable package by skirting
# around in the following manner:
#
# sudo apt-get remove libspdlog-dev
# Use manual install of header files...

# |Using subprojects/TestArtifactSensorNode/subprojects/spdlog.wrap
spdlog_dep = dependency('spdlog', required : true)

if not spdlog_dep.found()
    spdlog_proj = subproject('spdlog')
    spdlog_dep = spdlog_proj.get_variable('spdlog_dep')
endif

# Meson gives us the option of writing the above conditional operations
# in the following convenient shorthand as it is such a common operation:
#spdlog_dep = dependency('spdlog', fallback : [spdlog, spdlog_dep])

# '-lfmt' is better replaced with the following because more portable:

# |Using subprojects/TestArtifactSensorNode/subprojects/fmt.wrap
fmt_dep = dependency('fmt', required : true)

if not fmt_dep.found()
    fmt_proj = subproject('fmt')
    fmt_dep = fmt_proj.get_variable('fmt_dep')
endif

# As an addendum, ensure that the host system also has the requisite  
# Address Sanitizer libs installed:
# 
# sudo apt install libasan2
# sudo apt install libubsan0 lib64ubsan0
asan_dep = cxx.find_library('asan', required: true)
ubsan_dep = cxx.find_library('ubsan', required: true)

temperature_readout_project_sources = files([
    'SessionManager.cpp',
    'TemperatureReadoutApplication.cpp'
])

# /home/nuertey/py37-venv/bin/ninja -C build
temperature_readout_project = executable(
    'TemperatureReadoutApplication', 
    temperature_readout_project_sources,
    include_directories : incdir, # Do NOT explicitly specify an array object here.
    dependencies : [  asan_dep,
                      ubsan_dep,
                      thread_dep,
                      unwind_dep, 
                      fmt_dep
                   ],
    # As math.h is not a part of the standard C library, ensure to link to it.
    link_args : ['-Wl,-Map=TemperatureReadoutApplication.map', '-lasan', '-fsanitize=undefined', '-lm'],
    install : true,
)

custom_target('size', 
              output: ['dummy.txt'], 
              command: [find_program('size'), temperature_readout_project.full_path()], 
              depends: temperature_readout_project, 
              build_by_default: true
             )
