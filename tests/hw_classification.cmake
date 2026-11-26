# Acadia HAL — hardware classification for CTest (task 22).
#
# Every test target is assigned a hardware class so that host-only, simulated,
# target-required, and optional-hardware outcomes have distinct CTest labels and
# are not reported as ordinary pass/fail when hardware is absent.
#
# Classes:
#   HOST            — runs entirely on the host, no target hardware.
#   SIMULATED       — uses a fake/simulated backend (fake sysroot, mock lib).
#   TARGET_REQUIRED — needs a real board; absent hardware must SKIP/REQUIRE_FAIL.
#   OPTIONAL_HW     — optional peripheral; skip if the interface is absent.
#
# Labels become CTest LABELS and are used by CI to split host vs. hardware jobs.

# Modules that are pure host logic (compute/math/memory/management simulations).
set(_ACADIA_HOST_MODULES
  cpu gpu system_memory power_management typed_module_handler
  vision_accelerator_metrics software_interface functional_safety
  clock_and_timing reliability debug_and_development_interfaces
)

# Modules exercised against a simulated backend in the current suite.
set(_ACADIA_SIMULATED_MODULES
  environmental
)

# Modules that genuinely need the target board to mean anything.
set(_ACADIA_TARGET_REQUIRED_MODULES
  i2c spi uart gpio can_bus rtc emmc nvme sd_card pcie usb ethernet
  ethernet_switch mcu_interface gnss imu accelerometer bluetooth
  audio_interfaces battery thermal_management security_features
  watchdog_and_reset external_connector serializer_deserializer rs485
  analog_input pwm_output storage video_encode_decode mipi_csi mipi_dsi
)

# Optional peripherals: SKIP when the interface is absent, never PASS falsely.
set(_ACADIA_OPTIONAL_HW_MODULES
  poe_hat sense_hat camera_module3 camera_interfaces display_interfaces
  deep_learning_accelerator vision_accelerator hdmi wifi
)

# Look up the label for a module id. Returns "unknown" if unclassified.
function(acadia_hw_label mod out)
  if("${mod}" IN_LIST _ACADIA_HOST_MODULES)
    set(${out} "host" PARENT_SCOPE)
  elseif("${mod}" IN_LIST _ACADIA_SIMULATED_MODULES)
    set(${out} "simulated" PARENT_SCOPE)
  elseif("${mod}" IN_LIST _ACADIA_TARGET_REQUIRED_MODULES)
    set(${out} "target-required" PARENT_SCOPE)
  elseif("${mod}" IN_LIST _ACADIA_OPTIONAL_HW_MODULES)
    set(${out} "optional-hardware" PARENT_SCOPE)
  else()
    set(${out} "unknown" PARENT_SCOPE)
  endif()
endfunction()

# Apply labels to every registered test_* target. Central, so adding a module
# needs only a classification entry above (plus its test subdir).
function(acadia_apply_labels)
  set(tests ${ARGN})
  foreach(test IN LISTS tests)
    string(REGEX REPLACE "^test_" "" _mod "${test}")
    acadia_hw_label("${_mod}" _label)
    if("${_label}" STREQUAL "unknown")
      message(WARNING "acadia: test '${test}' has no hardware classification — add to tests/CMakeLists.txt classification")
      set(_label "host")  # fail-soft: never claim hardware for an unclassified test
    endif()
    set_tests_properties(${test} PROPERTIES LABELS "${_label}")
  endforeach()
endfunction()
