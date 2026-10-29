# Exercise the real entry point, including options, implementation dispatch,
# output publication and failure exit codes. All child runs are sequential.
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(implementation oop ecs archetype)
    foreach(condition mixed empty dummy_only rounded_zero)
        set(boids 8)
        set(dummies 5)
        set(percent 40)
        set(changes 2)
        if(condition STREQUAL "empty")
            set(boids 0)
            set(dummies 0)
            set(percent 100)
            set(changes 0)
        elseif(condition STREQUAL "dummy_only")
            set(boids 0)
            set(percent 100)
            set(changes 5)
        elseif(condition STREQUAL "rounded_zero")
            set(boids 1)
            set(dummies 1)
            set(changes 0)
        endif()
        set(path "${OUTPUT_DIR}/${implementation}-${condition}.csv")
        execute_process(COMMAND "${APP}" --benchmark --implementation ${implementation}
            --agents ${boids} --dummies ${dummies} --dummy-change-percent ${percent}
            --seed 123 --dt 0.02 --warmup 2 --steps 3 --output "${path}"
            RESULT_VARIABLE code OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
        if(NOT code EQUAL 0 OR NOT stdout MATCHES "Measured steps: 3" OR NOT EXISTS "${path}")
            message(FATAL_ERROR "${implementation}/${condition}: ${code}: ${stdout} ${stderr}")
        endif()
        file(READ "${path}" csv)
        foreach(pair "implementation;${implementation}" "boid_count;${boids}"
                     "dummy_count;${dummies}" "dummy_change_percent;${percent}"
                     "dummy_change_count;${changes}" "seed;123" "warmup_steps;2"
                     "measurement_steps;3")
            list(GET pair 0 name)
            list(GET pair 1 value)
            string(FIND "${csv}" "\"setting\",\"${name}\",\"\",\"${value}\",\"available\"" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "${path}: missing ${name}=${value}")
            endif()
        endforeach()
        foreach(required "\"step\",\"overall_ms\",\"2\""
                         "\"input_storage_size_bytes\"" "\"input_storage_capacity_bytes\""
                         "\"section_ms\",\"entity_changes\",\"2\""
                         "\"section_ms\",\"state_update\",\"2\"")
            string(FIND "${csv}" "${required}" found)
            if(found EQUAL -1)
                message(FATAL_ERROR "${path}: missing ${required}")
            endif()
        endforeach()
    endforeach()
endforeach()

function(expect_failure name)
    set(path "${OUTPUT_DIR}/${name}.csv")
    file(REMOVE "${path}")
    execute_process(COMMAND "${APP}" --benchmark --warmup 0 --steps 1
        --output "${path}" ${ARGN}
        RESULT_VARIABLE code OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
    if(NOT code EQUAL 1 OR NOT stderr MATCHES "Error:" OR EXISTS "${path}")
        message(FATAL_ERROR "${name}: expected failure without CSV: ${code}: ${stdout} ${stderr}")
    endif()
endfunction()
expect_failure(invalid_percent --dummy-change-percent 101)
expect_failure(negative_dummy --dummies -1)
expect_failure(invalid_implementation --implementation invalid)
expect_failure(zero_steps --steps 0)
expect_failure(numeric_overflow --agents 8 --dt 3.4028234e38)

set(existing "${OUTPUT_DIR}/existing.csv")
file(WRITE "${existing}" "previous successful run")
execute_process(COMMAND "${APP}" --benchmark --agents 8 --warmup 0 --steps 1
    --dt 3.4028234e38 --output "${existing}"
    RESULT_VARIABLE code OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
file(READ "${existing}" contents)
if(NOT code EQUAL 1 OR NOT contents STREQUAL "previous successful run")
    message(FATAL_ERROR "Failed simulation replaced an existing result")
endif()

# A file as a parent path fails before publication. No success summary follows.
set(blocker "${OUTPUT_DIR}/parent-file")
file(WRITE "${blocker}" "keep")
execute_process(COMMAND "${APP}" --benchmark --agents 0 --warmup 0 --steps 1
    --output "${blocker}/result.csv"
    RESULT_VARIABLE code OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 30)
if(NOT code EQUAL 1 OR NOT stderr MATCHES "Error:" OR stdout MATCHES "Raw results:")
    message(FATAL_ERROR "Output failure not reported: ${code}: ${stdout} ${stderr}")
endif()
file(READ "${blocker}" contents)
if(NOT contents STREQUAL "keep")
    message(FATAL_ERROR "Failed output modified the parent file")
endif()
