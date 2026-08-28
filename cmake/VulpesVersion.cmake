include_guard(GLOBAL)

function(vulpes_format_version description fallback_version output_version output_commit output_dirty)
    set(normalized "${description}")
    set(dirty FALSE)
    if(normalized MATCHES "^(.*)-dirty$")
        set(normalized "${CMAKE_MATCH_1}")
        set(dirty TRUE)
    endif()

    set(version "${fallback_version}")
    set(commit "unknown")
    if(normalized MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)-([0-9]+)-g([0-9a-fA-F]+)$")
        set(base_version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
        set(distance "${CMAKE_MATCH_4}")
        set(commit "${CMAKE_MATCH_5}")
        if(distance EQUAL 0)
            if(dirty)
                set(version "${base_version}+g${commit}.dirty")
            else()
                set(version "${base_version}")
            endif()
        else()
            set(version "${base_version}-dev.${distance}+g${commit}")
            if(dirty)
                string(APPEND version ".dirty")
            endif()
        endif()
    elseif(normalized MATCHES "^([0-9a-fA-F]+)$")
        set(commit "${CMAKE_MATCH_1}")
        set(version "${fallback_version}-dev+g${commit}")
        if(dirty)
            string(APPEND version ".dirty")
        endif()
    endif()

    set(${output_version} "${version}" PARENT_SCOPE)
    set(${output_commit} "${commit}" PARENT_SCOPE)
    set(${output_dirty} "${dirty}" PARENT_SCOPE)
endfunction()

function(vulpes_detect_version source_dir fallback_version output_version output_commit output_dirty output_description)
    if(VULPES_BUILD_VERSION_OVERRIDE)
        if(NOT VULPES_BUILD_VERSION_OVERRIDE MATCHES "^[0-9A-Za-z][0-9A-Za-z.+-]*$")
            message(FATAL_ERROR "VULPES_BUILD_VERSION_OVERRIDE contains unsupported characters")
        endif()
        set(${output_version} "${VULPES_BUILD_VERSION_OVERRIDE}" PARENT_SCOPE)
        set(${output_commit} "unknown" PARENT_SCOPE)
        set(${output_dirty} FALSE PARENT_SCOPE)
        set(${output_description} "override" PARENT_SCOPE)
        return()
    endif()

    set(description "")
    if(Git_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --long --always
                --dirty --abbrev=12
            WORKING_DIRECTORY "${source_dir}"
            RESULT_VARIABLE describe_result
            OUTPUT_VARIABLE description
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT describe_result EQUAL 0)
            set(description "")
        endif()
    endif()

    vulpes_format_version("${description}" "${fallback_version}" version commit dirty)
    set(${output_version} "${version}" PARENT_SCOPE)
    set(${output_commit} "${commit}" PARENT_SCOPE)
    set(${output_dirty} "${dirty}" PARENT_SCOPE)
    set(${output_description} "${description}" PARENT_SCOPE)
endfunction()

function(vulpes_watch_git source_dir)
    if(NOT Git_FOUND)
        return()
    endif()

    foreach(git_path HEAD index)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${git_path}"
            WORKING_DIRECTORY "${source_dir}"
            RESULT_VARIABLE path_result
            OUTPUT_VARIABLE resolved_path
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(path_result EQUAL 0)
            if(NOT IS_ABSOLUTE "${resolved_path}")
                cmake_path(ABSOLUTE_PATH resolved_path BASE_DIRECTORY "${source_dir}" NORMALIZE)
            endif()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${resolved_path}")
        endif()
    endforeach()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" symbolic-ref --quiet HEAD
        WORKING_DIRECTORY "${source_dir}"
        RESULT_VARIABLE reference_result
        OUTPUT_VARIABLE reference
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(reference_result EQUAL 0)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${reference}"
            WORKING_DIRECTORY "${source_dir}"
            RESULT_VARIABLE path_result
            OUTPUT_VARIABLE reference_path
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(path_result EQUAL 0)
            if(NOT IS_ABSOLUTE "${reference_path}")
                cmake_path(ABSOLUTE_PATH reference_path BASE_DIRECTORY "${source_dir}" NORMALIZE)
            endif()
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${reference_path}")
        endif()
    endif()
endfunction()
