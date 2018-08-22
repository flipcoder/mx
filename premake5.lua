workspace("mx")
    targetdir("bin")
    
    configurations {"Debug", "Release"}

        defines {
        }
        
        -- Debug Config
        configuration "Debug"
            defines { "DEBUG" }
            symbols "On"
            linkoptions { }

            configuration "linux"
                links {
                }
        
        -- Release Config
        configuration "Release"
            defines { "NDEBUG" }
            optimize "speed"
            targetname("mx_dist")

        -- gmake Config
        configuration "gmake"
            buildoptions { "-std=c++11" }
            -- buildoptions { "-std=c++11", "-pedantic", "-Wall", "-Wextra", '-v', '-fsyntax-only'}
            links {
                "pthread",
                "boost_system",
                "boost_chrono",
                "boost_coroutine",
            }
            includedirs {
                "/usr/local/include/",
            }

            libdirs {
                "/usr/local/lib"
            }
            
            buildoptions {
            }

            linkoptions {
            }
            
        configuration "macosx"
            links {
            }
            
            --buildoptions { "-U__STRICT_ANSI__", "-stdlib=libc++" }
            --linkoptions { "-stdlib=libc++" }

        configuration "windows"
            toolset "v140"
            flags { "MultiProcessorCompile" }
        
            links {
                "boost_system-vc140-mt-1_61",
            }

            includedirs {
                "c:/local/boost_1_61_0",
                "c:/msvc/include",
            }
            configuration { "windows", "Debug" }
                libdirs {
                    "c:/msvc/lib32/debug"
                }
            configuration { "windows" }
            libdirs {
                "c:/msvc/lib32",
                "c:/local/boost_1_61_0/lib32-msvc-14.0",
            }
            -- buildoptions {
                -- "/MP",
                -- "/Gm-",
            -- }
            
            configuration { "windows", "Debug" }
                links {
                }
            configuration {}
            configuration { "windows", "Release" }
                links {
                }

    project "mx"
        kind "ConsoleApp"
        language "C++"

        -- Project Files
        files {
            "tests/**",
            "mx/**"
        }

        -- Exluding Files
        excludes {
        }
        
        includedirs {
        }

