# Shared compiler warning flags.
#
# Usage: moku_enable_warnings(<target>)
# The flags are PRIVATE, so they are not forwarded to targets that link against <target>.

option(MOKU_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

function(moku_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(MOKU_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
        if(MOKU_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()