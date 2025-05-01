project "App"
    kind "ConsoleApp"
    language "C++"

    targetdir "bin/%{cfg.buildcfg}-%{cfg.platform}"
    objdir "obj/%{cfg.buildcfg}-%{cfg.platform}"

    files { "**.", "**.cpp" }

    includedirs {
        "C:/Personal/Libraries/glfw/include",
        "C:/VulkanSDK/1.3.296.0/Include"
    }

    libdirs {
        "C:/Personal/Libraries/glfw/lib-vc2022",
        "C:/VulkanSDK/1.3.296.0/Lib"
    }

    links { "glfw3.lib", "vulkan-1" }